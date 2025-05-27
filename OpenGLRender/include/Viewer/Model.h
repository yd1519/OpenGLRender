#ifndef MODEL_H
#define MODEL_H

#include <memory>
#include <string>
#include <unordered_map>
#include "Render/Vertex.h"
#include "Viewer/Material.h"
#include "Base/Geometry.h"

namespace OpenGL {

//定义单个顶点
struct Vertex {
	glm::vec3 a_position;
	glm::vec2 a_texCoord;
	glm::vec3 a_normal;
	glm::vec3 a_tangent;
};

//cpu端的数据
struct ModelVertexes : VertexArray {
	PrimitiveType primitiveType;
	size_t primitiveCnt = 0;
	std::vector<Vertex> vertexes; //最后会被转化为指针类型存储
	std::vector<int32_t> indices;

	std::shared_ptr<VertexArrayObject> vao = nullptr; //需要通过渲染器创建

	//更新vbo中的数据
	void UpdateVertexes() const {
		if (vao) {
			vao->updateVertexData(vertexesBuffer, vertexesBufferLength);
		}
	}

	//初始化顶点属性描述与缓冲指针 , 创建自定义顶点缓冲区
	void InitVertexes() {
		vertexSize = sizeof(Vertex);

		//设置用于定义顶点属性指针所需数据。
		vertexesDesc.resize(4);
		vertexesDesc[0] = { 3, sizeof(Vertex), offsetof(Vertex, a_position) };
		vertexesDesc[1] = { 2, sizeof(Vertex), offsetof(Vertex, a_texCoord) };
		vertexesDesc[2] = { 3, sizeof(Vertex), offsetof(Vertex, a_normal) };
		vertexesDesc[3] = { 3, sizeof(Vertex), offsetof(Vertex, a_tangent) };

		vertexesBuffer = vertexes.empty() ? nullptr : (uint8_t*)&vertexes[0];
		vertexesBufferLength = vertexes.size() * sizeof(Vertex);

		indexBuffer = indices.empty() ? nullptr : (int32_t*)&indices[0];
		indexBufferLength = indices.size() * sizeof(int32_t);
	}

};

struct ModelBase : ModelVertexes {
	BoundingBox aabb{};
	std::shared_ptr<Material> material = nullptr;

	virtual void resetStates() {
		vao = nullptr;
		if (material) {
			material->resetStates();
		}
	}
};

struct ModelPoints : ModelBase {};
struct ModelLines : ModelBase {};
struct ModelMesh : ModelBase {};

struct ModelNode {
	/*aiNode::mTransformation​​ 是一个描述节点（Node）相对于其父节点（Parent Node）的局部空间变换的4x4变换矩阵。
	它定义了该节点包含的所有网格（Mesh）在模型层级中的位置、旋转和缩放状态。
	相当于内部的model矩阵
	*/
	glm::mat4 transform = glm::mat4(1.0f);
	std::vector<ModelMesh> meshes;
	std::vector<ModelNode> children;
};

struct Model {
	std::string resourcePath;//模型文件绝对路径
	
	ModelNode rootNode;
	BoundingBox rootAABB;

	size_t meshCnt = 0;
	size_t primitiveCnt = 0;
	size_t vertexCnt = 0;

	glm::mat4 centeredTransform;// 模型中心化校准

	void resetStates() {
		resetNodeStates(rootNode);
	}

	void resetNodeStates(ModelNode& node) {
		for (auto& mesh : node.meshes) {
			mesh.resetStates();
		}
		for (auto& childNode : node.children) {
			resetNodeStates(childNode);
		}
	}
};

struct DemoScene {
	std::shared_ptr<Model> model;// 主模型（如角色）
	ModelLines worldAxis;// 世界坐标系
	ModelPoints pointLight;// 点光源
	ModelMesh floor; // 地面网格
	ModelMesh skybox;// 天空盒

	void resetStates() {
		if (model) { model->resetStates(); }
		worldAxis.resetStates();
		pointLight.resetStates();
		floor.resetStates();
		skybox.resetStates();
	}
};

}

#endif
