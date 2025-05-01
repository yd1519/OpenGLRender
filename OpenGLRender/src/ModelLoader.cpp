#include "Viewer/ModelLoader.h"
#include "Base/Logger.h"
#include "Base/ImageUtils.h"
#include "Base/ThreadPool.h"
#include "Viewer/Material.h"
#include "Viewer/Cube.h"
#include "Base/StringUtils.h"
#include <glm/glm/gtc/matrix_transform.hpp>
#include <set>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <iostream>


namespace OpenGL {
	ModelLoader::ModelLoader(Config& config) : config_(config) {
		loadWorldAxis();
		loadLights();
		loadFloor();
	}

	void ModelLoader::loadCubeMesh(ModelVertexes& mesh) {
		const float* cubeVertexes = Cube::getCubeVertexes();

		mesh.primitiveType = Primitive_TRIANGLE;
		mesh.primitiveCnt = 12;
		for (int i = 0; i < 12; i++) {
			for (int j = 0; j < 3; j++) {
				Vertex vertex{};
				vertex.a_position.x = cubeVertexes[i * 9 + j * 3 + 0];
				vertex.a_position.y = cubeVertexes[i * 9 + j * 3 + 1];
				vertex.a_position.z = cubeVertexes[i * 9 + j * 3 + 2];
				mesh.vertexes.push_back(vertex);
				mesh.indices.push_back(i * 3 + j);
			}
		}
		mesh.InitVertexes();
	}

	void ModelLoader::loadWorldAxis() {
		// 网格平面高度（略低于地面）
		float axisY = -0.01f;
		int index = 0;// 顶点索引计数器
		//---------------------------------生成网格顶点和索引--------------
		for (int i = -16; i <= 16; i++) {
			//沿x轴方向生成平行线，每条线需要两个顶点
			scene_.worldAxis.vertexes.push_back({ glm::vec3(-3.2, axisY, 0.2f * (float)i) });
			scene_.worldAxis.vertexes.push_back({ glm::vec3(3.2, axisY, 0.2f * (float)i) });
			scene_.worldAxis.indices.push_back(index++);
			scene_.worldAxis.indices.push_back(index++);

			// 沿z轴方向生成平行线
			scene_.worldAxis.vertexes.push_back({ glm::vec3(0.2f * (float)i, axisY, -3.2) });
			scene_.worldAxis.vertexes.push_back({ glm::vec3(0.2f * (float)i, axisY, 3.2) });
			scene_.worldAxis.indices.push_back(index++);
			scene_.worldAxis.indices.push_back(index++);
		}
		//----------------------初始化网格数据---------------------
		scene_.worldAxis.InitVertexes();
		//-----------------------设置渲染参数---------------------
		scene_.worldAxis.primitiveType = Primitive_LINE;
		scene_.worldAxis.primitiveCnt = scene_.worldAxis.indices.size() / 2;
		scene_.worldAxis.material = std::make_shared<Material>();
		scene_.worldAxis.material->shadingModel = Shading_BaseColor;
		scene_.worldAxis.material->baseColor = glm::vec4(0.25f, 0.25f, 0.25f, 1.f);
		scene_.worldAxis.material->lineWidth = 1.f;
		//------------------
	}

	void ModelLoader::loadLights() {
		// 设置点光源的渲染参数
		scene_.pointLight.primitiveType = Primitive_POINT;
		scene_.pointLight.primitiveCnt = 1;
		// 准备顶点和索引数据
		scene_.pointLight.vertexes.resize(scene_.pointLight.primitiveCnt);
		scene_.pointLight.indices.resize(scene_.pointLight.primitiveCnt);
		scene_.pointLight.vertexes[0] = { config_.pointLightPosition };
		scene_.pointLight.indices[0] = 0;
		// 创建并配置点光源材质
		scene_.pointLight.material = std::make_shared<Material>();
		scene_.pointLight.material->shadingModel = Shading_BaseColor;
		scene_.pointLight.material->baseColor = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
		scene_.pointLight.material->lineWidth = 1.0f;
		// 初始化顶点缓冲区
		scene_.pointLight.InitVertexes();
	}

