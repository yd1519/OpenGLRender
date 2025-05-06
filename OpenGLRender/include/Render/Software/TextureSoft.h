#ifndef TEXTURESOFT_H
#define TEXTURESOFT_H

#include <fstream>
#include "Base/UUID.h"
#include "Base/Buffer.h"
#include "Base/ImageUtils.h"
#include "Render/Texture.h"

namespace OpenGL {
#define SOFT_MS_CNT 4

// 软件实现纹理的存储
template<typename T>
class ImageBufferSoft {
public:
    ImageBufferSoft() = default;

    ImageBufferSoft(int w, int h, int samples = 1) {
        width = w;
        height = h;
        multiSample = samples > 1; // 标识是否是多重采样
        sampleCnt = samples;

        if (samples == 1) {
            buffer = Buffer<T>::makeDefault(w, h);
        }
        else if (samples == 4) {
            bufferMs4x = Buffer<glm::tvec4<T>>::makeDefault(w, h);
        }
        else {
            LOGE("create color buffer failed: samplers not support");
        }
    }

    // 强制设置为单采样
    explicit ImageBufferSoft(const std::shared_ptr<Buffer<T>>& buf) {
        width = (int)buf->getWidth();
        height = (int)buf->getHeight();
        multiSample = false;
        sampleCnt = 1;
        buffer = buf;
    }

public:
    std::shared_ptr<Buffer<T>> buffer;
    std::shared_ptr<Buffer<glm::tvec4<T>>> bufferMs4x;

    int width = 0;
    int height = 0;
    bool multiSample = false;
    int sampleCnt = 1; // 每个像素的采样点数
};

// 支持存储多张图像的纹理
template<typename T>
class TextureImageSoft {
public:
    inline int getWidth() {
        return empty() ? 0 : levels[0]->width;
    }

    inline int getHeight() {
        return empty() ? 0 : levels[0]->height;
    }

    inline bool empty() const {
        return levels.empty();
    }

    inline std::shared_ptr<ImageBufferSoft<T>>& getBuffer(uint32_t level = 0) {
        return levels[level];
    }

    void generateMipmap(bool sample = true);

public:
    std::vector<std::shared_ptr<ImageBufferSoft<T>>> levels;// mipmap级别
};

template<typename T>
class TextureSoft : public Texture {
public:
    explicit TextureSoft(const TextureDesc& desc) {
        //基础属性设置
        width = desc.width;
        height = desc.height;
        type = desc.type;
        format = desc.format;
        usage = desc.usage;
        useMipmaps = desc.useMipmaps;
        multiSample = desc.multiSample;

        switch (type) {
        case TextureType_2D:
            layerCount_ = 1;
            break;
        case TextureType_CUBE:
            layerCount_ = 6;
            break;
        default:
            break;
        }
        images_.resize(layerCount_);
    }

    int getId() const override {
        return uuid_.get();
    }

    void setSamplerDesc(SamplerDesc& sampler) override {
        samplerDesc_ = sampler;
    }

    // 存储纹理图像数据
    void setImageData(const std::vector<std::shared_ptr<Buffer<T>>>& buffers) override {
        if (multiSample) {
            LOGE("setImageData not support: multi sample texture");
            return;
        }

        if (width != buffers[0]->getWidth() || height != buffers[0]->getHeight()) {
            LOGE("setImageData error: size not match");
            return;
        }
        //  为每层设置图像数据
        for (int i = 0; i < layerCount_; i++) {
            images_[i].levels.resize(1);
            images_[i].levels[0] = std::make_shared<ImageBufferSoft<T>>(buffers[i]);

            if (useMipmaps) {// 生成mipmap
                images_[i].generateMipmap();
            }
        }
    }

    // 分配存储空间
    void initImageData() override {
        for (auto& image : images_) {
            image.levels.resize(1);
            image.levels[0] = std::make_shared<ImageBufferSoft<T>>(width, height, multiSample ? SOFT_MS_CNT : 1);
            if (useMipmaps) {// 如果需要，生成mipmap（不使用采样）
                image.generateMipmap(false);
            }
        }
    }

