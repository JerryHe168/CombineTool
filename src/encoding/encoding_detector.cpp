#include "combinetool/encoding/encoding_detector.h"
#include "combinetool/utils/file_utils.h"
#include "combinetool/config.h"

#include <algorithm>
#include <cmath>

namespace combinetool {
namespace encoding {

EncodingDetectionResult EncodingDetector::detectFromFile(const std::string& filePath) {
    auto buffer = utils::FileUtils::readAllBytes(filePath);
    if (buffer.empty()) {
        return { Encoding::Unknown, 0.0f, false, "Unknown" };
    }
    return detectFromBuffer(buffer);
}

EncodingDetectionResult EncodingDetector::detectFromBuffer(const std::vector<uint8_t>& buffer) {
    EncodingDetectionResult result;
    result.encoding = Encoding::Unknown;
    result.confidence = 0.0f;
    result.hasBOM = false;
    result.encodingName = "Unknown";
    
    if (buffer.empty()) {
        return result;
    }
    
    Encoding bomEncoding;
    result.hasBOM = checkBOM(buffer, bomEncoding);
    
    if (result.hasBOM) {
        result.encoding = bomEncoding;
        result.confidence = 0.95f;
        result.encodingName = encodingToString(bomEncoding);
        return result;
    }
    
    std::vector<std::pair<Encoding, float>> scores;
    
    if (isValidUTF8(buffer)) {
        float confidence = calculateUTF8Confidence(buffer);
        if (confidence > 0.0f) {
            scores.emplace_back(Encoding::UTF8, confidence);
        }
    }
    
    if (buffer.size() >= 2) {
        bool looksLikeUTF16LE = true;
        bool looksLikeUTF16BE = true;
        
        for (size_t i = 0; i < buffer.size() - 1; i += 2) {
            uint16_t wordLE = buffer[i] | (buffer[i + 1] << 8);
            uint16_t wordBE = (buffer[i] << 8) | buffer[i + 1];
            
            if (wordLE == 0 && (i == 0 || buffer[i - 1] != 0)) {
                looksLikeUTF16LE = false;
            }
            if (wordBE == 0 && (i == 0 || buffer[i] != 0)) {
                looksLikeUTF16BE = false;
            }
        }
        
        if (looksLikeUTF16LE && (buffer.size() % 2 == 0)) {
            scores.emplace_back(Encoding::UTF16_LE, 0.5f);
        }
        if (looksLikeUTF16BE && (buffer.size() % 2 == 0)) {
            scores.emplace_back(Encoding::UTF16_BE, 0.5f);
        }
    }
    
    float gbkConfidence = calculateGBKConfidence(buffer);
    if (gbkConfidence > 0.0f) {
        scores.emplace_back(Encoding::GBK, gbkConfidence);
    }
    
    if (isLikelyASCII(buffer)) {
        scores.emplace_back(Encoding::ASCII, 0.9f);
    }
    
    if (!scores.empty()) {
        std::sort(scores.begin(), scores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        result.encoding = scores[0].first;
        result.confidence = scores[0].second;
        result.encodingName = encodingToString(scores[0].first);
    } else {
        result.encoding = Encoding::ISO8859_1;
        result.confidence = 0.3f;
        result.encodingName = "ISO-8859-1";
    }
    
    return result;
}

EncodingDetectionResult EncodingDetector::detectFromString(const std::string& text) {
    std::vector<uint8_t> buffer(text.begin(), text.end());
    return detectFromBuffer(buffer);
}

std::string EncodingDetector::encodingToString(Encoding encoding) {
    switch (encoding) {
        case Encoding::UTF8:        return "UTF-8";
        case Encoding::UTF8_BOM:    return "UTF-8-BOM";
        case Encoding::UTF16_LE:    return "UTF-16-LE";
        case Encoding::UTF16_BE:    return "UTF-16-BE";
        case Encoding::UTF32_LE:    return "UTF-32-LE";
        case Encoding::UTF32_BE:    return "UTF-32-BE";
        case Encoding::GBK:         return "GBK";
        case Encoding::GB2312:      return "GB2312";
        case Encoding::GB18030:     return "GB18030";
        case Encoding::BIG5:        return "Big5";
        case Encoding::ISO8859_1:   return "ISO-8859-1";
        case Encoding::ASCII:       return "ASCII";
        case Encoding::Unknown:
        default:                    return "Unknown";
    }
}

Encoding EncodingDetector::stringToEncoding(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "utf-8" || lower == "utf8") return Encoding::UTF8;
    if (lower == "utf-8-bom" || lower == "utf8-bom") return Encoding::UTF8_BOM;
    if (lower == "utf-16-le" || lower == "utf16le") return Encoding::UTF16_LE;
    if (lower == "utf-16-be" || lower == "utf16be") return Encoding::UTF16_BE;
    if (lower == "utf-32-le" || lower == "utf32le") return Encoding::UTF32_LE;
    if (lower == "utf-32-be" || lower == "utf32be") return Encoding::UTF32_BE;
    if (lower == "gbk") return Encoding::GBK;
    if (lower == "gb2312") return Encoding::GB2312;
    if (lower == "gb18030") return Encoding::GB18030;
    if (lower == "big5") return Encoding::BIG5;
    if (lower == "iso-8859-1" || lower == "latin1") return Encoding::ISO8859_1;
    if (lower == "ascii") return Encoding::ASCII;
    
    return Encoding::Unknown;
}

bool EncodingDetector::hasBOM(const std::vector<uint8_t>& buffer) {
    Encoding dummy;
    return checkBOM(buffer, dummy);
}

std::vector<uint8_t> EncodingDetector::getBOMForEncoding(Encoding encoding) {
    switch (encoding) {
        case Encoding::UTF8_BOM:
            return { 0xEF, 0xBB, 0xBF };
        case Encoding::UTF16_LE:
            return { 0xFF, 0xFE };
        case Encoding::UTF16_BE:
            return { 0xFE, 0xFF };
        case Encoding::UTF32_LE:
            return { 0xFF, 0xFE, 0x00, 0x00 };
        case Encoding::UTF32_BE:
            return { 0x00, 0x00, 0xFE, 0xFF };
        default:
            return {};
    }
}

bool EncodingDetector::isValidUTF8(const std::vector<uint8_t>& buffer) {
    size_t i = 0;
    while (i < buffer.size()) {
        size_t seqLen = 0;
        if (!isValidUTF8Sequence(&buffer[i], seqLen)) {
            return false;
        }
        if (i + seqLen > buffer.size()) {
            return false;
        }
        i += seqLen;
    }
    return true;
}

bool EncodingDetector::isValidUTF16LE(const std::vector<uint8_t>& buffer) {
    if (buffer.size() % 2 != 0) {
        return false;
    }
    
    for (size_t i = 0; i < buffer.size(); i += 2) {
        uint16_t word = buffer[i] | (buffer[i + 1] << 8);
        
        if (word >= 0xD800 && word <= 0xDBFF) {
            if (i + 2 >= buffer.size()) {
                return false;
            }
            uint16_t nextWord = buffer[i + 2] | (buffer[i + 3] << 8);
            if (nextWord < 0xDC00 || nextWord > 0xDFFF) {
                return false;
            }
            i += 2;
        } else if (word >= 0xDC00 && word <= 0xDFFF) {
            return false;
        }
    }
    
    return true;
}

bool EncodingDetector::isValidUTF16BE(const std::vector<uint8_t>& buffer) {
    if (buffer.size() % 2 != 0) {
        return false;
    }
    
    for (size_t i = 0; i < buffer.size(); i += 2) {
        uint16_t word = (buffer[i] << 8) | buffer[i + 1];
        
        if (word >= 0xD800 && word <= 0xDBFF) {
            if (i + 2 >= buffer.size()) {
                return false;
            }
            uint16_t nextWord = (buffer[i + 2] << 8) | buffer[i + 3];
            if (nextWord < 0xDC00 || nextWord > 0xDFFF) {
                return false;
            }
            i += 2;
        } else if (word >= 0xDC00 && word <= 0xDFFF) {
            return false;
        }
    }
    
    return true;
}

bool EncodingDetector::isLikelyGBK(const std::vector<uint8_t>& buffer) {
    float confidence = calculateGBKConfidence(buffer);
    return confidence > 0.5f;
}

bool EncodingDetector::isLikelyASCII(const std::vector<uint8_t>& buffer) {
    for (uint8_t b : buffer) {
        if (b >= 0x80) {
            return false;
        }
        if (b < 0x09 || (b > 0x0D && b < 0x20)) {
            if (b != 0x08 && b != 0x0C) {
                return false;
            }
        }
    }
    return true;
}

bool EncodingDetector::isLikelyISO8859_1(const std::vector<uint8_t>& buffer) {
    for (uint8_t b : buffer) {
        if (b < 0x09 || (b > 0x0D && b < 0x20)) {
            if (b != 0x08 && b != 0x0C) {
                return false;
            }
        }
    }
    return true;
}

float EncodingDetector::calculateUTF8Confidence(const std::vector<uint8_t>& buffer) {
    size_t validSequences = 0;
    size_t totalSequences = 0;
    size_t multiByteChars = 0;
    size_t controlChars = 0;
    
    size_t i = 0;
    while (i < buffer.size()) {
        uint8_t b = buffer[i];
        
        if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D && b != 0x08 && b != 0x0C) {
            controlChars++;
        }
        
        if (b < 0x80) {
            validSequences++;
            totalSequences++;
            i++;
        } else {
            size_t seqLen = 0;
            if (isValidUTF8Sequence(&buffer[i], seqLen)) {
                if (i + seqLen <= buffer.size()) {
                    validSequences++;
                    multiByteChars++;
                }
            }
            totalSequences++;
            i += (seqLen > 0) ? seqLen : 1;
        }
    }
    
