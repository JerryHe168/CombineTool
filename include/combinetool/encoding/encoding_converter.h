#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "combinetool/types.h"

namespace combinetool {
namespace encoding {

struct ConversionResult {
    bool success;
    std::string output;
    size_t bytesConverted;
    size_t bytesLost;
    std::string errorMessage;
};

class EncodingConverter {
public:
    static ConversionResult convert(
        const std::string& input,
        Encoding sourceEncoding,
        Encoding targetEncoding,
        bool strict = false
    );
    
    static ConversionResult convertBuffer(
        const std::vector<uint8_t>& input,
        Encoding sourceEncoding,
        Encoding targetEncoding,
        bool strict = false
    );
    
    static std::string toUTF8(
        const std::string& input,
        Encoding sourceEncoding,
        bool strict = false
    );
    
    static std::string fromUTF8(
        const std::string& input,
        Encoding targetEncoding,
        bool strict = false
    );
    
    static bool canConvert(Encoding from, Encoding to);
    
    static std::vector<uint8_t> removeBOM(const std::vector<uint8_t>& buffer, Encoding encoding);
    static std::vector<uint8_t> addBOM(const std::vector<uint8_t>& buffer, Encoding encoding);

private:
    static std::u32string decodeToUTF32(
        const std::string& input,
        Encoding sourceEncoding,
        bool strict,
        size_t& bytesConverted,
        size_t& bytesLost
    );
    
    static std::string encodeFromUTF32(
        const std::u32string& input,
        Encoding targetEncoding,
        bool strict,
        size_t& charactersLost
    );
    
    static char32_t decodeUTF8Character(
        const uint8_t* buffer,
        size_t length,
        size_t& bytesUsed,
        bool strict,
        bool& valid
    );
    
    static char32_t decodeUTF16LECharacter(
        const uint8_t* buffer,
        size_t length,
        size_t& bytesUsed,
        bool strict,
        bool& valid
    );
    
    static char32_t decodeUTF16BECharacter(
        const uint8_t* buffer,
        size_t length,
        size_t& bytesUsed,
        bool strict,
        bool& valid
    );
    
    static char32_t decodeGBKCharacter(
        const uint8_t* buffer,
        size_t length,
        size_t& bytesUsed,
        bool strict,
        bool& valid
    );
    
    static size_t encodeUTF8Character(
        char32_t codepoint,
        uint8_t* buffer,
        size_t bufferSize
    );
    
    static size_t encodeUTF16LECharacter(
        char32_t codepoint,
        uint8_t* buffer,
        size_t bufferSize
    );
    
    static size_t encodeUTF16BECharacter(
        char32_t codepoint,
        uint8_t* buffer,
        size_t bufferSize
    );
    
    static size_t encodeGBKCharacter(
        char32_t codepoint,
        uint8_t* buffer,
        size_t bufferSize
    );
    
    static bool isGBKCodepoint(char32_t codepoint);
    static char32_t gbkToUnicode(uint16_t gbkCode);
    static uint16_t unicodeToGBK(char32_t codepoint);
};

}
}
