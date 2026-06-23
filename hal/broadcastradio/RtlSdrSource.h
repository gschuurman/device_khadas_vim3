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

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

struct rtlsdr_dev;
typedef struct rtlsdr_dev rtlsdr_dev_t;

namespace rtlsdr {

// Thin wrapper around librtlsdr that owns the dongle handle and an async RX thread.
// IQ buffers are delivered to the supplied callback on the RX thread.
class RtlSdrSource {
  public:
    using DataCallback = std::function<void(const uint8_t* iq, size_t nbytes)>;

    RtlSdrSource() = default;
    ~RtlSdrSource();

    // Open device index 0 and set the sample rate (Hz). Auto gain by default.
    bool open(uint32_t sampleRate);
    void close();
    bool isOpen() const { return mDev != nullptr; }

    // Retune the center frequency (Hz). Safe to call while streaming.
    bool setFrequency(uint32_t freqHz);
    uint32_t frequency() const { return mFreqHz; }

    // Gain control. Normal listening uses auto gain; seek uses a fixed mid-band
    // gain so the in-channel RSSI is comparable across frequencies (auto gain
    // would normalise it away).
    void setAutoGain();
    void setFixedGainForSeek();
    // Manual tuner gain in tenths of a dB; snaps to the nearest supported step.
    // A negative value re-enables tuner AGC. Returns the gain actually applied
    // (tenths of dB), or INT_MIN if AGC was selected / no device.
    int setManualGainTenthDb(int tenthDb);

    // Start/stop the async RX. start() spawns a thread running rtlsdr_read_async.
    bool start(DataCallback cb);
    void stop();
    bool isStreaming() const { return mStreaming; }

    // Fired (on the RX thread) if rtlsdr_read_async returns without a deliberate
    // stop() — i.e. the dongle was unplugged or the USB link died. Lets the upper
    // layers report antenna-lost and recover on the next tune. Must be lightweight
    // and must NOT join the RX thread (it is the RX thread).
    using UnplugCallback = std::function<void()>;
    void setUnplugCallback(UnplugCallback cb) { mUnplugCb = std::move(cb); }

  private:
    static void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx);

    rtlsdr_dev_t* mDev = nullptr;
    uint32_t mFreqHz = 0;
    DataCallback mCallback;
    std::thread mRxThread;  // populated while streaming
    std::atomic<bool> mStreaming{false};
    std::atomic<bool> mStopReq{false};  // set by stop() so the RX thread knows the exit was intentional
    UnplugCallback mUnplugCb;
};

}  // namespace rtlsdr
