package shogi.oex;

interface IEngineServiceCallback {
    // OEX callback
    void onReceiveResponse(String cmd);
}
