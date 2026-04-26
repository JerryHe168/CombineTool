#include "combinetool/filter/filter.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/format/format_detector.h"

#include <algorithm>
#include <functional>
#include <cctype>

namespace combinetool {
namespace filter {

Filter::Filter(const FilterConfig& config)
    : m_config(config)
{
    compilePatterns();
}

bool Filter::shouldKeep(const LineData& line) const {
    if (!line.isValid) {
        return false;
    }
    
    return shouldKeep(line.content);
}

bool Filter::shouldKeep(const std::string& content) const {
    if (m_config.filterBlankLines && isBlankLine(content)) {
        return false;
    }
    
    if (m_config.filterCommentLines && isCommentLine(content)) {
        return false;
    }
    
    if (!m_config.includePatterns.empty() || !m_includeRegexes.empty()) {
        if (!matchesInclude(content)) {
            return false;
        }
    }
    
    if (!m_config.excludePatterns.empty() || !m_excludeRegexes.empty()) {
        if (matchesExclude(content)) {
            return false;
        }
    }
    
    if (m_config.customFilter) {
        LineData dummy;
        dummy.content = content;
        dummy.isValid = true;
        if (!m_config.customFilter(content)) {
            return false;
        }
    }
    
    return true;
}

bool Filter::matchesInclude(const std::string& content) const {
    return matchesPatterns(content, m_config.includePatterns, m_includeRegexes);
}

bool Filter::matchesExclude(const std::string& content) const {
    return matchesPatterns(content, m_config.excludePatterns, m_excludeRegexes);
}

bool Filter::isBlankLine(const std::string& content) const {
    return utils::StringUtils::isBlank(content);
}

bool Filter::isCommentLine(const std::string& content) const {
    std::string trimmed = utils::StringUtils::trimLeft(content);
    return utils::StringUtils::startsWith(trimmed, m_config.commentPrefix);
}

void Filter::setCustomFilter(std::function<bool(const LineData&)> customFilter) {
    m_config.customFilter = [customFilter](const std::string& content) {
        LineData line;
        line.content = content;
        line.isValid = true;
        return customFilter(line);
    };
}

void Filter::compilePatterns() {
    auto compile = [this](const std::vector<std::string>& patterns, 
                           std::vector<std::regex>& regexes) {
        for (const auto& pattern : patterns) {
            if (pattern.empty()) {
                continue;
            }
            
            if (pattern.find_first_of("*?[]") != std::string::npos) {
                continue;
            }
            
            try {
                auto flags = std::regex::ECMAScript;
                if (!m_config.caseSensitive) {
                    flags |= std::regex::icase;
                }
                regexes.emplace_back(pattern, flags);
            } catch (const std::regex_error&) {
            }
        }
    };
    
    compile(m_config.includePatterns, m_includeRegexes);
    compile(m_config.excludePatterns, m_excludeRegexes);
}

bool Filter::matchesPatterns(
    const std::string& content,
    const std::vector<std::string>& patterns,
    const std::vector<std::regex>& regexes
) const {
    for (const auto& re : regexes) {
        if (std::regex_search(content, re)) {
            return true;
        }
    }
    
    for (const auto& pattern : patterns) {
        if (pattern.empty()) {
            continue;
        }
        
        if (pattern.find_first_of("*?[]") != std::string::npos) {
            if (matchesWildCard(pattern, content)) {
                return true;
            }
        } else {
            if (m_config.caseSensitive) {
                if (utils::StringUtils::contains(content, pattern)) {
                    return true;
                }
            } else {
                if (utils::StringUtils::containsIgnoreCase(content, pattern)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool Filter::matchesWildCard(const std::string& pattern, const std::string& content) const {
    if (m_config.caseSensitive) {
        return utils::StringUtils::wildCardMatch(pattern, content);
    } else {
        return utils::StringUtils::wildCardMatch(
            utils::StringUtils::toLower(pattern),
            utils::StringUtils::toLower(content)
        );
    }
}

Deduplicator::Deduplicator(const DeduplicationConfig& config)
    : m_config(config)
    , m_duplicateCount(0)
{
}

void Deduplicator::reset() {
    m_seenKeys.clear();
    m_uniqueLines.clear();
    m_duplicateCount = 0;
}

bool Deduplicator::isDuplicate(const LineData& line) {
    if (!line.isValid) {
        return true;
    }
    
    if (m_config.mode == DeduplicationMode::None) {
        m_uniqueLines.push_back(line);
        return false;
    }
    
    std::string key = generateKey(line.content);
    
    auto it = m_seenKeys.find(key);
    if (it != m_seenKeys.end()) {
        m_duplicateCount++;
        
        if (m_config.keepStrategy == DeduplicationKeepStrategy::Last) {
            for (auto& uniqueLine : m_uniqueLines) {
                std::string existingKey = generateKey(uniqueLine.content);
                if (existingKey == key) {
                    uniqueLine = line;
                    break;
                }
            }
        }
        
        return true;
    }
    
    m_seenKeys.insert(key);
    m_uniqueLines.push_back(line);
    return false;
}

bool Deduplicator::isDuplicate(const std::string& content) {
    LineData line;
    line.content = content;
    line.isValid = true;
    return isDuplicate(line);
}

size_t Deduplicator::getDuplicateCount() const {
    return m_duplicateCount;
}

size_t Deduplicator::getUniqueCount() const {
    return m_uniqueLines.size();
}

const std::vector<LineData>& Deduplicator::getUniqueLines() const {
    return m_uniqueLines;
}

std::string Deduplicator::generateKey(const std::string& content) const {
    switch (m_config.mode) {
        case DeduplicationMode::None:
            return content;
            
        case DeduplicationMode::FullLine:
            return m_config.caseSensitive ? content : utils::StringUtils::toLower(content);
            
        case DeduplicationMode::Partial:
        case DeduplicationMode::ColumnBased:
            return extractColumns(content);
            
        default:
            return content;
    }
}

std::string Deduplicator::extractColumns(const std::string& content) const {
    auto fields = format::FormatDetector::parseCSVLine(content, m_config.delimiter);
    
    if (fields.empty()) {
        return m_config.caseSensitive ? content : utils::StringUtils::toLower(content);
    }
    
    int startCol = m_config.startColumn;
    int endCol = m_config.endColumn;
    
    if (startCol < 0) startCol = 0;
    if (endCol < 0 || endCol >= static_cast<int>(fields.size())) {
        endCol = static_cast<int>(fields.size()) - 1;
    }
    
    if (startCol > endCol) {
        std::swap(startCol, endCol);
    }
    
    std::vector<std::string> selectedFields;
    for (int i = startCol; i <= endCol && i < static_cast<int>(fields.size()); ++i) {
        selectedFields.push_back(fields[i]);
    }
    
    std::string key = utils::StringUtils::join(selectedFields, m_config.delimiter);
    
    if (!m_config.caseSensitive) {
        key = utils::StringUtils::toLower(key);
    }
    
    return key;
}

void FilterPipeline::addFilter(std::unique_ptr<Filter> filter) {
    if (filter) {
        m_filters.push_back(std::move(filter));
    }
}

void FilterPipeline::setDeduplicator(std::unique_ptr<Deduplicator> deduplicator) {
    m_deduplicator = std::move(deduplicator);
}

bool FilterPipeline::process(const LineData& line) {
    LineData currentLine = line;
    
    for (const auto& filter : m_filters) {
        if (!filter->shouldKeep(currentLine)) {
            m_filteredCount++;
            return false;
        }
    }
    
    if (m_deduplicator) {
        if (m_deduplicator->isDuplicate(currentLine)) {
            m_duplicateCount++;
            return false;
        }
    }
    
    m_output.push_back(currentLine);
    return true;
}

std::vector<LineData> FilterPipeline::processAll(const std::vector<LineData>& lines) {
    m_filteredCount = 0;
    m_duplicateCount = 0;
    m_output.clear();
    
    if (m_deduplicator) {
        m_deduplicator->reset();
    }
    
    for (const auto& line : lines) {
        process(line);
    }
    
    return m_output;
}

size_t FilterPipeline::getFilteredCount() const {
    return m_filteredCount;
}

size_t FilterPipeline::getDuplicateCount() const {
    return m_duplicateCount;
}

size_t FilterPipeline::getOutputCount() const {
    return m_output.size();
}

}
}
