#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "combinetool/types.h"
#include "combinetool/config.h"
#include "combinetool/args/argument_parser.h"
#include "combinetool/merge/sequential_merger.h"
#include "combinetool/merge/interleaved_merger.h"
#include "combinetool/merge/conditional_merger.h"
#include "combinetool/merge/timestamp_sorted_merger.h"
#include "combinetool/merge/binary_merger.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/format/format_detector.h"

using namespace combinetool;
using namespace combinetool::args;
using namespace combinetool::merge;

#ifdef _WIN32
namespace {
std::vector<std::string> getUtf8Args() {
    std::vector<std::string> args;
    
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW == nullptr) {
        return args;
    }
    
    for (int i = 1; i < argc; ++i) {
        args.push_back(utils::StringUtils::fromWideString(std::wstring(argvW[i])));
    }
    
    LocalFree(argvW);
    return args;
}
}
#endif

void printBanner() {
    std::cout << R"(
  ______           _     _          _____           _ 
 |  ____|         | |   (_)        / ____|         | |
 | |__   _ __ ___ | |__  _ _ __   | |     ___   __| |
 |  __| | '_ ` _ \| '_ \| | '_ \  | |    / _ \ / _` |
 | |____| | | | | | |_) | | | | | | |___| (_) | (_| |
 |______|_| |_| |_|_.__/|_|_| |_|  \_____\___/ \__,_|
                                                         
)";
    std::cout << "  File Merge Tool v1.0.0\n";
    std::cout << "  A powerful cross-platform file merging utility\n\n";
}

void printUsage() {
    std::cout << R"(
Usage: combine-tool [OPTIONS] <INPUT_FILES...> -o OUTPUT_FILE

Core Merge Modes:
  -m, --mode MODE         Merge mode: sequential (default), interleaved, conditional,
                          timestamp-sorted, binary

File Format Options:
  -d, --delimiter CHAR    Output delimiter (default: ",")
  --no-header             Do not write header row
  -e, --encoding ENC      Target encoding: UTF-8 (default), UTF-16-LE, GBK, etc.

Timestamp Sort Options (for timestamp-sorted mode):
  --timestamp-format FMT  Timestamp format: auto (default), iso8601, rfc2822, unix, custom
  --timestamp-pattern PAT Custom timestamp pattern (e.g., "%Y-%m-%d %H:%M:%S")
  --timestamp-column COL  Column index for timestamp extraction (for CSV/TSV files)
  --timestamp-delimiter D Delimiter for column-based timestamp extraction (default: ",")

Binary Merge Options (for binary mode):
  --file-type TYPE        File type: auto (default), text, binary
  --binary-chunk SIZE     Chunk size for binary merge in bytes (default: 65536)

Deduplication Options:
  -D, --dedup MODE        Deduplication mode: none, line (default), column
  --dedup-keep STRATEGY   Keep strategy: first (default), last
  --dedup-columns RANGE   Column range for column-based deduplication (e.g., "0-2")
  --dedup-delimiter CHAR  Delimiter for column-based deduplication
  -I, --case-insensitive  Case-insensitive matching for filter and deduplicate

Filter Options:
  -i, --include PATTERNS  Include lines matching patterns (comma-separated)
  -x, --exclude PATTERNS  Exclude lines matching patterns (comma-separated)
  --filter-blank          Filter out blank lines
  --filter-comment        Filter out comment lines
  --comment-prefix PREFIX Comment prefix (default: "#")

Batch Processing:
  -r, --recursive         Recursively scan subdirectories
  --pattern GLOB          File pattern for recursive scan (default: "*")

Large File Support (Smart I/O):
  -M, --memory-map        Force use of memory-mapped I/O
  --smart-io              Enable/disable smart I/O (default: enabled)
  --mmap-threshold SIZE   Threshold for automatic memory-mapped I/O (default: 104857600 = 100MB)
  -b, --buffer-size SIZE  Buffer size for streaming I/O in bytes (default: 65536)

Conditional Merge Options:
  -k, --join-key KEY      Join key column index or name
  --join-type TYPE        Join type: inner (default), left, right, full

Information:
  -h, --help              Show this help message and exit
  --detect FILE           Detect file format, encoding, delimiter, etc.

Examples:
  # Basic sequential merge
  combine-tool file1.csv file2.csv -o output.csv

  # Interleaved merge with deduplication
  combine-tool -m interleaved -D line log1.txt log2.txt -o merged.txt

  # Timestamp sorted merge (auto-detect timestamp format)
  combine-tool -m timestamp-sorted log1.txt log2.txt log3.txt -o sorted_logs.txt

  # Timestamp sorted merge with specific column
  combine-tool -m timestamp-sorted --timestamp-column 0 --timestamp-delimiter "," data1.csv data2.csv -o sorted.csv

  # Binary file merge
  combine-tool -m binary part1.bin part2.bin part3.bin -o full.bin

  # Binary merge with custom chunk size
  combine-tool -m binary --binary-chunk 1048576 chunk*.bin -o merged.bin

  # Smart I/O with custom threshold
  combine-tool --mmap-threshold 52428800 large_file1.txt large_file2.txt -o output.txt

  # Recursive merge with filters
  combine-tool -r -i "error,warning" -x "debug" ./logs -o errors.txt

  # Conditional join (like SQL JOIN)
  combine-tool -m conditional -k 0 users.csv orders.csv -o joined.csv

  # Detect file information
  combine-tool --detect data.csv

)";
}

