#pragma once

#include <algorithm>
#include <string_view>
#include <vector>

class Utf8Mapping {
  struct PosConv {
    uint32_t charCol;
    uint32_t byteCol;

    uint32_t offset() const { return byteCol - charCol; }
  };

  struct LineMapping {
    uint32_t lineNo = 0;
    std::vector<PosConv> posConvs;
  };

 public:
  Utf8Mapping(std::string_view text) { buildMapping(text); }

  void buildMapping(std::string_view text) {
    lines.clear();
    LineMapping curLine;
    PosConv curPosConv = {0, 0};
    uint32_t lineNo = 0;

    for (size_t i = 0; i < text.size();) {
      if (text[i] == '\n') {
        if (!curLine.posConvs.empty()) {
          lines.push_back(std::move(curLine));
        }
        lineNo++;
        curLine = LineMapping{lineNo, {}};
        curPosConv = {0, 0};
        i++;
        continue;
      }

      const uint32_t seqLen = charSeqLength(text[i]);
      curPosConv.charCol++;
      curPosConv.byteCol += seqLen;
      i += seqLen;

      if (curPosConv.byteCol == curPosConv.charCol) continue;
      if (curLine.posConvs.empty()) {
        curLine.posConvs.push_back(curPosConv);
        continue;
      }
      const auto lastPosConv = curLine.posConvs.back();
      if (lastPosConv.offset() != curPosConv.offset()) {
        curLine.posConvs.push_back(curPosConv);
      }
    }

    if (!curLine.posConvs.empty()) {
      lines.push_back(curLine);
    }
  }

  uint32_t charToByte(uint32_t lineIndex, uint32_t charCol) const {
    const auto* posConvsPtr = findLine(lineIndex);
    if (posConvsPtr == nullptr || posConvsPtr->empty()) return charCol;

    auto posConvIt =
        std::upper_bound(posConvsPtr->begin(), posConvsPtr->end(), charCol,
                         [](uint32_t charCol, const PosConv& posConv) {
                           return charCol < posConv.charCol;
                         });
    if (posConvIt == posConvsPtr->begin()) return charCol;
    posConvIt = std::prev(posConvIt);
    return posConvIt->offset() + charCol;
  }

  uint32_t byteToChar(uint32_t lineIndex, uint32_t byteCol) const {
    const auto* posConvsPtr = findLine(lineIndex);
    if (posConvsPtr == nullptr || posConvsPtr->empty()) return byteCol;

    auto posConvIt =
        std::lower_bound(posConvsPtr->begin(), posConvsPtr->end(), byteCol,
                         [](const PosConv& posConv, uint32_t byteCol) {
                           return posConv.byteCol < byteCol;
                         });

    if (posConvIt == posConvsPtr->end()) {
      return byteCol - posConvsPtr->back().offset();
    }
    if (posConvIt->byteCol == byteCol) return posConvIt->charCol;

    const uint32_t prevOffset = (posConvIt == posConvsPtr->begin())
                                    ? 0
                                    : std::prev(posConvIt)->offset();
    const uint32_t curSeqLen = posConvIt->offset() - prevOffset;
    const uint32_t startByte = posConvIt->byteCol - curSeqLen;

    if (startByte <= byteCol) return posConvIt->charCol - 1;
    return byteCol - prevOffset;
  }

  void print(std::ostream& os) const;

 private:
  std::vector<LineMapping> lines;

  const decltype(LineMapping::posConvs)* findLine(uint32_t lineIndex) const {
    if (lines.empty()) return nullptr;
    auto it = std::lower_bound(lines.begin(), lines.end(), lineIndex,
                               [](const LineMapping& line, uint32_t lineIndex) {
                                 return line.lineNo < lineIndex;
                               });
    if (it == lines.end() || it->lineNo != lineIndex) return nullptr;
    return &it->posConvs;
  }

  uint32_t charSeqLength(unsigned char startByte) {
    uint32_t seqLen = 1;
    if ((startByte & 0x80U) != 0) {
      if ((startByte & 0xE0U) == 0xC0U) {
        seqLen = 2;
      } else if ((startByte & 0xF0U) == 0xE0U) {
        seqLen = 3;
      } else if ((startByte & 0xF8U) == 0xF0U) {
        seqLen = 4;
      }
    }
    return seqLen;
  }
};
