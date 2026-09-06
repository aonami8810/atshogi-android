{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE DataKinds #-}
{-# LANGUAGE TypeFamilies #-}
{-# LANGUAGE TypeOperators #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE UndecidableInstances #-}

module MERARenormalization (
    MERALayer(..),
    contractMERALocalCone,
    ScaleFactor
) where

import Foreign.Ptr
import Foreign.C.Types
import GHC.TypeLits
import Data.Proxy

-- | MERA Layer Representation (Layer 0 = 1-ply, Layer 9 = 512-ply)
data MERALayer (level :: Nat)

-- | Scale Renormalization Factor Calculation
type family ScaleFactor (l :: Nat) :: Nat where
  ScaleFactor 0 = 1
  ScaleFactor n = 2 GHC.TypeLits.* ScaleFactor (n - 1)

-- C++ FFI Import for the NEON/Portable kernel
foreign import ccall unsafe "contract_mera_step_neon"
    c_contract_mera_step :: Ptr CFloat -> Ptr CFloat -> Ptr CFloat -> Ptr CFloat -> Ptr CFloat -> CInt -> IO ()

-- | Deterministic MERA Local Causal Cone Contraction
contractMERALocalCone
    :: forall l. (KnownNat l, KnownNat (ScaleFactor l))
    => Proxy l                               -- ^ Current Layer Level
    -> Ptr CFloat                            -- ^ Output Potentials Array (chi)
    -> Ptr CFloat                            -- ^ Left Potentials Array (chi)
    -> Ptr CFloat                            -- ^ Right Potentials Array (chi)
    -> Ptr CFloat                            -- ^ Disentangler U Array (chi*chi*chi*chi)
    -> Ptr CFloat                            -- ^ Isometry V Array (chi*chi*chi)
    -> Int                                   -- ^ Bond Dimension (chi <= 181)
    -> IO ()
contractMERALocalCone _ outPtr leftPtr rightPtr uPtr vPtr chi = do
    let levelVal = natVal (Proxy :: Proxy l)
        _scaleVal = natVal (Proxy :: Proxy (ScaleFactor l))

    -- Safety Audit: Prevent out-of-bounds layer execution
    if levelVal >= 9
        then error "Topological Boundary Overflow: MERA layer cannot exceed L=9 (512-ply)."
        else if chi > 181 
             then error "TNRG Dimension Overflow: Bond dimension chi cannot exceed 181."
             else c_contract_mera_step outPtr leftPtr rightPtr uPtr vPtr (fromIntegral chi)
