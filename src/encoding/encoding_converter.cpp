#include "combinetool/encoding/encoding_converter.h"
#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/utils/string_utils.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace combinetool {
namespace encoding {

ConversionResult EncodingConverter::convert(
    const std::string& input,
    Encoding sourceEncoding,
    Encoding targetEncoding,
    bool strict
) {
    ConversionResult result;
    result.success = false;
    result.bytesConverted = 0;
    result.bytesLost = 0;
    result.errorMessage = "";
    
    if (sourceEncoding == targetEncoding) {
        result.success = true;
        result.output = input;
        result.bytesConverted = input.size();
        return result;
    }
    
    if (!canConvert(sourceEncoding, targetEncoding)) {
        result.errorMessage = "Cannot convert between specified encodings";
        return result;
    }
    
    size_t bytesConverted = 0;
    size_t bytesLost = 0;
    
    std::u32string utf32 = decodeToUTF32(
        input, sourceEncoding, strict, bytesConverted, bytesLost
    );
    
    result.bytesConverted = bytesConverted;
    result.bytesLost = bytesLost;
    
    size_t charsLost = 0;
    result.output = encodeFromUTF32(utf32, targetEncoding, strict, charsLost);
    result.bytesLost += charsLost;
    
    result.success = !strict || result.bytesLost == 0;
    
    if (!result.success && result.bytesLost > 0) {
        result.errorMessage = "Lost " + std::to_string(result.bytesLost) + 
                              " characters during conversion";
    }
    
    return result;
}

ConversionResult EncodingConverter::convertBuffer(
    const std::vector<uint8_t>& input,
    Encoding sourceEncoding,
    Encoding targetEncoding,
    bool strict
) {
    std::string inputStr(reinterpret_cast<const char*>(input.data()), input.size());
    return convert(inputStr, sourceEncoding, targetEncoding, strict);
}

std::string EncodingConverter::toUTF8(
    const std::string& input,
    Encoding sourceEncoding,
    bool strict
) {
    auto result = convert(input, sourceEncoding, Encoding::UTF8, strict);
    return result.output;
}

std::string EncodingConverter::fromUTF8(
    const std::string& input,
    Encoding targetEncoding,
    bool strict
) {
    auto result = convert(input, Encoding::UTF8, targetEncoding, strict);
    return result.output;
}

bool EncodingConverter::canConvert(Encoding from, Encoding to) {
    if (from == Encoding::Unknown || to == Encoding::Unknown) {
        return false;
    }
    return true;
}

std::vector<uint8_t> EncodingConverter::removeBOM(
    const std::vector<uint8_t>& buffer,
    Encoding encoding
) {
    auto bom = EncodingDetector::getBOMForEncoding(encoding);
    if (bom.empty() || buffer.size() < bom.size()) {
        return buffer;
    }
    
    bool hasBOM = true;
    for (size_t i = 0; i < bom.size(); ++i) {
        if (buffer[i] != bom[i]) {
            hasBOM = false;
            break;
        }
    }
    
    if (hasBOM) {
        return std::vector<uint8_t>(buffer.begin() + bom.size(), buffer.end());
    }
    
    return buffer;
}

std::vector<uint8_t> EncodingConverter::addBOM(
    const std::vector<uint8_t>& buffer,
    Encoding encoding
) {
    auto bom = EncodingDetector::getBOMForEncoding(encoding);
    if (bom.empty()) {
        return buffer;
    }
    
    std::vector<uint8_t> result;
    result.reserve(bom.size() + buffer.size());
    result.insert(result.end(), bom.begin(), bom.end());
    result.insert(result.end(), buffer.begin(), buffer.end());
    return result;
}

std::u32string EncodingConverter::decodeToUTF32(
    const std::string& input,
    Encoding sourceEncoding,
    bool strict,
    size_t& bytesConverted,
    size_t& bytesLost
) {
    std::u32string result;
    bytesConverted = 0;
    bytesLost = 0;
    
    const uint8_t* buffer = reinterpret_cast<const uint8_t*>(input.data());
    size_t bufferSize = input.size();
    
    size_t i = 0;
    while (i < bufferSize) {
        size_t bytesUsed = 0;
        bool valid = true;
        char32_t codepoint = 0;
        
        switch (sourceEncoding) {
            case Encoding::UTF8:
            case Encoding::UTF8_BOM:
                codepoint = decodeUTF8Character(buffer + i, bufferSize - i, bytesUsed, strict, valid);
                break;
            case Encoding::UTF16_LE:
                codepoint = decodeUTF16LECharacter(buffer + i, bufferSize - i, bytesUsed, strict, valid);
                break;
            case Encoding::UTF16_BE:
                codepoint = decodeUTF16BECharacter(buffer + i, bufferSize - i, bytesUsed, strict, valid);
                break;
            case Encoding::GBK:
            case Encoding::GB2312:
            case Encoding::GB18030:
                codepoint = decodeGBKCharacter(buffer + i, bufferSize - i, bytesUsed, strict, valid);
                break;
            case Encoding::ASCII:
            case Encoding::ISO8859_1:
                if (buffer[i] < 0x80 || sourceEncoding == Encoding::ISO8859_1) {
                    codepoint = buffer[i];
                    bytesUsed = 1;
                    valid = true;
                } else {
                    valid = false;
                    bytesUsed = 1;
                }
                break;
            default:
                valid = false;
                bytesUsed = 1;
                break;
        }
        
        if (valid) {
            result.push_back(codepoint);
            bytesConverted += bytesUsed;
        } else {
            if (strict) {
                bytesLost += bytesUsed;
            } else {
                result.push_back(0xFFFD);
                bytesConverted += bytesUsed;
            }
        }
        
        i += bytesUsed;
    }
    
    return result;
}

std::string EncodingConverter::encodeFromUTF32(
    const std::u32string& input,
    Encoding targetEncoding,
    bool strict,
    size_t& charactersLost
) {
    std::string result;
    charactersLost = 0;
    
    std::vector<uint8_t> buffer(8);
    
    for (char32_t codepoint : input) {
        size_t bytesUsed = 0;
        
        switch (targetEncoding) {
            case Encoding::UTF8:
            case Encoding::UTF8_BOM:
                bytesUsed = encodeUTF8Character(codepoint, buffer.data(), buffer.size());
                break;
            case Encoding::UTF16_LE:
                bytesUsed = encodeUTF16LECharacter(codepoint, buffer.data(), buffer.size());
                break;
            case Encoding::UTF16_BE:
                bytesUsed = encodeUTF16BECharacter(codepoint, buffer.data(), buffer.size());
                break;
            case Encoding::GBK:
            case Encoding::GB2312:
            case Encoding::GB18030:
                bytesUsed = encodeGBKCharacter(codepoint, buffer.data(), buffer.size());
                break;
            case Encoding::ASCII:
                if (codepoint < 0x80) {
                    buffer[0] = static_cast<uint8_t>(codepoint);
                    bytesUsed = 1;
                } else {
                    bytesUsed = 0;
                }
                break;
            case Encoding::ISO8859_1:
                if (codepoint < 0x100) {
                    buffer[0] = static_cast<uint8_t>(codepoint);
                    bytesUsed = 1;
                } else {
                    bytesUsed = 0;
                }
                break;
            default:
                bytesUsed = 0;
                break;
        }
        
        if (bytesUsed > 0) {
            result.append(reinterpret_cast<char*>(buffer.data()), bytesUsed);
        } else {
            if (strict) {
                charactersLost++;
            } else {
                buffer[0] = '?';
                result.append(1, '?');
            }
        }
    }
    
    if (targetEncoding == Encoding::UTF8_BOM) {
        auto bom = EncodingDetector::getBOMForEncoding(Encoding::UTF8_BOM);
        result.insert(0, reinterpret_cast<char*>(bom.data()), bom.size());
    }
    
    return result;
}

char32_t EncodingConverter::decodeUTF8Character(
    const uint8_t* buffer,
    size_t length,
    size_t& bytesUsed,
    bool strict,
    bool& valid
) {
    valid = true;
    bytesUsed = 0;
    
    if (length == 0) {
        valid = false;
        return 0;
    }
    
    uint8_t first = buffer[0];
    
    if ((first & 0x80) == 0) {
        bytesUsed = 1;
        return first;
    }
    
    if ((first & 0xE0) == 0xC0) {
        if (length < 2) {
            valid = false;
            bytesUsed = 1;
            return 0;
        }
        if ((buffer[1] & 0xC0) != 0x80) {
            valid = !strict;
            bytesUsed = 1;
            return 0;
        }
        bytesUsed = 2;
        char32_t codepoint = ((first & 0x1F) << 6) | (buffer[1] & 0x3F);
        if (codepoint < 0x80) {
            valid = !strict;
            return strict ? 0 : codepoint;
        }
        return codepoint;
    }
    
    if ((first & 0xF0) == 0xE0) {
        if (length < 3) {
            valid = false;
            bytesUsed = 1;
            return 0;
        }
        if ((buffer[1] & 0xC0) != 0x80 || (buffer[2] & 0xC0) != 0x80) {
            valid = !strict;
            bytesUsed = 1;
            return 0;
        }
        bytesUsed = 3;
        char32_t codepoint = ((first & 0x0F) << 12) | 
                              ((buffer[1] & 0x3F) << 6) | 
                              (buffer[2] & 0x3F);
        if (codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            valid = !strict;
            return strict ? 0 : codepoint;
        }
        return codepoint;
    }
    
    if ((first & 0xF8) == 0xF0) {
        if (length < 4) {
            valid = false;
            bytesUsed = 1;
            return 0;
        }
        if ((buffer[1] & 0xC0) != 0x80 || 
            (buffer[2] & 0xC0) != 0x80 || 
            (buffer[3] & 0xC0) != 0x80) {
            valid = !strict;
            bytesUsed = 1;
            return 0;
        }
        bytesUsed = 4;
        char32_t codepoint = ((first & 0x07) << 18) | 
                              ((buffer[1] & 0x3F) << 12) | 
                              ((buffer[2] & 0x3F) << 6) | 
                              (buffer[3] & 0x3F);
        if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
            valid = !strict;
            return strict ? 0 : codepoint;
        }
        return codepoint;
    }
    