	void ModelLoader::loadFloor() {
		// 地面参数配置
		float floorY = 0.01f;    // 地面Y坐标（略高于0避免渲染冲突）
		float floorSize = 2.0f;  // 地面半边长（总尺寸4x4单位）
		//-------------------------定义地面顶点数据-------------------------
		scene_.floor.vertexes.push_back({ glm::vec3(-floorSize, floorY, floorSize), glm::vec2(0.f, 1.f),
										  glm::vec3(0.f, 1.f, 0.f) }); // 左上
		scene_.floor.vertexes.push_back({ glm::vec3(-floorSize, floorY, -floorSize), glm::vec2(0.f, 0.f),
										  glm::vec3(0.f, 1.f, 0.f) }); // 左下
		scene_.floor.vertexes.push_back({ glm::vec3(floorSize, floorY, -floorSize), glm::vec2(1.f, 0.f),
										  glm::vec3(0.f, 1.f, 0.f) }); // 右下
		scene_.floor.vertexes.push_back({ glm::vec3(floorSize, floorY, floorSize), glm::vec2(1.f, 1.f),
										  glm::vec3(0.f, 1.f, 0.f) }); //右上
		//-------------------------定义三角形索引----------------------
		scene_.floor.indices.push_back(0);
		scene_.floor.indices.push_back(2);
		scene_.floor.indices.push_back(1);
		scene_.floor.indices.push_back(0);
		scene_.floor.indices.push_back(3);
		scene_.floor.indices.push_back(2);
		//-------------------------设置渲染参数---------------------
		scene_.floor.primitiveType = Primitive_TRIANGLE;
		scene_.floor.primitiveCnt = 2;
		// ------------------------------设置材质-----------------
		scene_.floor.material = std::make_shared<Material>();
		scene_.floor.material->shadingModel = Shading_BlinnPhong;
		scene_.floor.material->baseColor = glm::vec4(1.0f);
		scene_.floor.material->doubleSided = true;

		scene_.floor.aabb = BoundingBox(glm::vec3(-2, 0, -2), glm::vec3(2, 0, 2));
		scene_.floor.InitVertexes();
	}

	bool ModelLoader::loadSkybox(const std::string& filepath) {
		//------------------------参数校验-------------------------
		if (filepath.empty()) {
			return false;
		}
		//----------------------确保天空盒立方体网格已初始化-------------------------
		if (scene_.skybox.primitiveCnt <= 0) {
			loadCubeMesh(scene_.skybox);
		}
		//--------------------------检查材质缓存----------------------
		auto it = skyboxMaterialCache_.find(filepath);
		if (it != skyboxMaterialCache_.end()) {
			scene_.skybox.material = it->second;
			return true;
		}
		//------------------------创建天空盒材质-------------------------
		LOGD("load skybox, path: %s", filepath.c_str());
		std::shared_ptr<SkyboxMaterial> material = std::make_shared<SkyboxMaterial>();
		material->shadingModel = Shading_Skybox;

		std::vector<std::shared_ptr<Buffer<RGBA>>> skyboxTex;
		//------------------------分情况处理两种纹理格式-----------------------
		if (StringUtils::endsWith(filepath, "/")) {// 立方体贴图模式（6个面）
			skyboxTex.resize(6);

			// 使用线程池并行加载6个面纹理
			ThreadPool pool(6);
			pool.pushTask([&](int thread_id) { skyboxTex[0] = loadTextureFile(filepath + "right.jpg"); });
			pool.pushTask([&](int thread_id) { skyboxTex[1] = loadTextureFile(filepath + "left.jpg"); });
			pool.pushTask([&](int thread_id) { skyboxTex[2] = loadTextureFile(filepath + "top.jpg"); });
			pool.pushTask([&](int thread_id) { skyboxTex[3] = loadTextureFile(filepath + "bottom.jpg"); });
			pool.pushTask([&](int thread_id) { skyboxTex[4] = loadTextureFile(filepath + "front.jpg"); });
			pool.pushTask([&](int thread_id) { skyboxTex[5] = loadTextureFile(filepath + "back.jpg"); });
			pool.waitTasksFinish();

			auto& texData = material->textureData[MaterialTexType_CUBE];
			texData.tag = filepath;
			texData.width = skyboxTex[0]->getWidth();
			texData.height = skyboxTex[0]->getHeight();
			texData.data = std::move(skyboxTex);
			texData.wrapModeU = Wrap_CLAMP_TO_EDGE;
			texData.wrapModeV = Wrap_CLAMP_TO_EDGE;
			texData.wrapModeW = Wrap_CLAMP_TO_EDGE;
		}
		else {// 全景贴图模式（单张纹理）
			skyboxTex.resize(1);
			skyboxTex[0] = loadTextureFile(filepath);

			auto& texData = material->textureData[MaterialTexType_EQUIRECTANGULAR];
			texData.tag = filepath;
			texData.width = skyboxTex[0]->getWidth();
			texData.height = skyboxTex[0]->getHeight();
			texData.data = std::move(skyboxTex);
			texData.wrapModeU = Wrap_CLAMP_TO_EDGE;
			texData.wrapModeV = Wrap_CLAMP_TO_EDGE;
			texData.wrapModeW = Wrap_CLAMP_TO_EDGE;
		}
		//--------------------------更新缓存和场景--------------
		skyboxMaterialCache_[filepath] = material; // 加入缓存
		scene_.skybox.material = material;         // 应用到天空盒

		return true;
	}  