bool detectFileInfo(const std::string& filePath) {
    if (!utils::PathUtils::isFile(filePath)) {
        std::cerr << "Error: File not found: " << filePath << "\n";
        return false;
    }
    
    std::cout << "File: " << filePath << "\n";
    std::cout << "Size: " << utils::PathUtils::getFileSize(filePath) << " bytes\n\n";
    
    auto encodingResult = encoding::EncodingDetector::detectFromFile(filePath);
    std::cout << "Encoding Detection:\n";
    std::cout << "  Encoding: " << encodingResult.encodingName << "\n";
    std::cout << "  Confidence: " << (encodingResult.confidence * 100) << "%\n";
    std::cout << "  Has BOM: " << (encodingResult.hasBOM ? "Yes" : "No") << "\n\n";
    
    auto formatResult = format::FormatDetector::detectFromFile(filePath);
    std::cout << "Format Detection:\n";
    std::cout << "  Format: " << formatResult.formatName << "\n";
    std::cout << "  Confidence: " << (formatResult.confidence * 100) << "%\n";
    std::cout << "  Delimiter: '" << formatResult.detectedDelimiter << "' (confidence: " 
              << (formatResult.delimiterConfidence * 100) << "%)\n";
    std::cout << "  Has Header: " << (formatResult.hasHeader ? "Yes" : "No") 
              << " (confidence: " << (formatResult.headerConfidence * 100) << "%)\n";
    
    if (!formatResult.headerColumns.empty()) {
        std::cout << "  Header Columns: ";
        for (size_t i = 0; i < formatResult.headerColumns.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "'" << formatResult.headerColumns[i] << "'";
        }
        std::cout << "\n";
    }
    
    std::cout << "\nSample Lines Analyzed: " << formatResult.sampleLines << "\n";
    
    return true;
}

