#ifndef LOGGER_H
#define LOGGER_H

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <mutex>

namespace OpenGL {

#define LOGI(...) OpenGL::Logger::log(OpenGL::LOG_INFO,     __FILE__, __LINE__, __VA_ARGS__)
#define LOGD(...) OpenGL::Logger::log(OpenGL::LOG_DEBUG,    __FILE__, __LINE__, __VA_ARGS__)
#define LOGW(...) OpenGL::Logger::log(OpenGL::LOG_WARNING,  __FILE__, __LINE__, __VA_ARGS__)
#define LOGE(...) OpenGL::Logger::log(OpenGL::LOG_ERROR,    __FILE__, __LINE__, __VA_ARGS__)

static constexpr int MAX_LOG_LENGTH = 1024;

typedef void (*LogFunc)(void* context, int level, const char* msg);

enum LogLevel {
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARNING,
	LOG_ERROR,
};

class Logger {
public:
	static void setLogFunc(void* ctx, LogFunc func);
	static void setLogLevel(LogLevel level);
	static void log(LogLevel level, const char* file, int line, const char* message, ...);

private:
	static void* logContext_;
	static LogFunc logFunc_;
	static LogLevel minLevel_;

	static char buf_[MAX_LOG_LENGTH];
	static std::mutex mutex_;
};

}

#endif