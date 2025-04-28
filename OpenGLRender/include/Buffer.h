#ifndef BUFFER_H
#define BUFFER_H

#include "GLMInc.h"
#include "MemoryUtils.h"

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