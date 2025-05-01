#ifndef MATERIAL_H
#define MATERIAL_H

#include "Base/GLMInc.h"
#include <string>
#include <vector>
#include <memory>
#include "Base/Buffer.h"
#include "Render/Texture.h"
#include "Render/ShaderProgram.h"
#include "Render/RenderStates.h"

namespace OpenGL {

enum AlphaMode {
	Alpha_Opaque,
	Alpha_Blend,
};

//定义材质使用的着色模型：
enum ShadingModel {
	Shading_Unknown = 0,
	Shading_BaseColor,// 基础颜色（无光照）
	Shading_BlinnPhong,
	Shading_PBR,
	Shading_Skybox,
	Shading_IBL_Irradiance,// 基于图像的照明（辐照度）
	Shading_IBL_Prefilter,// IBL 预滤波环境贴图
	Shading_FXAA,// 快速近似抗锯齿
};

//纹理类型标识
enum MaterialTexType {
	MaterialTexType_NONE = 0,

	MaterialTexType_ALBEDO, // 反照率贴图
	MaterialTexType_NORMAL,// 法线贴图
	MaterialTexType_EMISSIVE,
	MaterialTexType_AMBIENT_OCCLUSION,
	MaterialTexType_METAL_ROUGHNESS,

	MaterialTexType_CUBE,// 立方体贴图（用于天空盒）
	MaterialTexType_EQUIRECTANGULAR,

	MaterialTexType_IBL_IRRADIANCE, // IBL 辐照度贴图
	MaterialTexType_IBL_PREFILTER,

	MaterialTexType_QUAD_FILTER,

	MaterialTexType_SHADOWMAP,// 阴影贴图
};

//Uniform 缓冲块，标识着色器中 Uniform 缓冲块的用途
enum UniformBlockType {
	UniformBlock_Scene,
	UniformBlock_Model,
	UniformBlock_Material,
	UniformBlock_QuadFilter,// 屏幕后处理参数
	UniformBlock_IBLPrefilter,
};

//场景参数
struct UniformsScene {
	glm::vec3 u_ambientColor;
	glm::vec3 u_cameraPosition;
	glm::vec3 u_pointLightPosition;
	glm::vec3 u_pointLightColor;
};

// 模型参数
struct UniformsModel {
	glm::uint32_t u_reverseZ;
	glm::mat4 u_modelMatrix;
	glm::mat4 u_modelViewProjectionMatrix;
	glm::mat3 u_inverseTransposeModelMatrix;
	glm::mat4 u_shadowMVPMatrix;// 阴影映射的 MVP 矩阵
};

//定义材质相关的 ​​Uniform 变量
struct UniformsMaterial {
	glm::uint32_t u_enableLight;
	glm::uint32_t u_enableIBL;
	glm::uint32_t u_enableShadow;

	glm::float32_t u_pointSize;
	glm::float32_t u_kSpecular;
	glm::vec4 u_baseColor;
};

struct UniformsQuadFilter {
	glm::vec2 u_screenSize;
};

struct UniformsIBLPrefilter {
	glm::float32_t u_srcResolution;
	glm::float32_t u_roughness;
};

//存储纹理的原始数据和参数：
struct TextureData {
	std::string tag; //存储纹理图像的绝对地址
	size_t width = 0;
	size_t height = 0;
	std::vector<std::shared_ptr<Buffer<RGBA>>> data;
	WrapMode wrapModeU = Wrap_REPEAT;
	WrapMode wrapModeV = Wrap_REPEAT;
	WrapMode wrapModeW = Wrap_REPEAT;
};

//存储​​运行时资源​​（着色器、GPU状态）
class MaterialObject {
public:
	ShadingModel shadingModel = Shading_Unknown;
	std::shared_ptr<RenderStates> renderStates;
	std::shared_ptr<ShaderProgram> shaderProgram;
	std::shared_ptr<ShaderResources> shaderResources;
};

//用于管理渲染时的材质属性、纹理和着色器配置
//存储​​材质属性​​（颜色、纹理路径等）和​​逻辑配置​​（是否双面渲染）
//是渲染一个mesh所需的资源
class Material {
public:
	static const char* shadingModelStr(ShadingModel model);
	static const char* materialTexTypeStr(MaterialTexType usage);
	static const char* samplerDefine(MaterialTexType usage);
	static const char* samplerName(MaterialTexType usage);

public:
	//完全释放
	virtual void reset() {
		shadingModel = Shading_Unknown;
		doubleSided = false;
		alphaMode = Alpha_Opaque;

		baseColor = glm::vec4(1.f);
		pointSize = 1.f;
		lineWidth = 1.f;

		textureData.clear();

		shaderDefines.clear();
		textures.clear();
		materialObj = nullptr;
	}
	//仅释放​​需要重新创建的GPU资源
	virtual void resetStates() {
		textures.clear();
		shaderDefines.clear();
		materialObj = nullptr;
	}

public:
	ShadingModel shadingModel = Shading_Unknown;
	bool doubleSided = false;// 是否双面渲染（禁用背面剔除）
	AlphaMode alphaMode = Alpha_Opaque;

	glm::vec4 baseColor = glm::vec4(1.f);
	float pointSize = 1.f; // 用于点光源
	float lineWidth = 1.f; // 用于网格线

	// key ：自定义纹理类型     
	std::unordered_map<int, TextureData> textureData;//原始的cpu端纹理数据

	std::set<std::string> shaderDefines;
	std::unordered_map<int, std::shared_ptr<Texture>> textures; //纹理对象
	std::shared_ptr<MaterialObject> materialObj = nullptr;
};

class SkyboxMaterial : public Material {
public:
	void resetStates() override {
		Material::resetStates();
		iblReady = false;
	}

public:
	bool iblReady = false;
};


}
#endif
