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

struct ReadResult {
    bool success;
    size_t bytesRead;
    bool eof;
    std::string errorMessage;
};

class StreamReader {
public:
    explicit StreamReader(
        const std::string& filePath,
        size_t bufferSize = config::DEFAULT_BUFFER_SIZE
    );
    ~StreamReader();
    
    bool isOpen() const;
    void close();
    
    ReadResult readLine(std::string& line, bool keepNewline = false);
    ReadResult readChunk(std::vector<char>& buffer, size_t& bytesRead);
    
    size_t getPosition() const;
    bool setPosition(size_t position);
    
    uint64_t getFileSize() const;
    size_t getBufferSize() const;
    
    const std::string& getFilePath() const;
    
    void setEncoding(Encoding encoding);
    Encoding getEncoding() const;
    
    void setProgressCallback(
        std::function<void(uint64_t current, uint64_t total)> callback
    );

private:
    std::string m_filePath;
    std::unique_ptr<std::ifstream> m_stream;
    std::vector<char> m_buffer;
    size_t m_bufferSize;
    uint64_t m_fileSize;
    size_t m_position;
    Encoding m_encoding;
    
    std::function<void(uint64_t, uint64_t)> m_progressCallback;
    
    bool detectFileSize();
    void updateProgress();
};

class LineIterator {
public:
    explicit LineIterator(StreamReader& reader);
    
    bool hasNext() const;
    bool next(std::string& line);
    
    void reset();

private:
    StreamReader& m_reader;
    std::string m_nextLine;
    bool m_hasNext;
    bool m_initialized;
    
    void prefetch();
};

}
}
