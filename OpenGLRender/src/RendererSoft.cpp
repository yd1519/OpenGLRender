#include "Render/Software/RendererSoft.h"
#include "Base/SIMD.h"
#include "Base/HashUtils.h"
#include "Render/Software/FramebufferSoft.h"
#include "Render/Software/TextureSoft.h"
#include "Render/Software/UniformSoft.h"
#include "Render/Software/ShaderProgramSoft.h"
#include "Render/Software/VertexSoft.h"
#include "Render/Software/BlendSoft.h"
#include "Render/Software/DepthSoft.h"
#include "Base/GLMInc.h"

namespace OpenGL {

#define RASTER_MULTI_THREAD

// framebuffer
std::shared_ptr<FrameBuffer> RendererSoft::createFrameBuffer(bool offscreen) {
    return std::make_shared<FrameBufferSoft>(offscreen);
}

// texture
std::shared_ptr<Texture> RendererSoft::createTexture(const TextureDesc& desc) {
    switch (desc.format) {
    case TextureFormat_RGBA8:   return std::make_shared<TextureSoft<RGBA>>(desc);
    case TextureFormat_FLOAT32: return std::make_shared<TextureSoft<float>>(desc);
    }
    return nullptr;
}

// vertex
std::shared_ptr<VertexArrayObject> RendererSoft::createVertexArrayObject(const VertexArray& vertexArray) {
    return std::make_shared<VertexArrayObjectSoft>(vertexArray);
}

// shader program
std::shared_ptr<ShaderProgram> RendererSoft::createShaderProgram() {
    return std::make_shared<ShaderProgramSoft>();
}

// pipeline states
std::shared_ptr<PipelineStates> RendererSoft::createPipelineStates(const RenderStates& renderStates) {
    return std::make_shared<PipelineStates>(renderStates);
}

// uniform
std::shared_ptr<UniformBlock> RendererSoft::createUniformBlock(const std::string& name, int size) {
    return std::make_shared<UniformBlockSoft>(name, size);
}

std::shared_ptr<UniformSampler> RendererSoft::createUniformSampler(const std::string& name, const TextureDesc& desc) {
    return std::make_shared<UniformSamplerSoft>(name, desc.type, desc.format);
}

// 配置并清理颜色和深度缓冲
void RendererSoft::beginRenderPass(std::shared_ptr<FrameBuffer>& frameBuffer, const ClearStates& states) {
    fbo_ = dynamic_cast<FrameBufferSoft*>(frameBuffer.get());

    if (!fbo_) {
        return;
    }

    fboColor_ = fbo_->getColorBuffer();
    fboDepth_ = fbo_->getDepthBuffer();

    //清理颜色缓冲
    if (states.colorFlag && fboColor_) {
        RGBA color = RGBA(states.clearColor.r * 255,
                          states.clearColor.g * 255,
                          states.clearColor.b * 255,
                          states.clearColor.a * 255);
        if (fboColor_->multiSample) {
            fboColor_->bufferMs4x->setAll(glm::tvec4<RGBA>(color));
        }
        else {
            fboColor_->buffer->setAll(color);
        }
    }

    //清理深度缓冲
    if (states.depthFlag && fboDepth_) {
        if (fboDepth_->multiSample) {
            fboDepth_->bufferMs4x->setAll(glm::tvec4<float>(states.clearDepth));
        }
        else {
            fboDepth_->buffer->setAll(states.clearDepth);
        }
    }
}

void RendererSoft::setViewPort(int x, int y, int width, int height) {
    //基础参数设置
    viewport_.x = (float)x;
    viewport_.y = (float)y;
    viewport_.width = (float)width;
    viewport_.height = (float)height;

    //深度范围配置
    viewport_.minDepth = 0.f;
    viewport_.maxDepth = 1.f;

    //计算绝对深度范围
    viewport_.absMinDepth = std::min(viewport_.minDepth, viewport_.maxDepth);
    viewport_.absMaxDepth = std::max(viewport_.minDepth, viewport_.maxDepth);

    //---------------------预计算视口变换参数--------------------------------

    // 视口中心点（用于后续计算）
    viewport_.innerO.x = viewport_.x + viewport_.width / 2.f;
    viewport_.innerO.y = viewport_.y + viewport_.height / 2.f;
    viewport_.innerO.z = viewport_.minDepth;
    viewport_.innerO.w = 0.f;

    // 视口缩放系数（预除以2加速后续计算）
    viewport_.innerP.x = viewport_.width / 2.f;    
    viewport_.innerP.y = viewport_.height / 2.f;   
    viewport_.innerP.z = viewport_.maxDepth - viewport_.minDepth;
    viewport_.innerP.w = 1.f;
}

void RendererSoft::setVertexArrayObject(std::shared_ptr<VertexArrayObject>& vao) {
    vao_ = dynamic_cast<VertexArrayObjectSoft*>(vao.get());
}

void RendererSoft::setShaderProgram(std::shared_ptr<ShaderProgram>& program) {
    shaderProgram_ = dynamic_cast<ShaderProgramSoft*>(program.get());
}

void RendererSoft::setShaderResources(std::shared_ptr<ShaderResources>& resources) {
    if (!resources) {
        return;
    }
    if (shaderProgram_) {
        shaderProgram_->bindResources(*resources);
    }
}

void RendererSoft::setPipelineStates(std::shared_ptr<PipelineStates>& states) {
    renderState_ = &states->renderStates;
}

// 主渲染函数，执行完整图形管线
void RendererSoft::draw() {
    if (!fbo_ || !vao_ || !shaderProgram_) {
        return;
    }

    fboColor_ = fbo_->getColorBuffer();
    fboDepth_ = fbo_->getDepthBuffer();
    primitiveType_ = renderState_->primitiveType;

    if (fboColor_) {// 优先使用颜色缓冲的采样数
        rasterSamples_ = fboColor_->sampleCnt;
    }
    else if (fboDepth_) {
        rasterSamples_ = fboDepth_->sampleCnt;
    }
    else {
        rasterSamples_ = 1;
    }

    processVertexShader();
    processPrimitiveAssembly();
    processClipping();
    processPerspectiveDivide();
    processViewportTransform();
    processFaceCulling();
    processRasterization();

    if (fboColor_ && fboColor_->multiSample) {
        multiSampleResolve();
    }
}

void RendererSoft::endRenderPass() {}

void RendererSoft::waitIdle() {}

/*初始化顶点着色器（varyings）存储空间,遍历所有顶点数据，执行顶点着色器程序*/
void RendererSoft::processVertexShader() {
    //初始化varyings缓冲区
    varyingsCnt_ = shaderProgram_->getShaderVaryingsSize() / sizeof(float);
    varyingsAlignedSize_ = MemoryUtils::alignedSize(varyingsCnt_ * sizeof(float)); 
    varyingsAlignedCnt_ = varyingsAlignedSize_ / sizeof(float);

    // 为所有顶点的输出分配空间
    varyings_ = MemoryUtils::makeAlignedBuffer<float>(vao_->vertexCnt * varyingsAlignedCnt_); 
    float* varyingBuffer = varyings_.get();

    // 准备顶点数据输入
    uint8_t* vertexPtr = vao_->vertexes.data();
    vertexes_.resize(vao_->vertexCnt);

    // 逐顶点处理
    for (int idx = 0; idx < vao_->vertexCnt; idx++) {
        VertexHolder& holder = vertexes_[idx];
        holder.discard = false;
        holder.index = idx;
        holder.vertex = vertexPtr;
        holder.varyings = (varyingsAlignedSize_ > 0) ? (varyingBuffer + idx * varyingsAlignedCnt_) : nullptr;
        vertexShaderImpl(holder);
        vertexPtr += vao_->vertexStride;
    }
}
    
// 根据图元类型进行图元装配处理，存储在primitives_
void RendererSoft::processPrimitiveAssembly() {
    switch (primitiveType_) {
    case Primitive_POINT:
        processPointAssembly();
        break;
    case Primitive_LINE:
        processLineAssembly();
        break;
    case Primitive_TRIANGLE:
        processPolygonAssembly();
        break;
    }
}

// 根据图元类型（点/线/三角形）分发到对应的裁剪函数，
void RendererSoft::processClipping() {
    size_t primitiveCnt = primitives_.size();
    for (int i = 0; i < primitiveCnt; i++) {
        auto& primitive = primitives_[i];
        if (primitive.discard) {
            continue;
        }
        switch (primitiveType_) {   
        case Primitive_POINT:
            clippingPoint(primitive);
            break;
        case Primitive_LINE:
            clippingLine(primitive);
            break;
        case Primitive_TRIANGLE:
            // 非填充模式（线框/点模式）跳过三角形裁剪
            if (renderState_->polygonMode != PolygonMode_FILL) {
                continue;
            }
            std::vector<PrimitiveHolder> appendPrimitives;
            clippingTriangle(primitive, appendPrimitives);// 执行三角形裁剪（可能向appendPrimitives添加新图元）
            primitives_.insert(primitives_.end(), appendPrimitives.begin(), appendPrimitives.end());
            break;
        }
    }

    // 先标记所有顶点为丢弃状态（后续只启用可见顶点），因为在裁剪过程中可能添加了新的顶点
    for (auto& vertex : vertexes_) {
        vertex.discard = true;
    }

    // 根据可见图元更新顶点状态
    for (auto& primitive : primitives_) {
        if (primitive.discard) {
            continue;
        }
        switch (primitiveType_) {
        case Primitive_POINT:
            vertexes_[primitive.indices[0]].discard = false;
            break;
        case Primitive_LINE:
            vertexes_[primitive.indices[0]].discard = false;
            vertexes_[primitive.indices[1]].discard = false;
            break;
        case Primitive_TRIANGLE:
            vertexes_[primitive.indices[0]].discard = false;
            vertexes_[primitive.indices[1]].discard = false;
            vertexes_[primitive.indices[2]].discard = false;
            break;
        }
    }
}

// 将所有顶点进行透视除法
void RendererSoft::processPerspectiveDivide() {
    for (auto& vertex : vertexes_) {
        if (vertex.discard) {
            continue;
        }
        perspectiveDivideImpl(vertex);
    }
}

// 将所有顶点进行视口变换
void RendererSoft::processViewportTransform() {
    for (auto& vertex : vertexes_) {
        if (vertex.discard) {
            continue;
        }
        viewportTransformImpl(vertex);
    }
}

// 面剔除
void RendererSoft::processFaceCulling() {
    if (primitiveType_ != Primitive_TRIANGLE) {
        return;
    }

    for (auto& triangle : primitives_) {
        if (triangle.discard) {
            continue;
        }

        // 这里使用fragPos（透视除法后的屏幕坐标）
        glm::aligned_vec4& v0 = vertexes_[triangle.indices[0]].fragPos;
        glm::aligned_vec4& v1 = vertexes_[triangle.indices[1]].fragPos;
        glm::aligned_vec4& v2 = vertexes_[triangle.indices[2]].fragPos;

        glm::vec3 n = glm::cross(glm::vec3(v1 - v0), glm::vec3(v2 - v0));
        float area = glm::dot(n, glm::vec3(0, 0, 1));
        triangle.frontFacing = area > 0;

        if (renderState_->cullFace) {
            triangle.discard = !triangle.frontFacing;  // discard back face
        }
    }
}

// 多类型图元光栅化分发处理
void RendererSoft::processRasterization() {
    switch (primitiveType_) {
    case Primitive_POINT:
        for (auto& primitive : primitives_) {
            if (primitive.discard) {
                continue;
            }
            auto* vert0 = &vertexes_[primitive.indices[0]];
            rasterizationPoint(vert0, pointSize_);
        }
        break;
    case Primitive_LINE:
        for (auto& primitive : primitives_) {
            if (primitive.discard) {
                continue;
            }
            auto* vert0 = &vertexes_[primitive.indices[0]];
            auto* vert1 = &vertexes_[primitive.indices[1]];
            rasterizationLine(vert0, vert1, renderState_->lineWidth);
        }
        break;
    case Primitive_TRIANGLE:
        // 初始化多线程上下文（每个线程独立副本）
        threadQuadCtx_.resize(threadPool_.getThreadCnt());
        for (auto& ctx : threadQuadCtx_) {
            ctx.SetVaryingsSize(varyingsAlignedCnt_);

            // 克隆着色器程序（保证线程安全）
            ctx.shaderProgram = shaderProgram_->clone();
            ctx.shaderProgram->prepareFragmentShader();

            // 设置导数计算上下文（用于ddx/ddy指令）
            DerivativeContext& df_ctx = ctx.shaderProgram->getShaderBuiltin().dfCtx;
            df_ctx.p0 = ctx.pixels[0].varyingsFrag;
            df_ctx.p1 = ctx.pixels[1].varyingsFrag;
            df_ctx.p2 = ctx.pixels[2].varyingsFrag;
            df_ctx.p3 = ctx.pixels[3].varyingsFrag;
        }
        rasterizationPolygons(primitives_);
        threadPool_.waitTasksFinish();
        break;
    }
}

/*执行片段着色器处理流程*/
void RendererSoft::processFragmentShader(glm::aligned_vec4& screenPos, bool front_facing, void* varyings, ShaderProgramSoft* shader) {
    if (!fboColor_) {
        return;
    }

    // 设置着色器内置变量
    auto& builtin = shader->getShaderBuiltin();
    builtin.FragCoord = screenPos; // gl_FragCoord: (x, y, z, 1/w)
    builtin.FrontFacing = front_facing;

    shader->bindFragmentShaderVaryings(varyings);
    shader->execFragmentShader();
}

/*执行逐采样点的片元操作，进行深度测试、blend、写入fbo, sample = 0 表示无MSAA*/
void RendererSoft::processPerSampleOperations(int x, int y, float depth, const glm::vec4& color, int sample) {
    // depth test
    if (!processDepthTest(x, y, depth, sample, false)) {
        return;
    }

    if (!fboColor_) {
        return;
    }

    glm::vec4 color_clamp = glm::clamp(color, 0.f, 1.f);

    // color blending
    processColorBlending(x, y, color_clamp, sample);

    // write final color to fbo
    setFrameColor(x, y, color_clamp * 255.f, sample);
}

//执行深度测试并更新深度缓冲区  skipWrite是否跳过深度写入
bool RendererSoft::processDepthTest(int x, int y, float depth, int sample, bool skipWrite) {
    if (!renderState_->depthTest || !fboDepth_) {
        return true;
    }

    // 将深度值限制在有效范围内
    depth = glm::clamp(depth, viewport_.absMinDepth, viewport_.absMaxDepth);

    // depth comparison
    float* zPtr = getFrameDepth(x, y, sample);
    if (zPtr && DepthTest(depth, *zPtr, renderState_->depthFunc)) {
        // depth attachment writes
        if (!skipWrite && renderState_->depthMask) {
            *zPtr = depth;
        }
        return true;
    }
    return false;
}

/*执行颜色混合操作*/
void RendererSoft::processColorBlending(int x, int y, glm::vec4& color, int sample) {
    if (renderState_->blend) {
        glm::vec4& srcColor = color;
        glm::vec4 dstColor = glm::vec4(0.f);
        auto* ptr = getFrameColor(x, y, sample);
        if (ptr) {
            dstColor = glm::vec4(*ptr) / 255.f;
        }
        color = calcBlendColor(srcColor, dstColor, renderState_->blendParams);
    }
}

void RendererSoft::processPointAssembly() {
    primitives_.resize(vao_->indicesCnt);
    for (int idx = 0; idx < primitives_.size(); idx++) {
        auto& point = primitives_[idx];
        point.indices[0] = vao_->indices[idx];
        point.discard = false;
    }
}

// 线段图元装配处理（将索引数据转换为线段图元）
void RendererSoft::processLineAssembly() {
    primitives_.resize(vao_->indicesCnt / 2);
    for (int idx = 0; idx < primitives_.size(); idx++) {
        auto& line = primitives_[idx];

        line.indices[0] = vao_->indices[idx * 2];
        line.indices[1] = vao_->indices[idx * 2 + 1];
        line.discard = false;
    }
}

// 多边形图元装配处理（将索引数据转换为三角形图元）
void RendererSoft::processPolygonAssembly() {
    primitives_.resize(vao_->indicesCnt / 3);
    for (int idx = 0; idx < primitives_.size(); idx++) {
        auto& triangle = primitives_[idx];
        //装配三角形顶点索引
        triangle.indices[0] = vao_->indices[idx * 3];
        triangle.indices[1] = vao_->indices[idx * 3 + 1];
        triangle.indices[2] = vao_->indices[idx * 3 + 2];
        triangle.discard = false;
    }
}

void RendererSoft::clippingPoint(PrimitiveHolder& point) {
    point.discard = (vertexes_[point.indices[0]].clipMask != 0);
}

//线段裁剪处理（基于齐次空间裁剪平面）
void RendererSoft::clippingLine(PrimitiveHolder& line, bool postVertexProcess) {
    VertexHolder* v0 = &vertexes_[line.indices[0]];
    VertexHolder* v1 = &vertexes_[line.indices[1]];

    // 提取裁剪相关数据（避免频繁访问结构体）
    auto clipMaskV0 = v0->clipMask;
    auto clipMaskV1 = v1->clipMask;
    auto clipPosV0 = v0->clipPos;
    auto clipPosV1 = v1->clipPos;

    bool fullClip = false; // 完全裁剪标志
    float t0 = 0.f;
    float t1 = 1.f;

    int mask = clipMaskV0 | clipMaskV1;
    if (mask != 0) {
        // 遍历所有6个裁剪平面
        for (int i = 0; i < 6; i++) {
            // 只处理当前线段涉及的裁剪平面
            if (mask & FrustumClipMaskArray[i]) {
                // 计算顶点到平面的距离（带符号）
                float d0 = glm::dot(FrustumClipPlane[i], glm::vec4(clipPosV0));
                float d1 = glm::dot(FrustumClipPlane[i], glm::vec4(clipPosV1));

                // 情况1：两点都在平面外侧 → 完全不可见
                if (d0 < 0 && d1 < 0) {
                    fullClip = true;
                    break;
                }
                else if (d0 < 0) {// 情况2：起点在平面外侧 → 计算交点参数t0
                    float t = -d0 / (d1 - d0);
                    t0 = std::max(t0, t);
                }
                else {// 情况3：终点在平面外侧 → 计算交点参数t1
                    float t = d0 / (d0 - d1);
                    t1 = std::min(t1, t);
                }
            }
        }
    }

    if (fullClip) {
        line.discard = true;
        return;
    }

    if (clipMaskV0) {
        line.indices[0] = clippingNewVertex(line.indices[0], line.indices[1], t0, postVertexProcess);
    }
    if (clipMaskV1) {
        line.indices[1] = clippingNewVertex(line.indices[0], line.indices[1], t1, postVertexProcess);
    }
}

//三角形裁剪处理（基于齐次空间裁剪平面）
void RendererSoft::clippingTriangle(PrimitiveHolder& triangle, std::vector<PrimitiveHolder>& appendPrimitives) {
    auto* v0 = &vertexes_[triangle.indices[0]];
    auto* v1 = &vertexes_[triangle.indices[1]];
    auto* v2 = &vertexes_[triangle.indices[2]];

    // 计算组合裁剪掩码（若三个顶点都在所有裁剪平面内则提前退出）
    int mask = v0->clipMask | v1->clipMask | v2->clipMask;
    if (mask == 0) {
        return;
    }

    //初始化裁剪状态
    bool fullClip = false;// 完全裁剪标志
    std::vector<size_t> indicesIn;// 当前裁剪阶段的输入顶点索引
    std::vector<size_t> indicesOut;// 当前裁剪阶段的输出顶点索引

    indicesIn.push_back(v0->index);
    indicesIn.push_back(v1->index);
    indicesIn.push_back(v2->index);

    // 逐平面裁剪 
    for (int planeIdx = 0; planeIdx < 6; planeIdx++) {
        // 只处理当前三角形涉及的裁剪平面
        if (mask & FrustumClipMaskArray[planeIdx]) {
            if (indicesIn.size() < 3) {
                fullClip = true;
                break;
            }
            indicesOut.clear();
            size_t idxPre = indicesIn[0];
            // 计算前一个顶点到裁剪平面的距离
            float dPre = glm::dot(FrustumClipPlane[planeIdx], glm::vec4(vertexes_[idxPre].clipPos));

            // 闭合多边形：将第一个顶点再次加入尾部
            indicesIn.push_back(idxPre);

            // 遍历每对相邻顶点（包括闭合边）
            for (int i = 1; i < indicesIn.size(); i++) {
                size_t idx = indicesIn[i];
                float d = glm::dot(FrustumClipPlane[planeIdx], glm::vec4(vertexes_[idx].clipPos));

                // 规则1：前一个顶点在可见侧 → 加入输出列表
                if (dPre >= 0) {
                    indicesOut.push_back(idxPre);
                }

                // 规则2：线段与裁剪平面相交 → 计算交点
                if (std::signbit(dPre) != std::signbit(d)) {
                    float t = d < 0 ? dPre / (dPre - d) : -dPre / (d - dPre); // 保证分母为正，减小浮点误差
                    // 创建新顶点（插值顶点属性）
                    auto vertIdx = clippingNewVertex(idxPre, idx, t);
                    indicesOut.push_back(vertIdx);
                }

                // 更新前一个顶点状态
                idxPre = idx;
                dPre = d;
            }

            std::swap(indicesIn, indicesOut);
        }
    }
    // 情况1：完全不可见
    if (fullClip || indicesIn.empty()) {
        triangle.discard = true;
        return;
    }

    // 情况2：仍为三角形（最常见情况）
    triangle.indices[0] = indicesIn[0];
    triangle.indices[1] = indicesIn[1];
    triangle.indices[2] = indicesIn[2];

    // 情况3：被裁剪为凸多边形（需要三角化）
    for (int i = 3; i < indicesIn.size(); i++) {
        appendPrimitives.emplace_back();
        PrimitiveHolder& ph = appendPrimitives.back();
        ph.discard = false;
        ph.indices[0] = indicesIn[0];
        ph.indices[1] = indicesIn[i - 1];
        ph.indices[2] = indicesIn[i];
        ph.frontFacing = triangle.frontFacing;
    }
}

// 根据图元类型进行光栅化
void RendererSoft::rasterizationPolygons(std::vector<PrimitiveHolder>& primitives) {
    switch (renderState_->polygonMode) {
    case PolygonMode_POINT:
        rasterizationPolygonsPoint(primitives);
        break;
    case PolygonMode_LINE:
        rasterizationPolygonsLine(primitives);
        break;
    case PolygonMode_FILL:
        rasterizationPolygonsTriangle(primitives);
        break;
    }
}

// 多边形点模式光栅化处理
void RendererSoft::rasterizationPolygonsPoint(std::vector<PrimitiveHolder>& primitives) {
    for (auto& triangle : primitives) {
        if (triangle.discard) {
            continue;
        }
        for (size_t idx : triangle.indices) {
            PrimitiveHolder point;
            point.discard = false;
            point.frontFacing = triangle.frontFacing;
            point.indices[0] = idx;

            // clipping
            clippingPoint(point);
            if (point.discard) {
                continue;
            }

            // rasterization
            rasterizationPoint(&vertexes_[point.indices[0]], pointSize_);
        }
    }
}

// 多边形线框模式光栅化处理
void RendererSoft::rasterizationPolygonsLine(std::vector<PrimitiveHolder>& primitives) {
    for (auto& triangle : primitives) {
        if (triangle.discard) {
            continue;
        }
        for (size_t i = 0; i < 3; i++) {
            PrimitiveHolder line;
            line.discard = false;
            line.frontFacing = triangle.frontFacing;
            line.indices[0] = triangle.indices[i];
            line.indices[1] = triangle.indices[(i + 1) % 3];

            // clipping
            clippingLine(line, true); //true: 表示在视口空间执行裁剪（已透视除法）
            if (line.discard) {
                continue;
            }

            // rasterization
            rasterizationLine(&vertexes_[line.indices[0]],
                &vertexes_[line.indices[1]],
                renderState_->lineWidth);
        }
    }
}

// 三角形图元光栅化
void RendererSoft::rasterizationPolygonsTriangle(std::vector<PrimitiveHolder>& primitives) {
    for (auto& triangle : primitives) {
        if (triangle.discard) {
            continue;
        }
        rasterizationTriangle(&vertexes_[triangle.indices[0]],
            &vertexes_[triangle.indices[1]],
            &vertexes_[triangle.indices[2]],
            triangle.frontFacing);
    }
}

// 点图元光栅化处理
void RendererSoft::rasterizationPoint(VertexHolder* v, float pointSize) {
    if (!fboColor_) {
        return;
    }

    // 计算点图元的屏幕空间包围盒，+0.5f 是为了将坐标从像素索引转换为像素中心坐标 
    float left = v->fragPos.x - pointSize / 2.f + 0.5f;
    float right = left + pointSize;
    float top = v->fragPos.y - pointSize / 2.f + 0.5f;
    float bottom = top + pointSize;

    // 获取屏幕空间位置引用（用于后续修改）
    glm::aligned_vec4& screenPos = v->fragPos;

    for (int x = (int)left; x < (int)right; x++) {
        for (int y = (int)top; y < (int)bottom; y++) {
            screenPos.x = (float)x;
            screenPos.y = (float)y;
            // 执行片段着色器
            processFragmentShader(screenPos, true, v->varyings, shaderProgram_);
            auto& builtIn = shaderProgram_->getShaderBuiltin();
            if (!builtIn.discard) {
                // TODO MSAA
                for (int idx = 0; idx < rasterSamples_; idx++) {
                    processPerSampleOperations(x, y, screenPos.z, builtIn.FragColor, idx);
                }
            }
        }
    }
}

// 直线光栅化处理
void RendererSoft::rasterizationLine(VertexHolder* v0, VertexHolder* v1, float lineWidth) {
    // TODO diamond-exit rule
    //  提取屏幕空间整数坐标（像素位置）
    int x0 = (int)v0->fragPos.x, y0 = (int)v0->fragPos.y;
    int x1 = (int)v1->fragPos.x, y1 = (int)v1->fragPos.y;

    // 保留原始深度和透视系数（用于插值）
    float z0 = v0->fragPos.z;
    float z1 = v1->fragPos.z;
    float w0 = v0->fragPos.w;
    float w1 = v1->fragPos.w;

    // 检测是否为陡峭线段（斜率>1）
    bool steep = false;
    if (std::abs(x0 - x1) < std::abs(y0 - y1)) {// 交换x/y坐标（将陡峭线段转换为平缓线段处理）
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }

    // 准备顶点属性插值源数据
    const float* varyingsIn[2] = { v0->varyings, v1->varyings };

    // 确保从左到右绘制（x0 < x1）
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(z0, z1);
        std::swap(w0, w1);
        std::swap(varyingsIn[0], varyingsIn[1]);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;

    //Bresenham算法核心
    int error = 0;// 误差累加器
    int dError = 2 * std::abs(dy);// 误差增量

    int y = y0;

    // 创建临时顶点用于插值结果存储
    auto varyings = MemoryUtils::makeBuffer<float>(varyingsCnt_);
    VertexHolder pt{};
    pt.varyings = varyings.get();

    //--------------------- 主循环：遍历x方向每个像素----------------- 
    float t = 0;// 插值系数
    for (int x = x0; x <= x1; x++) {
        // 计算当前点在线段中的比例t ∈ [0,1]
        t = (float)(x - x0) / (float)dx;
        pt.fragPos = glm::vec4(x, y, glm::mix(z0, z1, t), glm::mix(w0, w1, t));

        // 如果是陡峭线段，交换回原始坐标空间
        if (steep) {
            std::swap(pt.fragPos.x, pt.fragPos.y);
        }

        // 插值顶点属性（颜色/纹理坐标等）
        interpolateLinear(pt.varyings, varyingsIn, varyingsCnt_, t);

        // 以当前点为中心绘制线宽（扩展为圆形/方形）
        rasterizationPoint(&pt, lineWidth);

        // Bresenham误差更新
        error += dError;
        if (error > dx) {
            y += (y1 > y0 ? 1 : -1);// 根据斜率方向调整y
            error -= 2 * dx;
        }
    }
}

/*三角形光栅化处理， 分块处理*/
void RendererSoft::rasterizationTriangle(VertexHolder* v0, VertexHolder* v1, VertexHolder* v2, bool frontFacing) {
    // TODO top-left rule
    VertexHolder* vert[3] = { v0, v1, v2 };
    glm::aligned_vec4 screenPos[3] = { vert[0]->fragPos, vert[1]->fragPos, vert[2]->fragPos };
    BoundingBox bounds = triangleBoundingBox(screenPos, viewport_.width, viewport_.height);
    bounds.min -= 1.f;// 扩展1像素避免边界误差

    auto blockSize = rasterBlockSize_;// 分块大小（默认32x32）
    // 计算x/y方向的分块数量（向上取整）
    int blockCntX = (bounds.max.x - bounds.min.x + blockSize - 1.f) / blockSize;
    int blockCntY = (bounds.max.y - bounds.min.y + blockSize - 1.f) / blockSize;

    // 分块遍历
    for (int blockY = 0; blockY < blockCntY; blockY++) {
        for (int blockX = 0; blockX < blockCntX; blockX++) {
#ifdef RASTER_MULTI_THREAD
            threadPool_.pushTask([&, vert, bounds, blockSize, blockX, blockY](int thread_id) {
                // init pixel quad
                auto pixelQuad = threadQuadCtx_[thread_id];
#else
            auto pixelQuad = threadQuadCtx_[0];
#endif
            pixelQuad.frontFacing = frontFacing;
            // 填充顶点数据（位置、深度、透视校正系数、插值变量）
            for (int i = 0; i < 3; i++) {
                pixelQuad.vertPos[i] = vert[i]->fragPos;
                pixelQuad.vertZ[i] = &vert[i]->fragPos.z;
                pixelQuad.vertW[i] = vert[i]->fragPos.w;// 1/w（用于透视校正）
                pixelQuad.vertVaryings[i] = vert[i]->varyings;// 顶点着色器输出变量
            }

            // 优化数据布局（SIMD友好）重心坐标的系数 (α, β, γ) 默认对应三角形的三个顶点时，其计算顺序与 ​​顶点顺序相反​​，所以按v2,v1,v0的顺序存储
            glm::aligned_vec4* vertPos = pixelQuad.vertPos;
            pixelQuad.vertPosFlat[0] = { vertPos[2].x, vertPos[1].x, vertPos[0].x, 0.f };
            pixelQuad.vertPosFlat[1] = { vertPos[2].y, vertPos[1].y, vertPos[0].y, 0.f };
            pixelQuad.vertPosFlat[2] = { vertPos[0].z, vertPos[1].z, vertPos[2].z, 0.f };// 注意z顺序反转
            pixelQuad.vertPosFlat[3] = { vertPos[0].w, vertPos[1].w, vertPos[2].w, 0.f };

            // 处理当前分块
            int blockStartX = bounds.min.x + blockX * blockSize;
            int blockStartY = bounds.min.y + blockY * blockSize;
            // 以2x2像素为单元遍历（提高缓存命中率）
            for (int y = blockStartY + 1; y < blockStartY + blockSize && y <= bounds.max.y; y += 2) {
                for (int x = blockStartX + 1; x < blockStartX + blockSize && x <= bounds.max.x; x += 2) {
                    pixelQuad.Init((float)x, (float)y, rasterSamples_);
                    rasterizationPixelQuad(pixelQuad);
                }
            }
#ifdef RASTER_MULTI_THREAD
                });
#endif
        }
    }
}

/*执行像素四边形光栅化处理 ，处理流程包含：覆盖测试、深度插值、Early Z测试、变量插值和片段着色*/
void RendererSoft::rasterizationPixelQuad(PixelQuadContext& quad) {

    glm::aligned_vec4* vert = quad.vertPosFlat;
    glm::aligned_vec4& v0 = quad.vertPos[0];

    // -----------------------------重心坐标计算与覆盖测试----------------------------------
    for (auto& pixel : quad.pixels) {
        for (auto& sample : pixel.samples) {
            sample.inside = barycentric(vert, v0, sample.position, sample.barycentric);
        }
        pixel.InitCoverage();
        pixel.InitShadingSample();
    }
    if (!quad.CheckInside()) {
        return;
    }

    // ------------------------------深度值插值与裁剪------------------------------------------
    for (auto& pixel : quad.pixels) {
        for (auto& sample : pixel.samples) {
            if (!sample.inside) {
                continue;
            }

            // interpolate z, w
            interpolateBarycentric(&sample.position.z, quad.vertZ, 2, sample.barycentric);

            // 深度裁剪
            if (sample.position.z < viewport_.absMinDepth || sample.position.z > viewport_.absMaxDepth) {
                sample.inside = false;
            }

            // 透视校正：调整重心坐标（用于后续变量插值）
            sample.barycentric *= (1.f / sample.position.w * quad.vertW);
        }
    }

    // early z
    if (earlyZ_ && renderState_->depthTest) {
        if (!earlyZTest(quad)) {
            return;
        }
    }

    // 变量插值（用于片段着色）
    // note: all quad pixels should perform varying interpolate to enable varying partial derivative
    for (auto& pixel : quad.pixels) {
        interpolateBarycentric((float*)pixel.varyingsFrag, quad.vertVaryings, varyingsCnt_, pixel.sampleShading->barycentric);
    }

    //-------------------------------------- 片段着色与逐采样点操作----------------------------------------------
    for (auto& pixel : quad.pixels) {
        if (!pixel.inside) {
            continue;
        }

        // fragment shader
        processFragmentShader(pixel.sampleShading->position, quad.frontFacing, pixel.varyingsFrag, quad.shaderProgram.get());

        // 获取着色器输出颜色
        auto& builtIn = quad.shaderProgram->getShaderBuiltin();

        // 处理每个采样点（MSAA）或主采样点（非MSAA）
        if (pixel.sampleCount > 1) {
            for (int idx = 0; idx < pixel.sampleCount; idx++) {
                auto& sample = pixel.samples[idx];
                if (!sample.inside) {
                    continue;
                }
                processPerSampleOperations(sample.fboCoord.x, sample.fboCoord.y, sample.position.z, builtIn.FragColor, idx);
            }
        }   
        else {
            auto& sample = *pixel.sampleShading;
            processPerSampleOperations(sample.fboCoord.x, sample.fboCoord.y, sample.position.z, builtIn.FragColor, 0);
        }
    }
}

/* 执行early_z，提前剔除被遮挡的像素*/
bool RendererSoft::earlyZTest(PixelQuadContext& quad) {
    // 遍历四边形中的每个像素
    for (auto& pixel : quad.pixels) {
        if (!pixel.inside) {// 如果像素已被标记为不在三角形内，则跳过处理
            continue;
        }
        if (pixel.sampleCount > 1) {
            bool inside = false;
            for (int idx = 0; idx < pixel.sampleCount; idx++) {
                auto& sample = pixel.samples[idx];
                if (!sample.inside) { // 跳过不在三角形内的采样点
                    continue;
                }
                sample.inside = processDepthTest(sample.fboCoord.x, sample.fboCoord.y, sample.position.z, idx, true);
                if (sample.inside) {
                    inside = true;
                }
            }
            pixel.inside = inside;
        }
        else {
            auto& sample = *pixel.sampleShading;
            sample.inside = processDepthTest(sample.fboCoord.x, sample.fboCoord.y, sample.position.z, 0, true);
            pixel.inside = sample.inside;// 更新像素可见状态
        }
    }
    return quad.CheckInside();
}

/*多重采样抗锯齿(MSAA)解析函数 ，将多重采样缓冲区(MSAA)解析为单采样颜色缓冲区*/
void RendererSoft::multiSampleResolve() {
    if (!fboColor_->buffer) {
        fboColor_->buffer = Buffer<RGBA>::makeDefault(fboColor_->width, fboColor_->height);
    }

    auto* srcPtr = fboColor_->bufferMs4x->getRawDataPtr();  // MSAA源数据指针
    auto* dstPtr = fboColor_->buffer->getRawDataPtr();      // 目标单采样缓冲区指针

    // ----------------------------遍历所有像素行------------------------
    for (size_t row = 0; row < fboColor_->height; row++) {
        // 计算当前行的起始指针
        auto* rowSrc = srcPtr + row * fboColor_->width;
        auto* rowDst = dstPtr + row * fboColor_->width;
#ifdef RASTER_MULTI_THREAD
        threadPool_.pushTask([&, rowSrc, rowDst](int thread_id) {
#endif
            // 处理当前行的每个像素
            auto* src = rowSrc; 
            auto* dst = rowDst;
            for (size_t idx = 0; idx < fboColor_->width; idx++) {
                glm::vec4 color(0.f);
                for (int i = 0; i < fboColor_->sampleCnt; i++) {
                    color += (glm::vec4)(*src)[i];
                }
                color /= fboColor_->sampleCnt;
                *dst = color;
                src++;
                dst++;
            }
#ifdef RASTER_MULTI_THREAD
            });
#endif
    }
    // 等待所有线程完成（如果启用多线程）
    threadPool_.waitTasksFinish();
}

// 获取帧缓冲区指定像素位置的颜色指针，sample表示像素的第几个采样点，0表示主采样
RGBA* RendererSoft::getFrameColor(int x, int y, int sample) {
    if (!fboColor_) {
        return nullptr;
    }

    RGBA* ptr = nullptr;
    if (fboColor_->multiSample) {
        auto* ptrMs = fboColor_->bufferMs4x->get(x, y);
        if (ptrMs) {
            ptr = (RGBA*)ptrMs + sample;
        }
    }
    else {
        ptr = fboColor_->buffer->get(x, y);
    }

    return ptr;
}

float* RendererSoft::getFrameDepth(int x, int y, int sample) {
    if (!fboDepth_) {
        return nullptr;
    }

    float* depthPtr = nullptr;
    if (fboDepth_->multiSample) {
        auto* ptr = fboDepth_->bufferMs4x->get(x, y);
        if (ptr) {
            depthPtr = &ptr->x + sample;
        }
    }
    else {
        depthPtr = fboDepth_->buffer->get(x, y);
    }
    return depthPtr;
}

/*设置帧缓冲区中指定像素位置的颜色值*/
void RendererSoft::setFrameColor(int x, int y, const RGBA& color, int sample) {
    /*x,y：坐标，从左到右，从上到下， sample：多重采样索引 0为主采样*/
    RGBA* ptr = getFrameColor(x, y, sample);
    if (ptr) {
        *ptr = color;
    }
}

/*在裁剪过程中生成新的顶点*/
size_t RendererSoft::clippingNewVertex(size_t idx0, size_t idx1, float t, bool postVertexProcess) {
    vertexes_.emplace_back();
    VertexHolder& vh = vertexes_.back();
    vh.discard = false;
    vh.index = vertexes_.size() - 1;
    interpolateVertex(vh, vertexes_[idx0], vertexes_[idx1], t);

    if (postVertexProcess) {
        perspectiveDivideImpl(vh);// 透视除法
        viewportTransformImpl(vh);// 视口变换
    }

    return vh.index; // 新顶点的索引
}

/*执行顶点着色器处理，进行mvp变换之类的，转换到裁剪空间*/
void RendererSoft::vertexShaderImpl(VertexHolder& vertex) {
    shaderProgram_->bindVertexAttributes(vertex.vertex); // 将原始顶点数据（vertex.vertex）传递给着色器程序
    shaderProgram_->bindVertexShaderVaryings(vertex.varyings);// 为着色器指定varying变量的输出内存位置
    shaderProgram_->execVertexShader();

    //-----------------------------------获取着色器输出---------------------------------
    pointSize_ = shaderProgram_->getShaderBuiltin().PointSize;
    vertex.clipPos = shaderProgram_->getShaderBuiltin().Position;
    vertex.clipMask = countFrustumClipMask(vertex.clipPos);
}

/*执行透视除法*/
void RendererSoft::perspectiveDivideImpl(VertexHolder& vertex) {
    vertex.fragPos = vertex.clipPos;
    auto& pos = vertex.fragPos;
    float invW = 1.f / pos.w;
    pos *= invW;

    // 这个1/w值后续会用于：
    // 1. 透视校正插值（如纹理坐标、颜色等）
    // 2. 深度测试（实际深度值是z/w）
    pos.w = invW;
}

/*执行视口变换*/
void RendererSoft::viewportTransformImpl(VertexHolder& vertex) {
    // 缩放变换 
    vertex.fragPos *= viewport_.innerP;

    // 平移变换
    vertex.fragPos += viewport_.innerO;
}

/*计算顶点在视锥体裁剪空间中的裁剪掩码， 返回值: 6位掩码，每位代表一个裁剪平面的可见性状态*/
int RendererSoft::countFrustumClipMask(glm::aligned_vec4& clipPos) {
    int mask = 0;
    if (clipPos.w < clipPos.x) mask |= FrustumClipMask::POSITIVE_X;
    if (clipPos.w < -clipPos.x) mask |= FrustumClipMask::NEGATIVE_X;
    if (clipPos.w < clipPos.y) mask |= FrustumClipMask::POSITIVE_Y;
    if (clipPos.w < -clipPos.y) mask |= FrustumClipMask::NEGATIVE_Y;
    if (clipPos.w < clipPos.z) mask |= FrustumClipMask::POSITIVE_Z;
    if (clipPos.w < -clipPos.z) mask |= FrustumClipMask::NEGATIVE_Z;
    return mask;
}

/*计算三角形在屏幕空间中的包围盒*/
BoundingBox RendererSoft::triangleBoundingBox(glm::aligned_vec4* vert, float width, float height) {
    float minX = std::min(std::min(vert[0].x, vert[1].x), vert[2].x);
    float minY = std::min(std::min(vert[0].y, vert[1].y), vert[2].y);
    float maxX = std::max(std::max(vert[0].x, vert[1].x), vert[2].x);
    float maxY = std::max(std::max(vert[0].y, vert[1].y), vert[2].y);
    /*扩展边界0.5像素（考虑像素中心坐标 vs 像素覆盖区域），并限制在屏幕范围内（避免越界访问）*/
    minX = std::max(minX - 0.5f, 0.f);
    minY = std::max(minY - 0.5f, 0.f);
    maxX = std::min(maxX + 0.5f, width - 1.f);
    maxY = std::min(maxY + 0.5f, height - 1.f);

    auto min = glm::vec3(minX, minY, 0.f);
    auto max = glm::vec3(maxX, maxY, 0.f);
    return { min, max };
}

/*计算点p在三角形内的重心坐标，vert：顶点数组；v0：三角形第一个顶点；返回是否在三角形内*/
bool RendererSoft::barycentric(glm::aligned_vec4* vert, glm::aligned_vec4& v0, glm::aligned_vec4& p, glm::aligned_vec4& bc) {
#ifdef SOFTGL_SIMD_OPT
    // Ref: https://geometrian.com/programming/tutorials/cross-product/index.php
    // 计算向量差：vec0 = [v1.x - v0.x, v2.x - v0.x, p.x - v0.x, 0]
    __m128 vec0 = _mm_sub_ps(_mm_load_ps(&vert[0].x), _mm_set_ps(0, p.x, v0.x, v0.x));
    __m128 vec1 = _mm_sub_ps(_mm_load_ps(&vert[1].x), _mm_set_ps(0, p.y, v0.y, v0.y));

    __m128 tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 tmp2 = _mm_mul_ps(tmp0, vec1);
    __m128 tmp3 = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 u = _mm_sub_ps(_mm_mul_ps(tmp0, tmp1), tmp3);

    if (std::abs(MM_F32(u, 2)) < FLT_EPSILON) {
        return false;
    }

    u = _mm_div_ps(u, _mm_set1_ps(MM_F32(u, 2)));
    bc = { 1.f - (MM_F32(u, 0) + MM_F32(u, 1)), MM_F32(u, 1), MM_F32(u, 0), 0.f };
#else
    glm::vec3 u = glm::cross(glm::vec3(vert[0]) - glm::vec3(v0.x, v0.x, p.x),
        glm::vec3(vert[1]) - glm::vec3(v0.y, v0.y, p.y));
    if (std::abs(u.z) < FLT_EPSILON) {
        return false;
    }

    u /= u.z;
    bc = { 1.f - (u.x + u.y), u.y, u.x, 0.f };
#endif

    if (bc.x < 0 || bc.y < 0 || bc.z < 0) {
        return false;
    }

    return true;
}

/*顶点插值函数，在两个顶点之间进行插值。 最后调用了顶点着色器执行*/
void RendererSoft::interpolateVertex(VertexHolder& out, VertexHolder& v0, VertexHolder& v1, float t) {
    //--------------------内存分配---------------------------------------
    out.vertexHolder = MemoryUtils::makeBuffer<uint8_t>(vao_->vertexStride);
    out.vertex = out.vertexHolder.get();
    out.varyingsHolder = MemoryUtils::makeAlignedBuffer<float>(varyingsAlignedCnt_);
    out.varyings = out.varyingsHolder.get();

    // interpolate vertex (only support float element right now)
    const float* vertexIn[2] = { (float*)v0.vertex, (float*)v1.vertex };
    interpolateLinear((float*)out.vertex, vertexIn, vao_->vertexStride / sizeof(float), t);

    // vertex shader
    vertexShaderImpl(out);
}

/*线性插值*/
void RendererSoft::interpolateLinear(float* varsOut, const float* varsIn[2], size_t elemCnt, float t) {
    const float* inVar0 = varsIn[0];
    const float* inVar1 = varsIn[1];

    if (inVar0 == nullptr || inVar1 == nullptr) {
        return;
    }

    for (int i = 0; i < elemCnt; i++) {
        varsOut[i] = glm::mix(*(inVar0 + i), *(inVar1 + i), t);
    }
}

/*使用重心坐标对顶点属性进行插值计算*/
void RendererSoft::interpolateBarycentric(float* varsOut, const float* varsIn[3], size_t elemCnt, glm::aligned_vec4& bc) {
    const float* inVar0 = varsIn[0];
    const float* inVar1 = varsIn[1];
    const float* inVar2 = varsIn[2];

    if (inVar0 == nullptr || inVar1 == nullptr || inVar2 == nullptr) {
        return;
    }

    bool simd_enabled = false;
#ifdef SOFTGL_SIMD_OPT
    // 检查所有输入/输出指针的内存地址是否满足SIMD对齐要求
    if ((PTR_ADDR(inVar0) % SOFTGL_ALIGNMENT == 0) &&
        (PTR_ADDR(inVar1) % SOFTGL_ALIGNMENT == 0) &&
        (PTR_ADDR(inVar2) % SOFTGL_ALIGNMENT == 0) &&
        (PTR_ADDR(varsOut) % SOFTGL_ALIGNMENT == 0)) {
        simd_enabled = true;
    }
#endif

    if (simd_enabled) {
        interpolateBarycentricSIMD(varsOut, varsIn, elemCnt, bc);
    }
    else {
        for (int i = 0; i < elemCnt; i++) {
            varsOut[i] = glm::dot(glm::vec4(bc), glm::vec4(*(inVar0 + i), *(inVar1 + i), *(inVar2 + i), 0.f));
        }
    }
}

/*  elemCnt: 每个顶点的属性元素数量（如RGBA = 4，UV = 2等）
    bc: 重心坐标（bc[0],bc[1],bc[2]对应三个顶点的权重）*/ 
void RendererSoft::interpolateBarycentricSIMD(float* varsOut, const float* varsIn[3], size_t elemCnt, glm::aligned_vec4& bc) {
#ifdef SOFTGL_SIMD_OPT
    // 解引用输入属性指针（三个顶点的属性数组）
    const float* inVar0 = varsIn[0];
    const float* inVar1 = varsIn[1];
    const float* inVar2 = varsIn[2];

    uint32_t idx = 0;// 当前处理元素索引
    uint32_t end; // 分段处理结束位置

    //----------------------------第一阶段：AVX-256处理（每次处理8个float）------------------------
    end = elemCnt & (~7); // 计算能被8整除的最大元素数量（按8对齐）
    if (end > 0) {
        // 将重心坐标广播到AVX寄存器（bc0=8个bc[0]，bc1=8个bc[1]，bc2=8个bc[2]）
        __m256 bc0 = _mm256_set1_ps(bc[0]);
        __m256 bc1 = _mm256_set1_ps(bc[1]);
        __m256 bc2 = _mm256_set1_ps(bc[2]);

        // 主循环：每次处理8个元素
        for (; idx < end; idx += 8) {
            // 计算 varsOut = varsIn[0]*bc[0] + varsIn[1]*bc[1] + varsIn[2]*bc[2]
            __m256 sum = _mm256_mul_ps(_mm256_load_ps(inVar0 + idx), bc0);// sum = inVar0 * bc0
            sum = _mm256_fmadd_ps(_mm256_load_ps(inVar1 + idx), bc1, sum);// sum += inVar1 * bc1
            sum = _mm256_fmadd_ps(_mm256_load_ps(inVar2 + idx), bc2, sum);// sum += inVar2 * bc2
            _mm256_store_ps(varsOut + idx, sum);// 存储结果
        }
    }

    // -------------------------------第二阶段：SSE处理（每次处理4个float）---------------------------------------
    // 处理剩余元素（当总元素数不是8的倍数时）
    end = (elemCnt - idx) & (~3);
    if (end > 0) {
        __m128 bc0 = _mm_set1_ps(bc[0]);
        __m128 bc1 = _mm_set1_ps(bc[1]);
        __m128 bc2 = _mm_set1_ps(bc[2]);

        for (; idx < end; idx += 4) {
            __m128 sum = _mm_mul_ps(_mm_load_ps(inVar0 + idx), bc0);
            sum = _mm_fmadd_ps(_mm_load_ps(inVar1 + idx), bc1, sum);
            sum = _mm_fmadd_ps(_mm_load_ps(inVar2 + idx), bc2, sum);
            _mm_store_ps(varsOut + idx, sum);
        }
    }

    //----------------------------------第三阶段：标量处理（处理最后1-3个剩余元素）---------------------------------------
    for (; idx < elemCnt; idx++) {
        varsOut[idx] = 0;
        varsOut[idx] += *(inVar0 + idx) * bc[0];
        varsOut[idx] += *(inVar1 + idx) * bc[1];
        varsOut[idx] += *(inVar2 + idx) * bc[2];
    }
#endif
}


}