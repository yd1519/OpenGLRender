#ifndef RENDERERSOFT_H
#define RENDERERSOFT_H

#include <memory>
#include "Render/Renderer.h"
#include "Render/Software/RendererInternal.h"
#include "Base/Geometry.h"
#include "Base/ThreadPool.h"
#include "Render/Software/VertexSoft.h"
#include "Render/Software/FramebufferSoft.h"
namespace OpenGL {
class RendererSoft : public Renderer {
public:
	RendererType type() override { return Renderer_SOFT; }
	//**********************************资源创建接口*************************************
	// framebuffer
	std::shared_ptr<FrameBuffer> createFrameBuffer(bool offscreen) override;

	// texture
	std::shared_ptr<Texture> createTexture(const TextureDesc& desc) override;

	// vertex
	std::shared_ptr<VertexArrayObject> createVertexArrayObject(const VertexArray& vertexArray) override;

	// shader program
	std::shared_ptr<ShaderProgram> createShaderProgram() override;

	// pipeline states
	std::shared_ptr<PipelineStates> createPipelineStates(const RenderStates& renderStates) override;

	// uniform
	std::shared_ptr<UniformBlock> createUniformBlock(const std::string& name, int size) override;
	std::shared_ptr<UniformSampler> createUniformSampler(const std::string& name, const TextureDesc& desc) override;

	//***************************** 渲染管线控制接口  *************************************
	void beginRenderPass(std::shared_ptr<FrameBuffer>& frameBuffer, const ClearStates& states) override;
	void setViewPort(int x, int y, int width, int height) override;
	void setVertexArrayObject(std::shared_ptr<VertexArrayObject>& vao) override;
	void setShaderProgram(std::shared_ptr<ShaderProgram>& program) override;
	void setShaderResources(std::shared_ptr<ShaderResources>& resources) override;
	void setPipelineStates(std::shared_ptr<PipelineStates>& states) override;
	void draw() override;
	void endRenderPass() override;
	void waitIdle() override;

public:
	inline void setEnableEarlyZ(bool enable) { earlyZ_ = enable; };

private:
	/*******************************  图形管线处理阶段  *********************************/
	void processVertexShader();
	void processPrimitiveAssembly();
	void processClipping();
	void processPerspectiveDivide();
	void processViewportTransform();
	void processFaceCulling();
	void processRasterization();
	void processFragmentShader(glm::aligned_vec4& screenPos, bool frontFacing, void* varyings, ShaderProgramSoft* shader);
	void processPerSampleOperations(int x, int y, float depth, const glm::vec4& color, int sample);
	bool processDepthTest(int x, int y, float depth, int sample, bool skipWrite);
	void processColorBlending(int x, int y, glm::vec4& color, int sample);
	/*******************************  图元处理 *********************************/
	void processPointAssembly();
	void processLineAssembly();
	void processPolygonAssembly();
	/*******************************  裁剪算法  *********************************/
	void clippingPoint(PrimitiveHolder& point);
	void clippingLine(PrimitiveHolder& line, bool postVertexProcess = false);
	void clippingTriangle(PrimitiveHolder& triangle, std::vector<PrimitiveHolder>& appendPrimitives);
	/*******************************  插值计算  *********************************/
	void interpolateVertex(VertexHolder& out, VertexHolder& v0, VertexHolder& v1, float t);
	void interpolateLinear(float* varsOut, const float* varsIn[2], size_t elemCnt, float t);
	void interpolateBarycentric(float* varsOut, const float* varsIn[3], size_t elemCnt, glm::aligned_vec4& bc);
	void interpolateBarycentricSIMD(float* varsOut, const float* varsIn[3], size_t elemCnt, glm::aligned_vec4& bc);
	/*******************************  光栅化算法  *********************************/
	void rasterizationPoint(VertexHolder* v, float pointSize);
	void rasterizationLine(VertexHolder* v0, VertexHolder* v1, float lineWidth);
	void rasterizationTriangle(VertexHolder* v0, VertexHolder* v1, VertexHolder* v2, bool frontFacing);
	void rasterizationPolygons(std::vector<PrimitiveHolder>& primitives);
	void rasterizationPolygonsPoint(std::vector<PrimitiveHolder>& primitives);
	void rasterizationPolygonsLine(std::vector<PrimitiveHolder>& primitives);
	void rasterizationPolygonsTriangle(std::vector<PrimitiveHolder>& primitives);
	void rasterizationPixelQuad(PixelQuadContext& quad);
	/*******************************  高级特性  *********************************/
	bool earlyZTest(PixelQuadContext& quad);
	void multiSampleResolve();
private:
	/*******************************  帧缓冲访问  *********************************/
	inline RGBA* getFrameColor(int x, int y, int sample);
	inline float* getFrameDepth(int x, int y, int sample);
	inline void setFrameColor(int x, int y, const RGBA& color, int sample);
	/*******************************  辅助函数  *********************************/
	size_t clippingNewVertex(size_t idx0, size_t idx1, float t, bool postVertexProcess = false);
	void vertexShaderImpl(VertexHolder& vertex);
	void perspectiveDivideImpl(VertexHolder& vertex);
	void viewportTransformImpl(VertexHolder& vertex);
	int countFrustumClipMask(glm::aligned_vec4& clipPos);
	BoundingBox triangleBoundingBox(glm::aligned_vec4* vert, float width, float height);

	bool barycentric(glm::aligned_vec4* vert, glm::aligned_vec4& v0, glm::aligned_vec4& p, glm::aligned_vec4& bc);

private:
	//--------------------------------渲染状态-------------------------------------
	Viewport viewport_{};
	PrimitiveType primitiveType_ = Primitive_TRIANGLE;
	FrameBufferSoft* fbo_ = nullptr;
	const RenderStates* renderState_ = nullptr;
	VertexArrayObjectSoft* vao_ = nullptr;
	ShaderProgramSoft* shaderProgram_ = nullptr;
	//------------------------------帧缓冲资源--------------------------------
	std::shared_ptr<ImageBufferSoft<RGBA>> fboColor_ = nullptr;
	std::shared_ptr<ImageBufferSoft<float>> fboDepth_ = nullptr;
	//--------------------------- 临时数据存储-------------------------------
	std::vector<VertexHolder> vertexes_; // 处理中的顶点
	std::vector<PrimitiveHolder> primitives_; // 处理中的图元，其中包含顶点索引
	//----------------------------着色器变量存储----------------------------------
	std::shared_ptr<float> varyings_ = nullptr;// 存放所有顶点着色器输出给片段着色器的内容
	size_t varyingsCnt_ = 0;
	size_t varyingsAlignedCnt_ = 0;
	size_t varyingsAlignedSize_ = 0;
	//----------------------------渲染配置参数----------------------------
	float pointSize_ = 1.f;
	bool earlyZ_ = true;
	int rasterSamples_ = 1;
	int rasterBlockSize_ = 32;
	//---------------------------------并行处理--------------------------------------
	ThreadPool threadPool_;
	std::vector<PixelQuadContext> threadQuadCtx_;
};
}

#endif