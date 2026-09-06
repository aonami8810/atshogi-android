{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE ScopedTypeVariables #-}

module ATShogi.Extreme.TensorSweeper (
    sweepTropicalBoundary
) where

import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc (alloca)
import Foreign.Storable (peek)

-- | C++ 側の決定論的掃き出しカーネルをインポート
foreign import ccall unsafe "sweep_tropical_boundary_avx512_neon"
    c_sweep_tropical_boundary :: Ptr CFloat -> Ptr () -> Ptr CUInt -> Ptr CUInt -> IO ()

-- | 指定されたアクティブ局面ブロックに対して、トロピカル最短路のバックワード更新を実行
sweepTropicalBoundary :: Ptr CFloat -> Ptr () -> Ptr CUInt -> IO CUInt
sweepTropicalBoundary potentialsPtr segmentPtr activeIndicesPtr = do
    alloca $ \(maskPtr :: Ptr CUInt) -> do
        c_sweep_tropical_boundary potentialsPtr segmentPtr activeIndicesPtr maskPtr
        peek maskPtr
