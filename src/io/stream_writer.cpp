#include "combinetool/io/stream_writer.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/encoding/encoding_detector.h"

#include <algorithm>

namespace combinetool {
namespace io {

StreamWriter::StreamWriter(
    const std::string& filePath,
    bool append,
    size_t bufferSize
)
    : m_filePath(filePath)
    , m_bufferSize(bufferSize > 0 ? bufferSize : config::DEFAULT_BUFFER_SIZE)
    , m_bytesWritten(0)
    , m_encoding(Encoding::UTF8)
    , m_bomWritten(false)
{
    std::string dir = utils::PathUtils::getDirectoryName(filePath);
    if (!dir.empty() && !utils::PathUtils::exists(dir)) {
        utils::PathUtils::createDirectories(dir);
    }
    
    m_stream = utils::FileUtils::openForWrite(filePath, true, append);
    
    if (m_stream) {
        m_buffer.resize(m_bufferSize);
    }
}

StreamWriter::~StreamWriter() {
    close();
}

bool StreamWriter::isOpen() const {
    return m_stream && m_stream->is_open();
}

void StreamWriter::close() {
    if (m_stream) {
        flush();
        if (m_stream->is_open()) {
            m_stream->close();
        }
        m_stream.reset();
    }
}

bool StreamWriter::flush() {
    if (!m_stream) {
        return false;
    }
    
    m_stream->flush();
    return m_stream->good();
}

WriteResult StreamWriter::writeLine(const std::string& line, const std::string& newline) {
    WriteResult result;
    result.success = false;
    result.bytesWritten = 0;
    result.errorMessage = "";
    
    if (!isOpen()) {
        result.errorMessage = "File not open";
        return result;
    }
    
    if (!m_bomWritten && 
        (m_encoding == Encoding::UTF8_BOM || 
         m_encoding == Encoding::UTF16_LE ||
         m_encoding == Encoding::UTF16_BE)) {
        if (!writeBOM()) {
            result.errorMessage = "Failed to write BOM";
            return result;
        }
    }
    
    *m_stream << line << newline;
    
    if (!m_stream->good()) {
        result.errorMessage = "Write failed";
        return result;
    }
    
    result.bytesWritten = line.size() + newline.size();
    result.success = true;
    m_bytesWritten += result.bytesWritten;
    
    updateProgress();
    
    return result;
}

WriteResult StreamWriter::writeText(const std::string& text) {
    WriteResult result;
    result.success = false;
    result.bytesWritten = 0;
    result.errorMessage = "";
    
    if (!isOpen()) {
        result.errorMessage = "File not open";
        return result;
    }
    
    if (!m_bomWritten && 
        (m_encoding == Encoding::UTF8_BOM || 
         m_encoding == Encoding::UTF16_LE ||
         m_encoding == Encoding::UTF16_BE)) {
        if (!writeBOM()) {
            result.errorMessage = "Failed to write BOM";
            return result;
        }
    }
    
    *m_stream << text;
    
    if (!m_stream->good()) {
        result.errorMessage = "Write failed";
        return result;
    }
    
    result.bytesWritten = text.size();
    result.success = true;
    m_bytesWritten += result.bytesWritten;
    
    updateProgress();
    
    return result;
}

WriteResult StreamWriter::writeBuffer(const std::vector<char>& buffer) {
    return writeBuffer(buffer.data(), buffer.size());
}

WriteResult StreamWriter::writeBuffer(const char* data, size_t length) {
    WriteResult result;
    result.success = false;
    result.bytesWritten = 0;
    result.errorMessage = "";
    
    if (!isOpen()) {
        result.errorMessage = "File not open";
        return result;
    }
    
    if (data == nullptr || length == 0) {
        result.success = true;
        return result;
    }
    
    if (!m_bomWritten && 
        (m_encoding == Encoding::UTF8_BOM || 
         m_encoding == Encoding::UTF16_LE ||
         m_encoding == Encoding::UTF16_BE)) {
        if (!writeBOM()) {
            result.errorMessage = "Failed to write BOM";
            return result;
        }
    }
    
    m_stream->write(data, static_cast<std::streamsize>(length));
    
    if (!m_stream->good()) {
        result.errorMessage = "Write failed";
        return result;
    }
    
    result.bytesWritten = length;
    result.success = true;
    m_bytesWritten += result.bytesWritten;
    
    updateProgress();
    
    return result;
}

size_t StreamWriter::getPosition() const {
    if (!m_stream) {
        return static_cast<size_t>(m_bytesWritten);
    }
    return static_cast<size_t>(m_stream->tellp());
}

uint64_t StreamWriter::getBytesWritten() const {
    return m_bytesWritten;
}

const std::string& StreamWriter::getFilePath() const {
    return m_filePath;
}

void StreamWriter::setEncoding(Encoding encoding) {
    m_encoding = encoding;
}

Encoding StreamWriter::getEncoding() const {
    return m_encoding;
}

bool StreamWriter::writeBOM() {
    if (m_bomWritten) {
        return true;
    }
    
    auto bom = encoding::EncodingDetector::getBOMForEncoding(m_encoding);
    if (bom.empty()) {
        m_bomWritten = true;
        return true;
    }
    
    m_stream->write(reinterpret_cast<const char*>(bom.data()), static_cast<std::streamsize>(bom.size()));
    
    if (!m_stream->good()) {
        return false;
    }
    
    m_bytesWritten += bom.size();
    m_bomWritten = true;
    
    return true;
}

void StreamWriter::setProgressCallback(
    std::function<void(uint64_t, uint64_t)> callback
) {
    m_progressCallback = callback;
}

void StreamWriter::updateProgress() {
    if (m_progressCallback) {
        m_progressCallback(m_bytesWritten, 0);
    }
}

BufferedWriter::BufferedWriter(
    const std::string& filePath,
    size_t bufferSize,
    bool append
)
    : m_filePath(filePath)
    , m_bufferSize(bufferSize > 0 ? bufferSize : config::LARGE_BUFFER_SIZE)
    , m_bufferUsed(0)
    , m_bytesWritten(0)
{
    m_buffer.resize(m_bufferSize);
    
    std::string dir = utils::PathUtils::getDirectoryName(filePath);
    if (!dir.empty() && !utils::PathUtils::exists(dir)) {
        utils::PathUtils::createDirectories(dir);
    }
    
    m_stream = utils::FileUtils::openForWrite(filePath, true, append);
}

BufferedWriter::~BufferedWriter() {
    close();
}

bool BufferedWriter::isOpen() const {
    return m_stream && m_stream->is_open();
}

void BufferedWriter::close() {
    if (m_stream) {
        flushBuffer();
        if (m_stream->is_open()) {
            m_stream->close();
        }
        m_stream.reset();
    }
}

bool BufferedWriter::flush() {
    return flushBuffer();
}

bool BufferedWriter::writeLine(const std::string& line, const std::string& newline) {
    size_t totalSize = line.size() + newline.size();
    
    if (m_bufferUsed + totalSize > m_bufferSize) {
        if (!flushBuffer()) {
            return false;
        }
    }
    
    if (totalSize > m_bufferSize) {
        if (!flushBuffer()) {
            return false;
        }
        *m_stream << line << newline;
        if (!m_stream->good()) {
            return false;
        }
        m_bytesWritten += totalSize;
        return true;
    }
    
    std::memcpy(m_buffer.data() + m_bufferUsed, line.data(), line.size());
    m_bufferUsed += line.size();
    
    std::memcpy(m_buffer.data() + m_bufferUsed, newline.data(), newline.size());
    m_bufferUsed += newline.size();
    
    m_bytesWritten += totalSize;
    
    return true;
}

bool BufferedWriter::writeText(const std::string& text) {
    if (text.empty()) {
        return true;
    }
    
    if (m_bufferUsed + text.size() > m_bufferSize) {
        if (!flushBuffer()) {
            return false;
        }
    }
    
    if (text.size() > m_bufferSize) {
        if (!flushBuffer()) {
            return false;
        }
        m_stream->write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!m_stream->good()) {
            return false;
        }
        m_bytesWritten += text.size();
        return true;
    }
    
    std::memcpy(m_buffer.data() + m_bufferUsed, text.data(), text.size());
    m_bufferUsed += text.size();
    m_bytesWritten += text.size();
    
    return true;
}

uint64_t BufferedWriter::getBytesWritten() const {
    return m_bytesWritten;
}

size_t BufferedWriter::getBufferUsed() const {
    return m_bufferUsed;
}

size_t BufferedWriter::getBufferSize() const {
    return m_bufferSize;
}

bool BufferedWriter::flushBuffer() {
    if (!m_stream) {
        return false;
    }
    
    if (m_bufferUsed == 0) {
        return true;
    }
    
    m_stream->write(m_buffer.data(), static_cast<std::streamsize>(m_bufferUsed));
    
    if (!m_stream->good()) {
        return false;
    }
    
    m_bufferUsed = 0;
    return true;
}

}
}
