module Main where

import OrientedMatroid.TopologicalFFI
import System.IO
import System.Exit (exitSuccess)
import Control.Monad (forever)
import Control.Concurrent.MVar
import Data.Word (Word32)
import Data.Bits ((.|.), (.&.), shiftL, shiftR)
import Foreign.Ptr (Ptr, nullPtr)
import Data.Char (ord, chr)

-- USI ハンドラの状態
data EngineState = EngineState
    { engineStatePtr :: !(Ptr UsiEngineState) -- C++ へのmmapポインタ
    , currentPotentials :: !(Ptr Float)        -- 電位配列 (C++側メモリ)
    , currentMovesApplied :: ![String]        -- positionコマンドで適用された現在の手順
    }

main :: IO ()
main = do
    -- バッファリングを完全にオフにして、UI(ShogiHome等)へのリアルタイム出力を保証
    hSetBuffering stdout NoBuffering
    hSetBuffering stdin LineBuffering

    -- スレッドセーフなステート管理用の MVar (初期状態は未ロード)
    stateMVar <- newMVar Nothing

    -- USI メインループ
    forever $ do
        line <- getLine
        let cmd = words line
        case cmd of
            ["usi"] -> do
                putStrLn "id name ATShogi-OM-Extreme-V5"
                putStrLn "id author aonami8810"
                putStrLn "usiok"

            ["isready"] -> do
                mState <- takeMVar stateMVar
                case mState of
                    Just s -> do
                        putMVar stateMVar (Just s)
                        putStrLn "readyok"
                    Nothing -> do
                        -- 1.0 MB の結晶 'static_joseki.bin' を mmap ロード
                        let binPath = "static_joseki.bin"
                        ptr <- initUsiEngine binPath
                        if ptr == nullPtr
                            then do
                                hPutStrLn stderr "🚨 [ERROR] static_joseki.bin のロードに失敗しました"
                                putMVar stateMVar Nothing
                            else do
                                hPutStrLn stderr "🎉 [SUCCESS] 1.0 MB 定跡を L3 キャッシュへ mmap マウント完了"
                                let dummyPot = nullPtr
                                putMVar stateMVar (Just $ EngineState ptr dummyPot [])
                                putStrLn "readyok"

            ("position" : posSpec) -> do
                mState <- takeMVar stateMVar
                case mState of
                    Nothing -> do
                        hPutStrLn stderr "🚨 position: エンジンが初期化されていません"
                        putMVar stateMVar Nothing
                    Just s -> do
                        let moves = parseMoves posSpec
                        let updatedState = s { currentMovesApplied = moves }
                        putMVar stateMVar (Just updatedState)

            ("go" : _) -> do
                mState <- readMVar stateMVar
                case mState of
                    Nothing -> putStrLn "bestmove resign"
                    Just s -> do
                        -- 1. 現在局面における合法手の生成 (実際は盤面状態から算出)
                        let candidates = generateDummyCandidates (currentMovesApplied s)

                        -- 2. C++ FFI を叩き、0ms で最急降下最善手を決定
                        bestMoveW32 <- evaluateBestMoveUsi (engineStatePtr s) candidates (currentPotentials s)

                        -- 3. 指し手を Word32 から USI 形式文字列 ("7g7f"等) へ復元して出力
                        let bestMoveStr = unpackUsiMove bestMoveW32
                        putStrLn $ "bestmove " ++ bestMoveStr

            ["quit"] -> do
                mState <- takeMVar stateMVar
                case mState of
                    Just s -> destroyUsiEngine (engineStatePtr s)
                    Nothing -> return ()
                exitSuccess

            _ -> return ()

-- position コマンドの解析ヘルパー
parseMoves :: [String] -> [String]
parseMoves [] = []
parseMoves ("startpos" : xs) = parseMoves xs
parseMoves ("moves" : xs) = xs
parseMoves xs = xs

-- --- USI指し手 (TEXT) と Word32 (FFI) の双方向シリアライズ ---

-- USIの指し手を Word32 にパックする (4手座標＋成フラグを1ビット単位でビットパック)
packUsiMove :: String -> Word32
packUsiMove s =
    case s of
        (fx:fy:tx:ty:rest) ->
            let fX = fromIntegral (ord fx - ord '0')
                fY = fromIntegral (ord fy - ord 'a' + 1)
                tX = fromIntegral (ord tx - ord '0')
                tY = fromIntegral (ord ty - ord 'a' + 1)
                promo = if not (null rest) && head rest == '+' then 1 else 0
            in fX .|. (fY `shiftL` 8) .|. (tX `shiftL` 16) .|. (tY `shiftL` 24) .|. (promo `shiftL` 31)
        _ -> 0

-- Word32 から USIの指し手文字列を復元する
unpackUsiMove :: Word32 -> String
unpackUsiMove w =
    if w == 0xFFFFFFFF || w == 0
        then "resign"
        else
            let fX = w .&. 0xFF
                fY = (w `shiftR` 8) .&. 0xFF
                tX = (w `shiftR` 16) .&. 0xFF
                tY = (w `shiftR` 24) .&. 0xFF
                promo = (w `shiftR` 31) .&. 1
                fxChar = chr (fromIntegral (fX + fromIntegral (ord '0')))
                fyChar = chr (fromIntegral (fY + fromIntegral (ord 'a' - 1)))
                txChar = chr (fromIntegral (tX + fromIntegral (ord '0')))
                tyChar = chr (fromIntegral (tY + fromIntegral (ord 'a' - 1)))
                promoStr = if promo == 1 then "+" else ""
            in [fxChar, fyChar, txChar, tyChar] ++ promoStr

-- デモ用の合法手配列生成
generateDummyCandidates :: [String] -> [UsiMoveCandidate]
generateDummyCandidates currentMoves =
    let isGote = length currentMoves `mod` 2 == 1
        bestMoveStr = if not isGote then "7g7f" else "3c3d"
        bestW32 = packUsiMove bestMoveStr

        -- ダミーの候補手リスト (最善手を含む3手)
        cand1 = UsiMoveCandidate bestW32 1 (if isGote then 0 else 1) -- 遷移先ノード 1
        cand2 = UsiMoveCandidate (packUsiMove "2g2f") 2 (if isGote then 0 else 1) -- 遷移先ノード 2
        cand3 = UsiMoveCandidate (packUsiMove "8g8f") 3 (if isGote then 0 else 1) -- 遷移先ノード 3
    in [cand1, cand2, cand3]
