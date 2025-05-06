#ifndef VERTEX_H
#define VERTEX_H

#include <glad/glad.h>

#include <vector>
#include <memory>
#include "Base/GLMInc.h"


namespace OpenGL {

	class VertexArrayObject {
	public:
		virtual int getId() const = 0;
		virtual void updateVertexData(void* data, size_t length) = 0;
	};

	// 用于定义顶点属性指针时使用
	struct VertexAttributeDesc {
		size_t size;
		size_t stride;
		size_t offset;
	};

	//定义顶点数组
	struct VertexArray {

		size_t vertexSize = 0;//单个顶点数据大小
		std::vector<VertexAttributeDesc> vertexesDesc;

		//uint8_t* 可以隐式转换为其他指针类型（如 float*），
		// 便于后续绑定到图形 API 的顶点缓冲区。
		uint8_t* vertexesBuffer = nullptr;
		size_t vertexesBufferLength = 0; //顶点数据总长度。

		int32_t* indexBuffer = nullptr;
		size_t indexBufferLength = 0; // 索引数量
	};

}


#endif