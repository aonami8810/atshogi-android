{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE EmptyDataDecls #-}

module OrientedMatroid.TopologicalFFI
    ( UsiEngineState
    , UsiMoveCandidate(..)
    , initUsiEngine
    , destroyUsiEngine
    , evaluateBestMoveUsi
    ) where

import Foreign.Ptr (Ptr, castPtr)
import Foreign.C.Types (CInt(..), CFloat(..))
import Foreign.C.String (CString, withCString)
import Foreign.Storable (Storable(..))
import Foreign.Marshal.Array (withArray)
import Data.Word (Word32)

-- C++側の不透明な構造体ポインタ表現
data UsiEngineState

-- FFI境界を超える軽量構造体の定義 (C++のアライメント 4 / サイズ 12 構造に完全調和)
data UsiMoveCandidate = UsiMoveCandidate
    { moveId         :: !Word32  -- USI指し手圧縮コード (例: 7g7f)
    , targetStateIdx :: !Word32  -- 遷移先のハッセ図ノード配列インデックス
    , isGoteAfter    :: !CInt    -- 着手後の手番パリティ (0: 先手, 1: 後手)
    } deriving (Show, Eq)

instance Storable UsiMoveCandidate where
    sizeOf _ = 12  -- 4 bytes * 3
    alignment _ = 4
    peek ptr = do
        mId  <- peekByteOff ptr 0
        tIdx <- peekByteOff ptr 4
        isG  <- peekByteOff ptr 8
        return $ UsiMoveCandidate mId tIdx isG
    poke ptr (UsiMoveCandidate mId tIdx isG) = do
        pokeByteOff ptr 0 mId
        pokeByteOff ptr 4 tIdx
        pokeByteOff ptr 8 isG

-- C++ リンケージのバインディング定義
foreign import ccall unsafe "init_usi_engine"
    c_init_usi_engine :: CString -> IO (Ptr UsiEngineState)

foreign import ccall unsafe "destroy_usi_engine"
    c_destroy_usi_engine :: Ptr UsiEngineState -> IO ()

foreign import ccall unsafe "evaluate_best_move_usi"
    c_evaluate_best_move_usi :: Ptr UsiEngineState
                             -> Ptr UsiMoveCandidate
                             -> CInt
                             -> Ptr CFloat
                             -> IO Word32

-- Haskell 向け高階関数ラッパー
initUsiEngine :: FilePath -> IO (Ptr UsiEngineState)
initUsiEngine path = withCString path c_init_usi_engine

destroyUsiEngine :: Ptr UsiEngineState -> IO ()
destroyUsiEngine = c_destroy_usi_engine

evaluateBestMoveUsi :: Ptr UsiEngineState
                    -> [UsiMoveCandidate]
                    -> Ptr Float
                    -> IO Word32
evaluateBestMoveUsi statePtr candidates potentialsPtr =
    withArrayLen candidates $ \len candPtr -> do
        c_evaluate_best_move_usi statePtr candPtr (fromIntegral len) (castPtr potentialsPtr)

-- ヘルパー関数: リストを配列化して一時的にFfi境界で渡す
withArrayLen :: Storable a => [a] -> (Int -> Ptr a -> IO b) -> IO b
withArrayLen xs f = do
    let len = length xs
    withArray xs $ \ptr -> f len ptr
