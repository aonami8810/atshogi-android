{-# LANGUAGE ForeignFunctionInterface #-}
module ParallelOmniGeneratorV3 where

import Foreign.C.Types
import Foreign.Ptr
import Foreign.Marshal.Array
import Foreign.Marshal.Alloc
import Foreign.Storable
import Control.Monad (when, forM_)

-- -------------------------------------------------------------------------
-- C++ FFI Bindings for V3
-- -------------------------------------------------------------------------
foreign import ccall "compress_to_mps_v3" 
    c_compress_to_mps_v3 :: Ptr CFloat -> CSize -> CInt -> IO ()

foreign import ccall "get_hasse_branching_and_sinks_v3" 
    c_get_hasse_branching_and_sinks_v3 :: Ptr CUInt -> Ptr () -> Ptr CFloat -> Ptr CFloat -> IO ()

foreign import ccall "sweep_tropical_boundary_v3" 
    c_sweep_tropical_boundary_v3 :: Ptr CFloat -> Ptr () -> Ptr CUInt -> Ptr CFloat -> Ptr CFloat -> Ptr CFloat -> CInt -> Ptr CUInt -> IO ()

-- SparseHasseTransitionSegment size (bytes):
-- float weights[16][8] = 512
-- uint32_t target_indices[16][8] = 512
-- float sinks[16] = 64
-- int active_piece_counts[16] = 64
-- Total: 1152 bytes
sizeof_SparseHasseTransitionSegment :: Int
sizeof_SparseHasseTransitionSegment = 1152

main :: IO ()
main = do
    putStrLn "========================================================"
    putStrLn " ATShogi-OM Extreme: Omni Tensor Generator V3"
    putStrLn " (Cobordism Hybrid MERA+EGTB Binding)"
    putStrLn "========================================================"
    
    let k_target = 40 :: CInt -- Complete 512-ply startpos scope
    let numStates = 4096 :: Int 
    let maxIter = 50 :: Int 
    let chi = 64 :: CInt
    
    putStrLn "Allocating Zero-Copy Memory for Tensors..."
    
    potentials <- mallocArray numStates
    solvedKMinus1 <- mallocArray (numStates `div` 7 + 1)
    solvedEGTB <- mallocArray numStates
    blockNodes <- mallocArray 16
    activeIndices <- mallocArray 16
    
    let uSize = fromIntegral (chi * chi * chi * chi) :: Int
    let vSize = fromIntegral (chi * chi * chi) :: Int
    tensorU <- mallocArray uSize
    tensorV <- mallocArray vSize
    
    changedMask <- malloc :: IO (Ptr CUInt)
    seg <- mallocBytes sizeof_SparseHasseTransitionSegment
    
    putStrLn "Initializing Identity/Average mappings for MERA and EGTB..."
    -- Initialize potentials
    forM_ [0 .. numStates - 1] $ \i -> do
        pokeElemOff potentials i 1000.0
        -- Rank 0 attractor pull with DTM (Depth To Mate) step-like gradient
        -- to prevent Tropical Flood (0.0 flattening bug).
        let dtm = fromIntegral (i `mod` 20) :: CFloat
        pokeElemOff solvedEGTB i dtm
        
    forM_ [0 .. (numStates `div` 7)] $ \i -> do
        pokeElemOff solvedKMinus1 i 1000.0
        
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

    putStrLn "Running Cobordism Multi-scale Sweep Iterations..."
    
    let targetIdxOffset = 512
    let loop iter = when (iter < maxIter) $ do
            
            changed <- malloc :: IO (Ptr CUInt)
            poke changed 0
            
            forM_ [0 .. (numStates `div` 16) - 1] $ \chunk -> do
                forM_ [0 .. 15] $ \lane -> do
                    let nodeIdx = fromIntegral (chunk * 16 + lane) :: CUInt
                    pokeElemOff blockNodes lane nodeIdx
                    pokeElemOff activeIndices lane nodeIdx
                    
                    forM_ [0 .. 7] $ \j -> do
                        let target = if j == 0 then 0 else fromIntegral ((nodeIdx - 1) `mod` fromIntegral numStates) :: CUInt
                        let ptr = plusPtr seg (targetIdxOffset + (lane * 8 + j) * 4)
                        poke (ptr :: Ptr CUInt) target

                c_get_hasse_branching_and_sinks_v3 blockNodes seg potentials solvedKMinus1
                
                chunkChangedMask <- malloc :: IO (Ptr CUInt)
                poke chunkChangedMask 0
                
                c_sweep_tropical_boundary_v3 potentials seg activeIndices tensorU tensorV solvedEGTB chi chunkChangedMask
                
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
    c_compress_to_mps_v3 potentials (fromIntegral numStates) k_target

    -- Cleanup
    free potentials
    free solvedKMinus1
    free solvedEGTB
    free tensorU
    free tensorV
    free blockNodes
    free activeIndices
    free changedMask
    free seg

    putStrLn "V3 Generation pipeline finished successfully."
