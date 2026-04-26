#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "combinetool/types.h"

namespace combinetool {
namespace utils {

struct TimestampParseResult {
    bool success;
    std::chrono::system_clock::time_point timestamp;
    int64_t unixTimestamp;
    int64_t unixTimestampMs;
    std::string originalValue;
    size_t startPosition;
    size_t endPosition;
};

class TimestampExtractor {
public:
    explicit TimestampExtractor(const TimestampConfig& config);
    ~TimestampExtractor() = default;
    
    TimestampParseResult extract(const std::string& line);
    TimestampParseResult extractFromColumn(const std::string& line, size_t columnIndex, const std::string& delimiter);
    
    static TimestampParseResult parseISO8601(const std::string& str);
    static TimestampParseResult parseRFC2822(const std::string& str);
    static TimestampParseResult parseUnixTimestamp(const std::string& str);
    static TimestampParseResult parseCustom(const std::string& str, const std::string& pattern);
    
    static std::chrono::system_clock::time_point fromUnixTimestamp(int64_t seconds);
    static int64_t toUnixTimestamp(const std::chrono::system_clock::time_point& tp);
    static int64_t toUnixTimestampMs(const std::chrono::system_clock::time_point& tp);
    
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp, const std::string& format);
    static std::string formatTimestampISO8601(const std::chrono::system_clock::time_point& tp);
    
    static TimestampParseResult autoDetectAndParse(const std::string& str);
    
    void setConfig(const TimestampConfig& config);
    const TimestampConfig& getConfig() const;

private:
    TimestampConfig m_config;
    
    static bool isDigit(char c);
    static int parseTwoDigits(const std::string& str, size_t pos);
    static int parseFourDigits(const std::string& str, size_t pos);
    static std::chrono::system_clock::time_point makeTimePoint(
        int year, int month, int day,
        int hour, int minute, int second,
        int millisecond = 0
    );
};

class LogLineEntry {
public:
    LogLineEntry() = default;
    LogLineEntry(const std::string& line, const std::string& sourceFile, size_t lineNumber);
    
    const std::string& getLine() const;
    const std::string& getSourceFile() const;
    size_t getLineNumber() const;
    
    void setTimestamp(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point getTimestamp() const;
    bool hasTimestamp() const;
    
    bool operator<(const LogLineEntry& other) const;
    bool operator>(const LogLineEntry& other) const;
    bool operator<=(const LogLineEntry& other) const;
    bool operator>=(const LogLineEntry& other) const;

private:
    std::string m_line;
    std::string m_sourceFile;
    size_t m_lineNumber;
    std::chrono::system_clock::time_point m_timestamp;
    bool m_hasTimestamp;
};

class LogBuffer {
public:
    explicit LogBuffer(size_t maxEntries = 1000000);
    ~LogBuffer() = default;
    
    void addEntry(const LogLineEntry& entry);
    void sort();
    void clear();
    
    size_t size() const;
    bool empty() const;
    bool isFull() const;
    
    const LogLineEntry& operator[](size_t index) const;
    
    std::vector<LogLineEntry>::const_iterator begin() const;
    std::vector<LogLineEntry>::const_iterator end() const;

private:
    std::vector<LogLineEntry> m_entries;
    size_t m_maxEntries;
};

}
}