    valid = !strict;
    bytesUsed = 1;
    return 0;
}

char32_t EncodingConverter::decodeUTF16LECharacter(
    const uint8_t* buffer,
    size_t length,
    size_t& bytesUsed,
    bool strict,
    bool& valid
) {
    valid = true;
    bytesUsed = 0;
    
    if (length < 2) {
        valid = false;
        bytesUsed = length;
        return 0;
    }
    
    uint16_t word = buffer[0] | (buffer[1] << 8);
    bytesUsed = 2;
    
    if (word >= 0xD800 && word <= 0xDBFF) {
        if (length < 4) {
            valid = !strict;
            return strict ? 0 : word;
        }
        uint16_t trail = buffer[2] | (buffer[3] << 8);
        if (trail < 0xDC00 || trail > 0xDFFF) {
            valid = !strict;
            return strict ? 0 : word;
        }
        bytesUsed = 4;
        return 0x10000 + ((word - 0xD800) << 10) + (trail - 0xDC00);
    }
    
    if (word >= 0xDC00 && word <= 0xDFFF) {
        valid = !strict;
        return strict ? 0 : 0xFFFD;
    }
    
    return word;
}

char32_t EncodingConverter::decodeUTF16BECharacter(
    const uint8_t* buffer,
    size_t length,
    size_t& bytesUsed,
    bool strict,
    bool& valid
) {
    valid = true;
    bytesUsed = 0;
    
    if (length < 2) {
        valid = false;
        bytesUsed = length;
        return 0;
    }
    
    uint16_t word = (buffer[0] << 8) | buffer[1];
    bytesUsed = 2;
    
    if (word >= 0xD800 && word <= 0xDBFF) {
        if (length < 4) {
            valid = !strict;
            return strict ? 0 : word;
        }
        uint16_t trail = (buffer[2] << 8) | buffer[3];
        if (trail < 0xDC00 || trail > 0xDFFF) {
            valid = !strict;
            return strict ? 0 : word;
        }
        bytesUsed = 4;
        return 0x10000 + ((word - 0xD800) << 10) + (trail - 0xDC00);
    }
    
    if (word >= 0xDC00 && word <= 0xDFFF) {
        valid = !strict;
        return strict ? 0 : 0xFFFD;
    }
    
    return word;
}

