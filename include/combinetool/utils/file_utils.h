#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace combinetool {
namespace utils {

class FileUtils {
public:
    static std::vector<uint8_t> readAllBytes(const std::string& path);
    static std::string readAllText(const std::string& path);
    static std::vector<std::string> readAllLines(const std::string& path);
    
    static bool writeAllBytes(const std::string& path, const std::vector<uint8_t>& data);
    static bool writeAllText(const std::string& path, const std::string& content);
    static bool writeAllLines(const std::string& path, const std::vector<std::string>& lines);
    
    static bool appendText(const std::string& path, const std::string& content);
    static bool appendLine(const std::string& path, const std::string& line);
    
    static bool copy(const std::string& source, const std::string& dest, bool overwrite = false);
    static bool move(const std::string& source, const std::string& dest);
    static bool remove(const std::string& path);
    
    static std::unique_ptr<std::ifstream> openForRead(const std::string& path, bool binary = true);
    static std::unique_ptr<std::ofstream> openForWrite(const std::string& path, bool binary = true, bool append = false);
    
    static size_t readChunk(
        std::ifstream& stream,
        std::vector<char>& buffer,
        size_t position = static_cast<size_t>(-1)
    );
    
    static bool readLine(
        std::ifstream& stream,
        std::string& line,
        bool keepNewline = false
    );
    
    static bool writeLine(
        std::ofstream& stream,
        const std::string& line,
        const std::string& newline = "\n"
    );
    
    static std::streampos getPosition(std::ifstream& stream);
    static void setPosition(std::ifstream& stream, std::streampos pos);
    static std::streamsize getStreamSize(std::ifstream& stream);
    
    static bool compareFiles(const std::string& path1, const std::string& path2);
    static uint64_t computeCRC32(const std::string& path);
};

}
}
