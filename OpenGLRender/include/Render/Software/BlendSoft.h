#ifndef BLENDSOFT_H
#define BLENDSOFT_H

#include "Render/PipelineStates.h"

namespace OpenGL {

    template<typename T>/*计算混合因子*/
    T calcBlendFactor(const T& src, const float& srcAlpha,
        const T& dst, const float& dstAlpha,
        const BlendFactor& factor) {
        switch (factor) {
        case BlendFactor_ZERO:                return T(0.f);
        case BlendFactor_ONE:                 return T(1.f);
        case BlendFactor_SRC_COLOR:           return src;
        case BlendFactor_SRC_ALPHA:           return T(srcAlpha);
        case BlendFactor_DST_COLOR:           return dst;
        case BlendFactor_DST_ALPHA:           return T(dstAlpha);
        case BlendFactor_ONE_MINUS_SRC_COLOR: return T(1.f) - src;
        case BlendFactor_ONE_MINUS_SRC_ALPHA: return T(1.f - srcAlpha);
        case BlendFactor_ONE_MINUS_DST_COLOR: return T(1.f) - dst;
        case BlendFactor_ONE_MINUS_DST_ALPHA: return T(1.f - dstAlpha);
        }
        return T(0.f);
    }

    template<typename T>/*应用混合函数*/
    T calcBlendFunc(const T& src, const T& dst, const BlendFunction& func) {
        switch (func) {
        case BlendFunc_ADD:               return src + dst;
        case BlendFunc_SUBTRACT:          return src - dst;
        case BlendFunc_REVERSE_SUBTRACT:  return dst - src;
        case BlendFunc_MIN:               return glm::min(src, dst);
        case BlendFunc_MAX:               return glm::max(src, dst);
        }
        return src + dst;
    }

    glm::vec4 calcBlendColor(glm::vec4& src, glm::vec4& dst, const BlendParameters& params) {
        // RGB通道处理
        auto srcRgb = glm::vec3(src);
        auto dstRgb = glm::vec3(dst);
        // 计算RGB混合因子
        auto srcRgbF = calcBlendFactor<glm::vec3>(srcRgb, src.a, dstRgb, dst.a, params.blendSrcRgb);
        auto dstRgbF = calcBlendFactor<glm::vec3>(srcRgb, src.a, dstRgb, dst.a, params.blendDstRgb);
        //应用RGB混合函数
        auto retRgb = calcBlendFunc<glm::vec3>(srcRgb * srcRgbF, dstRgb * dstRgbF, params.blendFuncRgb);
        // Alpha通道处理（独立计算）
        auto srcAlphaF = calcBlendFactor<float>(src.a, src.a, dst.a, dst.a, params.blendSrcAlpha);
        auto dstAlphaF = calcBlendFactor<float>(src.a, src.a, dst.a, dst.a, params.blendDstAlpha);
        auto retAlpha = calcBlendFunc<float>(src.a * srcAlphaF, dst.a * dstAlphaF, params.blendFuncAlpha);

        return { retRgb, retAlpha };// 组合RGB和Alpha
    }
}
#endif