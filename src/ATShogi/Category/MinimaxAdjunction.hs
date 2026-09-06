{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE TypeFamilies #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE AllowAmbiguousTypes #-}

module ATShogi.Category.MinimaxAdjunction (
    TensorSpace(..),
    GameFunctor(..),
    MinimaxAdjunction(..),
    evaluateLinkingBraid
) where

import Foreign.Ptr
import Foreign.C.Types

class TensorSpace t where
  type Scalar t
  innerProduct :: t -> t -> Scalar t

class (TensorSpace (Dom f), TensorSpace (Cod f)) => GameFunctor f where
  type Dom f
  type Cod f
  mapTensor  :: f -> Dom f -> Cod f

-- | 2-Categoryにおける先手・後手のミニマックス随伴関係の静的定義
class (GameFunctor fs, GameFunctor fg, Dom fs ~ Cod fg, Cod fs ~ Dom fg) 
      => MinimaxAdjunction fs fg where
  unit   :: Dom fs -> (Dom fs -> Cod fs) -> (Cod fs -> Dom fs) -> Dom fs
  unit t fs_opt fg_opt = fg_opt (fs_opt t)

  counit :: Cod fs -> (Cod fs -> Dom fs) -> (Dom fs -> Cod fs) -> Cod fs
  counit t fg_opt fs_opt = fs_opt (fg_opt t)

-- FFI 定義
foreign import ccall unsafe "calculate_braid_linking_number_neon"
    c_calculate_braid_linking_number :: Ptr CFloat -> Ptr CFloat -> CInt -> IO CFloat

evaluateLinkingBraid :: Ptr CFloat -> Ptr CFloat -> Int -> IO Float
evaluateLinkingBraid pathPtr optPtr len = do
    val <- c_calculate_braid_linking_number pathPtr optPtr (fromIntegral len)
    return (realToFrac val)
