#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>

#include "Base/ImageUtils.h"
#include "Base/Logger.h"

namespace OpenGL {


std::shared_ptr<Buffer<RGBA>> ImageUtils::readImageRGBA(const std::string& path) {
	//使用stb库加载图像
	int iw = 0, ih = 0, n = 0;
	unsigned char* data = stbi_load(path.c_str(), &iw, &ih, &n, STBI_default);
	if (data == nullptr) {
		LOGD("ImageUtils::readImage failed, path: %s", path.c_str());
		return nullptr;
	}

	//创建自建的缓冲区
	auto buffer = Buffer<RGBA>::makeDefault(iw, ih);

	//转化为RGBA格式
	for (size_t y = 0; y < ih; y++) {
		for (size_t x = 0; x < iw; x++) {
			auto& to = *buffer->get(x, y);  // 获取目标像素位置
			size_t index = x + y * iw;        // 计算源数据索引

			//根据原图通道数进行不同的处理
			switch (n) {
				case STBI_grey: {//1通道
					to.r = data[index];
					to.g = to.b = to.r;
					to.a = 255;
					break;
				}
				case STBI_grey_alpha: {//2 通道
					to.r = data[index * 2 + 0];
					to.g = to.b = to.r;
					to.a = data[index * 2 + 1];
					break;
				}
				case STBI_rgb: {//3
					to.r = data[index * 3 + 0];
					to.g = data[index * 3 + 1];
					to.b = data[index * 3 + 2];
					to.a = 255;
					break;
				}
				case STBI_rgb_alpha: {//4
					to.r = data[index * 4 + 0];
					to.g = data[index * 4 + 1];
					to.b = data[index * 4 + 2];
					to.a = data[index * 4 + 3];
					break;
				}
				default: break;
			}

		}
	}

	//释放data
	stbi_image_free(data);//不能用free，因为stb库不一定用的malloc
	return buffer;

}

void ImageUtils::writeImage(const char* filename, int w, int h, int comp,
	const void* data, int strideInBytes, bool flipY) {
	stbi_flip_vertically_on_write(flipY);//是否垂直翻转

	// strideInBytes表示内存中 ​​每行像素数据占用的总字节数​​（包括实际像素数据和可能的填充字节）
	stbi_write_png(filename, w, h, comp, data, strideInBytes);
}

void ImageUtils::convertFloatImage(RGBA* dst, float* src, uint32_t width, uint32_t height) {
	float* srcPixel = src;	// 用于遍历原图像像素

	// 第一阶段：查找深度值的范围
	float depthMin = FLT_MAX;
	float depthMax = FLT_MIN;
	for (size_t i = 0; i < width * height; i++) {
		float depth = *srcPixel;
		depthMin = std::min(depthMin, depth);
		depthMax = std::max(depthMax, depth);
		srcPixel++;
	}

	// 重置指针位置，准备转换
	srcPixel = src;
	RGBA* dstPixel = dst; // 用于遍历目标图像像素

	// 第二阶段：执行实际转换
	for (int i = 0; i < width * height; i++) {
		float depth = *srcPixel;

		depth = (depth - depthMin) / (depthMax - depthMin);

		dstPixel->r = glm::clamp((int)(depth * 255.f), 0, 255);
		dstPixel->g = dstPixel->b = dstPixel->r;
		dstPixel->a = 255;

		dstPixel++;
		srcPixel++;
	}

}

}