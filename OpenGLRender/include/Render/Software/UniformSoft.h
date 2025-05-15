#ifndef UNIFORMSOFT_H
#define UNIFORMSOFT_H

#include "Render/Uniform.h"
#include "Base/Logger.h"
#include "Render/Software/ShaderProgramSoft.h"
namespace OpenGL {
class UniformBlockSoft : public UniformBlock {
public:
    UniformBlockSoft(const std::string& name, int size) : UniformBlock(name, size) {
        buffer_.resize(size);
    }

    // 获取该Uniform Block在着色器程序中的位置
    int getLocation(ShaderProgram& program) override {
        auto programSoft = dynamic_cast<ShaderProgramSoft*>(&program);
        return programSoft->getUniformLocation(name_);
    }

    // 将该Uniform Block绑定到着色器程序
    void bindProgram(ShaderProgram& program, int location) override {
        auto programSoft = dynamic_cast<ShaderProgramSoft*>(&program);
        programSoft->bindUniformBlockBuffer(buffer_.data(), buffer_.size(), location);
    }

    void setSubData(void* data, int len, int offset) override {
        memcpy(buffer_.data() + offset, data, len);
    }

    void setData(void* data, int len) override {
        setSubData(data, len, 0);
    }

private:
    std::vector<uint8_t> buffer_;
};

class UniformSamplerSoft : public UniformSampler {
public:
    explicit UniformSamplerSoft(const std::string& name, TextureType type, TextureFormat format)
        : UniformSampler(name, type, format) {
        switch (type) {
        case TextureType_2D:
            switch (format) {
            case TextureFormat_RGBA8:
                sampler_ = std::make_shared<Sampler2DSoft<RGBA>>();
                break;
            case TextureFormat_FLOAT32:
                sampler_ = std::make_shared<Sampler2DSoft<float>>();
                break;
            }
            break;
        case TextureType_CUBE:
            switch (format) {
            case TextureFormat_RGBA8:
                sampler_ = std::make_shared<SamplerCubeSoft<RGBA>>();
                break;
            case TextureFormat_FLOAT32:
                sampler_ = std::make_shared<SamplerCubeSoft<float>>();
                break;
            }
            break;
        default:
            sampler_ = nullptr;
            break;
        }
    }

    int getLocation(ShaderProgram& program) override {
        auto programSoft = dynamic_cast<ShaderProgramSoft*>(&program);
        return programSoft->getUniformLocation(name_);
    }

    void bindProgram(ShaderProgram& program, int location) override {
        auto programSoft = dynamic_cast<ShaderProgramSoft*>(&program);
        programSoft->bindUniformSampler(sampler_, location);
    }

    void setTexture(const std::shared_ptr<Texture>& tex) override {
        sampler_->setTexture(tex);
    }

private:
    std::shared_ptr<SamplerSoft> sampler_;
};
}

#endif
