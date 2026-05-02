#pragma once

#include "combinetool/merge/merger.h"

#include <vector>
#include <string>

namespace combinetool {
namespace merge {

struct FileHeaderInfo {
    std::string filePath;
    bool hasHeader;
    std::string headerLine;
    std::vector<std::string> headerColumns;
};

class SequentialMerger : public Merger {
public:
    explicit SequentialMerger(const MergeConfig& config);
    ~SequentialMerger() override;
    
    bool merge() override;

private:
    bool collectAllHeaders(std::vector<FileHeaderInfo>& headerInfos);
    bool verifyHeaderConsistency(const std::vector<FileHeaderInfo>& headerInfos);
    bool writeUnifiedHeader(const std::vector<FileHeaderInfo>& headerInfos);
    
    bool processFile(const std::string& filePath);
    bool writeLine(const std::string& line);
    
    std::unique_ptr<std::ofstream> m_outputStream;
};

}
}
