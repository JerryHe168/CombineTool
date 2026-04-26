#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/string_utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#endif

namespace combinetool {
namespace utils {

std::vector<uint8_t> FileUtils::readAllBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    
    return {};
}

std::string FileUtils::readAllText(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::vector<std::string> FileUtils::readAllLines(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
    }
    
    return lines;
}

bool FileUtils::writeAllBytes(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    
    return file.write(reinterpret_cast<const char*>(data.data()), data.size()).good();
}

bool FileUtils::writeAllText(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    
    return file.write(content.data(), content.size()).good();
}

bool FileUtils::writeAllLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    
    for (const auto& line : lines) {
        file << line << '\n';
        if (!file) {
            return false;
        }
    }
    
    return true;
}

bool FileUtils::appendText(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return false;
    }
    
    return file.write(content.data(), content.size()).good();
}

bool FileUtils::appendLine(const std::string& path, const std::string& line) {
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return false;
    }
    
    file << line << '\n';
    return file.good();
}

bool FileUtils::copy(const std::string& source, const std::string& dest, bool overwrite) {
    if (!PathUtils::isFile(source)) {
        return false;
    }
    
    if (PathUtils::exists(dest)) {
        if (!overwrite) {
            return false;
        }
        if (!FileUtils::remove(dest)) {
            return false;
        }
    }
    
    std::string destDir = PathUtils::getDirectoryName(dest);
    if (!destDir.empty() && !PathUtils::exists(destDir)) {
        if (!PathUtils::createDirectories(destDir)) {
            return false;
        }
    }
    
    std::ifstream srcFile(source, std::ios::binary);
    if (!srcFile) {
        return false;
    }
    
    std::ofstream destFile(dest, std::ios::binary);
    if (!destFile) {
        return false;
    }
    
    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> buffer(bufferSize);
    
    while (srcFile) {
        srcFile.read(buffer.data(), bufferSize);
        auto bytesRead = srcFile.gcount();
        if (bytesRead > 0) {
            destFile.write(buffer.data(), bytesRead);
            if (!destFile) {
                return false;
            }
        }
    }
    
    return true;
}

bool FileUtils::move(const std::string& source, const std::string& dest) {
#ifdef _WIN32
    std::wstring wSource = StringUtils::toWideString(source);
    std::wstring wDest = StringUtils::toWideString(dest);
    
    if (MoveFileW(wSource.c_str(), wDest.c_str())) {
        return true;
    }
#else
    if (rename(source.c_str(), dest.c_str()) == 0) {
        return true;
    }
#endif
    
    if (copy(source, dest, true)) {
        return FileUtils::remove(source);
    }
    
    return false;
}

bool FileUtils::remove(const std::string& path) {
#ifdef _WIN32
    std::wstring wPath = StringUtils::toWideString(path);
    if (PathUtils::isDirectory(path)) {
        return RemoveDirectoryW(wPath.c_str()) != FALSE;
    } else {
        return DeleteFileW(wPath.c_str()) != FALSE;
    }
#else
    if (PathUtils::isDirectory(path)) {
        return rmdir(path.c_str()) == 0;
    } else {
        return std::remove(path.c_str()) == 0;
    }
#endif
}

std::unique_ptr<std::ifstream> FileUtils::openForRead(const std::string& path, bool binary) {
    auto stream = std::make_unique<std::ifstream>();
    auto mode = binary ? std::ios::binary : std::ios::openmode(0);
    stream->open(path, mode);
    if (!stream->is_open()) {
        return nullptr;
    }
    return stream;
}

std::unique_ptr<std::ofstream> FileUtils::openForWrite(const std::string& path, bool binary, bool append) {
    auto stream = std::make_unique<std::ofstream>();
    auto mode = binary ? std::ios::binary : std::ios::openmode(0);
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }
    stream->open(path, mode);
    if (!stream->is_open()) {
        return nullptr;
    }
    return stream;
}

