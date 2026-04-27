#include "combinetool/utils/timestamp_extractor.h"
#include "combinetool/utils/string_utils.h"

#include <sstream>
#include <iomanip>
#include <regex>
#include <ctime>
#include <cmath>
#include <stdexcept>

namespace combinetool {
namespace utils {

TimestampExtractor::TimestampExtractor(const TimestampConfig& config)
    : m_config(config)
{
}

TimestampParseResult TimestampExtractor::extract(const std::string& line) {
    TimestampParseResult result;
    result.success = false;
    result.unixTimestamp = 0;
    result.unixTimestampMs = 0;
    result.startPosition = 0;
    result.endPosition = 0;
    
    if (m_config.extractColumn != static_cast<size_t>(-1)) {
        return extractFromColumn(line, m_config.extractColumn, m_config.columnDelimiter);
    }
    
    switch (m_config.format) {
        case TimestampFormat::ISO8601:
            result = parseISO8601(line);
            break;
        case TimestampFormat::RFC2822:
            result = parseRFC2822(line);
            break;
        case TimestampFormat::UnixTimestamp:
            result = parseUnixTimestamp(line);
            break;
        case TimestampFormat::Custom:
            result = parseCustom(line, m_config.customPattern);
            break;
        case TimestampFormat::Auto:
        default:
            result = autoDetectAndParse(line);
            break;
    }
    
    return result;
}

TimestampParseResult TimestampExtractor::extractFromColumn(
    const std::string& line, 
    size_t columnIndex, 
    const std::string& delimiter
) {
    TimestampParseResult result;
    result.success = false;
    
    auto columns = StringUtils::split(line, delimiter, true);
    if (columnIndex >= columns.size()) {
        return result;
    }
    
    std::string columnValue = StringUtils::trim(columns[columnIndex]);
    result.originalValue = columnValue;
    result.startPosition = line.find(columnValue);
    if (result.startPosition != std::string::npos) {
        result.endPosition = result.startPosition + columnValue.length();
    }
    
    switch (m_config.format) {
        case TimestampFormat::ISO8601:
            result = parseISO8601(columnValue);
            break;
        case TimestampFormat::RFC2822:
            result = parseRFC2822(columnValue);
            break;
        case TimestampFormat::UnixTimestamp:
            result = parseUnixTimestamp(columnValue);
            break;
        case TimestampFormat::Custom:
            result = parseCustom(columnValue, m_config.customPattern);
            break;
        case TimestampFormat::Auto:
        default:
            result = autoDetectAndParse(columnValue);
            break;
    }
    
    return result;
}

TimestampParseResult TimestampExtractor::parseISO8601(const std::string& str) {
    TimestampParseResult result;
    result.success = false;
    result.originalValue = str;
    
    static const std::vector<std::regex> isoPatterns = {
        std::regex(R"((\d{4})-(\d{1,2})-(\d{1,2})[T\s](\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d+))?(?:[+-]\d{2}:?\d{2}|Z)?)"),
        std::regex(R"((\d{4})-(\d{1,2})-(\d{1,2}))")
    };
    
    std::smatch match;
    for (const auto& pattern : isoPatterns) {
        if (std::regex_search(str, match, pattern)) {
            result.startPosition = match.position();
            result.endPosition = match.position() + match.length();
            
            int year = std::stoi(match[1]);
            int month = std::stoi(match[2]);
            int day = std::stoi(match[3]);
            int hour = 0;
            int minute = 0;
            int second = 0;
            int millisecond = 0;
            
            if (match.size() > 4 && match[4].matched) {
                hour = std::stoi(match[4]);
                minute = std::stoi(match[5]);
                second = std::stoi(match[6]);
                
                if (match.size() > 7 && match[7].matched) {
                    std::string msStr = match[7];
                    if (msStr.length() >= 3) {
                        millisecond = std::stoi(msStr.substr(0, 3));
                    } else if (msStr.length() == 2) {
                        millisecond = std::stoi(msStr) * 10;
                    } else if (msStr.length() == 1) {
                        millisecond = std::stoi(msStr) * 100;
                    }
                }
            }
            
            result.timestamp = makeTimePoint(year, month, day, hour, minute, second, millisecond);
            result.unixTimestamp = toUnixTimestamp(result.timestamp);
            result.unixTimestampMs = toUnixTimestampMs(result.timestamp);
            result.success = true;
            
            return result;
        }
    }
    
    return result;
}

TimestampParseResult TimestampExtractor::parseRFC2822(const std::string& str) {
    TimestampParseResult result;
    result.success = false;
    result.originalValue = str;
    
    static const std::regex rfcPattern(
        R"((Mon|Tue|Wed|Thu|Fri|Sat|Sun),\s+(\d{1,2})\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+(\d{4})\s+(\d{1,2}):(\d{1,2}):(\d{1,2})\s+([+-]\d{4}))"
    );
    
    std::smatch match;
    if (std::regex_search(str, match, rfcPattern)) {
        result.startPosition = match.position();
        result.endPosition = match.position() + match.length();
        
        int day = std::stoi(match[2]);
        
        std::string monthStr = match[3];
        int month = 1;
        if (monthStr == "Jan") month = 1;
        else if (monthStr == "Feb") month = 2;
        else if (monthStr == "Mar") month = 3;
        else if (monthStr == "Apr") month = 4;
        else if (monthStr == "May") month = 5;
        else if (monthStr == "Jun") month = 6;
        else if (monthStr == "Jul") month = 7;
        else if (monthStr == "Aug") month = 8;
        else if (monthStr == "Sep") month = 9;
        else if (monthStr == "Oct") month = 10;
        else if (monthStr == "Nov") month = 11;
        else if (monthStr == "Dec") month = 12;
        
        int year = std::stoi(match[4]);
        int hour = std::stoi(match[5]);
        int minute = std::stoi(match[6]);
        int second = std::stoi(match[7]);
        
        result.timestamp = makeTimePoint(year, month, day, hour, minute, second, 0);
        result.unixTimestamp = toUnixTimestamp(result.timestamp);
        result.unixTimestampMs = toUnixTimestampMs(result.timestamp);
        result.success = true;
    }
    
    return result;
}

TimestampParseResult TimestampExtractor::parseUnixTimestamp(const std::string& str) {
    TimestampParseResult result;
    result.success = false;
    result.originalValue = str;
    
    std::string trimmed = StringUtils::trim(str);
    
    static const std::regex unixPattern(R"(\b(\d{10,13})(?:\.\d+)?\b)");
    std::smatch match;
    
    if (std::regex_search(trimmed, match, unixPattern)) {
        result.startPosition = match.position();
        result.endPosition = match.position() + match.length();
        
        std::string tsStr = match[1];
        int64_t tsValue = 0;
        
        try {
            if (tsStr.length() == 10) {
                tsValue = std::stoll(tsStr);
                result.unixTimestamp = tsValue;
                result.unixTimestampMs = tsValue * 1000;
            } else if (tsStr.length() == 13) {
                tsValue = std::stoll(tsStr);
                result.unixTimestamp = tsValue / 1000;
                result.unixTimestampMs = tsValue;
            } else if (tsStr.length() >= 11) {
                int64_t magnitude = static_cast<int64_t>(std::pow(10, tsStr.length() - 10));
                tsValue = std::stoll(tsStr);
                result.unixTimestamp = tsValue / magnitude;
                result.unixTimestampMs = (tsValue / magnitude) * 1000;
            }
            
            result.timestamp = fromUnixTimestamp(result.unixTimestamp);
            result.success = true;
        } catch (const std::exception&) {
            result.success = false;
        }
    }
    
    return result;
}

TimestampParseResult TimestampExtractor::parseCustom(const std::string& str, const std::string& pattern) {
    TimestampParseResult result;
    result.success = false;
    result.originalValue = str;
    
    std::istringstream iss(str);
    std::tm tm = {};
    
    if (pattern == "%Y-%m-%d %H:%M:%S" || pattern == "%Y-%m-%dT%H:%M:%S") {
        result = parseISO8601(str);
        return result;
    }
    
    std::string processedPattern = pattern;
    if (processedPattern.find("%Y") != std::string::npos &&
        processedPattern.find("%m") != std::string::npos &&
        processedPattern.find("%d") != std::string::npos) {
        
        static const std::regex datePattern(
            R"((\d{4})[-/\.](\d{1,2})[-/\.](\d{1,2})(?:[T\s](\d{1,2}):(\d{1,2})(?::(\d{1,2}))?(?:\.(\d+))?)?)"
        );
        
        std::smatch match;
        if (std::regex_search(str, match, datePattern)) {
            result.startPosition = match.position();
            result.endPosition = match.position() + match.length();
            
            int year = std::stoi(match[1]);
            int month = std::stoi(match[2]);
            int day = std::stoi(match[3]);
            int hour = 0;
            int minute = 0;
            int second = 0;
            int millisecond = 0;
            
            if (match.size() > 4 && match[4].matched) {
                hour = std::stoi(match[4]);
                if (match.size() > 5 && match[5].matched) {
                    minute = std::stoi(match[5]);
                }
                if (match.size() > 6 && match[6].matched) {
                    second = std::stoi(match[6]);
                }
                if (match.size() > 7 && match[7].matched) {
                    std::string msStr = match[7];
                    if (msStr.length() >= 3) {
                        millisecond = std::stoi(msStr.substr(0, 3));
                    }
                }
            }
            
            result.timestamp = makeTimePoint(year, month, day, hour, minute, second, millisecond);
            result.unixTimestamp = toUnixTimestamp(result.timestamp);
            result.unixTimestampMs = toUnixTimestampMs(result.timestamp);
            result.success = true;
        }
    }
    
    return result;
}

std::chrono::system_clock::time_point TimestampExtractor::fromUnixTimestamp(int64_t seconds) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

int64_t TimestampExtractor::toUnixTimestamp(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()
    ).count();
}

int64_t TimestampExtractor::toUnixTimestampMs(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count();
}

std::string TimestampExtractor::formatTimestamp(
    const std::chrono::system_clock::time_point& tp, 
    const std::string& format
) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    return oss.str();
}

