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

#define LOG_TAG "BcRadioRtlSdr.Welle"

#include "WelleDab.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <android-base/logging.h>

namespace rtlsdr {

namespace {
// welle DAB input rate (matches backend INPUT_RATE). Two bytes per IQ sample.
constexpr uint32_t kDabInputRate = 2048000;
// ~1 s of 8-bit IQ; the OFDM processor drains it continuously. Power-of-two
// required by RingBuffer (it rounds up internally anyway).
constexpr uint32_t kIqRingBytes = 4 * 1024 * 1024;
constexpr int kOutRate = 48000;  // loopback sink rate
// Process-global MOT image-id counter. Unique across all sessions/blocks so the app's
// getImage cache (which outlives a session) never sees a reused id. 0 is reserved.
std::atomic<uint32_t> gSlideSeq{0};
}  // namespace

// ---------------------------------------------------------------------------
// WelleInput
// ---------------------------------------------------------------------------
WelleInput::WelleInput() : mBuffer(kIqRingBytes) {}

void WelleInput::feed(const uint8_t* iq, size_t nbytes) {
    if (nbytes == 0) return;
    // Estimate RF level from this block (subsample for speed): mean magnitude of
    // the IQ vector around the 128 DC midpoint, normalized to [0,1].
    double acc = 0.0;
    size_t n = 0;
    for (size_t i = 0; i + 1 < nbytes; i += 64) {  // every 32nd IQ sample
        const float fi = (static_cast<float>(iq[i]) - 128.0f) / 128.0f;
        const float fq = (static_cast<float>(iq[i + 1]) - 128.0f) / 128.0f;
        acc += std::sqrt(fi * fi + fq * fq);
        ++n;
    }
    if (n) mIqLevel = static_cast<float>(acc / n);
    // Drop on overflow (OFDM not draining): better to lose IQ than block the USB
    // event loop. RingBuffer silently writes only what fits.
    mBuffer.putDataIntoBuffer(iq, static_cast<int32_t>(nbytes));
}

void WelleInput::flush() {
    mBuffer.FlushRingBuffer();
}

int32_t WelleInput::getSamples(DSPCOMPLEX* buffer, int32_t size) {
    // size = number of complex samples requested -> 2 bytes each.
    std::vector<uint8_t> tmp(2 * size);
    const int32_t got = mBuffer.getDataFromBuffer(tmp.data(), 2 * size);
    const int32_t samples = got / 2;
    for (int32_t i = 0; i < samples; ++i) {
        buffer[i] = DSPCOMPLEX((float(tmp[2 * i]) - 128.0f) / 128.0f,
                               (float(tmp[2 * i + 1]) - 128.0f) / 128.0f);
    }
    return samples;
}

std::vector<DSPCOMPLEX> WelleInput::getSpectrumSamples(int size) {
    // Not used by the HAL (no spectrum UI); return zeros of the requested length.
    return std::vector<DSPCOMPLEX>(size, DSPCOMPLEX(0.0f, 0.0f));
}

int32_t WelleInput::getSamplesToRead(void) {
    return mBuffer.GetRingBufferReadAvailable() / 2;
}

// ---------------------------------------------------------------------------
// Controller / programme handler bridges (live on welle's threads).
// ---------------------------------------------------------------------------
class WelleController final : public RadioControllerInterface {
  public:
    explicit WelleController(WelleDabSession* s) : mSession(s) {}

    void onServiceDetected(uint32_t sId) override { mSession->onServiceDetected(sId); }
    void onNewEnsemble(uint16_t /*eId*/) override {}
    void onSetEnsembleLabel(DabLabel& label) override {
        mSession->onEnsembleLabel(label.utf8_label());
    }
    void onSyncChange(char isSync) override { mSession->onSync(isSync != 0); }
    void onSignalPresence(bool isSignal) override { mSession->onSignalPresence(isSignal); }

