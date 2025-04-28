#ifndef UNIFORM_H
#define UNIFORM_H

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include "UUID.h"
#include "Texture.h"

#define BIND_TEX_OPENGL(n) case n: GL_CHECK(glActiveTexture(GL_TEXTURE##n)); break;

namespace OpenGL {

class ShaderProgram;

class Uniform {
public:
	explicit Uniform(const std::string& name) : name_(name) {}

	//返回是第几个uniform，包括uniformblock和uniformsampler
	inline int getHash() const {
		return uuid_.get();
	}
	
	//获取uniform块在着色器程序的位置
	inline int getLocation(ShaderProgram& program) {
		return glGetUniformBlockIndex(program.getId(), name_.c_str());
	}

	virtual void bindProgram(ShaderProgram& program, int location) = 0;

public:
	std::string name_;

private:
	UUID<Uniform> uuid_;//表示Uniform的编号
};

class UniformBlock : public Uniform {
public:
	UniformBlock(const std::string& name, int size) : Uniform(name), blockSize(size) {
		GL_CHECK(glGenBuffers(1, &ubo_));
		GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, ubo_));
		GL_CHECK(glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STATIC_DRAW));
	}

	~UniformBlock() {
		GL_CHECK(glDeleteBuffers(1, &ubo_));
	}

	//将uniform块绑定到指定点
	void bindProgram(ShaderProgram& program, int location) override {
		if (location < 0) {
			return;
		}
		auto* programGL = dynamic_cast<ShaderProgram*>(&program);
		int binding = program.getUniformBlockBinding();

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
	int blockSize = 0;
	GLuint ubo_ = 0;
};

class UniformSampler : public Uniform {
public:

	UniformSampler(const std::string &name, TextureType type, TextureFormat format)
		:Uniform(name), type_(type), format_(format) {}

	void bindProgram(ShaderProgram& program, int location) override {
		if (location < 0) {
			return;
		}

		int binding = program.getUniformSamplerBinding();

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
	TextureType type_;
	TextureFormat format_;

	GLuint texTarget_ = 0;
	GLuint texId_ = 0;
};


class ShaderResources {
public:
	//键：编号
	std::unordered_map<int, std::shared_ptr<UniformBlock>> blocks;
	std::unordered_map<int, std::shared_ptr<UniformSampler>> samplers;
};

}

#endif