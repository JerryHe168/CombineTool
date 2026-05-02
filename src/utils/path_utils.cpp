#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/string_utils.h"

#include <climits>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#endif

namespace combinetool {
namespace utils {

#ifdef _WIN32
static std::string fromWidePath(const std::wstring& wpath) {
    return StringUtils::fromWideString(wpath);
}

static std::wstring toWidePath(const std::string& path) {
    return StringUtils::toWideString(path);
}
#endif

std::string PathUtils::getCurrentDirectory() {
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH> buffer;
    DWORD size = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (size == 0 || size > buffer.size()) {
        return "";
    }
    return fromWidePath(std::wstring(buffer.data(), size));
#else
    std::array<char, PATH_MAX> buffer;
    if (getcwd(buffer.data(), buffer.size()) == nullptr) {
        return "";
    }
    return std::string(buffer.data());
#endif
}

std::string PathUtils::getAbsolutePath(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    std::array<wchar_t, MAX_PATH> buffer;
    if (_wfullpath(buffer.data(), wpath.c_str(), static_cast<size_t>(buffer.size())) == nullptr) {
        return "";
    }
    return fromWidePath(std::wstring(buffer.data()));
#else
    std::array<char, PATH_MAX> buffer;
    if (realpath(path.c_str(), buffer.data()) == nullptr) {
        return "";
    }
    return std::string(buffer.data());
#endif
}

std::string PathUtils::getRelativePath(const std::string& base, const std::string& path) {
    std::string absBase = normalizePath(getAbsolutePath(base));
    std::string absPath = normalizePath(getAbsolutePath(path));
    
    if (absBase.empty() || absPath.empty()) {
        return path;
    }
    
    char sep = getDirectorySeparator();
    
    if (!StringUtils::endsWith(absBase, std::string(1, sep))) {
        absBase += sep;
    }
    
    size_t commonLen = 0;
    size_t lastSep = 0;
    
    while (commonLen < absBase.size() && commonLen < absPath.size()) {
        if (absBase[commonLen] != absPath[commonLen]) {
            break;
        }
        if (absBase[commonLen] == sep) {
            lastSep = commonLen;
        }
        ++commonLen;
    }
    
    if (commonLen == absBase.size() || commonLen > lastSep) {
        lastSep = commonLen;
    }
    
    if (lastSep == 0) {
        return absPath;
    }
    
    size_t baseRemaining = std::count(
        absBase.begin() + lastSep, absBase.end(), sep
    );
    
    std::string result;
    for (size_t i = 0; i < baseRemaining; ++i) {
        result += "..";
        result += sep;
    }
    
    if (lastSep < absPath.size()) {
        result += absPath.substr(lastSep + (absPath[lastSep] == sep ? 1 : 0));
    }
    
    return result;
}

std::string PathUtils::getDirectoryName(const std::string& path) {
    char sep = getDirectorySeparator();
    size_t lastSep = path.find_last_of("\\/");
    
    if (lastSep == std::string::npos) {
        return "";
    }
    
    if (lastSep == 0) {
        return std::string(1, sep);
    }
    
    return path.substr(0, lastSep);
}

std::string PathUtils::getFileName(const std::string& path) {
    size_t lastSep = path.find_last_of("\\/");
    if (lastSep == std::string::npos) {
        return path;
    }
    return path.substr(lastSep + 1);
}

std::string PathUtils::getFileNameWithoutExtension(const std::string& path) {
    std::string filename = getFileName(path);
    size_t lastDot = filename.find_last_of('.');
    
    if (lastDot == std::string::npos || lastDot == 0) {
        return filename;
    }
    
    return filename.substr(0, lastDot);
}

std::string PathUtils::getExtension(const std::string& path) {
    std::string filename = getFileName(path);
    size_t lastDot = filename.find_last_of('.');
    
    if (lastDot == std::string::npos || lastDot == filename.size() - 1) {
        return "";
    }
    
    return filename.substr(lastDot);
}

std::string PathUtils::combine(const std::string& path1, const std::string& path2) {
    if (path1.empty()) {
        return path2;
    }
    if (path2.empty()) {
        return path1;
    }
    
    if (isAbsolutePath(path2)) {
        return path2;
    }
    
    char sep = getDirectorySeparator();
    std::string result = path1;
    
    if (!StringUtils::endsWith(path1, std::string(1, sep)) &&
        !StringUtils::endsWith(path1, "/") &&
        !StringUtils::endsWith(path1, "\\")) {
        result += sep;
    }
    
    if (StringUtils::startsWith(path2, std::string(1, sep)) ||
        StringUtils::startsWith(path2, "/") ||
        StringUtils::startsWith(path2, "\\")) {
        result += path2.substr(1);
    } else {
        result += path2;
    }
    
    return normalizePath(result);
}

std::string PathUtils::combine(const std::vector<std::string>& paths) {
    if (paths.empty()) {
        return "";
    }
    
    std::string result = paths[0];
    for (size_t i = 1; i < paths.size(); ++i) {
        result = combine(result, paths[i]);
    }
    return result;
}

bool PathUtils::exists(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    return GetFileAttributesW(wpath.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
#endif
}

bool PathUtils::isFile(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISREG(buffer.st_mode);
#endif
}

bool PathUtils::isDirectory(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
#endif
}

uint64_t PathUtils::getFileSize(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fad)) {
        return 0;
    }
    ULARGE_INTEGER size;
    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    return static_cast<uint64_t>(size.QuadPart);
#else
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(buffer.st_size);
#endif
}

bool PathUtils::createDirectory(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = toWidePath(path);
    return CreateDirectoryW(wpath.c_str(), nullptr) != FALSE ||
           GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool PathUtils::createDirectories(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    
    if (exists(path)) {
        return isDirectory(path);
    }
    
    std::string parent = getDirectoryName(path);
    if (!parent.empty() && !exists(parent)) {
        if (!createDirectories(parent)) {
            return false;
        }
    }
    
    return createDirectory(path);
}

std::vector<std::string> PathUtils::listFiles(
    const std::string& directory,
    const std::string& pattern,
    bool recursive
) {
    std::vector<std::string> result;
    
    if (!isDirectory(directory)) {
        return result;
    }
    
#ifdef _WIN32
    std::wstring searchPath = toWidePath(combine(directory, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return result;
    }
    
    char sep = getDirectorySeparator();
    
    do {
        std::wstring filenameW = fd.cFileName;
        if (filenameW == L"." || filenameW == L"..") {
            continue;
        }
        
        std::string filename = fromWidePath(filenameW);
        std::string fullPath = combine(directory, filename);
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive) {
                auto subFiles = listFiles(fullPath, pattern, recursive);
                result.insert(result.end(), subFiles.begin(), subFiles.end());
            }
        } else {
            if (pattern == "*" || StringUtils::wildCardMatch(pattern, filename)) {
                result.push_back(fullPath);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        return result;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename == "." || filename == "..") {
            continue;
        }
        
        std::string fullPath = combine(directory, filename);
        
        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) != 0) {
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            if (recursive) {
                auto subFiles = listFiles(fullPath, pattern, recursive);
                result.insert(result.end(), subFiles.begin(), subFiles.end());
            }
        } else {
            if (pattern == "*" || 
                fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
                result.push_back(fullPath);
            }
        }
    }
    
