#ifndef RENDERSTATE_H
#define RENDERSTATE_H


#include "GLMInc.h"

namespace OpenGL {

//深度测试比较函数
enum DepthFunction {
    DepthFunc_NEVER,
    DepthFunc_LESS,
    DepthFunc_EQUAL,
    DepthFunc_LEQUAL,
    DepthFunc_GREATER,
    DepthFunc_NOTEQUAL,
    DepthFunc_GEQUAL,
    DepthFunc_ALWAYS,
};

//混合因子
enum BlendFactor {
    BlendFactor_ZERO,
    BlendFactor_ONE,
    BlendFactor_SRC_COLOR,
    BlendFactor_SRC_ALPHA,
    BlendFactor_DST_COLOR,
    BlendFactor_DST_ALPHA,
    BlendFactor_ONE_MINUS_SRC_COLOR,
    BlendFactor_ONE_MINUS_SRC_ALPHA,
    BlendFactor_ONE_MINUS_DST_COLOR,
    BlendFactor_ONE_MINUS_DST_ALPHA,
};

//混合操作
enum BlendFunction {
    BlendFunc_ADD,
    BlendFunc_SUBTRACT,
    BlendFunc_REVERSE_SUBTRACT,
    BlendFunc_MIN,
    BlendFunc_MAX,
};

//多边形渲染模式
enum PolygonMode {
    PolygonMode_POINT,
    PolygonMode_LINE,
    PolygonMode_FILL,
};

//配置颜色混合参数
struct BlendParameters {
    BlendFunction blendFuncRgb = BlendFunc_ADD;
    BlendFactor blendSrcRgb = BlendFactor_ONE;
    BlendFactor blendDstRgb = BlendFactor_ZERO;

    BlendFunction blendFuncAlpha = BlendFunc_ADD;
    BlendFactor blendSrcAlpha = BlendFactor_ONE;
    BlendFactor blendDstAlpha = BlendFactor_ZERO;

    void SetBlendFactor(BlendFactor src, BlendFactor dst) {
        blendSrcRgb = src;
        blendSrcAlpha = src;
        blendDstRgb = dst;
        blendDstAlpha = dst;
    }

    void SetBlendFunc(BlendFunction func) {
        blendFuncRgb = func;
        blendFuncAlpha = func;
    }
};

//图元类型
enum PrimitiveType {
    Primitive_POINT,
    Primitive_LINE,
    Primitive_TRIANGLE,
};

//汇总所有渲染状态配置
struct RenderStates {
    bool blend = false;
    BlendParameters blendParams;

    bool depthTest = false;
    bool depthMask = true;
    DepthFunction depthFunc = DepthFunc_LESS;

    bool cullFace = false;
    PrimitiveType primitiveType = Primitive_TRIANGLE;
    PolygonMode polygonMode = PolygonMode_FILL;

    float lineWidth = 1.f;
};

//帧缓冲清除配置
struct ClearStates {
    bool depthFlag = false;
    bool colorFlag = false;
    glm::vec4 clearColor = glm::vec4(0.f);
    float clearDepth = 1.f;
};

}



#endif
