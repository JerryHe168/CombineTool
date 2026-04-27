#include "combinetool/merge/merger.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"

namespace combinetool {
namespace merge {

Merger::Merger(const MergeConfig& config)
    : m_config(config)
    , m_totalLinesProcessed(0)
    , m_totalLinesWritten(0)
    , m_filesProcessed(0)
{
    initializeFilterAndDeduplicator();
}

void Merger::initializeFilterAndDeduplicator() {
    if (m_config.filterConfig.mode != FilterMode::Exclude || 
        !m_config.filterConfig.includePatterns.empty() ||
        !m_config.filterConfig.excludePatterns.empty() ||
        m_config.filterConfig.filterBlankLines ||
        m_config.filterConfig.filterCommentLines) {
        m_filter = std::make_unique<filter::Filter>(m_config.filterConfig);
    }
    
    if (m_config.deduplicationConfig.mode != DeduplicationMode::None) {
        m_deduplicator = std::make_unique<filter::Deduplicator>(m_config.deduplicationConfig);
    }
}

size_t Merger::getTotalLinesProcessed() const {
    return m_totalLinesProcessed;
}

size_t Merger::getTotalLinesWritten() const {
    return m_totalLinesWritten;
}

size_t Merger::getFilesProcessed() const {
    return m_filesProcessed;
}

void Merger::setProgressCallback(
    std::function<void(size_t, size_t, const std::string&)> callback
) {
    m_progressCallback = callback;
}

bool Merger::processLine(const LineData& line) {
    m_totalLinesProcessed++;
    return true;
}

bool Merger::shouldFilter(const LineData& line) {
    return false;
}

bool Merger::shouldKeep(const LineData& line) {
    if (m_filter) {
        return m_filter->shouldKeep(line);
    }
    return true;
}

bool Merger::isDuplicate(const LineData& line) {
    if (m_deduplicator) {
        return m_deduplicator->isDuplicate(line);
    }
    return false;
}

void Merger::updateProgress(size_t current, size_t total, const std::string& file) {
    if (m_progressCallback) {
        m_progressCallback(current, total, file);
    }
}

}
}
