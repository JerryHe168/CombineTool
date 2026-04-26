#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace combinetool {

enum class MergeMode {
    Sequential,
    Interleaved,
    Conditional,
    TimestampSorted,
    Binary
};

enum class TimestampFormat {
    Auto,
    ISO8601,
    RFC2822,
    UnixTimestamp,
    Custom
};

enum class FileType {
    AutoDetect,
    Text,
    Binary
};

struct TimestampConfig {
    TimestampFormat format;
    std::string customPattern;
    size_t extractColumn;
    std::string columnDelimiter;
};

struct SmartIOConfig {
    bool useSmartIO;
    uint64_t memoryMapThreshold;
    size_t streamBufferSize;
};

struct BinaryMergeConfig {
    FileType inputFileType;
    FileType outputFileType;
    size_t chunkSize;
    bool preserveHeaders;
};

enum class TextFormat {
    Unknown,
    PlainText,
    CSV,
    TSV,
    JSON,
    XML,
    Log
};

enum class Encoding {
    Unknown,
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_BE,
    UTF32_LE,
    UTF32_BE,
    GBK,
    GB2312,
    GB18030,
    BIG5,
    ISO8859_1,
    ASCII
};

enum class DeduplicationMode {
    None,
    FullLine,
    Partial,
    ColumnBased
};

enum class DeduplicationKeepStrategy {
    First,
    Last,
    All
};

enum class FilterMode {
    Include,
    Exclude,
    Custom
};

struct FileInfo {
    std::string path;
    std::string filename;
    std::string extension;
    uint64_t size;
    TextFormat format;
    Encoding encoding;
    std::string delimiter;
    bool hasHeader;
    std::vector<std::string> headerColumns;
};

struct FilterConfig {
    FilterMode mode;
    std::vector<std::string> includePatterns;
    std::vector<std::string> excludePatterns;
    bool filterBlankLines;
    bool filterCommentLines;
    std::string commentPrefix;
    bool caseSensitive;
    std::function<bool(const std::string&)> customFilter;
};

struct DeduplicationConfig {
    DeduplicationMode mode;
    DeduplicationKeepStrategy keepStrategy;
    int startColumn;
    int endColumn;
    std::string delimiter;
    bool caseSensitive;
};

struct MergeConfig {
    MergeMode mode;
    std::vector<std::string> inputFiles;
    std::string outputFile;
    Encoding targetEncoding;
    std::string outputDelimiter;
    bool recursive;
    bool useMemoryMapping;
    size_t bufferSize;
    bool keepOriginalHeaders;
    bool outputHeader;
    std::string conditionalExpression;
    FilterConfig filterConfig;
    DeduplicationConfig deduplicationConfig;
    
    TimestampConfig timestampConfig;
    SmartIOConfig smartIOConfig;
    BinaryMergeConfig binaryConfig;
};

struct LineData {
    std::string content;
    size_t lineNumber;
    std::string sourceFile;
    bool isValid;
};

using FilterCallback = std::function<bool(const LineData&)>;

}
