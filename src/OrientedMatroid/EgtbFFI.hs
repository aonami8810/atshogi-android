{-# LANGUAGE ForeignFunctionInterface #-}

{- |
Module      : OrientedMatroid.EgtbFFI
Description : Zero-JVM POSIX mmap & ARM NEON direct tensor contraction FFI
Copyright   : (c) ATShogi Project, 2026
License     : Apache-2.0

Bypasses the Android Kotlin/JNI layer completely by mapping egtbl7.bin
directly into process memory via POSIX mmap syscalls and evaluating
tensor contractions on L3 cache via ARM NEON SIMD.
-}

module OrientedMatroid.EgtbFFI
    ( EgtbContainer(..)
    , initEgtb
    , evaluateEgtbNEON
    , freeEgtb
    ) where

import Foreign.Ptr (Ptr, nullPtr, castPtr)
import Foreign.C.Types (CFloat(..), CInt(..), CSize(..))
import Foreign.C.String (CString, withCString)
import Foreign.Marshal.Alloc (alloca)
import Foreign.Storable (peek)
import System.IO (hPutStrLn, stderr)

-- | Haskell-side container tracking the memory-mapped EGTB pointer and its byte size.
data EgtbContainer = EgtbContainer
    { egtbPtr  :: !(Ptr CFloat)
    , egtbSize :: !CSize
    } deriving (Eq, Show)

-- Foreign import declarations for C++ mmap and NEON routines
foreign import ccall unsafe "load_egtb_mmap"
    c_load_egtb_mmap :: CString -> Ptr CSize -> IO (Ptr CFloat)

foreign import ccall unsafe "unload_egtb_mmap"
    c_unload_egtb_mmap :: Ptr CFloat -> CSize -> IO ()

foreign import ccall unsafe "contract_egtb_neon"
    c_contract_egtb_neon :: Ptr CFloat -> Ptr CFloat -> Ptr CFloat -> CInt -> IO CFloat

-- | 1. Maps the EGTB binary into virtual memory via POSIX mmap.
initEgtb :: FilePath -> IO (Maybe EgtbContainer)
initEgtb path = withCString path $ \cPath ->
    alloca $ \sizePtr -> do
        ptr <- c_load_egtb_mmap cPath sizePtr
        if ptr == nullPtr
            then do
                hPutStrLn stderr $ "[ATShogi::EGTB] Failed to mmap file: " ++ path
                return Nothing
            else do
                sz <- peek sizePtr
                hPutStrLn stderr $ "[ATShogi::EGTB] mmap succeeded. Address: " ++ show ptr ++ ", Size: " ++ show sz ++ " bytes"
                return $ Just $ EgtbContainer ptr sz

-- | 2. Zero-copy tensor contraction over L3 cache using ARM NEON SIMD.
evaluateEgtbNEON :: EgtbContainer -> Ptr Float -> Ptr Float -> Int -> IO Float
evaluateEgtbNEON container vecX vecY chi = do
    CFloat res <- c_contract_egtb_neon
                    (egtbPtr container)
                    (castPtr vecX)
                    (castPtr vecY)
                    (fromIntegral chi)
    return res

-- | 3. Safely releases the mmap mapping via munmap.
freeEgtb :: EgtbContainer -> IO ()
freeEgtb container = do
    c_unload_egtb_mmap (egtbPtr container) (egtbSize container)
    hPutStrLn stderr "[ATShogi::EGTB] munmap completed successfully."
