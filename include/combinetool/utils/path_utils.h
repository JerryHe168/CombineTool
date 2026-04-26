#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace combinetool {
namespace utils {

class PathUtils {
public:
    static std::string getCurrentDirectory();
    static std::string getAbsolutePath(const std::string& path);
    static std::string getRelativePath(const std::string& base, const std::string& path);
    
    static std::string getDirectoryName(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getFileNameWithoutExtension(const std::string& path);
    static std::string getExtension(const std::string& path);
    
    static std::string combine(const std::string& path1, const std::string& path2);
    static std::string combine(const std::vector<std::string>& paths);
    
    static bool exists(const std::string& path);
    static bool isFile(const std::string& path);
    static bool isDirectory(const std::string& path);
    
    static uint64_t getFileSize(const std::string& path);
    
    static bool createDirectory(const std::string& path);
    static bool createDirectories(const std::string& path);
    
    static std::vector<std::string> listFiles(
        const std::string& directory,
        const std::string& pattern = "*",
        bool recursive = false
    );
    
    static std::vector<std::string> listDirectories(
        const std::string& directory,
        bool recursive = false
    );
    
    static bool isAbsolutePath(const std::string& path);
    static std::string normalizePath(const std::string& path);
    
    static char getDirectorySeparator();
    static std::string getTempDirectory();
    static std::string getTempFileName();
};

}
}
