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

#include "fmdemod/Rds.h"

namespace fmdemod {

namespace {
constexpr uint32_t kPoly = 0x5B9;   // x^10+x^8+x^7+x^5+x^4+x^3+1
constexpr int kMaxErrorsInRow = 6;  // blocks with a bad syndrome before resync

// Block of 26 bits -> offset index (0..4) whose syndrome matches, or -1.
int offsetOf(uint32_t block26) {
    const uint16_t data = static_cast<uint16_t>(block26 >> 10);
    const uint16_t check = static_cast<uint16_t>(block26 & 0x3FF);
    const uint16_t offset = check ^ RdsDecoder::checkword(data);
    for (int i = 0; i < 5; ++i) {
        if (RdsDecoder::kOffset[i] == offset) return i;
    }
    return -1;
}

// Block position (A=0, B=1, C/C'=2, D=3) of an offset index.
int positionOf(int offset) {
    return offset <= 2 ? offset : (offset == 3 ? 2 : 3);
}

// RDS characters 0x80..0x9F (EBU Latin, IEC 62106 Annex E), as UTF-8.
const char* const kHigh[32] = {
        "á", "à", "é", "è", "í", "ì", "ó", "ò", "ú", "ù", "Ñ", "Ç", "Ş", "ß", "¡", "Ĳ",
        "â", "ä", "ê", "ë", "î", "ï", "ô", "ö", "û", "ü", "ñ", "ç", "ş", "ğ", "ı", "ĳ"};
}  // namespace

constexpr uint16_t RdsDecoder::kOffset[5];

// static
uint16_t RdsDecoder::checkword(uint16_t data) {
    uint32_t reg = static_cast<uint32_t>(data) << 10;
    for (int bit = 25; bit >= 10; --bit) {
        if (reg & (1u << bit)) reg ^= kPoly << (bit - 10);
    }
    return static_cast<uint16_t>(reg & 0x3FF);
}

void RdsDecoder::reset() {
    *this = RdsDecoder();
}

bool RdsDecoder::pushBit(int bit) {
    mShift = ((mShift << 1) | (bit & 1)) & 0x3FFFFFF;
    if (mBitsInShift < 26) ++mBitsInShift;

    if (!mSynced) {
        if (mBitsInShift < 26) return false;
        // Sync: a block with a valid syndrome, followed 26 bits later by the next block in
        // sequence (a random 26-bit pattern passes a syndrome check about 1 in 200 times).
        ++mSearchBits;
        const int offset = offsetOf(mShift);
        if (offset >= 0) {
            const int pos = positionOf(offset);
            if (mCandidatePos >= 0 && mSearchBits == 26 && pos == (mCandidatePos + 1) % 4) {
                mSynced = true;
                mErrorsInRow = 0;
                mBlockOk.fill(false);
                mBlocks[mCandidatePos] = mCandidateData;
                mBlockOk[mCandidatePos] = true;
                mBlocks[pos] = static_cast<uint16_t>(mShift >> 10);
                mBlockOk[pos] = true;
                if (pos == 2) mBlockCPrime = offset == 3;
                mGoodBlocks += 2;
                bool changed = false;
                if (pos == 3) {
                    changed = processGroup();
                    mBlockOk.fill(false);
                }
                mBlockIndex = (pos + 1) % 4;
                mBitCount = 0;
                mCandidatePos = -1;
                return changed;
            }
            mCandidatePos = pos;
            mCandidateData = static_cast<uint16_t>(mShift >> 10);
            mSearchBits = 0;
        } else if (mSearchBits > 26) {
            mCandidatePos = -1;
        }
        return false;
    }

    if (++mBitCount < 26) return false;
    mBitCount = 0;
    const int offset = offsetOf(mShift);
    const int expected = mBlockIndex;
    bool ok = offset >= 0 && positionOf(offset) == expected;
    if (ok) {
        mBlocks[expected] = static_cast<uint16_t>(mShift >> 10);
        mBlockOk[expected] = true;
        if (expected == 2) mBlockCPrime = offset == 3;
        mErrorsInRow = 0;
        ++mGoodBlocks;
    } else {
        mBlockOk[expected] = false;
        ++mBadBlocks;
        if (++mErrorsInRow >= kMaxErrorsInRow) {
            mSynced = false;
            mBitsInShift = 0;
            mCandidatePos = -1;
            return false;
        }
    }
    bool changed = false;
    if (expected == 3) {
        changed = processGroup();
        mBlockOk.fill(false);
    }
    mBlockIndex = (expected + 1) % 4;
    return changed;
}

bool RdsDecoder::processGroup() {
    // PI is in block A (and in C' of version B groups).
    const RdsInfo before = mInfo;
    if (!mBlockOk[0] && !mBlockOk[1]) return false;
    if (mBlockOk[0]) {
        mInfo.pi = mBlocks[0];
        mInfo.valid = true;
    } else if (mBlockOk[2] && mBlockCPrime) {
        mInfo.pi = mBlocks[2];
        mInfo.valid = true;
    }
    if (!mBlockOk[1]) return false;
    const uint16_t b = mBlocks[1];
    const int groupType = b >> 12;
    const bool versionB = (b >> 11) & 1;
    mInfo.valid = true;
    mInfo.tp = (b >> 10) & 1;
    mInfo.pty = (b >> 5) & 0x1F;

    if (groupType == 0) {
        mInfo.ta = (b >> 4) & 1;
        const int seg = b & 0x3;
        if (mBlockOk[3]) {
            const uint8_t c0 = mBlocks[3] >> 8, c1 = mBlocks[3] & 0xFF;
            if (mPsCandidate[2 * seg] == c0 && mPsCandidate[2 * seg + 1] == c1) {
                mPs[2 * seg] = c0;
                mPs[2 * seg + 1] = c1;
                mPsSegments |= 1 << seg;
            }
            mPsCandidate[2 * seg] = c0;
            mPsCandidate[2 * seg + 1] = c1;
        }
        if (mPsSegments == 0xF) {
            std::string ps;
            for (uint8_t c : mPs) appendChar(c, &ps);
            while (!ps.empty() && ps.back() == ' ') ps.pop_back();
            mInfo.ps = ps;
        }
    } else if (groupType == 2) {
        const int ab = (b >> 4) & 1;
        if (ab != mRtAb) {
            // Text A/B flag toggled: a new RadioText.
            mRtAb = ab;
            mRt.fill(' ');
            mRtSegments = 0;
            mRtLength = versionB ? 32 : 64;
        }
        const int seg = b & 0xF;
        if (!versionB && mBlockOk[2] && mBlockOk[3]) {
            const uint8_t chars[4] = {static_cast<uint8_t>(mBlocks[2] >> 8),
                                      static_cast<uint8_t>(mBlocks[2] & 0xFF),
                                      static_cast<uint8_t>(mBlocks[3] >> 8),
                                      static_cast<uint8_t>(mBlocks[3] & 0xFF)};
            for (int i = 0; i < 4; ++i) {
                mRt[4 * seg + i] = chars[i];
                if (chars[i] == 0x0D) mRtLength = 4 * seg + i;  // end of text
            }
            mRtSegments |= 1 << seg;
        } else if (versionB && mBlockOk[3]) {
            const uint8_t chars[2] = {static_cast<uint8_t>(mBlocks[3] >> 8),
                                      static_cast<uint8_t>(mBlocks[3] & 0xFF)};
            for (int i = 0; i < 2; ++i) {
                mRt[2 * seg + i] = chars[i];
                if (chars[i] == 0x0D) mRtLength = 2 * seg + i;
            }
            mRtSegments |= 1 << seg;
        }
        // Publish once every segment up to the end of the text is in.
        const int perSeg = versionB ? 2 : 4;
        const int segs = (mRtLength + perSeg - 1) / perSeg;
        const uint16_t need = segs >= 16 ? 0xFFFF : static_cast<uint16_t>((1u << segs) - 1);
        if (segs > 0 && (mRtSegments & need) == need) {
            std::string rt;
            for (int i = 0; i < mRtLength; ++i) appendChar(mRt[i], &rt);
            while (!rt.empty() && rt.back() == ' ') rt.pop_back();
            mInfo.rt = rt;
        }
    }
    return mInfo.valid != before.valid || mInfo.pi != before.pi || mInfo.pty != before.pty ||
           mInfo.tp != before.tp || mInfo.ta != before.ta || mInfo.ps != before.ps ||
           mInfo.rt != before.rt;
}

// static
void RdsDecoder::appendChar(uint8_t c, std::string* out) {
    if (c >= 0x20 && c < 0x7F) {
        // ASCII. (The RDS table has ¤ ― ║ ¯ at 0x24/0x5E/0x60/0x7E, but stations send ASCII.)
        out->push_back(static_cast<char>(c));
    } else if (c >= 0x80 && c < 0xA0) {
        out->append(kHigh[c - 0x80]);
    } else if (c == 0x0D || c == 0x0A || c == 0x00) {
        // End of text / line break / padding: nothing.
    } else {
        out->push_back(' ');
    }
}

}  // namespace fmdemod
