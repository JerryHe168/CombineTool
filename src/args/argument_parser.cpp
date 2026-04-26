#include "combinetool/args/argument_parser.h"
#include "combinetool/utils/string_utils.h"
#include "combinetool/utils/path_utils.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/format/format_detector.h"
#include "combinetool/config.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace combinetool {
namespace args {

ArgumentParser::ArgumentParser(
    const std::string& programName,
    const std::string& description
)
    : m_programName(programName)
    , m_description(description)
{
}

void ArgumentParser::addArgument(
    const std::string& longName,
    const std::string& shortName,
    const std::string& description,
    bool hasValue,
    bool isRequired,
    const std::string& defaultValue,
    bool isFlag
) {
    Argument arg;
    arg.name = longName;
    arg.shortName = shortName;
    arg.description = description;
    arg.hasValue = hasValue;
    arg.isRequired = isRequired;
    arg.defaultValue = defaultValue;
    arg.isFlag = isFlag;
    
    m_arguments.push_back(arg);
}

ParseResult ArgumentParser::parse(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return parse(args);
}

ParseResult ArgumentParser::parse(const std::vector<std::string>& args) {
    ParseResult result;
    result.success = true;
    
    for (const auto& arg : m_arguments) {
        if (!arg.defaultValue.empty() && arg.hasValue) {
            result.namedArgs[arg.name] = arg.defaultValue;
        }
    }
    
    size_t i = 0;
    while (i < args.size()) {
        const auto& arg = args[i];
        
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            result.success = false;
            result.errorMessage = "HELP_REQUESTED";
            printHelp();
            m_lastResult = result;
            return result;
        }
        
        if (arg == "--") {
            for (size_t j = i + 1; j < args.size(); ++j) {
                result.positionalArgs.push_back(args[j]);
            }
            break;
        }
        
        if (utils::StringUtils::startsWith(arg, "--")) {
            std::string name = arg.substr(2);
            auto* argument = findArgument(name);
            
            if (!argument) {
                result.success = false;
                result.errorMessage = "Unknown argument: " + arg;
                m_lastResult = result;
                return result;
            }
            
            if (argument->isFlag) {
                result.flags.push_back(argument->name);
            } else if (argument->hasValue) {
                if (i + 1 >= args.size()) {
                    result.success = false;
                    result.errorMessage = "Argument " + arg + " requires a value";
                    m_lastResult = result;
                    return result;
                }
                result.namedArgs[argument->name] = args[i + 1];
                ++i;
            } else {
                result.namedArgs[argument->name] = "true";
            }
        } else if (utils::StringUtils::startsWith(arg, "-") && arg.size() > 1) {
            if (arg.size() == 2) {
                std::string shortName(1, arg[1]);
                auto* argument = findArgument(shortName);
                
                if (!argument) {
                    result.success = false;
                    result.errorMessage = "Unknown argument: " + arg;
                    m_lastResult = result;
                    return result;
                }
                
                if (argument->isFlag) {
                    result.flags.push_back(argument->name);
                } else if (argument->hasValue) {
                    if (i + 1 >= args.size()) {
                        result.success = false;
                        result.errorMessage = "Argument " + arg + " requires a value";
                        m_lastResult = result;
                        return result;
                    }
                    result.namedArgs[argument->name] = args[i + 1];
                    ++i;
                } else {
                    result.namedArgs[argument->name] = "true";
                }
            } else {
                for (size_t j = 1; j < arg.size(); ++j) {
                    std::string shortName(1, arg[j]);
                    auto* argument = findArgument(shortName);
                    
                    if (!argument) {
                        result.success = false;
                        result.errorMessage = "Unknown argument: -" + std::string(1, arg[j]);
                        m_lastResult = result;
                        return result;
                    }
                    
                    if (argument->isFlag) {
                        result.flags.push_back(argument->name);
                    }
                }
            }
        } else {
            result.positionalArgs.push_back(arg);
        }
        
        ++i;
    }
    
    for (const auto& arg : m_arguments) {
        if (arg.isRequired) {
            bool found = false;
            for (const auto& flag : result.flags) {
                if (flag == arg.name) {
                    found = true;
                    break;
                }
            }
            if (result.namedArgs.count(arg.name) > 0) {
                found = true;
            }
            if (!found) {
                result.success = false;
                result.errorMessage = "Required argument missing: --" + arg.name;
                m_lastResult = result;
                return result;
            }
        }
    }
    
    m_lastResult = result;
    return result;
}

std::string ArgumentParser::getHelp() const {
    std::ostringstream oss;
    
    oss << m_description << "\n\n";
    oss << "Usage:\n  " << getUsage() << "\n\n";
    
    if (!m_arguments.empty()) {
        oss << "Options:\n";
        
        size_t maxWidth = 0;
        for (const auto& arg : m_arguments) {
            size_t width = arg.name.size() + (arg.shortName.empty() ? 0 : 4) + (arg.hasValue ? 6 : 0);
            maxWidth = std::max(maxWidth, width);
        }
        
        for (const auto& arg : m_arguments) {
            std::ostringstream argStr;
            
            if (!arg.shortName.empty()) {
                argStr << "-" << arg.shortName << ", ";
            }
            argStr << "--" << arg.name;
            if (arg.hasValue) {
                argStr << " <value>";
            }
            
            oss << "  " << std::left << std::setw(static_cast<int>(maxWidth + 2)) << argStr.str()
                << arg.description;
            
            if (arg.isRequired) {
                oss << " [required]";
            }
            if (!arg.defaultValue.empty()) {
                oss << " [default: " << arg.defaultValue << "]";
            }
            
            oss << "\n";
        }
    }
    
    return oss.str();
}

