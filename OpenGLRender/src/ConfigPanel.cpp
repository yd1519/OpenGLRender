#include "Viewer/configPanel.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "json11/json11.hpp"
#include "Base/Logger.h"
#include "Base/FileUtils.h"


namespace OpenGL {

bool ConfigPanel::init(void* window, int width, int height) {
    // 记录面板尺寸
    frameWidth_ = width;
    frameHeight_ = height;

    // 初始化ImGui上下文
    IMGUI_CHECKVERSION(); //检查ImGui版本兼容性(防止头文件和库版本不匹配)
    ImGui::CreateContext();/*创建ImGui上下文，这个上下文包含所有ImGui的状态和数据， 每个上下文都是独立的，可以实现多窗口不同风格的ImGui*/

    // 配置io参数
    ImGuiIO& io = ImGui::GetIO(); //获取ImGui的IO对象，用于配置各种输入输出参数
    io.IniFilename = nullptr;// 禁用自动保存布局到ini文件

    // 设置ImGui样式
    ImGui::StyleColorsDark(); // 暗色主题，还有light和classic
    ImGuiStyle* style = &ImGui::GetStyle(); // 获取样式对象指针以便自定义
    style->Alpha = 0.8f; //设置全局透明度(0.0完全透明，1.0完全不透明)

    // 初始化平台/渲染后端
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window, true); // 初始化GLFW后端绑定 true表示让ImGui安装自己的GLFW回调函数
    ImGui_ImplOpenGL3_Init("#version 330 core"); //初始化OpenGL3后端，必须与着色器版本匹配

    // load config
    return loadConfig();
}

// 绘制ImGui配置面板的主入口函数
void ConfigPanel::onDraw() {
    // ------------------------------开始新帧----------------------
    ImGui_ImplOpenGL3_NewFrame();// 准备OpenGL3渲染后端的新帧
    ImGui_ImplGlfw_NewFrame();// 准备GLFW平台的新帧（处理输入事件等）
    ImGui::NewFrame();// 开始新的ImGui帧（重置绘制命令等）

    // -----------------------------创建设置窗口-----------------------------------
    ImGui::Begin("Settings",
        nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_AlwaysAutoResize);
    //------------------------------绘制具体设置项-------------------------
    drawSettings();
    ImGui::SetWindowPos(ImVec2(frameWidth_ - ImGui::GetWindowWidth(), 0));// 固定窗口到右上角
    ImGui::End();// 结束设置窗口

    ImGui::Render();// 生成顶点/索引缓冲等渲染数据
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); //执行OpenGL渲染
}

