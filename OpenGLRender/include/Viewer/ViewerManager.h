#ifndef VIEWERMANAGER_H
#define VIEWERMANAGER_H

#include "Viewer/Camera.h"
#include "Render/Renderer.h"
#include "Viewer/ConfigPanel.h"
#include "Viewer/ModelLoader.h"
#include "Viewer/RenderDebug.h"
#include "Viewer/ViewerOpenGL.h"
#include "Viewer/ViewerSoftware.h"
#include "Viewer/OrbitController.h"

namespace OpenGL {

#define RENDER_TYPE_NONE (-1)

class ViewerManager {
public:
	bool create(void* window, int width, int height, int outTexId) {
		window_ = window;
		width_ = width;
		height_ = height;
		outTexId_ = outTexId;

		// camera		
		camera_ = std::make_shared<Camera>();
		camera_->setPerspective(glm::radians(CAMERA_FOV), (float)width / (float)height, CAMERA_NEAR, CAMERA_FAR);

		orbitController_ = std::make_shared<SmoothOrbitController>(std::make_shared<OrbitController>(*camera_));


		// config
		config_ = std::make_shared<Config>();
		configPanel_ = std::make_shared<ConfigPanel>(*config_);

		//viewer software
		auto viewer_soft = std::make_shared<ViewerSoftware>(*config_, *camera_);
		viewers_[Renderer_SOFT] = viewer_soft;

		//viewer opengl
		auto viewer_opengl = std::make_shared<ViewerOpenGL>(*config_, *camera_);
		viewers_[Renderer_OPENGL] = viewer_opengl;

		// modelloader
		modelLoader_ = std::make_shared<ModelLoader>(*config_);

		// 设置控制面板的各种按钮的回调函数
		setupConfigPanelActions();

		// 初始化控制面板
		return configPanel_->init(window, width, height);
	}

	//设置控制面板的各种按钮的回调函数
	void setupConfigPanelActions() {
		configPanel_->setResetCameraFunc([&]() -> void { orbitController_->reset(); });
		configPanel_->setResetMipmapsFunc([&]() -> void {
			waitRenderIdle();
			modelLoader_->getScene().model->resetStates(); 
		});
		// 该函数将阴影贴图纹理设置nullptr
		configPanel_->setResetReverseZFunc([&]() ->void {
			waitRenderIdle();
			auto& viewer = viewers_[config_->rendererType];
			viewer->resetReverseZ();
		});

		configPanel_->setReloadModelFunc([&](const std::string& path) -> bool {
			waitRenderIdle();
			return modelLoader_->loadModel(path);
		});

		configPanel_->setReloadSkyboxFunc([&](const std::string& path) -> bool {
			waitRenderIdle();
			return modelLoader_->loadSkybox(path);
		});

		configPanel_->setFrameDumpFunc([&]() -> void {dumpFrame_ = true; });// renderdoc

		configPanel_->setUpdateLightFunc([&](glm::vec3& position, glm::vec3& color) -> void {
			auto& scene = modelLoader_->getScene();
			scene.pointLight.vertexes[0].a_position = position;
			scene.pointLight.UpdateVertexes(); // 更新vao所绑定的数据
			scene.pointLight.material->baseColor = glm::vec4(color, 1.0f);
		});
	}

	int drawFrame() {
		orbitController_->update();
		camera_->update();
		configPanel_->update();

		config_->triangleCount_ = modelLoader_->getModelPrimitiveCnt();

		auto& viewer = viewers_[config_->rendererType];
		if (rendererType_ != config_->rendererType) {
			resetStates();
			rendererType_ = config_->rendererType;
			viewer->create(width_, height_, outTexId_);
		}
		viewer->configRenderer(); // 将camera和cameradepth的reverse_z设置为false；
		if (dumpFrame_) {// 帧捕获模式开启
			RenderDebugger::startFrameCapture(viewer->getDevicePointer(window_)); // 开始捕获GPU指令
		}

		viewer->drawFrame(modelLoader_->getScene());
		//结束帧捕获
		if (dumpFrame_) {
			dumpFrame_ = false;
			RenderDebugger::endFrameCapture(viewer->getDevicePointer(window_));// 结束捕获并保存数据
		}

		return viewer->swapBuffer();
	}

	inline void destroy() {
		resetStates();
		for (auto& it : viewers_) {
			it.second->destroy();
		}
	}

	inline void waitRenderIdle() {
		if (rendererType_ != RENDER_TYPE_NONE) {
			viewers_[rendererType_]->waitRenderIdle();
		}
	}

	// 切换渲染器的时候调用
	inline void resetStates() {
		waitRenderIdle(); //同步GPU，确保所有正在执行的渲染命令完成
		modelLoader_->resetAllModelStates();
		modelLoader_->getScene().resetStates();
	}

	// 绘制控制面板
	inline void drawPanel() {
		if (showConfigPanel_) {
			configPanel_->onDraw();
		}
	}

	// 切换控制面板的显示状态
	inline void togglePanelState() {
		showConfigPanel_ = !showConfigPanel_;
	}

	inline void updateSize(int width, int height) {
		width_ = width;
		height_ = height;
		configPanel_->updateSize(width, height);
	}

	inline void updateGestureZoom(float x, float y) {
		orbitController_->zoomX = x;
		orbitController_->zoomY = y;
	}

	inline void updateGestureRotate(float x, float y) {
		orbitController_->rotateX = x;
		orbitController_->rotateY = y;
	}

	inline void updateGesturePan(float x, float y) {
		orbitController_->panX = x;
		orbitController_->panY = y;
	}

	inline bool wantCaptureKeyboard() {
		return configPanel_->wantCaptureKeyboard();
	}

	inline bool wantCaptureMouse() {
		return configPanel_->wantCaptureMouse();
	}


private:
	void* window_ = nullptr;
	int width_ = 0;
	int height_ = 0;
	int outTexId_ = 0;

	std::shared_ptr<Config> config_;
	std::shared_ptr<ConfigPanel> configPanel_;
	std::shared_ptr<Camera> camera_;
	std::shared_ptr<SmoothOrbitController> orbitController_;
	std::shared_ptr<ModelLoader> modelLoader_;
	
	std::unordered_map<int, std::shared_ptr<Viewer>> viewers_;

	int rendererType_ = RENDER_TYPE_NONE;
	bool showConfigPanel_ = true;
	bool dumpFrame_ = false; //是否触发帧截图
};

}

#endif
