#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <unordered_map>
#include <string>
#include <set>
#include "FileUtils.h"
#include "GLSLUtils.h"
#include "Uniform.h"

namespace OpenGL {

class ShaderProgram {
public:
	int getId() const {
		return (int)programId_;
	}

	void addDefine(const std::string& def) {
		programGLSL_.addDefine(def);
	}

	void addDefines(const std::set<std::string>& defs) {
		for (auto& def : defs) {
			addDefine(def);
		}
	}

	//将所有的uniform块和uniformsampler块进行绑定
	void bindResources(ShaderResources& resources) {
		for (auto& kv : resources.blocks) {
			bindUniform(*kv.second);
		}

		for (auto& kv : resources.samplers) {
			bindUniform(*kv.second);
		}
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
	//绑定uniform块
	bool bindUniform(Uniform& uniform) {
		int hash = uniform.getHash();
		int location = -1;
		if (uniformLocations_.find(hash) == uniformLocations_.end()) {
			location = uniform.getLocation(*this);
			uniformLocations_[hash] = location;
		}
		else {
			location = uniformLocations_[hash];
		}

		if (location < 0) {
			return false;
		}
		
		uniform.bindProgram(*this, location);
		return true;
	}

private:
	/*​键：Uniform 的哈希值（uniform.getHash()）
	值：Uniform 在着色器中的位置（location）*/
	std::unordered_map<int, int> uniformLocations_;

	GLuint programId_ = 0;
	ProgramGLSL programGLSL_;

	//递增机制
	int uniformBlockBinding_ = 0;
	int uniformSamplerBinding_ = 0;
};



}


#endif