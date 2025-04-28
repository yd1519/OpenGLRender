#ifndef VERTEX_H
#define VERTEX_H

#include <glad/glad.h>

#include <vector>
#include <memory>
#include "GLMInc.h"
#include "OpenGLUtils.h"

namespace OpenGL {
//用于定义顶点属性指针时使用
struct VertexAttributeDesc {
	size_t size;
	size_t stride;
	size_t offset;
};

//定义顶点数组
struct VertexArray {

	size_t vertexSize = 0;//单个顶点大小
	std::vector<VertexAttributeDesc> vertexesDesc;

	//uint8_t* 可以隐式转换为其他指针类型（如 float*），
	// 便于后续绑定到图形 API 的顶点缓冲区。
	uint8_t* vertexesBuffer = nullptr;
	size_t vertexesBufferLength = 0; //顶点数据总长度。

	int32_t* indexBuffer = nullptr;
	size_t indexBufferLength = 0; // 索引数量
};


class VertexArrayObject {
public:
	explicit VertexArrayObject(const VertexArray& vertexArr) {
		//如果顶点数组为空则返回
		if (!vertexArr.vertexesBuffer || !vertexArr.indexBuffer) {
			return;
		}
		indicesCnt_ = vertexArr.indexBufferLength / sizeof(int32_t);

		//配置vao
		GL_CHECK(glGenVertexArrays(1, &vao_));
		GL_CHECK(glBindVertexArray(vao_));

		//配置vbo
		GL_CHECK(glGenBuffers(1, &vbo_));
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER,
								vertexArr.vertexesBufferLength,
								vertexArr.vertexesBuffer,
								GL_STATIC_DRAW));
		//配置顶点属性
		for (int i = 0; i < vertexArr.vertexesDesc.size(); i++) {
			auto& desc = vertexArr.vertexesDesc[i];
			GL_CHECK(glVertexAttribPointer(i, desc.size, GL_FLOAT, GL_FALSE, desc.stride, (void*)desc.offset));
			GL_CHECK(glEnableVertexAttribArray(i));
		}

		//配置ebo
		GL_CHECK(glGenBuffers(1, &ebo_));
		GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_));
		GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
								vertexArr.indexBufferLength,
								vertexArr.indexBuffer,
								GL_STATIC_DRAW));
	}

	//删除缓存
	~VertexArrayObject() {
		if (vbo_) GL_CHECK(glDeleteBuffers(1, &vbo_));
		if (ebo_) GL_CHECK(glDeleteBuffers(1, &ebo_));
		if (vao_) GL_CHECK(glDeleteVertexArrays(1, &vao_));
	}

	//更新顶点数据
	void updateVertexData(void* data, size_t length) {
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER, length, data, GL_STATIC_DRAW));
	}

	
	int getId() const {
		return (int)vao_;
	}

	inline void bind() const {
		if (vao_) {
			GL_CHECK(glBindVertexArray(vao_));
		}
	}

	inline size_t getIndicesCnt() const {
		return indicesCnt_;
	}


private:
	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	GLuint ebo_ = 0;
	size_t indicesCnt_ = 0;
};
}


#endif