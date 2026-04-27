#pragma once

#include "combinetool/merge/merger.h"

namespace combinetool {
namespace merge {

class SequentialMerger : public Merger {
public:
    explicit SequentialMerger(const MergeConfig& config);
    ~SequentialMerger() override;
    
    bool merge() override;

private:
    bool processFile(const std::string& filePath, bool isFirstFile);
    bool writeLine(const std::string& line);
    
    std::unique_ptr<std::ofstream> m_outputStream;
};

}
}