    // Telemetry callbacks.
    void onSNR(float snr) override { mSession->onSnr(snr); }
    void onFrequencyCorrectorChange(int /*fine*/, int /*coarse*/) override {}
    void onDateTimeUpdate(const dab_date_time_t& /*dt*/) override {}
    void onFIBDecodeSuccess(bool ok, const uint8_t* /*fib*/) override {
        mSession->onFibDecode(ok);
    }
    void onNewImpulseResponse(std::vector<float>&& /*d*/) override {}
    void onConstellationPoints(std::vector<DSPCOMPLEX>&& /*d*/) override {}
    void onNewNullSymbol(std::vector<DSPCOMPLEX>&& /*d*/) override {}
    void onTIIMeasurement(tii_measurement_t&& /*m*/) override {}
    void onMessage(message_level_t /*l*/, const std::string& /*t*/,
                   const std::string& /*t2*/) override {}

  private:
    WelleDabSession* const mSession;
};

class WelleProgramme final : public ProgrammeHandlerInterface {
  public:
    explicit WelleProgramme(WelleDabSession* s) : mSession(s) {}

    void onNewAudio(std::vector<int16_t>&& audioData, int sampleRate,
                    const std::string& mode) override {
        // welle emits interleaved stereo S16.
        const size_t frames = audioData.size() / 2;
        mSession->onAudio(audioData.data(), frames, sampleRate);
        // Periodic audio-health line: sample rate / codec mode and any decode errors
        // accumulated since the last report. Choppy audio with a clean loopback feed is
        // almost always welle dropping superframes here (reception) or a rate mismatch.
        if (sampleRate != mLastRate || mode != mLastMode) {
            LOG(INFO) << "DAB audio: rate=" << sampleRate << " mode=\"" << mode << "\"";
            mLastRate = sampleRate;
            mLastMode = mode;
        }
        // Report decode errors once every 5 s, but only when there actually are any —
        // clean playback stays silent in the log. (Choppy audio with no errors here and a
        // clean loopback feed is downstream: e.g. USB isochronous starvation of the output
        // device when it shares a hub with the RTL-SDR's DAB-rate traffic.)
        const auto now = std::chrono::steady_clock::now();
        if (mReportTime.time_since_epoch().count() == 0) mReportTime = now;
        if (std::chrono::duration<double>(now - mReportTime).count() >= 5.0) {
            if (mAacErr || mRsUncorr || mFrameErr) {
                LOG(WARNING) << "DAB decode errors (last 5 s): aacErr=" << mAacErr
                             << " rsUncorr=" << mRsUncorr << " frameErr=" << mFrameErr;
            }
            mReportTime = now;
            mAacErr = mRsUncorr = mFrameErr = 0;
        }
    }
    void onNewDynamicLabel(const std::string& label) override {
        mSession->onDynamicLabel(label);
    }
    void onMOT(const mot_file_t& mot) override {
        mSession->onMot(mot.data.data(), mot.data.size(), mot.content_sub_type,
                        mot.content_name);
    }

    void onFrameErrors(int frameErrors) override {
        if (frameErrors > 0) mFrameErr += frameErrors;
    }
    void onRsErrors(bool uncorrected, int /*numCorrected*/) override {
        if (uncorrected) ++mRsUncorr;
    }
    void onAacErrors(int aacErrors) override {
        if (aacErrors > 0) mAacErr += aacErrors;
    }
    void onPADLengthError(size_t /*announced*/, size_t /*actual*/) override {}