std::string TimestampExtractor::formatTimestampISO8601(const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration - seconds);
    
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (milliseconds.count() > 0) {
        oss << "." << std::setfill('0') << std::setw(3) << milliseconds.count();
    }
    
    oss << "Z";
    return oss.str();
}

TimestampParseResult TimestampExtractor::autoDetectAndParse(const std::string& str) {
    TimestampParseResult result;
    
    result = parseUnixTimestamp(str);
    if (result.success) {
        return result;
    }
    
    result = parseISO8601(str);
    if (result.success) {
        return result;
    }
    
    result = parseRFC2822(str);
    if (result.success) {
        return result;
    }
    
    result.success = false;
    return result;
}

void TimestampExtractor::setConfig(const TimestampConfig& config) {
    m_config = config;
}

const TimestampConfig& TimestampExtractor::getConfig() const {
    return m_config;
}

bool TimestampExtractor::isDigit(char c) {
    return c >= '0' && c <= '9';
}

int TimestampExtractor::parseTwoDigits(const std::string& str, size_t pos) {
    if (pos + 1 >= str.length()) return 0;
    return (str[pos] - '0') * 10 + (str[pos + 1] - '0');
}

int TimestampExtractor::parseFourDigits(const std::string& str, size_t pos) {
    if (pos + 3 >= str.length()) return 0;
    return (str[pos] - '0') * 1000 + 
           (str[pos + 1] - '0') * 100 + 
           (str[pos + 2] - '0') * 10 + 
           (str[pos + 3] - '0');
}

