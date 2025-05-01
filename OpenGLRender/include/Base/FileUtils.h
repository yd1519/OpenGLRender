#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <fstream>
#include "Logger.h"

namespace OpenGL {

    class FileUtils {
    public:
        //检查文件是否存在
        static bool exists(const std::string& path) {
            std::ifstream file(path);//析构时自动关闭文件
            return file.good();
        }

        //读取二进制文件到内存
        static std::vector<uint8_t> readBytes(const std::string& path) {
            std::vector<uint8_t> ret;
            std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                LOGE("failed to open file: %s", path.c_str());
                return ret;//return 包含文件内容的字节数组（失败返回空数组）
            }

            size_t size = file.tellg();//tellg返回当前位置，即文件末尾=大小
            if (size <= 0) {
                LOGE("failed to read file, invalid size: %d", size);
                return ret;
            }

            ret.resize(size);// 预分配内存

            // 回到文件头并读取全部内容
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(ret.data()), (std::streamsize)size); //ret.data() 返回一个 ​​指向底层连续内存数组的指针​​，类型是 uint8_t*

            return ret;
        }

        //读取文本文件
        static std::string readText(const std::string& path) {
            auto data = readBytes(path);
            if (data.empty()) {
                return "";
            }

            return { (char*)data.data(), data.size() };
        }

        //------------------------------默认行为是覆盖写入​------------

        //写入二进制数据到文件
        static bool writeBytes(const std::string& path, const char* data, size_t length) {
            std::ofstream file(path, std::ios::out | std::ios::binary);
            if (!file.is_open()) {
                LOGE("failed to open file: %s", path.c_str());
                return false;
            }

            file.write(data, length);
            return true;
        }

        //写入文本到文件
        static bool writeText(const std::string& path, const std::string& str) {
            std::ofstream file(path, std::ios::out);
            if (!file.is_open()) {
                LOGE("failed to open file: %s", path.c_str());
                return false;
            }

            file.write(str.c_str(), str.length());
            file.close();

            return true;
        }
    };

}

#endif