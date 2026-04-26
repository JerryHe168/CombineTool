#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "combinetool/types.h"
#include "combinetool/config.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace combinetool {
namespace io {

class MappedFileReader {
public:
    explicit MappedFileReader(
        const std::string& filePath,
        size_t mapSize = config::MAX_MEMORY_MAP_SIZE
    );
    ~MappedFileReader();
    
    bool isOpen() const;
    bool isMapped() const;
    void close();
    
    bool map();
    bool unmap();
    
    const char* getData() const;
    char* getData();
    size_t getMappedSize() const;
    uint64_t getFileSize() const;
    
    bool readLine(std::string& line, size_t& offset, bool keepNewline = false);
    
    const std::string& getFilePath() const;
    
    void setEncoding(Encoding encoding);
    Encoding getEncoding() const;
    
    void setProgressCallback(
        std::function<void(uint64_t current, uint64_t total)> callback
    );

private:
    std::string m_filePath;
    uint64_t m_fileSize;
    size_t m_mapSize;
    Encoding m_encoding;
    
#ifdef _WIN32
    HANDLE m_fileHandle;
    HANDLE m_mapHandle;
    LPVOID m_mappedData;
#else
    int m_fileDescriptor;
    void* m_mappedData;
#endif
    
    bool m_isOpen;
    bool m_isMapped;
    size_t m_currentOffset;
    
    std::function<void(uint64_t, uint64_t)> m_progressCallback;
    
    bool openFile();
    void closeFile();
    void updateProgress();
};

class MappedLineIterator {
public:
    explicit MappedLineIterator(MappedFileReader& reader);
    
    bool hasNext() const;
    bool next(std::string& line);
    
    void reset();

private:
    MappedFileReader& m_reader;
    size_t m_offset;
    std::string m_nextLine;
    bool m_hasNext;
    bool m_initialized;
    
    void prefetch();
};

}
}
