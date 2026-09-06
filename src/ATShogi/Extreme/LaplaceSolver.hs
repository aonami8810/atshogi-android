{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE ScopedTypeVariables #-}

module ATShogi.Extreme.LaplaceSolver (
    solveConformalLaplace
) where

import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc (alloca)
import Foreign.Storable (peek, poke, peekElemOff)
import Control.Monad (when)

-- | C++ 側のラプラス Jacobi 反復ステップ関数
foreign import ccall unsafe "solve_laplace_iteration_step"
    c_solve_laplace_iteration_step :: Ptr CFloat -- ^ potentials_array
                                   -> Ptr CFloat -- ^ next_potentials
                                   -> Ptr ()     -- ^ OrbifoldLaplacianSegment
                                   -> Ptr CUInt  -- ^ active_indices
                                   -> CUInt      -- ^ boundary_mask
                                   -> IO ()

-- | 指定された絶対許容誤差（ε = 10^-5）に達するまで、Jacobi反復を一括して回しきる
solveConformalLaplace :: Ptr CFloat         -- ^ 入力ポテンシャル（T_old）
                      -> Ptr CFloat         -- ^ 出力ポテンシャル（T_new）
                      -> Ptr ()             -- ^ 結合トポロジーセグメント
                      -> Ptr CUInt          -- ^ 反復対象インデックス
                      -> CUInt              -- ^ 境界マスク
                      -> Int                -- ^ 最大反復回数 (Max Iterations)
                      -> Double             -- ^ 許容閾値 (Epsilon)
                      -> IO Int             -- ^ 収束に要した反復回数を返す
solveConformalLaplace tOld tNew segPtr indicesPtr mask maxIter eps =
    loop tOld tNew 0
  where
    loop src dest iter
        | iter >= maxIter = return iter
        | otherwise = do
            -- 1ステップ Jacobi 更新
            c_solve_laplace_iteration_step src dest segPtr indicesPtr mask

            -- L2残差（収束性）の監査
            diff <- calculateL2Difference src dest 16
            if diff < eps
                then return (iter + 1)
                else do
                    -- バッファのポインタをスワップ（インプレース交互更新）
                    loop dest src (iter + 1)

-- 簡易残差差分計算ヘルパー
calculateL2Difference :: Ptr CFloat -> Ptr CFloat -> Int -> IO Double
calculateL2Difference p1 p2 size = do
    diffs <- mapM (\i -> do
        v1 <- peekElemOff p1 i
        v2 <- peekElemOff p2 i
        let d = realToFrac (v1 - v2)
        return (d * d)) [0..(size - 1)]
    return $ sqrt (sum diffs)
