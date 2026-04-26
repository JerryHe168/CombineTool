#pragma once

#include <string>
#include <vector>
#include <functional>
#include <regex>
#include <unordered_set>
#include "combinetool/types.h"

namespace combinetool {
namespace filter {

class Filter {
public:
    explicit Filter(const FilterConfig& config);
    
    bool shouldKeep(const LineData& line) const;
    bool shouldKeep(const std::string& content) const;
    
    bool matchesInclude(const std::string& content) const;
    bool matchesExclude(const std::string& content) const;
    
    bool isBlankLine(const std::string& content) const;
    bool isCommentLine(const std::string& content) const;
    
    void setCustomFilter(std::function<bool(const LineData&)> customFilter);

private:
    FilterConfig m_config;
    std::vector<std::regex> m_includeRegexes;
    std::vector<std::regex> m_excludeRegexes;
    
    void compilePatterns();
    
    bool matchesPatterns(
        const std::string& content,
        const std::vector<std::string>& patterns,
        const std::vector<std::regex>& regexes
    ) const;
    
    bool matchesWildCard(const std::string& pattern, const std::string& content) const;
};

class Deduplicator {
public:
    explicit Deduplicator(const DeduplicationConfig& config);
    
    void reset();
    
    bool isDuplicate(const LineData& line);
    bool isDuplicate(const std::string& content);
    
    size_t getDuplicateCount() const;
    size_t getUniqueCount() const;
    
    const std::vector<LineData>& getUniqueLines() const;

private:
    DeduplicationConfig m_config;
    std::unordered_set<std::string> m_seenKeys;
    std::vector<LineData> m_uniqueLines;
    size_t m_duplicateCount;
    
    std::string generateKey(const std::string& content) const;
    std::string extractColumns(const std::string& content) const;
};

class FilterPipeline {
public:
    void addFilter(std::unique_ptr<Filter> filter);
    void setDeduplicator(std::unique_ptr<Deduplicator> deduplicator);
    
    bool process(const LineData& line);
    std::vector<LineData> processAll(const std::vector<LineData>& lines);
    
    size_t getFilteredCount() const;
    size_t getDuplicateCount() const;
    size_t getOutputCount() const;

private:
    std::vector<std::unique_ptr<Filter>> m_filters;
    std::unique_ptr<Deduplicator> m_deduplicator;
    std::vector<LineData> m_output;
    size_t m_filteredCount;
    size_t m_duplicateCount;
};

}
}
