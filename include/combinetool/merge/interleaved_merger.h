#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/filter/filter.h"

namespace combinetool {
namespace merge {

class InterleavedMerger : public Merger {
public:
    explicit InterleavedMerger(const MergeConfig& config);
    ~InterleavedMerger() override;
    
    bool merge() override;
    
    void setChunkSize(size_t linesPerChunk);
    size_t getChunkSize() const;

private:
    size_t m_linesPerChunk;
    std::unique_ptr<filter::Filter> m_filter;
    std::unique_ptr<filter::Deduplicator> m_deduplicator;
    
    struct FileReaderState {
        std::unique_ptr<std::ifstream> stream;
        std::string filePath;
        size_t lineNumber;
        bool hasHeader;
        bool headerWritten;
        bool eof;
    };
    
    std::vector<FileReaderState> m_readers;
    std::unique_ptr<std::ofstream> m_outputStream;
    
    bool openAllFiles();
    bool closeAllFiles();
    bool readNextLine(FileReaderState& reader, std::string& line);
    bool writeLine(const std::string& line);
};

}
}
