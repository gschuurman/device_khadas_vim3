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

#define LOG_TAG "BcRadioRtlSdr.DeviceManager"

#include "DeviceManager.h"

#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>

#include <android-base/logging.h>
#include <android-base/properties.h>

namespace {
double getDoubleProp(const char* key, double def) {
    const std::string v = ::android::base::GetProperty(key, "");
    if (v.empty()) return def;
    char* end = nullptr;
    const double d = std::strtod(v.c_str(), &end);
    return (end == v.c_str()) ? def : d;
}

constexpr uint32_t kDabInputRate = 2048000;  // welle-core INPUT_RATE
}  // namespace

namespace rtlsdr {

// static
DeviceManager& DeviceManager::getInstance() {
    static DeviceManager sInstance;
    return sInstance;
}

void DeviceManager::onIqData(const uint8_t* iq, size_t nbytes) {
    mPcmScratch.clear();
    mDemod.process(iq, nbytes, &mPcmScratch);

    // Always refresh the RSSI proxy so seek can read it.
    mChannelPower.store(mDemod.takeChannelPower());

    if (mPcmScratch.empty()) return;

    // While seeking, the dongle is hopping across the band; play silence rather than
    // the station fragments, but keep the loopback fed so the capture doesn't underrun.
    if (mSeeking.load()) {
        std::fill(mPcmScratch.begin(), mPcmScratch.end(), int16_t{0});
    }

    enqueuePcm(mPcmScratch.data(), mPcmScratch.size() / LoopbackSink::kChannels);
}

void DeviceManager::onIqDataDab(const uint8_t* iq, size_t nbytes) {
    if (mDab) mDab->feedIq(iq, nbytes);
}

void DeviceManager::enqueuePcm(const int16_t* pcm, size_t frames) {
    if (frames == 0) return;
    const size_t nsamples = frames * LoopbackSink::kChannels;
    std::lock_guard<std::mutex> lk(mQueueLock);
    mQueue.insert(mQueue.end(), pcm, pcm + nsamples);
    if (mQueue.size() > kMaxQueueSamples) {
        // No one is draining the loopback fast enough; drop the oldest audio.
        const size_t drop = mQueue.size() - kMaxQueueSamples;
        mQueue.erase(mQueue.begin(), mQueue.begin() + drop);
    }
    mQueueCv.notify_one();
}

void DeviceManager::writerLoop() {
    // The loopback feed is real-time: a late write underruns the capture side (choppy
    // audio). Bump this thread above the welle OFDM/AAC decode threads it shares cores
    // with so it always wins the race to refill the loopback. Best-effort: SCHED_FIFO
    // needs CAP_SYS_NICE (audioserver has it); fall back to a strong nice otherwise.
    {
        struct sched_param sp = {};
        sp.sched_priority = 2;
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
            setpriority(PRIO_PROCESS, 0, -19);
        }
    }
    std::vector<int16_t> local;
    while (true) {
        {
            std::unique_lock<std::mutex> lk(mQueueLock);
            // DAB pre-roll: hold until the queue has buffered the cushion (or we're
            // stopping), so the loopback starts full and doesn't underrun on the first
            // bursty superframes.
            if (mPreroll > 0) {
                mQueueCv.wait(lk, [this] {
                    return !mWriterRunning || mQueue.size() >= mPreroll;
                });
                mPreroll = 0;
            }
            mQueueCv.wait(lk, [this] { return !mWriterRunning || !mQueue.empty(); });
            if (!mWriterRunning && mQueue.empty()) break;
            local.swap(mQueue);
            mQueue.clear();
        }
        if (!local.empty()) {
            mSink.write(local.data(), local.size() / LoopbackSink::kChannels);
            local.clear();
        }
    }
}

void DeviceManager::startStreamingLocked(Band band) {
    mWriterRunning = true;
    {
        std::lock_guard<std::mutex> lk(mQueueLock);
        mPreroll = (band == Band::kDab) ? kDabPrerollSamples : 0;
    }
    mWriterThread = std::thread([this] { writerLoop(); });
    if (band == Band::kDab) {
        mSource.start([this](const uint8_t* iq, size_t n) { onIqDataDab(iq, n); });
    } else {
        mSource.start([this](const uint8_t* iq, size_t n) { onIqData(iq, n); });
    }
}

bool DeviceManager::ensureSourceRate(uint32_t sampleRate) {
    if (mSource.isOpen() && mSourceRate == sampleRate) return true;
    if (mSource.isOpen()) {
        // Sample rate change requires a reopen; tear down any active streaming first.
        stopStreamingLocked();
        mSource.close();
    }
    if (!mSource.open(sampleRate)) return false;
    mSourceRate = sampleRate;
    return true;
}

