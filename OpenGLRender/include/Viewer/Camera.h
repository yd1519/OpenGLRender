#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include "Base/GLMInc.h"
#include <glm/glm/gtc/matrix_transform.hpp>
#include "Base/Geometry.h"


enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 60.0f;
constexpr float CAMERA_NEAR = 0.01f;
constexpr float CAMERA_FAR = 100.f;
namespace OpenGL {

class Camera {
public:
    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(-1.5, 3, 3), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom_(ZOOM)
    {
        fov_ = glm::radians(ZOOM);
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom_(ZOOM)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp = glm::vec3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    inline void setReverseZ(bool reverse) {
        reverseZ_ = reverse;
    }

    // 实验中
    void reset() {
        Position = glm::vec3(-1.5f, 3.0f, 3.0f);
        Up = glm::vec3(0.0f, 1.0f, 0.0f);
        Front = glm::vec3(1.5f, -2.0f, -3.0f);
    }

    //设置透视矩阵参数
    void setPerspective(float Zoom, float aspect, float near, float far) {
        Zoom_ = Zoom;
        fov_ = glm::radians(Zoom);
        near_ = near;
        far_ = far;
        aspect_ = aspect;
    }

    void lookAt(const glm::vec3& position, const glm::vec3& front, const glm::vec3& up) {
        Position = position;
        Front = front;
        Up = up;
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 viewMatrix() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    //实际上是从投影空间转化到世界空间
    glm::vec3 getWorldPositionFromView(glm::vec3 pos) const {
        glm::mat4 proj, view, projInv, viewInv;
        proj = projectionMatrix();
        view = viewMatrix();

        projInv = glm::inverse(proj);
        viewInv = glm::inverse(view);

        glm::vec4 pos_world = viewInv * projInv * glm::vec4(pos, 1);
        pos_world /= pos_world.w;

        return glm::vec3{ pos_world };
    }

    //
    glm::mat4 projectionMatrix() const {
        float tanHalfFovInverse = 1.f / std::tan((fov_ * 0.5f));

        glm::mat4 projection(0.f);
        projection[0][0] = tanHalfFovInverse / aspect_;
        projection[1][1] = tanHalfFovInverse;

        if (reverseZ_) {
            projection[2][2] = 0.f;
            projection[2][3] = -1.f;
            projection[3][2] = near_;
        }
        else {
            projection[2][2] = -1.f;
            projection[2][3] = -1.f;
            projection[3][2] = -near_;
        }

        return projection;
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        if (direction == FORWARD)
            Position += Front * velocity;
        if (direction == BACKWARD)
            Position -= Front * velocity;
        if (direction == LEFT)
            Position -= Right * velocity;
        if (direction == RIGHT)
            Position += Right * velocity;
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom_ -= (float)yoffset;
        if (Zoom_ < 1.0f)
            Zoom_ = 1.0f;
        if (Zoom_ > 45.0f)
            Zoom_ = 45.0f;
        fov_ = glm::radians(Zoom_);
    }
    
    inline const Frustum& getFrustum() const { return frustum_; }

    inline const float& fov() const { return glm::radians(Zoom_); }
    inline const float& aspect() const { return aspect_; }
    inline const float& near() const { return near_; }
    inline const float& far() const { return far_; }

    inline const glm::vec3& eye() const { return Position; }
    inline const glm::vec3& up() const { return Up; }

    /*更新摄像机视锥体(Frustum)数据, 计算摄像机的6个裁剪平面、8个角点以及包围盒(AABB)*/
    void update() {
        //glm::vec3 forward(glm::normalize(center_ - eye_));
        glm::vec3 side(glm::normalize(cross(Front, Up)));
        glm::vec3 up(glm::cross(side, Front));

        float nearHeightHalf = near_ * std::tan(fov_ / 2.f);
        float farHeightHalf = far_ * std::tan(fov_ / 2.f);
        float nearWidthHalf = nearHeightHalf * aspect_;
        float farWidthHalf = farHeightHalf * aspect_;

        // near plane
        glm::vec3 nearCenter = Position + Front * near_;
        glm::vec3 nearNormal = Front;
        frustum_.planes[0].set(nearNormal, nearCenter);

        // far plane
        glm::vec3 farCenter = Position + Front * far_;
        glm::vec3 farNormal = -Front;
        frustum_.planes[1].set(farNormal, farCenter);

        // top plane
        glm::vec3 topCenter = nearCenter + up * nearHeightHalf;
        glm::vec3 topNormal = glm::cross(glm::normalize(topCenter - Position), side);
        frustum_.planes[2].set(topNormal, topCenter);

        // bottom plane
        glm::vec3 bottomCenter = nearCenter - up * nearHeightHalf;
        glm::vec3 bottomNormal = glm::cross(side, glm::normalize(bottomCenter - Position));
        frustum_.planes[3].set(bottomNormal, bottomCenter);

        // left plane
        glm::vec3 leftCenter = nearCenter - side * nearWidthHalf;
        glm::vec3 leftNormal = glm::cross(glm::normalize(leftCenter - Position), up);
        frustum_.planes[4].set(leftNormal, leftCenter);

        // right plane
        glm::vec3 rightCenter = nearCenter + side * nearWidthHalf;
        glm::vec3 rightNormal = glm::cross(up, glm::normalize(rightCenter - Position));
        frustum_.planes[5].set(rightNormal, rightCenter);

        // 8 corners
        glm::vec3 nearTopLeft = nearCenter + up * nearHeightHalf - side * nearWidthHalf;
        glm::vec3 nearTopRight = nearCenter + up * nearHeightHalf + side * nearWidthHalf;
        glm::vec3 nearBottomLeft = nearCenter - up * nearHeightHalf - side * nearWidthHalf;
        glm::vec3 nearBottomRight = nearCenter - up * nearHeightHalf + side * nearWidthHalf;

        glm::vec3 farTopLeft = farCenter + up * farHeightHalf - side * farWidthHalf;
        glm::vec3 farTopRight = farCenter + up * farHeightHalf + side * farWidthHalf;
        glm::vec3 farBottomLeft = farCenter - up * farHeightHalf - side * farWidthHalf;
        glm::vec3 farBottomRight = farCenter - up * farHeightHalf + side * farWidthHalf;

        frustum_.corners[0] = nearTopLeft;
        frustum_.corners[1] = nearTopRight;
        frustum_.corners[2] = nearBottomLeft;
        frustum_.corners[3] = nearBottomRight;
        frustum_.corners[4] = farTopLeft;
        frustum_.corners[5] = farTopRight;
        frustum_.corners[6] = farBottomLeft;
        frustum_.corners[7] = farBottomRight;

        // bounding box
        frustum_.bbox.min = glm::vec3(std::numeric_limits<float>::max());
        frustum_.bbox.max = glm::vec3(std::numeric_limits<float>::min());
        for (auto& corner : frustum_.corners) {
            frustum_.bbox.min.x = std::min(frustum_.bbox.min.x, corner.x);
            frustum_.bbox.min.y = std::min(frustum_.bbox.min.y, corner.y);
            frustum_.bbox.min.z = std::min(frustum_.bbox.min.z, corner.z);

            frustum_.bbox.max.x = std::max(frustum_.bbox.max.x, corner.x);
            frustum_.bbox.max.y = std::max(frustum_.bbox.max.y, corner.y);
            frustum_.bbox.max.z = std::max(frustum_.bbox.max.z, corner.z);
        }
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Up = glm::normalize(glm::cross(Right, Front));
    }

private:
	//Camera attributes
	glm::vec3 Position;
	glm::vec3 Front;
	glm::vec3 Up;
	glm::vec3 Right;
	glm::vec3 WorldUp;//全局基准上向量
	// euler Angles
	float Yaw;
	float Pitch;
	 // camera options
	float MovementSpeed;
	float MouseSensitivity;
	float Zoom_; // 角度制
    float fov_; // 弧度制

    //frustum atttributes
    float near_ = 0.01f;
    float far_ = 100.0f;
    float aspect_ = 1.0f;
    
    bool reverseZ_ = false;

    Frustum frustum_;
};
}


#endif
