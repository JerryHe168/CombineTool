#define NOMINMAX

#include "combinetool/merge/binary_merger.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <iostream>
#include <chrono>
#include <algorithm>

namespace combinetool {
namespace merge {

BinaryMerger::BinaryMerger(const MergeConfig& config)
    : Merger(config)
    , m_chunkSize(config.binaryConfig.chunkSize > 0 ? config.binaryConfig.chunkSize : config::DEFAULT_BINARY_CHUNK_SIZE)
    , m_strategy(BinaryMergeStrategy::ChunkedCopy)
{
    std::memset(&m_stats, 0, sizeof(m_stats));
    m_buffer.resize(m_chunkSize);
}

BinaryMerger::~BinaryMerger() = default;

bool BinaryMerger::merge() {
    if (m_config.inputFiles.empty()) {
        return false;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!initialize()) {
        return false;
    }
    
    uint64_t totalSize = calculateTotalSize();
    m_strategy = selectOptimalStrategy(totalSize, m_config.inputFiles.size());
    
    bool success = false;
    
    switch (m_strategy) {
        case BinaryMergeStrategy::SequentialAppend:
            success = mergeSequential();
            break;
        case BinaryMergeStrategy::MemoryMapped:
            success = mergeMemoryMapped();
            break;
        case BinaryMergeStrategy::ChunkedCopy:
        default:
            success = mergeChunked();
            break;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();
    
    cleanup();
    
    return success;
}

void BinaryMerger::setChunkSize(size_t chunkSize) {
    m_chunkSize = chunkSize;
    if (m_buffer.size() != m_chunkSize) {
        m_buffer.resize(m_chunkSize);
    }
}

size_t BinaryMerger::getChunkSize() const {
    return m_chunkSize;
}

void BinaryMerger::setStrategy(BinaryMergeStrategy strategy) {
    m_strategy = strategy;
}

BinaryMergeStrategy BinaryMerger::getStrategy() const {
    return m_strategy;
}

const BinaryMergeStats& BinaryMerger::getStats() const {
    return m_stats;
}

bool BinaryMerger::initialize() {
    std::memset(&m_stats, 0, sizeof(m_stats));
    m_readers.clear();
    
    m_outputStream = utils::FileUtils::openForWrite(m_config.outputFile, true, false);
    if (!m_outputStream) {
        return false;
    }
    
    for (const auto& filePath : m_config.inputFiles) {
        if (!utils::PathUtils::isFile(filePath)) {
            continue;
        }
        
        FileReaderState state;
        state.filePath = filePath;
        state.fileSize = utils::PathUtils::getFileSize(filePath);
        state.bytesRead = 0;
        state.eof = false;
        state.readerType = io::ReaderType::Stream;
        
        FileType detectedType = FileType::AutoDetect;
        detectFileType(filePath, detectedType);
        
        if (m_config.smartIOConfig.useSmartIO && 
            state.fileSize > m_config.smartIOConfig.memoryMapThreshold &&
            state.fileSize <= config::MAX_MEMORY_MAP_SIZE) {
            state.mappedReader = std::make_unique<io::MappedFileReader>(
                filePath,
                config::MAX_MEMORY_MAP_SIZE
            );
            
            if (state.mappedReader && state.mappedReader->isOpen()) {
                state.mappedReader->map();
                if (state.mappedReader->isMapped()) {
                    state.readerType = io::ReaderType::MemoryMapped;
                }
            }
        }
        
        if (state.readerType == io::ReaderType::Stream) {
            state.smartReader = std::make_unique<io::SmartFileReader>(
                filePath,
                m_config.smartIOConfig.memoryMapThreshold,
                m_config.bufferSize,
                FileType::Binary
            );
        }
        
        m_readers.push_back(std::move(state));
    }
    
    return !m_readers.empty();
}

bool BinaryMerger::cleanup() {
    for (auto& reader : m_readers) {
        if (reader.smartReader) {
            reader.smartReader->close();
            reader.smartReader.reset();
        }
        if (reader.mappedReader) {
            reader.mappedReader->close();
            reader.mappedReader.reset();
        }
    }
    m_readers.clear();
    
    if (m_outputStream) {
        if (m_outputStream->is_open()) {
            m_outputStream->close();
        }
        m_outputStream.reset();
    }
    
    return true;
}

bool BinaryMerger::detectFileType(const std::string& filePath, FileType& detectedType) {
    detectedType = FileType::Binary;
    
    auto formatResult = format::FormatDetector::detectFromFile(filePath);
    
    if (formatResult.format == TextFormat::Unknown) {
        uint64_t fileSize = utils::PathUtils::getFileSize(filePath);
        if (fileSize > 0) {
            auto bytes = utils::FileUtils::readAllBytes(filePath);
            if (!bytes.empty()) {
                size_t nullCount = 0;
                size_t controlCount = 0;
                size_t totalCheck = std::min(bytes.size(), static_cast<size_t>(4096));
                
                for (size_t i = 0; i < totalCheck; ++i) {
                    uint8_t b = bytes[i];
                    if (b == 0) {
                        nullCount++;
                    } else if (b < 0x20 && b != '\t' && b != '\n' && b != '\r') {
                        controlCount++;
                    }
                }
                
                if (nullCount == 0 && controlCount < totalCheck / 10) {
                    detectedType = FileType::Text;
                }
            }
        }
    } else {
        detectedType = FileType::Text;
    }
    
    return true;
}

BinaryMergeStrategy BinaryMerger::selectOptimalStrategy(uint64_t totalSize, size_t fileCount) {
    if (m_config.smartIOConfig.useSmartIO) {
        uint64_t threshold = m_config.smartIOConfig.memoryMapThreshold;
        
        if (totalSize <= threshold && totalSize <= config::MAX_MEMORY_MAP_SIZE) {
            bool allCanMap = true;
            for (const auto& state : m_readers) {
                if (state.fileSize > config::MAX_MEMORY_MAP_SIZE) {
                    allCanMap = false;
                    break;
                }
            }
            
            if (allCanMap) {
                return BinaryMergeStrategy::MemoryMapped;
            }
        }
    }
    
    if (fileCount == 1) {
        return BinaryMergeStrategy::SequentialAppend;
    }
    
    return BinaryMergeStrategy::ChunkedCopy;
}

bool BinaryMerger::mergeSequential() {
    for (size_t i = 0; i < m_readers.size(); ++i) {
        auto& state = m_readers[i];
        
        updateProgress(m_filesProcessed, m_config.inputFiles.size(), state.filePath);
        
        bool success = false;
        
        if (state.readerType == io::ReaderType::MemoryMapped && state.mappedReader) {
            success = copyFileMapped(state);
        } else if (state.smartReader) {
            success = copyFileStream(state);
        }
        
        if (!success) {
            return false;
        }
        
        m_filesProcessed++;
    }
    
    return true;
}

bool BinaryMerger::mergeChunked() {
    for (size_t i = 0; i < m_readers.size(); ++i) {
        auto& state = m_readers[i];
        
        updateProgress(m_filesProcessed, m_config.inputFiles.size(), state.filePath);
        
        bool success = copyFileChunked(state);
        
        if (!success) {
            return false;
        }
        
        m_filesProcessed++;
    }
    
    return true;
}

bool BinaryMerger::mergeMemoryMapped() {
    for (size_t i = 0; i < m_readers.size(); ++i) {
        auto& state = m_readers[i];
        
        updateProgress(m_filesProcessed, m_config.inputFiles.size(), state.filePath);
        
        bool success = false;
        
        if (state.mappedReader && state.mappedReader->isMapped()) {
            success = copyFileMapped(state);
        } else if (state.smartReader) {
            success = copyFileChunked(state);
        }
        
        if (!success) {
            return false;
        }
        
        m_filesProcessed++;
    }
    
    return true;
}

bool BinaryMerger::copyFileStream(FileReaderState& state) {
    if (!state.smartReader || !state.smartReader->isOpen()) {
        return false;
    }
    
    std::vector<char> buffer(m_chunkSize);
    size_t bytesRead = 0;
    
    while (true) {
        io::ReadResult result = state.smartReader->readChunk(buffer, bytesRead);
        
        if (!result.success && result.eof && bytesRead == 0) {
            break;
        }
        
        if (bytesRead > 0) {
            if (!writeBytes(buffer.data(), bytesRead)) {
                return false;
            }
            state.bytesRead += bytesRead;
            updateStats(bytesRead, bytesRead);
        }
        
        if (result.eof) {
            break;
        }
    }
    
    return true;
}

bool BinaryMerger::copyFileMapped(FileReaderState& state) {
    if (!state.mappedReader || !state.mappedReader->isMapped()) {
        return false;
    }
    
    const char* data = state.mappedReader->getData();
    size_t fileSize = static_cast<size_t>(state.mappedReader->getFileSize());
    
    if (!data || fileSize == 0) {
        return true;
    }
    
    size_t bytesWritten = 0;
    size_t remaining = fileSize;
    const char* ptr = data;
    
    while (remaining > 0) {
        size_t toWrite = std::min(m_chunkSize, remaining);
        
        if (!writeBytes(ptr, toWrite)) {
            return false;
        }
        
        bytesWritten += toWrite;
        remaining -= toWrite;
        ptr += toWrite;
        
        state.bytesRead += toWrite;
        updateStats(toWrite, toWrite);
    }
    
    return true;
}

bool BinaryMerger::copyFileChunked(FileReaderState& state) {
    std::unique_ptr<std::ifstream> stream = utils::FileUtils::openForRead(state.filePath, true);
    if (!stream || !stream->is_open()) {
        return false;
    }
    
    std::vector<char> buffer(m_chunkSize);
    
    while (!stream->eof()) {
        stream->read(buffer.data(), static_cast<std::streamsize>(m_chunkSize));
        size_t bytesRead = static_cast<size_t>(stream->gcount());
        
        if (bytesRead > 0) {
            if (!writeBytes(buffer.data(), bytesRead)) {
                return false;
            }
            state.bytesRead += bytesRead;
            m_stats.chunksProcessed++;
            updateStats(bytesRead, bytesRead);
        }
    }
    
    stream->close();
    return true;
}

bool BinaryMerger::writeBytes(const char* data, size_t size) {
    if (!m_outputStream || !data || size == 0) {
        return false;
    }
    
    m_outputStream->write(data, static_cast<std::streamsize>(size));
    
    if (!m_outputStream->good()) {
        return false;
    }
    
    return true;
}

bool BinaryMerger::writeChunk(const std::vector<char>& buffer, size_t bytesToWrite) {
    if (bytesToWrite == 0 || bytesToWrite > buffer.size()) {
        return true;
    }
    
    return writeBytes(buffer.data(), bytesToWrite);
}

uint64_t BinaryMerger::calculateTotalSize() const {
    uint64_t total = 0;
    for (const auto& state : m_readers) {
        total += state.fileSize;
    }
    return total;
}

void BinaryMerger::updateStats(uint64_t bytesRead, uint64_t bytesWritten) {
    m_stats.totalBytesRead += bytesRead;
    m_stats.totalBytesWritten += bytesWritten;
}

}
}
