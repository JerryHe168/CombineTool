#include "combinetool/format/format_detector.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/config.h"

#include <algorithm>
#include <set>
#include <regex>
#include <cctype>
#include <sstream>

namespace combinetool {
namespace format {

FormatDetectionResult FormatDetector::detectFromFile(const std::string& filePath) {
    FormatDetectionResult result;
    result.format = TextFormat::Unknown;
    result.formatName = "Unknown";
    result.confidence = 0.0f;
    result.detectedDelimiter = config::DEFAULT_DELIMITER;
    result.delimiterConfidence = 0.0f;
    result.hasHeader = config::DEFAULT_OUTPUT_HEADER;
    result.headerConfidence = 0.0f;
    result.sampleLines = 0;
    result.totalLinesEstimate = 0;
    
    auto lines = utils::FileUtils::readAllLines(filePath);
    if (lines.empty()) {
        return result;
    }
    
    std::vector<std::string> sampleLines;
    size_t maxLines = std::min(lines.size(), config::HEADER_DETECTION_LINES + 20);
    for (size_t i = 0; i < maxLines && i < lines.size(); ++i) {
        sampleLines.push_back(lines[i]);
    }
    
    result.sampleLines = sampleLines.size();
    result.totalLinesEstimate = lines.size();
    
    std::string extension = utils::PathUtils::getExtension(filePath);
    TextFormat extFormat = detectByExtension(extension);
    
    TextFormat contentFormat = detectByContentPattern(sampleLines);
    
    if (extFormat == contentFormat && extFormat != TextFormat::Unknown) {
        result.format = extFormat;
        result.confidence = 0.9f;
    } else if (contentFormat != TextFormat::Unknown) {
        result.format = contentFormat;
        result.confidence = 0.7f;
    } else if (extFormat != TextFormat::Unknown) {
        result.format = extFormat;
        result.confidence = 0.5f;
    } else {
        result.format = TextFormat::PlainText;
        result.confidence = 0.3f;
    }
    result.formatName = formatToString(result.format);
    
    if (result.format == TextFormat::CSV || result.format == TextFormat::TSV) {
        result.detectedDelimiter = detectDelimiter(sampleLines);
        result.delimiterConfidence = calculateDelimiterConfidence(sampleLines, result.detectedDelimiter);
        
        result.hasHeader = detectHasHeader(sampleLines, result.detectedDelimiter);
        result.headerConfidence = calculateHeaderConfidence(sampleLines, result.detectedDelimiter);
        
        if (result.hasHeader && !sampleLines.empty()) {
            result.headerColumns = parseCSVLine(sampleLines[0], result.detectedDelimiter);
        }
    } else if (result.format == TextFormat::TSV) {
        result.detectedDelimiter = "\t";
        result.delimiterConfidence = 0.95f;
    } else {
        result.detectedDelimiter = ",";
        result.delimiterConfidence = 0.3f;
    }
    
    return result;
}

FormatDetectionResult FormatDetector::detectFromContent(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    return detectFromLines(lines);
}

FormatDetectionResult FormatDetector::detectFromLines(const std::vector<std::string>& lines) {
    FormatDetectionResult result;
    result.format = TextFormat::Unknown;
    result.formatName = "Unknown";
    result.confidence = 0.0f;
    result.detectedDelimiter = config::DEFAULT_DELIMITER;
    result.delimiterConfidence = 0.0f;
    result.hasHeader = false;
    result.headerConfidence = 0.0f;
    result.sampleLines = 0;
    result.totalLinesEstimate = 0;
    
    if (lines.empty()) {
        return result;
    }
    
    result.sampleLines = lines.size();
    result.totalLinesEstimate = lines.size();
    
    result.format = detectByContentPattern(lines);
    if (result.format == TextFormat::Unknown) {
        result.format = TextFormat::PlainText;
        result.confidence = 0.3f;
    } else {
        result.confidence = 0.7f;
    }
    result.formatName = formatToString(result.format);
    
    if (result.format == TextFormat::CSV || result.format == TextFormat::TSV) {
        result.detectedDelimiter = detectDelimiter(lines);
        result.delimiterConfidence = calculateDelimiterConfidence(lines, result.detectedDelimiter);
        
        result.hasHeader = detectHasHeader(lines, result.detectedDelimiter);
        result.headerConfidence = calculateHeaderConfidence(lines, result.detectedDelimiter);
        
        if (result.hasHeader && !lines.empty()) {
            result.headerColumns = parseCSVLine(lines[0], result.detectedDelimiter);
        }
    }
    
    return result;
}

std::string FormatDetector::detectDelimiter(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return config::DEFAULT_DELIMITER;
    }
    
