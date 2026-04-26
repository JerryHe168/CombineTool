#define NOMINMAX

#include "combinetool/io/smart_file_reader.h"
#include "combinetool/io/stream_reader.h"
#include "combinetool/io/mapped_file_reader.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <algorithm>

namespace combinetool {
namespace io {

SmartFileReader::SmartFileReader(
    const std::string& filePath,
    uint64_t memoryMapThreshold,
    size_t bufferSize,
    FileType fileType
)
    : m_filePath(filePath)
    , m_memoryMapThreshold(memoryMapThreshold)
    , m_bufferSize(bufferSize)
    , m_fileType(fileType)
    , m_readerType(ReaderType::Stream)
    , m_fileSize(0)
    , m_mappedOffset(0)
{
    m_fileSize = utils::PathUtils::getFileSize(filePath);
    
    if (m_fileType == FileType::AutoDetect) {
        detectFileType();
    }
    
    selectReaderType();
    initializeReader();
}

SmartFileReader::~SmartFileReader() {
    close();
}

bool SmartFileReader::isOpen() const {
    if (m_readerType == ReaderType::Stream) {
        return m_streamReader && m_streamReader->isOpen();
    } else {
        return m_mappedReader && m_mappedReader->isOpen();
    }
}

void SmartFileReader::close() {
    if (m_streamReader) {
        m_streamReader->close();
        m_streamReader.reset();
    }
    if (m_mappedReader) {
        m_mappedReader->close();
        m_mappedReader.reset();
    }
}

ReaderType SmartFileReader::getReaderType() const {
    return m_readerType;
}

bool SmartFileReader::readLine(std::string& line, bool keepNewline) {
    line.clear();
    
    if (m_readerType == ReaderType::Stream) {
        if (!m_streamReader) {
            return false;
        }
        ReadResult result = m_streamReader->readLine(line, keepNewline);
        return result.success || (result.eof && !line.empty());
    } else {
        if (!m_mappedReader || !m_mappedReader->isMapped()) {
            return false;
        }
        return m_mappedReader->readLine(line, m_mappedOffset, keepNewline);
    }
}

ReadResult SmartFileReader::readChunk(std::vector<char>& buffer, size_t& bytesRead) {
    ReadResult result;
    result.success = false;
    result.bytesRead = 0;
    result.eof = false;
    result.errorMessage = "";
    bytesRead = 0;
    
    if (m_readerType == ReaderType::Stream) {
        if (!m_streamReader) {
            result.errorMessage = "Stream reader not initialized";
            return result;
        }
        return m_streamReader->readChunk(buffer, bytesRead);
    } else {
        if (!m_mappedReader || !m_mappedReader->isMapped()) {
            result.errorMessage = "Mapped reader not initialized";
            return result;
        }
        
        const char* data = m_mappedReader->getData();
        if (!data) {
            result.errorMessage = "Mapped data is null";
            return result;
        }
        
        size_t remaining = static_cast<size_t>(m_fileSize) - m_mappedOffset;
        size_t toRead = std::min(buffer.size(), remaining);
        
        if (toRead > 0) {
            std::memcpy(buffer.data(), data + m_mappedOffset, toRead);
            bytesRead = toRead;
            m_mappedOffset += toRead;
        }
        
        result.bytesRead = bytesRead;
        result.eof = m_mappedOffset >= static_cast<size_t>(m_fileSize);
        result.success = bytesRead > 0 || result.eof;
        
        updateProgress(m_mappedOffset, m_fileSize);
        
        return result;
    }
}

size_t SmartFileReader::getPosition() const {
    if (m_readerType == ReaderType::Stream) {
        return m_streamReader ? m_streamReader->getPosition() : 0;
    } else {
        return m_mappedOffset;
    }
}

bool SmartFileReader::setPosition(size_t position) {
    if (m_readerType == ReaderType::Stream) {
        return m_streamReader ? m_streamReader->setPosition(position) : false;
    } else {
        if (position > static_cast<size_t>(m_fileSize)) {
            return false;
        }
        m_mappedOffset = position;
        return true;
    }
}

uint64_t SmartFileReader::getFileSize() const {
    return m_fileSize;
}

size_t SmartFileReader::getBufferSize() const {
    return m_bufferSize;
}

const std::string& SmartFileReader::getFilePath() const {
    return m_filePath;
}

void SmartFileReader::setEncoding(Encoding encoding) {
    if (m_streamReader) {
        m_streamReader->setEncoding(encoding);
    }
    if (m_mappedReader) {
        m_mappedReader->setEncoding(encoding);
    }
}

Encoding SmartFileReader::getEncoding() const {
    if (m_readerType == ReaderType::Stream && m_streamReader) {
        return m_streamReader->getEncoding();
    } else if (m_mappedReader) {
        return m_mappedReader->getEncoding();
    }
    return Encoding::Unknown;
}

void SmartFileReader::setProgressCallback(
    std::function<void(uint64_t current, uint64_t total)> callback
) {
    m_progressCallback = callback;
}

uint64_t SmartFileReader::getDefaultMemoryMapThreshold() {
    return config::DEFAULT_MEMORY_MAP_THRESHOLD;
}

bool SmartFileReader::detectFileType() {
    if (m_fileSize == 0) {
        m_fileType = FileType::Text;
        return true;
    }
    
    auto formatResult = format::FormatDetector::detectFromFile(m_filePath);
    
    if (formatResult.formatName == "Binary") {
        m_fileType = FileType::Binary;
    } else {
        m_fileType = FileType::Text;
    }
    
    return true;
}

bool SmartFileReader::selectReaderType() {
    if (m_fileSize > m_memoryMapThreshold && m_fileSize <= config::MAX_MEMORY_MAP_SIZE) {
        if (m_fileType == FileType::Text) {
            m_readerType = ReaderType::MemoryMapped;
        } else {
            m_readerType = ReaderType::Stream;
        }
    } else {
        m_readerType = ReaderType::Stream;
    }
    
    return true;
}

void SmartFileReader::initializeReader() {
    if (m_readerType == ReaderType::Stream) {
        m_streamReader = std::make_unique<StreamReader>(m_filePath, m_bufferSize);
        if (m_progressCallback) {
            m_streamReader->setProgressCallback(m_progressCallback);
        }
    } else {
        m_mappedReader = std::make_unique<MappedFileReader>(m_filePath, config::MAX_MEMORY_MAP_SIZE);
        if (m_mappedReader->isOpen()) {
            m_mappedReader->map();
        }
        m_mappedOffset = 0;
        if (m_progressCallback && m_mappedReader) {
            m_mappedReader->setProgressCallback(m_progressCallback);
        }
    }
}

void SmartFileReader::updateProgress(uint64_t current, uint64_t total) {
    if (m_progressCallback) {
        m_progressCallback(current, total);
    }
}

SmartLineIterator::SmartLineIterator(SmartFileReader& reader)
    : m_reader(reader)
    , m_hasNext(false)
    , m_initialized(false)
{
}

bool SmartLineIterator::hasNext() const {
    if (!m_initialized) {
        const_cast<SmartLineIterator*>(this)->prefetch();
    }
    return m_hasNext;
}

bool SmartLineIterator::next(std::string& line) {
    if (!m_initialized) {
        prefetch();
    }
    
    if (!m_hasNext) {
        return false;
    }
    
    line = m_nextLine;
    prefetch();
    
    return true;
}

void SmartLineIterator::reset() {
    m_reader.setPosition(0);
    m_hasNext = false;
    m_initialized = false;
    m_nextLine.clear();
}

void SmartLineIterator::prefetch() {
    m_initialized = true;
    m_hasNext = false;
    m_nextLine.clear();
    
    m_hasNext = m_reader.readLine(m_nextLine);
}

}
}