std::string ArgumentParser::getUsage() const {
    std::ostringstream oss;
    
    oss << m_programName << " [options] <inputs...> -o <output>";
    return oss.str();
}

bool ArgumentParser::hasFlag(const std::string& name) const {
    for (const auto& flag : m_lastResult.flags) {
        if (flag == name) {
            return true;
        }
    }
    return false;
}

std::string ArgumentParser::getValue(const std::string& name) const {
    auto it = m_lastResult.namedArgs.find(name);
    if (it != m_lastResult.namedArgs.end()) {
        return it->second;
    }
    return "";
}

bool ArgumentParser::hasValue(const std::string& name) const {
    return m_lastResult.namedArgs.count(name) > 0;
}

const std::vector<std::string>& ArgumentParser::getPositionalArgs() const {
    return m_lastResult.positionalArgs;
}

Argument* ArgumentParser::findArgument(const std::string& name) {
    for (auto& arg : m_arguments) {
        if (arg.name == name || arg.shortName == name) {
            return &arg;
        }
    }
    return nullptr;
}

const Argument* ArgumentParser::findArgument(const std::string& name) const {
    for (const auto& arg : m_arguments) {
        if (arg.name == name || arg.shortName == name) {
            return &arg;
        }
    }
    return nullptr;
}

void ArgumentParser::printHelp() const {
}

MergeConfig ConfigBuilder::buildFromArgs(const ParseResult& result) {
    MergeConfig config;
    
    config.mode = MergeMode::Sequential;
    config.targetEncoding = Encoding::UTF8;
    config.outputDelimiter = config::DEFAULT_DELIMITER;
    config.recursive = config::DEFAULT_RECURSIVE;
    config.useMemoryMapping = config::DEFAULT_USE_MEMORY_MAPPING;
    config.bufferSize = config::DEFAULT_BUFFER_SIZE;
    config.keepOriginalHeaders = config::DEFAULT_KEEP_ORIGINAL_HEADERS;
    config.outputHeader = config::DEFAULT_OUTPUT_HEADER;
    
    config.filterConfig.mode = FilterMode::Exclude;
    config.filterConfig.filterBlankLines = false;
    config.filterConfig.filterCommentLines = false;
    config.filterConfig.commentPrefix = config::DEFAULT_COMMENT_PREFIX;
    config.filterConfig.caseSensitive = config::DEFAULT_CASE_SENSITIVE;
    
    config.deduplicationConfig.mode = DeduplicationMode::None;
    config.deduplicationConfig.keepStrategy = DeduplicationKeepStrategy::First;
    config.deduplicationConfig.startColumn = config::DEFAULT_START_COLUMN;
    config.deduplicationConfig.endColumn = config::DEFAULT_END_COLUMN;
    config.deduplicationConfig.caseSensitive = config::DEFAULT_CASE_SENSITIVE;
    
    for (const auto& kv : result.namedArgs) {
        const auto& key = kv.first;
        const auto& value = kv.second;
        
        if (key == "mode" || key == "m") {
            config.mode = parseMergeMode(value);
        } else if (key == "output" || key == "o") {
            config.outputFile = value;
        } else if (key == "encoding" || key == "e") {
            config.targetEncoding = parseEncoding(value);
        } else if (key == "delimiter" || key == "d") {
            if (value == "\\t" || value == "\\t") {
                config.outputDelimiter = "\t";
            } else {
                config.outputDelimiter = value;
            }
        } else if (key == "join-key" || key == "k") {
            config.conditionalExpression = value;
        } else if (key == "join-type") {
        } else if (key == "include" || key == "i") {
            config.filterConfig.includePatterns = utils::StringUtils::split(value, ",", false);
        } else if (key == "exclude" || key == "x") {
            config.filterConfig.excludePatterns = utils::StringUtils::split(value, ",", false);
        } else if (key == "filter-blank") {
            config.filterConfig.filterBlankLines = (value == "true" || value == "1" || value == "yes");
        } else if (key == "filter-comment") {
            config.filterConfig.filterCommentLines = (value == "true" || value == "1" || value == "yes");
        } else if (key == "comment-prefix") {
            config.filterConfig.commentPrefix = value;
        } else if (key == "dedup" || key == "D") {
            config.deduplicationConfig.mode = parseDeduplicationMode(value);
        } else if (key == "dedup-keep") {
            config.deduplicationConfig.keepStrategy = parseKeepStrategy(value);
        } else if (key == "dedup-columns") {
            auto parts = utils::StringUtils::split(value, "-", false);
            if (parts.size() >= 1) {
                config.deduplicationConfig.startColumn = std::stoi(parts[0]);
                if (parts.size() >= 2) {
                    config.deduplicationConfig.endColumn = std::stoi(parts[1]);
                }
            }
        } else if (key == "dedup-delimiter") {
            config.deduplicationConfig.delimiter = value;
        } else if (key == "case-insensitive" || key == "I") {
            config.filterConfig.caseSensitive = false;
            config.deduplicationConfig.caseSensitive = false;
        } else if (key == "recursive" || key == "r") {
            config.recursive = (value == "true" || value == "1" || value == "yes" || value.empty());
        } else if (key == "memory-map" || key == "M") {
            config.useMemoryMapping = (value == "true" || value == "1" || value == "yes" || value.empty());
        } else if (key == "no-header") {
            config.outputHeader = false;
        } else if (key == "buffer-size" || key == "b") {
            config.bufferSize = static_cast<size_t>(std::stoull(value));
        }
    }
    
    for (const auto& flag : result.flags) {
        if (flag == "recursive" || flag == "r") {
            config.recursive = true;
        } else if (flag == "memory-map" || flag == "M") {
            config.useMemoryMapping = true;
        } else if (flag == "case-insensitive" || flag == "I") {
            config.filterConfig.caseSensitive = false;
            config.deduplicationConfig.caseSensitive = false;
        } else if (flag == "filter-blank") {
            config.filterConfig.filterBlankLines = true;
        } else if (flag == "filter-comment") {
            config.filterConfig.filterCommentLines = true;
        } else if (flag == "no-header") {
            config.outputHeader = false;
        }
    }
    
    config.inputFiles = expandInputFiles(result.positionalArgs, config.recursive);
    
    return config;
}

