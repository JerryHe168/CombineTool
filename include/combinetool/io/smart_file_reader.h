#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "combinetool/types.h"
#include "combinetool/config.h"
#include "combinetool/io/stream_reader.h"
#include "combinetool/io/mapped_file_reader.h"

namespace combinetool {
namespace io {

enum class ReaderType {
    Stream,
    MemoryMapped
};

class SmartFileReader {
public:
    explicit SmartFileReader(
        const std::string& filePath,
        uint64_t memoryMapThreshold = config::DEFAULT_MEMORY_MAP_THRESHOLD,
        size_t bufferSize = config::DEFAULT_BUFFER_SIZE,
        FileType fileType = FileType::AutoDetect
    );
    
    ~SmartFileReader();
    
    bool isOpen() const;
    void close();
    
    ReaderType getReaderType() const;
    
    bool readLine(std::string& line, bool keepNewline = false);
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
    
    static uint64_t getDefaultMemoryMapThreshold();

private:
    std::string m_filePath;
    uint64_t m_memoryMapThreshold;
    size_t m_bufferSize;
    FileType m_fileType;
    
    ReaderType m_readerType;
    uint64_t m_fileSize;
    
    std::unique_ptr<StreamReader> m_streamReader;
    std::unique_ptr<MappedFileReader> m_mappedReader;
    size_t m_mappedOffset;
    
    std::function<void(uint64_t, uint64_t)> m_progressCallback;
    
    bool detectFileType();
    bool selectReaderType();
    void initializeReader();
    void updateProgress(uint64_t current, uint64_t total);
};

class SmartLineIterator {
public:
    explicit SmartLineIterator(SmartFileReader& reader);
    
    bool hasNext() const;
    bool next(std::string& line);
    
    void reset();

private:
    SmartFileReader& m_reader;
    std::string m_nextLine;
    bool m_hasNext;
    bool m_initialized;
    
    void prefetch();
};

}
}
