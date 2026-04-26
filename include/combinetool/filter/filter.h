#pragma once

#include <string>
#include <vector>
#include <functional>
#include <regex>
#include <unordered_set>
#include <memory>
#include <mutex>
#include "combinetool/types.h"

namespace combinetool {
namespace filter {

class RegexCache {
public:
    struct CacheKey {
        std::vector<std::string> includePatterns;
        std::vector<std::string> excludePatterns;
        bool caseSensitive;
        
        bool operator==(const CacheKey& other) const;
    };
    
    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const;
    };
    
    struct CachedRegexes {
        std::vector<std::shared_ptr<std::regex>> includeRegexes;
        std::vector<std::shared_ptr<std::regex>> excludeRegexes;
    };
    
    static RegexCache& instance();
    
    std::shared_ptr<CachedRegexes> getOrCreate(
        const std::vector<std::string>& includePatterns,
        const std::vector<std::string>& excludePatterns,
        bool caseSensitive
    );
    
    void clear();

private:
    RegexCache() = default;
    
    std::unordered_map<CacheKey, std::shared_ptr<CachedRegexes>, CacheKeyHash> m_cache;
    std::mutex m_mutex;
};

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
    std::shared_ptr<RegexCache::CachedRegexes> m_cachedRegexes;
    
    void compilePatterns();
    
    bool matchesPatterns(
        const std::string& content,
        const std::vector<std::string>& patterns,
        const std::vector<std::shared_ptr<std::regex>>& regexes
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