void DeviceManager::stopStreamingLocked() {
    mSource.stop();  // joins the RX thread; no more enqueues after this
    {
        std::lock_guard<std::mutex> lk(mQueueLock);
        mWriterRunning = false;
        mQueueCv.notify_one();
    }
    if (mWriterThread.joinable()) mWriterThread.join();
    {
        std::lock_guard<std::mutex> lk(mQueueLock);
        mQueue.clear();
    }
    mSink.close();
}

bool DeviceManager::tuneFm(uint32_t freqHz) {
    std::lock_guard<std::mutex> lk(mLock);

    // Preempt DAB if it currently owns the dongle.
    if (mOwner == Band::kDab) {
        stopStreamingLocked();
        mDab.reset();
        mOwner = Band::kNone;
    }

    if (!ensureSourceRate(fmdemod::FmDemod::kInputRate)) {
        return false;
    }
    if (!mSink.isOpen()) {
        if (!mSink.open()) {
            LOG(ERROR) << "failed to open loopback sink";
            return false;
        }
    }

    // Retuning: reset the demod so we don't smear the previous station's filter history.
    mDemod.reset();
    if (!mSource.setFrequency(freqHz)) {
        return false;
    }

    if (!mSource.isStreaming()) {
        startStreamingLocked(Band::kFm);
    }
    mOwner = Band::kFm;
    return true;
}

bool DeviceManager::tuneDab(int blockFreqHz, bool doScan) {
    std::lock_guard<std::mutex> lk(mLock);

    // Reuse the live session when already decoding this exact block (e.g. tuning to a
    // service in the ensemble the scan just finished, or switching between services in
    // the same ensemble). Tearing it down would discard the already-decoded FIC and
    // force a multi-second re-sync; keeping it makes those tunes effectively instant.
    // Only reuse for service tuning (doScan=false): a fresh scan pass wants a clean
    // session so the service list isn't stale.
    if (!doScan && mOwner == Band::kDab && mDab && mDabFreqHz == blockFreqHz) {
        LOG(INFO) << "reusing live DAB session on " << blockFreqHz << " Hz";
        return true;
    }

    // Switching bands always restarts streaming (different sample rate and IQ sink).
    if (mOwner != Band::kNone) {
        stopStreamingLocked();
        mDab.reset();
        mDabFreqHz = 0;
        mOwner = Band::kNone;
    }

    if (!ensureSourceRate(kDabInputRate)) {
        return false;
    }
    if (!mSink.isOpen() && !mSink.open()) {
        LOG(ERROR) << "failed to open loopback sink";
        return false;
    }
    if (!mSource.setFrequency(static_cast<uint32_t>(blockFreqHz))) {
        return false;
    }
    // DAB is an OFDM/QAM waveform: unlike constant-envelope FM, it must not clip.
    // Tuner AGC overloads the front-end in Band III (observed mean IQ magnitude
    // ~0.9 = ADC railing), which destroys sync. Use a moderate manual gain;
    // override live via `setprop vendor.broadcastradio.dab.gain <tenths-dB>`
    // (negative = back to AGC).
    // Prefer a gain already learned by the adaptive search (sticky across blocks and
    // into service tuning); otherwise start from the property default.
    int gainTenthDb = mDabGainTenthDb;
    if (gainTenthDb < 0) {
        gainTenthDb = android::base::GetIntProperty("vendor.broadcastradio.dab.gain", 340);
    }
    mSource.setManualGainTenthDb(gainTenthDb);

    mDab = std::make_unique<WelleDabSession>();
    mDab->setAudioCallback(
            [this](const int16_t* pcm, size_t frames) { enqueuePcm(pcm, frames); });
    if (mDabUpdateCb) mDab->setUpdateCallback(mDabUpdateCb);
    if (!mDab->start(blockFreqHz, doScan)) {
        mDab.reset();
        return false;
    }

    startStreamingLocked(Band::kDab);
    mOwner = Band::kDab;
    mDabFreqHz = blockFreqHz;
    LOG(INFO) << "tuned DAB block at " << blockFreqHz << " Hz, scan=" << doScan;
    return true;
}

bool DeviceManager::selectDabService(uint32_t sid) {
    std::lock_guard<std::mutex> lk(mLock);
    if (mOwner != Band::kDab || !mDab) return false;
    return mDab->selectService(sid);
}

std::vector<WelleDabSession::ServiceInfo> DeviceManager::dabServices() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->services() : std::vector<WelleDabSession::ServiceInfo>{};
}

