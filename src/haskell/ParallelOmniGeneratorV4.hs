{-# LANGUAGE ForeignFunctionInterface #-}
module ParallelOmniGeneratorV4 where

import Foreign.C.Types
import Foreign.Ptr
import Foreign.Marshal.Array
import Foreign.Marshal.Alloc
import Foreign.Storable
import Control.Monad (when, forM_)

-- -------------------------------------------------------------------------
-- C++ FFI Bindings for V4 (EGTBL 40 Pure MERA Sweep)
-- -------------------------------------------------------------------------
foreign import ccall "compress_to_mps_v4" 
    c_compress_to_mps_v4 :: Ptr CFloat -> CSize -> CInt -> IO ()

foreign import ccall "get_hasse_branching_v4" 
    c_get_hasse_branching_v4 :: Ptr CUInt -> Ptr () -> IO ()

foreign import ccall "sweep_tropical_boundary_v4" 
    c_sweep_tropical_boundary_v4 :: Ptr CFloat -> Ptr () -> Ptr CUInt -> Ptr CFloat -> Ptr CFloat -> CInt -> Ptr CUInt -> IO ()

-- SparseHasseTransitionSegmentV4 size (bytes):
-- float weights[16][8] = 512
-- uint32_t target_indices[16][8] = 512
-- Total: 1024 bytes
sizeof_SparseHasseTransitionSegmentV4 :: Int
sizeof_SparseHasseTransitionSegmentV4 = 1024

main :: IO ()
main = do
    putStrLn "========================================================"
    putStrLn " ATShogi-OM Extreme: Omni Tensor Generator V4"
    putStrLn " (Pure EGTBL 40 MERA Topological Engine)"
    putStrLn "========================================================"
    
    let k_target = 40 :: CInt -- Complete 512-ply startpos scope
    let numStates = 4096 :: Int 
    let maxIter = 50 :: Int 
    let chi = 64 :: CInt
    
    putStrLn "Allocating Zero-Copy Memory for Tensors..."
    
    potentials <- mallocArray numStates
    blockNodes <- mallocArray 16
    activeIndices <- mallocArray 16
    
    let uSize = fromIntegral (chi * chi * chi * chi) :: Int
    let vSize = fromIntegral (chi * chi * chi) :: Int
    tensorU <- mallocArray uSize
    tensorV <- mallocArray vSize
    
    changedMask <- malloc :: IO (Ptr CUInt)
    seg <- mallocBytes sizeof_SparseHasseTransitionSegmentV4
    
    putStrLn "Initializing Identity/Average mappings for MERA..."
    -- Initialize potentials
    forM_ [0 .. numStates - 1] $ \i -> do
        pokeElemOff potentials i 1000.0
        
    -- Initialize tensor U (Identity)
    forM_ [0 .. uSize - 1] $ \i -> pokeElemOff tensorU i 0.0
    forM_ [0 .. fromIntegral chi - 1] $ \c -> do
        forM_ [0 .. fromIntegral chi - 1] $ \d -> do
            let ab = c * fromIntegral chi + d
            let idx = (c * fromIntegral chi + d) * (fromIntegral chi * fromIntegral chi) + ab
            pokeElemOff tensorU idx 1.0

    -- Initialize tensor V (Average)
    forM_ [0 .. vSize - 1] $ \i -> pokeElemOff tensorV i 0.0
    forM_ [0 .. fromIntegral chi * fromIntegral chi - 1] $ \ab -> do
        let a = ab `div` fromIntegral chi
        let b = ab `mod` fromIntegral chi
        let i = (a + b) `mod` fromIntegral chi
        let idx = ab * fromIntegral chi + i
        pokeElemOff tensorV idx (1.0 / fromIntegral chi)

    putStrLn "Running Pure MERA Topological Sweep Iterations..."
    
    let loop iter = when (iter < maxIter) $ do
            
            changed <- malloc :: IO (Ptr CUInt)
            poke changed 0
            
            forM_ [0 .. (numStates `div` 16) - 1] $ \chunk -> do
                forM_ [0 .. 15] $ \lane -> do
                    let nodeIdx = fromIntegral (chunk * 16 + lane) :: CUInt
                    pokeElemOff blockNodes lane nodeIdx
                    pokeElemOff activeIndices lane nodeIdx

                c_get_hasse_branching_v4 blockNodes seg
                
                chunkChangedMask <- malloc :: IO (Ptr CUInt)
                poke chunkChangedMask 0
                
                c_sweep_tropical_boundary_v4 potentials seg activeIndices tensorU tensorV chi chunkChangedMask
                
                mask <- peek chunkChangedMask
                currChanged <- peek changed
                poke changed (currChanged + mask)
                free chunkChangedMask
                
            totalChanged <- peek changed
            free changed
            
            if totalChanged == 0 && iter > 10 then do
                putStrLn $ "Sweep converged at iteration " ++ show iter
            else
                loop (iter + 1)
                
    loop 0
    
    putStrLn "Sweep Complete. Formatting into 1.0 MB TNRG..."
    c_compress_to_mps_v4 potentials (fromIntegral numStates) k_target

    -- Cleanup
    free potentials
    free tensorU
    free tensorV
    free blockNodes
    free activeIndices
    free changedMask
    free seg

    putStrLn "V4 Generation pipeline finished successfully."
