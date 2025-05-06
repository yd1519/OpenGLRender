#ifndef SAMPLERSOFT_H
#define SAMPLERSOFT_H

#include <functional>
#include "Render/Software/TextureSoft.h"

namespace OpenGL {
// 适用于周期性边界
#define CoordMod(i, n) ((i) & ((n) - 1) + (n)) & ((n) - 1)  // (i % n + n) % n
#define CoordMirror(i) (i) >= 0 ? (i) : (-1 - (i))

template<typename T>
class BaseSampler {
public:
    virtual bool empty() = 0;
    inline size_t width() const { return width_; }
    inline size_t height() const { return height_; }
    inline T& borderColor() { return borderColor_; };

    // 核心纹理采样实现
    T textureImpl(TextureImageSoft<T>* tex, glm::vec2& uv, float lod = 0.f, glm::ivec2 offset = glm::ivec2(0));

    // 静态采样方法
    static T sampleNearest(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border);
    static T sampleBilinear(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border);

    // 根据环绕模式获取纹理像素值
    static T pixelWithWrapMode(Buffer<T>* buffer, int x, int y, WrapMode wrap, T border);

    // 双线性采样工具方法
    static void sampleBufferBilinear(Buffer<T>* buffer_out, Buffer<T>* buffer_in, T border);
    static T samplePixelBilinear(Buffer<T>* buffer, glm::vec2 uv, WrapMode wrap, T border);

    inline void setWrapMode(int wrap_mode) {
        wrapMode_ = (WrapMode)wrap_mode;
    }

    inline void setFilterMode(int filter_mode) {
        filterMode_ = (FilterMode)filter_mode;
    }

    inline void setLodFunc(std::function<float(BaseSampler<T>*)>* func) {
        lodFunc_ = func;
    }

    static void generateMipmaps(TextureImageSoft<T>* tex, bool sample);

protected:
    T borderColor_;

    size_t width_ = 0;
    size_t height_ = 0;

    bool useMipmaps = false;
    WrapMode wrapMode_ = Wrap_CLAMP_TO_EDGE;
    FilterMode filterMode_ = Filter_LINEAR;

    // LOD计算函数指针
    std::function<float(BaseSampler<T>*)>* lodFunc_ = nullptr;
};

template<typename T>
void BaseSampler<T>::generateMipmaps(TextureImageSoft<T>* tex, bool sample) {
    int width = tex->getWidth();
    int height = tex->getHeight();

    auto level0 = tex->getBuffer();
    tex->levels.resize(1);
    tex->levels[0] = level0;

    uint32_t levelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    for (uint32_t level = 1; level < levelCount; level++) {
        tex->levels.push_back(std::make_shared<ImageBufferSoft<T>>(std::max(1, width >> level), std::max(1, height >> level)));
    }

    if (!sample) {
        return;
    }

    for (int i = 1; i < tex->levels.size(); i++) {
        BaseSampler<T>::sampleBufferBilinear(tex->levels[i]->buffer.get(), tex->levels[i - 1]->buffer.get(), T(0));
    }
}

template<typename T>
void TextureImageSoft<T>::generateMipmap(bool sample) {
    BaseSampler<T>::generateMipmaps(this, sample);
}

template<typename T>
T BaseSampler<T>::pixelWithWrapMode(Buffer<T>* buffer, int x, int y, WrapMode wrap, T border) {
    int w = (int)buffer->getWidth();
    int h = (int)buffer->getHeight();
    switch (wrap) {// 根据环绕模式进行坐标转换
        case Wrap_REPEAT: {
            x = CoordMod(x, w);
            y = CoordMod(y, h);
            break;
        }
        case Wrap_MIRRORED_REPEAT: {
            x = CoordMod(x, 2 * w);
            y = CoordMod(y, 2 * h);

            x -= w;
            y -= h;

            x = CoordMirror(x);
            y = CoordMirror(y);

            x = w - 1 - x;
            y = h - 1 - y;
            break;
        }
        case Wrap_CLAMP_TO_EDGE: {
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x >= w) x = w - 1;
            if (y >= h) y = h - 1;
            break;
        }
        case Wrap_CLAMP_TO_BORDER: {
            if (x < 0 || x >= w) return border;
            if (y < 0 || y >= h) return border;
            break;
        }
    }

    T* ptr = buffer->get(x, y);
    if (ptr) {
        return *ptr;
    }
    return T(0);
}



















}

#endif