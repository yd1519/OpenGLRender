#ifndef BUFFER_H
#define BUFFER_H

#include "Base/GLMInc.h"
#include "Base/MemoryUtils.h"

namespace OpenGL {

enum BufferLayout {
    Layout_Linear,
    Layout_Tiled,
    Layout_Morton,
};

template<typename T>//基类，实现了线性布局的缓冲区。
class Buffer {
 public:
  static std::shared_ptr<Buffer<T>> makeDefault(size_t w, size_t h);
  static std::shared_ptr<Buffer<T>> makeLayout(size_t w, size_t h, BufferLayout layout);

  virtual void initLayout() {
    innerWidth_ = width_;
    innerHeight_ = height_;
  }

  virtual inline size_t convertIndex(size_t x, size_t y) const {
    return x + y * innerWidth_;
  }

  /*virtual BufferLayout getLayout() const {
    return Layout_Linear;
  }*/
  

  //分配内存并初始化布局。
  void create(size_t w, size_t h, const uint8_t *data = nullptr) {
    if (w > 0 && h > 0) {
      if (width_ == w && height_ == h) {
        return;
      }
      width_ = w;
      height_ = h;

      initLayout();
      dataSize_ = innerWidth_ * innerHeight_;
      data_ = MemoryUtils::makeBuffer<T>(dataSize_, data);
    }
  }

  virtual void destroy() {
    width_ = 0;
    height_ = 0;
    innerWidth_ = 0;
    innerHeight_ = 0;
    dataSize_ = 0;
    data_ = nullptr;
  }

  inline T *getRawDataPtr() const {
    return data_.get();
  }

  inline size_t getRawDataSize() const {
    return dataSize_;
  }

  inline size_t getRawDataBytesSize() const {
    return dataSize_ * sizeof(T);
  }

  inline bool empty() const {
    return data_ == nullptr;
  }

  inline size_t getWidth() const {
    return width_;
  }

  inline size_t getHeight() const {
    return height_;
  }

  //返回坐标(x, y) 处数据的指针。
  inline T *get(size_t x, size_t y) {
    T *ptr = data_.get();
    if (ptr != nullptr && x < width_ && y < height_) {
      return &ptr[convertIndex(x, y)];
    }
    return nullptr;
  }

  //设置坐标 (x, y) 处的数据值。
  inline void set(size_t x, size_t y, const T &pixel) {
    T *ptr = data_.get();
    if (ptr != nullptr && x < width_ && y < height_) {
      ptr[convertIndex(x, y)] = pixel;
    }
  }

  //将缓冲区数据拷贝到外部内存 out。
  void copyRawDataTo(T *out, bool flip_y = false) const {
    T *ptr = data_.get();
    if (ptr != nullptr) {
      if (!flip_y) {
        memcpy(out, ptr, dataSize_ * sizeof(T));
      } else {
        for (int i = 0; i < innerHeight_; i++) {
          memcpy(out + innerWidth_ * i, ptr + innerWidth_ * (innerHeight_ - 1 - i),
                 innerWidth_ * sizeof(T));
        }
      }
    }
  }

  inline void clear() const {
    T *ptr = data_.get();
    if (ptr != nullptr) {
      memset(ptr, 0, dataSize_ * sizeof(T));
    }
  }

  //遍历缓冲区，将所有元素设为 val。
  inline void setAll(T val) const {
    T *ptr = data_.get();
    if (ptr != nullptr) {
      for (int i = 0; i < dataSize_; i++) {
        ptr[i] = val;
      }
    }
  }

 protected:
  size_t width_ = 0;//缓冲区的逻辑宽度和高度。
  size_t height_ = 0;
  size_t innerWidth_ = 0;//实际分配的内存宽度和高度（可能因对齐或分块而不同）。
  size_t innerHeight_ = 0;
  std::shared_ptr<T> data_ = nullptr; //通过 std::shared_ptr<T> 管理的缓冲区数据指针，支持自动内存释放。
  size_t dataSize_ = 0;     //缓冲区实际分配的总元素个数（innerWidth_ * innerHeight_）。
};

template<typename T>//将图像划分为固定大小的块（默认 4x4），块内按行存储。
class TiledBuffer : public Buffer<T> {
public:

    void initLayout() override {
        tileWidth_ = (this->width_ + tileSize_ - 1) / tileSize_;
        tileHeight_ = (this->height_ + tileSize_ - 1) / tileSize_;
        this->innerWidth_ = tileWidth_ * tileSize_;
        this->innerHeight_ = tileHeight_ * tileSize_;
}

    inline size_t convertIndex(size_t x, size_t y) const override {
        uint16_t tileX = x >> bits_;              // x / tileSize_
        uint16_t tileY = y >> bits_;              // y / tileSize_
        uint16_t inTileX = x & (tileSize_ - 1);   // x % tileSize_
        uint16_t inTileY = y & (tileSize_ - 1);   // y % tileSize_

        return ((tileY * tileWidth_ + tileX) << bits_ << bits_) + (inTileY << bits_) + inTileX;
    }

    BufferLayout getLayout() const override {
        return Layout_Tiled;
    }

private:
    const static int tileSize_ = 4;   // 4 x 4
    const static int bits_ = 2;       // tileSize_ = 2^bits_
    size_t tileWidth_ = 0;
    size_t tileHeight_ = 0;
};

template<typename T>//将图像划分为固定大小的块（默认 32x32），块内按 Morton 顺序存储。
class MortonBuffer : public Buffer<T> {
public:

    void initLayout() override {
        tileWidth_ = (this->width_ + tileSize_ - 1) / tileSize_;
        tileHeight_ = (this->height_ + tileSize_ - 1) / tileSize_;
        this->innerWidth_ = tileWidth_ * tileSize_;
        this->innerHeight_ = tileHeight_ * tileSize_;
    }

    /**
     * Ref: https://gist.github.com/JarkkoPFC/0e4e599320b0cc7ea92df45fb416d79a
     */
    static inline uint16_t encode16_morton2(uint8_t x_, uint8_t y_) {
        uint32_t res = x_ | (uint32_t(y_) << 16);
        res = (res | (res << 4)) & 0x0f0f0f0f;
        res = (res | (res << 2)) & 0x33333333;
        res = (res | (res << 1)) & 0x55555555;
        return uint16_t(res | (res >> 15));
    }

    inline size_t convertIndex(size_t x, size_t y) const override {
        uint16_t tileX = x >> bits_;              // x / tileSize_
        uint16_t tileY = y >> bits_;              // y / tileSize_
        uint16_t inTileX = x & (tileSize_ - 1);   // x % tileSize_
        uint16_t inTileY = y & (tileSize_ - 1);   // y % tileSize_

        uint16_t mortonIndex = encode16_morton2(inTileX, inTileY);

        return ((tileY * tileWidth_ + tileX) << bits_ << bits_) + mortonIndex;
    }

    BufferLayout getLayout() const override {
        return Layout_Morton;
    }

private:
    const static int tileSize_ = 32;  // 32 x 32
    const static int bits_ = 5;       // tileSize_ = 2^bits_
    size_t tileWidth_ = 0;
    size_t tileHeight_ = 0;
};

template<typename T>
std::shared_ptr<Buffer<T>> Buffer<T>::makeDefault(size_t w, size_t h) {
    std::shared_ptr<Buffer<T>> ret = nullptr;
#if SOFTGL_TEXTURE_TILED
    ret = std::make_shared<TiledBuffer<T>>();
#elif SOFTGL_TEXTURE_MORTON
    ret = std::make_shared<MortonBuffer<T>>();
#else
    ret = std::make_shared<Buffer<T>>();
#endif
    ret->create(w, h);
    return ret;
}

//makeLayout：根据传入的 BufferLayout 参数创建指定布局的缓冲区。
template<typename T>
std::shared_ptr<Buffer<T>> Buffer<T>::makeLayout(size_t w, size_t h, BufferLayout layout) {
    std::shared_ptr<Buffer<T>> ret = nullptr;

    //是否缺乏break
    switch (layout) {
    case Layout_Tiled: {
        ret = std::make_shared<TiledBuffer<T>>();
    }
    case Layout_Morton: {
        ret = std::make_shared<MortonBuffer<T>>();
    }
    case Layout_Linear:
    default: {
        ret = std::make_shared<Buffer<T>>();
    }
    }

    ret->create(w, h);
    return ret;
}


}
#endif