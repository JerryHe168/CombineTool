#include "combinetool/merge/timestamp_sorted_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <iostream>
#include <algorithm>

namespace combinetool {
namespace merge {

TimestampSortedMerger::TimestampSortedMerger(const MergeConfig& config)
    : Merger(config)
    , m_maxMemoryEntries(1000000)
{
    m_extractor = std::make_unique<utils::TimestampExtractor>(config.timestampConfig);
}

TimestampSortedMerger::~TimestampSortedMerger() = default;

bool TimestampSortedMerger::merge() {
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
    
    if (!initializeReaders()) {
        return false;
    }
    
    bool success = false;
    
    if (shouldUseExternalMerge()) {
        success = mergeExternal();
    } else {
        success = mergeInMemory();
    }
    
    closeAllReaders();
    m_outputStream->close();
    
    return success;
}

void TimestampSortedMerger::setMaxMemoryBuffer(size_t maxEntries) {
    m_maxMemoryEntries = maxEntries;
}

size_t TimestampSortedMerger::getMaxMemoryBuffer() const {
    return m_maxMemoryEntries;
}

bool TimestampSortedMerger::initializeReaders() {
    m_readers.clear();
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        FileReaderState state;
        state.filePath = filePath;
        state.lineNumber = 0;
        state.eof = false;
        state.headerWritten = false;
        state.hasNextEntry = false;
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        // For plain text and log files, don't assume they have headers
        state.hasHeader = false;
        
        state.reader = std::make_unique<io::SmartFileReader>(
            filePath,
            m_config.smartIOConfig.memoryMapThreshold,
            m_config.bufferSize
        );
        
        if (!state.reader->isOpen()) {
            continue;
        }
        
        state.iterator = std::make_unique<io::SmartLineIterator>(*state.reader);
        
        m_readers.push_back(std::move(state));
    }
    
