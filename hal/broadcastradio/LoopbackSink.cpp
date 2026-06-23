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

#define LOG_TAG "BcRadioRtlSdr.Sink"

#include "LoopbackSink.h"

#include <fstream>
#include <regex>

#include <android-base/logging.h>
#include <tinyalsa/asoundlib.h>

namespace rtlsdr {

// static
int LoopbackSink::findLoopbackCard() {
    std::ifstream cards("/proc/asound/cards");
    if (!cards.is_open()) return -1;
    // Lines look like: " 7 [Loopback       ]: Loopback - Loopback"
    std::regex re(R"(^\s*(\d+)\s+\[Loopback)");
    std::string line;
    while (std::getline(cards, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            return std::stoi(m[1].str());
        }
    }
    return -1;
}

LoopbackSink::~LoopbackSink() {
    close();
}

bool LoopbackSink::open() {
    if (mPcm != nullptr) return true;

    const int card = findLoopbackCard();
    if (card < 0) {
        LOG(ERROR) << "snd-aloop Loopback card not found in /proc/asound/cards";
        return false;
    }

    struct pcm_config config = {};
    config.channels = kChannels;
    config.rate = kRate;
    config.format = PCM_FORMAT_S16_LE;
    // 1024 frames x 8 = ~170 ms of buffer (was 4 = ~85 ms). Deeper buffer absorbs
    // the writer-thread bursts so the snd-aloop capture side doesn't underrun.
    config.period_size = 1024;
    config.period_count = 8;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    // Play into Loopback device 0 -> audio HAL captures from device 1.
    mPcm = pcm_open(static_cast<unsigned int>(card), /*device=*/0, PCM_OUT, &config);
    if (mPcm == nullptr || !pcm_is_ready(mPcm)) {
        LOG(ERROR) << "pcm_open(Loopback card " << card << ",0) failed: "
                   << (mPcm ? pcm_get_error(mPcm) : "null");
        close();
        return false;
    }
    LOG(INFO) << "Loopback sink open on card " << card << " device 0";
    return true;
}

void LoopbackSink::close() {
    if (mPcm != nullptr) {
        pcm_close(mPcm);
        mPcm = nullptr;
    }
}

bool LoopbackSink::write(const int16_t* frames, size_t frameCount) {
    if (mPcm == nullptr || frameCount == 0) return false;
    const unsigned int bytes = static_cast<unsigned int>(frameCount * kChannels * sizeof(int16_t));
    const int ret = pcm_write(mPcm, frames, bytes);
    if (ret != 0) {
        LOG(WARNING) << "pcm_write failed: " << pcm_get_error(mPcm);
        return false;
    }
    return true;
}

}  // namespace rtlsdr
