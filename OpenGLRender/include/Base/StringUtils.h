#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <string>

namespace OpenGL {
	class StringUtils {
	public:
		// 检查字符串是否以指定后缀结尾
		inline static bool endsWith(const std::string& str, const std::string& suffix) {
			return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
		}
		// 检查字符串是否以指定前缀开头
		inline static bool startsWith(const std::string& str, const std::string& prefix) {
			return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
		}
	};
}
#endif
