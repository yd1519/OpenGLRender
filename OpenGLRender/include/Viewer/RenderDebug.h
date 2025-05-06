#ifndef RENDERDEBUG_H
#define RENDERDEBUG_H

#include "renderdoc/renderdoc_app.h"

namespace OpenGL {

/*RenderDoc 是一个流行的图形调试工具，
主要用于调试和分析 Direct3D、OpenGL 和 Vulkan 等图形 API 的渲染过程。
通过其 ​In-Application API，开发者可以在代码中直接控制帧的捕获和调试。
*/
// use RenderDoc In-application API to dump frame
// see: https://renderdoc.org/docs/in_application_api.html

class RenderDebugger {
public:
	static void startFrameCapture(RENDERDOC_DevicePointer device = nullptr);//开始捕获帧
	static void endFrameCapture(RENDERDOC_DevicePointer device = nullptr);//结束捕获

private:
	static RENDERDOC_API_1_0_0* getRenderDocApi();

private:
	static RENDERDOC_API_1_0_0* rdoc_;
};

}

#endif
