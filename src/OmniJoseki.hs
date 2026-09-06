{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE EmptyDataDecls #-}

module OmniJoseki 
    ( OmniJosekiGenerator
    , createGenerator
    , destroyGenerator
    , distillOracleEvaluation
    , wittenComplexBackpropagation
    , applyOrbifoldProjection
    , compressAndExportTTSVD
    , generateOmniJoseki
    ) where

import Foreign.C.Types
import Foreign.C.String
import Foreign.Ptr
import Control.Exception (bracket)

-- Opaque type representing the C++ OmniJosekiGenerator struct
data OmniJosekiGeneratorStruct
type OmniJosekiGenerator = Ptr OmniJosekiGeneratorStruct

-- FFI Declarations
foreign import ccall "create_generator" 
    c_create_generator :: IO OmniJosekiGenerator

foreign import ccall "destroy_generator" 
    c_destroy_generator :: OmniJosekiGenerator -> IO ()

foreign import ccall "distill_oracle_evaluation" 
    c_distill_oracle_evaluation :: OmniJosekiGenerator -> CString -> IO ()

foreign import ccall "witten_complex_backpropagation" 
    c_witten_complex_backpropagation :: OmniJosekiGenerator -> IO ()

foreign import ccall "apply_orbifold_projection" 
    c_apply_orbifold_projection :: OmniJosekiGenerator -> IO ()

foreign import ccall "compress_and_export_ttsvd" 
    c_compress_and_export_ttsvd :: OmniJosekiGenerator -> CString -> IO ()

-- Haskell Wrappers

-- | Creates a new OmniJosekiGenerator instance.
createGenerator :: IO OmniJosekiGenerator
createGenerator = c_create_generator

-- | Destroys an OmniJosekiGenerator instance.
destroyGenerator :: OmniJosekiGenerator -> IO ()
destroyGenerator = c_destroy_generator

-- | Distill Oracle (Suisho5/NNUE) evaluation.
distillOracleEvaluation :: OmniJosekiGenerator -> String -> IO ()
distillOracleEvaluation gen nnuePath = 
    withCString nnuePath $ \c_nnuePath ->
        c_distill_oracle_evaluation gen c_nnuePath

-- | Execute Witten complex backpropagation on Tropical Semiring.
wittenComplexBackpropagation :: OmniJosekiGenerator -> IO ()
wittenComplexBackpropagation = c_witten_complex_backpropagation

-- | Apply Orbifold G_351 quotient space projection.
applyOrbifoldProjection :: OmniJosekiGenerator -> IO ()
applyOrbifoldProjection = c_apply_orbifold_projection

-- | Execute TT-SVD compression and export to binary.
compressAndExportTTSVD :: OmniJosekiGenerator -> String -> IO ()
compressAndExportTTSVD gen outputPath = 
    withCString outputPath $ \c_outputPath ->
        c_compress_and_export_ttsvd gen c_outputPath

-- | High-level wrapper to run the full Omni-Joseki generation pipeline safely.
generateOmniJoseki :: String -> String -> IO ()
generateOmniJoseki nnuePath outputPath = 
    bracket createGenerator destroyGenerator $ \gen -> do
        putStrLn "[Haskell] Starting Omni-Joseki Tensor Generation..."
        distillOracleEvaluation gen nnuePath
        wittenComplexBackpropagation gen
        applyOrbifoldProjection gen
        compressAndExportTTSVD gen outputPath
        putStrLn "[Haskell] Generation Complete."