    closedir(dir);
#endif
    
    return result;
}

std::vector<std::string> PathUtils::listDirectories(
    const std::string& directory,
    bool recursive
) {
    std::vector<std::string> result;
    
    if (!isDirectory(directory)) {
        return result;
    }
    
#ifdef _WIN32
    std::wstring searchPath = toWidePath(combine(directory, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return result;
    }
    
    do {
        std::wstring filenameW = fd.cFileName;
        if (filenameW == L"." || filenameW == L"..") {
            continue;
        }
        
        std::string filename = fromWidePath(filenameW);
        std::string fullPath = combine(directory, filename);
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            result.push_back(fullPath);
            
            if (recursive) {
                auto subDirs = listDirectories(fullPath, recursive);
                result.insert(result.end(), subDirs.begin(), subDirs.end());
            }
        }
    } while (FindNextFileW(hFind, &fd));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        return result;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename == "." || filename == "..") {
            continue;
        }
        
        std::string fullPath = combine(directory, filename);
        
        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) != 0) {
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            result.push_back(fullPath);
            
            if (recursive) {
                auto subDirs = listDirectories(fullPath, recursive);
                result.insert(result.end(), subDirs.begin(), subDirs.end());
            }
        }
    }
    
    closedir(dir);
#endif
    
    return result;
}

bool PathUtils::isAbsolutePath(const std::string& path) {
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':') {
        return true;
    }
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    return false;
#else
    return !path.empty() && path[0] == '/';
#endif
}

