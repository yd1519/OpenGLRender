#ifndef VIEWER_H
#define VIEWER_H

#include "Viewer/Model.h"
#include "Viewer/Camera.h"
#include "Viewer/Config.h"
#include "Render/Renderer.h"
#include "Viewer/QuadFilter.h"
#include "Viewer/Environment.h"
#include <functional>
namespace OpenGL {

class Viewer {
public:
    Viewer(Config& config, Camera& camera) : config_(config), cameraMain_(camera) {}

    virtual bool create(int width, int height, int outTexId);
    virtual void destroy();

    virtual void configRenderer() {};
    virtual void drawFrame(DemoScene& scene);
    virtual int swapBuffer() = 0;

    void waitRenderIdle();
    void resetReverseZ();

    // used by RenderDoc to capture frames
    virtual void* getDevicePointer(void* window) { return nullptr; }

protected:
    virtual std::shared_ptr<Renderer> createRenderer() = 0;
    virtual bool loadShaders(ShaderProgram& program, ShadingModel shading) = 0;

private:
    void cleanup();

    void drawShadowMap();

    void processFXAASetup();
    void processFXAADraw();

    void setupScene();
    void setupPoints(ModelPoints& points);
    void setupLines(ModelLines& lines);
    void setupMeshBaseColor(ModelMesh& mesh, bool wireframe);
    void setupMeshTextured(ModelMesh& mesh);
    void setupModelNodes(ModelNode& node, bool wireframe);
    void setupSkybox(ModelMesh& skybox);

    void drawScene(bool shadowPass);
    void drawModelNodes(ModelNode& node, bool shadowPass, glm::mat4& transform, AlphaMode mode, float specular = 1.f);
    void drawModelMesh(ModelMesh& mesh, bool shadowPass, float specular);

    void pipelineSetup(ModelBase& model, ShadingModel shading, const std::set<int>& uniformBlocks,
    const std::function<void(RenderStates& rs)>& extraStates = nullptr);
    void pipelineDraw(ModelBase& model);

    void setupMainBuffers();
    void setupShadowMapBuffers();
    void setupMainColorBuffer(bool multiSample);
    void setupMainDepthBuffer(bool multiSample);

    void setupVertexArray(ModelVertexes& vertexes);
    void setupTextures(Material& material);
    bool setupShaderProgram(Material& material, ShadingModel shading);
    void setupSamplerUniforms(Material& material);
    void setupPipelineStates(ModelBase& model, const std::function<void(RenderStates& rs)>& extraStates);
    void setupMaterial(ModelBase& model, ShadingModel shading, const std::set<int>& uniformBlocks,
    const std::function<void(RenderStates& rs)>& extraStates);

    void updateUniformScene();
    void updateUniformModel(const glm::mat4& model, const glm::mat4& view);
    void updateUniformMaterial(Material& material, float specular = 1.f);

    inline SkyboxMaterial* getSkyboxMaterial();
    bool initSkyboxIBL();
    bool iBLEnabled();
    void updateIBLTextures(MaterialObject* materialObj);
    void updateShadowTextures(MaterialObject* materialObj, bool shadowPass);

    static std::set<std::string> generateShaderDefines(Material& material);
    static size_t getShaderProgramCacheKey(ShadingModel shading, const std::set<std::string>& defines);
    static size_t getPipelineCacheKey(Material& material, const RenderStates& rs);

    std::shared_ptr<Texture> createTextureCubeDefault(int width, int height, uint32_t usage, bool mipmaps = false);
    std::shared_ptr<Texture> createTexture2DDefault(int width, int height, TextureFormat format, uint32_t usage, bool mipmaps = false);
    bool checkMeshFrustumCull(ModelMesh& mesh, const glm::mat4& transform);

protected:
    Config& config_;

    Camera& cameraMain_;
    std::shared_ptr<Camera> cameraDepth_ = nullptr;
    Camera* camera_ = nullptr;

    DemoScene* scene_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int outTexId_ = 0;

    std::shared_ptr<Renderer> renderer_ = nullptr;

    // main fbo
    std::shared_ptr<FrameBuffer> fboMain_ = nullptr;
    std::shared_ptr<Texture> texColorMain_ = nullptr;
    std::shared_ptr<Texture> texDepthMain_ = nullptr;

    // shadow map
    std::shared_ptr<FrameBuffer> fboShadow_ = nullptr;
    std::shared_ptr<Texture> texDepthShadow_ = nullptr; //存储 ​​实时生成的阴影贴图
    /*当场景没有有效阴影贴图时（如阴影未启用或初始化失败）
        在 setupTextures() 中赋给材质的 MaterialTexType_SHADOWMAP 槽位*/
    std::shared_ptr<Texture> shadowPlaceholder_ = nullptr;

    // fxaa
    std::shared_ptr<QuadFilter> fxaaFilter_ = nullptr;
    std::shared_ptr<Texture> texColorFxaa_ = nullptr;

    // ibl
    std::shared_ptr<Texture> iblPlaceholder_ = nullptr;
    std::shared_ptr<IBLGenerator> iblGenerator_ = nullptr;

    // uniforms，会在setupmaterial（）被赋值到model中, 
    std::shared_ptr<UniformBlock> uniformBlockScene_;
    std::shared_ptr<UniformBlock> uniformBlockModel_;
    std::shared_ptr<UniformBlock> uniformBlockMaterial_;

    // caches ...........key: hash值
    std::unordered_map<size_t, std::shared_ptr<ShaderProgram>> programCache_;
    std::unordered_map<size_t, std::shared_ptr<PipelineStates>> pipelineCache_;
};
}

#endif
