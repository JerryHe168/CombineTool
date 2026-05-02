#pragma once

#include <cstddef>
#include <cstdint>

namespace combinetool {

namespace config {

constexpr size_t DEFAULT_BUFFER_SIZE = 64 * 1024;
constexpr size_t LARGE_BUFFER_SIZE = 1 * 1024 * 1024;
constexpr size_t MAX_MEMORY_MAP_SIZE = static_cast<size_t>(2) * 1024 * 1024 * 1024;

constexpr uint64_t DEFAULT_MEMORY_MAP_THRESHOLD = 100 * 1024 * 1024;
constexpr bool DEFAULT_USE_SMART_IO = true;

constexpr size_t FORMAT_DETECTION_SAMPLE_SIZE = 100 * 1024;
constexpr size_t ENCODING_DETECTION_SAMPLE_SIZE = 64 * 1024;
constexpr size_t HEADER_DETECTION_LINES = 10;

constexpr size_t ESTIMATE_LINE_COUNT_SAMPLE_LINES = 10;
constexpr size_t ESTIMATE_LINE_COUNT_DEFAULT_BYTES_PER_LINE = 100;

constexpr const char* DEFAULT_DELIMITER = ",";
constexpr const char* DEFAULT_COMMENT_PREFIX = "#";

constexpr bool DEFAULT_CASE_SENSITIVE = true;
constexpr bool DEFAULT_RECURSIVE = false;
constexpr bool DEFAULT_USE_MEMORY_MAPPING = false;
constexpr bool DEFAULT_KEEP_ORIGINAL_HEADERS = true;
constexpr bool DEFAULT_OUTPUT_HEADER = true;

constexpr int DEFAULT_START_COLUMN = 0;
constexpr int DEFAULT_END_COLUMN = -1;

constexpr size_t DEFAULT_BINARY_CHUNK_SIZE = 64 * 1024;

}

}
