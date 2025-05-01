#ifndef UNIFORM_H
#define UNIFORM_H

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include "Base/UUID.h"
#include "Render/Texture.h"

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
	virtual int getLocation(ShaderProgram& program) = 0;

	//绑定数据到着色器程序中
	virtual void bindProgram(ShaderProgram& program, int location) = 0;

public:
	std::string name_;
private:
	UUID<Uniform> uuid_; //表示Uniform的编号
};

class UniformBlock : public Uniform {
public:
	UniformBlock(const std::string& name, int size) : Uniform(name), blockSize(size) {}

	//部分更新Uniform缓冲对象
	virtual void setSubData(void* data, int len, int offset) = 0;

	//全量更新Uniform缓冲对象
	virtual void setData(void* data, int len) = 0;

private:
	int blockSize = 0;
};

class UniformSampler : public Uniform {
public:

	UniformSampler(const std::string& name, TextureType type, TextureFormat format)
		:Uniform(name), type_(type), format_(format) {}

	virtual void setTexture(const std::shared_ptr<Texture>& tex) = 0;

private:
	TextureType type_;
	TextureFormat format_;
};


class ShaderResources {
public:
	//键：编号
	std::unordered_map<int, std::shared_ptr<UniformBlock>> blocks;
	std::unordered_map<int, std::shared_ptr<UniformSampler>> samplers;
};

}

#endif