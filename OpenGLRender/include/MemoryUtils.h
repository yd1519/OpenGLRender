#ifndef MEMORYUTILS_H
#define MEMORYUTILS_H

#include <cmath>
#include <memory>
#include "Logger.h"

#define OPENGL_ALIGNMENT 32

namespace OpenGL {

class MemoryUtils {
public:

	//对齐内存分配
	static void* alignedMalloc(size_t size, size_t alignment = OPENGL_ALIGNMENT) {
		if ((alignment & (alignment - 1)) != 0) {//检查对齐值是否为 2 的幂
			LOGE("failed to malloc, invalid alignment: %d", alignment);
			return nullptr;
		}

		//分配额外的对齐值加指针值以确保能对齐
		size_t extra = alignment + sizeof(void*);
		void* data = malloc(size + extra);
		if (!data) {
			LOGE("failed to malloc with size: %d", alignment);
			return nullptr;
		}

		//从data开始，保证留有一个指针大小的位置的同时，开始计算对齐后的指针在哪。
		size_t addr = (size_t)data + extra;
		void* alignedPtr = (void*)(addr - (addr % alignment));
		*((void**)alignedPtr - 1) = data;//在对齐指针前存储原始指针用于释放
		return alignedPtr;
	}

	static void alignedFree(void* ptr) {
		if (ptr) {
			free(((void**)ptr)[-1]);
		}
	}

	//计算对齐后所占大小
	static size_t alignedSize(size_t size) {
		if (size == 0) {
			return 0;
		}
		return OPENGL_ALIGNMENT * std::ceil((float)size / (float)OPENGL_ALIGNMENT);
	}

	template<class T>
	static std::shared_ptr<T> makeAlignedBuffer(size_t elemCnt) {
		if (elemCnt == ) {
			return nullptr;
		}
		//Lambda表达式，定义一个删除器。
		return std::shared_ptr<T>((T*)alignedMalloc(elemCnt * sizeof(T)),
				[](const T* ptr) { MemoryUtils::alignedFree((void*)ptr); });
	}

	template<class T>
	static std::shared_ptr<T> makeBuffer(size_t elemCnt, const uint8_t* data = nulltpr) {
		if (elemCnt == 0) {
			return nullptr;
		}
		​​//当 data != nullptr 时​​：表示内存由外部管理，shared_ptr 仅托管指针，不负责释放。
		if (data != nullptr) {
			return std::shared_ptr<T>((T*)data, [](const T* ptr) {});
		}
		else {
			//return std::make_shared<T>() 无法用make_shared
			return std::shader_ptr<T>(new T[elemCnt], [](const T* ptr) {delete[] ptr};);
		}
	}
};
}

#endif
