#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <memory>
#include <vector>
#include "GLMInc.h"
#include "OpenGLUtils.h"
#include "Buffer.h"

namespace OpenGL {
//纹理环绕方式
enum WrapMode {
	Wrap_REPEAT,
	Wrap_MIRRORED_REPEAT,
	Wrap_CLAMP_TO_EDGE,
	Wrap_CLAMP_TO_BORDER,
};

//纹理过滤方式
enum FilterMode {
	Filter_NEAREST,
	Filter_LINEAR,
	Filter_NEAREST_MIPMAP_NEAREST,
	Filter_LINEAR_MIPMAP_NEAREST,
	Filter_NEAREST_MIPMAP_LINEAR,
	Filter_LINEAR_MIPMAP_LINEAR,
};

enum CubeMapFace {
	TEXTURE_CUBE_MAP_POSITIVE_X = 0,
	TEXTURE_CUBE_MAP_NEGATIVE_X = 1,
	TEXTURE_CUBE_MAP_POSITIVE_Y = 2,
	TEXTURE_CUBE_MAP_NEGATIVE_Y = 3,
	TEXTURE_CUBE_MAP_POSITIVE_Z = 4,
	TEXTURE_CUBE_MAP_NEGATIVE_Z = 5,
};

enum BorderColor {
	Border_BLACK = 0,
	Border_WHITE,
};


//采样器描述符
struct SamplerDesc {
	FilterMode filterMin = Filter_NEAREST;//纹理缩小，也就是一个纹理像素 < 一个屏幕像素
	FilterMode filterMag = Filter_LINEAR;

	WrapMode wrapS = Wrap_CLAMP_TO_EDGE;
	WrapMode wrapT = Wrap_CLAMP_TO_EDGE;
	WrapMode wrapR = Wrap_CLAMP_TO_EDGE; //3D纹理会有R方向。

	BorderColor borderColor = Border_BLACK; // CLAMP_TO_BORDER时的颜色
};


enum TextureType {
	TextureType_2D,
	TextureType_CUBE,
};


enum TextureFormat {
	TextureFormat_RGBA8 = 0,
	TextureFormat_FLOAT32 = 1,
};

enum TextureUsage {
	TextureUsage_Sampler = 1 << 0,            
	TextureUsage_UploadData = 1 << 1,          
	TextureUsage_AttachmentColor = 1 << 2, 
	TextureUsage_AttachmentDepth = 1 << 3, 
	TextureUsage_RendererOutput = 1 << 4,
};


struct TextureDesc {
	int width = 0;
	int height = 0;
	TextureType type = TextureType_2D;
	TextureFormat format = TextureFormat_RGBA8;
	uint32_t usage = TextureUsage_Sampler;
	bool useMipmaps = false;
	bool multiSample = false;
	std::string tag;//用于调试；可以区分同名纹理
};

struct TextureOpenGLDesc {
	GLint internalformat;//将纹理存储什么格式
	GLenum format; //原图的格式
	GLenum type;	//原图的类型
};

class Texture : public TextureDesc {
public:
	static TextureOpenGLDesc GetOpenGLDesc(TextureFormat format) {
		TextureOpenGLDesc ret{};

		switch (format) {
		case TextureFormat_RGBA8: {
			ret.internalformat = GL_RGBA;
			ret.format = GL_RGBA;
			ret.type = GL_UNSIGNED_BYTE;
			break;
		}
		case TextureFormat_FLOAT32: {
			ret.internalformat = GL_DEPTH_COMPONENT;
			ret.format = GL_DEPTH_COMPONENT;
			ret.type = GL_FLOAT;
			break;
		}
		}

		return ret;
	}

	virtual ~Texture() = default;

	inline uint32_t getLevelWidth(uint32_t level) {
		return std::max(1, width >> level);
	}

	inline uint32_t getLevelHeight(uint32_t level) {
		return std::max(1, height >> level);
	}

	int getId() { 
		return static_cast<int>(texId_);
	}

	void setSamplerDesc(SamplerDesc& sampler) {
		if (multiSample) {
			return;
		}

		GL_CHECK(glBindTexture(target_, texId_));
		GL_CHECK(glTexParameteri(target_, GL_TEXTURE_WRAP_S, sampler.wrapS));
		GL_CHECK(glTexParameteri(target_, GL_TEXTURE_WRAP_T, sampler.wrapT));
		GL_CHECK(glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, sampler.filterMin));
		GL_CHECK(glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, sampler.filterMag));