    if (totalSequences == 0) {
        return 0.0f;
    }
    
    float validRatio = static_cast<float>(validSequences) / totalSequences;
    float controlRatio = static_cast<float>(controlChars) / buffer.size();
    
    float confidence = validRatio;
    
    if (multiByteChars > 0) {
        float multiByteRatio = static_cast<float>(multiByteChars) / totalSequences;
        confidence = 0.8f * validRatio + 0.2f * multiByteRatio;
    }
    
    if (controlRatio > 0.05f) {
        confidence *= (1.0f - controlRatio);
    }
    
    return std::max(0.0f, std::min(1.0f, confidence));
}

float EncodingDetector::calculateGBKConfidence(const std::vector<uint8_t>& buffer) {
    size_t gbkPairs = 0;
    size_t highBytes = 0;
    size_t controlChars = 0;
    
    size_t i = 0;
    while (i < buffer.size()) {
        uint8_t b = buffer[i];
        
        if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D && b != 0x08 && b != 0x0C) {
            controlChars++;
        }
        
        if (isGBKFirstByte(b)) {
            highBytes++;
            if (i + 1 < buffer.size()) {
                uint8_t second = buffer[i + 1];
                if (isGBKSecondByte(second)) {
                    gbkPairs++;
                }
            }
            i += 2;
        } else if (b < 0x80) {
            i++;
        } else {
            i++;
        }
    }
    
