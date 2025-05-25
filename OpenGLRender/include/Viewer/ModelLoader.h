#ifndef MODELLOADER_H
#define MODELLOADER_H

#include <unordered_map>
#include <mutex>
#include <assimp/scene.h>

#include "Base/Buffer.h"
#include "Viewer/Model.h"
#include "Viewer/Config.h"
#include "Base/Geometry.h"


namespace OpenGL {

class ModelLoader {
public:
	explicit ModelLoader(Config& config);

	//  加载3D模型文件并解析为引擎内部格式
	bool loadModel(const std::string& filepath);
	bool loadSkybox(const std::string& filepath);

	inline DemoScene& getScene() { return scene_; }

	inline size_t getModelPrimitiveCnt() const {
		if (scene_.model) {
			return scene_.model->primitiveCnt;
		}
	}

	//将已加载的model状态重置,释放GPU资源
	inline void resetAllModelStates() {
		for (auto& kv : modelCache_) {
			kv.second->resetStates();
		}
		for (auto& kv : skyboxMaterialCache_) {
			kv.second->resetStates();
		}
	}

	static void loadCubeMesh(ModelVertexes& mesh);

private:
	// 在Y=axisY平面创建一个16x16单位的网格线框，作为场景参考坐标系
	void loadWorldAxis();
	// 加载点光源
	void loadLights();
	// 加载地面
	void loadFloor();

	//处理Assimp节点数据，递归构建模型节点树
	bool processNode(const aiNode* ai_node, const aiScene* ai_scene, ModelNode& outNode, glm::mat4& transform);
	
	//处理单个网格数据，将其转换为引擎内部格式
	bool processMesh(const aiMesh* ai_mesh, const aiScene* ai_scene, ModelMesh& outMesh);

	// 处理材质中的纹理数据，将Assimp纹理转换为引擎内部格式
	void processMaterial(const aiMaterial* ai_material, aiTextureType textureType, Material& material);

	//将assimp格式的矩阵转换为 glm格式
	static glm::mat4 convertMatrix(const aiMatrix4x4& m);
	static BoundingBox convertBoundingBox(const aiAABB& aabb);
	static WrapMode convertTexWrapMode(const aiTextureMapMode& mode);

	// 使​​包围盒中心与坐标系原点对齐​​的操作
	static glm::mat4 adjustModelCenter(BoundingBox& bounds);

	// 预加载场景中所有材质引用的纹理文件
	void preloadTextureFiles(const aiScene* scene, const std::string& resDir);

	//将所给路径图像加载为自定义的格式存储。
	std::shared_ptr<Buffer<RGBA>> loadTextureFile(const std::string& path);
private:
	Config& config_;

	DemoScene scene_;

	//避免重复加载
	/*key:model文件绝对路径*/
	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
	/*key：纹理文件路径*/
	std::unordered_map<std::string, std::shared_ptr<Buffer<RGBA>>> textureDataCache_;//存储原始数据
	//key: 纹理文件路径
	std::unordered_map<std::string, std::shared_ptr<SkyboxMaterial>> skyboxMaterialCache_;

	std::mutex modelLoadMutex_;
	std::mutex texCacheMutex_;
};

}
#endif