	bool ModelLoader::loadModel(const std::string& filepath) {
		std::lock_guard<std::mutex> lock(modelLoadMutex_);
		// -------------------------输入验证--------------------------------
		if (filepath.empty()) {
			LOGE("模型文件路径为空");
			return false;
		}
		//-------------------------缓存检查---------------------------------
		auto it = modelCache_.find(filepath);
		if (it != modelCache_.end()) {
			scene_.model = it->second;
			return true;
		}
		//------------------------创建新模型-----------------------------
		modelCache_[filepath] = std::make_shared<Model>();
		scene_.model = modelCache_[filepath];
		LOGD("开始加载模型: %s", filepath.c_str());
		//-------------------------Assimp模型读取--------------------------------
		Assimp::Importer importer;
		// 设置模型处理标志（重要！）
		const unsigned int flags =
			aiProcess_Triangulate |          // 确保所有面都是三角形
			aiProcess_CalcTangentSpace |     // 计算切线空间（用于法线贴图）
			aiProcess_FlipUVs |              // 翻转UV垂直方向（适配OpenGL坐标系）
			aiProcess_GenBoundingBoxes;       // 生成每个网格的包围盒

		// 读取模型文件
		const aiScene* scene = importer.ReadFile(filepath, flags);

		// 检查加载结果
		if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			LOGE("模型加载失败: %s", importer.GetErrorString());
			return false;
		}
		
		// 提取文件目录
		scene_.model->resourcePath = filepath.substr(0, filepath.find_last_of('/'));
		//-------------------------纹理预加载--避免重复加载-----------------------------------
		preloadTextureFiles(scene, scene_.model->resourcePath);
		//------------------------节点层级处理-----------------------------------
		glm::mat4 currTransform = glm::mat4(1.0f);  // 初始化为单位矩阵
		if (!processNode(scene->mRootNode, scene, scene_.model->rootNode, currTransform)) {
			LOGE("节点处理失败");
			return false;
		}

