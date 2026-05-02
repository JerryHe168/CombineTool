#include "combinetool/merge/sequential_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <fstream>
#include <iostream>

namespace combinetool {
namespace merge {

SequentialMerger::SequentialMerger(const MergeConfig& config)
    : Merger(config)
{
}

SequentialMerger::~SequentialMerger() = default;

bool SequentialMerger::merge() {
    if (m_config.inputFiles.empty()) {
        return false;
    }
    
    m_outputStream = utils::FileUtils::openForWrite(m_config.outputFile);
    if (!m_outputStream) {
        return false;
    }
    
    m_totalLinesProcessed = 0;
    m_totalLinesWritten = 0;
    m_filesProcessed = 0;
    
    std::vector<FileHeaderInfo> headerInfos;
    if (!collectAllHeaders(headerInfos)) {
        return false;
    }
    
    if (!verifyHeaderConsistency(headerInfos)) {
        return false;
    }
    
    if (!writeUnifiedHeader(headerInfos)) {
        return false;
    }
    
    size_t totalFiles = m_config.inputFiles.size();
    
    for (size_t i = 0; i < m_config.inputFiles.size(); ++i) {
        const auto& filePath = m_config.inputFiles[i];
        
        updateProgress(m_filesProcessed, totalFiles, filePath);
        
        if (!processFile(filePath)) {
            return false;
        }
        
        m_filesProcessed++;
    }
    
    m_outputStream->close();
    
    updateProgress(m_filesProcessed, totalFiles, "");
    
    return true;
}

bool SequentialMerger::collectAllHeaders(std::vector<FileHeaderInfo>& headerInfos) {
    headerInfos.clear();
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        FileHeaderInfo info;
        info.filePath = filePath;
        info.hasHeader = false;
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        info.hasHeader = formatResult.hasHeader;
        
        if (info.hasHeader && !formatResult.headerColumns.empty()) {
            info.headerColumns = formatResult.headerColumns;
        }
        
        if (info.hasHeader) {
            auto stream = utils::FileUtils::openForRead(filePath, true);
            if (stream) {
                std::string line;
                if (utils::FileUtils::readLine(*stream, line)) {
                    info.headerLine = line;
                }
            }
        }
        
        headerInfos.push_back(info);
    }
    
    return true;
}

bool SequentialMerger::verifyHeaderConsistency(const std::vector<FileHeaderInfo>& headerInfos) {
    if (headerInfos.empty()) {
        return true;
    }
    
    bool firstHasHeader = headerInfos[0].hasHeader;
    const std::vector<std::string>& firstColumns = headerInfos[0].headerColumns;
    
    for (size_t i = 1; i < headerInfos.size(); ++i) {
        const auto& info = headerInfos[i];
        
        if (info.hasHeader != firstHasHeader) {
            std::cerr << "Warning: File " << info.filePath 
                      << " has inconsistent header presence compared to first file" << std::endl;
            continue;
        }
        
        if (info.hasHeader && firstHasHeader) {
            if (info.headerColumns.size() != firstColumns.size()) {
                std::cerr << "Warning: File " << info.filePath 
                          << " has different column count (" << info.headerColumns.size() 
                          << ") than first file (" << firstColumns.size() << ")" << std::endl;
                continue;
            }
            
            bool columnsMatch = true;
            for (size_t j = 0; j < info.headerColumns.size(); ++j) {
                if (info.headerColumns[j] != firstColumns[j]) {
                    columnsMatch = false;
                    break;
                }
            }
            
            if (!columnsMatch) {
                std::cerr << "Warning: File " << info.filePath 
                          << " has different column names than first file" << std::endl;
            }
        }
    }
    
    return true;
}

bool SequentialMerger::writeUnifiedHeader(const std::vector<FileHeaderInfo>& headerInfos) {
    if (!m_config.outputHeader || headerInfos.empty()) {
        return true;
    }
    
    const auto& firstInfo = headerInfos[0];
    if (!firstInfo.hasHeader || firstInfo.headerLine.empty()) {
        return true;
    }
    
    auto encodingResult = encoding::EncodingDetector::detectFromFile(firstInfo.filePath);
    bool needConversion = encodingResult.encoding != m_config.targetEncoding;
    
    std::string headerToWrite = firstInfo.headerLine;
    
    if (needConversion) {
        auto converted = encoding::EncodingConverter::convert(
            headerToWrite + "\n", 
            encodingResult.encoding, 
            m_config.targetEncoding,
            false
        );
        if (converted.success) {
            headerToWrite = converted.output;
        }
    }
    
    if (!writeLine(headerToWrite)) {
        return false;
    }
    
    return true;
}

bool SequentialMerger::processFile(const std::string& filePath) {
    if (!utils::PathUtils::isFile(filePath)) {
        return false;
    }
    
    auto formatResult = format::FormatDetector::detectFromFile(filePath);
    
    auto encodingResult = encoding::EncodingDetector::detectFromFile(filePath);
    
    bool needConversion = encodingResult.encoding != m_config.targetEncoding;
    
    auto stream = utils::FileUtils::openForRead(filePath, true);
    if (!stream) {
        return false;
    }
    
    size_t lineNumber = 0;
    std::string line;
    
    while (utils::FileUtils::readLine(*stream, line)) {
        lineNumber++;
        
        if (formatResult.hasHeader && lineNumber == 1) {
            continue;
        }
        
        std::string processedLine = line;
        
        if (needConversion) {
            auto converted = encoding::EncodingConverter::convert(
                line, 
                encodingResult.encoding, 
                m_config.targetEncoding,
                false
            );
            if (converted.success) {
                processedLine = converted.output;
            }
        }
        
        LineData lineData;
        lineData.content = processedLine;
        lineData.lineNumber = lineNumber;
        lineData.sourceFile = filePath;
        lineData.isValid = true;
        
        processLine(lineData);
        
        if (!shouldKeep(lineData)) {
            continue;
        }
        
        if (isDuplicate(lineData)) {
            continue;
        }
        
        if (!writeLine(processedLine)) {
            return false;
        }
    }
    
    return true;
}

bool SequentialMerger::writeLine(const std::string& line) {
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
