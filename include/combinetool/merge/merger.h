#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <fstream>
#include "combinetool/types.h"
#include "combinetool/filter/filter.h"

namespace combinetool {
namespace merge {

class Merger {
public:
    explicit Merger(const MergeConfig& config);
    virtual ~Merger() = default;
    
    virtual bool merge() = 0;
    
    size_t getTotalLinesProcessed() const;
    size_t getTotalLinesWritten() const;
    size_t getFilesProcessed() const;
    
    void setProgressCallback(
        std::function<void(size_t current, size_t total, const std::string& currentFile)> callback
    );

protected:
    MergeConfig m_config;
    size_t m_totalLinesProcessed;
    size_t m_totalLinesWritten;
    size_t m_filesProcessed;
    
    std::function<void(size_t, size_t, const std::string&)> m_progressCallback;
    
    virtual bool processLine(const LineData& line);
    virtual bool shouldFilter(const LineData& line);
    
    void updateProgress(size_t current, size_t total, const std::string& file);
};

}
}