    auto delimiters = getCommonDelimiters();
    
    std::map<std::string, float> scores;
    
    for (const auto& delim : delimiters) {
        scores[delim] = calculateDelimiterConfidence(lines, delim);
    }
    
    std::string bestDelimiter = config::DEFAULT_DELIMITER;
    float bestScore = 0.0f;
    
    for (const auto& pair : scores) {
        if (pair.second > bestScore) {
            bestScore = pair.second;
            bestDelimiter = pair.first;
        }
    }
    
    return bestDelimiter;
}

std::string FormatDetector::detectDelimiterFromLine(const std::string& line) {
    auto delimiters = getCommonDelimiters();
    
    std::map<std::string, size_t> counts;
    
    for (const auto& delim : delimiters) {
        counts[delim] = utils::StringUtils::countOccurrences(line, delim);
    }
    
    std::string bestDelimiter = config::DEFAULT_DELIMITER;
    size_t maxCount = 0;
    
    for (const auto& pair : counts) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            bestDelimiter = pair.first;
        }
    }
    
    return bestDelimiter;
}

bool FormatDetector::detectHasHeader(const std::vector<std::string>& lines, const std::string& delimiter) {
    if (lines.size() < 2) {
        return false;
    }
    
    float confidence = calculateHeaderConfidence(lines, delimiter);
    return confidence > 0.5f;
}

std::vector<ColumnInfo> FormatDetector::analyzeColumns(
    const std::vector<std::string>& lines,
    const std::string& delimiter,
    bool hasHeader
) {
    std::vector<ColumnInfo> result;
    
    if (lines.empty()) {
        return result;
    }
    
    size_t startIndex = hasHeader ? 1 : 0;
    size_t numLines = lines.size();
    
    if (startIndex >= numLines) {
        return result;
    }
    
    auto firstRow = parseCSVLine(lines[startIndex], delimiter);
    size_t numColumns = firstRow.size();
    
    result.resize(numColumns);
    for (size_t i = 0; i < numColumns; ++i) {
        result[i].index = i;
        result[i].name = hasHeader ? parseCSVLine(lines[0], delimiter)[i] : "col_" + std::to_string(i);
        result[i].isNumeric = true;
        result[i].isBoolean = true;
        result[i].isDateTime = true;
        result[i].maxLength = 0;
        result[i].minLength = static_cast<size_t>(-1);
        result[i].nullCount = 0;
    }
    
    for (size_t lineIdx = startIndex; lineIdx < numLines; ++lineIdx) {
        auto fields = parseCSVLine(lines[lineIdx], delimiter);
        
        for (size_t colIdx = 0; colIdx < std::min(fields.size(), numColumns); ++colIdx) {
            const auto& value = fields[colIdx];
            auto& colInfo = result[colIdx];
            
            if (isNullValue(value)) {
                colInfo.nullCount++;
                continue;
            }
            
            size_t len = value.length();
            colInfo.maxLength = std::max(colInfo.maxLength, len);
            colInfo.minLength = std::min(colInfo.minLength, len);
            
            if (colInfo.isNumeric && !isNumericField(value)) {
                colInfo.isNumeric = false;
            }
            if (colInfo.isBoolean && !isBooleanField(value)) {
                colInfo.isBoolean = false;
            }
            if (colInfo.isDateTime && !isDateTimeField(value)) {
                colInfo.isDateTime = false;
            }
        }
    }
    
    for (auto& colInfo : result) {
        if (colInfo.minLength == static_cast<size_t>(-1)) {
            colInfo.minLength = 0;
        }
    }
    
    return result;
}

TextFormat FormatDetector::detectFormatByName(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "csv") return TextFormat::CSV;
    if (lower == "tsv" || lower == "tab") return TextFormat::TSV;
    if (lower == "json") return TextFormat::JSON;
    if (lower == "xml") return TextFormat::XML;
    if (lower == "log") return TextFormat::Log;
    if (lower == "plain" || lower == "text" || lower == "txt") return TextFormat::PlainText;
    
    return TextFormat::Unknown;
}

std::string FormatDetector::formatToString(TextFormat format) {
    switch (format) {
        case TextFormat::PlainText: return "PlainText";
        case TextFormat::CSV: return "CSV";
        case TextFormat::TSV: return "TSV";
        case TextFormat::JSON: return "JSON";
        case TextFormat::XML: return "XML";
        case TextFormat::Log: return "Log";
        case TextFormat::Unknown:
        default: return "Unknown";
    }
}