MergeMode ConfigBuilder::parseMergeMode(const std::string& mode) {
    std::string lower = utils::StringUtils::toLower(mode);
    
    if (lower == "sequential" || lower == "seq" || lower == "s") {
        return MergeMode::Sequential;
    }
    if (lower == "interleaved" || lower == "interleave" || lower == "i") {
        return MergeMode::Interleaved;
    }
    if (lower == "conditional" || lower == "condition" || lower == "join" || lower == "c") {
        return MergeMode::Conditional;
    }
    
    return MergeMode::Sequential;
}

Encoding ConfigBuilder::parseEncoding(const std::string& encoding) {
    return encoding::EncodingDetector::stringToEncoding(encoding);
}

TextFormat ConfigBuilder::parseFormat(const std::string& format) {
    return format::FormatDetector::detectFormatByName(format);
}

DeduplicationMode ConfigBuilder::parseDeduplicationMode(const std::string& mode) {
    std::string lower = utils::StringUtils::toLower(mode);
    
    if (lower == "none" || lower == "off" || lower == "false") {
        return DeduplicationMode::None;
    }
    if (lower == "line" || lower == "full" || lower == "f") {
        return DeduplicationMode::FullLine;
    }
    if (lower == "partial" || lower == "p") {
        return DeduplicationMode::Partial;
    }
    if (lower == "column" || lower == "columns" || lower == "c") {
        return DeduplicationMode::ColumnBased;
    }
    
    return DeduplicationMode::FullLine;
}

DeduplicationKeepStrategy ConfigBuilder::parseKeepStrategy(const std::string& strategy) {
    std::string lower = utils::StringUtils::toLower(strategy);
    
    if (lower == "first" || lower == "f") {
        return DeduplicationKeepStrategy::First;
    }
    if (lower == "last" || lower == "l") {
        return DeduplicationKeepStrategy::Last;
    }
    if (lower == "all" || lower == "a") {
        return DeduplicationKeepStrategy::All;
    }
    
    return DeduplicationKeepStrategy::First;
}

std::vector<std::string> ConfigBuilder::expandInputFiles(
    const std::vector<std::string>& inputs,
    bool recursive,
    const std::string& pattern
) {
    std::vector<std::string> files;
    
    for (const auto& input : inputs) {
        if (utils::PathUtils::isFile(input)) {
            files.push_back(input);
        } else if (utils::PathUtils::isDirectory(input)) {
            auto dirFiles = utils::PathUtils::listFiles(input, pattern, recursive);
            files.insert(files.end(), dirFiles.begin(), dirFiles.end());
        } else {
            size_t lastSep = input.find_last_of("\\/");
            std::string dir, filePattern;
            
            if (lastSep != std::string::npos) {
                dir = input.substr(0, lastSep);
                filePattern = input.substr(lastSep + 1);
            } else {
                dir = ".";
                filePattern = input;
            }
            
            if (dir.empty()) {
                dir = ".";
            }
            
            if (utils::PathUtils::isDirectory(dir)) {
                auto dirFiles = utils::PathUtils::listFiles(dir, filePattern, recursive);
                files.insert(files.end(), dirFiles.begin(), dirFiles.end());
            }
        }
    }
    
    std::sort(files.begin(), files.end());
    auto last = std::unique(files.begin(), files.end());
    files.erase(last, files.end());
    
    return files;
}

}
}
