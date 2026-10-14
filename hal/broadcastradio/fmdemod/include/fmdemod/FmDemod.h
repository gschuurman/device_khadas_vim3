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

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "fmdemod/Rds.h"

namespace fmdemod {

// Decimating FIR filter with persistent state across process() chunks.
class FirDecimator {
  public:
    void init(std::vector<float> taps, int decimation);
    // Push one input sample. If a decimated output is produced, writes it to *out
    // and returns true; otherwise returns false.
    bool push(float x, float* out);
    void reset();

  private:
    std::vector<float> mTaps;  // reversed
    std::vector<float> mRing;  // delay line, every sample twice (size 2 * taps)
    int mDecim = 1;
    int mWidx = 0;
    int mPhase = 0;
};

// Second-order IIR section (RBJ biquad), direct form I.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    // Constant 0 dB peak gain band-pass around f0 (normalised to the sample rate).
    void bandpass(float f0Norm, float q);
    float process(float x);
    void reset() { x1 = x2 = y1 = y2 = 0; }
};

// Wideband broadcast-FM demodulator: RTL-SDR raw uint8 I/Q -> 48 kHz S16 stereo PCM + RDS.
//
// Signal chain:
//   capture 1.152 MS/s complex  --(LPF, /6)-->  192 kHz complex
//   polar discriminator (FM)     ------------>  192 kHz multiplex (MPX)
//   MPX: 19 kHz pilot PLL (phase phi)
//     L+R: LPF 15 kHz (/4)                           -> 48 kHz
//     L-R: MPX * 2 sin(2 phi), same LPF (/4)         -> 48 kHz, blended in when the pilot is locked
//     L = S + D, R = S - D, 50 us de-emphasis (EU) each
//   RDS: MPX * (cos, sin)(3 phi) -> LPF, /16 -> 12 kHz complex -> BPSK phase from <z^2>
//     bit clock from the pilot (16 pilot cycles per bit) -> biphase + differential decoding
//     -> RdsDecoder
class FmDemod {
  public:
    // RTL-SDR capture rate this demod expects (must be set on the tuner).
    static constexpr int kInputRate = 1152000;
    static constexpr int kOutputRate = 48000;
    static constexpr int kMpxRate = 192000;

    FmDemod();

    // Feed raw RTL-SDR bytes (interleaved uint8 I, Q). Appends 48 kHz S16 stereo
    // frames (interleaved L, R) to *out.
    void process(const uint8_t* iq, size_t nbytes, std::vector<int16_t>* out);
    void reset();

    // Mean in-channel power (mean |z|^2 of the post-stage-1, ~90 kHz-wide complex
    // signal) accumulated since the last call; resets the accumulator. Used by seek
    // as an RSSI proxy: with a fixed tuner gain this rises sharply on a real station.
    double takeChannelPower();

    // True while the 19 kHz stereo pilot is locked (stereo output).
    bool stereo() const { return mStereo.load(); }
    // Current RDS data, and a counter that changes whenever it does. Thread-safe.
    RdsInfo rds() const;
    uint32_t rdsGeneration() const { return mRdsGeneration.load(); }

    // Feeds 192 kHz MPX samples (after the discriminator); used by process() and the host
    // tests. Appends 48 kHz S16 stereo frames to *out.
    void processMpx(float mpx, std::vector<int16_t>* out);

  private:
    void processPilot(float mpx);
    void processRds(float mpx);
    void rdsSample(float i, float q);
    FirDecimator mFirI;  // complex stage 1 (in-phase)
    FirDecimator mFirQ;  // complex stage 1 (quadrature)
    FirDecimator mFirA;  // audio stage 2 (L+R)
    FirDecimator mFirD;  // audio stage 2 (L-R), same taps as mFirA

    float mPrevI = 0.f;
    float mPrevQ = 0.f;
    float mDeemphL = 0.f;
    float mDeemphR = 0.f;
    float mDeemphAlpha;
    float mAudioGain;

    // Pilot PLL: phase of the 19 kHz pilot (tracks sin(phi)), in radians [0, 2 pi).
    Biquad mPilotBp;
    double mPhase = 0.0;
    double mFreq = 0.0;        // loop frequency correction, rad/sample
    float mPilotI = 0.f;       // LPF of bp(mpx) * sin(phi): pilot amplitude / 2 when locked
    float mPilotPow = 0.f;     // LPF of bp(mpx)^2
    float mPilotAmp = 0.f;     // amplitude estimate for loop gain normalisation
    std::atomic<bool> mStereo{false};  // read from other threads
    float mBlend = 0.f;        // 0 = mono .. 1 = full stereo
    uint64_t mPilotCycles = 0; // completed pilot cycles (RDS bit clock)
    float mSin2 = 0.f, mSin3 = 0.f, mCos3 = 0.f;

    // RDS.
    FirDecimator mRdsI1, mRdsQ1;  // 192 kHz -> 48 kHz
    FirDecimator mRdsI2, mRdsQ2;  // 48 kHz -> 12 kHz
    float mRdsZ2Re = 0.f, mRdsZ2Im = 0.f;  // <z^2> for the BPSK phase
    uint64_t mChipCycle = 0;       // pilot cycle the current chip belongs to
    float mChipAcc = 0.f;
    float mChips[32] = {};         // last 32 chips (one per pilot cycle), ring
    uint32_t mChipCount = 0;
    float mAlignEnergy[16] = {};   // biphase metric energy per bit alignment
    int mLastBit = 0;              // previous biphase bit (differential decoding)
    RdsDecoder mRdsDecoder;
    mutable std::mutex mRdsLock;
    RdsInfo mRdsInfo;              // published copy, under mRdsLock
    std::atomic<uint32_t> mRdsGeneration{0};

    double mPowAccum = 0.0;  // sum of in-channel |z|^2 over stage-1 outputs
    uint64_t mPowCount = 0;
};

}  // namespace fmdemod