    return !m_readers.empty();
}

bool TimestampSortedMerger::closeAllReaders() {
    for (auto& reader : m_readers) {
        if (reader.reader) {
            reader.reader->close();
        }
    }
    m_readers.clear();
    return true;
}

bool TimestampSortedMerger::readNextEntry(FileReaderState& state) {
    if (state.eof || !state.iterator) {
        state.hasNextEntry = false;
        return false;
    }
    
    std::string line;
    if (!state.iterator->next(line)) {
        state.eof = true;
        state.hasNextEntry = false;
        return false;
    }
    
    state.lineNumber++;
    
    utils::LogLineEntry entry(line, state.filePath, state.lineNumber);
    
    auto result = m_extractor->extract(line);
    if (result.success) {
        entry.setTimestamp(result.timestamp);
    }
    
    state.nextEntry = entry;
    state.hasNextEntry = true;
    
    return true;
}

bool TimestampSortedMerger::fillHeap() {
    while (!m_mergeHeap.empty()) {
        m_mergeHeap.pop();
    }
    
    for (size_t i = 0; i < m_readers.size(); ++i) {
        auto& reader = m_readers[i];
        
        if (!reader.hasNextEntry) {
            readNextEntry(reader);
        }
        
        if (reader.hasNextEntry) {
            MergeHeapEntry heapEntry;
            heapEntry.entry = reader.nextEntry;
            heapEntry.fileIndex = i;
            heapEntry.lineIndex = reader.lineNumber;
            
            m_mergeHeap.push(heapEntry);
            reader.hasNextEntry = false;
        }
    }
    
    return !m_mergeHeap.empty();
}

bool TimestampSortedMerger::mergeInMemory() {
    utils::LogBuffer buffer(m_maxMemoryEntries);
    
    for (size_t fileIdx = 0; fileIdx < m_readers.size(); ++fileIdx) {
        auto& reader = m_readers[fileIdx];
        
        updateProgress(m_filesProcessed, m_config.inputFiles.size(), reader.filePath);
        
        bool isFirstFile = (fileIdx == 0);
        bool headerProcessed = false;
        
        while (readNextEntry(reader)) {
            utils::LogLineEntry entry = reader.nextEntry;
            m_totalLinesProcessed++;
            
            if (reader.hasHeader && !headerProcessed) {
                headerProcessed = true;
                
                if (isFirstFile && m_config.outputHeader) {
                    if (!writeLine(entry.getLine())) {
                        return false;
                    }
                }
                continue;
            }
            
            buffer.addEntry(entry);
        }
        
        m_filesProcessed++;
    }
    
    buffer.sort();
    
    for (const auto& entry : buffer) {
        if (!processAndWriteEntry(entry)) {
            return false;
        }
    }
    
    return true;
}

bool TimestampSortedMerger::mergeExternal() {
    if (!fillHeap()) {
        return false;
    }
    
    std::vector<bool> headerWritten(m_readers.size(), false);
    
    while (!m_mergeHeap.empty()) {
        MergeHeapEntry heapEntry = m_mergeHeap.top();
        m_mergeHeap.pop();
        
        auto& reader = m_readers[heapEntry.fileIndex];
        
        if (reader.hasHeader && !headerWritten[heapEntry.fileIndex]) {
            headerWritten[heapEntry.fileIndex] = true;
            
            if (heapEntry.fileIndex == 0 && m_config.outputHeader) {
                if (!writeLine(heapEntry.entry.getLine())) {
                    return false;
                }
            }
            
            if (readNextEntry(reader)) {
                MergeHeapEntry newHeapEntry;
                newHeapEntry.entry = reader.nextEntry;
                newHeapEntry.fileIndex = heapEntry.fileIndex;
                newHeapEntry.lineIndex = reader.lineNumber;
                m_mergeHeap.push(newHeapEntry);
                reader.hasNextEntry = false;
            }
            
            continue;
        }
        
        m_totalLinesProcessed++;
        
        if (!processAndWriteEntry(heapEntry.entry)) {
            return false;
        }
        
        if (readNextEntry(reader)) {
            MergeHeapEntry newHeapEntry;
            newHeapEntry.entry = reader.nextEntry;
            newHeapEntry.fileIndex = heapEntry.fileIndex;
            newHeapEntry.lineIndex = reader.lineNumber;
            m_mergeHeap.push(newHeapEntry);
            reader.hasNextEntry = false;
        }
    }
    
    return true;
}

bool TimestampSortedMerger::processAndWriteEntry(const utils::LogLineEntry& entry) {
    auto encodingResult = encoding::EncodingDetector::detectFromFile(entry.getSourceFile());
    bool needConversion = encodingResult.encoding != m_config.targetEncoding;
    
    std::string processedLine = entry.getLine();
    
    if (needConversion) {
        auto converted = encoding::EncodingConverter::convert(
            processedLine,
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
    lineData.lineNumber = entry.getLineNumber();
    lineData.sourceFile = entry.getSourceFile();
    lineData.isValid = true;
    
    processLine(lineData);
    
    if (!shouldKeep(lineData)) {
        return true;
    }
    
    if (isDuplicate(lineData)) {
        return true;
    }
    
    return writeLine(processedLine);
}

bool TimestampSortedMerger::writeLine(const std::string& line) {
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

bool TimestampSortedMerger::shouldUseExternalMerge() const {
    uint64_t estimatedLines = estimateTotalLines();
    return estimatedLines > m_maxMemoryEntries;
}

uint64_t TimestampSortedMerger::estimateTotalLines() const {
    uint64_t totalBytes = 0;
    uint64_t totalSampleLines = 0;
    uint64_t totalSampleBytes = 0;
    
    for (const auto& filePath : m_config.inputFiles) {
        uint64_t fileSize = utils::PathUtils::getFileSize(filePath);
        totalBytes += fileSize;
        
        if (fileSize == 0) {
            continue;
        }
        
        auto stream = utils::FileUtils::openForRead(filePath, true);
        if (!stream) {
            continue;
        }
        
        std::string line;
        size_t linesRead = 0;
        size_t bytesFromLines = 0;
        
        auto formatResult = format::FormatDetector::detectFromFile(filePath);
        
        while (linesRead < config::ESTIMATE_LINE_COUNT_SAMPLE_LINES && 
               utils::FileUtils::readLine(*stream, line)) {
            if (formatResult.hasHeader && linesRead == 0) {
                linesRead++;
                continue;
            }
            
            bytesFromLines += line.size() + 1;
            linesRead++;
        }
        
        if (linesRead > 0) {
            totalSampleLines += linesRead;
            totalSampleBytes += bytesFromLines;
        }
    }
    
    if (totalSampleLines > 0 && totalSampleBytes > 0) {
        size_t avgBytesPerLine = static_cast<size_t>(totalSampleBytes / totalSampleLines);
        if (avgBytesPerLine > 0) {
            return totalBytes / avgBytesPerLine;
        }
    }
    
    return totalBytes / config::ESTIMATE_LINE_COUNT_DEFAULT_BYTES_PER_LINE;
}

}
}
