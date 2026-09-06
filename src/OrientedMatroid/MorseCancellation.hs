{-# LANGUAGE ForeignFunctionInterface #-}

{- |
Module      : OrientedMatroid.MorseCancellation
Description : FFI bindings for discrete Morse critical pair cancellation
Copyright   : (c) ATShogi Project, 2026
License     : Apache-2.0

Cancels discrete Morse critical pairs (Index 0 minimum and Index 1 saddle)
sharing a unique gradient trajectory on the 81-square board grid via ARM NEON SIMD.
-}

module OrientedMatroid.MorseCancellation
    ( cancelCriticalPairs
    ) where

import Foreign.Ptr (Ptr, castPtr)
import Foreign.C.Types (CFloat(..), CUChar(..))
import Data.Word (Word8)

-- Foreign import of C++ ARM NEON critical pair cancellation kernel
foreign import ccall unsafe "cancel_critical_pairs_neon"
    c_cancel_critical_pairs_neon :: Ptr CFloat -> Ptr CUChar -> IO ()

-- | Cancels critical pairs in place over the 81-square board gradients and critical mask.
-- boardGradients: Ptr Float of length 81
-- criticalMask: Ptr Word8 of length 81 (0 = Regular, 1 = Index 0 Critical, 2 = Index 1 Critical)
cancelCriticalPairs :: Ptr Float -> Ptr Word8 -> IO ()
cancelCriticalPairs gradPtr maskPtr =
    c_cancel_critical_pairs_neon (castPtr gradPtr) (castPtr maskPtr)
