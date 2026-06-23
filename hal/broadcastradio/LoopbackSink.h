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
#include <cstdint>
#include <string>

struct pcm;

namespace rtlsdr {

// Writes decoded 48 kHz S16 stereo PCM into the snd-aloop "Loopback" card playback side
// (device 0). The audio HAL captures the mirrored stream from Loopback device 1 as the
// AUDIO_DEVICE_IN_FM_TUNER source and patches it to bus0_media_out.
class LoopbackSink {
  public:
    static constexpr int kRate = 48000;
    static constexpr int kChannels = 2;

    LoopbackSink() = default;
    ~LoopbackSink();

    bool open();
    void close();
    bool isOpen() const { return mPcm != nullptr; }

    // Write interleaved L/R S16 frames. Returns false on a fatal PCM error.
    bool write(const int16_t* frames, size_t frameCount);

  private:
    // Resolve the ALSA card index of the snd-aloop "Loopback" card by name.
    static int findLoopbackCard();

    struct pcm* mPcm = nullptr;
};

}  // namespace rtlsdr
