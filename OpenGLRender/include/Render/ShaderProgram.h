#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <unordered_map>
#include <string>
#include <set>
#include "Base/FileUtils.h"
#include "Render/OpenGL/GLSLUtils.h"
#include "Render/Uniform.h"

namespace OpenGL {

class ShaderProgram {
public:
	virtual int getId() const = 0;

	virtual void addDefine(const std::string& def) = 0;

	virtual void addDefines(const std::set<std::string>& defs) {
		for (auto& def : defs) {
			addDefine(def);
		}
	}

	//将所有的uniform块和uniformsampler块进行绑定
	virtual void bindResources(ShaderResources& resources) {
		for (auto& kv : resources.blocks) {
			bindUniform(*kv.second);
		}

		for (auto& kv : resources.samplers) {
			bindUniform(*kv.second);
		}
	}

protected:
	//绑定uniform
	virtual bool bindUniform(Uniform& uniform) {
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

protected:
	/*​键：Uniform 的哈希值（uniform.getHash()）第几个uniform
	值：Uniform 在着色器中的位置（location）*/
	std::unordered_map<int, int> uniformLocations_;
};
}

#endif