package shogi.oex;

import shogi.oex.IEngineServiceCallback;

interface IEngineService {
    void registerCallback(IEngineServiceCallback callback);
    void unregisterCallback(IEngineServiceCallback callback);
    void writeCommand(String cmd);

    // ShogiHome dynamically calls getInfo
    String getInfo();
}
