#define NOMINMAX

#include "combinetool/io/mapped_file_reader.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_detector.h"

#include <cstring>
#include <algorithm>

namespace combinetool {
namespace io {

MappedFileReader::MappedFileReader(
    const std::string& filePath,
    size_t mapSize
)
    : m_filePath(filePath)
    , m_fileSize(0)
    , m_mapSize(mapSize)
    , m_encoding(Encoding::Unknown)
#ifdef _WIN32
    , m_fileHandle(INVALID_HANDLE_VALUE)
    , m_mapHandle(NULL)
    , m_mappedData(nullptr)
#else
    , m_fileDescriptor(-1)
    , m_mappedData(MAP_FAILED)
#endif
    , m_isOpen(false)
    , m_isMapped(false)
    , m_currentOffset(0)
    , m_chunkSize(config::LARGE_BUFFER_SIZE)
    , m_currentChunkOffset(0)
    , m_currentChunkSize(0)
    , m_chunkReadOffset(0)
    , m_isChunkedMode(false)
    , m_chunkNeedsRefill(true)
{
    if (openFile()) {
        auto encodingResult = encoding::EncodingDetector::detectFromFile(filePath);
        m_encoding = encodingResult.encoding;
        
        if (m_fileSize > m_mapSize) {
            m_isChunkedMode = true;
        }
    }
}

MappedFileReader::~MappedFileReader() {
    close();
}

bool MappedFileReader::isOpen() const {
    return m_isOpen;
}

bool MappedFileReader::isMapped() const {
    return m_isMapped;
}

void MappedFileReader::close() {
    unmap();
    closeFile();
}

bool MappedFileReader::map() {
    if (!m_isOpen) {
        return false;
    }
    
    if (m_isMapped) {
        return true;
    }
    
    if (m_fileSize > m_mapSize) {
        return false;
    }
    
#ifdef _WIN32
    m_mapHandle = CreateFileMappingW(
        m_fileHandle,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    
    if (m_mapHandle == NULL) {
        return false;
    }
    
    m_mappedData = MapViewOfFile(
        m_mapHandle,
        FILE_MAP_READ,
        0,
        0,
        0
    );
    
    if (m_mappedData == nullptr) {
        CloseHandle(m_mapHandle);
        m_mapHandle = NULL;
        return false;
    }
#else
    m_mappedData = mmap(
        nullptr,
        static_cast<size_t>(m_fileSize),
        PROT_READ,
        MAP_PRIVATE,
        m_fileDescriptor,
        0
    );
    
    if (m_mappedData == MAP_FAILED) {
        return false;
    }
#endif
    
    m_isMapped = true;
    m_currentOffset = 0;
    
    return true;
}

bool MappedFileReader::unmap() {
    if (!m_isMapped) {
        return true;
    }
    
#ifdef _WIN32
    if (m_mappedData != nullptr) {
        UnmapViewOfFile(m_mappedData);
        m_mappedData = nullptr;
    }
    if (m_mapHandle != NULL) {
        CloseHandle(m_mapHandle);
        m_mapHandle = NULL;
    }
#else
    if (m_mappedData != MAP_FAILED) {
        munmap(m_mappedData, static_cast<size_t>(m_fileSize));
        m_mappedData = MAP_FAILED;
    }
#endif
    
    m_isMapped = false;
    m_currentOffset = 0;
    
    return true;
}

const char* MappedFileReader::getData() const {
    if (!m_isMapped || m_mappedData == nullptr) {
        return nullptr;
    }
#ifdef _WIN32
    return static_cast<const char*>(m_mappedData);
#else
    return static_cast<const char*>(m_mappedData == MAP_FAILED ? nullptr : m_mappedData);
#endif
}

char* MappedFileReader::getData() {
    return const_cast<char*>(const_cast<const MappedFileReader*>(this)->getData());
}

size_t MappedFileReader::getMappedSize() const {
    if (!m_isMapped) {
        return 0;
    }
    return static_cast<size_t>(m_fileSize);
}

uint64_t MappedFileReader::getFileSize() const {
    return m_fileSize;
}

bool MappedFileReader::readLine(
    std::string& line,
    size_t& offset,
    bool keepNewline
) {
    line.clear();
    
    if (!m_isMapped || offset >= static_cast<size_t>(m_fileSize)) {
        return false;
    }
    
    const char* data = getData();
    if (data == nullptr) {
        return false;
    }
    
    size_t start = offset;
    size_t end = start;
    size_t fileSize = static_cast<size_t>(m_fileSize);
    bool foundNewline = false;
    bool hasCR = false;
    bool hasLF = false;
    
    while (end < fileSize) {
        if (data[end] == '\n') {
            foundNewline = true;
            hasLF = true;
            break;
        }
        if (data[end] == '\r') {
            foundNewline = true;
            hasCR = true;
            if (end + 1 < fileSize && data[end + 1] == '\n') {
                hasLF = true;
            }
            break;
        }
        ++end;
    }
    
    if (start == end && !foundNewline && end >= fileSize) {
        return false;
    }
    
    line.assign(data + start, end - start);
    
    if (foundNewline) {
        if (hasCR && hasLF) {
            end += 2;
        } else {
            end += 1;
        }
    }
    
    if (keepNewline && foundNewline) {
        if (hasCR) {
            line += "\r\n";
        } else {
            line += "\n";
        }
    }
    
    offset = end;
    m_currentOffset = end;
    
    updateProgress();
    
    return true;
}

const std::string& MappedFileReader::getFilePath() const {
    return m_filePath;
}

void MappedFileReader::setEncoding(Encoding encoding) {
    m_encoding = encoding;
}

Encoding MappedFileReader::getEncoding() const {
    return m_encoding;
}

void MappedFileReader::setProgressCallback(
    std::function<void(uint64_t, uint64_t)> callback
) {
    m_progressCallback = callback;
}

bool MappedFileReader::openFile() {
#ifdef _WIN32
    std::wstring wPath;
    if (!m_filePath.empty()) {
        int size = MultiByteToWideChar(
            CP_UTF8, 0, m_filePath.c_str(), static_cast<int>(m_filePath.length()), nullptr, 0
        );
        if (size > 0) {
            wPath.resize(size, 0);
            MultiByteToWideChar(
                CP_UTF8, 0, m_filePath.c_str(), static_cast<int>(m_filePath.length()), &wPath[0], size
            );
        }
    }
    
    m_fileHandle = CreateFileW(
        wPath.empty() ? nullptr : wPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER size;
    if (!GetFileSizeEx(m_fileHandle, &size)) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    m_fileSize = static_cast<uint64_t>(size.QuadPart);
#else
    m_fileDescriptor = open(m_filePath.c_str(), O_RDONLY);
    if (m_fileDescriptor < 0) {
        return false;
    }
    
    struct stat st;
    if (fstat(m_fileDescriptor, &st) != 0) {
        close(m_fileDescriptor);
        m_fileDescriptor = -1;
        return false;
    }
    
    m_fileSize = static_cast<uint64_t>(st.st_size);
#endif
    
    m_isOpen = true;
    return true;
}

void MappedFileReader::closeFile() {
    if (!m_isOpen) {
        return;
    }
    
#ifdef _WIN32
    if (m_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_fileDescriptor >= 0) {
        close(m_fileDescriptor);
        m_fileDescriptor = -1;
    }
#endif
    
    m_isOpen = false;
}

void MappedFileReader::updateProgress() {
    if (m_progressCallback && m_fileSize > 0) {
        m_progressCallback(getCurrentOffset(), m_fileSize);
    }
}

MappedLineIterator::MappedLineIterator(MappedFileReader& reader)
    : m_reader(reader)
    , m_offset(0)
    , m_hasNext(false)
    , m_initialized(false)
{
}

bool MappedLineIterator::hasNext() const {
    if (!m_initialized) {
        const_cast<MappedLineIterator*>(this)->prefetch();
    }
    return m_hasNext;
}

bool MappedLineIterator::next(std::string& line) {
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

void MappedLineIterator::reset() {
    m_offset = 0;
    m_hasNext = false;
    m_initialized = false;
    m_nextLine.clear();
}

void MappedLineIterator::prefetch() {
    m_initialized = true;
    m_hasNext = false;
    m_nextLine.clear();
    
    m_hasNext = m_reader.readLine(m_nextLine, m_offset, false);
}

void MappedFileReader::setChunkSize(size_t chunkSize) {
    m_chunkSize = chunkSize;
}

size_t MappedFileReader::getChunkSize() const {
    return m_chunkSize;
}

bool MappedFileReader::isChunkedMode() const {
    return m_isChunkedMode;
}

size_t MappedFileReader::getSystemPageSize() const {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<size_t>(sysInfo.dwPageSize);
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

uint64_t MappedFileReader::alignToPageBoundary(uint64_t offset) const {
    size_t pageSize = getSystemPageSize();
    return offset - (offset % pageSize);
}

bool MappedFileReader::mapChunk(uint64_t offset) {
    if (!m_isOpen) {
        return false;
    }
    
    if (m_isMapped) {
        unmapChunk();
    }
    
    if (offset >= m_fileSize) {
        return false;
    }
    
    uint64_t alignedOffset = alignToPageBoundary(offset);
    uint64_t offsetWithinPage = offset - alignedOffset;
    size_t actualChunkSize = m_chunkSize;
    
    if (alignedOffset + actualChunkSize > m_fileSize) {
        actualChunkSize = static_cast<size_t>(m_fileSize - alignedOffset);
    }
    
#ifdef _WIN32
    m_mapHandle = CreateFileMappingW(
        m_fileHandle,
        NULL,
        PAGE_READONLY,
        static_cast<DWORD>(alignedOffset >> 32),
        static_cast<DWORD>(alignedOffset & 0xFFFFFFFF),
        NULL
    );
    
    if (m_mapHandle == NULL) {
        return false;
    }
    
    DWORD highOffset = static_cast<DWORD>(alignedOffset >> 32);
    DWORD lowOffset = static_cast<DWORD>(alignedOffset & 0xFFFFFFFF);
    
    m_mappedData = MapViewOfFile(
        m_mapHandle,
        FILE_MAP_READ,
        highOffset,
        lowOffset,
        actualChunkSize
    );
    
    if (m_mappedData == nullptr) {
        CloseHandle(m_mapHandle);
        m_mapHandle = NULL;
        return false;
    }
#else
    m_mappedData = mmap(
        nullptr,
        actualChunkSize,
        PROT_READ,
        MAP_PRIVATE,
        m_fileDescriptor,
        static_cast<off_t>(alignedOffset)
    );
    
    if (m_mappedData == MAP_FAILED) {
        return false;
    }
#endif
    
    m_currentChunkOffset = alignedOffset;
    m_currentChunkSize = actualChunkSize;
    m_chunkReadOffset = static_cast<size_t>(offsetWithinPage);
    m_isMapped = true;
    m_chunkNeedsRefill = false;
    
    return true;
}

bool MappedFileReader::unmapChunk() {
    return unmap();
}

bool MappedFileReader::mapNextChunk() {
    uint64_t nextOffset = m_currentChunkOffset + m_currentChunkSize;
    return mapChunk(nextOffset);
}

const char* MappedFileReader::getChunkData() const {
    if (!m_isMapped || m_mappedData == nullptr) {
        return nullptr;
    }
    return static_cast<const char*>(m_mappedData);
}

size_t MappedFileReader::getChunkSizeMapped() const {
    return m_currentChunkSize;
}

uint64_t MappedFileReader::getChunkOffset() const {
    return m_currentChunkOffset;
}

uint64_t MappedFileReader::getCurrentOffset() const {
    return !m_isChunkedMode ? static_cast<uint64_t>(m_currentOffset) :
           (m_currentChunkOffset + m_chunkReadOffset);
}

bool MappedFileReader::isAtEndOfFile() const {
    return getCurrentOffset() >= m_fileSize;
}

bool MappedFileReader::readBytes(char* buffer, size_t bufferSize, size_t& bytesRead) {
    bytesRead = 0;
    
    if (!m_isChunkedMode) {
        if (!m_isMapped) {
            return false;
        }
        
        const char* data = getData();
        if (data == nullptr) {
            return false;
        }
        
        size_t remaining = static_cast<size_t>(m_fileSize) - m_currentOffset;
        size_t toRead = std::min(bufferSize, remaining);
        
        if (toRead > 0) {
            std::memcpy(buffer, data + m_currentOffset, toRead);
            bytesRead = toRead;
            m_currentOffset += toRead;
            updateProgress();
        }
        
        return bytesRead > 0 || m_currentOffset < m_fileSize;
    }
    
    while (bytesRead < bufferSize && !isAtEndOfFile()) {
        if (m_chunkNeedsRefill || !m_isMapped) {
            uint64_t nextOffset = m_currentChunkOffset + m_chunkReadOffset;
            if (!mapChunk(nextOffset)) {
                break;
            }
        }
        
        const char* data = getChunkData();
        if (data == nullptr) {
            break;
        }
        
        size_t remainingInChunk = m_currentChunkSize - m_chunkReadOffset;
        size_t remainingNeeded = bufferSize - bytesRead;
        size_t toRead = std::min(remainingInChunk, remainingNeeded);
        
        if (toRead > 0) {
            std::memcpy(buffer + bytesRead, data + m_chunkReadOffset, toRead);
            bytesRead += toRead;
            m_chunkReadOffset += toRead;
        }
        
        if (m_chunkReadOffset >= m_currentChunkSize) {
            m_chunkNeedsRefill = true;
        }
    }
    
    return bytesRead > 0 || (!isAtEndOfFile());
}

bool MappedFileReader::readLineChunked(std::string& line, bool keepNewline) {
    line.clear();
    
    if (!m_isChunkedMode) {
        return false;
    }
    
    std::string accumulated;
    bool foundNewline = false;
    bool hasCR = false;
    bool hasLF = false;
    
    while (!isAtEndOfFile() && !foundNewline) {
        if (m_chunkNeedsRefill || !m_isMapped) {
            uint64_t nextOffset = m_currentChunkOffset + m_chunkReadOffset;
            if (!mapChunk(nextOffset)) {
                break;
            }
        }
        
        const char* data = getChunkData();
        if (data == nullptr) {
            break;
        }
        
        size_t start = m_chunkReadOffset;
        size_t end = start;
        size_t chunkSize = m_currentChunkSize;
        
        while (end < chunkSize) {
            if (data[end] == '\n') {
                foundNewline = true;
                hasLF = true;
                break;
            }
            if (data[end] == '\r') {
                foundNewline = true;
                hasCR = true;
                if (end + 1 < chunkSize && data[end + 1] == '\n') {
                    hasLF = true;
                }
                break;
            }
            ++end;
        }
        
        if (start < end) {
            accumulated.append(data + start, end - start);
        }
        
        if (foundNewline) {
            if (hasCR && hasLF) {
                end += 2;
            } else {
                end += 1;
            }
        } else {
            end = chunkSize;
        }
        
        m_chunkReadOffset = end;
        
        if (m_chunkReadOffset >= m_currentChunkSize) {
            m_chunkNeedsRefill = true;
        }
    }
    
    if (accumulated.empty() && !foundNewline && isAtEndOfFile()) {
        return false;
    }
    
    line = accumulated;
    
    if (keepNewline && foundNewline) {
        if (hasCR) {
            line += "\r\n";
        } else {
            line += "\n";
        }
    }
    
    return true;
}

}
}
