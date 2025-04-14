#ifndef GLSLUTILS_H
#define GLSLUTILS_H

#include <string>
#include <glad/glad.h>

namespace OpenGL {

    constexpr char const* OpenGL_GLSL_VERSION = "#version 330 core";
    constexpr char const* OpenGL_GLSL_DEFINE = "OpenGL";


    //用于管理单个着色器
    class ShaderGLSL {
    public:
        explicit ShaderGLSL(GLenum type) : type_(type) {
            header_ = OpenGL_GLSL_VERSION;
            header_ += "\n";
        };
        ~ShaderGLSL() { destroy(); }

        void setHeader(const std::string& header);
        void addDefines(const std::string& def);
        bool loadSource(const std::string& source); //加载 Shader 源代码并编译。
        bool loadFile(const std::string& path); //从文件加载 Shader 源代码并编译。
        void destroy();

        inline bool empty() const { return 0 == id_; };
        inline GLuint getId() const { return id_; };

    private:
        static std::string compatibleVertexPreprocess(const std::string& source);//对顶点着色器源代码进行预处理（兼容性处理）。
        static std::string compatibleFragmentPreprocess(const std::string& source); //对片段着色器源代码进行预处理（兼容性处理）。

    private:
        GLenum type_;//Shader 的类型（GLenum），例如 GL_VERTEX_SHADER 或 GL_FRAGMENT_SHADER。
        GLuint id_ = 0;//Shader 的 OpenGL 对象 ID。
        std::string header_; //Shader 的头信息（如版本号和自定义头）
        std::string defines_;//Shader 的宏定义
    };


    //将多个 Shader 链接成一个完整的程序
    class ProgramGLSL {
    public:
        ProgramGLSL() { addDefine(OpenGL_GLSL_DEFINE); }
        ~ProgramGLSL() { destroy(); }

        void addDefine(const std::string& def);
        bool loadSource(const std::string& vsSource, const std::string& fsSource); //加载顶点着色器和片段着色器的源代码，并链接成 Shader Program。最终都会调用loadshader
        bool loadFile(const std::string& vsPath, const std::string& fsPath); //从文件加载顶点着色器和片段着色器的源代码，并链接成 Shader Program。
        void use() const;//启用当前的 Shader Program。
        void destroy();

        inline bool empty() const { return 0 == id_; };
        inline GLuint getId() const { return id_; };

    private:
        bool loadShader(ShaderGLSL& vs, ShaderGLSL& fs); //加载并链接顶点着色器和片段着色器。

    private:
        GLuint id_ = 0;
        std::string defines_;
    };

}




#endif