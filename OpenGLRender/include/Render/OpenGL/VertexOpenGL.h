#ifndef VERTEXOPENGL_H
#define VERTEXOPENGL_H

#include <glad/glad.h>
#include "Render/Vertex.h"

#include "Render/OpenGL/OpenGLUtils.h"

namespace OpenGL {

class VertexArrayObjectOpenGL : public VertexArrayObject {
public:
	// 创建vao_, 分配gpu内存并上传数据
	explicit VertexArrayObjectOpenGL(const VertexArray& vertexArr) {
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
	~VertexArrayObjectOpenGL() {
		if (vbo_) GL_CHECK(glDeleteBuffers(1, &vbo_));
		if (ebo_) GL_CHECK(glDeleteBuffers(1, &ebo_));
		if (vao_) GL_CHECK(glDeleteVertexArrays(1, &vao_));
	}

	//更新顶点数据
	void updateVertexData(void* data, size_t length) override {
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER, length, data, GL_STATIC_DRAW));
	}

	int getId() const override  {
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
