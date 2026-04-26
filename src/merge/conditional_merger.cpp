#define NOMINMAX

#include "combinetool/merge/conditional_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"
#include "combinetool/io/smart_file_reader.h"

#include <fstream>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>

namespace combinetool {
namespace merge {

ConditionalMerger::ConditionalMerger(const MergeConfig& config)
    : Merger(config)
    , m_joinKeyColumn(0)
    , m_joinType(JoinType::Inner)
{
    if (config.filterConfig.mode != FilterMode::Exclude || 
        !config.filterConfig.includePatterns.empty() ||
        !config.filterConfig.excludePatterns.empty() ||
        config.filterConfig.filterBlankLines ||
        config.filterConfig.filterCommentLines) {
        m_filter = std::make_unique<filter::Filter>(config.filterConfig);
    }
    
    if (config.deduplicationConfig.mode != DeduplicationMode::None) {
        m_deduplicator = std::make_unique<filter::Deduplicator>(config.deduplicationConfig);
    }
}

ConditionalMerger::~ConditionalMerger() = default;

bool ConditionalMerger::merge() {
    if (m_config.inputFiles.size() < 2) {
        return false;
    }
    
    m_outputStream = utils::FileUtils::openForWrite(m_config.outputFile);
    if (!m_outputStream) {
        return false;
    }
    
    m_totalLinesProcessed = 0;
    m_totalLinesWritten = 0;
    m_filesProcessed = 0;
    
    if (!prepareFiles()) {
        return false;
    }
    
    if (!resolveKeyColumns()) {
        return false;
    }
    
    if (!performHashJoin()) {
        return false;
    }
    
    m_outputStream->close();
    m_filesProcessed = m_config.inputFiles.size();
    
    return true;
}

bool ConditionalMerger::shouldUseHashJoin(uint64_t file1Size, uint64_t file2Size) const {
    return true;
}

bool ConditionalMerger::prepareFiles() {
    m_fileInfos.clear();
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        
        FileInfo fileInfo;
        fileInfo.filePath = filePath;
        fileInfo.delimiter = formatResult.detectedDelimiter;
        fileInfo.hasHeader = formatResult.hasHeader;
        fileInfo.keyColumnIndex = 0;
        fileInfo.columnCount = 0;
        
        if (formatResult.hasHeader && !formatResult.headerColumns.empty()) {
            fileInfo.headers = formatResult.headerColumns;
            fileInfo.columnCount = formatResult.headerColumns.size();
        }
        
        m_fileInfos.push_back(fileInfo);
    }
    