std::string DeviceManager::dabEnsembleLabel() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->ensembleLabel() : std::string{};
}

std::string DeviceManager::dabDynamicLabel() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->dynamicLabel() : std::string{};
}

bool DeviceManager::dabIsSynced() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->isSynced() : false;
}

float DeviceManager::dabSnr() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->snr() : 0.0f;
}

bool DeviceManager::dabSignalPresent() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->signalPresent() : false;
}

float DeviceManager::dabIqLevel() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->iqLevel() : 0.0f;
}

uint32_t DeviceManager::dabFibOk() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->fibOk() : 0;
}

uint32_t DeviceManager::dabFibTotal() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->fibTotal() : 0;
}

uint32_t DeviceManager::dabSlideId() const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->slideId() : 0;
}

bool DeviceManager::dabGetImage(uint32_t id, std::vector<uint8_t>& out) const {
    std::lock_guard<std::mutex> lk(mLock);
    return mDab ? mDab->getSlide(id, out) : false;
}

int DeviceManager::setDabGainTenthDb(int tenthDb) {
    std::lock_guard<std::mutex> lk(mLock);
    const int actual = mSource.setManualGainTenthDb(tenthDb);
    if (tenthDb >= 0) mDabGainTenthDb = tenthDb;
    return actual;
}

void DeviceManager::setDabUpdateCallback(WelleDabSession::UpdateCallback cb) {
    std::lock_guard<std::mutex> lk(mLock);
    mDabUpdateCb = cb;
    if (mDab) mDab->setUpdateCallback(cb);
}

void DeviceManager::release(Band band) {
    std::lock_guard<std::mutex> lk(mLock);
    if (mOwner != band) return;  // not the current owner; nothing to do
    stopStreamingLocked();
    if (band == Band::kDab) mDab.reset();
    mOwner = Band::kNone;
    LOG(INFO) << "released dongle";
}

uint32_t DeviceManager::seekFm(bool directionUp, uint32_t lowerHz, uint32_t upperHz,
                               uint32_t stepHz) {
    std::lock_guard<std::mutex> lk(mLock);
    if (!mSource.isOpen() || !mSource.isStreaming() || stepHz == 0 || upperHz <= lowerHz) {
        return 0;
    }

    // Tunables (sysprops so they can be calibrated on-device without a rebuild):
    //   seek_factor : stop when in-channel power exceeds (running noise floor * factor)
    //   seek_thresh : if > 0, use this absolute power threshold instead of the floor
    //   seek_settle_ms : dwell per frequency (PLL settle + FIR flush + a few RX buffers)
    const double factor = getDoubleProp("persist.vendor.radio.fm.seek_factor", 6.0);
    const double absThresh = getDoubleProp("persist.vendor.radio.fm.seek_thresh", 0.0);
    const int settleUs =
            static_cast<int>(getDoubleProp("persist.vendor.radio.fm.seek_settle_ms", 45)) * 1000;

    const uint32_t startFreq = mSource.frequency();
    const int nsteps = static_cast<int>((upperHz - lowerHz) / stepHz) + 1;

    mSeekCancel = false;
    mSeeking = true;
    mSource.setFixedGainForSeek();  // fixed gain so RSSI is comparable across freqs

    uint32_t freq = startFreq;
    uint32_t found = 0;
    double floor = 1e30;
    for (int i = 0; i < nsteps && !mSeekCancel.load(); ++i) {
        if (directionUp) {
            freq = (freq + stepHz > upperHz) ? lowerHz : freq + stepHz;
        } else {
            freq = (freq < lowerHz + stepHz) ? upperHz : freq - stepHz;
        }
        mSource.setFrequency(freq);
        usleep(settleUs);  // RX thread keeps updating mChannelPower meanwhile

        const double p = mChannelPower.load();
        if (p < floor) floor = p;  // track noise floor as the running minimum
        const double thr = (absThresh > 0.0) ? absThresh : floor * factor;
        LOG(INFO) << "seek " << freq << " Hz power=" << p << " floor=" << floor << " thr=" << thr;

        // Need at least one prior sample to have a floor reference.
        if (i > 0 && p > thr) {
            found = freq;
            break;
        }
    }

    mSeeking = false;
    mSource.setAutoGain();
    mDemod.reset();
    mSource.setFrequency(found ? found : startFreq);
    LOG(INFO) << "seek done: " << (found ? "found " : "no station, restored ")
              << (found ? found : startFreq) << " Hz";
    return found;
}

}  // namespace rtlsdr
