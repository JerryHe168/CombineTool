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
    
    size_t totalFiles = m_config.inputFiles.size();
    
    for (size_t i = 0; i < m_config.inputFiles.size(); ++i) {
        const auto& filePath = m_config.inputFiles[i];
        bool isFirstFile = (i == 0);
        
        updateProgress(m_filesProcessed, totalFiles, filePath);
        
        if (!processFile(filePath, isFirstFile)) {
            return false;
        }
        
        m_filesProcessed++;
    }
    
    m_outputStream->close();
    
    updateProgress(m_filesProcessed, totalFiles, "");
    
    return true;
}

bool SequentialMerger::processFile(const std::string& filePath, bool isFirstFile) {
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
    bool headerSkipped = false;
    std::string line;
    
    while (utils::FileUtils::readLine(*stream, line)) {
        lineNumber++;
        
        if (formatResult.hasHeader && lineNumber == 1) {
            if (isFirstFile && m_config.outputHeader) {
                if (needConversion) {
                    auto converted = encoding::EncodingConverter::convert(
                        line + "\n", 
                        encodingResult.encoding, 
                        m_config.targetEncoding,
                        false
                    );
                    if (converted.success) {
                        line = converted.output;
                    }
                }
                if (!writeLine(line)) {
                    return false;
                }
            }
            headerSkipped = true;
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
