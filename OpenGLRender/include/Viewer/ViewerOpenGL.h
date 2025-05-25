#ifndef VIEWEROPENGL_H
#define VIEWEROPENGL_H

#include <glad/glad.h>
#include "Viewer/Viewer.h"
#include "Render/OpenGL/OpenGLUtils.h"
#include "Render/OpenGL/RendererOpenGL.h"
#include "Viewer/Config.h"


namespace OpenGL {


#define	CASE_CREATE_SHADER_GL(shading, source) case shading : \
	return programGL->compileAndLinkFile(SHADER_GLSL_DIR + #source + ".vert", \
										 SHADER_GLSL_DIR + #source + ".frag")

class ViewerOpenGL : public Viewer {
public:
	ViewerOpenGL(Config& config, Camera& camera) : Viewer(config, camera) {//创建两个帧缓冲对象。
		GL_CHECK(glGenFramebuffers(1, &fbo_in_));
		GL_CHECK(glGenFramebuffers(1, &fbo_out_));
	}

	// 关闭反向深度
	void configRenderer() override {
		config_.reverseZ = false;

		camera_->setReverseZ(config_.reverseZ);
		cameraDepth_->setReverseZ(config_.reverseZ);
	}

	int swapBuffer() override {
		int width = texColorMain_->width;
		int height = texColorMain_->height;

		if (texColorMain_->multiSample) {
			GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_in_));
			GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texColorMain_->getId(), 0));

			GL_CHECK(glBindTexture(GL_TEXTURE_2D, outTexId_)); //可以不绑定
			GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_out_));
			GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexId_, 0));

			GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_in_));
			GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_out_));

			//自动将多个采样点合并为单个像素值，实现平滑边缘
			GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
			return outTexId_;
		}
		return texColorMain_->getId();

	}

	void destroy() override {
		Viewer::destroy();

		GL_CHECK(glDeleteFramebuffers(1, &fbo_in_));
		GL_CHECK(glDeleteFramebuffers(1, &fbo_out_));
	}

	// 创建渲染器
	std::shared_ptr<Renderer> createRenderer() override {
		auto renderer = std::make_shared<RendererOpenGL>();
		if (renderer->create()) {
			return renderer;
		}
		else {
			return nullptr;
		}
	}

	// 加载对应着色器模型的着色器文件
	bool loadShaders(ShaderProgram& program, ShadingModel shading) override {
		auto* programGL = dynamic_cast<ShaderProgramOpenGL*>(&program);
		switch (shading) {
			CASE_CREATE_SHADER_GL(Shading_BaseColor, BasicGLSL);
			CASE_CREATE_SHADER_GL(Shading_BlinnPhong, BlinnPhongGLSL);
			CASE_CREATE_SHADER_GL(Shading_PBR, PbrGLSL);
			CASE_CREATE_SHADER_GL(Shading_Skybox, SkyboxGLSL);
			CASE_CREATE_SHADER_GL(Shading_IBL_Irradiance, IBLIrradianceGLSL);
			CASE_CREATE_SHADER_GL(Shading_IBL_Prefilter, IBLPrefilterGLSL);
			CASE_CREATE_SHADER_GL(Shading_FXAA, FxaaGLSL);
			default:
				break;
		}
	}

private:
	GLuint fbo_in_ = 0;
	GLuint fbo_out_ = 0;
};







}
#endif
