{-# LANGUAGE ForeignFunctionInterface #-}

{- |
Module      : OrientedMatroid.DecayDecider
Description : Spatiotemporal double decay lambda decision and NEON tensor merging FFI
Copyright   : (c) ATShogi Project, 2026
License     : Apache-2.0

Implements the dynamic interpolation model:
  T_total = T_static + lambda(n, T) * Delta_T
where lambda(n, T) = lambda_0 * gamma^n * (delta_p2^2 / (delta_p2^2 + epsilon)).
-}

module OrientedMatroid.DecayDecider
    ( GameContext(..)
    , mergeTensors
    , computeSpatiotemporalLambda
    ) where

import Foreign.Ptr (Ptr, castPtr)
import Foreign.C.Types (CFloat(..), CInt(..))

-- | Context tracking game trajectory deviations and topological distortion.
data GameContext = GameContext
    { stepsOffTrunk      :: !Int   -- ^ Moves elapsed since deviating from trunk (n)
    , gaifullinDeviation :: !Float -- ^ Topological distortion from Gaifullin audit (delta p_2)
    , baseLambda         :: !Float -- ^ Initial punishment intensity (lambda_0)
    } deriving (Show, Eq)

-- | Pure Haskell calculation of spatiotemporal lambda
computeSpatiotemporalLambda :: GameContext -> Float -> Float -> Float
computeSpatiotemporalLambda ctx gamma eps =
    let n = stepsOffTrunk ctx
        lambdaT = gamma ^ n
        dp2 = gaifullinDeviation ctx
        dp2Sq = dp2 * dp2
        lambdaS = dp2Sq / (dp2Sq + eps)
    in baseLambda ctx * lambdaT * lambdaS

-- | Foreign import of ARM NEON tensor merger kernel
foreign import ccall unsafe "merge_tensors_neon"
    c_merge_tensors :: Ptr CFloat -- ^ Static trunk tensor
                    -> Ptr CFloat -- ^ Deviation delta tensor
                    -> Ptr CFloat -- ^ Merged output buffer
                    -> CFloat     -- ^ lambda_0
                    -> CFloat     -- ^ gamma
                    -> CInt       -- ^ n (elapsed moves)
                    -> CFloat     -- ^ delta_p2 (Gaifullin deviation)
                    -> CFloat     -- ^ epsilon (regularization)
                    -> CInt       -- ^ size (element count)
                    -> IO ()

-- | Haskell safe wrapper for ARM NEON SIMD tensor merge
mergeTensors :: Ptr Float   -- ^ Static trunk tensor
             -> Ptr Float   -- ^ Dynamic punishment delta
             -> Ptr Float   -- ^ Target output buffer
             -> GameContext -- ^ Current game context
             -> Int         -- ^ Number of tensor elements
             -> IO ()
mergeTensors staticPtr deltaPtr outPtr ctx size =
    c_merge_tensors
        (castPtr staticPtr)
        (castPtr deltaPtr)
        (castPtr outPtr)
        (CFloat $ baseLambda ctx)
        (CFloat 0.85) -- Default gamma (15% decay per elapsed half-move)
        (fromIntegral $ stepsOffTrunk ctx)
        (CFloat $ gaifullinDeviation ctx)
        (CFloat 1e-4) -- Default regularization epsilon
        (fromIntegral size)
