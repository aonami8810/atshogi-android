module PersistentSafety where

-- Phase 1: Opening
-- Dummy implementation of H1 persistent barrier validation
checkBarrierLife :: Int -> Int -> Bool
checkBarrierLife birth death = (death - birth) >= 3
