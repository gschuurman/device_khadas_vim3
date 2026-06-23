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
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// welle-core (libwellecore). Pulls in std::complex DSPCOMPLEX, the three abstract
// interfaces (InputInterface/RadioControllerInterface/ProgrammeHandlerInterface)
// and RadioReceiver. The HAL feeds IQ via WelleInput and consumes audio/metadata
// via the controller/programme handlers below.
#include "radio-controller.h"
#include "radio-receiver.h"
#include "ringbuffer.h"
#include "virtual_input.h"

namespace rtlsdr {

// IQ feed into welle-core. Stores raw 8-bit interleaved IQ in a ring buffer (filled
// from the RTL-SDR async RX thread) and converts to DSPCOMPLEX on demand for the
// OFDM processor, exactly like welle's own CRTL_SDR reader. All InputInterface
// methods are satisfied; gain/AGC are no-ops because the dongle is driven by
// RtlSdrSource, not by welle.
class WelleInput final : public CVirtualInput {
  public:
    WelleInput();
    ~WelleInput() override = default;

    // Called from the RTL-SDR RX thread.
    void feed(const uint8_t* iq, size_t nbytes);
    void flush();

    // Mean magnitude of the most recently fed IQ block, normalized to [0,1].
    // ~0.005-0.02 is the dongle noise floor; a present DAB ensemble is higher.
    float iqLevel() const { return mIqLevel.load(); }

    // InputInterface
    void setFrequency(int frequency) override { mFrequency = frequency; }
    int getFrequency(void) const override { return mFrequency; }
    bool is_ok(void) override { return mOk.load(); }
    bool restart(void) override { mOk = true; return true; }
    void stop(void) override { mOk = false; }
    void reset(void) override { mBuffer.FlushRingBuffer(); }
    int32_t getSamples(DSPCOMPLEX* buffer, int32_t size) override;
    std::vector<DSPCOMPLEX> getSpectrumSamples(int size) override;
    int32_t getSamplesToRead(void) override;
    float setGain(int) override { return 0.f; }
    float getGain(void) const override { return 0.f; }
    int getGainCount(void) override { return 0; }
    void setAgc(bool) override {}
    std::string getDescription(void) override { return "RTL-SDR (HAL-fed)"; }

    // CVirtualInput
    CDeviceID getID(void) override { return CDeviceID::ANDROID_RTL_SDR; }

  private:
    RingBuffer<uint8_t> mBuffer;  // raw 8-bit IQ bytes (2 bytes per sample)
    std::atomic<bool> mOk{true};
    std::atomic<float> mIqLevel{0.0f};
    int mFrequency = 0;
};

// Collects ensemble / service / metadata callbacks from welle-core.
class WelleController; // fwd
class WelleProgramme;  // fwd

// One DAB tuning session: owns the welle RadioReceiver, the IQ input, and the
// metadata sinks. Lifecycle: start(blockFreqHz, scan) -> (feed IQ continuously) ->
// selectService(sid) to route audio -> stop(). Not copyable.
class WelleDabSession {
  public:
    // pcm is interleaved stereo S16 already resampled to 48 kHz.
    using AudioCallback = std::function<void(const int16_t* pcm, size_t frames)>;
    // Fired whenever the service list or ensemble/dynamic label changes.
    using UpdateCallback = std::function<void()>;

    struct ServiceInfo {
        uint32_t sid = 0;
        std::string label;
    };

    WelleDabSession();
    ~WelleDabSession();
    WelleDabSession(const WelleDabSession&) = delete;
    WelleDabSession& operator=(const WelleDabSession&) = delete;

    // (Re)start the receiver tuned to blockFreqHz. doScan only affects how
    // aggressively welle searches; we always end up with a service list.
    bool start(int blockFreqHz, bool doScan);
    void stop();

    // Feed raw 8-bit IQ from the RTL-SDR RX thread.
    void feedIq(const uint8_t* iq, size_t nbytes);

    // Route the audio component of the service with this SId to the audio callback.
    // Returns false if the service is unknown or has no audio component (yet).
    bool selectService(uint32_t sid);
    uint32_t currentService() const { return mCurrentSid.load(); }

