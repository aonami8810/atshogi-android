#include <math.h>

extern "C" {

/**
 * 中盤のMERA/PEPS局所評価値と、終盤のEGTB詰みポテンシャルをコボルディズムシグモイドで境界なく接着する
 */
float bridge_potentials_cobordism(float vMid, float vEnd, int k) {
    const float k_star = 7.0f; // moto g05 の1MB L3キャッシュ常駐限界駒数
    const float alpha = 1.5f;  // コボルディズム遷移の鋭さパラメータ
    
    // 駒数 k に基づく滑らかなシグモイド重み w(k)
    // k -> 7に近づくにつれて、EGTBの「詰み重力」が中盤局面を支配し始める
    float weight = 1.0f / (1.0f + expf(alpha * ((float)k - k_star)));
    
    // コボルディズム・ブレンド
    return (1.0f - weight) * vMid + weight * vEnd;
}

}