int main(int argc, char* argv[]) {
    ArgumentParser parser("combine-tool", "CombineTool - A powerful cross-platform file merging utility");
    
    parser.addArgument("help", "h", "Show help message", false, false, "", false);
    parser.addArgument("mode", "m", "Merge mode: sequential, interleaved, conditional, timestamp-sorted, binary", true, false, "sequential");
    parser.addArgument("output", "o", "Output file path", true, false);
    parser.addArgument("encoding", "e", "Target encoding (UTF-8, UTF-16-LE, GBK, etc.)", true, false, "UTF-8");
    parser.addArgument("delimiter", "d", "Output delimiter (use \\t for tab)", true, false, ",");
    
    parser.addArgument("timestamp-format", "", "Timestamp format: auto, iso8601, rfc2822, unix, custom", true, false, "auto");
    parser.addArgument("timestamp-pattern", "", "Custom timestamp pattern", true, false, "");
    parser.addArgument("timestamp-column", "", "Column index for timestamp extraction", true, false, "-1");
    parser.addArgument("timestamp-delimiter", "", "Delimiter for column-based timestamp extraction", true, false, ",");
    
    parser.addArgument("file-type", "", "File type: auto, text, binary", true, false, "auto");
    parser.addArgument("binary-chunk", "", "Chunk size for binary merge in bytes", true, false, "65536");
    
    parser.addArgument("smart-io", "", "Enable smart I/O (automatic memory mapping)", false, false, "", true);
    parser.addArgument("mmap-threshold", "", "Threshold for automatic memory-mapped I/O in bytes", true, false, "104857600");
    
    parser.addArgument("dedup", "D", "Deduplication mode: none, line, column", true, false, "none");
    parser.addArgument("dedup-keep", "", "Keep strategy for duplicates: first, last", true, false, "first");
    parser.addArgument("dedup-columns", "", "Column range for column-based deduplication (e.g., 0-2)", true);
    parser.addArgument("dedup-delimiter", "", "Delimiter for column-based deduplication", true, false, ",");
    parser.addArgument("case-insensitive", "I", "Case-insensitive matching", false, false, "", true);
    
    parser.addArgument("include", "i", "Include lines matching patterns (comma-separated)", true);
    parser.addArgument("exclude", "x", "Exclude lines matching patterns (comma-separated)", true);
    parser.addArgument("filter-blank", "", "Filter out blank lines", false, false, "", true);
    parser.addArgument("filter-comment", "", "Filter out comment lines", false, false, "", true);
    parser.addArgument("comment-prefix", "", "Comment prefix (default: #)", true, false, "#");
    
    parser.addArgument("recursive", "r", "Recursively scan subdirectories", false, false, "", true);
    parser.addArgument("pattern", "", "File pattern for recursive scan", true, false, "*");
    
    parser.addArgument("memory-map", "M", "Force use of memory-mapped I/O", false, false, "", true);
    parser.addArgument("buffer-size", "b", "Buffer size in bytes", true, false, "65536");
    
    parser.addArgument("join-key", "k", "Join key column index or name", true);
    parser.addArgument("join-type", "", "Join type: inner, left, right, full", true, false, "inner");
    
    parser.addArgument("no-header", "", "Do not write header row", false, false, "", true);
    parser.addArgument("detect", "", "Detect file format, encoding, etc.", true);
    
    ParseResult result;
#ifdef _WIN32
    std::vector<std::string> utf8Args = getUtf8Args();
    result = parser.parse(utf8Args);
#else
    result = parser.parse(argc, argv);
#endif
    
    if (!result.success) {
        if (result.errorMessage == "HELP_REQUESTED") {
            printBanner();
            printUsage();
            return 0;
        }
        
        std::cerr << "Error: " << result.errorMessage << "\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }
    
    if (parser.hasValue("detect")) {
        return detectFileInfo(parser.getValue("detect")) ? 0 : 1;
    }
    
    if (result.positionalArgs.empty()) {
        printBanner();
        std::cerr << "Error: No input files specified.\n";
        std::cerr << "Use --help for usage information.\n";
        return 1;
    }
    
    if (!parser.hasValue("output")) {
        std::cerr << "Error: Output file not specified (use -o or --output).\n";
        return 1;
    }
    
    MergeConfig config = ConfigBuilder::buildFromArgs(result);
    
    if (config.inputFiles.empty()) {
        std::cerr << "Error: No valid input files found.\n";
        return 1;
    }
    
    printBanner();
    
    std::cout << "Input files: " << config.inputFiles.size() << "\n";
    for (const auto& file : config.inputFiles) {
        std::cout << "  - " << file << "\n";
    }
    std::cout << "Output file: " << config.outputFile << "\n";
    std::cout << "Merge mode: ";
    switch (config.mode) {
        case MergeMode::Sequential: std::cout << "Sequential"; break;
        case MergeMode::Interleaved: std::cout << "Interleaved"; break;
        case MergeMode::Conditional: std::cout << "Conditional"; break;
        case MergeMode::TimestampSorted: std::cout << "Timestamp-Sorted"; break;
        case MergeMode::Binary: std::cout << "Binary"; break;
    }
    std::cout << "\n";
    
    if (config.mode != MergeMode::Binary) {
        std::cout << "Target encoding: " << encoding::EncodingDetector::encodingToString(config.targetEncoding) << "\n";
    }
    
    std::cout << "Smart I/O: " << (config.smartIOConfig.useSmartIO ? "Enabled" : "Disabled") << "\n";
    if (config.smartIOConfig.useSmartIO) {
        std::cout << "Memory map threshold: " << config.smartIOConfig.memoryMapThreshold << " bytes (" 
                  << std::fixed << std::setprecision(2) 
                  << static_cast<double>(config.smartIOConfig.memoryMapThreshold) / (1024 * 1024) << " MB)\n";
    }
    
    std::cout << "\nProcessing...\n";
    
    bool success = false;
    
    switch (config.mode) {
        case MergeMode::Sequential: {
            SequentialMerger merger(config);
            merger.setProgressCallback([](size_t current, size_t total, const std::string& file) {
                if (!file.empty()) {
                    std::cout << "  Processing: " << file << " (" << current << "/" << total << ")\r";
                    std::cout.flush();
                }
            });
            success = merger.merge();
            std::cout << "\n";
            std::cout << "Lines processed: " << merger.getTotalLinesProcessed() << "\n";
            std::cout << "Lines written: " << merger.getTotalLinesWritten() << "\n";
            std::cout << "Files processed: " << merger.getFilesProcessed() << "\n";
            break;
        }
        
        case MergeMode::Interleaved: {
            InterleavedMerger merger(config);
            merger.setChunkSize(1);
            success = merger.merge();
            std::cout << "Lines processed: " << merger.getTotalLinesProcessed() << "\n";
            std::cout << "Lines written: " << merger.getTotalLinesWritten() << "\n";
            std::cout << "Files processed: " << merger.getFilesProcessed() << "\n";
            break;
        }
        
        case MergeMode::Conditional: {
            ConditionalMerger merger(config);
            if (!config.conditionalExpression.empty()) {
                if (config.conditionalExpression.find_first_not_of("0123456789") == std::string::npos) {
                    merger.setJoinKeyColumn(std::stoi(config.conditionalExpression));
                } else {
                    merger.setJoinKeyColumn(config.conditionalExpression);
                }
            }
            success = merger.merge();
            std::cout << "Lines processed: " << merger.getTotalLinesProcessed() << "\n";
            std::cout << "Lines written: " << merger.getTotalLinesWritten() << "\n";
            std::cout << "Files processed: " << merger.getFilesProcessed() << "\n";
            break;
        }
        
        case MergeMode::TimestampSorted: {
            TimestampSortedMerger merger(config);
            merger.setProgressCallback([](size_t current, size_t total, const std::string& file) {
                if (!file.empty()) {
                    std::cout << "  Processing: " << file << " (" << current << "/" << total << ")\r";
                    std::cout.flush();
                }
            });
            success = merger.merge();
            std::cout << "\n";
            std::cout << "Lines processed: " << merger.getTotalLinesProcessed() << "\n";
            std::cout << "Lines written: " << merger.getTotalLinesWritten() << "\n";
            std::cout << "Files processed: " << merger.getFilesProcessed() << "\n";
            break;
        }
        
        case MergeMode::Binary: {
            BinaryMerger merger(config);
            merger.setProgressCallback([](size_t current, size_t total, const std::string& file) {
                if (!file.empty()) {
                    std::cout << "  Processing: " << file << " (" << current << "/" << total << ")\r";
                    std::cout.flush();
                }
            });
            success = merger.merge();
            std::cout << "\n";
            
            const auto& stats = merger.getStats();
            std::cout << "Bytes processed: " << stats.totalBytesRead << " bytes (" 
                      << std::fixed << std::setprecision(2) 
                      << static_cast<double>(stats.totalBytesRead) / (1024 * 1024) << " MB)\n";
            std::cout << "Bytes written: " << stats.totalBytesWritten << " bytes (" 
                      << std::fixed << std::setprecision(2) 
                      << static_cast<double>(stats.totalBytesWritten) / (1024 * 1024) << " MB)\n";
            std::cout << "Files processed: " << stats.filesProcessed << "\n";
            std::cout << "Chunks processed: " << stats.chunksProcessed << "\n";
            std::cout << "Elapsed time: " << std::fixed << std::setprecision(3) 
                      << stats.elapsedSeconds << " seconds\n";
            if (stats.elapsedSeconds > 0) {
                double speed = static_cast<double>(stats.totalBytesWritten) / stats.elapsedSeconds / (1024 * 1024);
                std::cout << "Average speed: " << std::fixed << std::setprecision(2) 
                          << speed << " MB/s\n";
            }
            break;
        }
    }
    
    if (success) {
        std::cout << "\nSuccess! Output written to: " << config.outputFile << "\n";
        return 0;
    } else {
        std::cerr << "\nError: Merge operation failed.\n";
        return 1;
    }
}
