#include "combinetool/merge/interleaved_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <fstream>

namespace combinetool {
namespace merge {

InterleavedMerger::InterleavedMerger(const MergeConfig& config)
    : Merger(config)
    , m_linesPerChunk(1)
{
}

InterleavedMerger::~InterleavedMerger() {
    closeAllFiles();
}

bool InterleavedMerger::merge() {
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
    
    if (!openAllFiles()) {
        return false;
    }
    
    bool headersWritten = false;
    size_t totalFiles = m_config.inputFiles.size();
    
    while (true) {
        bool hasAnyData = false;
        
        for (auto& reader : m_readers) {
            if (reader.eof) {
                continue;
            }
            
            for (size_t chunk = 0; chunk < m_linesPerChunk; ++chunk) {
                if (reader.eof) {
                    break;
                }
                
                std::string line;
                if (!readNextLine(reader, line)) {
                    reader.eof = true;
                    continue;
                }
                
                hasAnyData = true;
                
                if (reader.hasHeader && !reader.headerWritten) {
                    if (!headersWritten && m_config.outputHeader) {
                        if (!writeLine(line)) {
                            return false;
                        }
                        headersWritten = true;
                    }
                    reader.headerWritten = true;
                    continue;
                }
                
                LineData lineData;
                lineData.content = line;
                lineData.lineNumber = reader.lineNumber;
                lineData.sourceFile = reader.filePath;
                lineData.isValid = true;
                
                processLine(lineData);
                
                if (!shouldKeep(lineData)) {
                    continue;
                }
                
                if (isDuplicate(lineData)) {
                    continue;
                }
                
                if (!writeLine(line)) {
                    return false;
                }
            }
        }
        
        if (!hasAnyData) {
            break;
        }
        
        updateProgress(0, totalFiles, "");
    }
    
    closeAllFiles();
    m_outputStream->close();
    
    m_filesProcessed = m_config.inputFiles.size();
    
    return true;
}

void InterleavedMerger::setChunkSize(size_t linesPerChunk) {
    m_linesPerChunk = linesPerChunk > 0 ? linesPerChunk : 1;
}

size_t InterleavedMerger::getChunkSize() const {
    return m_linesPerChunk;
}

bool InterleavedMerger::openAllFiles() {
    m_readers.clear();
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        
        FileReaderState reader;
        reader.filePath = filePath;
        reader.stream = utils::FileUtils::openForRead(filePath, true);
        reader.lineNumber = 0;
        reader.hasHeader = formatResult.hasHeader;
        reader.headerWritten = false;
        reader.eof = false;
        
        if (!reader.stream) {
            continue;
        }
        
        m_readers.push_back(std::move(reader));
    }
    
    return !m_readers.empty();
}

bool InterleavedMerger::closeAllFiles() {
    for (auto& reader : m_readers) {
        if (reader.stream) {
            reader.stream->close();
        }
    }
    m_readers.clear();
    return true;
}

bool InterleavedMerger::readNextLine(FileReaderState& reader, std::string& line) {
    if (!reader.stream || reader.eof) {
        return false;
    }
    
    if (!utils::FileUtils::readLine(*reader.stream, line)) {
        return false;
    }
    
    reader.lineNumber++;
    
    auto encodingResult = encoding::EncodingDetector::detectFromString(line);
    if (encodingResult.encoding != m_config.targetEncoding && 
        encodingResult.encoding != Encoding::Unknown) {
        auto converted = encoding::EncodingConverter::convert(
            line, 
            encodingResult.encoding, 
            m_config.targetEncoding,
            false
        );
        if (converted.success) {
            line = converted.output;
        }
    }
    
    return true;
}

bool InterleavedMerger::writeLine(const std::string& line) {
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
