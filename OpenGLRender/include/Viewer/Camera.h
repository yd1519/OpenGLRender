#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include "Base/GLMInc.h"
#include <glm/glm/gtc/matrix_transform.hpp>
#include "Base/Geometry.h"

namespace OpenGL {
    constexpr float CAMERA_FOV = 60.f;
    constexpr float CAMERA_NEAR = 0.01f;
    constexpr float CAMERA_FAR = 100.f;

class Camera {
public:
    void update();

    inline void setReverseZ(bool reverse) {
        reverseZ_ = reverse;
    }

    void setPerspective(float fov, float aspect, float near, float far);

    void lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

    glm::mat4 projectionMatrix() const;
    glm::mat4 viewMatrix() const;

    glm::vec3 getWorldPositionFromView(glm::vec3 pos) const;

    inline const Frustum& getFrustum() const { return frustum_; }

    inline const float& fov() const { return fov_; }
    inline const float& aspect() const { return aspect_; }
    inline const float& near() const { return near_; }
    inline const float& far() const { return far_; }

    inline const glm::vec3& eye() const { return eye_; }
    inline const glm::vec3& center() const { return center_; }
    inline const glm::vec3& up() const { return up_; }

private:
    float fov_ = glm::radians(60.f);
    float aspect_ = 1.0f;
    float near_ = 0.01f;
    float far_ = 100.f;

    bool reverseZ_ = false;

    glm::vec3 eye_{}; //摄像机的位置（世界坐标系）。
    glm::vec3 center_{};//摄像机看向的目标点（世界坐标系）。
    glm::vec3 up_{};//摄像机的上方向向量（世界坐标系）。

    Frustum frustum_; //摄像机的视锥体（Frustum 类型），用于视锥体裁剪。
};
}

#endif
