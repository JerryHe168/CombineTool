#include "combinetool/io/stream_reader.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_detector.h"

#include <sstream>
#include <algorithm>

namespace combinetool {
namespace io {

StreamReader::StreamReader(
    const std::string& filePath,
    size_t bufferSize
)
    : m_filePath(filePath)
    , m_bufferSize(bufferSize > 0 ? bufferSize : config::DEFAULT_BUFFER_SIZE)
    , m_fileSize(0)
    , m_position(0)
    , m_encoding(Encoding::Unknown)
{
    m_buffer.resize(m_bufferSize);
    m_stream = utils::FileUtils::openForRead(filePath, true);
    
    if (m_stream) {
        detectFileSize();
        
        auto encodingResult = encoding::EncodingDetector::detectFromFile(filePath);
        m_encoding = encodingResult.encoding;
    }
}

StreamReader::~StreamReader() {
    close();
}

bool StreamReader::isOpen() const {
    return m_stream && m_stream->is_open();
}

void StreamReader::close() {
    if (m_stream) {
        if (m_stream->is_open()) {
            m_stream->close();
        }
        m_stream.reset();
    }
}

ReadResult StreamReader::readLine(std::string& line, bool keepNewline) {
    ReadResult result;
    result.success = false;
    result.bytesRead = 0;
    result.eof = false;
    result.errorMessage = "";
    
    if (!isOpen()) {
        result.errorMessage = "File not open";
        return result;
    }
    
    line.clear();
    std::vector<char> tempBuffer(4096);
    bool foundNewline = false;
    
    while (!foundNewline && !m_stream->eof()) {
        m_stream->getline(tempBuffer.data(), static_cast<std::streamsize>(tempBuffer.size()));
        
        auto count = static_cast<size_t>(m_stream->gcount());
        
        if (count > 0) {
            if (tempBuffer[count - 1] == '\0') {
                --count;
            }
            line.append(tempBuffer.data(), count);
        }
        
        if (m_stream->eof()) {
            result.eof = true;
            result.success = !line.empty() || m_position > 0;
            result.bytesRead = line.size();
            m_position += result.bytesRead;
            updateProgress();
            return result;
        }
        
        if (m_stream->fail() && !m_stream->eof()) {
            m_stream->clear();
        } else {
            foundNewline = true;
            if (keepNewline) {
                line += '\n';
            }
        }
    }
    
    result.success = !line.empty() || !m_stream->eof();
    result.bytesRead = line.size() + (foundNewline ? 1 : 0);
    result.eof = m_stream->eof();
    m_position += result.bytesRead;
    
    updateProgress();
    
    return result;
}

ReadResult StreamReader::readChunk(std::vector<char>& buffer, size_t& bytesRead) {
    ReadResult result;
    result.success = false;
    result.bytesRead = 0;
    result.eof = false;
    result.errorMessage = "";
    bytesRead = 0;
    
    if (!isOpen()) {
        result.errorMessage = "File not open";
        return result;
    }
    
    if (buffer.empty()) {
        result.errorMessage = "Buffer is empty";
        return result;
    }
    
    m_stream->read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    
    bytesRead = static_cast<size_t>(m_stream->gcount());
    result.bytesRead = bytesRead;
    result.eof = m_stream->eof();
    result.success = bytesRead > 0 || m_stream->eof();
    m_position += bytesRead;
    
    updateProgress();
    
    return result;
}

size_t StreamReader::getPosition() const {
    if (!m_stream) {
        return m_position;
    }
    return static_cast<size_t>(m_stream->tellg());
}

bool StreamReader::setPosition(size_t position) {
    if (!m_stream) {
        return false;
    }
    
    m_stream->seekg(static_cast<std::streamoff>(position));
    m_position = position;
    
    if (m_stream->fail() && !m_stream->eof()) {
        return false;
    }
    
    return true;
}

uint64_t StreamReader::getFileSize() const {
    return m_fileSize;
}

size_t StreamReader::getBufferSize() const {
    return m_bufferSize;
}

const std::string& StreamReader::getFilePath() const {
    return m_filePath;
}

void StreamReader::setEncoding(Encoding encoding) {
    m_encoding = encoding;
}

Encoding StreamReader::getEncoding() const {
    return m_encoding;
}

void StreamReader::setProgressCallback(
    std::function<void(uint64_t, uint64_t)> callback
) {
    m_progressCallback = callback;
}

bool StreamReader::detectFileSize() {
    m_fileSize = utils::PathUtils::getFileSize(m_filePath);
    return m_fileSize > 0;
}

void StreamReader::updateProgress() {
    if (m_progressCallback && m_fileSize > 0) {
        m_progressCallback(m_position, m_fileSize);
    }
}

LineIterator::LineIterator(StreamReader& reader)
    : m_reader(reader)
    , m_hasNext(false)
    , m_initialized(false)
{
}

bool LineIterator::hasNext() const {
    if (!m_initialized) {
        const_cast<LineIterator*>(this)->prefetch();
    }
    return m_hasNext;
}

bool LineIterator::next(std::string& line) {
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

void LineIterator::reset() {
    m_reader.setPosition(0);
    m_hasNext = false;
    m_initialized = false;
    m_nextLine.clear();
}

void LineIterator::prefetch() {
    m_initialized = true;
    m_hasNext = false;
    m_nextLine.clear();
    
    auto result = m_reader.readLine(m_nextLine);
    m_hasNext = result.success || !result.eof;
}

}
}