// 绘制配置面板的设置界面
void ConfigPanel::drawSettings() {
    //-----------------------------渲染器类型设置---------------------------------
    const char* rendererItems[] = {"Software", "OpenGL"}; 
    ImGui::Separator();// 添加分隔线
    ImGui::Text("renderer");
    for (int i = 0; i < 2; i++) {// 渲染器类型单选按钮组
        if (ImGui::RadioButton(rendererItems[i], config_.rendererType == i)) {
            config_.rendererType = i;
        }
        ImGui::SameLine();// 保持按钮在同一行
    }
    ImGui::Separator();

    // --------------------------------相机控制---------------------------
    ImGui::Separator();
    ImGui::Text("camera:");
    ImGui::SameLine();
    if (ImGui::SmallButton("reset")) {
        if (resetCameraFunc_) {
            resetCameraFunc_();
        }
    }

    //----------------------------------调试工具-----------------------------
    ImGui::Separator();
    ImGui::Text("debug (RenderDoc):");
    ImGui::SameLine();
    if (ImGui::SmallButton("capture")) {
        if (frameDumpFunc_) {
            frameDumpFunc_();
        }
    }

    // ----------------------------------性能信息-----------------------------------------
    ImGui::Separator();
    ImGui::Text("fps: %.1f (%.2f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text("triangles: %zu", config_.triangleCount_);

    // ---------------------------------模型加载--------------------------------------------
    ImGui::Separator();
    ImGui::Text("load model");

    int modelIdx = 0;
    for (; modelIdx < modelNames_.size(); modelIdx++) {// 查找当前选中模型的索引
        if (config_.modelName == modelNames_[modelIdx]) {
            break;
        }
    }
    //模型选择下拉框（使用##隐藏标签）
    if (ImGui::Combo("##load model", &modelIdx, modelNames_.data(), (int)modelNames_.size())) {
        reloadModel(modelNames_[modelIdx]);// 当选择变化时重新加载模型
    }

    // -------------------------------------天空盒设置-----------------------------------------
    ImGui::Separator();
    ImGui::Checkbox("load skybox", &config_.showSkybox);// 天空盒启用复选框（同时控制后续选项的显示）

    if (config_.showSkybox) {
        // pbr ibl
        ImGui::Checkbox("enable IBL", &config_.pbrIbl);

        // 查找当前天空盒索引
        int skyboxIdx = 0;
        for (; skyboxIdx < skyboxNames_.size(); skyboxIdx++) {
            if (config_.skyboxName == skyboxNames_[skyboxIdx]) {
                break;
            }
        }
        if (ImGui::Combo("##skybox", &skyboxIdx, skyboxNames_.data(), (int)skyboxNames_.size())) {
            reloadSkybox(skyboxNames_[skyboxIdx]);
        }
    }

    // -------------------------------渲染参数--------------------------
    ImGui::Separator();
    ImGui::Text("clear color");
    ImGui::ColorEdit4("clear color", (float*)&config_.clearColor, ImGuiColorEditFlags_NoLabel);

    // 线框模式切换
    ImGui::Separator();
    ImGui::Checkbox("wireframe", &config_.wireframe);

    // 世界坐标轴显示
    ImGui::Separator();
    ImGui::Checkbox("world axis", &config_.worldAxis);

    // 阴影地面显示（自动关联阴影贴图）
    ImGui::Separator();
    ImGui::Checkbox("shadow floor", &config_.showFloor);
    config_.shadowMap = config_.showFloor;// 自动同步状态

    //-------------------------------------------光照设置-------------------------
    if (!config_.wireframe) {
        // light
        ImGui::Separator();
        ImGui::Text("ambient color");
        ImGui::ColorEdit3("ambient color", (float*)&config_.ambientColor, ImGuiColorEditFlags_NoLabel);// 环境光颜色选择

        ImGui::Separator();
        ImGui::Checkbox("point light", &config_.showLight);// 点光源开关
        if (config_.showLight) {
            ImGui::Text("light color");
            ImGui::ColorEdit3("light color", (float*)&config_.pointLightColor, ImGuiColorEditFlags_NoLabel);

            ImGui::Text("light position");
            ImGui::SliderAngle("##light position", &lightPositionAngle_, 0, 360.f);
        }

        // mipmaps
        ImGui::Separator();
        if (ImGui::Checkbox("mipmaps", &config_.mipmaps)) {
            if (resetMipmapsFunc_) {
                resetMipmapsFunc_();
            }
        }
    }

    // face cull
    ImGui::Separator();
    ImGui::Checkbox("cull face", &config_.cullFace);

    // depth test
    ImGui::Separator();
    ImGui::Checkbox("depth test", &config_.depthTest);

    // reverse z
    ImGui::Separator();
    if (ImGui::Checkbox("reverse z", &config_.reverseZ)) {
        if (resetReverseZFunc_) {
            resetReverseZFunc_();
        }
    }

    // Anti aliasing
    const char* aaItems[] = {
        "NONE",
        "MSAA",
        "FXAA",
    };
    ImGui::Separator();
    ImGui::Text("Anti-aliasing");
    for (int i = 0; i < 3; i++) {
        if (ImGui::RadioButton(aaItems[i], config_.aaType == i)) {
            config_.aaType = i;
        }
        ImGui::SameLine();
    }
}

/*此函数负责清理ImGui相关的所有资源，包括：
*-OpenGL渲染后端资源
* -GLFW平台集成资源
* -ImGui上下文及其所有内部状态*/
void ConfigPanel::destroy() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

//更新配置面板的状态（每帧调用），更新点光源位置（圆周运动）
void ConfigPanel::update() {
    // update light position
    config_.pointLightPosition = 2.f * glm::vec3(glm::sin(lightPositionAngle_), 1.2f, glm::cos(lightPositionAngle_));
    if (updateLightFunc_) {
        updateLightFunc_(config_.pointLightPosition, config_.pointLightColor);
    }
}

// 更新面板尺寸
void ConfigPanel::updateSize(int width, int height) {
    frameWidth_ = width;
    frameHeight_ = height;
}

// 用于查询ImGui是否需要处理键盘输入
bool ConfigPanel::wantCaptureKeyboard() {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;// 返回当前键盘捕获状态
}

// 检测ImGui是否需要处理鼠标输入
bool ConfigPanel::wantCaptureMouse() {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;// 返回当前鼠标捕获状态
}

//加载并解析配置文件，初始化模型和天空盒资源路径
bool ConfigPanel::loadConfig() {
    auto configPath = ASSETS_DIR + "assets.json"; //构建完整配置文件路径
    //读取配置文件内容
    auto configStr = FileUtils::readText(configPath);
    if (configStr.empty()) {
        LOGE("load models failed: error read config file");
        return false;
    }

    // 使用json11解析JSON配置
    std::string err;
    const auto json = json11::Json::parse(configStr, err);
    for (auto& kv : json["model"].object_items()) {// 解析模型路径
        modelPaths_[kv.first] = ASSETS_DIR + kv.second["path"].string_value();
    }
    for (auto& kv : json["skybox"].object_items()) {// 解析天空盒路径
        skyboxPaths_[kv.first] = ASSETS_DIR + kv.second["path"].string_value();
    }

    if (modelPaths_.empty()) {// 验证至少加载了一个模型
        LOGE("load models failed: %s", err.c_str());
        return false;
    }

    //准备ImGui下拉菜单需要的数据
    for (const auto& kv : modelPaths_) {
        modelNames_.emplace_back(kv.first.c_str());
    }
    for (const auto& kv : skyboxPaths_) {
        skyboxNames_.emplace_back(kv.first.c_str());
    }

    // 加载第一个模型和天空盒作为默认资源
    return reloadModel(modelPaths_.begin()->first) && reloadSkybox(skyboxPaths_.begin()->first);
}

// 重新加载指定模型
bool ConfigPanel::reloadModel(const std::string& name) {
    if (name != config_.modelName) {// 检查模型名称是否实际发生变化
        config_.modelName = name;
        config_.modelPath = modelPaths_[config_.modelName];

        if (reloadModelFunc_) {
            return reloadModelFunc_(config_.modelPath);
        }
    }

    return true;
}

// 重新加载指定天空盒
bool ConfigPanel::reloadSkybox(const std::string& name) {
    if (name != config_.skyboxName) {
        config_.skyboxName = name;
        config_.skyboxPath = skyboxPaths_[config_.skyboxName];

        if (reloadSkyboxFunc_) {
            return reloadSkyboxFunc_(config_.skyboxPath);
        }
    }

    return true;
}

}