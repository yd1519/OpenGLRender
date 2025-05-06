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

    // 静态采样方法 uv：纹理坐标
    static T sampleNearest(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border);
    static T sampleBilinear(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border);

    // 根据环绕模式获取纹理像素值
    static T pixelWithWrapMode(Buffer<T>* buffer, int x, int y, WrapMode wrap, T border);

    // 双线性采样工具方法
    static void sampleBufferBilinear(Buffer<T>* buffer_out, Buffer<T>* buffer_in, T border);
    static T samplePixelBilinear(Buffer<T>* buffer, glm::vec2 uv, WrapMode wrap, T border);// uv像素坐标

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
T BaseSampler<T>::textureImpl(TextureImageSoft<T>* tex, glm::vec2& uv, float lod, glm::ivec2 offset) {//lod：mipmap级别
    if (tex != nullptr && !tex->empty()) {
        // -------------------------------------基础非mipmap采样--------------------------------
        if (filterMode_ == Filter_NEAREST) {
            return sampleNearest(tex->levels[0]->buffer.get(), uv, wrapMode_, offset, borderColor_);
        }
        if (filterMode_ == Filter_LINEAR) {
            return sampleBilinear(tex->levels[0]->buffer.get(), uv, wrapMode_, offset, borderColor_);
        }

        // ------------------------------------------使用mipmap的时候----------------------------------------------
        int max_level = (int)tex->levels.size() - 1;
        // -------------------最近mipmap层级选择（不混合）--------------------
        if (filterMode_ == Filter_NEAREST_MIPMAP_NEAREST || filterMode_ == Filter_LINEAR_MIPMAP_NEAREST) {
            int level = glm::clamp((int)glm::ceil(lod + 0.5f) - 1, 0, max_level);// 计算目标mipmap级别（四舍五入）
            if (filterMode_ == Filter_NEAREST_MIPMAP_NEAREST) {
                return sampleNearest(tex->levels[level]->buffer.get(), uv, wrapMode_, offset, borderColor_);
            }
            else {
                return sampleBilinear(tex->levels[level]->buffer.get(), uv, wrapMode_, offset, borderColor_);
            }
        }
        // -----------------三线性插值（混合）-----------------------
        if (filterMode_ == Filter_NEAREST_MIPMAP_LINEAR || filterMode_ == Filter_LINEAR_MIPMAP_LINEAR) {
            // 确定两个相邻mipmap级别
            int level_hi = glm::clamp((int)std::floor(lod), 0, max_level);
            int level_lo = glm::clamp(level_hi + 1, 0, max_level);

            T texel_hi, texel_lo;
            if (filterMode_ == Filter_NEAREST_MIPMAP_LINEAR) {
                texel_hi = sampleNearest(tex->levels[level_hi]->buffer.get(), uv, wrapMode_, offset, borderColor_);
            }
            else {
                texel_hi = sampleBilinear(tex->levels[level_hi]->buffer.get(), uv, wrapMode_, offset, borderColor_);
            }
            // 如果两个层级相同（如lod=0且max_level=0）
            if (level_hi == level_lo) {
                return texel_hi;
            }
            else {
                if (filterMode_ == Filter_NEAREST_MIPMAP_LINEAR) {
                    texel_lo = sampleNearest(tex->levels[level_lo]->buffer.get(), uv, wrapMode_, offset, borderColor_);
                }
                else {
                    texel_lo = sampleBilinear(tex->levels[level_lo]->buffer.get(), uv, wrapMode_, offset, borderColor_);
                }
            }

            float f = glm::fract(lod);// 插值系数
            return glm::mix(texel_hi, texel_lo, f);
        }
    }
    return T(0);
}

