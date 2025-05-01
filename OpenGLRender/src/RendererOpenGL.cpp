#include "Render/OpenGL/RendererOpenGL.h"
#include "Render/OpenGL/FrameBufferOpenGL.h"
#include "Render/OpenGL/TextureOpenGL.h"
#include "Render/OpenGL/UniformOpenGL.h"
#include "Render/PipelineStates.h"
#include "Render/OpenGL/OpenGLUtils.h"
#include <memory>

//根据布尔变量值自动启用或禁用对应的 OpenGL 功能状态
#define GL_STATE_SET(var, gl_state) if (var) GL_CHECK(glEnable(gl_state)); else GL_CHECK(glDisable(gl_state));


namespace OpenGL {

std::shared_ptr<FrameBuffer> RendererOpenGL::createFrameBuffer(bool offscreen) {
	return std::make_shared<FrameBufferOpenGL>(offscreen);
}

std::shared_ptr<Texture> RendererOpenGL::createTexture(const TextureDesc& desc) {
    switch (desc.type) {
    case TextureType_2D:    return std::make_shared<Texture2DOpenGL>(desc);
    case TextureType_CUBE:  return std::make_shared<TextureCubeOpenGL>(desc);
    }
    return nullptr;
}

std::shared_ptr<VertexArrayObject> RendererOpenGL::createVertexArrayObject(const VertexArray& vertexArray) {
    return std::make_shared<VertexArrayObjectOpenGL>(vertexArray);
}

std::shared_ptr<ShaderProgram> RendererOpenGL::createShaderProgram() {
    return std::make_shared<ShaderProgramOpenGL>();
}

std::shared_ptr<PipelineStates> RendererOpenGL::createPipelineStates(const RenderStates& renderStates) {
    return std::make_shared<PipelineStates>(renderStates);
}

std::shared_ptr<UniformBlock> RendererOpenGL::createUniformBlock(const std::string& name, int size) {
    return std::make_shared<UniformBlockOpenGL>(name, size);
}

std::shared_ptr<UniformSampler> RendererOpenGL::createUniformSampler(const std::string& name, const TextureDesc& desc) {
    return std::make_shared<UniformSamplerOpenGL>(name, desc.type, desc.format);
}

// 绑定帧缓冲以及清理color或depth
void RendererOpenGL::beginRenderPass(std::shared_ptr<FrameBuffer>& frameBuffer, const ClearStates& states) {
    auto* fbo = dynamic_cast<FrameBufferOpenGL*>(frameBuffer.get());
    fbo->bind();//绑定fboShadow_中fbo_。

    GLbitfield clearBit = 0;
    if (states.colorFlag) {
        GL_CHECK(glClearColor(states.clearColor.r, states.clearColor.g, states.clearColor.b, states.clearColor.a));
        clearBit |= GL_COLOR_BUFFER_BIT;
    }
    if (states.depthFlag) {
        clearBit |= GL_DEPTH_BUFFER_BIT;
    }
    GL_CHECK(glClear(clearBit));
}

void RendererOpenGL::setViewPort(int x, int y, int width, int height) {
    GL_CHECK(glViewport(x, y, width, height));
}

// 绑定vao
void RendererOpenGL::setVertexArrayObject(std::shared_ptr<VertexArrayObject>& vao) {
    if (!vao) {
        return;
    }
    vao_ = dynamic_cast<VertexArrayObjectOpenGL*>(vao.get());
    vao_->bind();
}

// 启用着色器
void RendererOpenGL::setShaderProgram(std::shared_ptr<ShaderProgram>& program) {
    if (!program) {
        return;
    }
    shaderProgram_ = dynamic_cast<ShaderProgramOpenGL*>(program.get());
    shaderProgram_->use();
}

// 将所给的uniformblock和sampler全部绑定
void RendererOpenGL::setShaderResources(std::shared_ptr<ShaderResources>& resources) {
    if (!resources) {
        return;
    }
    if (shaderProgram_) {
        shaderProgram_->bindResources(*resources);
    }
}

void RendererOpenGL::setPipelineStates(std::shared_ptr<PipelineStates>& states) {
    if (!states) {
        return;
    }
    pipelineStates_ = states.get();

    auto& renderStates = states->renderStates;
    // -----------------------------混合状态设置------------------------------------------
    GL_STATE_SET(renderStates.blend, GL_BLEND)
    // 设置混合方程式（分别指定RGB和Alpha通道的计算方式）
    GL_CHECK(glBlendEquationSeparate(OpenGL::cvtBlendFunction(renderStates.blendParams.blendFuncRgb),
                                     OpenGL::cvtBlendFunction(renderStates.blendParams.blendFuncAlpha)));
    // 设置混合因子（分别指定源和目标颜色的混合系数）
    GL_CHECK(glBlendFuncSeparate(OpenGL::cvtBlendFactor(renderStates.blendParams.blendSrcRgb),
                                 OpenGL::cvtBlendFactor(renderStates.blendParams.blendDstRgb),
                                 OpenGL::cvtBlendFactor(renderStates.blendParams.blendSrcAlpha),
                                 OpenGL::cvtBlendFactor(renderStates.blendParams.blendDstAlpha)));

    //----------------------------------深度状态设置----------------------------------------
    GL_STATE_SET(renderStates.depthTest, GL_DEPTH_TEST)
    GL_CHECK(glDepthMask(renderStates.depthMask));
    GL_CHECK(glDepthFunc(OpenGL::cvtDepthFunc(renderStates.depthFunc)));

    //--------------------------------------其他渲染状态设置----------------------
    GL_STATE_SET(renderStates.cullFace, GL_CULL_FACE)
    // 设置多边形填充模式（点/线框/填充）
    GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, OpenGL::cvtPolygonMode(renderStates.polygonMode)));
    //// 设置线宽
    GL_CHECK(glLineWidth(renderStates.lineWidth));
    GL_CHECK(glEnable(GL_PROGRAM_POINT_SIZE));
}

void RendererOpenGL::draw() {
    GLenum mode = OpenGL::cvtDrawMode(pipelineStates_->renderStates.primitiveType);
    GL_CHECK(glDrawElements(mode, (GLsizei)vao_->getIndicesCnt(), GL_UNSIGNED_INT, nullptr));
}

// 结束当前渲染通道，重置OpenGL状态到默认值
void RendererOpenGL::endRenderPass() {
    GL_CHECK(glDisable(GL_BLEND));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glDepthMask(true));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
}

//通过glFinish()阻塞当前CPU线程，直到GPU执行完所有已提交的命令
void RendererOpenGL::waitIdle() {
    GL_CHECK(glFinish());// 强制刷新命令缓冲区并等待
}





}