  private:
    WelleDabSession* const mSession;
    // Decode-error diagnostics (welle decoder thread only).
    std::chrono::steady_clock::time_point mReportTime{};
    int mLastRate = 0;
    std::string mLastMode;
    int mAacErr = 0;
    int mRsUncorr = 0;
    int mFrameErr = 0;
};

// ---------------------------------------------------------------------------
// WelleDabSession
// ---------------------------------------------------------------------------
WelleDabSession::WelleDabSession() = default;

WelleDabSession::~WelleDabSession() {
    stop();
}

bool WelleDabSession::start(int blockFreqHz, bool doScan) {
    stop();

    mInput = std::make_unique<WelleInput>();
    mController = std::make_unique<WelleController>(this);
    mProgramme = std::make_unique<WelleProgramme>(this);

    mInput->setFrequency(blockFreqHz);
    mInput->restart();

    RadioReceiverOptions rro;  // defaults are fine for Band III TM1
    mRx = std::make_unique<RadioReceiver>(*mController, *mInput, rro);
    mRx->restart(doScan);

    {
        std::lock_guard<std::mutex> lk(mMeta);
        mServices.clear();
        mEnsembleLabel.clear();
        mDynamicLabel.clear();
    }
    {
        std::lock_guard<std::mutex> lk(mSlideMutex);
        mSlides.clear();
        mSlideId = 0;
    }
    mSynced = false;
    mSnr = 0.0f;
    mSignal = false;
    mFibOk = 0;
    mFibTotal = 0;
    mCurrentSid = 0;
    mRunning = true;

    // Start the notifier thread that runs mUpdateCb off welle's decoder thread.
    {
        std::lock_guard<std::mutex> lk(mNotifyMutex);
        mNotifyStop = false;
        mNotifyPending = false;
    }
    mNotifyThread = std::thread([this] { notifierLoop(); });
    LOG(INFO) << "welle session started on " << blockFreqHz << " Hz, scan=" << doScan;
    return true;
}

void WelleDabSession::stop() {
    if (!mRunning && !mRx && !mNotifyThread.joinable()) return;
    if (mRx) {
        mRx->stop();   // joins the OFDM thread: no more callbacks after this
        mRx.reset();
    }
    if (mInput) mInput->stop();
    // Tear down the notifier thread once welle can no longer signal it.
    if (mNotifyThread.joinable()) {
        {
            std::lock_guard<std::mutex> lk(mNotifyMutex);
            mNotifyStop = true;
        }
        mNotifyCv.notify_all();
        mNotifyThread.join();
    }
    mProgramme.reset();
    mController.reset();
    mInput.reset();
    mRunning = false;
}

void WelleDabSession::feedIq(const uint8_t* iq, size_t nbytes) {
    if (mInput) mInput->feed(iq, nbytes);
}

bool WelleDabSession::selectService(uint32_t sid) {
    if (!mRx) return false;
    const Service s = mRx->getService(sid);
    if (s.serviceId == 0 || !mRx->serviceHasAudioComponent(s)) {
        LOG(WARNING) << "service " << sid << " has no audio component (yet)";
        return false;
    }
    if (!mRx->playSingleProgramme(*mProgramme, "", s)) {
        LOG(WARNING) << "playSingleProgramme failed for " << sid;
        return false;
    }
    // MOT slideshow art is per-programme: drop the previous service's slides so the
    // framework doesn't show stale art until the new service's MOT arrives.
    if (sid != mCurrentSid.load()) {
        std::lock_guard<std::mutex> lk(mSlideMutex);
        mSlides.clear();
        mSlideId = 0;
    }
    mCurrentSid = sid;
    LOG(INFO) << "selected DAB service " << sid;
    return true;
}

std::vector<WelleDabSession::ServiceInfo> WelleDabSession::services() const {
    // welle's onServiceDetected callback does not fire reliably in scan mode, so
    // query the receiver's authoritative service list directly. Fall back to the
    // callback-populated map if the receiver is gone.
    std::map<uint32_t, ServiceInfo> merged;
    {
        std::lock_guard<std::mutex> lk(mMeta);
        merged = mServices;
    }
    if (mRx) {
        for (const auto& s : mRx->getServiceList()) {
            if (s.serviceId == 0) continue;
            const std::string label = s.serviceLabel.utf8_label();
            auto& info = merged[s.serviceId];
            info.sid = s.serviceId;
            if (!label.empty()) info.label = label;
        }
    }
    std::vector<ServiceInfo> out;
    out.reserve(merged.size());
    for (const auto& [sid, info] : merged) out.push_back(info);
    return out;
}

std::string WelleDabSession::ensembleLabel() const {
    std::lock_guard<std::mutex> lk(mMeta);
    return mEnsembleLabel;
}

std::string WelleDabSession::dynamicLabel() const {
    std::lock_guard<std::mutex> lk(mMeta);
    return mDynamicLabel;
}

void WelleDabSession::onServiceDetected(uint32_t sid) {
    // IMPORTANT: welle invokes this from FIBProcessor::processFIB while holding the
    // fib-processor's (non-recursive) mutex. We must NOT call back into the receiver
    // here (e.g. getService()) — that re-locks the same mutex on the same thread and
    // self-deadlocks welle's decoder. Just record the SId; the human-readable label
    // is filled in later by services()/getServiceList() from our own thread.
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mMeta);
        if (mServices.find(sid) == mServices.end()) {
            mServices[sid] = ServiceInfo{sid, ""};
            changed = true;
        }
    }
    if (changed) notifyUpdate();
}

