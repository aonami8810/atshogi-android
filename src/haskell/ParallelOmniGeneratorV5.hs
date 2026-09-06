{-# LANGUAGE ForeignFunctionInterface #-}
module ParallelOmniGeneratorV5 where

import Foreign.C.Types
import Foreign.Ptr
import Foreign.Marshal.Array
import Foreign.Marshal.Alloc
import Foreign.Storable
import Control.Monad (when, forM_)

-- -------------------------------------------------------------------------
-- C++ FFI Bindings for V5 (True Bipartite Min-Max EGTBL 40 Sweep)
-- -------------------------------------------------------------------------
foreign import ccall "compress_to_mps_v5" 
    c_compress_to_mps_v5 :: Ptr CFloat -> CSize -> CInt -> IO ()

foreign import ccall "get_hasse_branching_v5" 
    c_get_hasse_branching_v5 :: Ptr CUInt -> Ptr () -> IO ()

foreign import ccall "sweep_tropical_boundary_v5" 
    c_sweep_tropical_boundary_v5 :: Ptr CFloat -> Ptr () -> Ptr CUInt -> Ptr CFloat -> Ptr CFloat -> CInt -> Ptr CUInt -> IO ()

foreign import ccall "project_dtm_to_asymptotic_potentials" 
    c_project_dtm_to_asymptotic_potentials :: Ptr CFloat -> CInt -> IO ()

-- SparseHasseTransitionSegmentV5 size (bytes):
-- float weights[16][8] = 512
-- uint32_t target_indices[16][8] = 512
-- float sinks[16] = 64
-- int is_gote_turn[16] = 64
-- Total: 1152 bytes
sizeof_SparseHasseTransitionSegmentV5 :: Int
sizeof_SparseHasseTransitionSegmentV5 = 1152

main :: IO ()
main = do
    putStrLn "========================================================"
    putStrLn " ATShogi-OM Extreme: Omni Tensor Generator V5"
    putStrLn " (Genuine Bipartite Min-Max Tropical Engine)"
    putStrLn "========================================================"
    
    let k_target = 40 :: CInt
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
    seg <- mallocBytes sizeof_SparseHasseTransitionSegmentV5
    
    putStrLn "Initializing Identity/Average mappings for MERA..."
    forM_ [0 .. numStates - 1] $ \i -> do
        pokeElemOff potentials i 1000.0
        
    forM_ [0 .. uSize - 1] $ \i -> pokeElemOff tensorU i 0.0
    forM_ [0 .. fromIntegral chi - 1] $ \c -> do
        forM_ [0 .. fromIntegral chi - 1] $ \d -> do
            let ab = c * fromIntegral chi + d
            let idx = (c * fromIntegral chi + d) * (fromIntegral chi * fromIntegral chi) + ab
            pokeElemOff tensorU idx 1.0

    forM_ [0 .. vSize - 1] $ \i -> pokeElemOff tensorV i 0.0
    forM_ [0 .. fromIntegral chi * fromIntegral chi - 1] $ \ab -> do
        let a = ab `div` fromIntegral chi
        let b = ab `mod` fromIntegral chi
        let i = (a + b) `mod` fromIntegral chi
        let idx = ab * fromIntegral chi + i
        pokeElemOff tensorV idx (1.0 / fromIntegral chi)

    putStrLn "Running Genuine Min-Max Tropical Sweep Iterations..."
    
    let loop iter = when (iter < maxIter) $ do
            
            changed <- malloc :: IO (Ptr CUInt)
            poke changed 0
            
            forM_ [0 .. (numStates `div` 16) - 1] $ \chunk -> do
                forM_ [0 .. 15] $ \lane -> do
                    let nodeIdx = fromIntegral (chunk * 16 + lane) :: CUInt
                    pokeElemOff blockNodes lane nodeIdx
                    pokeElemOff activeIndices lane nodeIdx

                c_get_hasse_branching_v5 blockNodes seg
                
                chunkChangedMask <- malloc :: IO (Ptr CUInt)
                poke chunkChangedMask 0
                
                c_sweep_tropical_boundary_v5 potentials seg activeIndices tensorU tensorV chi chunkChangedMask
                
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
    
    putStrLn "Sweep Complete. Projecting Linear DTM to Asymptotic Potentials..."
    c_project_dtm_to_asymptotic_potentials potentials (fromIntegral numStates)

    putStrLn "Formatting into 1.0 MB TNRG..."
    c_compress_to_mps_v5 potentials (fromIntegral numStates) k_target

    -- Cleanup
    free potentials
    free tensorU
    free tensorV
    free blockNodes
    free activeIndices
    free changedMask
    free seg

    putStrLn "V5 Generation pipeline finished successfully."