char32_t EncodingConverter::decodeGBKCharacter(
    const uint8_t* buffer,
    size_t length,
    size_t& bytesUsed,
    bool strict,
    bool& valid
) {
    valid = true;
    bytesUsed = 0;
    
    if (length == 0) {
        valid = false;
        return 0;
    }
    
    uint8_t first = buffer[0];
    
    if (first < 0x80) {
        bytesUsed = 1;
        return first;
    }
    
    if (length < 2) {
        valid = !strict;
        bytesUsed = 1;
        return 0;
    }
    
    uint8_t second = buffer[1];
    
    if ((first >= 0x81 && first <= 0xFE) && 
        ((second >= 0x40 && second <= 0x7E) || (second >= 0x80 && second <= 0xFE))) {
        bytesUsed = 2;
        uint16_t gbkCode = (first << 8) | second;
        return gbkToUnicode(gbkCode);
    }
    
    valid = !strict;
    bytesUsed = 1;
    return 0;
}

size_t EncodingConverter::encodeUTF8Character(
    char32_t codepoint,
    uint8_t* buffer,
    size_t bufferSize
) {
    if (codepoint < 0x80) {
        if (bufferSize < 1) return 0;
        buffer[0] = static_cast<uint8_t>(codepoint);
        return 1;
    }
    
    if (codepoint < 0x800) {
        if (bufferSize < 2) return 0;
        buffer[0] = static_cast<uint8_t>(0xC0 | (codepoint >> 6));
        buffer[1] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
        return 2;
    }
    
    if (codepoint < 0x10000) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return 0;
        }
        if (bufferSize < 3) return 0;
        buffer[0] = static_cast<uint8_t>(0xE0 | (codepoint >> 12));
        buffer[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
        return 3;
    }
    
    if (codepoint <= 0x10FFFF) {
        if (bufferSize < 4) return 0;
        buffer[0] = static_cast<uint8_t>(0xF0 | (codepoint >> 18));
        buffer[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
        return 4;
    }
    
    return 0;
}

