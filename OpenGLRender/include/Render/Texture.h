#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <memory>
#include <vector>
#include "Base/GLMInc.h"
#include "Base/Buffer.h"

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
	TextureFormat_FLOAT32 = 1, //深度图
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
	std::string tag;//用于调试/识别；可以当作创建hash值的参数
};

class Texture : public TextureDesc {
public:
	virtual ~Texture() = default;

	inline uint32_t getLevelWidth(uint32_t level) {
		return std::max(1, width >> level);
	}

	inline uint32_t getLevelHeight(uint32_t level) {
		return std::max(1, height >> level);
	}

	virtual int getId() const = 0;

	virtual void setSamplerDesc(SamplerDesc& sampler) {};
	virtual void initImageData() {};
	virtual void setImageData(const std::vector<std::shared_ptr<Buffer<RGBA>>>& buffers) {};
	virtual void setImageData(const std::vector<std::shared_ptr<Buffer<float>>>& buffers) {};
	//将当前纹理的指定层级（Mipmap Level）和面（CubeMap Face）导出为图像文件
	virtual void dumpImage(const char* path, uint32_t layer, uint32_t level) = 0;

};

}

#endif