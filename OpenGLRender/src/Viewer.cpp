#include "Viewer/Viewer.h"
#include "Base/hashUtils.h"


namespace OpenGL {

#define SHADOW_MAP_WIDTH 512
#define SHADOW_MAP_HEIGHT 512

#define CREATE_UNIFORM_BLOCK(name) renderer_->createUniformBlock(#name, sizeof(name))

// camera, renderer, uniformblock, shadowplacehold, iblplacehold
bool Viewer::create(int width, int height, int outTexId) {

	cleanup();

	width_ = width;
	height_ = height;
	outTexId_ = outTexId;

	camera_ = &cameraMain_;

	// ------------------深度相机初始化-----------------
	if (!cameraDepth_) {
		cameraDepth_ = std::make_shared<Camera>();
		cameraDepth_->setPerspective(glm::radians(ZOOM),
									 (float)SHADOW_MAP_WIDTH / (float)SHADOW_MAP_HEIGHT,
									 CAMERA_NEAR, CAMERA_FAR);
	}

	//-------------------渲染器初始化------------------
	if (!renderer_) {
		renderer_ = createRenderer();// 子类函数
	}
	if (!renderer_) {// 检查渲染器是否创建成功
		LOGE("Viewer::create failed: createRenderer error");
		return false;
	}

	//---------------------创建默认资源--------------------
	uniformBlockScene_ = CREATE_UNIFORM_BLOCK(UniformsScene);
	uniformBlockModel_ = CREATE_UNIFORM_BLOCK(UniformsModel);
	uniformBlockMaterial_ = CREATE_UNIFORM_BLOCK(UniformsMaterial);

	shadowPlaceholder_ = createTexture2DDefault(1, 1, TextureFormat_FLOAT32, TextureUsage_Sampler);
	iblPlaceholder_ = createTextureCubeDefault(1, 1, TextureUsage_Sampler);

	return true;
}

//将所有成员设为nullptr
void Viewer::destroy() {
	cleanup();
	cameraDepth_ = nullptr;
	if (renderer_) {
		renderer_->destroy();
	}
	renderer_ = nullptr;
}

// 将renderer_暂停，将所有资源设为nullptr；
void Viewer::cleanup() {
	if (renderer_) {
		renderer_->waitIdle();
	}
	fboMain_ = nullptr;
	texColorMain_ = nullptr;
	texDepthMain_ = nullptr;
	fboShadow_ = nullptr;
	texDepthShadow_ = nullptr;
	shadowPlaceholder_ = nullptr;
	fxaaFilter_ = nullptr;
	texColorFxaa_ = nullptr;
	iblPlaceholder_ = nullptr;
	iblGenerator_ = nullptr;
	uniformBlockScene_ = nullptr;
	uniformBlockModel_ = nullptr;
	uniformBlockMaterial_ = nullptr;
	programCache_.clear();
	pipelineCache_.clear();
}

// 将texDepthShadow_设置为nullptr；
void Viewer::resetReverseZ() {
	texDepthShadow_ = nullptr;
}

// 等待GPU中任务完成
void Viewer::waitRenderIdle() {
	if (renderer_) {
		renderer_->waitIdle();
	}
}

void Viewer::drawFrame(DemoScene& scene) {
	if (!renderer_) {
		return;
	}
	scene_ = &scene;

	// setup framebuffer
	setupMainBuffers();// 主渲染帧缓冲（颜色+深度），会设置texColorMain_和texDepthMain_的值，同时绑定到fboMain_中。
	setupShadowMapBuffers();// 阴影贴图帧缓冲

	// init skybox textures & ibl
	initSkyboxIBL();// 天空盒纹理与基于图像的照明（IBL）

	// setup model materials
	setupScene();// 场景材质与模型数据

	// draw shadow map
	drawShadowMap(); //阴影贴图生成

	// setup fxaa
	processFXAASetup();

	// main pass
	ClearStates clearStates{};
	clearStates.colorFlag = true;
	clearStates.depthFlag = config_.depthTest;
	clearStates.clearColor = config_.clearColor;
	clearStates.clearDepth = config_.reverseZ ? 0.f : 1.f;

	renderer_->beginRenderPass(fboMain_, clearStates);//表示绑定fboMain_中的帧缓冲对象，准备开始渲染。
	renderer_->setViewPort(0, 0, width_, height_);

	// draw scene
	drawScene(false);//false表示渲染的不是阴影贴图。

	// end main pass
	renderer_->endRenderPass();

	// draw fxaa
	processFXAADraw();
}

// 绘制阴影贴图
void Viewer::drawShadowMap() {
	if (!config_.shadowMap) {
		return;
	}

	// shadow pass
	ClearStates clearDepth{};
	clearDepth.depthFlag = true;
	clearDepth.clearDepth = config_.reverseZ ? 0.f : 1.f;
	renderer_->beginRenderPass(fboShadow_, clearDepth);//绑定fboShadow_中的帧缓冲对象
	renderer_->setViewPort(0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);

	// set camera
	cameraDepth_->lookAt(config_.pointLightPosition, glm::vec3(0), glm::vec3(0, 1, 0));
	cameraDepth_->update();
	camera_ = cameraDepth_.get();

	// draw scene
	drawScene(true);

	// end shadow pass
	renderer_->endRenderPass();// reset gl states

	// set back to main camera
	camera_ = &cameraMain_;//之前是以光源为摄像机的位置。
}

// 初始化Fxaa所需的资源
void Viewer::processFXAASetup() {
	if (config_.aaType != AAType_FXAA) {
		return;
	}

	if (!texColorFxaa_) {
		TextureDesc texDesc{};
		texDesc.width = width_;
		texDesc.height = height_;
		texDesc.type = TextureType_2D;
		texDesc.format = TextureFormat_RGBA8;
		texDesc.usage = TextureUsage_Sampler | TextureUsage_AttachmentColor;
		texDesc.useMipmaps = false;
		texDesc.multiSample = false;
		texColorFxaa_ = renderer_->createTexture(texDesc);

		SamplerDesc sampler{};
		sampler.filterMin = Filter_LINEAR;
		sampler.filterMag = Filter_LINEAR;
		texColorFxaa_->setSamplerDesc(sampler);

		texColorFxaa_->initImageData(); //初始化纹理存储空间
	}

	if (!fxaaFilter_) {
		fxaaFilter_ = std::make_shared<QuadFilter>(width_, height_, renderer_,
			[&](ShaderProgram& program) -> bool {
				return loadShaders(program, Shading_FXAA);
			});
	}

	fboMain_->setColorAttachment(texColorFxaa_, 0);
	fboMain_->setOffscreen(true);

	fxaaFilter_->setTextures(texColorFxaa_, texColorMain_);
}

// 执行FXAA后处理渲染
void Viewer::processFXAADraw() {
	if (config_.aaType != AAType_FXAA) {
		return;
	}

	fxaaFilter_->draw();
}

// 根据config中的值，将scene中的成员进行setup
void Viewer::setupScene() {
	// point light
	if (config_.showLight) {
		setupPoints(scene_->pointLight);
	}

	// world axis
	if (config_.worldAxis) {
		setupLines(scene_->worldAxis);
	}

	// floor
	if (config_.showFloor) {
		if (config_.wireframe) {
			setupMeshBaseColor(scene_->floor, true);
		}
		else {
			setupMeshTextured(scene_->floor);
		}
	}

	// skybox
	if (config_.showSkybox) {
		setupSkybox(scene_->skybox);
	}

	// model nodes
	ModelNode& modelNode = scene_->model->rootNode;
	setupModelNodes(modelNode, config_.wireframe);
}

// 设置点光源的渲染管线
void Viewer::setupPoints(ModelPoints& points) {
	pipelineSetup(points, points.material->shadingModel, { UniformBlock_Model, UniformBlock_Material });
}

// 设置线段模型的渲染管线
void Viewer::setupLines(ModelLines& lines) {
	pipelineSetup(lines, lines.material->shadingModel, { UniformBlock_Model, UniformBlock_Material });
}

// 设置天空盒的渲染管线
void Viewer::setupSkybox(ModelMesh& skybox) {
	pipelineSetup(skybox, skybox.material->shadingModel,
		{ UniformBlock_Model },
		[&](RenderStates& rs) -> void {
			rs.depthFunc = config_.reverseZ ? DepthFunc_GEQUAL : DepthFunc_LEQUAL;
			rs.depthMask = false;
		});
}

// 递归设置模型节点及其子节点的网格数据
void Viewer::setupModelNodes(ModelNode& node, bool wireframe) {
	for (auto& mesh : node.meshes) {
		// setup mesh
		if (wireframe) {
			setupMeshBaseColor(mesh, true);
		}
		else {
			setupMeshTextured(mesh);
		}
	}

	// setup child
	for (auto& childNode : node.children) {
		setupModelNodes(childNode, wireframe);
	}
}

// 设置网格的基础颜色渲染管线（主要用于线框模式或简单着色）
void Viewer::setupMeshBaseColor(ModelMesh& mesh, bool wireframe) {
	pipelineSetup(mesh, Shading_BaseColor, { UniformBlock_Model, UniformBlock_Scene, UniformBlock_Material },
				  [&](RenderStates& rs) -> void {rs.polygonMode = wireframe ? PolygonMode_LINE : PolygonMode_FILL;});
}

// 设置带纹理的mesh渲染管线
void Viewer::setupMeshTextured(ModelMesh& mesh) {
	pipelineSetup(mesh, mesh.material->shadingModel, { UniformBlock_Model, UniformBlock_Scene, UniformBlock_Material });
}

// 主场景绘制函数，控制整个场景的渲染顺序和逻辑
void Viewer::drawScene(bool shadowPass) {
	/*绘制顺序：
		1. 点光源标记
		2. 世界坐标轴
		3. 地板
		4. 不透明物体
		5. 天空盒
		6. 透明物体
	*/

	// update scene uniform
	updateUniformScene();
	updateUniformModel(glm::mat4(1.0f), camera_->viewMatrix()); 

	// draw point light
	if (!shadowPass && config_.showLight) {
		updateUniformMaterial(*scene_->pointLight.material);
		pipelineDraw(scene_->pointLight);
	}

	// draw world axis
	if (!shadowPass && config_.worldAxis) {
		updateUniformMaterial(*scene_->worldAxis.material);
		pipelineDraw(scene_->worldAxis);
	}

	// draw floor
	if (!shadowPass && config_.showFloor) {
		drawModelMesh(scene_->floor, shadowPass, 0.f);
	}

	// draw model nodes opaque
	ModelNode& modelNode = scene_->model->rootNode;
	drawModelNodes(modelNode, shadowPass, scene_->model->centeredTransform, Alpha_Opaque);

	// draw skybox
	if (!shadowPass && config_.showSkybox) {
		// view matrix only rotation
		updateUniformModel(glm::mat4(1.0f), glm::mat3(camera_->viewMatrix()));
		pipelineDraw(scene_->skybox);
	}

	// draw model nodes blend
	drawModelNodes(modelNode, shadowPass, scene_->model->centeredTransform, Alpha_Blend);
}

// 递归绘制模型节点及其子节点，更新uniformmodel
void Viewer::drawModelNodes(ModelNode& node, bool shadowPass, glm::mat4& transform, AlphaMode mode, float specular) {
	//计算当前节点的世界变换矩阵
	glm::mat4 modelMatrix = transform * node.transform; //transform:父节点累计的model矩阵

	// update model uniform
	updateUniformModel(modelMatrix, camera_->viewMatrix());

	// draw nodes
	for (auto& mesh : node.meshes) {
		if (mesh.material->alphaMode != mode) {
			continue;
		}

		// frustum cull
		if (!checkMeshFrustumCull(mesh, modelMatrix)) {
			return;
		}

		drawModelMesh(mesh, shadowPass, specular);
	}

	// draw child
	for (auto& childNode : node.children) {
		drawModelNodes(childNode, shadowPass, modelMatrix, mode, specular);
	}
}

//绘制单个mesh，更新material、ibltexture、shadow texture
void Viewer::drawModelMesh(ModelMesh& mesh, bool shadowPass, float specular) {
	// update material
	updateUniformMaterial(*mesh.material, specular);

	// update IBL textures
	if (mesh.material->shadingModel == Shading_PBR) {
		updateIBLTextures(mesh.material->materialObj.get());
	}

	// update shadow textures
	if (config_.shadowMap) {
		updateShadowTextures(mesh.material->materialObj.get(), shadowPass);
	}

	// draw mesh
	pipelineDraw(mesh);
}

// 设置模型的渲染管线配置，准备模型渲染所需的所有GPU资源，包括顶点数据、材质、着色器和渲染状态
void Viewer::pipelineSetup(ModelBase& model, ShadingModel shading, const std::set<int>& uniformBlocks,
							const std::function<void(RenderStates& rs)>& extraStates) {
	setupVertexArray(model);

	// reset materialObj if ShadingModel changed
	if (model.material->materialObj != nullptr) {
		if (model.material->materialObj->shadingModel != shading) {
			model.material->materialObj = nullptr;
		}
	}

	setupMaterial(model, shading, uniformBlocks, extraStates);
}

// 执行模型绘制命令的管线配置和绘制调用
void Viewer::pipelineDraw(ModelBase& model) {
	auto& materialObj = model.material->materialObj;

	renderer_->setVertexArrayObject(model.vao);
	renderer_->setShaderProgram(materialObj->shaderProgram);
	renderer_->setShaderResources(materialObj->shaderResources);
	renderer_->setPipelineStates(materialObj->pipelineStates);
	renderer_->draw();
}

// 设置主渲染缓冲区（颜色缓冲和深度缓冲）
void Viewer::setupMainBuffers() {

	if (config_.aaType == AAType_MSAA) {
		setupMainColorBuffer(true);
		setupMainDepthBuffer(true);
	}
	else {
		setupMainColorBuffer(false);
		setupMainDepthBuffer(false);
	}

	if (!fboMain_) {
		fboMain_ = renderer_->createFrameBuffer(false); 
	}

	fboMain_->setColorAttachment(texColorMain_, 0); 
	fboMain_->setDepthAttachment(texDepthMain_); 
	fboMain_->setOffscreen(false); 

	if (!fboMain_->isValid()) {
		LOGE("setupMainBuffers failed");
	}
}

// 设置阴影贴图所需的帧缓冲和深度纹理
void Viewer::setupShadowMapBuffers() {
	if (!config_.shadowMap) {
		return;
	}

	if (!fboShadow_) {
		fboShadow_ = renderer_->createFrameBuffer(true);
	}

	if (!texDepthShadow_) {
		TextureDesc texDesc{};
		texDesc.width = SHADOW_MAP_WIDTH;
		texDesc.height = SHADOW_MAP_HEIGHT;
		texDesc.type = TextureType_2D;
		texDesc.format = TextureFormat_FLOAT32;
		texDesc.usage = TextureUsage_Sampler | TextureUsage_AttachmentDepth;
		texDesc.useMipmaps = false;
		texDesc.multiSample = false;
		texDepthShadow_ = renderer_->createTexture(texDesc);

		SamplerDesc sampler{};
		sampler.filterMin = Filter_NEAREST;
		sampler.filterMag = Filter_NEAREST;
		sampler.wrapS = Wrap_CLAMP_TO_BORDER;
		sampler.wrapT = Wrap_CLAMP_TO_BORDER;
		sampler.borderColor = config_.reverseZ ? Border_BLACK : Border_WHITE;
		texDepthShadow_->setSamplerDesc(sampler);

		texDepthShadow_->initImageData();
		fboShadow_->setDepthAttachment(texDepthShadow_);

		if (!fboShadow_->isValid()) {
			LOGE("setupShadowMapBuffers failed");
		}
	}
}

// 初始化主颜色缓冲区纹理，创建空纹理
void Viewer::setupMainColorBuffer(bool multiSample) {
	
	if (!texColorMain_ || texColorMain_->multiSample != multiSample) {
		TextureDesc texDesc{}; 
		texDesc.width = width_;             
		texDesc.height = height_;           
		texDesc.type = TextureType_2D;      
		texDesc.format = TextureFormat_RGBA8;
		texDesc.usage = TextureUsage_AttachmentColor | TextureUsage_RendererOutput;
		texDesc.useMipmaps = false;             
		texDesc.multiSample = multiSample;      

		texColorMain_ = renderer_->createTexture(texDesc);

		SamplerDesc sampler{};
		sampler.filterMin = Filter_LINEAR;
		sampler.filterMag = Filter_LINEAR;

		texColorMain_->setSamplerDesc(sampler);

		texColorMain_->initImageData();
	}
}

//设置主深度缓冲区的纹理对象，分配GPU资源
void Viewer::setupMainDepthBuffer(bool multiSample) {
	if (!texDepthMain_ || texDepthMain_->multiSample != multiSample) {
		TextureDesc texDesc{};
		texDesc.width = width_;
		texDesc.height = height_;
		texDesc.type = TextureType_2D;
		texDesc.format = TextureFormat_FLOAT32;
		texDesc.usage = TextureUsage_AttachmentDepth;
		texDesc.useMipmaps = false;
		texDesc.multiSample = multiSample;
		texDepthMain_ = renderer_->createTexture(texDesc);

		SamplerDesc sampler{};
		sampler.filterMin = Filter_NEAREST;
		sampler.filterMag = Filter_NEAREST;
		texDepthMain_->setSamplerDesc(sampler);

		texDepthMain_->initImageData();
	}
}

// 为模型纹理分配GPU资源，以及配置MaterialObj、渲染管线状态
void Viewer::setupMaterial(ModelBase& model, ShadingModel shading, const std::set<int>& uniformBlocks,
						   const std::function<void(RenderStates& rs)>& extraStates) {
	auto& material = *model.material;
	//-------------------------- 创建材质中纹理的gpu资源-------------------
	if (material.textures.empty()) {
		setupTextures(material);
		material.shaderDefines = generateShaderDefines(material);
	}
	// -----------------------模型中材质的shaderResources------------------
	if (!material.materialObj) {
		material.materialObj = std::make_shared<MaterialObject>();
		material.materialObj->shadingModel = shading;

		// setup uniform samplers
		if (setupShaderProgram(material, shading)) {
			setupSamplerUniforms(material);
		}

		// setup uniform blocks
		for (auto& key : uniformBlocks) {
			std::shared_ptr<UniformBlock> uniform = nullptr;
			switch (key) {
			case UniformBlock_Scene: {
				uniform = uniformBlockScene_;
				break;
			}
			case UniformBlock_Model: {
				uniform = uniformBlockModel_;
				break;
			}
			case UniformBlock_Material: {
				uniform = uniformBlockMaterial_;
				break;
			}
			default:
				break;
			}
			if (uniform) {
				material.materialObj->shaderResources->blocks[key] = uniform;
			}
		}
	}
	// -------------------配置模型中的渲染管线状态--------------
	setupPipelineStates(model, extraStates);
}

// 创建vao
void Viewer::setupVertexArray(ModelVertexes& vertexes) {
	if (!vertexes.vao) {
		vertexes.vao = renderer_->createVertexArrayObject(vertexes);
	}
}

// 遍历材质的所有纹理数据，创建并配置对应的GPU资源
void Viewer::setupTextures(Material& material) {
	for (auto& kv : material.textureData) {
		TextureDesc texDesc{};
		texDesc.width = (int)kv.second.width;
		texDesc.height = (int)kv.second.height;
		texDesc.format = TextureFormat_RGBA8;
		texDesc.usage = TextureUsage_Sampler | TextureUsage_UploadData;
		texDesc.useMipmaps = false;
		texDesc.multiSample = false;

		SamplerDesc sampler{};
		sampler.wrapS = kv.second.wrapModeU;
		sampler.wrapT = kv.second.wrapModeV;
		sampler.filterMin = Filter_LINEAR;
		sampler.filterMag = Filter_LINEAR;

		std::shared_ptr<Texture> texture = nullptr;
		switch (kv.first) {
		case MaterialTexType_IBL_IRRADIANCE:
		case MaterialTexType_IBL_PREFILTER: {
			// 跳过ibl纹理（后面统一处理）
			break;
		}
		case MaterialTexType_CUBE: {
			texDesc.type = TextureType_CUBE;
			sampler.wrapR = kv.second.wrapModeW;
			break;
		}
		default: {
			texDesc.type = TextureType_2D;
			texDesc.useMipmaps = config_.mipmaps;
			sampler.filterMin = config_.mipmaps ? Filter_LINEAR_MIPMAP_LINEAR : Filter_LINEAR;
			break;
		}
		}
		texture = renderer_->createTexture(texDesc);
		texture->setSamplerDesc(sampler);
		texture->setImageData(kv.second.data);
		texture->tag = kv.second.tag;
		material.textures[kv.first] = texture;
	}

	// 设置默认阴影贴图
	if (material.shadingModel != Shading_Skybox) {
		material.textures[MaterialTexType_SHADOWMAP] = shadowPlaceholder_;
	}

	// 设置默认ibl纹理
	if (material.shadingModel == Shading_PBR) {
		material.textures[MaterialTexType_IBL_IRRADIANCE] = iblPlaceholder_;
		material.textures[MaterialTexType_IBL_PREFILTER] = iblPlaceholder_;
	}
}

// 根据材质中的纹理创建对应的uniformsampler并存储到materialobj->shaderResources中
void Viewer::setupSamplerUniforms(Material& material) {
	for (auto& kv : material.textures) {
		// 创建sampler uniform
		const char* samplerName = Material::samplerName((MaterialTexType)kv.first);
		if (samplerName) {
			auto uniform = renderer_->createUniformSampler(samplerName, *kv.second);
			uniform->setTexture(kv.second);
			material.materialObj->shaderResources->samplers[kv.first] = std::move(uniform);
		}
	}
}

/*设置材质的着色器程序，根据着色模型和材质定义的宏，创建或复用对应的着色器程序*/
bool Viewer::setupShaderProgram(Material& material, ShadingModel shading) {
	size_t cacheKey = getShaderProgramCacheKey(shading, material.shaderDefines);

	// -----------------尝试从cache中读取--------------------------------
	auto cachedProgram = programCache_.find(cacheKey);
	if (cachedProgram != programCache_.end()) {
		material.materialObj->shaderProgram = cachedProgram->second;
		material.materialObj->shaderResources = std::make_shared<ShaderResources>();
		return true;
	}
	// --------------------缓存未命中：创建新的着色器程序-------------------------
	auto program = renderer_->createShaderProgram();
	program->addDefines(material.shaderDefines);

	bool success = loadShaders(*program, shading);//
	if (success) {
		// 添加至cache
		programCache_[cacheKey] = program;
		material.materialObj->shaderProgram = program;
		material.materialObj->shaderResources = std::make_shared<ShaderResources>();
	}
	else {
		LOGE("setupShaderProgram failed: %s", Material::shadingModelStr(shading));
	}

	return success;
}

// 设置模型的渲染管线状态
void Viewer::setupPipelineStates(ModelBase& model, const std::function<void(RenderStates& rs)>& extraStates) {
	auto& material = *model.material;
	RenderStates rs;
	rs.blend = material.alphaMode == Alpha_Blend;
	rs.blendParams.SetBlendFactor(BlendFactor_SRC_ALPHA, BlendFactor_ONE_MINUS_SRC_ALPHA);

	rs.depthTest = config_.depthTest;
	rs.depthMask = !rs.blend;   // disable depth write if blending enabled
	rs.depthFunc = config_.reverseZ ? DepthFunc_GREATER : DepthFunc_LESS;

	rs.cullFace = config_.cullFace && (!material.doubleSided);
	rs.primitiveType = model.primitiveType;
	rs.polygonMode = PolygonMode_FILL;

	rs.lineWidth = material.lineWidth;

	if (extraStates) {
		extraStates(rs);
	}

	size_t cacheKey = getPipelineCacheKey(material, rs);
	auto it = pipelineCache_.find(cacheKey);
	if (it != pipelineCache_.end()) {
		material.materialObj->pipelineStates = it->second;
	}
	else {
		auto pipelineStates = renderer_->createPipelineStates(rs);
		material.materialObj->pipelineStates = pipelineStates;
		pipelineCache_[cacheKey] = pipelineStates;
	}
}

/*更新场景级别的统一变量(Uniform)数据， 将全局光照参数、摄像机位置等场景级数据上传到GPU着色器*/
void Viewer::updateUniformScene() {
	static UniformsScene uniformsScene{};

	uniformsScene.u_ambientColor = config_.ambientColor;
	uniformsScene.u_cameraPosition = camera_->eye();
	uniformsScene.u_pointLightPosition = config_.pointLightPosition;
	uniformsScene.u_pointLightColor = config_.pointLightColor;

	uniformBlockScene_->setData(&uniformsScene, sizeof(UniformsScene));
}

/*更新模型变换相关的统一变量(Uniform)数据，计算并上传模型矩阵、MVP矩阵及其衍生矩阵到GPU着色器*/
void Viewer::updateUniformModel(const glm::mat4& model, const glm::mat4& view) {
	static UniformsModel uniformsModel{}; //变换矩阵

	uniformsModel.u_reverseZ = config_.reverseZ ? 1u : 0u;
	uniformsModel.u_modelMatrix = model;
	uniformsModel.u_modelViewProjectionMatrix = camera_->projectionMatrix() * view * model;
	uniformsModel.u_inverseTransposeModelMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

	// shadow mvp
	if (config_.shadowMap && cameraDepth_) {
		//等价于shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5，将坐标从NDC空间[-1,1]转换到纹理空间[0,1]
		const glm::mat4 biasMat = glm::mat4(0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
		uniformsModel.u_shadowMVPMatrix = biasMat * cameraDepth_->projectionMatrix() * cameraDepth_->viewMatrix() * model;
	}

	uniformBlockModel_->setData(&uniformsModel, sizeof(UniformsModel));
}

/*更新材质相关的统一变量(Uniform)数据，将材质属性和渲染配置打包发送到GPU着色器*/
void Viewer::updateUniformMaterial(Material& material, float specular) {
	static UniformsMaterial uniformsMaterial{};

	uniformsMaterial.u_enableLight = config_.showLight ? 1u : 0u;
	uniformsMaterial.u_enableIBL = iBLEnabled() ? 1u : 0u;
	uniformsMaterial.u_enableShadow = config_.shadowMap ? 1u : 0u;

	uniformsMaterial.u_pointSize = material.pointSize;
	uniformsMaterial.u_kSpecular = specular;
	uniformsMaterial.u_baseColor = material.baseColor;

	uniformBlockMaterial_->setData(&uniformsMaterial, sizeof(UniformsMaterial));// 上传到GPU
}

/*初始化ibl资源，将等距柱状图转化为立方体贴图，生成irradianceMap、prefilterMap*/
bool Viewer::initSkyboxIBL() {
	if (!(config_.showSkybox && config_.pbrIbl)) {//检查配置是否启用天空盒和pbribl
		return false;
	}

	if (!iblGenerator_) {//确保ibl生成器已创建
		iblGenerator_ = std::make_shared<IBLGenerator>(renderer_);
	}

	if (getSkyboxMaterial()->iblReady) {// 检查是否已经初始化过ibl
		return true;
	}

	auto& skybox = scene_->skybox;
	if (skybox.material->textures.empty()) {// 为材质中纹理分配GPU资源
		setupTextures(*skybox.material);
	}

	std::shared_ptr<Texture> textureCube = nullptr;

	// 等距柱状图转立方体贴图处理
	auto texCubeIt = skybox.material->textures.find(MaterialTexType_CUBE);
	if (texCubeIt == skybox.material->textures.end()) {
		// 查找等距柱状图纹理
		auto texEqIt = skybox.material->textures.find(MaterialTexType_EQUIRECTANGULAR);
		if (texEqIt != skybox.material->textures.end()) {
			// 创建目标立方体贴图
			auto tex2d = std::dynamic_pointer_cast<Texture>(texEqIt->second);
			auto cubeSize = std::min(tex2d->width, tex2d->height);
			auto texCvt = createTextureCubeDefault(cubeSize, cubeSize, TextureUsage_AttachmentColor | TextureUsage_Sampler);
			// 执行转换
			auto success = iblGenerator_->convertEquirectangular([&](ShaderProgram& program) -> bool {
															 	 return loadShaders(program, Shading_Skybox);}, 
																 tex2d, texCvt);

			LOGD("convert equirectangular to cube map: %s.", success ? "success" : "failed");
			if (success) {
				textureCube = texCvt;
				skybox.material->textures[MaterialTexType_CUBE] = texCvt;

				// 更新天空盒材质
				renderer_->waitIdle();
				skybox.material->textures.erase(MaterialTexType_EQUIRECTANGULAR);
				skybox.material->shaderDefines = generateShaderDefines(*skybox.material);
				skybox.material->materialObj = nullptr;
			}
		}
	}
	else {
		textureCube = std::dynamic_pointer_cast<Texture>(texCubeIt->second);
	}

	if (!textureCube) {//检查立方体贴图是否有效
		LOGE("initSkyboxIBL failed: skybox texture cube not available");
		return false;
	}

	// 生成irradiance map
	LOGD("generate ibl irradiance map ...");
	auto texIrradiance = createTextureCubeDefault(kIrradianceMapSize, kIrradianceMapSize, TextureUsage_AttachmentColor | TextureUsage_Sampler);
	if (iblGenerator_->generateIrradianceMap([&](ShaderProgram& program) -> bool {return loadShaders(program, Shading_IBL_Irradiance);},
											 textureCube, texIrradiance)) {
		skybox.material->textures[MaterialTexType_IBL_IRRADIANCE] = std::move(texIrradiance);
	}
	else {
		LOGE("initSkyboxIBL failed: generate irradiance map failed");
		return false;
	}
	LOGD("generate ibl irradiance map done.");

	// 生成prefilter map
	LOGD("generate ibl prefilter map ...");
	auto texPrefilter = createTextureCubeDefault(kPrefilterMapSize, kPrefilterMapSize, TextureUsage_AttachmentColor | TextureUsage_Sampler, true);
	if (iblGenerator_->generatePrefilterMap([&](ShaderProgram& program) -> bool {return loadShaders(program, Shading_IBL_Prefilter);},
											textureCube, texPrefilter)) {
		skybox.material->textures[MaterialTexType_IBL_PREFILTER] = std::move(texPrefilter);
	}
	else {
		LOGE("initSkyboxIBL failed: generate prefilter map failed");
		return false;
	}
	LOGD("generate ibl prefilter map done.");

	//清理和状态更新
	renderer_->waitIdle();// 确保所有GPU操作完成
	iblGenerator_->clearCaches();// 清理生成器缓存
	getSkyboxMaterial()->iblReady = true;// 标记IBL已就绪
	return true;
}

bool Viewer::iBLEnabled() {
	return config_.showSkybox && config_.pbrIbl && getSkyboxMaterial()->iblReady;
}

SkyboxMaterial* Viewer::getSkyboxMaterial() {
	return dynamic_cast<SkyboxMaterial*>(scene_->skybox.material.get());
}

/*更新材质中的IBL纹理绑定, 根据IBL是否启用，动态切换辐照度贴图和预过滤贴图的绑定目标*/
void Viewer::updateIBLTextures(MaterialObject* materialObj) {
	if (!materialObj->shaderResources) {
		return;
	}

	auto& samplers = materialObj->shaderResources->samplers;
	if (iBLEnabled()) {
		auto& skyboxTextures = scene_->skybox.material->textures;
		samplers[MaterialTexType_IBL_IRRADIANCE]->setTexture(skyboxTextures[MaterialTexType_IBL_IRRADIANCE]);
		samplers[MaterialTexType_IBL_PREFILTER]->setTexture(skyboxTextures[MaterialTexType_IBL_PREFILTER]);
	}
	else {
		samplers[MaterialTexType_IBL_IRRADIANCE]->setTexture(iblPlaceholder_);
		samplers[MaterialTexType_IBL_PREFILTER]->setTexture(iblPlaceholder_);
	}
}

/*更新材质中的阴影贴图绑定状态,根据当前是否处于阴影渲染阶段，动态切换阴影贴图的绑定目标*/
void Viewer::updateShadowTextures(MaterialObject* materialObj, bool shadowPass) {
	if (!materialObj->shaderResources) {
		return;
	}

	auto& samplers = materialObj->shaderResources->samplers;
	if (shadowPass) {
		samplers[MaterialTexType_SHADOWMAP]->setTexture(shadowPlaceholder_);
	}
	else {
		samplers[MaterialTexType_SHADOWMAP]->setTexture(texDepthShadow_);
	}
}

// 为纹理分配GPU内存
std::shared_ptr<Texture> Viewer::createTexture2DDefault(int width, int height, TextureFormat format, uint32_t usage, bool mipmaps) {
	TextureDesc texDesc{};
	texDesc.width = width;
	texDesc.height = height;
	texDesc.type = TextureType_2D;
	texDesc.format = format;
	texDesc.usage = usage;
	texDesc.useMipmaps = mipmaps;
	texDesc.multiSample = false;
	auto texture2d = renderer_->createTexture(texDesc);
	if (!texture2d) {
		return nullptr;
	}

	SamplerDesc sampler{};
	sampler.filterMin = mipmaps ? Filter_LINEAR_MIPMAP_LINEAR : Filter_LINEAR;
	sampler.filterMag = Filter_LINEAR;
	texture2d->setSamplerDesc(sampler);

	texture2d->initImageData();
	return texture2d;
}

std::shared_ptr<Texture> Viewer::createTextureCubeDefault(int width, int height, uint32_t usage, bool mipmaps) {
	TextureDesc texDesc{};
	texDesc.width = width;
	texDesc.height = height;
	texDesc.type = TextureType_CUBE;
	texDesc.format = TextureFormat_RGBA8;
	texDesc.usage = usage;
	texDesc.useMipmaps = mipmaps;
	texDesc.multiSample = false;
	auto textureCube = renderer_->createTexture(texDesc);
	if (!textureCube) {
		return nullptr;
	}

	SamplerDesc sampler{};
	sampler.filterMin = mipmaps ? Filter_LINEAR_MIPMAP_LINEAR : Filter_LINEAR;
	sampler.filterMag = Filter_LINEAR;
	textureCube->setSamplerDesc(sampler);

	textureCube->initImageData();
	return textureCube;
}

/*生成材质对应的着色器宏定义集合
 根据材质中实际使用的纹理类型，自动生成对应的预处理器宏定义
 这些宏定义将用于控制着色器的编译分支（如是否启用法线贴图、PBR贴图等）*/
std::set<std::string> Viewer::generateShaderDefines(Material& material) {
	std::set<std::string> shaderDefines;
	for (auto& kv : material.textures) {
		const char* samplerDefine = Material::samplerDefine((MaterialTexType)kv.first);
		if (samplerDefine) {
			shaderDefines.insert(samplerDefine);
		}
	}
	return shaderDefines;
}

// 通过哈希组合材质和渲染状态的关键参数，生成唯一标识符
size_t Viewer::getPipelineCacheKey(Material& material, const RenderStates& rs) {
	size_t seed = 0;

	HashUtils::hashCombine(seed, (int)material.materialObj->shadingModel);

	// TODO pack together
	HashUtils::hashCombine(seed, rs.blend);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendFuncRgb);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendSrcRgb);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendDstRgb);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendFuncAlpha);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendSrcAlpha);
	HashUtils::hashCombine(seed, (int)rs.blendParams.blendDstAlpha);

	HashUtils::hashCombine(seed, rs.depthTest);
	HashUtils::hashCombine(seed, rs.depthMask);
	HashUtils::hashCombine(seed, (int)rs.depthFunc);

	HashUtils::hashCombine(seed, rs.cullFace);
	HashUtils::hashCombine(seed, (int)rs.primitiveType);
	HashUtils::hashCombine(seed, (int)rs.polygonMode);

	HashUtils::hashCombine(seed, rs.lineWidth);

	return seed;
}

//通过混合着色模型类型和预处理器宏定义集合，生成一个唯一哈希值作为缓存键
size_t Viewer::getShaderProgramCacheKey(ShadingModel shading, const std::set<std::string>& defines) {
	size_t seed = 0;
	HashUtils::hashCombine(seed, (int)shading);
	for (auto& def : defines) {
		HashUtils::hashCombine(seed, def);
	}
	return seed;
}

// 判断mesh所在box是否与相机视锥体相交
bool Viewer::checkMeshFrustumCull(ModelMesh& mesh, const glm::mat4& transform) {
	BoundingBox bbox = mesh.aabb.transform(transform);
	return camera_->getFrustum().intersects(bbox);
}

}