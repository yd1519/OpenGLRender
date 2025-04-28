#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include "Buffer.h"
#include <string>

namespace OpenGL {

class ImageUtils {
public:
	//将stb库加载出来的图像转化为自己定义的格式存储
	static std::shared_ptr<Buffer<RGBA>> readImageRGBA(const std::string& path);

	//将自己定义的格式存储的图像数据转化png格式
	static void writeImage(const char* filename, int w, int h, int comp, const void* data, int strideInBytes, bool flipY);

	//将浮点型深度图像数据转换为可视化的RGBA图像,用于深度图，将float转为int.
	static void convertFloatImage(RGBA* dst, float* src, uint32_t width, uint32_t height);
};
}
#endif
