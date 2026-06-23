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

#include <aidl/android/hardware/broadcastradio/BnBroadcastRadio.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "DeviceManager.h"

namespace aidl::android::hardware::broadcastradio {

// RTL-SDR backed BroadcastRadio HAL. One instance per band (amfm / dab). FM is fully
// implemented (P2); the DAB instance advertises itself but rejects tuning until P3.
class BroadcastRadio final : public BnBroadcastRadio {
  public:
    enum class Band { kAmFm, kDab };

    explicit BroadcastRadio(Band band);
    ~BroadcastRadio();

    ndk::ScopedAStatus getAmFmRegionConfig(bool full, AmFmRegionConfig* returnConfigs) override;
    ndk::ScopedAStatus getDabRegionConfig(std::vector<DabTableEntry>* returnConfigs) override;
    ndk::ScopedAStatus getImage(int32_t id, std::vector<uint8_t>* returnImage) override;
    ndk::ScopedAStatus getProperties(Properties* returnProperties) override;

    ndk::ScopedAStatus setTunerCallback(const std::shared_ptr<ITunerCallback>& callback) override;
    ndk::ScopedAStatus unsetTunerCallback() override;
    ndk::ScopedAStatus tune(const ProgramSelector& program) override;
    ndk::ScopedAStatus seek(bool directionUp, bool skipSubChannel) override;
    ndk::ScopedAStatus step(bool directionUp) override;
    ndk::ScopedAStatus cancel() override;
    ndk::ScopedAStatus startProgramListUpdates(const ProgramFilter& filter) override;
    ndk::ScopedAStatus stopProgramListUpdates() override;
    ndk::ScopedAStatus isConfigFlagSet(ConfigFlag flag, bool* returnIsSet) override;
    ndk::ScopedAStatus setConfigFlag(ConfigFlag flag, bool value) override;
    ndk::ScopedAStatus setParameters(const std::vector<VendorKeyValue>& parameters,
                                     std::vector<VendorKeyValue>* returnParameters) override;
    ndk::ScopedAStatus getParameters(const std::vector<std::string>& keys,
                                     std::vector<VendorKeyValue>* returnParameters) override;
    ndk::ScopedAStatus registerAnnouncementListener(
            const std::shared_ptr<IAnnouncementListener>& listener,
            const std::vector<AnnouncementType>& enabled,
            std::shared_ptr<ICloseHandle>* returnCloseHandle) override;

  private:
    // Tune the FM hardware to freqKhz, push audio, and notify the client. mLock held.
    ndk::ScopedAStatus tuneFmLocked(uint32_t freqKhz);
    ProgramInfo makeFmProgramInfo(uint32_t freqKhz) const;
    static uint32_t clampFmKhz(int64_t freqKhz);

    // Cancel any in-flight async seek and join its thread.
    void stopSeekThread();
    // Worker body: runs the (blocking) signal-based seek and reports the result
    // via the captured tuner callback. Runs off-binder so seek()/cancel() return
    // promptly and the app never blocks on the multi-second band sweep.
    void seekWorker(bool directionUp, std::shared_ptr<ITunerCallback> cb);

    // --- DAB (P3) ----------------------------------------------------------
    // Tune a DAB block and, if sidExt != 0, select that service. Runs on a worker
    // because acquiring sync + FIC + the audio component takes a couple of seconds.
    void dabTuneWorker(uint32_t blockFreqKhz, uint32_t ensemble, uint64_t sidExt,
                       std::shared_ptr<ITunerCallback> cb);
    // Ensemble scan: sweep the Band III blocks, collect services, stream them to the
    // client as ProgramListChunks.
    void dabScanWorker(std::shared_ptr<ITunerCallback> cb);
    // Diagnostic/recovery sweep (prop vendor.broadcastradio.dab.scansweep): probe every
    // Band III block, log sync/services, land on the first ensemble that decodes.
    void dabSweepWorker(std::shared_ptr<ITunerCallback> cb);
    // Build a DAB ProgramInfo for the given service, filling in ensemble/service/DLS
    // metadata from the live welle session.
    // nowPlaying=true attaches the live MOT slideshow art (albumArt id) — only the
    // tuned service carries it, not the Browse list entries.
    ProgramInfo makeDabProgramInfo(uint64_t sidExt, uint32_t ensemble, uint32_t freqKhz,
                                   const std::string& serviceName,
                                   bool nowPlaying = false) const;
    // Publish the services welle has decoded for the currently-tuned DAB block as a
    // program list. A tune decodes the whole ensemble's FIC, so this populates Browse
    // with the tuned ensemble even when no full-band scan ran (the app's restore-tune
    // cancels the scan). purge=true replaces the list (initial emit after a tune);
    // purge=false grows it as more services arrive (update-callback path).
    void emitDabProgramList(const std::shared_ptr<ITunerCallback>& cb, bool purge);
    void cancelDab() { mDabCancel = true; }

    // Combined module: the band actually active is chosen per-tune from the program
    // selector and tracked here so seek/step/program-list route correctly. Seeded at
    // construction from the instance's default band.
    std::atomic<Band> mActiveBand;
    std::mutex mLock;
    std::shared_ptr<ITunerCallback> mCallback;
    std::atomic<uint32_t> mCurrentFreqKhz{87500};
    int mConfigFlags = 0;

    std::mutex mSeekMutex;  // serialises the per-instance worker thread lifecycle
    std::thread mSeekThread;

    // DAB worker + state (the DAB instance only).
    std::thread mDabThread;
    std::atomic<bool> mDabCancel{false};
    std::atomic<uint32_t> mDabBlockKhz{0};
    std::atomic<uint32_t> mDabEnsemble{0};
    std::atomic<uint64_t> mDabSidExt{0};
    std::string mLastDabListSig;  // (sid:label) signature of the last published DAB list; guarded by mLock

    // FM signal-quality metering: FM has no async event source (unlike DAB's welle
    // callback), so a ProgramInfo built at tune time captures signalQuality=0 before the
    // RX thread has accumulated any in-channel power. This thread periodically re-pushes
    // onCurrentProgramInfoChanged while FM is the live band so the UI bar tracks reception.
    void meterLoop();
    std::thread mMeterThread;
    std::atomic<bool> mMeterRun{false};

    // Antenna/tuner-presence reporting. The framework starts "undetermined"; we report
    // connected on the first successful tune and disconnected on an RTL-SDR unplug.
    // Deduped so we only push onAntennaStateChange on an actual transition.
    void reportAntennaState(bool connected);
    std::atomic<int> mAntennaState{-1};  // -1 undetermined, 0 disconnected, 1 connected
};

}  // namespace aidl::android::hardware::broadcastradio
