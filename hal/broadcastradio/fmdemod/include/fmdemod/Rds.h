/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace fmdemod {

// What a station sends over RDS (IEC 62106). Strings are UTF-8.
struct RdsInfo {
    bool valid = false;  // at least one group decoded since the last reset
    uint16_t pi = 0;     // programme identification
    uint8_t pty = 0;     // programme type (0..31)
    bool tp = false;     // traffic programme
    bool ta = false;     // traffic announcement on air
    std::string ps;      // programme service name (8 chars, trailing spaces trimmed)
    std::string rt;      // RadioText (up to 64 chars)
};

// RDS group layer: block synchronisation on the checkword syndromes and decoding of the
// groups the car UI uses (0A/0B: PS, TA/TP, PTY; 2A/2B: RadioText; PI from every group).
// Fed one (differentially decoded) data bit at a time.
class RdsDecoder {
  public:
    void reset();
    // Push one data bit. Returns true if the published RdsInfo changed.
    bool pushBit(int bit);
    const RdsInfo& info() const { return mInfo; }
    // Groups and blocks seen since reset (for tests / signal quality).
    uint32_t goodBlocks() const { return mGoodBlocks; }
    uint32_t badBlocks() const { return mBadBlocks; }

    // Checkword of a 16-bit information word (CRC with g(x) = x^10+x^8+x^7+x^5+x^4+x^3+1),
    // without offset word. Exposed for the test signal generator.
    static uint16_t checkword(uint16_t data);
    // Offset words A, B, C, C', D.
    static constexpr uint16_t kOffset[5] = {0x0FC, 0x198, 0x168, 0x350, 0x1B4};

  private:
    bool processGroup();
    // EBU Latin-based RDS character set (Annex E) -> UTF-8, appended to 'out'.
    static void appendChar(uint8_t c, std::string* out);

    uint32_t mShift = 0;      // last 26 bits
    int mBitsInShift = 0;
    bool mSynced = false;
    int mCandidatePos = -1;   // block position of a sync candidate, -1 = none
    uint16_t mCandidateData = 0;
    int mSearchBits = 0;      // bits since the candidate
    int mBlockIndex = 0;      // expected block 0..3 (A, B, C, D) while synced
    int mBitCount = 0;        // bits into the current block while synced
    int mErrorsInRow = 0;
    std::array<uint16_t, 4> mBlocks{};
    std::array<bool, 4> mBlockOk{};
    bool mBlockCPrime = false;

    // PS assembly: 4 segments of 2 chars; a segment is taken after it was received twice
    // with the same value, to keep glitches off the screen.
    std::array<uint8_t, 8> mPsCandidate{};
    std::array<uint8_t, 8> mPs{};
    uint8_t mPsSegments = 0;  // bit per confirmed segment
    // RadioText assembly: 16 segments of 4 chars (2A) or 32 chars of 2 (2B).
    std::array<uint8_t, 64> mRt{};
    uint16_t mRtSegments = 0;  // bit per received segment (2A: 16, 2B: low 16 used)
    int mRtLength = 64;
    int mRtAb = -1;

    RdsInfo mInfo;
    uint32_t mGoodBlocks = 0;
    uint32_t mBadBlocks = 0;
};

}  // namespace fmdemod
