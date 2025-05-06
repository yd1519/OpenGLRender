#ifndef UNIFORMOPENGL_H
#define UNIFORMOPENGL_H

#include <glad/glad.h>
#include <string>
#include "Base/Logger.h"
#include "Render/Uniform.h"
#include "Render/OpenGL/OpenGLUtils.h"
#include "Render/OpenGL/ShaderProgramOpenGL.h"

//激活纹理单元
#define BIND_TEX_OPENGL(n) case n: GL_CHECK(glActiveTexture(GL_TEXTURE##n)); break;

namespace OpenGL {

class UniformBlockOpenGL : public UniformBlock {
public:
	UniformBlockOpenGL(const std::string& name, int size) : UniformBlock(name, size) {
		GL_CHECK(glGenBuffers(1, &ubo_));
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, ubo_));
		GL_CHECK(glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STATIC_DRAW));
	}

	~UniformBlockOpenGL() {
		GL_CHECK(glDeleteBuffers(1, &ubo_));
	}

	int getLocation(ShaderProgram& program) override {
		return glGetUniformBlockIndex(program.getId(), name_.c_str());
	}

	//将uniform块绑定到指定点
	void bindProgram(ShaderProgram& program, int location) {
		if (location < 0) {
			return;
		}
		auto* programGL = dynamic_cast<ShaderProgramOpenGL*>(&program);
		int binding = programGL->getUniformBlockBinding();

		GL_CHECK(glUniformBlockBinding(program.getId(), location, binding));//将着色器程序中的uniform绑定到指定点
		GL_CHECK(glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo_));//将uniform缓冲对象绑定到指定点。
	}

	//部分更新Uniform缓冲对象
	void setSubData(void* data, int len, int offset) {
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, ubo_));
		GL_CHECK(glBufferSubData(GL_UNIFORM_BUFFER, offset, len, data));
	}

	//全量更新Uniform缓冲对象
	void setData(void* data, int len) {
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, ubo_));
		GL_CHECK(glBufferData(GL_UNIFORM_BUFFER, len, data, GL_STATIC_DRAW));
	}

private:
	GLuint ubo_ = 0;
};

class UniformSamplerOpenGL : public UniformSampler {
public:
	explicit UniformSamplerOpenGL(const std::string& name, TextureType type, TextureFormat format)
		:UniformSampler(name, type, format) {}

	// location： 采样器在着色器程序中的位置
	void bindProgram(ShaderProgram& program, int location) override {
		if (location < 0) {
			return;
		}
		auto* programGL = dynamic_cast<ShaderProgramOpenGL*>(&program);
		int binding = programGL->getUniformSamplerBinding();

		//激活纹理单元
		switch (binding) {
			BIND_TEX_OPENGL(0)
			BIND_TEX_OPENGL(1)
			BIND_TEX_OPENGL(2)
			BIND_TEX_OPENGL(3)
			BIND_TEX_OPENGL(4)
			BIND_TEX_OPENGL(5)
			BIND_TEX_OPENGL(6)
			BIND_TEX_OPENGL(7)
			default: {
				LOGE("UniformSampler::bindProgram error: texture unit not support");
				break;
			}
		}
		GL_CHECK(glBindTexture(texTarget_, texId_));
		GL_CHECK(glUniform1i(location, binding));
	}

	void setTexture(const std::shared_ptr<Texture>& tex) {
		switch (tex->type) {
		case TextureType_2D:
			texTarget_ = GL_TEXTURE_2D;
			break;
		case TextureType_CUBE:
			texTarget_ = GL_TEXTURE_CUBE_MAP;
			break;
		default:
			LOGE("UniformSampler::setTexture error: texture type not support");
			break;
		}
		texId_ = tex->getId();
	}

private:
	GLuint texTarget_ = 0; // 纹理类型
	GLuint texId_ = 0; // 纹理id
};

}

#endif
