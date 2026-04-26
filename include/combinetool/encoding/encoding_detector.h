#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "combinetool/types.h"

namespace combinetool {
namespace encoding {

struct EncodingDetectionResult {
    Encoding encoding;
    float confidence;
    bool hasBOM;
    std::string encodingName;
};

class EncodingDetector {
public:
    static EncodingDetectionResult detectFromFile(const std::string& filePath);
    static EncodingDetectionResult detectFromBuffer(const std::vector<uint8_t>& buffer);
    static EncodingDetectionResult detectFromString(const std::string& text);
    
    static std::string encodingToString(Encoding encoding);
    static Encoding stringToEncoding(const std::string& name);
    
    static bool hasBOM(const std::vector<uint8_t>& buffer);
    static std::vector<uint8_t> getBOMForEncoding(Encoding encoding);
    
    static bool isValidUTF8(const std::vector<uint8_t>& buffer);
    static bool isValidUTF16LE(const std::vector<uint8_t>& buffer);
    static bool isValidUTF16BE(const std::vector<uint8_t>& buffer);
    
    static bool isLikelyGBK(const std::vector<uint8_t>& buffer);
    static bool isLikelyASCII(const std::vector<uint8_t>& buffer);
    static bool isLikelyISO8859_1(const std::vector<uint8_t>& buffer);

private:
    static float calculateUTF8Confidence(const std::vector<uint8_t>& buffer);
    static float calculateGBKConfidence(const std::vector<uint8_t>& buffer);
    
    static bool checkBOM(const std::vector<uint8_t>& buffer, Encoding& outEncoding);
    
    static bool isValidUTF8Sequence(const uint8_t* bytes, size_t& sequenceLength);
    
    static bool isGBKFirstByte(uint8_t b);
    static bool isGBKSecondByte(uint8_t b);
};

}
}