std::vector<std::string> FormatDetector::parseCSVLine(const std::string& line, const std::string& delimiter) {
    std::vector<std::string> fields;
    
    if (line.empty()) {
        return fields;
    }
    
    if (delimiter.length() != 1) {
        return utils::StringUtils::split(line, delimiter, true);
    }
    
    char delim = delimiter[0];
    size_t i = 0;
    size_t len = line.length();
    bool inQuote = false;
    std::string currentField;
    
    while (i < len) {
        char c = line[i];
        
        if (c == '"') {
            if (inQuote && i + 1 < len && line[i + 1] == '"') {
                currentField += '"';
                i += 2;
                continue;
            }
            inQuote = !inQuote;
            i++;
            continue;
        }
        
        if (c == delim && !inQuote) {
            fields.push_back(currentField);
            currentField.clear();
            i++;
            continue;
        }
        
        currentField += c;
        i++;
    }
    
    fields.push_back(currentField);
    return fields;
}

std::string FormatDetector::formatCSVLine(const std::vector<std::string>& fields, const std::string& delimiter) {
    if (fields.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        
        if (i > 0) {
            oss << delimiter;
        }
        
        bool needsQuote = field.find(delimiter) != std::string::npos ||
                          field.find('"') != std::string::npos ||
                          field.find('\n') != std::string::npos ||
                          field.find('\r') != std::string::npos;
        
        if (needsQuote) {
            oss << '"';
            for (char c : field) {
                if (c == '"') {
                    oss << "\"\"";
                } else {
                    oss << c;
                }
            }
            oss << '"';
        } else {
            oss << field;
        }
    }
    
    return oss.str();
}

bool FormatDetector::isNumericField(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    
    std::string trimmed = utils::StringUtils::trim(value);
    if (trimmed.empty()) {
        return false;
    }
    
    size_t start = 0;
    if (trimmed[0] == '+' || trimmed[0] == '-') {
        start = 1;
    }
    
    bool hasDecimal = false;
    bool hasExponent = false;
    
    for (size_t i = start; i < trimmed.length(); ++i) {
        char c = trimmed[i];
        
        if (std::isdigit(static_cast<unsigned char>(c))) {
            continue;
        }
        
        if (c == '.' && !hasDecimal && !hasExponent) {
            hasDecimal = true;
            continue;
        }
        
        if ((c == 'e' || c == 'E') && !hasExponent && i > start) {
            hasExponent = true;
            if (i + 1 < trimmed.length() && (trimmed[i + 1] == '+' || trimmed[i + 1] == '-')) {
                i++;
            }
            continue;
        }
        
        return false;
    }
    
    if (start == trimmed.length()) {
        return false;
    }
    
    return true;
}

bool FormatDetector::isBooleanField(const std::string& value) {
    std::string lower = utils::StringUtils::toLower(utils::StringUtils::trim(value));
    
    return lower == "true" || lower == "false" ||
           lower == "yes" || lower == "no" ||
           lower == "1" || lower == "0" ||
           lower == "on" || lower == "off";
}

