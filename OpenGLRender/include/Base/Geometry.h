#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "GLMInc.h"

namespace OpenGL {
//定义了一个AABB框。
class BoundingBox {
public:
    BoundingBox() = default;
    BoundingBox(const glm::vec3& a, const glm::vec3& b) : min(a), max(b) {}

    // 获取边界框的八个顶点坐标
    void getCorners(glm::vec3* dst) const;

    // 对边界框进行矩阵变换，生成新的边界框
    BoundingBox transform(const glm::mat4& matrix) const;

    //判断当前边界框是否与另一个边界框相交。
    bool intersects(const BoundingBox& box) const;

    //将当前边界框与另一个边界框合并，生成一个新的边界框。
    void merge(const BoundingBox& box);

protected:
    // 更新 min 和 max 的值，确保它们包含 point
    static void updateMinMax(glm::vec3* point, glm::vec3* min, glm::vec3* max);

public:
    glm::vec3 min{ 0.f, 0.f, 0.f };//分别表示边界框的最小点和最大点
    glm::vec3 max{ 0.f, 0.f, 0.f };
};

//定义了一个平面
class Plane {
public:
    enum PlaneIntersects {//表示平面与几何体的相交状态
        Intersects_Cross = 0,
        Intersects_Tangent = 1,
        Intersects_Front = 2,//几何体 ​​全部​​ 位于平面法线指向的一侧
        Intersects_Back = 3
    };
    //通过法向量 n 和平面上的点 pt 设置平面。
    void set(const glm::vec3& n, const glm::vec3& pt) {
        normal_ = glm::normalize(n);
        d_ = -(glm::dot(normal_, pt));
    }

    //计算点 pt 到平面的距离。
    float distance(const glm::vec3& pt) const {
        // 点到平面的有符号距离 d = dot(normal_, p) + d_； 那么使得方程等于0的点就是平面上的点，那么距离也就是0
        return glm::dot(normal_, pt) + d_;
    }

    //获取平面的法向量。
    inline const glm::vec3& getNormal() const {
        return normal_;
    }

    //判断平面与边界框的相交情况。
    Plane::PlaneIntersects intersects(const BoundingBox& box) const;

    // 判断点是否在平面上 (世界坐标)
    Plane::PlaneIntersects intersects(const glm::vec3& p0) const;

    // 判断与线段是否相交 (世界坐标)
    Plane::PlaneIntersects intersects(const glm::vec3& p0, const glm::vec3& p1) const;

    // 判断与三角形是否相交 (世界坐标)
    Plane::PlaneIntersects intersects(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2) const;

private:
    // 方程：n.dot(p) + d = 0; 
    glm::vec3 normal_;
    float d_ = 0;//​​ 平面到原点的有符号距离​​
};


struct Frustum {
public:
    bool intersects(const BoundingBox& box) const;

    // check intersect with point (world space)
    bool intersects(const glm::vec3& p0) const;

    // check intersect with line segment (world space)
    bool intersects(const glm::vec3& p0, const glm::vec3& p1) const;

    // check intersect with triangle (world space)
    bool intersects(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2) const;

public:
    /**
     * planes[0]: near;
     * planes[1]: far;
     * planes[2]: top;
     * planes[3]: bottom;
     * planes[4]: left;
     * planes[5]: right;
     */
    Plane planes[6];

    /**
     * corners[0]: nearTopLeft;
     * corners[1]: nearTopRight;
     * corners[2]: nearBottomLeft;
     * corners[3]: nearBottomRight;
     * corners[4]: farTopLeft;
     * corners[5]: farTopRight;
     * corners[6]: farBottomLeft;
     * corners[7]: farBottomRight;
     */
    glm::vec3 corners[8];

    BoundingBox bbox;
};

//分别表示物体在视锥体的正/负 X、Y、Z 方向。
enum FrustumClipMask {
    POSITIVE_X = 1 << 0,
    NEGATIVE_X = 1 << 1,
    POSITIVE_Y = 1 << 2,
    NEGATIVE_Y = 1 << 3,
    POSITIVE_Z = 1 << 4,
    NEGATIVE_Z = 1 << 5,
};

const int FrustumClipMaskArray[6] = {
    FrustumClipMask::POSITIVE_X,//000001
    FrustumClipMask::NEGATIVE_X,//000010
    FrustumClipMask::POSITIVE_Y,//000100
    FrustumClipMask::NEGATIVE_Y,//001000
    FrustumClipMask::POSITIVE_Z,//010000
    FrustumClipMask::NEGATIVE_Z,//100000
};

//定义了视锥体的 6 个裁剪平面（glm::vec4 类型），平面方程为 Ax + By + Cz + D = 0。
const glm::vec4 FrustumClipPlane[6] = {
    {-1, 0, 0, 1},
    {1, 0, 0, 1},
    {0, -1, 0, 1},
    {0, 1, 0, 1},
    {0, 0, -1, 1},
    {0, 0, 1, 1}
};


}
#endif