    if (highBytes == 0) {
        return 0.0f;
    }
    
    float pairRatio = static_cast<float>(gbkPairs) / highBytes;
    float controlRatio = static_cast<float>(controlChars) / buffer.size();
    
    float confidence = pairRatio;
    
    if (controlRatio > 0.05f) {
        confidence *= (1.0f - controlRatio);
    }
    
    return std::max(0.0f, std::min(1.0f, confidence));
}

bool EncodingDetector::checkBOM(const std::vector<uint8_t>& buffer, Encoding& outEncoding) {
    outEncoding = Encoding::Unknown;
    
    if (buffer.size() >= 4) {
        if (buffer[0] == 0x00 && buffer[1] == 0x00 && 
            buffer[2] == 0xFE && buffer[3] == 0xFF) {
            outEncoding = Encoding::UTF32_BE;
            return true;
        }
        if (buffer[0] == 0xFF && buffer[1] == 0xFE && 
            buffer[2] == 0x00 && buffer[3] == 0x00) {
            outEncoding = Encoding::UTF32_LE;
            return true;
        }
    }
    
    if (buffer.size() >= 3) {
        if (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
            outEncoding = Encoding::UTF8_BOM;
            return true;
        }
    }
    
    if (buffer.size() >= 2) {
        if (buffer[0] == 0xFE && buffer[1] == 0xFF) {
            outEncoding = Encoding::UTF16_BE;
            return true;
        }
        if (buffer[0] == 0xFF && buffer[1] == 0xFE) {
            outEncoding = Encoding::UTF16_LE;
            return true;
        }
    }
    
    return false;
}

bool EncodingDetector::isValidUTF8Sequence(const uint8_t* bytes, size_t& sequenceLength) {
    if (bytes == nullptr) {
        return false;
    }
    
    uint8_t first = bytes[0];
    
    if ((first & 0x80) == 0) {
        sequenceLength = 1;
        return true;
    }
    
    if ((first & 0xE0) == 0xC0) {
        sequenceLength = 2;
        if ((bytes[1] & 0xC0) != 0x80) {
            return false;
        }
        uint32_t codepoint = ((first & 0x1F) << 6) | (bytes[1] & 0x3F);
        if (codepoint < 0x80) {
            return false;
        }
        return true;
    }
    
    if ((first & 0xF0) == 0xE0) {
        sequenceLength = 3;
        if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) {
            return false;
        }
        uint32_t codepoint = ((first & 0x0F) << 12) | 
                              ((bytes[1] & 0x3F) << 6) | 
                              (bytes[2] & 0x3F);
        if (codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return false;
        }
        return true;
    }
    
    if ((first & 0xF8) == 0xF0) {
        sequenceLength = 4;
        if ((bytes[1] & 0xC0) != 0x80 || 
            (bytes[2] & 0xC0) != 0x80 || 
            (bytes[3] & 0xC0) != 0x80) {
            return false;
        }
        uint32_t codepoint = ((first & 0x07) << 18) | 
                              ((bytes[1] & 0x3F) << 12) | 
                              ((bytes[2] & 0x3F) << 6) | 
                              (bytes[3] & 0x3F);
        if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
            return false;
        }
        return true;
    }
    
    return false;
}

bool EncodingDetector::isGBKFirstByte(uint8_t b) {
    return (b >= 0x81 && b <= 0xFE);
}

bool EncodingDetector::isGBKSecondByte(uint8_t b) {
    return (b >= 0x40 && b <= 0x7E) || (b >= 0x80 && b <= 0xFE);
}

}
}