    std::vector<ServiceInfo> services() const;
    std::string ensembleLabel() const;
    std::string dynamicLabel() const;

    // MOT slideshow. welle hands us a decoded JPEG/PNG per slide; we keep a small
    // ring of the most recent ones keyed by a monotonic id. slideId() is the latest
    // (0 = none yet); getSlide() serves the bytes the framework asks for via getImage().
    uint32_t slideId() const { return mSlideId.load(); }
    bool getSlide(uint32_t id, std::vector<uint8_t>& out) const;
    bool isSynced() const { return mSynced.load(); }
    // Reception diagnostics (latest values reported by welle on this block).
    float snr() const { return mSnr.load(); }
    bool signalPresent() const { return mSignal.load(); }
    float iqLevel() const { return mInput ? mInput->iqLevel() : 0.0f; }
    // FIB CRC counters since the last start(): the definitive signal-quality
    // indicator. ok>0 means the FIC is decoding (services will follow).
    uint32_t fibOk() const { return mFibOk.load(); }
    uint32_t fibTotal() const { return mFibTotal.load(); }

    void setAudioCallback(AudioCallback cb) { mAudioCb = std::move(cb); }
    void setUpdateCallback(UpdateCallback cb) { mUpdateCb = std::move(cb); }

  private:
    friend class WelleController;
    friend class WelleProgramme;

    // Called by the handlers (on welle threads).
    void onServiceDetected(uint32_t sid);
    void onEnsembleLabel(const std::string& label);
    void onMot(const uint8_t* data, size_t len, int subType, const std::string& name);
    void onSync(bool synced);
    void onSnr(float snr) { mSnr = snr; }
    void onSignalPresence(bool present) { mSignal = present; }
    void onFibDecode(bool ok) {
        ++mFibTotal;
        if (ok) ++mFibOk;
    }
    void onAudio(const int16_t* pcm, size_t frames, int sampleRate);
    void onDynamicLabel(const std::string& label);
    void notifyUpdate();
    void notifierLoop();

    std::unique_ptr<WelleInput> mInput;
    std::unique_ptr<WelleController> mController;
    std::unique_ptr<WelleProgramme> mProgramme;
    std::unique_ptr<RadioReceiver> mRx;

    mutable std::mutex mMeta;
    std::map<uint32_t, ServiceInfo> mServices;  // by SId
    std::string mEnsembleLabel;
    std::string mDynamicLabel;

    // MOT slideshow ring. Images can be tens of KB, so guard them with their own
    // mutex (separate from mMeta) to keep getImage() off the metadata-update path.
    static constexpr size_t kMaxSlides = 4;
    mutable std::mutex mSlideMutex;
    std::map<uint32_t, std::vector<uint8_t>> mSlides;  // by slide id (most recent kMaxSlides)
    std::atomic<uint32_t> mSlideId{0};                 // latest valid id; 0 = none
    // Image ids come from a process-global monotonic counter (see WelleDab.cpp): the
    // app caches getImage() results by id and that cache outlives both service switches
    // and block re-tunes (new sessions), so ids must never be reused. Clearing the ring
    // drops the bytes and sets mSlideId=0 but the global counter never rewinds.

    std::atomic<bool> mSynced{false};
    std::atomic<float> mSnr{0.0f};
    std::atomic<bool> mSignal{false};
    std::atomic<uint32_t> mFibOk{0};
    std::atomic<uint32_t> mFibTotal{0};
    std::atomic<uint32_t> mCurrentSid{0};
    bool mRunning = false;

    // 48 kHz interleaved stereo accumulator for resampling non-48k audio.
    std::vector<int16_t> mResampleScratch;

    AudioCallback mAudioCb;
    UpdateCallback mUpdateCb;

    // mUpdateCb may call back into the receiver (getServiceList), so it must never
    // run on welle's decoder thread while the fib mutex is held. notifyUpdate() only
    // signals this dedicated thread, which invokes mUpdateCb off welle's thread.
    std::thread mNotifyThread;
    std::mutex mNotifyMutex;
    std::condition_variable mNotifyCv;
    bool mNotifyPending = false;
    bool mNotifyStop = false;
};

}  // namespace rtlsdr
