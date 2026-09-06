#include "StratifiedCobordism40.h"
#include <algorithm>

namespace atshogi::cobordism {

float evaluateStratifiedCobordism40(float vEnd, float vTrans, float vMid, float vOpen, int k) {
    if (k <= 7) {
        return vEnd;
    } else if (k <= 14) {
        float t = static_cast<float>(k - 7) / 7.0f;
        float w = hermiteSmooth5th(t);
        return vEnd * (1.0f - w) + vTrans * w;
    } else if (k <= 28) {
        float t = static_cast<float>(k - 14) / 14.0f;
        float w = hermiteSmooth5th(t);
        return vTrans * (1.0f - w) + vMid * w;
    } else {
        float t = static_cast<float>(std::min(k, 40) - 28) / 12.0f;
        float w = hermiteSmooth5th(t);
        return vMid * (1.0f - w) + vOpen * w;
    }
}

} // namespace atshogi::cobordism

extern "C" {

float atshogi_stratified_cobordism40_eval(float vEnd, float vTrans, float vMid, float vOpen, int k) {
    return atshogi::cobordism::evaluateStratifiedCobordism40(vEnd, vTrans, vMid, vOpen, k);
}

}
