#ifndef VIEWERSOFTWARE_H
#define VIEWERSOFTWARE_H

#include "Viewer/Viewer.h"
#include "Render/OpenGL/OpenGLUtils.h"
#include "Render/Software/RendererSoft.h"
#include "Render/Software/TextureSoft.h"
#include "Viewer/Shader/Software/ShaderSoft.h"

namespace OpenGL {

#define CASE_CREATE_SHADER_SOFT(shading, source) case shading: \
  return programSoft->SetShaders(std::make_shared<source::VS>(), std::make_shared<source::FS>())

class ViewerSoftware : public Viewer {
public:
    ViewerSoftware(Config& config, Camera& camera) : Viewer(config, camera) {}

    void configRenderer() override {
        camera_->setReverseZ(config_.reverseZ);
        cameraDepth_->setReverseZ(config_.reverseZ);
    }

    /*获取软件渲染的颜色缓冲区,通过glTexSubImage2D上传到GPU纹理*/
    int swapBuffer() override {
        auto* texOut = dynamic_cast<TextureSoft<RGBA> *>(texColorMain_.get());
        auto buffer = texOut->getImage().getBuffer()->buffer;
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, outTexId_));
        GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                                 (int)buffer->getWidth(),
                                 (int)buffer->getHeight(),
                                 GL_RGBA,
                                 GL_UNSIGNED_BYTE,
                                 buffer->getRawDataPtr()));
        return outTexId_;
    }

    std::shared_ptr<Renderer> createRenderer() override {
        auto renderer = std::make_shared<RendererSoft>();
        if (!renderer->create()) {
            return nullptr;
        }
        return renderer;
    }

    bool loadShaders(ShaderProgram& program, ShadingModel shading) override {
        auto* programSoft = dynamic_cast<ShaderProgramSoft*>(&program);
        switch (shading) {
            CASE_CREATE_SHADER_SOFT(Shading_BaseColor, ShaderBasic);
            CASE_CREATE_SHADER_SOFT(Shading_BlinnPhong, ShaderBlinnPhong);
            CASE_CREATE_SHADER_SOFT(Shading_PBR, ShaderPbrIBL);
            CASE_CREATE_SHADER_SOFT(Shading_Skybox, ShaderSkybox);
            CASE_CREATE_SHADER_SOFT(Shading_FXAA, ShaderFXAA);
            CASE_CREATE_SHADER_SOFT(Shading_IBL_Irradiance, ShaderIBLIrradiance);
            CASE_CREATE_SHADER_SOFT(Shading_IBL_Prefilter, ShaderIBLPrefilter);
            default:
                break;
        }

        return false;
    }
};

}

#endif