    // 将第layer张纹理的level级别的mipmap存储到path中
    void dumpImage(const char* path, uint32_t layer, uint32_t level) override {
        dumpImageSoft(path, images_[layer], level);
    }

    inline SamplerDesc& getSamplerDesc() {
        return samplerDesc_;
    }

    inline TextureImageSoft<T>& getImage(uint32_t layer = 0) {
        return images_[layer];
    }

    inline void getBorderColor(float& ret) {
        ret = glm::clamp(cvtBorderColor(samplerDesc_.borderColor).r, 0.f, 1.f);
    }

    inline void getBorderColor(RGBA& ret) {
        ret = glm::clamp(cvtBorderColor(samplerDesc_.borderColor) * 255.f, { 0, 0, 0, 0 }, { 255, 255, 255, 255 });
    }

    // 从文件加载纹理数据
    bool loadFromFile(const char* path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            LOGE("failed to open file: %s", path);
            return false;
        }

        for (int i = 0; i < layerCount_; i++) {
            auto& layer = images_[i];
            for (int level = 0; level < layer.levels.size(); level++) {
                auto& img = layer.getBuffer(level);
                if (multiSample) {
                    file.read((char*)img->bufferMs4x->getRawDataPtr(), img->bufferMs4x->getRawDataBytesSize());
                }
                else {
                    file.read((char*)img->buffer->getRawDataPtr(), img->buffer->getRawDataBytesSize());
                }
            }
        }

        if (!file) {
            LOGE("failed to read file: %s", path);
            file.close();
            return false;
        }

        file.close();
        return true;
    }

    // 将纹理数据保存到文件
    void storeToFile(const char* path)   {
        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            LOGE("failed to open file: %s", path);
            return;
        }

        // 逐层逐级别写入数据
        for (int i = 0; i < layerCount_; i++) {
            auto& layer = images_[i];
            for (int level = 0; level < layer.levels.size(); level++) {
                auto& img = layer.getBuffer(level);
                if (multiSample) {
                    file.write((char*)img->bufferMs4x->getRawDataPtr(), img->bufferMs4x->getRawDataBytesSize());
                }
                else {
                    file.write((char*)img->buffer->getRawDataPtr(), img->buffer->getRawDataBytesSize());
                }
            }
        }

        file.close();
    }

protected:
    static inline glm::vec4 cvtBorderColor(BorderColor color) {
        switch (color) {
        case Border_BLACK:
            return glm::vec4(0.f);
        case Border_WHITE:
            return glm::vec4(1.f);
        default:
            break;
        }
        return glm::vec4(0.f);
    }

    // 将纹理图像转储到文件
    void dumpImageSoft(const char* path, TextureImageSoft<T> image, uint32_t level) {
        if (multiSample) {
            return;
        }

        void* pixels = image.getBuffer(level)->buffer->getRawDataPtr();
        auto levelWidth = (int32_t)getLevelWidth(level);
        auto levelHeight = (int32_t)getLevelHeight(level);

        // 浮点纹理需要转换为RGBA格式
        if (format == TextureFormat_FLOAT32) {
            auto* rgba_pixels = new uint8_t[levelWidth * levelHeight * 4];
            ImageUtils::convertFloatImage(reinterpret_cast<RGBA*>(rgba_pixels), reinterpret_cast<float*>(pixels), levelWidth, levelHeight);
            ImageUtils::writeImage(path, levelWidth, levelHeight, 4, rgba_pixels, levelWidth * 4, true);
            delete[] rgba_pixels;
        }
        else {
            ImageUtils::writeImage(path, levelWidth, levelHeight, 4, pixels, levelWidth * 4, true);
        }
    }

protected:
    UUID<TextureSoft<T>> uuid_;
    SamplerDesc samplerDesc_;
    std::vector<TextureImageSoft<T>> images_;
    uint32_t layerCount_ = 1; // 纹理层数，表示是否是cube纹理

};
}

#endif
