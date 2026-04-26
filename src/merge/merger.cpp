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

void Merger::updateProgress(size_t current, size_t total, const std::string& file) {
    if (m_progressCallback) {
        m_progressCallback(current, total, file);
    }
}

}
}
