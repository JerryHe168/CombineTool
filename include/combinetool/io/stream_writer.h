#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <functional>
#include "combinetool/types.h"
#include "combinetool/config.h"

namespace combinetool {
namespace io {

struct WriteResult {
    bool success;
    size_t bytesWritten;
    std::string errorMessage;
};

class StreamWriter {
public:
    explicit StreamWriter(
        const std::string& filePath,
        bool append = false,
        size_t bufferSize = config::DEFAULT_BUFFER_SIZE
    );
    ~StreamWriter();
    
    bool isOpen() const;
    void close();
    bool flush();
    
    WriteResult writeLine(const std::string& line, const std::string& newline = "\n");
    WriteResult writeText(const std::string& text);
    WriteResult writeBuffer(const std::vector<char>& buffer);
    WriteResult writeBuffer(const char* data, size_t length);
    
    size_t getPosition() const;
    uint64_t getBytesWritten() const;
    
    const std::string& getFilePath() const;
    
    void setEncoding(Encoding encoding);
    Encoding getEncoding() const;
    bool writeBOM();
    
    void setProgressCallback(
        std::function<void(uint64_t current, uint64_t estimated)> callback
    );

private:
    std::string m_filePath;
    std::unique_ptr<std::ofstream> m_stream;
    std::vector<char> m_buffer;
    size_t m_bufferSize;
    uint64_t m_bytesWritten;
    Encoding m_encoding;
    bool m_bomWritten;
    
    std::function<void(uint64_t, uint64_t)> m_progressCallback;
    
    void updateProgress();
};

class BufferedWriter {
public:
    explicit BufferedWriter(
        const std::string& filePath,
        size_t bufferSize = config::LARGE_BUFFER_SIZE,
        bool append = false
    );
    ~BufferedWriter();
    
    bool isOpen() const;
    void close();
    bool flush();
    
    bool writeLine(const std::string& line, const std::string& newline = "\n");
    bool writeText(const std::string& text);
    
    uint64_t getBytesWritten() const;
    size_t getBufferUsed() const;
    size_t getBufferSize() const;

private:
    std::string m_filePath;
    std::unique_ptr<std::ofstream> m_stream;
    std::vector<char> m_buffer;
    size_t m_bufferSize;
    size_t m_bufferUsed;
    uint64_t m_bytesWritten;
    
    bool flushBuffer();
};

}
}
