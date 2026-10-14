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

#include "fmdemod/FmDemod.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fmdemod {

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Windowed-sinc (Hamming) low-pass FIR. fcNorm = cutoff / sampleRate (cycles/sample).
std::vector<float> designLowpass(int numTaps, float fcNorm) {
    std::vector<float> h(numTaps);
    const float m = (numTaps - 1) / 2.0f;
    float sum = 0.f;
    for (int k = 0; k < numTaps; ++k) {
        const float n = k - m;
        float sinc;
        if (std::fabs(n) < 1e-6f) {
            sinc = 2.f * fcNorm;
        } else {
            sinc = std::sin(2.f * kPi * fcNorm * n) / (kPi * n);
        }
        const float window = 0.54f - 0.46f * std::cos(2.f * kPi * k / (numTaps - 1));
        h[k] = sinc * window;
        sum += h[k];
    }
    // Normalize to unity DC gain.
    for (float& v : h) v /= sum;
    return h;
}

constexpr double kTwoPi = 6.283185307179586;
constexpr float kPilotHz = 19000.f;
// Pilot amplitude (rad/sample at 192 kHz) below which there is no usable pilot. The nominal
// pilot is 9% of 75 kHz deviation = 6.75 kHz -> 2 pi * 6750 / 192000 = 0.22.
constexpr float kMinPilotAmp = 0.05f;
// Pilot lock ratio (in-phase / total, 1 = clean lock) to switch stereo on / off.
constexpr float kLockOn = 0.8f;
constexpr float kLockOff = 0.6f;

inline int16_t clamp16(float v) {
    if (v > 32767.f) return 32767;
    if (v < -32768.f) return -32768;
    return static_cast<int16_t>(std::lround(v));
}
}  // namespace

void FirDecimator::init(std::vector<float> taps, int decimation) {
    // Taps reversed, so the convolution is a forward dot product over the delay line.
    mTaps.assign(taps.rbegin(), taps.rend());
    mDecim = decimation;
    mRing.assign(2 * mTaps.size(), 0.f);
    mWidx = 0;
    mPhase = 0;
}

void FirDecimator::reset() {
    std::fill(mRing.begin(), mRing.end(), 0.f);
    mWidx = 0;
    mPhase = 0;
}

bool FirDecimator::push(float x, float* out) {
    // Every sample is stored twice (n apart), so the last n samples are always contiguous at
    // mRing[mWidx + 1 .. mWidx + n] (oldest first) and the dot product vectorises.
    const int n = static_cast<int>(mTaps.size());
    mRing[mWidx] = x;
    mRing[mWidx + n] = x;
    mWidx = mWidx + 1 == n ? 0 : mWidx + 1;
    if (++mPhase < mDecim) return false;
    mPhase = 0;

    const float* window = mRing.data() + mWidx;
    const float* taps = mTaps.data();
    float acc = 0.f;
    for (int k = 0; k < n; ++k) acc += taps[k] * window[k];
    *out = acc;
    return true;
}

void Biquad::bandpass(float f0Norm, float q) {
    const float w0 = 2.f * kPi * f0Norm;
    const float alpha = std::sin(w0) / (2.f * q);
    const float a0 = 1.f + alpha;
    b0 = alpha / a0;
    b1 = 0.f;
    b2 = -alpha / a0;
    a1 = -2.f * std::cos(w0) / a0;
    a2 = (1.f - alpha) / a0;
    reset();
}

