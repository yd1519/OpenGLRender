#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H

#include "Base/MemoryUtils.h"
#include "Render/Software/ShaderProgramSoft.h"

namespace OpenGL {

struct Viewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;

    // ref: https://registry.khronos.org/vulkan/specs/1.0/html/chap24.html#vertexpostproc-viewport
    glm::vec4 innerO; // 视口变换偏移量，innerO = glm::vec4(viewportWidth/2, viewportHeight/2, 0.5, 0.0);
    glm::vec4 innerP; // 视口变换比例，innerP = glm::vec4(viewportWidth/2, viewportHeight/2, 0.5, 1.0);

    float absMinDepth; //用于裁剪
    float absMaxDepth;
};

// 顶点数据容器，用于着色过程存储临时数据
struct VertexHolder {
    bool discard = false;
    size_t index = 0; // 在顶点数组中的下标

    void* vertex = nullptr; // 指向vao中第index个顶点数据
    float* varyings = nullptr; // 指向渲染器中varyings第index个varying

    int clipMask = 0; // 裁剪空间掩码(6个裁剪平面)，
    glm::aligned_vec4 clipPos = glm::vec4(0.f);     // 裁剪空间坐标，经过顶点着色器处理后得出
    glm::aligned_vec4 fragPos = glm::vec4(0.f);     // 屏幕空间坐标
    std::shared_ptr<uint8_t> vertexHolder = nullptr; //顶点数据内存块
    std::shared_ptr<float> varyingsHolder = nullptr; //varying数据内存块
};                                                                                  

// 图元数据容器，图元装配阶段
struct PrimitiveHolder {
    bool discard = false;
    bool frontFacing = true; // 是否正面朝向
    size_t indices[3] = { 0, 0, 0 };
};

// 采样点内容
class SampleContext {
public:
    bool inside = false; //是否在三角形内
    glm::ivec2 fboCoord = glm::ivec2(0); // 在FBO中的坐标 fboCoord = glm::ivec2(floor(position.x), floor(position.y));
    glm::aligned_vec4 position = glm::aligned_vec4(0.f); // 精确采样位置
    glm::aligned_vec4 barycentric = glm::aligned_vec4(0.f);// 重心坐标
};

// 像素渲染上下文
class PixelContext {
public:
    // 获取4x多重采样的采样点位置(旋转网格模式)
    inline static glm::vec2* GetSampleLocation4X() {
        static glm::vec2 location_4x[4] = {
            {0.375f, 0.875f},
            {0.875f, 0.625f},
            {0.125f, 0.375f},
            {0.625f, 0.125f},
        };
        return location_4x;
    }

    // 初始化像素上下文
    void Init(float x, float y, int sample_cnt = 1) {
        inside = false;
        sampleCount = sample_cnt;
        coverage = 0;
        if (sampleCount > 1) {
            samples.resize(sampleCount + 1);  //多重采样: 存储额外采样点+中心点
            if (sampleCount == 4) { // 初始化4个采样点位置
                for (int i = 0; i < sampleCount; i++) {
                    samples[i].fboCoord = glm::ivec2(x, y);
                    samples[i].position = glm::vec4(GetSampleLocation4X()[i] + glm::vec2(x, y), 0.f, 0.f);
                }
                // pixel center
                samples[4].fboCoord = glm::ivec2(x, y);
                samples[4].position = glm::vec4(x + 0.5f, y + 0.5f, 0.f, 0.f);
                sampleShading = &samples[4];
            }
            else {
                // not support
            }
        }
        else { // 无抗拒齿
            samples.resize(1);
            samples[0].fboCoord = glm::ivec2(x, y);
            samples[0].position = glm::vec4(x + 0.5f, y + 0.5f, 0.f, 0.f);
            sampleShading = &samples[0];
        }
    }

    // 初始化覆盖率(计算有多少采样点在三角形内)
    bool InitCoverage() {
        if (sampleCount > 1) {
            coverage = 0;
            inside = false;
            for (int i = 0; i < samples.size() - 1; i++) { // 统计有效采样点
                if (samples[i].inside) {
                    coverage++;
                }
            }
            inside = coverage > 0;
        }
        else {
            coverage = 1;
            inside = samples[0].inside;
        }
        return inside;
    }

    // 初始化着色采样点(选择第一个有效采样点)
    void InitShadingSample() {
        if (sampleShading->inside) {
            return;
        }
        for (auto& sample : samples) {
            if (sample.inside) {
                sampleShading = &sample;
                break;
            }
        }
    }

public:
    bool inside = false;
    float* varyingsFrag = nullptr; // 纹理坐标、法线向量、.....
    std::vector<SampleContext> samples; // 采样点
    SampleContext* sampleShading = nullptr; // 当前用于着色的采样点
    int sampleCount = 0; // 采样数(1 / 4)
    int coverage = 0; // 覆盖率(有效采样点数)
};

// 2x2像素块上下文
class PixelQuadContext {
public:
    // 设置varying数据大小(按需分配内存), size :每个像素的varying数据量
    void SetVaryingsSize(size_t size) {
        if (varyingsAlignedCnt_ != size) {
            varyingsAlignedCnt_ = size;
            // 分配对齐内存: 4个像素 x 每个像素size个float
            varyingsPool_ = MemoryUtils::makeAlignedBuffer<float>(4 * varyingsAlignedCnt_);
            // 设置每个像素的varying指针
            for (int i = 0; i < 4; i++) {
                pixels[i].varyingsFrag = varyingsPool_.get() + i * varyingsAlignedCnt_;
            }
        }
    }
    
    // 初始化2x2像素块
    void Init(float x, float y, int sample_cnt = 1) {
        pixels[0].Init(x, y, sample_cnt);
        pixels[1].Init(x + 1, y, sample_cnt);
        pixels[2].Init(x, y + 1, sample_cnt);
        pixels[3].Init(x + 1, y + 1, sample_cnt);
    }

    // 检查是否至少有一个像素在三角形内
    bool CheckInside() {
        return pixels[0].inside || pixels[1].inside || pixels[2].inside || pixels[3].inside;
    }

public:
    /**
        *   p2--p3
        *   |   |
        *   p0--p1
        */
    PixelContext pixels[4];

    // triangle vertex screen space position
    glm::aligned_vec4 vertPos[3]; // 三角形原始顶点位置
    glm::aligned_vec4 vertPosFlat[4]; // 展平后的位置， 便于SIMD计算  x0,x1,x2,0; y0,y1,....

    // 深度校正相关
    const float* vertZ[3] = { nullptr, nullptr, nullptr }; // 顶点深度值指针，可以用于插值
    glm::aligned_vec4 vertW = glm::aligned_vec4(0.f, 0.f, 0.f, 1.f); // W分量

    // triangle vertex shader varyings
    const float* vertVaryings[3] = { nullptr, nullptr, nullptr }; // 顶点变量数据

    // triangle Facing
    bool frontFacing = true;

    // shader program
    std::shared_ptr<ShaderProgramSoft> shaderProgram = nullptr;

private:
    size_t varyingsAlignedCnt_ = 0; // varying数据对齐大小
    std::shared_ptr<float> varyingsPool_ = nullptr; // varying数据内存池
};

}

#endif