bool FormatDetector::isDateTimeField(const std::string& value) {
    if (value.length() < 6) {
        return false;
    }
    
    std::string trimmed = utils::StringUtils::trim(value);
    
    static const std::vector<std::regex> datePatterns = {
        std::regex(R"(\d{4}[-/]\d{1,2}[-/]\d{1,2})"),
        std::regex(R"(\d{1,2}[-/]\d{1,2}[-/]\d{2,4})"),
        std::regex(R"(\d{4}[-/]\d{1,2}[-/]\d{1,2}\s+\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?(\s*[APap][Mm])?)"),
        std::regex(R"(\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?(\s*[APap][Mm])?)"),
        std::regex(R"(\d{8})"),
        std::regex(R"(\d{12})"),
    };
    
    for (const auto& pattern : datePatterns) {
        if (std::regex_search(trimmed, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool FormatDetector::isNullValue(const std::string& value) {
    std::string lower = utils::StringUtils::toLower(utils::StringUtils::trim(value));
    
    return lower.empty() || lower == "null" || lower == "nil" ||
           lower == "none" || lower == "na" || lower == "n/a" ||
           lower == "undefined" || lower == "?";
}

TextFormat FormatDetector::detectByExtension(const std::string& extension) {
    std::string lower = extension;
    if (!lower.empty() && lower[0] == '.') {
        lower = lower.substr(1);
    }
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "csv") return TextFormat::CSV;
    if (lower == "tsv" || lower == "tab") return TextFormat::TSV;
    if (lower == "json") return TextFormat::JSON;
    if (lower == "xml") return TextFormat::XML;
    if (lower == "log") return TextFormat::Log;
    if (lower == "txt" || lower == "text") return TextFormat::PlainText;
    
    return TextFormat::Unknown;
}

TextFormat FormatDetector::detectByContentPattern(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return TextFormat::Unknown;
    }
    
    if (isJSONFormat(lines)) {
        return TextFormat::JSON;
    }
    
    if (isXMLFormat(lines)) {
        return TextFormat::XML;
    }
    
    if (isLogFormat(lines)) {
        return TextFormat::Log;
    }
    
    if (isTSVFormat(lines)) {
        return TextFormat::TSV;
    }
    
    if (isCSVFormat(lines)) {
        return TextFormat::CSV;
    }
    
    return TextFormat::PlainText;
}

float FormatDetector::calculateDelimiterConfidence(
    const std::vector<std::string>& lines,
    const std::string& delimiter
) {
    if (lines.empty() || delimiter.empty()) {
        return 0.0f;
    }
    
    size_t consistentRows = 0;
    size_t totalRows = 0;
    std::set<size_t> columnCounts;
    
    for (const auto& line : lines) {
        if (utils::StringUtils::isBlank(line)) {
            continue;
        }
        
        auto fields = parseCSVLine(line, delimiter);
        columnCounts.insert(fields.size());
        totalRows++;
    }
    
    if (totalRows == 0) {
        return 0.0f;
    }
    
    if (columnCounts.size() == 1) {
        size_t columns = *columnCounts.begin();
        if (columns >= 2) {
            return 0.9f;
        }
        if (columns == 1) {
            return 0.1f;
        }
    }
    
    float consistency = 1.0f - static_cast<float>(columnCounts.size() - 1) / totalRows;
    size_t maxColumns = 0;
    for (auto count : columnCounts) {
        maxColumns = std::max(maxColumns, count);
    }
    
    if (maxColumns >= 2) {
        return 0.5f + 0.3f * consistency;
    }
    
    return 0.1f;
}

float FormatDetector::calculateHeaderConfidence(
    const std::vector<std::string>& lines,
    const std::string& delimiter
) {
    if (lines.size() < 2) {
        return 0.0f;
    }
    
    auto firstLine = parseCSVLine(lines[0], delimiter);
    size_t numColumns = firstLine.size();
    
    if (numColumns < 1) {
        return 0.0f;
    }
    
    float score = 0.0f;
    
    bool allText = true;
    for (const auto& field : firstLine) {
        if (isNumericField(field)) {
            allText = false;
            break;
        }
    }
    if (allText) {
        score += 0.3f;
    }
    
    for (const auto& field : firstLine) {
        if (field.find_first_of(" :;,.?!@#$%^&*()[]{}<>") != std::string::npos) {
            if (isNumericField(field)) {
                score -= 0.1f;
            }
        }
    }
    
    std::vector<std::vector<std::string>> dataRows;
    for (size_t i = 1; i < lines.size(); ++i) {
        auto fields = parseCSVLine(lines[i], delimiter);
        if (fields.size() == numColumns) {
            dataRows.push_back(fields);
        }
    }
    
    if (dataRows.size() >= 1) {
        bool typeConsistent = true;
        for (size_t col = 0; col < numColumns; ++col) {
            bool firstIsNumeric = isNumericField(firstLine[col]);
            
            size_t numericInData = 0;
            for (const auto& row : dataRows) {
                if (col < row.size() && isNumericField(row[col])) {
                    numericInData++;
                }
            }
            
            float dataNumericRatio = static_cast<float>(numericInData) / dataRows.size();
            
            if (firstIsNumeric && dataNumericRatio < 0.5f) {
                typeConsistent = false;
            }
            if (!firstIsNumeric && dataNumericRatio > 0.7f) {
                typeConsistent = true;
            }
        }
        
        if (typeConsistent) {
            score += 0.3f;
        }
    }
    
    size_t uniqueFirst = 0;
    for (const auto& field : firstLine) {
        std::string lower = utils::StringUtils::toLower(utils::StringUtils::trim(field));
        if (lower.find("id") != std::string::npos ||
            lower.find("name") != std::string::npos ||
            lower.find("date") != std::string::npos ||
            lower.find("time") != std::string::npos ||
            lower.find("value") != std::string::npos ||
            lower.find("count") != std::string::npos ||
            lower.find("sum") != std::string::npos ||
            lower.find("total") != std::string::npos ||
            lower.find("price") != std::string::npos ||
            lower.find("code") != std::string::npos) {
            uniqueFirst++;
        }
    }
    score += static_cast<float>(uniqueFirst) * 0.1f / numColumns;
    
    return std::max(0.0f, std::min(1.0f, score));
}

bool FormatDetector::looksLikeHeaderRow(
    const std::vector<std::string>& fields,
    const std::vector<std::vector<std::string>>& dataRows
) {
    if (fields.empty()) {
        return false;
    }
    
    bool allText = true;
    for (const auto& field : fields) {
        if (isNumericField(field)) {
            allText = false;
            break;
        }
    }
    
    return allText;
}

bool FormatDetector::hasConsistentColumnCount(
    const std::vector<std::string>& lines,
    const std::string& delimiter
) {
    if (lines.empty()) {
        return true;
    }
    
    size_t expectedColumns = 0;
    for (const auto& line : lines) {
        if (utils::StringUtils::isBlank(line)) {
            continue;
        }
        
        auto fields = parseCSVLine(line, delimiter);
        if (expectedColumns == 0) {
            expectedColumns = fields.size();
        } else if (fields.size() != expectedColumns) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> FormatDetector::getCommonDelimiters() {
    return { ",", "\t", "|", ";", " ", ":" };
}

bool FormatDetector::isCSVFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }
    
    float commaConfidence = calculateDelimiterConfidence(lines, ",");
    float tabConfidence = calculateDelimiterConfidence(lines, "\t");
    
    if (commaConfidence > 0.5f && commaConfidence >= tabConfidence) {
        return true;
    }
    
    return false;
}

bool FormatDetector::isTSVFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }
    
    float tabConfidence = calculateDelimiterConfidence(lines, "\t");
    float commaConfidence = calculateDelimiterConfidence(lines, ",");
    
    if (tabConfidence > 0.5f && tabConfidence > commaConfidence) {
        return true;
    }
    
    return false;
}

