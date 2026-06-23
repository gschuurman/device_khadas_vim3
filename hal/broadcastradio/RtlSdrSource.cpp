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

#define LOG_TAG "BcRadioRtlSdr.Source"

#include "RtlSdrSource.h"

#include <climits>
#include <cstdlib>

#include <android-base/logging.h>
#include <rtl-sdr.h>

namespace rtlsdr {

RtlSdrSource::~RtlSdrSource() {
    stop();
    close();
}

bool RtlSdrSource::open(uint32_t sampleRate) {
    if (mDev != nullptr) return true;

    if (rtlsdr_get_device_count() == 0) {
        LOG(ERROR) << "no RTL-SDR device found";
        return false;
    }
    if (rtlsdr_open(&mDev, 0) != 0 || mDev == nullptr) {
        LOG(ERROR) << "rtlsdr_open failed";
        mDev = nullptr;
        return false;
    }
    if (rtlsdr_set_sample_rate(mDev, sampleRate) != 0) {
        LOG(ERROR) << "rtlsdr_set_sample_rate(" << sampleRate << ") failed";
        close();
        return false;
    }
    // Automatic gain: tuner AGC on, RTL2832 digital AGC off.
    setAutoGain();
    LOG(INFO) << "opened RTL-SDR at " << sampleRate << " S/s";
    return true;
}

void RtlSdrSource::setAutoGain() {
    if (mDev == nullptr) return;
    rtlsdr_set_tuner_gain_mode(mDev, 0);  // 0 = tuner AGC
    rtlsdr_set_agc_mode(mDev, 0);         // RTL2832 digital AGC off
}

void RtlSdrSource::setFixedGainForSeek() {
    if (mDev == nullptr) return;
    // Pick a stable mid-band manual gain so RSSI is comparable across frequencies.
    // Query the supported gain table and choose the middle entry.
    int gains[64];
    const int n = rtlsdr_get_tuner_gains(mDev, gains);
    rtlsdr_set_tuner_gain_mode(mDev, 1);  // manual
    if (n > 0) {
        rtlsdr_set_tuner_gain(mDev, gains[n / 2]);
    }
}

int RtlSdrSource::setManualGainTenthDb(int tenthDb) {
    if (mDev == nullptr) return INT_MIN;
    if (tenthDb < 0) {
        rtlsdr_set_tuner_gain_mode(mDev, 0);  // back to tuner AGC
        rtlsdr_set_agc_mode(mDev, 0);
        return INT_MIN;
    }
    int gains[64];
    const int n = rtlsdr_get_tuner_gains(mDev, gains);
    rtlsdr_set_tuner_gain_mode(mDev, 1);  // manual
    rtlsdr_set_agc_mode(mDev, 0);         // RTL2832 digital AGC off
    if (n <= 0) return INT_MIN;
    int best = gains[0];
    for (int i = 1; i < n; ++i) {
        if (std::abs(gains[i] - tenthDb) < std::abs(best - tenthDb)) best = gains[i];
    }
    rtlsdr_set_tuner_gain(mDev, best);
    LOG(INFO) << "manual tuner gain set to " << best << " (0.1 dB), requested " << tenthDb;
    return best;
}

void RtlSdrSource::close() {
    if (mDev != nullptr) {
        rtlsdr_close(mDev);
        mDev = nullptr;
    }
}

bool RtlSdrSource::setFrequency(uint32_t freqHz) {
    if (mDev == nullptr) return false;
    if (rtlsdr_set_center_freq(mDev, freqHz) != 0) {
        LOG(ERROR) << "rtlsdr_set_center_freq(" << freqHz << ") failed";
        return false;
    }
    mFreqHz = freqHz;
    LOG(INFO) << "tuned to " << freqHz << " Hz";
    return true;
}

// static
void RtlSdrSource::rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx) {
    auto* self = static_cast<RtlSdrSource*>(ctx);
    if (self != nullptr && self->mCallback && len > 0) {
        self->mCallback(buf, len);
    }
}

bool RtlSdrSource::start(DataCallback cb) {
    if (mDev == nullptr) return false;
    if (mStreaming) return true;

    mCallback = std::move(cb);
    rtlsdr_reset_buffer(mDev);
    mStopReq = false;
    mStreaming = true;
    mRxThread = std::thread([this]() {
        // Blocks until rtlsdr_cancel_async() is called from stop().
        // Small transfers (16 KiB = ~7.1 ms at 1.152 MS/s) instead of the 256 KiB
        // (~114 ms) default: the default delivers IQ in bursts far larger than the
        // ~85 ms loopback buffer, which underran the capture and made audio choppy.
        // Smaller buffers also let seek sample the RSSI quickly. buf_len must be a
        // multiple of 512; 32 buffers keeps the USB pipeline full.
        rtlsdr_read_async(mDev, &RtlSdrSource::rtlsdrCallback, this, /*buf_num=*/32,
                          /*buf_len=*/16384);
        mStreaming = false;
        // If we got here without stop() asking, the device went away (unplug / USB
        // reset). Notify so the HAL can report antenna-lost; recovery happens on the
        // next tune (which stop()/close()/open()s a fresh handle).
        if (!mStopReq.load() && mUnplugCb) {
            LOG(ERROR) << "RTL-SDR async RX ended unexpectedly (dongle unplugged?)";
            mUnplugCb();
        }
    });
    return true;
}

void RtlSdrSource::stop() {
    if (!mStreaming && !mRxThread.joinable()) return;
    mStopReq = true;
    if (mDev != nullptr) {
        rtlsdr_cancel_async(mDev);
    }
    if (mRxThread.joinable()) {
        mRxThread.join();
    }
    mStreaming = false;
    mCallback = nullptr;
}

}  // namespace rtlsdr