		glm::vec4 borderColor = sampler.borderColor == 0 ? glm::vec4(0.0f) : glm::vec4(1.0f);
		GL_CHECK(glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, &borderColor[0]));
	}
	virtual void initImageData() {};
	virtual void setImageData(const std::vector<std::shared_ptr<Buffer<RGBA>>>& buffers) {};
	
	/*
	//将当前纹理的指定层级（Mipmap Level）和面（CubeMap Face）导出为图像文件
	void dumpImage(const char* path, uint32_t layer, uint32_t level) {
		if (multiSample) {
			return;
		}

		GLuint fbo;
		GL_CHECK(glGenFramebuffers(1, &fbo));
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

		GLenum attachment = format == TextureFormat_FLOAT32 ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
		GLenum target = multiSample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		if (type == TextureType_CUBE) {
			target = OpenGL::cvtCubeFace(static_cast<CubeMapFace>(layer));
		}
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, target, texId_, level));

		auto levelWidth = (int32_t)getLevelWidth(level);
		auto levelHeight = (int32_t)getLevelHeight(level);

		auto* pixels = new uint8_t[levelWidth * levelHeight * 4];
		GL_CHECK(glReadPixels(0, 0, levelWidth, levelHeight, glDesc_.format, glDesc_.type, pixels));

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		GL_CHECK(glDeleteFramebuffers(1, &fbo));

		// convert float to rgba
		if (format == TextureFormat_FLOAT32) {
			ImageUtils::convertFloatImage(reinterpret_cast<RGBA*>(pixels), reinterpret_cast<float*>(pixels), levelWidth, levelHeight);
		}
		ImageUtils::writeImage(path, levelWidth, levelHeight, 4, pixels, levelWidth * 4, true);
		delete[] pixels;
	}*/

protected:
	GLuint texId_ = 0;
	TextureOpenGLDesc glDesc_{};
	GLenum target_ = 0;
};

class Texture2D : public Texture {
public:
	explicit Texture2D(const TextureDesc& desc) {
		assert(desc.type == TextureType_2D);
		width = desc.width;
		height = desc.height;
		type = TextureType_2D;
		format = desc.format;
		usage = desc.usage;
		useMipmaps = desc.useMipmaps;
		multiSample = desc.multiSample;
		target_ = multiSample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

		glDesc_ = GetOpenGLDesc(format);
		GL_CHECK(glGenTextures(1, &texId_));
	}

	~Texture2D() override {
		GL_CHECK(glDeleteTextures(1, &texId_));
	}

	//设置纹理图像数据
	void setImageData(const std::vector<std::shared_ptr<Buffer<RGBA>>>& buffers) override {
		if (multiSample) {
			LOGE("setImageData not support: multi sample texture");
			return;
		}

		if (format != TextureFormat_RGBA8) {
			LOGE("setImageData error: format not match");
			return;
		}

		if (width != buffers[0]->getWidth() || height != buffers[0]->getHeight()) {
			LOGE("setImageData error: size not match");
			return;
		}

		GL_CHECK(glBindTexture(target_, texId_));
		GL_CHECK(glTexImage2D(target_, 0, glDesc_.internalformat, width, height, 0, glDesc_.format, glDesc_.type,
			buffers[0]->getRawDataPtr()));

		if (useMipmaps) {
			GL_CHECK(glGenerateMipmap(target_));
		}
	}

	void initImageData() override {
		GL_CHECK(glBindTexture(target_, texId_));
		if (multiSample) {
			GL_CHECK(glTexImage2DMultisample(target_, 4, glDesc_.internalformat, width, height, GL_TRUE));
		}
		else {
			GL_CHECK(glTexImage2D(target_, 0, glDesc_.internalformat, width, height, 0, glDesc_.format, glDesc_.type, nullptr));
		}
	}

};

class TextureCube : public Texture {
public:
	explicit TextureCube(const TextureDesc& desc) {
		assert(desc.type == TextureType_CUBE);

		width = desc.width;
		height = desc.height;
		type = TextureType_CUBE;
		format = desc.format;
		usage = desc.usage;
		useMipmaps = desc.useMipmaps;
		multiSample = desc.multiSample;
		target_ = GL_TEXTURE_CUBE_MAP;

		glDesc_ = GetOpenGLDesc(format);
		GL_CHECK(glGenTextures(1, &texId_));
	}
	~TextureCube() {
		GL_CHECK(glDeleteTextures(1, &texId_));
	}

	void setImageData(const std::vector<std::shared_ptr<Buffer<RGBA>>>& buffers) override {
		if (multiSample) {
			return;
		}

		if (format != TextureFormat_RGBA8) {
			LOGE("setImageData error: format not match");
			return;
		}

		if (width != buffers[0]->getWidth() || height != buffers[0]->getHeight()) {
			LOGE("setImageData error: size not match");
			return;
		}

		GL_CHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, texId_));
		for (int i = 0; i < 6; i++) {
			GL_CHECK(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glDesc_.internalformat, width, height, 0,
				glDesc_.format, glDesc_.type, buffers[i]->getRawDataPtr()));
		}
		if (useMipmaps) {
			GL_CHECK(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));
		}
	}

	void initImageData() override {
		GL_CHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, texId_));
		for (int i = 0; i < 6; i++) {
			GL_CHECK(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glDesc_.internalformat, width, height, 0,
				glDesc_.format, glDesc_.type, nullptr));
		}

		if (useMipmaps) {
			GL_CHECK(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));
		}
	}

};


}

#endif