size_t FileUtils::readChunk(
    std::ifstream& stream,
    std::vector<char>& buffer,
    size_t position
) {
    if (!stream) {
        return 0;
    }
    
    if (position != static_cast<size_t>(-1)) {
        stream.seekg(static_cast<std::streamoff>(position));
        if (!stream) {
            return 0;
        }
    }
    
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    auto bytesRead = static_cast<size_t>(stream.gcount());
    
    return bytesRead;
}

bool FileUtils::readLine(
    std::ifstream& stream,
    std::string& line,
    bool keepNewline
) {
    line.clear();
    
    if (!stream) {
        return false;
    }
    
    constexpr size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize);
    
    while (stream) {
        stream.getline(buffer.data(), static_cast<std::streamsize>(bufferSize));
        
        if (stream.eof() && stream.gcount() == 0) {
            break;
        }
        
        auto count = static_cast<size_t>(stream.gcount());
        if (count > 0 && buffer[count - 1] == '\0') {
            --count;
        }
        
        line.append(buffer.data(), count);
        
        if (stream.fail()) {
            stream.clear();
        } else {
            if (keepNewline) {
                line += '\n';
            }
            break;
        }
    }
    
    return !line.empty() || (stream.eof() && !stream.bad());
}

bool FileUtils::writeLine(
    std::ofstream& stream,
    const std::string& line,
    const std::string& newline
) {
    if (!stream) {
        return false;
    }
    
    stream.write(line.data(), static_cast<std::streamsize>(line.size()));
    if (!stream) {
        return false;
    }
    
    stream.write(newline.data(), static_cast<std::streamsize>(newline.size()));
    return stream.good();
}

std::streampos FileUtils::getPosition(std::ifstream& stream) {
    return stream.tellg();
}

void FileUtils::setPosition(std::ifstream& stream, std::streampos pos) {
    stream.seekg(pos);
}

std::streamsize FileUtils::getStreamSize(std::ifstream& stream) {
    auto current = stream.tellg();
    stream.seekg(0, std::ios::end);
    auto size = stream.tellg();
    stream.seekg(current);
    return size;
}

bool FileUtils::compareFiles(const std::string& path1, const std::string& path2) {
    if (!PathUtils::isFile(path1) || !PathUtils::isFile(path2)) {
        return false;
    }
    
    auto size1 = PathUtils::getFileSize(path1);
    auto size2 = PathUtils::getFileSize(path2);
    
    if (size1 != size2) {
        return false;
    }
    
    std::ifstream file1(path1, std::ios::binary);
    std::ifstream file2(path2, std::ios::binary);
    
    if (!file1 || !file2) {
        return false;
    }
    
    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> buffer1(bufferSize);
    std::vector<char> buffer2(bufferSize);
    
    while (file1 && file2) {
        file1.read(buffer1.data(), static_cast<std::streamsize>(bufferSize));
        file2.read(buffer2.data(), static_cast<std::streamsize>(bufferSize));
        
        auto count1 = static_cast<size_t>(file1.gcount());
        auto count2 = static_cast<size_t>(file2.gcount());
        
        if (count1 != count2) {
            return false;
        }
        
        if (std::memcmp(buffer1.data(), buffer2.data(), count1) != 0) {
            return false;
        }
    }
    
    return true;
}

uint64_t FileUtils::computeCRC32(const std::string& path) {
    static const uint32_t crcTable[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return 0;
    }
    
    uint32_t crc = 0xffffffff;
    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> buffer(bufferSize);
    
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(bufferSize));
        auto count = static_cast<size_t>(file.gcount());
        
        for (size_t i = 0; i < count; ++i) {
            crc = crcTable[(crc ^ static_cast<uint8_t>(buffer[i])) & 0xff] ^ (crc >> 8);
        }
    }
    
    return static_cast<uint64_t>(crc ^ 0xffffffff);
}

}
}
