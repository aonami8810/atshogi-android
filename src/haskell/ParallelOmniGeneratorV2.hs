{-# LANGUAGE ForeignFunctionInterface #-}
module ParallelOmniGeneratorV2 where

import Foreign.C.Types
import Foreign.Ptr
import Foreign.Marshal.Array
import Foreign.Marshal.Alloc
import Foreign.Storable
import Control.Monad (when, forM_)

-- -------------------------------------------------------------------------
-- C++ FFI Bindings
-- -------------------------------------------------------------------------
foreign import ccall "compress_to_mps" 
    c_compress_to_mps :: Ptr CFloat -> CSize -> CInt -> IO ()

foreign import ccall "get_hasse_branching_and_sinks" 
    c_get_hasse_branching_and_sinks :: Ptr CUInt -> Ptr () -> Ptr CFloat -> Ptr CFloat -> IO ()

foreign import ccall "sweep_tropical_boundary_v2" 
    c_sweep_tropical_boundary_v2 :: Ptr CFloat -> Ptr () -> Ptr CUInt -> Ptr CUInt -> IO ()

-- Struct sizes (bytes)
-- SparseHasseTransitionSegment:
-- float weights[16][8] = 512 bytes
-- uint32_t target_indices[16][8] = 512 bytes
-- float sinks[16] = 64 bytes
-- Total: 1088 bytes
sizeof_SparseHasseTransitionSegment :: Int
sizeof_SparseHasseTransitionSegment = 1088

main :: IO ()
main = do
    putStrLn "========================================================"
    putStrLn " Topological Shogi Engine: Omni Tensor Generator V2"
    putStrLn " (Linux Native Build / 0-ply Tensor Initialization)"
    putStrLn "========================================================"
    
    let k_target = 7 :: CInt
    let numStates = 4096 :: Int -- Simplified state space size for this generation
    let maxIter = 50 :: Int     -- Number of sweep iterations to converge
    
    putStrLn $ "Allocating Tensors for k=" ++ show k_target ++ " Space=" ++ show numStates ++ "..."
    
    -- Allocate necessary arrays
    potentials <- mallocArray numStates
    solvedKMinus1 <- mallocArray (numStates `div` 7 + 1)
    blockNodes <- mallocArray 16
    activeIndices <- mallocArray 16
    changedMask <- malloc :: IO (Ptr CUInt)
    seg <- mallocBytes sizeof_SparseHasseTransitionSegment
    
    -- Initialize potentials: Rank 0 Attractor at index 0 (Checkmate = 0.0), others High
    forM_ [0 .. numStates - 1] $ \i -> do
        pokeElemOff potentials i (if i == 0 then 0.0 else 1000.0)
        
    -- Initialize solved k-1 array (fake data representing solved endgame potentials)
    forM_ [0 .. (numStates `div` 7)] $ \i -> do
        pokeElemOff solvedKMinus1 i (fromIntegral i * 1.5 :: CFloat)
        
    -- Setup dummy graph edges inside the segment (simulate 16 lanes)
    -- Using ptr arithmetic to initialize target_indices
    let targetIdxOffset = 512
    forM_ [0 .. 15] $ \lane -> do
        let nodeIdx = fromIntegral ((lane * 17) `mod` numStates) :: CUInt
        pokeElemOff blockNodes lane nodeIdx
        pokeElemOff activeIndices lane nodeIdx
        
        -- Set targets to lead towards index 0 (Attractor)
        forM_ [0 .. 7] $ \j -> do
            let target = if j == 0 then 0 else fromIntegral ((nodeIdx - 1) `mod` fromIntegral numStates) :: CUInt
            let ptr = plusPtr seg (targetIdxOffset + (lane * 8 + j) * 4)
            poke (ptr :: Ptr CUInt) target

    putStrLn "Running exact sweep iterations (Log-Entropy + Homotopy Contraction)..."
    
    -- Main Sweep Loop
    let loop iter = when (iter < maxIter) $ do
            
            -- 全状態空間(4096)を16のチャンクに分割してスイープ
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

                c_get_hasse_branching_and_sinks blockNodes seg potentials solvedKMinus1
                
                chunkChangedMask <- malloc :: IO (Ptr CUInt)
                poke chunkChangedMask 0
                c_sweep_tropical_boundary_v2 potentials seg activeIndices chunkChangedMask
                
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
    
    putStrLn "Sweep Complete. Formatting into TT-SVD TNRG..."
    
    -- Write to static_joseki.bin
    c_compress_to_mps potentials (fromIntegral numStates) k_target

    -- Cleanup
    free potentials
    free solvedKMinus1
    free blockNodes
    free activeIndices
    free changedMask
    free seg

    putStrLn "Generation pipeline finished successfully."
