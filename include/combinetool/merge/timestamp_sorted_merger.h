#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/utils/timestamp_extractor.h"
#include "combinetool/io/smart_file_reader.h"

#include <queue>
#include <vector>
#include <memory>

namespace combinetool {
namespace merge {

struct MergeHeapEntry {
    utils::LogLineEntry entry;
    size_t fileIndex;
    size_t lineIndex;
    
    bool operator>(const MergeHeapEntry& other) const {
        return entry > other.entry;
    }
};

class TimestampSortedMerger : public Merger {
public:
    explicit TimestampSortedMerger(const MergeConfig& config);
    ~TimestampSortedMerger() override;
    
    bool merge() override;
    
    void setMaxMemoryBuffer(size_t maxEntries);
    size_t getMaxMemoryBuffer() const;

private:
    size_t m_maxMemoryEntries;
    std::unique_ptr<utils::TimestampExtractor> m_extractor;
    std::unique_ptr<std::ofstream> m_outputStream;
    
    struct FileReaderState {
        std::unique_ptr<io::SmartFileReader> reader;
        std::unique_ptr<io::SmartLineIterator> iterator;
        std::string filePath;
        size_t lineNumber;
        bool hasHeader;
        bool headerWritten;
        bool eof;
        utils::LogLineEntry nextEntry;
        bool hasNextEntry;
    };
    
    std::vector<FileReaderState> m_readers;
    std::priority_queue<MergeHeapEntry, std::vector<MergeHeapEntry>, std::greater<MergeHeapEntry>> m_mergeHeap;
    
    bool initializeReaders();
    bool closeAllReaders();
    
    bool readNextEntry(FileReaderState& state);
    bool fillHeap();
    
    bool mergeInMemory();
    bool mergeExternal();
    
    bool processAndWriteEntry(const utils::LogLineEntry& entry);
    bool writeLine(const std::string& line);
    
    bool shouldUseExternalMerge() const;
    uint64_t estimateTotalLines() const;
};

}
}