template<typename T>/*生成纹理的mipmap链*/
void BaseSampler<T>::generateMipmaps(TextureImageSoft<T>* tex, bool sample) {
    //是否使用采样方式生成（true = 双线性下采样，false = 仅分配内存）
    int width = tex->getWidth();
    int height = tex->getHeight();

    // 确保至少存在基础级别（level 0）
    auto level0 = tex->getBuffer();
    tex->levels.resize(1);
    tex->levels[0] = level0;

    // 计算mipmap级别总数（基于最大尺寸的对数）
    uint32_t levelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    for (uint32_t level = 1; level < levelCount; level++) {
        tex->levels.push_back(std::make_shared<ImageBufferSoft<T>>(std::max(1, width >> level), std::max(1, height >> level)));
    }

    if (!sample) {
        return;
    }

    for (int i = 1; i < tex->levels.size(); i++) {
        BaseSampler<T>::sampleBufferBilinear(tex->levels[i]->buffer.get(), tex->levels[i - 1]->buffer.get(), T(0));//T(0)黑色
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

template<typename T>/*最近邻采样实现*/
T BaseSampler<T>::sampleNearest(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border) {
    glm::vec2 texUV = uv * glm::vec2(buffer->getWidth(), buffer->getHeight());
    
    // 取整并应用偏移量（向下取整保证一致性）
    auto x = (int)glm::floor(texUV.x) + offset.x; // offset 像素偏移量（用于各向异性采样等场景）
    auto y = (int)glm::floor(texUV.y) + offset.y;

    return pixelWithWrapMode(buffer, x, y, wrap, border);
}

template<typename T>
T BaseSampler<T>::sampleBilinear(Buffer<T>* buffer, glm::vec2& uv, WrapMode wrap, glm::ivec2& offset, T border) {
    glm::vec2 texUV = uv * glm::vec2(buffer->getWidth(), buffer->getHeight());
    texUV.x += (float)offset.x;
    texUV.y += (float)offset.y;
    return samplePixelBilinear(buffer, texUV, wrap, border);
}

template<typename T>/*使用双线性插值对输入缓冲区进行下采样，生成输出缓冲区*/
void BaseSampler<T>::sampleBufferBilinear(Buffer<T>* buffer_out, Buffer<T>* buffer_in, T border) {
    // 计算输入纹理到输出纹理的尺寸比例
    float ratio_x = (float)buffer_in->getWidth() / (float)buffer_out->getWidth();
    float ratio_y = (float)buffer_in->getHeight() / (float)buffer_out->getHeight();

    /*计算采样偏移量（0.5 * ratio保证像素中心对齐），这个delta补偿了从输出像素中心映射到输入纹理时的坐标偏移*/
    glm::vec2 delta = 0.5f * glm::vec2(ratio_x, ratio_y);//如ratio = 2，那么对于输出（0，0）的采样点就在输入坐标（1， 1）正好在四个像素的中心

    // 遍历输出缓冲区的每个像素
    for (int y = 0; y < (int)buffer_out->getHeight(); y++) {
        for (int x = 0; x < (int)buffer_out->getWidth(); x++) {
            glm::vec2 uv = glm::vec2((float)x * ratio_x, (float)y * ratio_y) + delta;
            auto color = samplePixelBilinear(buffer_in, uv, Wrap_CLAMP_TO_EDGE, border);
            buffer_out->set(x, y, color);
        }
    }
}

template<typename T>/*双线性插值采样实现*/
T BaseSampler<T>::samplePixelBilinear(Buffer<T>* buffer, glm::vec2 uv, WrapMode wrap, T border) {
    auto x = (int)glm::floor(uv.x - 0.5f);
    auto y = (int)glm::floor(uv.y - 0.5f);

    auto s1 = pixelWithWrapMode(buffer, x, y, wrap, border);        // 左下
    auto s2 = pixelWithWrapMode(buffer, x + 1, y, wrap, border);    // 右下
    auto s3 = pixelWithWrapMode(buffer, x, y + 1, wrap, border);    // 左上
    auto s4 = pixelWithWrapMode(buffer, x + 1, y + 1, wrap, border);// 右上

    // 计算插值系数（获取坐标的小数部分）  示例：uv(0.503,0.498) → f(0.003, -0.002) → fract后(0.003,0.998)
    glm::vec2 f = glm::fract(uv - glm::vec2(0.5f));

    return glm::mix(glm::mix(s1, s2, f.x), glm::mix(s3, s4, f.x), f.y);
}

// -------------------------------------------------------------------------------------------------

template<typename T>
class BaseSampler2D : public BaseSampler<T> {
public:
    // 绑定纹理数据
    inline void setImage(TextureImageSoft<T>* tex) {
        tex_ = tex;
        BaseSampler<T>::width_ = (tex_ == nullptr) ? 0 : tex_->getWidth();
        BaseSampler<T>::height_ = (tex_ == nullptr) ? 0 : tex_->getHeight();
        BaseSampler<T>::useMipmaps = BaseSampler<T>::filterMode_ > Filter_LINEAR;
    }

    inline bool empty() override {
        return tex_ == nullptr;
    }

    T texture2DImpl(glm::vec2& uv, float bias = 0.f) {
        float lod = bias;// 基础偏置值
        if (BaseSampler<T>::useMipmaps && BaseSampler<T>::lodFunc_) {
            lod += (*BaseSampler<T>::lodFunc_)(this);
        }
        return texture2DLodImpl(uv, lod);
    }

    T texture2DLodImpl(glm::vec2& uv, float lod = 0.f, glm::ivec2 offset = glm::ivec2(0)) {
        return BaseSampler<T>::textureImpl(tex_, uv, lod, offset); // offset可以用于精细控制，比如MSAA场景
    }

private:
    TextureImageSoft<T>* tex_ = nullptr;
};

// -------------------------------------------------------------------------------------------------
template<typename T>
class BaseSamplerCube : public BaseSampler<T> {
public:
    BaseSamplerCube() {
        BaseSampler<T>::wrapMode_ = Wrap_CLAMP_TO_EDGE;
        BaseSampler<T>::filterMode_ = Filter_LINEAR;
    }

    // 给立方体贴图的一个面赋值
    inline void setImage(TextureImageSoft<T>* tex, int idx) {
        texes_[idx] = tex;
        if (idx == 0) {
            BaseSampler<T>::width_ = (tex == nullptr) ? 0 : tex->getWidth();
            BaseSampler<T>::height_ = (tex == nullptr) ? 0 : tex->getHeight();
            BaseSampler<T>::useMipmaps = BaseSampler<T>::filterMode_ > Filter_LINEAR;
        }
    }

    //检查纹理是否为空
    inline bool empty() override {
        return texes_[0] == nullptr;
    }

    T textureCubeImpl(glm::vec3& coord, float bias = 0.f) {
        float lod = bias;
        // cube sampler derivative not support
        // lod += dFd()...
        return textureCubeLodImpl(coord, lod);
    }

    T textureCubeLodImpl(glm::vec3& coord, float lod = 0.f) {
        int index; // 表示哪一个面
        glm::vec2 uv;
        convertXYZ2UV(coord.x, coord.y, coord.z, &index, &uv.x, &uv.y);
        TextureImageSoft<T>* tex = texes_[index];
        return BaseSampler<T>::textureImpl(tex, uv, lod);
    }

    // 三维坐标到立方体面UV的转换
    static void convertXYZ2UV(float x, float y, float z, int* index, float* u, float* v);

private:
    // +x, -x, +y, -y, +z, -z
    TextureImageSoft<T>* texes_[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
};

template<typename T>
void BaseSamplerCube<T>::convertXYZ2UV(float x, float y, float z, int* index, float* u, float* v) {
    // Ref: https://en.wikipedia.org/wiki/Cube_mapping
    float absX = std::fabs(x);
    float absY = std::fabs(y);
    float absZ = std::fabs(z);

    bool isXPositive = x > 0;
    bool isYPositive = y > 0;
    bool isZPositive = z > 0;

    float maxAxis, uc, vc;

    // POSITIVE X
    if (isXPositive && absX >= absY && absX >= absZ) {
        maxAxis = absX;
        uc = -z;
        vc = y;
        *index = 0;
    }
    // NEGATIVE X
    if (!isXPositive && absX >= absY && absX >= absZ) {
        maxAxis = absX;
        uc = z;
        vc = y;
        *index = 1;
    }
    // POSITIVE Y
    if (isYPositive && absY >= absX && absY >= absZ) {
        maxAxis = absY;
        uc = x;
        vc = -z;
        *index = 2;
    }
    // NEGATIVE Y
    if (!isYPositive && absY >= absX && absY >= absZ) {
        maxAxis = absY;
        uc = x;
        vc = z;
        *index = 3;
    }
    // POSITIVE Z
    if (isZPositive && absZ >= absX && absZ >= absY) {
        maxAxis = absZ;
        uc = x;
        vc = y;
        *index = 4;
    }
    // NEGATIVE Z
    if (!isZPositive && absZ >= absX && absZ >= absY) {
        maxAxis = absZ;
        uc = -x;
        vc = y;
        *index = 5;
    }

    // flip y
    vc = -vc;

    // Convert range from -1 to 1 to 0 to 1
    *u = 0.5f * (uc / maxAxis + 1.0f);
    *v = 0.5f * (vc / maxAxis + 1.0f);
}

// -------------------------------------------------------------------------------------------------
class SamplerSoft {
public:
    virtual TextureType texType() = 0;
    virtual void setTexture(const std::shared_ptr<Texture>& tex) = 0;
};

// -------------------------------------------------------------------------------------------------

template<typename T>
class Sampler2DSoft : public SamplerSoft {
public:
    TextureType texType() override {
        return TextureType_2D;
    }

    // 绑定纹理对象
    void setTexture(const std::shared_ptr<Texture>& tex) override {
        tex_ = dynamic_cast<TextureSoft<T> *>(tex.get());
        tex_->getBorderColor(sampler_.borderColor());
        sampler_.setFilterMode(tex_->getSamplerDesc().filterMin);
        sampler_.setWrapMode(tex_->getSamplerDesc().wrapS);
        sampler_.setImage(&tex_->getImage());
    }

    inline TextureSoft<T>* getTexture() const {
        return tex_;
    }

    inline void setLodFunc(std::function<float(BaseSampler<T>*)>* func) {
        sampler_.setLodFunc(func);
    }

    inline T texture2D(glm::vec2 coord, float bias = 0.f) {
        return sampler_.texture2DImpl(coord, bias);
    }

    inline T texture2DLod(glm::vec2 coord, float lod = 0.f) {
        return sampler_.texture2DLodImpl(coord, lod);
    }

    inline T texture2DLodOffset(glm::vec2 coord, float lod, glm::ivec2 offset) {
        return sampler_.texture2DLodImpl(coord, lod, offset);
    }

private:
    BaseSampler2D<T> sampler_;
    TextureSoft<T>* tex_ = nullptr;
};

template<typename T>
class SamplerCubeSoft : public SamplerSoft {
public:
    TextureType texType() override {
        return TextureType_CUBE;
    }

    void setTexture(const std::shared_ptr<Texture>& tex) override {
        tex_ = dynamic_cast<TextureSoft<T> *>(tex.get());
        tex_->getBorderColor(sampler_.borderColor());
        sampler_.setFilterMode(tex_->getSamplerDesc().filterMin);
        sampler_.setWrapMode(tex_->getSamplerDesc().wrapS);
        for (int i = 0; i < 6; i++) {
            sampler_.setImage(&tex_->getImage((CubeMapFace)i), i);
        }
    }

    inline TextureSoft<T>* getTexture() const {
        return tex_;
    }

    inline T textureCube(glm::vec3 coord, float bias = 0.f) {
        return sampler_.textureCubeImpl(coord, bias);
    }

    inline T textureCubeLod(glm::vec3 coord, float lod = 0.f) {
        return sampler_.textureCubeLodImpl(coord, lod);
    }

private:
    BaseSamplerCube<T> sampler_;
    TextureSoft<T>* tex_ = nullptr;
};



}

#endif