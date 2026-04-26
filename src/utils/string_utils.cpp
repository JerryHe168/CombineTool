#include "combinetool/utils/string_utils.h"
#include <algorithm>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace combinetool {
namespace utils {

std::string StringUtils::trim(const std::string& str) {
    return trimRight(trimLeft(str));
}

std::string StringUtils::trimLeft(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](int ch) {
        return std::isspace(static_cast<unsigned char>(ch));
    });
    return std::string(start, str.end());
}

std::string StringUtils::trimRight(const std::string& str) {
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](int ch) {
        return std::isspace(static_cast<unsigned char>(ch));
    }).base();
    return std::string(str.begin(), end);
}

std::vector<std::string> StringUtils::split(
    const std::string& str, 
    const std::string& delimiter,
    bool keepEmpty
) {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        for (char c : str) {
            result.emplace_back(1, c);
        }
        return result;
    }
    
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos) {
        std::string part = str.substr(prev, pos - prev);
        if (keepEmpty || !part.empty()) {
            result.push_back(std::move(part));
        }
        prev = pos + delimiter.length();
    }
    
    std::string part = str.substr(prev);
    if (keepEmpty || !part.empty()) {
        result.push_back(std::move(part));
    }
    
    return result;
}

std::vector<std::string_view> StringUtils::splitView(
    std::string_view str,
    std::string_view delimiter,
    bool keepEmpty
) {
    std::vector<std::string_view> result;
    if (delimiter.empty()) {
        for (char c : str) {
            result.emplace_back(&c, 1);
        }
        return result;
    }
    
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string_view::npos) {
        std::string_view part = str.substr(prev, pos - prev);
        if (keepEmpty || !part.empty()) {
            result.push_back(part);
        }
        prev = pos + delimiter.length();
    }
    
    std::string_view part = str.substr(prev);
    if (keepEmpty || !part.empty()) {
        result.push_back(part);
    }
    
    return result;
}

std::string StringUtils::join(
    const std::vector<std::string>& parts,
    const std::string& delimiter
) {
    if (parts.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        oss << delimiter << parts[i];
    }
    return oss.str();
}

std::string StringUtils::toLower(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (unsigned char ch : str) {
        result += static_cast<char>(std::tolower(ch));
    }
    return result;
}

std::string StringUtils::toUpper(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (unsigned char ch : str) {
        result += static_cast<char>(std::toupper(ch));
    }
    return result;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool StringUtils::contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}

bool StringUtils::containsIgnoreCase(const std::string& str, const std::string& substring) {
    std::string strLower = toLower(str);
    std::string subLower = toLower(substring);
    return strLower.find(subLower) != std::string::npos;
}

bool StringUtils::isBlank(const std::string& str) {
    return str.empty() || std::all_of(str.begin(), str.end(), [](int ch) {
        return std::isspace(static_cast<unsigned char>(ch));
    });
}

bool StringUtils::isEmpty(const std::string& str) {
    return str.empty();
}

std::string StringUtils::replace(
    const std::string& str,
    const std::string& from,
    const std::string& to
) {
    if (from.empty()) {
        return str;
    }
    
    std::string result = str;
    size_t pos = result.find(from);
    if (pos != std::string::npos) {
        result.replace(pos, from.length(), to);
    }
    return result;
}

std::string StringUtils::replaceAll(
    const std::string& str,
    const std::string& from,
    const std::string& to
) {
    if (from.empty()) {
        return str;
    }
    
    std::string result;
    result.reserve(str.length());
    
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = str.find(from, prev)) != std::string::npos) {
        result.append(str, prev, pos - prev);
        result.append(to);
        prev = pos + from.length();
    }
    
    result.append(str, prev, std::string::npos);
    return result;
}

size_t StringUtils::countOccurrences(const std::string& str, const std::string& substring) {
    if (substring.empty()) {
        return 0;
    }
    
    size_t count = 0;
    size_t pos = 0;
    while ((pos = str.find(substring, pos)) != std::string::npos) {
        ++count;
        pos += substring.length();
    }
    return count;
}

std::string StringUtils::escapeRegex(const std::string& str) {
    static const std::string specialChars = "\\^$.|?*+()[]{}/";
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char ch : str) {
        if (specialChars.find(ch) != std::string::npos) {
            result += '\\';
        }
        result += ch;
    }
    return result;
}

bool StringUtils::wildCardMatch(const std::string& pattern, const std::string& str) {
    size_t pIdx = 0;
    size_t sIdx = 0;
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;
    
    while (sIdx < str.size()) {
        if (pIdx < pattern.size() && (pattern[pIdx] == str[sIdx] || pattern[pIdx] == '?')) {
            ++pIdx;
            ++sIdx;
        } else if (pIdx < pattern.size() && pattern[pIdx] == '*') {
            starIdx = pIdx++;
            matchIdx = sIdx;
        } else if (starIdx != std::string::npos) {
            pIdx = starIdx + 1;
            sIdx = ++matchIdx;
        } else {
            return false;
        }
    }
    
    while (pIdx < pattern.size() && pattern[pIdx] == '*') {
        ++pIdx;
    }
    
    return pIdx == pattern.size();
}

#ifdef _WIN32
std::wstring StringUtils::toWideString(const std::string& str) {
    if (str.empty()) {
        return L"";
    }
    
    int size = MultiByteToWideChar(
        CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0
    );
    if (size <= 0) {
        return L"";
    }
    
    std::wstring result(size, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], size
    );
    return result;
}

std::string StringUtils::fromWideString(const std::wstring& wstr) {
    if (wstr.empty()) {
        return "";
    }
    
    int size = WideCharToMultiByte(
        CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), nullptr, 0, nullptr, nullptr
    );
    if (size <= 0) {
        return "";
    }
    
    std::string result(size, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &result[0], size, nullptr, nullptr
    );
    return result;
}
#else
std::wstring StringUtils::toWideString(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

std::string StringUtils::fromWideString(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}
#endif

}
}
