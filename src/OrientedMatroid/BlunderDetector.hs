{-# LANGUAGE ForeignFunctionInterface #-}

{- |
Module      : OrientedMatroid.BlunderDetector
Description : Automatic blunder severity estimation via gradient norm spikes and NEON FFI
Copyright   : (c) ATShogi Project, 2026
License     : Apache-2.0

Evaluates blunder severity lambda_0 directly from the squared L2 norm
of the gradient residual vector Delta g = g_actual - g_expected using
an algebraic Hill saturation function (without computing square roots).
-}

module OrientedMatroid.BlunderDetector
    ( detectBlunderSeverity
    , detectBlunderSeverityPtr
    ) where

import Foreign.Ptr (Ptr, castPtr)
import Foreign.C.Types (CFloat(..))
import Data.Vector.Storable (Vector, unsafeWith)

-- | Foreign import of C++ ARM NEON blunder detection kernel
foreign import ccall unsafe "calculate_blunder_lambda0"
    c_calculateBlunderLambda0 :: Ptr CFloat -> Ptr CFloat -> CFloat -> CFloat -> IO CFloat

-- | Evaluates blunder severity lambda_0 using Storable Vectors
detectBlunderSeverity :: Vector Float -- ^ Expected best-move gradient (size: 81)
                      -> Vector Float -- ^ Actual opponent move gradient (size: 81)
                      -> Float        -- ^ Half-saturation threshold squared (theta^2)
                      -> Float        -- ^ Maximum penalty intensity (lambda_max)
                      -> IO Float
detectBlunderSeverity vExpected vActual thetaSq lambdaMax =
    unsafeWith vExpected $ \ptrExpected ->
        unsafeWith vActual $ \ptrActual ->
            detectBlunderSeverityPtr (castPtr ptrExpected) (castPtr ptrActual) thetaSq lambdaMax

-- | Direct pointer variant for zero-allocation tight loops
detectBlunderSeverityPtr :: Ptr Float -> Ptr Float -> Float -> Float -> IO Float
detectBlunderSeverityPtr ptrExpected ptrActual thetaSq lambdaMax = do
    CFloat val <- c_calculateBlunderLambda0
                    (castPtr ptrExpected)
                    (castPtr ptrActual)
                    (CFloat thetaSq)
                    (CFloat lambdaMax)
    return val