size_t EncodingConverter::encodeUTF16LECharacter(
    char32_t codepoint,
    uint8_t* buffer,
    size_t bufferSize
) {
    if (codepoint < 0x10000) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return 0;
        }
        if (bufferSize < 2) return 0;
        buffer[0] = static_cast<uint8_t>(codepoint & 0xFF);
        buffer[1] = static_cast<uint8_t>(codepoint >> 8);
        return 2;
    }
    
    if (codepoint <= 0x10FFFF) {
        if (bufferSize < 4) return 0;
        char32_t adjusted = codepoint - 0x10000;
        uint16_t lead = static_cast<uint16_t>(0xD800 + (adjusted >> 10));
        uint16_t trail = static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF));
        buffer[0] = static_cast<uint8_t>(lead & 0xFF);
        buffer[1] = static_cast<uint8_t>(lead >> 8);
        buffer[2] = static_cast<uint8_t>(trail & 0xFF);
        buffer[3] = static_cast<uint8_t>(trail >> 8);
        return 4;
    }
    
    return 0;
}

size_t EncodingConverter::encodeUTF16BECharacter(
    char32_t codepoint,
    uint8_t* buffer,
    size_t bufferSize
) {
    if (codepoint < 0x10000) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return 0;
        }
        if (bufferSize < 2) return 0;
        buffer[0] = static_cast<uint8_t>(codepoint >> 8);
        buffer[1] = static_cast<uint8_t>(codepoint & 0xFF);
        return 2;
    }
    
    if (codepoint <= 0x10FFFF) {
        if (bufferSize < 4) return 0;
        char32_t adjusted = codepoint - 0x10000;
        uint16_t lead = static_cast<uint16_t>(0xD800 + (adjusted >> 10));
        uint16_t trail = static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF));
        buffer[0] = static_cast<uint8_t>(lead >> 8);
        buffer[1] = static_cast<uint8_t>(lead & 0xFF);
        buffer[2] = static_cast<uint8_t>(trail >> 8);
        buffer[3] = static_cast<uint8_t>(trail & 0xFF);
        return 4;
    }
    
    return 0;
}

size_t EncodingConverter::encodeGBKCharacter(
    char32_t codepoint,
    uint8_t* buffer,
    size_t bufferSize
) {
    if (codepoint < 0x80) {
        if (bufferSize < 1) return 0;
        buffer[0] = static_cast<uint8_t>(codepoint);
        return 1;
    }
    
    uint16_t gbkCode = unicodeToGBK(codepoint);
    if (gbkCode == 0) {
        return 0;
    }
    
    if (bufferSize < 2) return 0;
    buffer[0] = static_cast<uint8_t>(gbkCode >> 8);
    buffer[1] = static_cast<uint8_t>(gbkCode & 0xFF);
    return 2;
}

bool EncodingConverter::isGBKCodepoint(char32_t codepoint) {
    return unicodeToGBK(codepoint) != 0;
}

namespace {
constexpr UINT CP_GBK = 936;
}

char32_t EncodingConverter::gbkToUnicode(uint16_t gbkCode) {
    if (gbkCode < 0x80) {
        return gbkCode;
    }
    
#ifdef _WIN32
    char mb[2] = { static_cast<char>(gbkCode >> 8), static_cast<char>(gbkCode & 0xFF) };
    wchar_t wc;
    int result = MultiByteToWideChar(CP_GBK, 0, mb, 2, &wc, 1);
    if (result > 0) {
        return static_cast<char32_t>(wc);
    }
#endif
    
    return 0xFFFD;
}

uint16_t EncodingConverter::unicodeToGBK(char32_t codepoint) {
    if (codepoint < 0x80) {
        return static_cast<uint16_t>(codepoint);
    }
    
#ifdef _WIN32
    wchar_t wc = static_cast<wchar_t>(codepoint);
    char mb[3] = {0};
    int result = WideCharToMultiByte(CP_GBK, 0, &wc, 1, mb, 3, nullptr, nullptr);
    if (result == 2) {
        return (static_cast<uint8_t>(mb[0]) << 8) | static_cast<uint8_t>(mb[1]);
    }
#endif
    
    return 0;
}

}
}
