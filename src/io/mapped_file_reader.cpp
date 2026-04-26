#include "combinetool/io/mapped_file_reader.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_detector.h"

#include <cstring>

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
{
    if (openFile()) {
        auto encodingResult = encoding::EncodingDetector::detectFromFile(filePath);
        m_encoding = encodingResult.encoding;
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
            CP_ACP, 0, m_filePath.c_str(), static_cast<int>(m_filePath.length()), nullptr, 0
        );
        if (size > 0) {
            wPath.resize(size, 0);
            MultiByteToWideChar(
                CP_ACP, 0, m_filePath.c_str(), static_cast<int>(m_filePath.length()), &wPath[0], size
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
        m_progressCallback(m_currentOffset, m_fileSize);
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

}
}