float Biquad::process(float x) {
    const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

FmDemod::FmDemod() {
    // Stage 1: 1.152 MHz -> 192 kHz (decimate by 6), cut 90 kHz. 49 taps: 33 (softer) cost
    // ~3 dB of stereo separation, a wider or sharper channel lets in more noise and loses RDS
    // early (host test sweep, tests/fmdemod_test.cpp).
    mFirI.init(designLowpass(49, 90000.0f / kInputRate), 6);
    mFirQ.init(designLowpass(49, 90000.0f / kInputRate), 6);
    // Stage 2: 192 kHz -> 48 kHz (decimate by 4). Cut 15 kHz (audio, kills the 19 kHz pilot).
    // L-R uses the same taps, so both paths have the same delay.
    mFirA.init(designLowpass(65, 15000.0f / kMpxRate), 4);
    mFirD.init(designLowpass(65, 15000.0f / kMpxRate), 4);

    // Pilot: band-pass (~1.9 kHz wide) in front of the PLL.
    mPilotBp.bandpass(kPilotHz / kMpxRate, 10.f);

    // RDS (57 kHz, +/- 2.4 kHz): 192 -> 48 kHz with a short filter (nothing may alias onto
    // 0..2.4 kHz), then 48 -> 12 kHz with the real channel filter.
    mRdsI1.init(designLowpass(31, 6000.0f / kMpxRate), 4);
    mRdsQ1.init(designLowpass(31, 6000.0f / kMpxRate), 4);
    mRdsI2.init(designLowpass(97, 2800.0f / 48000.0f), 4);
    mRdsQ2.init(designLowpass(97, 2800.0f / 48000.0f), 4);

    // 50 us de-emphasis (EU/Australia) one-pole IIR at 48 kHz.
    const float tau = 50e-6f;
    mDeemphAlpha = 1.f - std::exp(-1.f / (kOutputRate * tau));

    // atan2 yields radians; scale so typical ~75 kHz deviation maps near full scale.
    mAudioGain = 6000.0f;
}

void FmDemod::reset() {
    mFirI.reset();
    mFirQ.reset();
    mFirA.reset();
    mFirD.reset();
    mPrevI = mPrevQ = 0.f;
    mDeemphL = mDeemphR = 0.f;
    mPowAccum = 0.0;
    mPowCount = 0;

    mPilotBp.reset();
    mPhase = mFreq = 0.0;
    mPilotI = mPilotPow = mPilotAmp = 0.f;
    mStereo.store(false);
    mBlend = 0.f;
    mPilotCycles = 0;
    mSin2 = mSin3 = mCos3 = 0.f;

    mRdsI1.reset();
    mRdsQ1.reset();
    mRdsI2.reset();
    mRdsQ2.reset();
    mRdsZ2Re = mRdsZ2Im = 0.f;
    mChipCycle = 0;
    mChipAcc = 0.f;
    std::fill(std::begin(mChips), std::end(mChips), 0.f);
    mChipCount = 0;
    std::fill(std::begin(mAlignEnergy), std::end(mAlignEnergy), 0.f);
    mLastBit = 0;
    mRdsDecoder.reset();
    {
        std::lock_guard<std::mutex> lk(mRdsLock);
        mRdsInfo = RdsInfo();
    }
    ++mRdsGeneration;
}

RdsInfo FmDemod::rds() const {
    std::lock_guard<std::mutex> lk(mRdsLock);
    return mRdsInfo;
}

void FmDemod::processPilot(float mpx) {
    const float bp = mPilotBp.process(mpx);
    const float s = std::sin(static_cast<float>(mPhase));
    const float c = std::cos(static_cast<float>(mPhase));
    // Harmonics for the L-R (38 kHz) and RDS (57 kHz) carriers.
    mSin2 = 2.f * s * c;
    const float cos2 = c * c - s * s;
    mSin3 = mSin2 * c + cos2 * s;
    mCos3 = cos2 * c - mSin2 * s;

    // Lock detector and amplitude: ~50 ms one-pole averages.
    constexpr float kAvg = 1.f / (kMpxRate * 0.05f);
    mPilotI += kAvg * (bp * s - mPilotI);
    mPilotPow += kAvg * (bp * bp - mPilotPow);
    mPilotAmp = std::sqrt(2.f * std::max(mPilotPow, 0.f));
    const float ratio = mPilotAmp > 1e-6f ? 2.f * mPilotI / mPilotAmp : 0.f;
    const bool stereo = mStereo.load(std::memory_order_relaxed);
    if (!stereo && ratio > kLockOn && mPilotAmp > kMinPilotAmp) {
        mStereo.store(true);
    } else if (stereo && (ratio < kLockOff || mPilotAmp < 0.7f * kMinPilotAmp)) {
        mStereo.store(false);
    }

    // PLL: phase detector bp * cos(phi) ~ A/2 sin(theta - phi), normalised by A/2; PI loop,
    // ~20 Hz bandwidth, zeta 0.707.
    constexpr double kOmega0 = kTwoPi * kPilotHz / kMpxRate;
    constexpr double kWn = 2.0 * kTwoPi * 20.0 / 1.06 / kMpxRate;
    constexpr double kKp = 2.0 * 0.707 * kWn;
    constexpr double kKi = kWn * kWn;
    constexpr double kMaxFreq = kTwoPi * 50.0 / kMpxRate;  // +/- 50 Hz pull range
    double err = mPilotAmp > 1e-4f ? (bp * c) / (0.5f * mPilotAmp) : 0.0;
    err = std::clamp(err, -1.0, 1.0);
    mFreq = std::clamp(mFreq + kKi * err, -kMaxFreq, kMaxFreq);
    mPhase += kOmega0 + mFreq + kKp * err;
    while (mPhase >= kTwoPi) {
        mPhase -= kTwoPi;
        ++mPilotCycles;
    }
    while (mPhase < 0) mPhase += kTwoPi;
}

void FmDemod::processRds(float mpx) {
    float i1, q1, i2, q2;
    const bool produced = mRdsI1.push(mpx * mCos3, &i1);
    mRdsQ1.push(mpx * mSin3, &q1);
    if (!produced) return;
    const bool produced2 = mRdsI2.push(i1, &i2);
    mRdsQ2.push(q1, &q2);
    if (produced2) rdsSample(i2, q2);
}

void FmDemod::rdsSample(float i, float q) {
    // BPSK: the carrier phase relative to 3 phi is fixed but unknown (in phase or quadrature
    // with the pilot harmonic, plus filter delays). <z^2> points at twice that phase.
    constexpr float kZ2Avg = 1.f / (12000.f * 0.5f);
    mRdsZ2Re += kZ2Avg * ((i * i - q * q) - mRdsZ2Re);
    mRdsZ2Im += kZ2Avg * ((2.f * i * q) - mRdsZ2Im);
    const float angle = 0.5f * std::atan2(mRdsZ2Im, mRdsZ2Re);
    const float y = i * std::cos(angle) + q * std::sin(angle);

    // Chips: one per pilot cycle (the RDS clock is locked to the pilot: 16 cycles per bit).
    // The constant filter delay is absorbed by the alignment search below.
    while (mChipCycle < mPilotCycles) {
        mChips[mChipCount % 32] = mChipAcc;
        mChipAcc = 0.f;
        const uint32_t n = mChipCount++;
        ++mChipCycle;
        if (n < 16) continue;
        // Biphase bit ending at chip n: first half minus second half.
        float m = 0.f;
        for (uint32_t k = n - 15; k <= n - 8; ++k) m += mChips[k % 32];
        for (uint32_t k = n - 7; k <= n; ++k) m -= mChips[k % 32];
        const int align = n % 16;
        mAlignEnergy[align] = 0.99f * mAlignEnergy[align] + 0.01f * m * m;
        int best = 0;
        for (int a = 1; a < 16; ++a) {
            if (mAlignEnergy[a] > mAlignEnergy[best]) best = a;
        }
        if (align != best) continue;
        const int bit = m > 0.f ? 1 : 0;
        const int data = bit ^ mLastBit;  // differential decoding
        mLastBit = bit;
        if (mRdsDecoder.pushBit(data)) {
            std::lock_guard<std::mutex> lk(mRdsLock);
            mRdsInfo = mRdsDecoder.info();
            ++mRdsGeneration;
        }
    }
    // A pilot that jumps backwards (reset) restarts the chip count.
    if (mChipCycle > mPilotCycles) mChipCycle = mPilotCycles;
    mChipAcc += y;
}

void FmDemod::processMpx(float mpx, std::vector<int16_t>* out) {
    processPilot(mpx);
    processRds(mpx);

    float sum, diff;
    const bool produced = mFirA.push(mpx, &sum);
    mFirD.push(mpx * 2.f * mSin2, &diff);
    if (!produced) return;

    // Slide between mono and stereo (~50 ms) instead of switching hard.
    constexpr float kBlendRate = 1.f / (kOutputRate * 0.05f);
    mBlend += kBlendRate * ((mStereo.load(std::memory_order_relaxed) ? 1.f : 0.f) - mBlend);
    const float l = sum + mBlend * diff;
    const float r = sum - mBlend * diff;
    mDeemphL += mDeemphAlpha * (l - mDeemphL);
    mDeemphR += mDeemphAlpha * (r - mDeemphR);
    out->push_back(clamp16(mDeemphL * mAudioGain));
    out->push_back(clamp16(mDeemphR * mAudioGain));
}

double FmDemod::takeChannelPower() {
    const double p = mPowCount ? mPowAccum / static_cast<double>(mPowCount) : 0.0;
    mPowAccum = 0.0;
    mPowCount = 0;
    return p;
}

void FmDemod::process(const uint8_t* iq, size_t nbytes, std::vector<int16_t>* out) {
    const size_t nsamp = nbytes / 2;
    for (size_t i = 0; i < nsamp; ++i) {
        const float I = (static_cast<float>(iq[2 * i]) - 127.5f) * (1.f / 127.5f);
        const float Q = (static_cast<float>(iq[2 * i + 1]) - 127.5f) * (1.f / 127.5f);

        float dI, dQ;
        const bool produced = mFirI.push(I, &dI);
        mFirQ.push(Q, &dQ);  // same decimation phase as mFirI
        if (!produced) continue;

        // In-channel power (RSSI proxy for seek).
        mPowAccum += static_cast<double>(dI) * dI + static_cast<double>(dQ) * dQ;
        ++mPowCount;

        // Polar discriminator: arg(z[n] * conj(z[n-1])).
        const float demod = std::atan2(dQ * mPrevI - dI * mPrevQ, dI * mPrevI + dQ * mPrevQ);
        mPrevI = dI;
        mPrevQ = dQ;

        processMpx(demod, out);
    }
}

}  // namespace fmdemod
