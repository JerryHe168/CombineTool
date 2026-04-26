#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "combinetool/types.h"

namespace combinetool {
namespace args {

struct Argument {
    std::string name;
    std::string shortName;
    std::string description;
    bool hasValue;
    bool isRequired;
    std::string defaultValue;
    bool isFlag;
};

struct ParseResult {
    bool success;
    std::string errorMessage;
    std::vector<std::string> positionalArgs;
    std::map<std::string, std::string> namedArgs;
    std::vector<std::string> flags;
};

class ArgumentParser {
public:
    ArgumentParser(const std::string& programName, const std::string& description);
    
    void addArgument(
        const std::string& longName,
        const std::string& shortName,
        const std::string& description,
        bool hasValue = false,
        bool isRequired = false,
        const std::string& defaultValue = "",
        bool isFlag = false
    );
    
    ParseResult parse(int argc, char* argv[]);
    ParseResult parse(const std::vector<std::string>& args);
    
    std::string getHelp() const;
    std::string getUsage() const;
    
    bool hasFlag(const std::string& name) const;
    std::string getValue(const std::string& name) const;
    bool hasValue(const std::string& name) const;
    
    const std::vector<std::string>& getPositionalArgs() const;

private:
    std::string m_programName;
    std::string m_description;
    std::vector<Argument> m_arguments;
    ParseResult m_lastResult;
    
    Argument* findArgument(const std::string& name);
    const Argument* findArgument(const std::string& name) const;
    
    void printHelp() const;
};

class ConfigBuilder {
public:
    static MergeConfig buildFromArgs(const ParseResult& result);
    
    static MergeMode parseMergeMode(const std::string& mode);
    static Encoding parseEncoding(const std::string& encoding);
    static TextFormat parseFormat(const std::string& format);
    static DeduplicationMode parseDeduplicationMode(const std::string& mode);
    static DeduplicationKeepStrategy parseKeepStrategy(const std::string& strategy);
    
    static std::vector<std::string> expandInputFiles(
        const std::vector<std::string>& inputs,
        bool recursive,
        const std::string& pattern = "*"
    );
};

}
}
