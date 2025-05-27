#ifndef QUADFILTER_H
#define QUADFILTER_H

#include <memory>
#include <functional>
#include "Viewer/Model.h"
#include "Render/Renderer.h"


namespace OpenGL {
//用于实现基于屏幕空间四边形(Quad)的后处理
class QuadFilter {
public:
	QuadFilter(int width, int height, const std::shared_ptr<Renderer>& renderer,
			   const std::function<bool(ShaderProgram& program)>& shaderFunc);

	void setTextures(std::shared_ptr<Texture>& texIn, std::shared_ptr<Texture>& texOut);

	void draw();

private:
	int width_ = 0;
	int height_ = 0;
	bool initReady_ = false; // 初始化完成标志

	ModelMesh quadMesh_;    // 全屏四边形网格
	std::shared_ptr<Renderer> renderer_; // 渲染器
	std::shared_ptr<FrameBuffer> fbo_;  

	/*struct UniformsQuadFilter {
		glm::vec2 u_screenSize;
	};*/
	UniformsQuadFilter uniformFilter_{}; // cpu端uniform数据
	std::shared_ptr<UniformBlock> uniformBlockFilter_; //uniform资源块
	std::shared_ptr<UniformSampler> uniformTexIn_; // 需要处理的纹理
};
}

#endif