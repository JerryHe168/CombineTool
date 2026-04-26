#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cctype>

namespace combinetool {
namespace utils {

class StringUtils {
public:
    static std::string trim(const std::string& str);
    static std::string trimLeft(const std::string& str);
    static std::string trimRight(const std::string& str);
    
    static std::vector<std::string> split(
        const std::string& str, 
        const std::string& delimiter,
        bool keepEmpty = false
    );
    
    static std::vector<std::string_view> splitView(
        std::string_view str,
        std::string_view delimiter,
        bool keepEmpty = false
    );
    
    static std::string join(
        const std::vector<std::string>& parts,
        const std::string& delimiter
    );
    
    static std::string toLower(const std::string& str);
    static std::string toUpper(const std::string& str);
    
    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    static bool contains(const std::string& str, const std::string& substring);
    static bool containsIgnoreCase(const std::string& str, const std::string& substring);
    
    static bool isBlank(const std::string& str);
    static bool isEmpty(const std::string& str);
    
    static std::string replace(
        const std::string& str,
        const std::string& from,
        const std::string& to
    );
    
    static std::string replaceAll(
        const std::string& str,
        const std::string& from,
        const std::string& to
    );
    
    static size_t countOccurrences(const std::string& str, const std::string& substring);
    
    static std::string escapeRegex(const std::string& str);
    
    static bool wildCardMatch(const std::string& pattern, const std::string& str);
    
    static std::wstring toWideString(const std::string& str);
    static std::string fromWideString(const std::wstring& wstr);
};

}
}
