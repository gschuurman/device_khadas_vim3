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

// Host test of FmDemod against a synthetic broadcast: stereo multiplex (L 1 kHz, R 3 kHz, 19 kHz
// pilot, L-R on 38 kHz) with RDS (0A PS + 2A RadioText groups, checkwords, differential +
// biphase coding, 57 kHz clock locked to the pilot), FM-modulated at 75 kHz deviation and
// quantised to RTL-SDR style uint8 I/Q with noise.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "fmdemod/FmDemod.h"

using fmdemod::FmDemod;
using fmdemod::RdsDecoder;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kFs = FmDemod::kInputRate;
constexpr double kPilot = 19000.0;
constexpr double kBitRate = kPilot / 16.0;  // 1187.5

struct Signal {
    bool stereo = true;
    bool pilot = true;
    bool rds = true;
    bool rdsQuadrature = false;  // RDS carrier cos(3wt) instead of sin(3wt)
    double seconds = 6.0;
    double noise = 1.5;          // uint8 LSB rms per I/Q component
    double rdsLevel = 0.045;     // fraction of full deviation
    std::string ps = "VIM3 FM ";
    std::string rt = "Hello from the VIM3 head unit\r";
    uint16_t pi = 0x8201;
    uint8_t pty = 10;
};

// RDS bit stream: alternating 0A (PS) and 2A (RadioText) groups.
std::vector<int> rdsBits(const Signal& s, int groups) {
    std::vector<int> bits;
    auto block = [&](uint16_t data, int offset) {
        const uint16_t cw = RdsDecoder::checkword(data) ^ RdsDecoder::kOffset[offset];
        for (int b = 15; b >= 0; --b) bits.push_back((data >> b) & 1);
        for (int b = 9; b >= 0; --b) bits.push_back((cw >> b) & 1);
    };
    std::string rt = s.rt;
    while (rt.size() % 4) rt.push_back(' ');
    const int rtSegs = static_cast<int>(rt.size() / 4);
    int psSeg = 0, rtSeg = 0;
    for (int g = 0; g < groups; ++g) {
        const uint16_t common = (1 << 10) | (s.pty << 5);  // TP = 1
        if (g % 2 == 0) {
            const uint16_t b = (0 << 12) | common | (1 << 3) | psSeg;  // 0A, MS = music
            block(s.pi, 0);
            block(b, 1);
            block(0xE0CD, 2);  // AF codes (ignored)
            block(static_cast<uint16_t>((static_cast<uint8_t>(s.ps[2 * psSeg]) << 8) |
                                        static_cast<uint8_t>(s.ps[2 * psSeg + 1])), 4);
            psSeg = (psSeg + 1) % 4;
        } else {
            const uint16_t b = (2 << 12) | common | rtSeg;  // 2A, text A/B = 0
            block(s.pi, 0);
            block(b, 1);
            const auto c = [&](int i) { return static_cast<uint8_t>(rt[4 * rtSeg + i]); };
            block(static_cast<uint16_t>((c(0) << 8) | c(1)), 2);
            block(static_cast<uint16_t>((c(2) << 8) | c(3)), 4);
            rtSeg = (rtSeg + 1) % rtSegs;
        }
    }
    return bits;
}

struct Result {
    std::vector<int16_t> pcm;
    bool stereo = false;
    fmdemod::RdsInfo rds;
};

