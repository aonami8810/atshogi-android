{-# LANGUAGE ForeignFunctionInterface #-}

module OrientedMatroid.CobordismBridge (
    evaluateHybridPotential,
    computeCobordismWeight
) where

import Foreign.C.Types

foreign import ccall unsafe "bridge_potentials_cobordism"
  c_bridge_potentials :: CFloat -> CFloat -> CInt -> CFloat

-- | 中盤の探索評価値と終盤のテンソル評価値を境界なくシームレスに接着
evaluateHybridPotential :: Float -> Float -> Int -> Float
evaluateHybridPotential vMid vEnd k =
  realToFrac $ c_bridge_potentials (realToFrac vMid) (realToFrac vEnd) (fromIntegral k)

computeCobordismWeight :: Int -> Float
computeCobordismWeight k = 1.0 / (1.0 + exp (1.5 * fromIntegral (k - 7)))
