#pragma once

#include "combinetool/merge/merger.h"
#include "combinetool/filter/filter.h"

namespace combinetool {
namespace merge {

class SequentialMerger : public Merger {
public:
    explicit SequentialMerger(const MergeConfig& config);
    ~SequentialMerger() override;
    
    bool merge() override;

private:
    std::unique_ptr<filter::Filter> m_filter;
    std::unique_ptr<filter::Deduplicator> m_deduplicator;
    
    bool processFile(const std::string& filePath, bool isFirstFile);
    bool writeLine(const std::string& line);
    
    std::unique_ptr<std::ofstream> m_outputStream;
};

}
}
