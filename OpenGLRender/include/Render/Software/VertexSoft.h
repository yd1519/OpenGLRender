#ifndef VERTEXSOFT_H
#define VERTEXSOFT_H

#include "Base/UUID.h"
#include "Render/Vertex.h"

namespace OpenGL {

class VertexArrayObjectSoft : public VertexArrayObject {
public:
	explicit VertexArrayObjectSoft(const VertexArray& vertexArray) {
		// init vertexes
		vertexStride = vertexArray.vertexesDesc[0].stride;
		vertexCnt = vertexArray.vertexesBufferLength / vertexStride;
		vertexes.resize(vertexCnt * vertexStride);
		memcpy(vertexes.data(), vertexArray.vertexesBuffer, vertexArray.vertexesBufferLength);

		// init indices
		indicesCnt = vertexArray.indexBufferLength / sizeof(int32_t);
		indices.resize(indicesCnt);
		memcpy(indices.data(), vertexArray.indexBuffer, vertexArray.indexBufferLength);
	}

	int getId() const override {
		return uuid_.get();
	}

	void updateVertexData(void* data, size_t length) override {
		memcpy(vertexes.data(), data, std::min(length, vertexes.size()));
	}
public:
	size_t vertexStride = 0;
	size_t vertexCnt = 0;
	size_t indicesCnt = 0;
	std::vector<uint8_t> vertexes;
	std::vector<int32_t> indices;
private:
	UUID<VertexArrayObjectSoft> uuid_;
};

}

#endif
