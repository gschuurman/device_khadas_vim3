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
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <memory>
#include <string>

#include "LoopbackSink.h"
#include "RtlSdrSource.h"
#include "WelleDab.h"
#include "fmdemod/FmDemod.h"

namespace rtlsdr {

// Single shared owner of the one RTL-SDR dongle. Both BroadcastRadio instances (amfm, dab)
// route through here; only one band can stream at a time, so a tune from one band preempts
// the other. For now only the FM path is implemented (P2); DAB is added in P3.
class DeviceManager {
  public:
    enum class Band { kNone, kFm, kDab };

    static DeviceManager& getInstance();

    // Tune the FM band to freqHz and (re)start streaming decoded audio into the loopback.
    // Acquires the dongle for the FM band, preempting DAB if it held it. Returns true on
    // success.
    bool tuneFm(uint32_t freqHz);

    // Release the dongle (called when the owning tuner session closes / cancels audio).
    void release(Band band);

    // Signal-based FM seek: starting from the current frequency, step across the band
    // in the given direction and stop on the first frequency whose in-channel RSSI
    // exceeds the detection threshold. Returns the found frequency (Hz), or 0 if none
    // (in which case the original frequency is restored). Blocking; run off-binder.
    // Honours cancelSeek().
    uint32_t seekFm(bool directionUp, uint32_t lowerHz, uint32_t upperHz, uint32_t stepHz);
    void cancelSeek() { mSeekCancel = true; }

    bool isStereo() const { return false; }  // mono for now (P2); stereo decode is P4

    // Latest in-channel FM power (mean |IQ|^2 after channel filtering; linear RSSI
    // proxy updated on the RX thread). Used to derive ProgramInfo.signalQuality.
    double fmChannelPower() const { return mChannelPower.load(); }

    // --- DAB (P3) ----------------------------------------------------------
    // Tune the dongle to a DAB block (centre freq Hz) and start the welle-core
    // receiver. Acquires the dongle for DAB, preempting FM. Optionally runs an
    // ensemble scan. Returns true on success.
    bool tuneDab(int blockFreqHz, bool doScan);
    // Route the audio of the DAB service with this SId to the loopback.
    bool selectDabService(uint32_t sid);
    // Metadata snapshots for the DAB BroadcastRadio instance.
    std::vector<WelleDabSession::ServiceInfo> dabServices() const;
    std::string dabEnsembleLabel() const;
    std::string dabDynamicLabel() const;
    bool dabIsSynced() const;
    float dabSnr() const;
    bool dabSignalPresent() const;
    float dabIqLevel() const;
    uint32_t dabFibOk() const;
    uint32_t dabFibTotal() const;
    // Latest MOT slideshow image id for the live DAB session (0 = none), and the
    // bytes for a previously-advertised id (served back to the framework via getImage).
    uint32_t dabSlideId() const;
    bool dabGetImage(uint32_t id, std::vector<uint8_t>& out) const;
    // Set the DAB front-end gain live (tenths of a dB; <0 = tuner AGC) and make it
    // sticky: subsequent DAB tunes (other blocks, service selection) reuse it until
    // changed again. Returns the actual gain applied (snapped to a supported step).
    // Used by the scan worker's adaptive-gain search. Safe to call without mLock held.
    int setDabGainTenthDb(int tenthDb);
    // Register a callback fired (on a welle thread) when the DAB service list or
    // labels change, so the HAL can push onProgramListUpdated / metadata.
    void setDabUpdateCallback(WelleDabSession::UpdateCallback cb);

    // Register a callback fired (on the RX thread) when the RTL-SDR disappears
    // mid-stream (unplug / USB reset). Forwarded to the source. Used by the HAL to
    // report antenna-lost; the dongle is reacquired on the next tune.
    void setUnplugCallback(RtlSdrSource::UnplugCallback cb) {
        mSource.setUnplugCallback(std::move(cb));
    }

  private:
    DeviceManager() = default;

    // RX callback (runs on the librtlsdr async thread): demod FM IQ and enqueue PCM.
    void onIqData(const uint8_t* iq, size_t nbytes);
    // RX callback for DAB: hand raw IQ to the welle-core input ring.
    void onIqDataDab(const uint8_t* iq, size_t nbytes);
    // Push decoded PCM (interleaved S16 @48k) into the loopback queue.
    void enqueuePcm(const int16_t* pcm, size_t frames);
    // Ensure the dongle is open at the given sample rate, reopening if it differs.
    bool ensureSourceRate(uint32_t sampleRate);  // mLock held
    // Writer thread: drains the PCM queue into the loopback (paced by ALSA at 48 kHz),
    // decoupled from USB RX so a stalled loopback never blocks the libusb event loop.
    void writerLoop();
    void startStreamingLocked(Band band);  // mLock held
    void stopStreamingLocked();            // mLock held

    // ~2 s of 48 kHz stereo; if the loopback isn't being drained we drop oldest audio.
    static constexpr size_t kMaxQueueSamples = 48000 * 2 * 2;
    // DAB pre-roll: the welle decoder delivers audio in bursty ~120 ms superframe chunks
    // with a cold-start latency, so writing the very first chunk straight through leaves
    // the loopback playback buffer half-empty and the capture side underruns (an audible
    // glitch right after tuning). Buffer this much PCM before the writer starts draining
    // so the pipeline starts full. FM streams continuously and needs no pre-roll.
    static constexpr size_t kDabPrerollSamples = 48000 * 2 * 4 / 10;  // 0.4 s stereo
    size_t mPreroll = 0;  // guarded by mQueueLock; >0 = writer holding until buffered

    mutable std::mutex mLock;
    RtlSdrSource mSource;
    LoopbackSink mSink;
    fmdemod::FmDemod mDemod;
    std::vector<int16_t> mPcmScratch;  // demod output buffer (RX thread only)
    Band mOwner = Band::kNone;
    uint32_t mSourceRate = 0;  // current dongle sample rate (FM 1.152M / DAB 2.048M)

    std::unique_ptr<WelleDabSession> mDab;  // present while DAB owns the dongle
    int mDabFreqHz = 0;  // block freq the live DAB session is tuned to (0 = none)
    WelleDabSession::UpdateCallback mDabUpdateCb;  // forwarded to each DAB session
    // Sticky DAB front-end gain (tenths dB) learned by the adaptive-gain search.
    // <0 means "not learned yet": tuneDab falls back to the vendor.broadcastradio.dab.gain
    // property (default 340). Guarded by mLock.
    int mDabGainTenthDb = -1;

    // PCM hand-off from the RX thread to the writer thread.
    std::thread mWriterThread;
    std::mutex mQueueLock;
    std::condition_variable mQueueCv;
    std::vector<int16_t> mQueue;  // guarded by mQueueLock
    bool mWriterRunning = false;

    // Seek state. mChannelPower is the latest in-channel RSSI proxy (updated on the RX
    // thread); while mSeeking, the RX thread feeds silence into the loopback so the
    // retuning station fragments aren't played out.
    std::atomic<bool> mSeeking{false};
    std::atomic<bool> mSeekCancel{false};
    std::atomic<double> mChannelPower{0.0};
};

}  // namespace rtlsdr
