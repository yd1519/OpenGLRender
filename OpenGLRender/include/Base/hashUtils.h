#ifndef HASHUTILS_H
#define HASHUTILS_H

#include <string>
#include <md5/md5.h>

namespace OpenGL {
class HashUtils {
public:
    template<class T>
    inline static void hashCombine(size_t& seed, T const& v) {//用于组合多个值的哈希，如结构体的哈希计算
        seed ^= std::hash<T>()(v) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    }
    //Murmur3 哈希算法实现
    inline static uint32_t murmur3(const uint32_t* key, size_t wordCount, uint32_t seed) noexcept {
        uint32_t h = seed;
        size_t i = wordCount;
        do {
            uint32_t k = *key++;
            k *= 0xcc9e2d51u;
            k = (k << 15u) | (k >> 17u);
            k *= 0x1b873593u;
            h ^= k;
            h = (h << 13u) | (h >> 19u);
            h = (h * 5u) + 0xe6546b64u;
        } while (--i);
        h ^= wordCount;
        h ^= h >> 16u;
        h *= 0x85ebca6bu;
        h ^= h >> 13u;
        h *= 0xc2b2ae35u;
        h ^= h >> 16u;
        return h;
    }
    // 使用Murmur3的哈希组合函数
    template<typename T>
    inline static void hashCombineMurmur(size_t& seed, const T& key) {

        static_assert(0 == (sizeof(key) & 3u), "Hashing requires a size that is a multiple of 4.");
        uint32_t keyHash = HashUtils::murmur3((const uint32_t*)&key, sizeof(key) / 4, 0);
        seed ^= keyHash + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    }
    // MD5哈希计算函数
    inline static std::string getHashMD5(const char* data, size_t length) {
        unsigned char digest[17] = { 0 };

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, (unsigned char*)data, length);
        MD5_Final(digest, &ctx);

        char str[33] = { 0 };
        hexToStr(str, digest, 16);
        return { str };
    }

    inline static std::string getHashMD5(const std::string& text) {
        return getHashMD5(text.c_str(), text.length());
    }

private:
    // 十六进制转字符串辅助函数
    static void hexToStr(char* str, const unsigned char* digest, int length) {
        uint8_t hexDigit;
        for (int i = 0; i < length; i++) {
            hexDigit = (digest[i] >> 4) & 0xF;
            str[i * 2] = (hexDigit <= 9) ? (hexDigit + '0') : (hexDigit + 'a' - 10);
            hexDigit = digest[i] & 0xF;
            str[i * 2 + 1] = (hexDigit <= 9) ? (hexDigit + '0') : (hexDigit + 'a' - 10);
        }
    }
};
}

#endif
