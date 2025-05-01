#ifndef SHADERPROGRAMOPENGL_H
#define SHADERPROGRAMOPENGL_H

#include "Render/ShaderProgram.h"

namespace OpenGL {

class ShaderProgramOpenGL : public ShaderProgram {
public:
	int getId() const override {
		return (int)programId_;
	}

	void addDefine(const std::string& def) override {
		programGLSL_.addDefine(def);
	}

	bool compileAndLink(const std::string& vsSource, const std::string& fsSource) {
		bool ret = programGLSL_.loadSource(vsSource, fsSource);
		programId_ = programGLSL_.getId();
		return ret;
	}

	bool compileAndLinkFile(const std::string& vsPath, const std::string& fsPath) {
		return compileAndLink(FileUtils::readText(vsPath), FileUtils::readText(fsPath));
	}

	inline void use() {
		programGLSL_.use();
		uniformBlockBinding_ = 0;
		uniformSamplerBinding_ = 0;
	}

	//每次调用都会返回一个​​新的、唯一的绑定点编号​
	inline int getUniformBlockBinding() {
		return uniformBlockBinding_++;
	}

	inline int getUniformSamplerBinding() {
		return uniformSamplerBinding_++;
	}
private:
	GLuint programId_ = 0;
	ProgramGLSL programGLSL_;

	//递增机制
	int uniformBlockBinding_ = 0;
	int uniformSamplerBinding_ = 0; // 用于纹理
};

}

#endif
