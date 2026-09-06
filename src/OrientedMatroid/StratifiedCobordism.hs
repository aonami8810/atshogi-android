{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE DataKinds #-}
{-# LANGUAGE KindSignatures #-}

{- |
Module      : OrientedMatroid.StratifiedCobordism
Description : Complete k=0..40 Stratified Cobordism Filtration & Boundary Inversion
Copyright   : (c) ATShogi Project, 2026
License     : Apache-2.0

Defines the complete manifold filtration X_0 \subset X_1 \subset ... \subset X_40 = S
and smooth C^2 Hermite transition maps across stratified piece boundaries.
-}

module OrientedMatroid.StratifiedCobordism
  ( StratumLevel(..)
  , computeStratifiedWeight
  , evaluateStratifiedCobordism40
  , hermiteSmoothing5th
  ) where

import Foreign.C.Types (CFloat(..), CInt(..), CSize(..))
import Foreign.Ptr (Ptr)

-- | Stratum classification according to remaining piece count k
data StratumLevel
  = TerminalEndgame      -- ^ k <= 7 (EGTB Strong Solution)
  | TransitionCobordism  -- ^ 8 <= k <= 14 (Cobordism Bridge)
  | TacticalMiddlegame   -- ^ 15 <= k <= 28 (Morse Cancellation Skeleton)
  | GrandOpening         -- ^ 29 <= k <= 40 (Global Homotopy Manifold)
  deriving (Eq, Show, Ord, Enum)

-- | Classifies integer piece count k into StratumLevel
classifyStratum :: Int -> StratumLevel
classifyStratum k
  | k <= 7    = TerminalEndgame
  | k <= 14   = TransitionCobordism
  | k <= 28   = TacticalMiddlegame
  | otherwise = GrandOpening

-- | 5th-order Hermite smooth transition polynomial: S(t) = 6t^5 - 15t^4 + 10t^3
-- Guarantees C^2 continuity with vanishing 1st and 2nd derivatives at t=0 and t=1
hermiteSmoothing5th :: Float -> Float
hermiteSmoothing5th t
  | t <= 0.0  = 0.0
  | t >= 1.0  = 1.0
  | otherwise = 6.0 * (t ** 5) - 15.0 * (t ** 4) + 10.0 * (t ** 3)

-- | Computes normalized boundary blend weight across stratum [k_low, k_high]
computeStratifiedWeight :: Int -> Int -> Int -> Float
computeStratifiedWeight k kLow kHigh
  | k <= kLow  = 0.0
  | k >= kHigh = 1.0
  | otherwise  =
      let t = fromIntegral (k - kLow) / fromIntegral (kHigh - kLow)
      in hermiteSmoothing5th t

-- | Foreign import of C++/NEON stratified cobordism multi-level evaluator
foreign import ccall unsafe "atshogi_stratified_cobordism40_eval"
    c_stratified_eval :: CFloat -> CFloat -> CFloat -> CFloat -> CInt -> CFloat

-- | Evaluates 4-layer stratified potential across k=0..40
evaluateStratifiedCobordism40 :: Float -> Float -> Float -> Float -> Int -> Float
evaluateStratifiedCobordism40 vEnd vTrans vMid vOpen k =
    let CFloat res = c_stratified_eval
                       (CFloat vEnd)
                       (CFloat vTrans)
                       (CFloat vMid)
                       (CFloat vOpen)
                       (fromIntegral k)
    in res
