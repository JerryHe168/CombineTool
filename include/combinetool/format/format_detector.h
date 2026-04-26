#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "combinetool/types.h"

namespace combinetool {
namespace format {

struct FormatDetectionResult {
    TextFormat format;
    std::string formatName;
    float confidence;
    
    std::string detectedDelimiter;
    float delimiterConfidence;
    
    bool hasHeader;
    std::vector<std::string> headerColumns;
    float headerConfidence;
    
    size_t sampleLines;
    size_t totalLinesEstimate;
};

struct ColumnInfo {
    size_t index;
    std::string name;
    bool isNumeric;
    bool isBoolean;
    bool isDateTime;
    size_t maxLength;
    size_t minLength;
    size_t nullCount;
};

class FormatDetector {
public:
    static FormatDetectionResult detectFromFile(const std::string& filePath);
    static FormatDetectionResult detectFromContent(const std::string& content);
    static FormatDetectionResult detectFromLines(const std::vector<std::string>& lines);
    
    static std::string detectDelimiter(const std::vector<std::string>& lines);
    static std::string detectDelimiterFromLine(const std::string& line);
    
    static bool detectHasHeader(const std::vector<std::string>& lines, const std::string& delimiter);
    static std::vector<ColumnInfo> analyzeColumns(
        const std::vector<std::string>& lines,
        const std::string& delimiter,
        bool hasHeader
    );
    
    static TextFormat detectFormatByName(const std::string& name);
    static std::string formatToString(TextFormat format);
    
    static std::vector<std::string> parseCSVLine(const std::string& line, const std::string& delimiter);
    static std::string formatCSVLine(const std::vector<std::string>& fields, const std::string& delimiter);
    
    static bool isNumericField(const std::string& value);
    static bool isBooleanField(const std::string& value);
    static bool isDateTimeField(const std::string& value);
    static bool isNullValue(const std::string& value);

private:
    static TextFormat detectByExtension(const std::string& extension);
    static TextFormat detectByContentPattern(const std::vector<std::string>& lines);
    
    static float calculateDelimiterConfidence(
        const std::vector<std::string>& lines,
        const std::string& delimiter
    );
    
    static float calculateHeaderConfidence(
        const std::vector<std::string>& lines,
        const std::string& delimiter
    );
    
    static bool looksLikeHeaderRow(
        const std::vector<std::string>& fields,
        const std::vector<std::vector<std::string>>& dataRows
    );
    
    static bool hasConsistentColumnCount(
        const std::vector<std::string>& lines,
        const std::string& delimiter
    );
    
    static std::vector<std::string> getCommonDelimiters();
    
    static bool isCSVFormat(const std::vector<std::string>& lines);
    static bool isTSVFormat(const std::vector<std::string>& lines);
    static bool isJSONFormat(const std::vector<std::string>& lines);
    static bool isXMLFormat(const std::vector<std::string>& lines);
    static bool isLogFormat(const std::vector<std::string>& lines);
};

}
}
