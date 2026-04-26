#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/io/smart_file_reader.h"
#include "combinetool/io/mapped_file_reader.h"

#include <vector>
#include <memory>
#include <fstream>

namespace combinetool {
namespace merge {

enum class BinaryMergeStrategy {
    SequentialAppend,
    ChunkedCopy,
    MemoryMapped
};

struct BinaryMergeStats {
    uint64_t totalBytesRead;
    uint64_t totalBytesWritten;
    size_t filesProcessed;
    double elapsedSeconds;
    size_t chunksProcessed;
};

class BinaryMerger : public Merger {
public:
    explicit BinaryMerger(const MergeConfig& config);
    ~BinaryMerger() override;
    
    bool merge() override;
    
    void setChunkSize(size_t chunkSize);
    size_t getChunkSize() const;
    
    void setStrategy(BinaryMergeStrategy strategy);
    BinaryMergeStrategy getStrategy() const;
    
    const BinaryMergeStats& getStats() const;

private:
    size_t m_chunkSize;
    BinaryMergeStrategy m_strategy;
    BinaryMergeStats m_stats;
    std::unique_ptr<std::ofstream> m_outputStream;
    std::vector<char> m_buffer;
    
    struct FileReaderState {
        std::unique_ptr<io::SmartFileReader> smartReader;
        std::unique_ptr<io::MappedFileReader> mappedReader;
        std::string filePath;
        uint64_t fileSize;
        uint64_t bytesRead;
        bool eof;
        io::ReaderType readerType;
    };
    
    std::vector<FileReaderState> m_readers;
    
    bool initialize();
    bool cleanup();
    
    bool detectFileType(const std::string& filePath, FileType& detectedType);
    BinaryMergeStrategy selectOptimalStrategy(uint64_t totalSize, size_t fileCount);
    
    bool mergeSequential();
    bool mergeChunked();
    bool mergeMemoryMapped();
    
    bool copyFileStream(FileReaderState& state);
    bool copyFileMapped(FileReaderState& state);
    bool copyFileChunked(FileReaderState& state);
    
    bool writeBytes(const char* data, size_t size);
    bool writeChunk(const std::vector<char>& buffer, size_t bytesToWrite);
    
    uint64_t calculateTotalSize() const;
    void updateStats(uint64_t bytesRead, uint64_t bytesWritten);
};

}
}
