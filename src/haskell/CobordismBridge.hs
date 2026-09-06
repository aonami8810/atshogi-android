{-# LANGUAGE ForeignFunctionInterface #-}
module CobordismBridge where

import Foreign.C.Types

-- Phase 4: Integration
foreign import ccall "compute_cobordism_weight" c_compute_weight :: CInt -> IO CFloat

getWeight :: Int -> IO Float
getWeight k = do
    w <- c_compute_weight (fromIntegral k)
    return (realToFrac w)