    return m_fileInfos.size() >= 2;
}

void ConditionalMerger::setJoinKeyColumn(int columnIndex) {
    m_joinKeyColumn = columnIndex;
    m_joinKeyColumnName.clear();
}

void ConditionalMerger::setJoinKeyColumn(const std::string& columnName) {
    m_joinKeyColumnName = columnName;
    m_joinKeyColumn = -1;
}

void ConditionalMerger::setJoinType(const std::string& joinType) {
    std::string lower = utils::StringUtils::toLower(joinType);
    
    if (lower == "inner" || lower == "inner_join") {
        m_joinType = JoinType::Inner;
    } else if (lower == "left" || lower == "left_join") {
        m_joinType = JoinType::Left;
    } else if (lower == "right" || lower == "right_join") {
        m_joinType = JoinType::Right;
    } else if (lower == "full" || lower == "full_join" || lower == "outer") {
        m_joinType = JoinType::Full;
    }
}

bool ConditionalMerger::resolveKeyColumns() {
    for (auto& fileInfo : m_fileInfos) {
        if (m_joinKeyColumn >= 0) {
            fileInfo.keyColumnIndex = m_joinKeyColumn;
        } else if (!m_joinKeyColumnName.empty()) {
            bool found = false;
            for (size_t i = 0; i < fileInfo.headers.size(); ++i) {
                if (fileInfo.headers[i] == m_joinKeyColumnName) {
                    fileInfo.keyColumnIndex = static_cast<int>(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                fileInfo.keyColumnIndex = 0;
            }
        } else {
            fileInfo.keyColumnIndex = 0;
        }
    }
    
    return true;
}

bool ConditionalMerger::loadRightTableToMemory(
    const std::string& filePath,
    const std::string& delimiter,
    bool hasHeader,
    size_t keyColumnIndex,
    RowList& rows,
    RowIndexMap& index,
    size_t& columnCount
) {
    io::SmartFileReader reader(
        filePath,
        m_config.smartIOConfig.memoryMapThreshold,
        m_config.bufferSize
    );
    
    if (!reader.isOpen()) {
        return false;
    }
    
    io::SmartLineIterator iterator(reader);
    size_t lineNumber = 0;
    columnCount = 0;
    
    std::string line;
    while (iterator.hasNext()) {
        if (!iterator.next(line)) {
            break;
        }
        
        m_totalLinesProcessed++;
        lineNumber++;
        
        if (lineNumber == 1 && hasHeader) {
            auto headerRow = format::FormatDetector::parseCSVLine(line, delimiter);
            columnCount = headerRow.size();
            continue;
        }
        
        if (utils::StringUtils::isBlank(line)) {
            continue;
        }
        
        auto row = format::FormatDetector::parseCSVLine(line, delimiter);
        
        if (columnCount == 0 && !row.empty()) {
            columnCount = row.size();
        }
        
        if (keyColumnIndex < row.size()) {
            const std::string& key = row[keyColumnIndex];
            index[key].push_back(rows.size());
        }
        
        rows.push_back(row);
    }
    
    return !rows.empty();
}

bool ConditionalMerger::streamLeftTableAndJoin(
    const std::string& filePath,
    const std::string& delimiter,
    bool hasHeader,
    size_t leftKeyColumnIndex,
    size_t leftColumnCount,
    const RowList& rightRows,
    const RowIndexMap& rightIndex,
    size_t rightColumnCount,
    const std::vector<std::string>& rightHeaders,
    bool writeHeaders
) {
    io::SmartFileReader reader(
        filePath,
        m_config.smartIOConfig.memoryMapThreshold,
        m_config.bufferSize
    );
    
    if (!reader.isOpen()) {
        return false;
    }
    
    io::SmartLineIterator iterator(reader);
    size_t lineNumber = 0;
    
    std::unordered_map<std::string, bool> matchedKeys;
    std::vector<std::string> leftHeaders;
    bool headerWritten = !writeHeaders;
    
    std::string line;
    while (iterator.hasNext()) {
        if (!iterator.next(line)) {
            break;
        }
        
        m_totalLinesProcessed++;
        lineNumber++;
        
        if (lineNumber == 1 && hasHeader) {
            leftHeaders = format::FormatDetector::parseCSVLine(line, delimiter);
            
            if (writeHeaders && !headerWritten) {
                auto combinedHeaders = combineRows(
                    leftHeaders, rightHeaders,
                    leftHeaders.size(), rightHeaders.size()
                );
                auto headerLine = format::FormatDetector::formatCSVLine(
                    combinedHeaders, m_config.outputDelimiter
                );
                if (!writeLine(headerLine)) {
                    return false;
                }
                headerWritten = true;
            }
            continue;
        }
        
        if (utils::StringUtils::isBlank(line)) {
            continue;
        }
        
        auto leftRow = format::FormatDetector::parseCSVLine(line, delimiter);
        
        std::string key;
        bool hasKey = false;
        
        if (leftKeyColumnIndex < leftRow.size()) {
            key = leftRow[leftKeyColumnIndex];
            hasKey = true;
        }
        
        std::vector<size_t> matchingRightRowIndices;
        bool hasMatch = false;
        
        if (hasKey) {
            auto it = rightIndex.find(key);
            if (it != rightIndex.end()) {
                matchingRightRowIndices = it->second;
                hasMatch = true;
                matchedKeys[key] = true;
            }
        }
        
        bool shouldOutput = false;
        
        switch (m_joinType) {
            case JoinType::Inner:
                shouldOutput = hasMatch;
                break;
            case JoinType::Left:
                shouldOutput = true;
                break;
            case JoinType::Right:
                shouldOutput = false;
                break;
            case JoinType::Full:
                shouldOutput = true;
                break;
        }
        
        if (!shouldOutput) {
            continue;
        }
        
        if (!hasMatch) {
            std::vector<std::string> emptyRow(rightColumnCount);
            auto combined = combineRows(leftRow, emptyRow, leftRow.size(), rightColumnCount);
            
            LineData lineData;
            lineData.content = format::FormatDetector::formatCSVLine(
                combined, m_config.outputDelimiter
            );
            lineData.isValid = true;
            
            if (m_filter && !m_filter->shouldKeep(lineData)) {
                continue;
            }
            
            if (m_deduplicator && m_deduplicator->isDuplicate(lineData)) {
                continue;
            }
            
            if (!writeLine(lineData.content)) {
                return false;
            }
        } else {
            for (size_t rightIdx : matchingRightRowIndices) {
                if (rightIdx >= rightRows.size()) {
                    continue;
                }
                
                const auto& rightRow = rightRows[rightIdx];
                
                auto combined = combineRows(leftRow, rightRow, leftRow.size(), rightRow.size());
                
                LineData lineData;
                lineData.content = format::FormatDetector::formatCSVLine(
                    combined, m_config.outputDelimiter
                );
                lineData.isValid = true;
                
                if (m_filter && !m_filter->shouldKeep(lineData)) {
                    continue;
                }
                
                if (m_deduplicator && m_deduplicator->isDuplicate(lineData)) {
                    continue;
                }
                
                if (!writeLine(lineData.content)) {
                    return false;
                }
            }
        }
    }
    
    if (m_joinType == JoinType::Right || m_joinType == JoinType::Full) {
        for (const auto& entry : rightIndex) {
            const std::string& key = entry.first;
            
            if (matchedKeys.find(key) != matchedKeys.end()) {
                continue;
            }
            
            for (size_t rightIdx : entry.second) {
                if (rightIdx >= rightRows.size()) {
                    continue;
                }
                
                const auto& rightRow = rightRows[rightIdx];
                
                std::vector<std::string> emptyRow(leftColumnCount);
                auto combined = combineRows(emptyRow, rightRow, leftColumnCount, rightRow.size());
                
                LineData lineData;
                lineData.content = format::FormatDetector::formatCSVLine(
                    combined, m_config.outputDelimiter
                );
                lineData.isValid = true;
                
                if (m_filter && !m_filter->shouldKeep(lineData)) {
                    continue;
                }
                
                if (m_deduplicator && m_deduplicator->isDuplicate(lineData)) {
                    continue;
                }
                
                if (!writeLine(lineData.content)) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

bool ConditionalMerger::performHashJoin() {
    if (m_fileInfos.size() < 2) {
        return false;
    }
    
    const auto& leftFile = m_fileInfos[0];
    const auto& rightFile = m_fileInfos[1];
    
    uint64_t leftFileSize = utils::PathUtils::getFileSize(leftFile.filePath);
    uint64_t rightFileSize = utils::PathUtils::getFileSize(rightFile.filePath);
    
    bool swapTables = false;
    if (rightFileSize > leftFileSize && (m_joinType == JoinType::Inner || m_joinType == JoinType::Full)) {
        swapTables = true;
    }
    
    const auto* smallerFile = swapTables ? &leftFile : &rightFile;
    const auto* largerFile = swapTables ? &rightFile : &leftFile;
    
    RowList rightRows;
    RowIndexMap rightIndex;
    size_t rightColumnCount = 0;
    
    if (!loadRightTableToMemory(
        smallerFile->filePath,
        smallerFile->delimiter,
        smallerFile->hasHeader,
        static_cast<size_t>(smallerFile->keyColumnIndex),
        rightRows,
        rightIndex,
        rightColumnCount
    )) {
        return false;
    }
    
    bool writeHeaders = m_config.outputHeader;
    
    if (swapTables) {
        return streamLeftTableAndJoin(
            largerFile->filePath,
            largerFile->delimiter,
            largerFile->hasHeader,
            static_cast<size_t>(largerFile->keyColumnIndex),
            largerFile->columnCount,
            rightRows,
            rightIndex,
            rightColumnCount,
            smallerFile->headers,
            writeHeaders
        );
    } else {
        return streamLeftTableAndJoin(
            largerFile->filePath,
            largerFile->delimiter,
            largerFile->hasHeader,
            static_cast<size_t>(largerFile->keyColumnIndex),
            largerFile->columnCount,
            rightRows,
            rightIndex,
            rightColumnCount,
            smallerFile->headers,
            writeHeaders
        );
    }
}

bool ConditionalMerger::writeEmptyRow(size_t columnCount) {
    std::vector<std::string> emptyRow(columnCount);
    auto line = format::FormatDetector::formatCSVLine(emptyRow, m_config.outputDelimiter);
    return writeLine(line);
}

std::vector<std::string> ConditionalMerger::combineRows(
    const std::vector<std::string>& row1,
    const std::vector<std::string>& row2,
    size_t file1Cols,
    size_t file2Cols
) {
    std::vector<std::string> result;
    result.reserve(file1Cols + file2Cols);
    
    for (size_t i = 0; i < file1Cols; ++i) {
        if (i < row1.size()) {
            result.push_back(row1[i]);
        } else {
            result.emplace_back();
        }
    }
    
    for (size_t i = 0; i < file2Cols; ++i) {
        if (i < row2.size()) {
            result.push_back(row2[i]);
        } else {
            result.emplace_back();
        }
    }
    
    return result;
}

bool ConditionalMerger::writeLine(const std::string& line) {
    if (!m_outputStream) {
        return false;
    }
    
    *m_outputStream << line;
    if (!line.empty() && (line.back() != '\n' && line.back() != '\r')) {
        *m_outputStream << '\n';
    }
    m_totalLinesWritten++;
    
    return m_outputStream->good();
}

}
}