		//----------------------模块中心化处理--------------------------
		scene_.model->centeredTransform = adjustModelCenter(scene_.model->rootAABB);
		return true;

	}

	bool ModelLoader::processNode(const aiNode* ai_node,
								  const aiScene* ai_scene,
								  ModelNode& outNode,
								  glm::mat4& transform) {
		// 安全检查
		if (!ai_node) {
			return false;
		}
		// -----------转换节点局部变换矩阵并计算当前世界变换-------------
		outNode.transform = convertMatrix(ai_node->mTransformation);
		auto currTransform = transform * outNode.transform;// 累积世界变换
		// -----------------处理当前节点所有网格------------------------
		for (size_t i = 0; i < ai_node->mNumMeshes; i++) {
			const aiMesh* meshPtr = ai_scene->mMeshes[ai_node->mMeshes[i]];
			if (meshPtr) {
				ModelMesh mesh;
				if (processMesh(meshPtr, ai_scene, mesh)) {
					scene_.model->meshCnt++;
					scene_.model->primitiveCnt += mesh.primitiveCnt;
					scene_.model->vertexCnt += mesh.vertexes.size();

					// 变换网格AABB并合并到模型根AABB
					auto bounds = mesh.aabb.transform(currTransform);
					scene_.model->rootAABB.merge(bounds);

					outNode.meshes.push_back(std::move(mesh));
				}
			}
		}
		// --------------------递归处理所有子节点-------------------
		for (size_t i = 0; i < ai_node->mNumChildren; i++) {
			ModelNode children;
			if (processNode(ai_node->mChildren[i], ai_scene, children, currTransform)) {
				outNode.children.push_back(std::move(children));
			}
		}
		return true;
	}

	bool ModelLoader::processMesh(const aiMesh* ai_mesh, const aiScene* ai_scene, ModelMesh& outMesh) {
		std::vector<Vertex> vertexes;
		std::vector<int> indices;
		// ----------------------------顶点数据处理-----------------------------
		for (size_t i = 0; i < ai_mesh->mNumVertices; i++) {
			Vertex vertex;
			
			if (ai_mesh->HasPositions()) {
				vertex.a_position.x = ai_mesh->mVertices[i].x;
				vertex.a_position.y = ai_mesh->mVertices[i].y;
				vertex.a_position.z = ai_mesh->mVertices[i].z;
			}

			if (ai_mesh->HasTextureCoords(0)) {// 第 0 套uv坐标
				vertex.a_texCoord.x = ai_mesh->mTextureCoords[0][i].x;
				vertex.a_texCoord.y = ai_mesh->mTextureCoords[0][i].y;
			}
			else {
				vertex.a_texCoord.x = 0.0f;
				vertex.a_texCoord.y = 0.0f;
			}

			if (ai_mesh->HasNormals()) {
				vertex.a_normal.x = ai_mesh->mNormals[i].x;
				vertex.a_normal.y = ai_mesh->mNormals[i].y;
				vertex.a_normal.z = ai_mesh->mNormals[i].z;
			}

			if (ai_mesh->HasTangentsAndBitangents()) {
				vertex.a_tangent.x = ai_mesh->mTangents[i].x;
				vertex.a_tangent.y = ai_mesh->mTangents[i].y;
				vertex.a_tangent.z = ai_mesh->mTangents[i].z;
			}

			vertexes.push_back(std::move(vertex));
		}
		
		//------------------------------索引数据处理---------------------------------------
		for (size_t i = 0; i < ai_mesh->mNumFaces; i++) {
			aiFace face = ai_mesh->mFaces[i];
			if (face.mNumIndices != 3) {
				LOGE("ModelLoader::processMesh, mesh not transformed to triangle mesh.");
				return false;
			}
			for (size_t j = 0; j < face.mNumIndices; j++) {
				indices.push_back((int)(face.mIndices[j]));
			}
		}

		//--------------------------------材质初始化---------------------------------------
		outMesh.material = std::make_shared<Material>();
		outMesh.material->baseColor = glm::vec4(1.f);
		if (ai_mesh->mMaterialIndex >= 0) {
			const aiMaterial* material = ai_scene->mMaterials[ai_mesh->mMaterialIndex];
			
			//-------------------处理alpha模式-----------
			outMesh.material->alphaMode = Alpha_Opaque;//默认不透明
			aiString alphaMode;
			if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == aiReturn_SUCCESS) {
				if (aiString("BLEND") == alphaMode) {
					outMesh.material->alphaMode = Alpha_Blend;
				}
			}

			// -----------------处理双面渲染标记-----------
			outMesh.material->doubleSided = false;
			bool doubleSide;
			if (material->Get(AI_MATKEY_TWOSIDED, doubleSide) == aiReturn_SUCCESS) {
				outMesh.material->doubleSided = doubleSide;
			}

			//----------------处理着色模型-----------------------
			outMesh.material->shadingModel = Shading_BlinnPhong;
			aiShadingMode shading_mode;
			if (material->Get(AI_MATKEY_SHADING_MODEL, shading_mode), aiReturn_SUCCESS) {
				if (aiShadingMode_PBR_BRDF == shading_mode) {
					outMesh.material->shadingModel = Shading_PBR;
				}
			}

			//------------处理各种纹理----------------
			for (int i = 0; i <= AI_TEXTURE_TYPE_MAX; i++) {
				processMaterial(material, static_cast<aiTextureType>(i), *outMesh.material);
			}
		}

		//--------------------------------网格最终设置---------------------------------------
		outMesh.primitiveType = Primitive_TRIANGLE;
		outMesh.primitiveCnt = ai_mesh->mNumFaces;
		outMesh.vertexes = std::move(vertexes);
		outMesh.indices = std::move(indices);
		outMesh.aabb = convertBoundingBox(ai_mesh->mAABB);
		outMesh.InitVertexes();
	}

	void ModelLoader::processMaterial(const aiMaterial* ai_material,
									  aiTextureType textureType,
									  Material& material) {

		//检查是否存在该类型的纹理数据
		if (ai_material->GetTextureCount(textureType) <= 0) {
			return;
		}

		//--------------------------遍历该类型的所有纹理---------------------------
		for (size_t i = 0; i < ai_material->GetTextureCount(textureType); i++) {
			aiTextureMapMode texMapMode[2];/*纹理UV环绕模式*/
			aiString texPath;
			aiReturn retStatus = ai_material->GetTexture(textureType, i, &texPath,
														 nullptr, nullptr, nullptr, nullptr,
													     &texMapMode[0]);
			if (retStatus != aiReturn_SUCCESS || texPath.length == 0) {
				LOGW("load texture type=%d, index=%d failed with return value=%d", textureType, i, retStatus);
				continue;
			}

			// 构造绝对路径
			std::string absolutePath = scene_.model->resourcePath + "/" + texPath.C_Str();

			// ------------将纹理类型转化为引擎自定义类型---------------
			MaterialTexType texType = MaterialTexType_NONE;
			switch (textureType) {
				case aiTextureType_BASE_COLOR:        
				case aiTextureType_DIFFUSE:           
					texType = MaterialTexType_ALBEDO;   
					break;
				case aiTextureType_NORMALS:           
					texType = MaterialTexType_NORMAL;
					break;
				case aiTextureType_EMISSIVE:          
					texType = MaterialTexType_EMISSIVE;
					break;
				case aiTextureType_LIGHTMAP:          // 光照贴图（用作AO）
					texType = MaterialTexType_AMBIENT_OCCLUSION;
					break;
				case aiTextureType_UNKNOWN:           // 未知类型（可能用于PBR的金属/粗糙度）   
					texType = MaterialTexType_METAL_ROUGHNESS;
					break;
				default:// 不支持的纹理类型直接跳过
					//LOGW("texture type: %s not support", aiTextureTypeToString(textureType));
					continue;  // not support
			}
			//------------------------------加载纹理文件数据到内存-----------------------
			std::shared_ptr<Buffer<RGBA>> buffer = loadTextureFile(absolutePath);
			if (buffer) {
				auto& texData = material.textureData[texType];
				texData.tag = absolutePath;
				texData.width = buffer->getWidth();
				texData.height = buffer->getHeight();
				texData.data.push_back(buffer);
				texData.wrapModeU = convertTexWrapMode(texMapMode[0]);
				texData.wrapModeV = convertTexWrapMode(texMapMode[1]);
			}
			else {
				LOGE("load texture failed: %s, path: %s", Material::materialTexTypeStr(texType), absolutePath.c_str());
			}
		}

		
	}

	void ModelLoader::preloadTextureFiles(const aiScene* scene, const std::string& resDir) {
		// --------------------------收集场景中所有纹理路径-------------------------

		// 使用set去重
		std::set<std::string> texPaths;

		// 遍历所有材质
		for (int materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
			aiMaterial* material = scene->mMaterials[materialIndex];

			// 遍历材质所用的所有纹理（法线、漫反射...)
			for (int texType = aiTextureType_NONE; texType <= AI_TEXTURE_TYPE_MAX; texType++) {
				auto textureType = static_cast<aiTextureType>(texType);//显示转化为枚举类型。
				size_t texCnt = material->GetTextureCount(textureType);

				// 遍历每个纹理
				for (size_t i = 0; i < texCnt; i++) {
					aiString texPath;
					aiReturn retStates = material->GetTexture(textureType, i, &texPath);

					//  跳过无效纹理路径
					if (retStates != aiReturn_SUCCESS || texPath.length == 0) {
						continue;
					}

					texPaths.insert(resDir + "/" + texPath.C_Str());
				}
			}
		}

		if (texPaths.empty()) {
			return;
		}
		// --------------------------使用线程池并发加载纹理-------------------------
		ThreadPool pool(std::min(texPaths.size(), (size_t)std::thread::hardware_concurrency()));
		for (auto& path : texPaths) {
			// [&]​​ 表示 ​​以引用方式捕获所有外部变量​​（隐式捕获）。
			pool.pushTask([&](int threadId) {loadTextureFile(path); });
		}
	}
	
	std::shared_ptr<Buffer<RGBA>> ModelLoader::loadTextureFile(const std::string& path) {

		//检查是否已经在缓存中了
		texCacheMutex_.lock();
		if (textureDataCache_.find(path) != textureDataCache_.end()) {
			/*应避免先解锁后访问数据
			texCacheMutex_.unlock();
			return textureDataCache_[path];
			*/
			auto& buffer = textureDataCache_[path];
			texCacheMutex_.unlock();
			return buffer;
		}
		texCacheMutex_.unlock();

		LOGD("load texture, path: %s", path.c_str());

		//不在缓存中则读取至cache中
		auto buffer = ImageUtils::readImageRGBA(path);
		if (buffer == nullptr) {
			LOGD("load texture failed, path: %s", path.c_str());
			return nullptr;
		}

		texCacheMutex_.lock();
		textureDataCache_[path] = buffer;
		texCacheMutex_.unlock();

		return buffer;

	}

	glm::mat4 ModelLoader::convertMatrix(const aiMatrix4x4& m) {
		glm::mat4 ret;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				ret[j][i] = m[i][j];
			}
		}
		return ret;
	}

	BoundingBox ModelLoader::convertBoundingBox(const aiAABB& aabb) {
		BoundingBox ret{};
		ret.min = glm::vec3(aabb.mMin.x, aabb.mMin.y, aabb.mMin.z);
		ret.max = glm::vec3(aabb.mMax.x, aabb.mMax.y, aabb.mMax.z);
		return ret;
	}

	WrapMode ModelLoader::convertTexWrapMode(const aiTextureMapMode& mode) {
		WrapMode retWrapMode;
		switch (mode) {
		case aiTextureMapMode_Wrap:
			retWrapMode = Wrap_REPEAT;
			break;
		case aiTextureMapMode_Clamp:
			retWrapMode = Wrap_CLAMP_TO_EDGE;
			break;
		case aiTextureMapMode_Mirror:
			retWrapMode = Wrap_MIRRORED_REPEAT;
			break;
		default:
			retWrapMode = Wrap_REPEAT;
			break;
		}

		return retWrapMode;
	}
	
	glm::mat4 ModelLoader::adjustModelCenter(BoundingBox& bounds) {
		glm::mat4 modelTransform(1.0f);
		//--------------------------计算平移量--------------------------
		glm::vec3 trans = (bounds.max + bounds.min) / -2.f; //平移向量
		trans.y = -bounds.min.y;//将模型底部（min.y）对齐到原点平面
		//-------------------------计算缩放比例--------------------------
		float bounds_len = glm::length(bounds.max - bounds.min);
		float scale_factor = 3.f / bounds_len;
		//--------------------------构建变换矩阵--------------------------------
		modelTransform = glm::scale(modelTransform, glm::vec3(scale_factor));
		modelTransform = glm::translate(modelTransform, trans);
		return modelTransform;
	}
}
