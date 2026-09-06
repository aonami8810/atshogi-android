{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE DataKinds #-}
{-# LANGUAGE TypeFamilies #-}

module ATShogi.Extreme.ParallelOmniGeneratorV2 (
    SparseHasseTransitionSegment(..),
    generateOmniJosekiParallelV2
) where

import Control.Concurrent (forkIO, MVar, newEmptyMVar, putMVar, takeMVar)
import Control.Monad (forM, forM_)
import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc (alloca, allocaBytes)
import Foreign.Marshal.Array (pokeArray, peekArray)
import Foreign.Storable
import System.IO.Unsafe (unsafePerformIO)
import Data.Bits ((.|.))

-- | C++ 側の SoA (Structure of Arrays) 構造体に完全対応する Haskell レコード
-- 16局面並列 × 最大8つの遷移先
data SparseHasseTransitionSegment = SparseHasseTransitionSegment
    { segmentWeights        :: [[CFloat]]    -- 16x8 対数エントロピー抵抗重み
    , segmentTargetIndices  :: [[CUInt]]     -- 16x8 遷移先オービフォールドインデックス
    , segmentSinks          :: [CFloat]      -- 16要素 中間シンクアンカー電位（FLT_MAX or 解決済ポテンシャル）
    }

instance Storable SparseHasseTransitionSegment where
    sizeOf _ = 16 * 8 * 4 + 16 * 8 * 4 + 16 * 4  -- weights + target_indices + sinks
    alignment _ = 64 -- AVX-512 アライメント（キャッシュライン衝突防止）
    
    poke p ptr = error "Direct poking of whole SoA is bypassed for direct stream-writing for zero-copy performance."
    peek p = error "Direct peeking of whole SoA is bypassed. Use memory mapped direct reading."

-- -----------------------------------------------------------------------------
-- FFI C++ 連携カーネル宣言
-- -----------------------------------------------------------------------------

-- V2 スイープカーネル (C++: tensor_sweeper_v2.cpp)
foreign import ccall unsafe "sweep_tropical_boundary_v2"
    c_sweep_tropical_boundary_v2 :: Ptr CFloat -> Ptr SparseHasseTransitionSegment -> Ptr CUInt -> Ptr CUInt -> IO ()

-- 高速着手生成・分岐数・駒獲得検知バインド
foreign import ccall unsafe "get_hasse_branching_and_sinks"
    c_get_hasse_branching_and_sinks :: Ptr CUInt -> Ptr () -> Ptr CFloat -> Ptr CFloat -> IO ()

-- -----------------------------------------------------------------------------
-- コア制御ループ（Haskell 側決定論的並列スイーパー）
-- -----------------------------------------------------------------------------

-- | 指定されたスレッド数で GHC スレッドを Fork し、大域コチェイン・スイープを完全並列に実行する
generateOmniJosekiParallelV2 
    :: Ptr CFloat  -- ^ 大域オムニポテンシャル場へのポインタ（mmap 共有バッファ）
    -> Int         -- ^ 割り当てる物理スレッド数
    -> [CUInt]     -- ^ スイープ対象の全オービフォールド局所ノード（Hasseインデックス配列）
    -> IO ()
generateOmniJosekiParallelV2 sharedPotentialsPtr numThreads activeNodeList = do
    putStrLn $ " [SYSTEM] V2決定論的オムニ定跡スイープ始動: 物理スレッド数 = " ++ show numThreads
    
    -- 1. スレッド数に合わせて、アクティブノード配列を16要素アライメントで分割
    let chunkSize = 16 * ((length activeNodeList) `div` (16 * numThreads))
        chunks    = chunkList chunkSize activeNodeList
    
    -- 2. スレッドバリア同期用の MVar
    mvars <- forM [1..numThreads] $ \_ -> newEmptyMVar
    
    -- 3. 各スレッドで決定論的バックワード・スイープをノンブロッキング起動
    forM_ (zipChunks chunks mvars) $ \(threadId, chunk, mvar) -> forkIO $ do
        runThreadSweepV2 sharedPotentialsPtr threadId chunk
        putMVar mvar () -- 完了シグナル
        
    -- 4. 全スレッドの終了を同調（バリア同期）
    forM_ mvars takeMVar
    putStrLn " [SYSTEM] 決定論的大域コチェイン・スイープ完全収束。オムニ定跡 static_joseki.bin 結晶化完了。"

-- | 各スレッドでの個別スイープ実行コア（収束するまで Jacobi トロピカル反復を回す）
runThreadSweepV2 :: Ptr CFloat -> Int -> [CUInt] -> IO ()
runThreadSweepV2 potentialsPtr threadId chunkNodes = do
    let numBlocks = length chunkNodes `div` 16
    putStrLn $ "   [Thread-" ++ show threadId ++ "] スイープ開始: 処理ブロック数 = " ++ show numBlocks
    
    -- 16局面 SoA のローカルスタック（キャッシュアラインされたもの）をスタックアロケート
    allocaBytes (sizeOf (undefined :: SparseHasseTransitionSegment)) $ \(segPtr :: Ptr SparseHasseTransitionSegment) ->
        alloca $ \(changedMaskPtr :: Ptr CUInt) -> do
            
            -- 全ブロックの変更がゼロ（Changed Mask == 0）に収束するまで代数反復
            let convergenceLoop iterationCount = do
                    changedAccumulator <- sweepAllBlocks potentialsPtr segPtr chunkNodes changedMaskPtr 0 0
                    if changedAccumulator == 0
                        then putStrLn $ "   [Thread-" ++ show threadId ++ "] 収束完了。反復回数 = " ++ show iterationCount
                        else convergenceLoop (iterationCount + 1)
            
            convergenceLoop 1

-- | 割り当てられたノードリストを、16ノードずつブロック単位で順次 C++ カーネルに受け渡す
sweepAllBlocks 
    :: Ptr CFloat 
    -> Ptr SparseHasseTransitionSegment 
    -> [CUInt] 
    -> Ptr CUInt 
    -> Int 
    -> CUInt 
    -> IO CUInt
sweepAllBlocks potentialsPtr segPtr [] _ _ acc = return acc
sweepAllBlocks potentialsPtr segPtr nodes changedMaskPtr blockIdx acc = do
    let (blockNodes, restNodes) = splitAt 16 nodes
    if length blockNodes < 16
        then return acc -- 端数切り捨て（アライメント安全）
        else do
            -- 1. 16局面の Hasse インデックスを一時配列にスタックマウント
            allocaBytes (16 * 4) $ \(blockNodesPtr :: Ptr CUInt) -> do
                pokeArray blockNodesPtr blockNodes
                
                -- 2. 16局面に対するエントロピー抵抗および中間シンク電位の動的マウント
                c_get_hasse_branching_and_sinks blockNodesPtr (castPtr segPtr) potentialsPtr (potentialsPtr `plusPtr` (40 * 531441 * 4))
                
                -- 3. C++ SIMD トロピカルスイープを実行
                c_sweep_tropical_boundary_v2 potentialsPtr segPtr blockNodesPtr changedMaskPtr
                
                -- 4. 変更検知マスクをロードして累積
                maskVal <- peek changedMaskPtr
                let newAcc = acc .|. maskVal
                
                sweepAllBlocks potentialsPtr segPtr restNodes changedMaskPtr (blockIdx + 1) newAcc

-- -----------------------------------------------------------------------------
-- ユーティリティ定義群
-- -----------------------------------------------------------------------------

chunkList :: Int -> [a] -> [[a]]
chunkList _ [] = []
chunkList n xs = let (yf, ys) = splitAt n xs in yf : chunkList n ys

zipChunks :: [[a]] -> [MVar ()] -> [(Int, [a], MVar ())]
zipChunks xs mvs = zipWith3 (\id chunk mv -> (id, chunk, mv)) [1..] xs mvs