Result run(const Signal& s) {
    const size_t n = static_cast<size_t>(s.seconds * kFs);
    const std::vector<int> bits = rdsBits(s, static_cast<int>(s.seconds * kBitRate / 104) + 2);
    // Differential encoding, then biphase symbols: e = 1 -> (+, -), e = 0 -> (-, +).
    std::vector<int> enc(bits.size());
    int prev = 0;
    for (size_t i = 0; i < bits.size(); ++i) prev = enc[i] = bits[i] ^ prev;

    std::mt19937 rng(1234);
    std::normal_distribution<double> gauss(0.0, s.noise);
    const double bitOffset = 0.37;  // arbitrary bit-clock phase (in bits)
    double phase = 0.0, shaped = 0.0;
    std::vector<uint8_t> iq(2 * n);
    for (size_t k = 0; k < n; ++k) {
        const double t = k / kFs;
        const double wt = 2 * kPi * kPilot * t;
        const double l = std::sin(2 * kPi * 1000.0 * t);
        const double r = std::sin(2 * kPi * 3000.0 * t);
        double mpx = 0.0;
        if (s.stereo) {
            mpx += 0.45 * ((l + r) / 2) + 0.45 * ((l - r) / 2) * std::sin(2 * wt);
        } else {
            mpx += 0.45 * (l + r) / 2;
        }
        if (s.pilot) mpx += 0.09 * std::sin(wt);
        if (s.rds) {
            const double bitPos = t * kBitRate + bitOffset;
            const size_t bi = static_cast<size_t>(bitPos);
            const int e = enc[bi % enc.size()];
            const bool firstHalf = (bitPos - bi) < 0.5;
            const double sym = (e ? 1.0 : -1.0) * (firstHalf ? 1.0 : -1.0);
            shaped += (sym - shaped) * 0.02;  // gentle band limiting
            mpx += s.rdsLevel * shaped * (s.rdsQuadrature ? std::cos(3 * wt) : std::sin(3 * wt));
        }
        phase += 2 * kPi * 75000.0 * mpx / kFs;
        if (phase > kPi) phase -= 2 * kPi;
        if (phase < -kPi) phase += 2 * kPi;
        const double i = 127.5 + 90.0 * std::cos(phase) + gauss(rng);
        const double q = 127.5 + 90.0 * std::sin(phase) + gauss(rng);
        iq[2 * k] = static_cast<uint8_t>(std::clamp(std::lround(i), 0L, 255L));
        iq[2 * k + 1] = static_cast<uint8_t>(std::clamp(std::lround(q), 0L, 255L));
    }
    FmDemod demod;
    Result res;
    // Feed in RTL-SDR sized chunks.
    for (size_t off = 0; off < iq.size(); off += 32768) {
        demod.process(iq.data() + off, std::min<size_t>(32768, iq.size() - off), &res.pcm);
    }
    res.stereo = demod.stereo();
    res.rds = demod.rds();
    return res;
}

// Goertzel power of one channel (0 = L, 1 = R) at f over the last 'seconds'.
double tone(const std::vector<int16_t>& pcm, int ch, double f, double seconds) {
    const size_t frames = pcm.size() / 2;
    const size_t count = static_cast<size_t>(seconds * FmDemod::kOutputRate);
    const size_t start = frames - count;
    const double w = 2 * kPi * f / FmDemod::kOutputRate;
    double s1 = 0, s2 = 0;
    for (size_t i = start; i < frames; ++i) {
        const double s0 = pcm[2 * i + ch] + 2 * std::cos(w) * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - 2 * std::cos(w) * s1 * s2;
}

int gFailures = 0;
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++gFailures;
}

double db(double a, double b) {
    return 10 * std::log10(a / std::max(b, 1e-30));
}

}  // namespace

int main() {
    {
        const Result r = run(Signal());
        const double sepL = db(tone(r.pcm, 0, 1000, 2), tone(r.pcm, 0, 3000, 2));
        const double sepR = db(tone(r.pcm, 1, 3000, 2), tone(r.pcm, 1, 1000, 2));
        std::printf("  separation L %.1f dB, R %.1f dB\n", sepL, sepR);
        check(r.stereo, "stereo pilot locked");
        check(sepL > 25 && sepR > 25, "stereo separation > 25 dB");
        std::printf("  RDS: valid=%d PI=%04X PTY=%d TP=%d PS='%s' RT='%s'\n", r.rds.valid,
                    r.rds.pi, r.rds.pty, r.rds.tp, r.rds.ps.c_str(), r.rds.rt.c_str());
        check(r.rds.pi == 0x8201 && r.rds.pty == 10 && r.rds.tp, "RDS PI, PTY, TP");
        check(r.rds.ps == "VIM3 FM", "RDS PS");
        check(r.rds.rt == "Hello from the VIM3 head unit", "RDS RadioText");
    }
    {
        Signal s;
        s.rdsQuadrature = true;
        const Result r = run(s);
        check(r.rds.ps == "VIM3 FM" && r.rds.rt == "Hello from the VIM3 head unit",
              "RDS with the carrier in quadrature to the pilot harmonic");
    }
    {
        Signal s;
        s.noise = 12.0;  // much weaker signal
        const Result r = run(s);
        std::printf("  noisy: stereo=%d PS='%s' RT='%s'\n", r.stereo, r.rds.ps.c_str(),
                    r.rds.rt.c_str());
        check(r.rds.ps == "VIM3 FM", "RDS PS at high noise");
    }
    {
        Signal s;
        s.stereo = false;
        s.pilot = false;
        s.rds = false;
        const Result r = run(s);
        double maxDiff = 0;
        for (size_t i = r.pcm.size() / 2; i + 1 < r.pcm.size(); i += 2) {
            maxDiff = std::max(maxDiff, std::fabs(static_cast<double>(r.pcm[i]) - r.pcm[i + 1]));
        }
        check(!r.stereo && maxDiff == 0, "mono broadcast: stereo off, L == R");
        check(!r.rds.valid, "no RDS on a station without RDS");
    }
    std::printf("%s\n", gFailures ? "FAILED" : "ALL PASSED");
    return gFailures ? 1 : 0;
}
