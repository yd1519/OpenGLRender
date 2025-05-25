#ifndef SHADERPROGRAMSOFT_H
#define SHADERPROGRAMSOFT_H

#include <vector>
#include "Base/UUID.h"
#include "Render/ShaderProgram.h"
#include "Render/Software/ShaderSoft.h"


namespace OpenGL {
class ShaderProgramSoft : public ShaderProgram {
public:
    int getId() const override {
        return uuid_.get();
    }

    void addDefine(const std::string& def) override {
        defines_.emplace_back(def);
    }
    // 设置顶点 / 片段着色器
    bool SetShaders(std::shared_ptr<ShaderSoft> vs, std::shared_ptr<ShaderSoft> fs) {
        vertexShader_ = std::move(vs);
        fragmentShader_ = std::move(fs);

        // defines
        auto& defineDesc = vertexShader_->getDefines();
        //  创建与宏定义数量匹配的缓冲区（1字节/宏）
        definesBuffer_ = MemoryUtils::makeBuffer<uint8_t>(defineDesc.size());
        // 将缓冲区绑定到两个着色器
        vertexShader_->bindDefines(definesBuffer_.get());
        fragmentShader_->bindDefines(definesBuffer_.get());
        // 初始化缓冲区（默认所有宏未启用）
        memset(definesBuffer_.get(), 0, sizeof(uint8_t) * defineDesc.size());
        // 标记已启用的宏（1表示启用）
        for (auto& name : defines_) {
            for (int i = 0; i < defineDesc.size(); i++) {
                if (defineDesc[i] == name) {
                    definesBuffer_.get()[i] = 1;
                }
            }
        }

        // 内置变量绑定
        vertexShader_->bindBuiltin(&builtin_);
        fragmentShader_->bindBuiltin(&builtin_);

        // Uniform缓冲区初始化
        uniformBuffer_ = MemoryUtils::makeBuffer<uint8_t>(vertexShader_->getShaderUniformsSize());
        vertexShader_->bindShaderUniforms(uniformBuffer_.get());
        fragmentShader_->bindShaderUniforms(uniformBuffer_.get());

        return true;
    }

    //----------------------资源绑定接口---------------------------------------//
    inline void bindVertexAttributes(void* ptr) {
        vertexShader_->bindShaderAttributes(ptr);
    }

    inline void bindUniformBlockBuffer(void* data, size_t len, int binding) {
        int offset = vertexShader_->GetUniformOffset(binding);
        memcpy(uniformBuffer_.get() + offset, data, len);
    }
    // 绑定纹理采样器
    inline void bindUniformSampler(std::shared_ptr<SamplerSoft>& sampler, int binding) {
        int offset = vertexShader_->GetUniformOffset(binding); // 在uniform缓冲区的偏移量
        auto** ptr = reinterpret_cast<SamplerSoft**>(uniformBuffer_.get() + offset);
        *ptr = sampler.get();
    }

    inline void bindVertexShaderVaryings(void* ptr) {
        vertexShader_->bindShaderVaryings(ptr);
    }

    inline void bindFragmentShaderVaryings(void* ptr) {
        fragmentShader_->bindShaderVaryings(ptr);
    }

    inline size_t getShaderVaryingsSize() {
        return vertexShader_->getShaderVaryingsSize();
    }

    inline int getUniformLocation(const std::string& name) {
        return vertexShader_->getUniformLocation(name);
    }

    inline ShaderBuiltin& getShaderBuiltin() {
        return builtin_;
    }

    inline void execVertexShader() {
        vertexShader_->shaderMain();
    }

    //准备执行片段着色器（如初始化导数计算等）
    inline void prepareFragmentShader() {
        fragmentShader_->prepareExecMain();
    }

    inline void execFragmentShader() {
        fragmentShader_->setupSamplerDerivative();// 设置纹理导数
        fragmentShader_->shaderMain();
    }

    inline std::shared_ptr<ShaderProgramSoft> clone() const {
        auto ret = std::make_shared<ShaderProgramSoft>(*this);

        ret->vertexShader_ = vertexShader_->clone();
        ret->fragmentShader_ = fragmentShader_->clone();
        ret->vertexShader_->bindBuiltin(&ret->builtin_);
        ret->fragmentShader_->bindBuiltin(&ret->builtin_);

        return ret;
    }

private:
    ShaderBuiltin builtin_; // 着色器内置变量
    std::vector<std::string> defines_; //启用的宏定义列表

    std::shared_ptr<ShaderSoft> vertexShader_;
    std::shared_ptr<ShaderSoft> fragmentShader_;

    std::shared_ptr<uint8_t> definesBuffer_;  //宏定义启用状态缓冲区  0->false; 1->true
    std::shared_ptr<uint8_t> uniformBuffer_;   //Uniform变量数据缓冲区

private:
    UUID<ShaderProgramSoft> uuid_;
};

}

#endif