std::string PathUtils::normalizePath(const std::string& path) {
    if (path.empty()) {
        return "";
    }
    
    char sep = getDirectorySeparator();
    
#ifdef _WIN32
    bool isUNC = false;
    std::string uncPrefix;
    bool isDrive = false;
    std::string drivePrefix;
    
    if (path.size() >= 2 && (path[0] == '\\' || path[0] == '/') && 
                            (path[1] == '\\' || path[1] == '/')) {
        isUNC = true;
        uncPrefix = "\\\\";
        
        size_t serverEnd = path.find_first_of("\\/", 2);
        if (serverEnd != std::string::npos) {
            size_t shareEnd = path.find_first_of("\\/", serverEnd + 1);
            if (shareEnd != std::string::npos) {
                uncPrefix = path.substr(0, shareEnd);
            } else {
                uncPrefix = path.substr(0, serverEnd);
            }
        } else {
            uncPrefix = path;
        }
    } else if (path.size() >= 2 && path[1] == ':') {
        isDrive = true;
        drivePrefix = path.substr(0, 2);
        if (path.size() > 2 && (path[2] == '\\' || path[2] == '/')) {
            drivePrefix += sep;
        }
    }
#endif
    
    size_t startPos = 0;
#ifdef _WIN32
    if (isUNC) {
        startPos = uncPrefix.size();
    } else if (isDrive) {
        startPos = drivePrefix.size();
    }
#endif
    
    std::string tempPath;
    tempPath.reserve(path.size());
    
    char lastChar = 0;
    for (size_t i = startPos; i < path.size(); ++i) {
        char ch = path[i];
        if (ch == '/' || ch == '\\') {
            if (lastChar != '/' && lastChar != '\\') {
                tempPath += sep;
            }
            lastChar = ch;
        } else {
            tempPath += ch;
            lastChar = ch;
        }
    }
    
    std::vector<std::string> parts;
    if (!tempPath.empty()) {
        parts = StringUtils::split(tempPath, std::string(1, sep), true);
    }
    
    std::vector<std::string> normalizedParts;
    
    for (const auto& part : parts) {
        if (part == "..") {
            if (!normalizedParts.empty() && normalizedParts.back() != "..") {
                normalizedParts.pop_back();
            } else {
                normalizedParts.push_back(part);
            }
        } else if (part == "." || part.empty()) {
            continue;
        } else {
            normalizedParts.push_back(part);
        }
    }
    
    std::string result;
    
#ifdef _WIN32
    if (isUNC) {
        result = uncPrefix;
        if (!normalizedParts.empty()) {
            if (!StringUtils::endsWith(result, std::string(1, sep))) {
                result += sep;
            }
            result += StringUtils::join(normalizedParts, std::string(1, sep));
        }
        return result;
    }
    
    if (isDrive) {
        result = drivePrefix;
        if (!normalizedParts.empty()) {
            if (!StringUtils::endsWith(result, std::string(1, sep))) {
                result += sep;
            }
            result += StringUtils::join(normalizedParts, std::string(1, sep));
        }
        return result;
    }
#endif
    
    if (normalizedParts.empty()) {
        return std::string(1, sep);
    }
    
    result = StringUtils::join(normalizedParts, std::string(1, sep));
    
    if (isAbsolutePath(path)) {
        result = std::string(1, sep) + result;
    }
    
    return result;
}

char PathUtils::getDirectorySeparator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string PathUtils::getTempDirectory() {
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH> buffer;
    DWORD size = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (size == 0 || size > buffer.size()) {
        return "";
    }
    return fromWidePath(std::wstring(buffer.data(), size));
#else
    const char* temp = getenv("TMPDIR");
    if (temp) return std::string(temp);
    
    temp = getenv("TMP");
    if (temp) return std::string(temp);
    
    temp = getenv("TEMP");
    if (temp) return std::string(temp);
    
    return "/tmp";
#endif
}

std::string PathUtils::getTempFileName() {
    std::string tempDir = getTempDirectory();
    char sep = getDirectorySeparator();
    
#ifdef _WIN32
    std::wstring wTempDir = toWidePath(tempDir);
    std::array<wchar_t, MAX_PATH> buffer;
    
    if (GetTempFileNameW(wTempDir.c_str(), L"ct_", 0, buffer.data()) == 0) {
        return "";
    }
    
    return fromWidePath(std::wstring(buffer.data()));
#else
    std::string pattern = tempDir;
    if (!StringUtils::endsWith(pattern, std::string(1, sep))) {
        pattern += sep;
    }
    pattern += "ct_XXXXXX";
    
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    
    int fd = mkstemp(buffer.data());
    if (fd == -1) {
        return "";
    }
    close(fd);
    
    return std::string(buffer.data());
#endif
}

}
}