std::chrono::system_clock::time_point TimestampExtractor::makeTimePoint(
    int year, int month, int day,
    int hour, int minute, int second,
    int millisecond
) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    
    auto time_t = std::mktime(&tm);
    if (time_t == -1) {
        return std::chrono::system_clock::time_point();
    }
    
    auto tp = std::chrono::system_clock::from_time_t(time_t);
    tp += std::chrono::milliseconds(millisecond);
    
    return tp;
}

LogLineEntry::LogLineEntry(const std::string& line, const std::string& sourceFile, size_t lineNumber)
    : m_line(line)
    , m_sourceFile(sourceFile)
    , m_lineNumber(lineNumber)
    , m_hasTimestamp(false)
{
}

const std::string& LogLineEntry::getLine() const {
    return m_line;
}

const std::string& LogLineEntry::getSourceFile() const {
    return m_sourceFile;
}

size_t LogLineEntry::getLineNumber() const {
    return m_lineNumber;
}

void LogLineEntry::setTimestamp(const std::chrono::system_clock::time_point& tp) {
    m_timestamp = tp;
    m_hasTimestamp = true;
}

std::chrono::system_clock::time_point LogLineEntry::getTimestamp() const {
    return m_timestamp;
}

bool LogLineEntry::hasTimestamp() const {
    return m_hasTimestamp;
}

bool LogLineEntry::operator<(const LogLineEntry& other) const {
    if (m_hasTimestamp && other.m_hasTimestamp) {
        if (m_timestamp != other.m_timestamp) {
            return m_timestamp < other.m_timestamp;
        }
    }
    
    if (m_sourceFile != other.m_sourceFile) {
        return m_sourceFile < other.m_sourceFile;
    }
    
    return m_lineNumber < other.m_lineNumber;
}

bool LogLineEntry::operator>(const LogLineEntry& other) const {
    return other < *this;
}

bool LogLineEntry::operator<=(const LogLineEntry& other) const {
    return !(*this > other);
}

bool LogLineEntry::operator>=(const LogLineEntry& other) const {
    return !(*this < other);
}

LogBuffer::LogBuffer(size_t maxEntries)
    : m_maxEntries(maxEntries)
{
    m_entries.reserve(std::min(maxEntries, static_cast<size_t>(100000)));
}

void LogBuffer::addEntry(const LogLineEntry& entry) {
    if (m_entries.size() < m_maxEntries) {
        m_entries.push_back(entry);
    }
}

void LogBuffer::sort() {
    std::sort(m_entries.begin(), m_entries.end());
}

void LogBuffer::clear() {
    m_entries.clear();
}

size_t LogBuffer::size() const {
    return m_entries.size();
}

bool LogBuffer::empty() const {
    return m_entries.empty();
}

bool LogBuffer::isFull() const {
    return m_entries.size() >= m_maxEntries;
}

const LogLineEntry& LogBuffer::operator[](size_t index) const {
    return m_entries[index];
}

std::vector<LogLineEntry>::const_iterator LogBuffer::begin() const {
    return m_entries.begin();
}

std::vector<LogLineEntry>::const_iterator LogBuffer::end() const {
    return m_entries.end();
}

}
}
