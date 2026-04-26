cmake_minimum_required(VERSION 3.14)
project(CombineTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W3 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(SOURCES
    src/main.cpp
    src/utils/string_utils.cpp
    src/utils/file_utils.cpp
    src/utils/path_utils.cpp
    src/utils/timestamp_extractor.cpp
    src/encoding/encoding_detector.cpp
    src/encoding/encoding_converter.cpp
    src/format/format_detector.cpp
    src/filter/filter.cpp
    src/merge/merger.cpp
    src/merge/sequential_merger.cpp
    src/merge/interleaved_merger.cpp
    src/merge/conditional_merger.cpp
    src/merge/timestamp_sorted_merger.cpp
    src/merge/binary_merger.cpp
    src/io/stream_reader.cpp
    src/io/mapped_file_reader.cpp
    src/io/smart_file_reader.cpp
    src/io/stream_writer.cpp
    src/args/argument_parser.cpp
)

set(HEADERS
    include/combinetool/utils/string_utils.h
    include/combinetool/utils/file_utils.h
    include/combinetool/utils/path_utils.h
    include/combinetool/utils/timestamp_extractor.h
    include/combinetool/encoding/encoding_detector.h
    include/combinetool/encoding/encoding_converter.h
    include/combinetool/format/format_detector.h
    include/combinetool/filter/filter.h
    include/combinetool/merge/merger.h
    include/combinetool/merge/sequential_merger.h
    include/combinetool/merge/interleaved_merger.h
    include/combinetool/merge/conditional_merger.h
    include/combinetool/merge/timestamp_sorted_merger.h
    include/combinetool/merge/binary_merger.h
    include/combinetool/io/stream_reader.h
    include/combinetool/io/mapped_file_reader.h
    include/combinetool/io/smart_file_reader.h
    include/combinetool/io/stream_writer.h
    include/combinetool/args/argument_parser.h
    include/combinetool/types.h
    include/combinetool/config.h
)

add_executable(combine-tool ${SOURCES} ${HEADERS})

target_include_directories(combine-tool
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(combine-tool PRIVATE shlwapi)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(combine-tool PRIVATE dl)
endif()

set_target_properties(combine-tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS combine-tool
    RUNTIME DESTINATION bin
)
