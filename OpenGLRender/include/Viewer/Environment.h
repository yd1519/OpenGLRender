#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <functional>
#include "Viewer/Camera.h"
#include "Viewer/Model.h"
#include "Render/FrameBuffer.h"
#include "Render/Renderer.h"

namespace OpenGL {

constexpr int kIrradianceMapSize = 32;
constexpr int kPrefilterMaxMipLevels = 5;
constexpr int kPrefilterMapSize = 128;

//包含渲染立方体贴图所需的所有资源
struct CubeRenderContext {
    std::shared_ptr<FrameBuffer> fbo;
    Camera camera;
    ModelMesh modelSkybox;
    std::shared_ptr<UniformBlock> uniformsBlockModel;
};

class IBLGenerator {
public:
    explicit IBLGenerator(const std::shared_ptr<Renderer>& renderer) : renderer_(renderer) {};
    inline void clearCaches() { contextCache_.clear(); }

    // 将等距柱状投影贴图转化为立方体贴图
    bool convertEquirectangular(const std::function<bool(ShaderProgram& program)>& shaderFunc,
                                const std::shared_ptr<Texture>& texIn,
                                std::shared_ptr<Texture>& texOut);

    bool generateIrradianceMap(const std::function<bool(ShaderProgram& program)>& shaderFunc,
                               const std::shared_ptr<Texture>& texIn,
                               std::shared_ptr<Texture>& texOut);

    bool generatePrefilterMap(const std::function<bool(ShaderProgram& program)>& shaderFunc,
                              const std::shared_ptr<Texture>& texIn,
                              std::shared_ptr<Texture>& texOut);

private:
    bool createCubeRenderContext(CubeRenderContext& context,
                                 const std::function<bool(ShaderProgram& program)>& shaderFunc,
                                 const std::shared_ptr<Texture>& texIn,
                                 MaterialTexType texType);

    void drawCubeFaces(CubeRenderContext& context, uint32_t width, uint32_t height, std::shared_ptr<Texture>& texOut,
                       uint32_t texOutLevel = 0, const std::function<void()>& beforeDraw = nullptr);

    bool loadFromCache(std::shared_ptr<Texture>& tex);
    void storeToCache(std::shared_ptr<Texture>& tex);

    static std::string getTextureHashKey(std::shared_ptr<Texture>& tex);
    static std::string getCacheFilePath(const std::string& hashKey);

private:
    std::shared_ptr<Renderer> renderer_;
    std::vector<std::shared_ptr<CubeRenderContext>> contextCache_;// 渲染上下文缓存
};
}


#endif
