#ifndef SHADERSOFT_H
#define SHADERSOFT_H

#include <functional>
#include "SamplerSoft.h"

namespace OpenGL {
// 用于管理2x2像素分块的处理状态
class PixelQuadContext;
constexpr float PI = 3.14159265359;

class UniformDesc {
public:
    UniformDesc(const char* name, int offset)
        : name(name), offset(offset) {};

public:
    std::string name;
    int offset; // 在Uniform缓冲区中的偏移量（字节）
};

// 存储2x2像素分块中四个像素的中间结果，用于导数计算
struct DerivativeContext {
    float* p0 = nullptr;
    float* p1 = nullptr;
    float* p2 = nullptr;
    float* p3 = nullptr;
};

struct ShaderBuiltin {
    // ----------顶点着色器输出----------
    glm::vec4 Position = glm::vec4{ 0.f };
    float PointSize = 1.f;

    // ----------片段着色器输入--------
    glm::vec4 FragCoord;
    bool FrontFacing;

    // ----------片段着色器输出-------------
    glm::vec4 FragColor;
    bool discard = false;

    // ---------导数计算上下文-------------
    DerivativeContext dfCtx;
};

class ShaderSoft {
public:
    virtual void shaderMain() = 0;                      //着色器入口函数（类似GLSL的main）

    virtual void bindDefines(void* ptr) = 0;            //绑定预处理宏
    virtual void bindBuiltin(void* ptr) = 0;            //绑定内置变量
    virtual void bindShaderAttributes(void* ptr) = 0;   //绑定顶点属性
    virtual void bindShaderUniforms(void* ptr) = 0;     //绑定uniform变量
    virtual void bindShaderVaryings(void* ptr) = 0;     //绑定varying变量

    virtual size_t getShaderUniformsSize() = 0;         //获取uniform缓冲区大小
    virtual size_t getShaderVaryingsSize() = 0;         //获取varying数据大小

    virtual std::vector<std::string>& getDefines() = 0; //获取预处理宏列表
    virtual std::vector<UniformDesc>& getUniformsDesc() = 0;//获取uniform描述符

    virtual std::shared_ptr<ShaderSoft> clone() = 0;    //克隆接口（原型模式）

public:
    static inline glm::ivec2 textureSize(Sampler2DSoft<RGBA>* sampler, int lod) {
        auto& buffer = sampler->getTexture()->getImage().getBuffer(lod);
        return { buffer->width, buffer->height };
    }

    static inline glm::ivec2 textureSize(Sampler2DSoft<float>* sampler, int lod) {
        auto& buffer = sampler->getTexture()->getImage().getBuffer(lod);
        return { buffer->width, buffer->height };
    }

    static inline glm::vec4 texture(Sampler2DSoft<RGBA>* sampler, glm::vec2 coord) {
        glm::vec4 ret = sampler->texture2D(coord);
        return ret / 255.f;
    }

    static inline float texture(Sampler2DSoft<float>* sampler, glm::vec2 coord) {
        float ret = sampler->texture2D(coord);
        return ret;
    }

    static inline glm::vec4 texture(SamplerCubeSoft<RGBA>* sampler, glm::vec3 coord) {
        glm::vec4 ret = sampler->textureCube(coord);
        return ret / 255.f;
    }

    static inline glm::vec4 textureLod(Sampler2DSoft<RGBA>* sampler, glm::vec2 coord, float lod = 0.f) {
        glm::vec4 ret = sampler->texture2DLod(coord, lod);
        return ret / 255.f;
    }

    static inline glm::vec4 textureLod(SamplerCubeSoft<RGBA>* sampler, glm::vec3 coord, float lod = 0.f) {
        glm::vec4 ret = sampler->textureCubeLod(coord, lod);
        return ret / 255.f;
    }

    static inline glm::vec4 textureLodOffset(Sampler2DSoft<RGBA>* sampler, glm::vec2 coord, float lod, glm::ivec2 offset) {
        glm::vec4 ret = sampler->texture2DLodOffset(coord, lod, offset);
        return ret / 255.f;
    }

public:
    //-----------------------运行时数据-----------------------------
    ShaderBuiltin* gl = nullptr; 
    std::function<float(BaseSampler<RGBA>*)> texLodFunc;

    // 自动计算纹理LOD级别
    float getSampler2DLod(BaseSampler<RGBA>* sampler) const {
        auto& dfCtx = gl->dfCtx;
        size_t dfOffset = getSamplerDerivativeOffset(sampler);

        // 获取像素块四个角的UV坐标
        auto* coord0 = (glm::vec2*)(dfCtx.p0 + dfOffset);
        auto* coord1 = (glm::vec2*)(dfCtx.p1 + dfOffset);
        auto* coord2 = (glm::vec2*)(dfCtx.p2 + dfOffset);
        auto* coord3 = (glm::vec2*)(dfCtx.p3 + dfOffset);

        // 计算纹理坐标导数（屏幕空间）
        glm::vec2 texSize = glm::vec2(sampler->width(), sampler->height());
        glm::vec2 dx = glm::vec2(*coord1 - *coord0);
        glm::vec2 dy = glm::vec2(*coord2 - *coord0);
        dx *= texSize;
        dy *= texSize;
        float d = glm::max(glm::dot(dx, dx), glm::dot(dy, dy));
        return glm::max(0.5f * glm::log2(d), 0.0f);
    }

    virtual void prepareExecMain() {
        texLodFunc = std::bind(&ShaderSoft::getSampler2DLod, this, std::placeholders::_1);
    }

    virtual size_t getSamplerDerivativeOffset(BaseSampler<RGBA>* sampler) const {
        return 0;
    }

    virtual void setupSamplerDerivative() {}

    int getUniformLocation(const std::string& name) {
        auto& desc = getUniformsDesc();
        for (int i = 0; i < desc.size(); i++) {
            if (desc[i].name == name) {
                return i;
            }
        }
        return -1;
    };

    int GetUniformOffset(int loc) {
        auto& desc = getUniformsDesc();
        if (loc < 0 || loc > desc.size()) {
            return -1;
        }
        return desc[loc].offset;
    };
};


#define CREATE_SHADER_OVERRIDE                          \
    ShaderDefines *def = nullptr;                         \
    ShaderAttributes *a = nullptr;                        \
    ShaderUniforms *u = nullptr;                          \
    ShaderVaryings *v = nullptr;                          \
                                                        \
    void bindDefines(void *ptr) override {                \
    def = static_cast<ShaderDefines *>(ptr);            \
    }                                                     \
                                                        \
    void bindBuiltin(void *ptr) override {                \
    gl = static_cast<ShaderBuiltin *>(ptr);             \
    }                                                     \
                                                        \
    void bindShaderAttributes(void *ptr) override {       \
    a = static_cast<ShaderAttributes *>(ptr);           \
    }                                                     \
                                                        \
    void bindShaderUniforms(void *ptr) override {         \
    u = static_cast<ShaderUniforms *>(ptr);             \
    }                                                     \
                                                        \
    void bindShaderVaryings(void *ptr) override {         \
    v = static_cast<ShaderVaryings *>(ptr);             \
    }                                                     \
                                                        \
    size_t getShaderUniformsSize() override {             \
    return sizeof(ShaderUniforms);                      \
    }                                                     \
                                                        \
    size_t getShaderVaryingsSize() override {             \
    return sizeof(ShaderVaryings);                      \
    }

#define CREATE_SHADER_CLONE(T)                          \
    std::shared_ptr<ShaderSoft> clone() override {        \
        return std::make_shared<T>(*this);                  \
    }



}

#endif
