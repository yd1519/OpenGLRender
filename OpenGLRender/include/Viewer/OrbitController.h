#ifndef ORBITCONTROLLER_H
#define ORBITCONTROLLER_H

#include <memory>
#include "Viewer/Camera.h"

namespace OpenGL {
//实现基于轨道（Orbit）模式的相机控制器，支持平移、旋转和缩放操作
class OrbitController {
public:
    explicit OrbitController(Camera& camera);

    void update();

    void panByPixels(double dx, double dy);

    void rotateByPixels(double dx, double dy);

    void zoomByPixels(double dx, double dy);

    void reset();

private:
    Camera& camera_;

    glm::vec3 eye_{};
    glm::vec3 center_{};
    glm::vec3 up_{};

    float armLength_ = 0.f;
    glm::vec3 armDir_{};

    float panSensitivity_ = 0.1f;
    float zoomSensitivity_ = 0.2f;
    float rotateSensitivity_ = 0.2f;
};

//为OrbitController添加平滑过渡效果的封装类
class SmoothOrbitController {
public:
    explicit SmoothOrbitController(std::shared_ptr<OrbitController> orbit_controller)
        : orbitController_(std::move(orbit_controller)) {}

    void update() {
        if (std::abs(zoomX) > motionEps || std::abs(zoomY) > motionEps) {
            zoomX /= motionSensitivity;
            zoomY /= motionSensitivity;
            orbitController_->zoomByPixels(zoomX, zoomY);
        }
        else {
            zoomX = 0;
            zoomY = 0;
        }

        if (std::abs(rotateX) > motionEps || std::abs(rotateY) > motionEps) {
            rotateX /= motionSensitivity;
            rotateY /= motionSensitivity;
            orbitController_->rotateByPixels(rotateX, rotateY);
        }
        else {
            rotateX = 0;
            rotateY = 0;
        }

        if (std::abs(panX) > motionEps || std::abs(panY) > motionEps) {
            orbitController_->panByPixels(panX, panY);
            panX = 0;
            panY = 0;
        }

        orbitController_->update();
    }

    inline void reset() {
        orbitController_->reset();
    }

    double zoomX = 0;
    double zoomY = 0;
    double rotateX = 0;
    double rotateY = 0;
    double panX = 0;
    double panY = 0;

private:
    const double motionEps = 0.001f;
    const double motionSensitivity = 1.2f;
    std::shared_ptr<OrbitController> orbitController_;
};
}
#endif