bool FormatDetector::isJSONFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }
    
    std::string firstLine = utils::StringUtils::trimLeft(lines[0]);
    std::string lastLine;
    
    for (const auto& line : lines) {
        if (!utils::StringUtils::isBlank(line)) {
            lastLine = utils::StringUtils::trimRight(line);
        }
    }
    
    if (utils::StringUtils::startsWith(firstLine, "[") || 
        utils::StringUtils::startsWith(firstLine, "{")) {
        return true;
    }
    
    size_t braceCount = 0;
    size_t bracketCount = 0;
    for (const auto& line : lines) {
        braceCount += utils::StringUtils::countOccurrences(line, "{");
        braceCount -= utils::StringUtils::countOccurrences(line, "}");
        bracketCount += utils::StringUtils::countOccurrences(line, "[");
        bracketCount -= utils::StringUtils::countOccurrences(line, "]");
    }
    
    if ((braceCount == 0 && utils::StringUtils::countOccurrences(lines[0], "{") > 0) ||
        (bracketCount == 0 && utils::StringUtils::countOccurrences(lines[0], "[") > 0)) {
        return true;
    }
    
    return false;
}

bool FormatDetector::isXMLFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }
    
    for (const auto& line : lines) {
        std::string trimmed = utils::StringUtils::trim(line);
        
        if (utils::StringUtils::startsWith(trimmed, "<?xml")) {
            return true;
        }
        
        if (utils::StringUtils::startsWith(trimmed, "<!DOCTYPE")) {
            return true;
        }
        
        if (utils::StringUtils::startsWith(trimmed, "<") && 
            utils::StringUtils::endsWith(trimmed, ">") &&
            trimmed.length() > 2) {
            return true;
        }
    }
    
    return false;
}

bool FormatDetector::isLogFormat(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return false;
    }
    
    static const std::vector<std::regex> logPatterns = {
        std::regex(R"(\d{4}[-/]\d{1,2}[-/]\d{1,2}\s+\d{1,2}:\d{1,2}:\d{1,2})"),
        std::regex(R"(\[(INFO|DEBUG|WARN|ERROR|FATAL|TRACE)\])", std::regex::icase),
        std::regex(R"(\b(INFO|DEBUG|WARN|ERROR|FATAL|TRACE)\b)", std::regex::icase),
    };
    
    size_t matchingLines = 0;
    for (const auto& line : lines) {
        for (const auto& pattern : logPatterns) {
            if (std::regex_search(line, pattern)) {
                matchingLines++;
                break;
            }
        }
    }
    
    float ratio = static_cast<float>(matchingLines) / lines.size();
    return ratio > 0.3f;
}

}
}