void WelleDabSession::onEnsembleLabel(const std::string& label) {
    {
        std::lock_guard<std::mutex> lk(mMeta);
        if (mEnsembleLabel == label) return;
        mEnsembleLabel = label;
    }
    notifyUpdate();
}

void WelleDabSession::onMot(const uint8_t* data, size_t len, int subType,
                            const std::string& name) {
    if (data == nullptr || len == 0) return;
    uint32_t id;
    {
        std::lock_guard<std::mutex> lk(mSlideMutex);
        id = ++gSlideSeq;            // process-global, never rewinds
        if (id == 0) id = ++gSlideSeq;  // skip 0 ("no art") on wrap
        mSlides[id].assign(data, data + len);
        // Evict the oldest slides beyond the ring depth (map is ordered by id).
        while (mSlides.size() > kMaxSlides) mSlides.erase(mSlides.begin());
        mSlideId = id;
    }
    LOG(INFO) << "MOT slide id=" << id << " (" << len << " bytes, subtype=" << subType
              << (name.empty() ? "" : ", \"" + name + "\"") << ")";
    notifyUpdate();
}

bool WelleDabSession::getSlide(uint32_t id, std::vector<uint8_t>& out) const {
    std::lock_guard<std::mutex> lk(mSlideMutex);
    const auto it = mSlides.find(id);
    if (it == mSlides.end()) return false;
    out = it->second;
    return true;
}

void WelleDabSession::onSync(bool synced) {
    mSynced = synced;
}

void WelleDabSession::onDynamicLabel(const std::string& label) {
    {
        std::lock_guard<std::mutex> lk(mMeta);
        if (mDynamicLabel == label) return;
        mDynamicLabel = label;
    }
    notifyUpdate();
}

void WelleDabSession::onAudio(const int16_t* pcm, size_t frames, int sampleRate) {
    if (!mAudioCb || frames == 0) return;

    if (sampleRate == kOutRate) {
        mAudioCb(pcm, frames);
        return;
    }

    // Linear-resample interleaved stereo to 48 kHz. DAB+ AAC is usually 48k or 32k,
    // MP2 is 48k or 24k; the loopback runs at a fixed 48k stereo.
    const double ratio = static_cast<double>(kOutRate) / sampleRate;
    const size_t outFrames = static_cast<size_t>(frames * ratio);
    mResampleScratch.resize(outFrames * 2);
    for (size_t i = 0; i < outFrames; ++i) {
        const double srcPos = i / ratio;
        const size_t i0 = static_cast<size_t>(srcPos);
        const size_t i1 = std::min(i0 + 1, frames - 1);
        const double frac = srcPos - i0;
        for (int c = 0; c < 2; ++c) {
            const double a = pcm[i0 * 2 + c];
            const double b = pcm[i1 * 2 + c];
            mResampleScratch[i * 2 + c] =
                    static_cast<int16_t>(a + (b - a) * frac);
        }
    }
    mAudioCb(mResampleScratch.data(), outFrames);
}

void WelleDabSession::notifyUpdate() {
    // Only signal: the actual mUpdateCb (which may re-enter the receiver via
    // getServiceList) runs on the notifier thread, never on welle's locked thread.
    std::lock_guard<std::mutex> lk(mNotifyMutex);
    mNotifyPending = true;
    mNotifyCv.notify_one();
}

void WelleDabSession::notifierLoop() {
    for (;;) {
        {
            std::unique_lock<std::mutex> lk(mNotifyMutex);
            mNotifyCv.wait(lk, [this] { return mNotifyPending || mNotifyStop; });
            if (mNotifyStop) return;
            mNotifyPending = false;
        }
        if (mUpdateCb) mUpdateCb();
    }
}

}  // namespace rtlsdr
