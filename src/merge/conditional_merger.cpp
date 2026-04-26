#include "combinetool/merge/conditional_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <fstream>
#include <map>
#include <set>

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
    
    if (!loadAllFiles()) {
        return false;
    }
    
    if (!resolveKeyColumns()) {
        return false;
    }
    
    if (!performJoin()) {
        return false;
    }
    
    m_outputStream->close();
    m_filesProcessed = m_config.inputFiles.size();
    
    return true;
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

bool ConditionalMerger::loadAllFiles() {
    m_fileData.clear();
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        
        auto lines = utils::FileUtils::readAllLines(filePath);
        if (lines.empty()) {
            continue;
        }
        
        FileData fileData;
        fileData.filePath = filePath;
        fileData.keyColumnIndex = 0;
        
        size_t startLine = 0;
        if (formatResult.hasHeader && !lines.empty()) {
            fileData.headers = format::FormatDetector::parseCSVLine(
                lines[0], 
                formatResult.detectedDelimiter
            );
            startLine = 1;
        }
        
        for (size_t i = startLine; i < lines.size(); ++i) {
            if (utils::StringUtils::isBlank(lines[i])) {
                continue;
            }
            
            auto row = format::FormatDetector::parseCSVLine(
                lines[i], 
                formatResult.detectedDelimiter
            );
            fileData.rows.push_back(row);
        }
        
        m_fileData.push_back(fileData);
    }
    
    return m_fileData.size() >= 2;
}

bool ConditionalMerger::resolveKeyColumns() {
    for (auto& fileData : m_fileData) {
        if (m_joinKeyColumn >= 0) {
            fileData.keyColumnIndex = m_joinKeyColumn;
        } else if (!m_joinKeyColumnName.empty()) {
            bool found = false;
            for (size_t i = 0; i < fileData.headers.size(); ++i) {
                if (fileData.headers[i] == m_joinKeyColumnName) {
                    fileData.keyColumnIndex = static_cast<int>(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                fileData.keyColumnIndex = 0;
            }
        } else {
            fileData.keyColumnIndex = 0;
        }
    }
    
    return true;
}

bool ConditionalMerger::performJoin() {
    if (m_fileData.size() < 2) {
        return false;
    }
    
    const auto& file1 = m_fileData[0];
    const auto& file2 = m_fileData[1];
    
    if (m_config.outputHeader) {
        auto combinedHeaders = combineRows(file1.headers, file2.headers, 
            file1.headers.empty() ? 0 : file1.headers.size(),
            file2.headers.empty() ? 0 : file2.headers.size());
        
        auto line = format::FormatDetector::formatCSVLine(
            combinedHeaders, 
            m_config.outputDelimiter
        );
        if (!writeLine(line)) {
            return false;
        }
    }
    
    std::map<std::string, std::vector<const std::vector<std::string>*>> map1;
    std::map<std::string, std::vector<const std::vector<std::string>*>> map2;
    std::set<std::string> allKeys;
    
    for (const auto& row : file1.rows) {
        if (static_cast<size_t>(file1.keyColumnIndex) < row.size()) {
            const std::string& key = row[file1.keyColumnIndex];
            map1[key].push_back(&row);
            allKeys.insert(key);
        }
    }
    
    for (const auto& row : file2.rows) {
        if (static_cast<size_t>(file2.keyColumnIndex) < row.size()) {
            const std::string& key = row[file2.keyColumnIndex];
            map2[key].push_back(&row);
            allKeys.insert(key);
        }
    }
    
    for (const auto& key : allKeys) {
        auto it1 = map1.find(key);
        auto it2 = map2.find(key);
        
        bool has1 = it1 != map1.end();
        bool has2 = it2 != map2.end();
        
        bool shouldOutput = false;
        
        switch (m_joinType) {
            case JoinType::Inner:
                shouldOutput = has1 && has2;
                break;
            case JoinType::Left:
                shouldOutput = has1;
                break;
            case JoinType::Right:
                shouldOutput = has2;
                break;
            case JoinType::Full:
                shouldOutput = true;
                break;
        }
        
        if (!shouldOutput) {
            continue;
        }
        
        const auto& rows1 = has1 ? it1->second : std::vector<const std::vector<std::string>*>();
        const auto& rows2 = has2 ? it2->second : std::vector<const std::vector<std::string>*>();
        
        if (rows1.empty()) {
            for (const auto* row2 : rows2) {
                std::vector<std::string> emptyRow(file1.rows.empty() ? 0 : file1.rows[0].size());
                auto combined = combineRows(emptyRow, *row2, 
                    emptyRow.size(), row2->size());
                auto line = format::FormatDetector::formatCSVLine(
                    combined, m_config.outputDelimiter
                );
                if (!writeLine(line)) {
                    return false;
                }
            }
        } else if (rows2.empty()) {
            for (const auto* row1 : rows1) {
                std::vector<std::string> emptyRow(file2.rows.empty() ? 0 : file2.rows[0].size());
                auto combined = combineRows(*row1, emptyRow, 
                    row1->size(), emptyRow.size());
                auto line = format::FormatDetector::formatCSVLine(
                    combined, m_config.outputDelimiter
                );
                if (!writeLine(line)) {
                    return false;
                }
            }
        } else {
            for (const auto* row1 : rows1) {
                for (const auto* row2 : rows2) {
                    auto combined = combineRows(*row1, *row2, 
                        row1->size(), row2->size());
                    
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
    }
    
    return true;
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
    
    *m_outputStream << line << '\n';
    m_totalLinesWritten++;
    
    return m_outputStream->good();
}

}
}
