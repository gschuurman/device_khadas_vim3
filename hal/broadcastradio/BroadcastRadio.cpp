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

#define LOG_TAG "BcRadioRtlSdr"

#include "BroadcastRadio.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <broadcastradio-utils-aidl/Utils.h>

namespace aidl::android::hardware::broadcastradio {

namespace {
// EU FM band, all values in kHz.
constexpr uint32_t kFmLowerKhz = 87500;
constexpr uint32_t kFmUpperKhz = 108000;
constexpr uint32_t kFmSpacingKhz = 100;

// EU Band III DAB blocks (subset 5A..13F), frequency in kHz. Matches welle's table.
struct DabBlock {
    const char* label;
    uint32_t freqKhz;
};
constexpr DabBlock kDabBlocks[] = {
        {"5A", 174928},  {"5B", 176640},  {"5C", 178352},  {"5D", 180064},
        {"6A", 181936},  {"6B", 183648},  {"6C", 185360},  {"6D", 187072},
        {"7A", 188928},  {"7B", 190640},  {"7C", 192352},  {"7D", 194064},
        {"8A", 195936},  {"8B", 197648},  {"8C", 199360},  {"8D", 201072},
        {"9A", 202928},  {"9B", 204640},  {"9C", 206352},  {"9D", 208064},
        {"10A", 209936}, {"10B", 211648}, {"10C", 213360}, {"10D", 215072},
        {"11A", 216928}, {"11B", 218640}, {"11C", 220352}, {"11D", 222064},
        {"12A", 223936}, {"12B", 225648}, {"12C", 227360}, {"12D", 229072},
        {"13A", 230784}, {"13B", 232496}, {"13C", 234208}, {"13D", 235776},
        {"13E", 237488}, {"13F", 239200},
};

// How long a tune waits for the requested service to become decodable.
// A cold tune (different ensemble than the live session) must re-sync the OFDM frame
// and re-decode the FIC before the target service's audio subchannel is known; on a
// marginal Band III whip this can take ~10 s. Same-ensemble tunes reuse the live
// session (see DeviceManager::tuneDab) and resolve almost immediately.
constexpr auto kDabTuneTimeout = std::chrono::seconds(15);
constexpr auto kDabPollInterval = std::chrono::milliseconds(200);

// Map an FM in-channel power proxy (mean |IQ|^2 after channel filtering) to the
// 0..100 signalQuality the UI renders as bars. Power is linear, so work in dB; the
// floor/ceil bounds were calibrated against live `seek ... power=` logs (noise floor
// ~ -38 dB, a strong local station ~ -10 dB). Tunable via properties for field tuning.
int fmSignalQuality(double power) {
    if (power <= 0.0) return 0;
    const double db = 10.0 * std::log10(power);
    const double lo = ::android::base::GetIntProperty("vendor.broadcastradio.fm.q.lodb", -38);
    const double hi = ::android::base::GetIntProperty("vendor.broadcastradio.fm.q.hidb", -10);
    const double q = (db - lo) / (hi - lo) * 100.0;
    return static_cast<int>(std::clamp(q, 0.0, 100.0));
}

// Map DAB welle metrics to 0..100. welle's SNR estimate tops out ~12-15 dB on a clean
// ensemble; the FIC CRC ratio (fibOk/fibTotal) is the authoritative lock indicator.
// Blend the two so the bar reflects both raw RF (SNR) and decode health (FIB).
int dabSignalQuality(bool synced, float snrDb, uint32_t fibOk, uint32_t fibTotal) {
    if (!synced) return 0;
    const double snrScore = std::clamp(static_cast<double>(snrDb) / 12.0, 0.0, 1.0);
    const double fibScore = fibTotal ? std::clamp(static_cast<double>(fibOk) / fibTotal, 0.0, 1.0)
                                     : 0.0;
    const double q = (0.6 * snrScore + 0.4 * fibScore) * 100.0;
    return static_cast<int>(std::clamp(q, 0.0, 100.0));
}

ndk::ScopedAStatus resultToStatus(Result result, const std::string& msg) {
    LOG(WARNING) << msg << ": " << toString(result);
    return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(static_cast<int32_t>(result),
                                                                   msg.c_str());
}
}  // namespace

BroadcastRadio::BroadcastRadio(Band band) : mActiveBand(band) {
    LOG(INFO) << "RTL-SDR BroadcastRadio HAL created (combined AM/FM+DAB), default band "
              << (band == Band::kAmFm ? "AM/FM" : "DAB");
}

BroadcastRadio::~BroadcastRadio() {
    stopSeekThread();
    mMeterRun = false;
    if (mMeterThread.joinable()) mMeterThread.join();
    rtlsdr::DeviceManager::getInstance().release(
            mActiveBand.load() == Band::kAmFm ? rtlsdr::DeviceManager::Band::kFm
                                              : rtlsdr::DeviceManager::Band::kDab);
}

void BroadcastRadio::stopSeekThread() {
    std::lock_guard<std::mutex> g(mSeekMutex);
    rtlsdr::DeviceManager::getInstance().cancelSeek();
    if (mSeekThread.joinable()) mSeekThread.join();
    mDabCancel = true;
    if (mDabThread.joinable()) mDabThread.join();
    mDabCancel = false;
}

void BroadcastRadio::seekWorker(bool directionUp, std::shared_ptr<ITunerCallback> cb) {
    const uint32_t foundHz = rtlsdr::DeviceManager::getInstance().seekFm(
            directionUp, kFmLowerKhz * 1000, kFmUpperKhz * 1000, kFmSpacingKhz * 1000);
    if (cb != nullptr) {
        if (foundHz != 0) {
            mCurrentFreqKhz = foundHz / 1000;
            cb->onCurrentProgramInfoChanged(makeFmProgramInfo(foundHz / 1000));
        } else {
            cb->onTuneFailed(Result::TIMEOUT, utils::makeSelectorAmfm(mCurrentFreqKhz.load()));
        }
    }
}

ProgramInfo BroadcastRadio::makeDabProgramInfo(uint64_t sidExt, uint32_t ensemble,
                                              uint32_t freqKhz,
                                              const std::string& serviceName,
                                              bool nowPlaying) const {
    ProgramInfo info = {};
    info.selector = utils::makeSelectorDab(sidExt, ensemble, freqKhz);
    info.logicallyTunedTo = info.selector.primaryId;
    info.physicallyTunedTo = utils::makeIdentifier(IdentifierType::DAB_FREQUENCY_KHZ, freqKhz);
    info.infoFlags = ProgramInfo::FLAG_LIVE | ProgramInfo::FLAG_TUNABLE;
    auto& dm = rtlsdr::DeviceManager::getInstance();
    const bool synced = dm.dabIsSynced();
    if (synced) {
        info.infoFlags |= ProgramInfo::FLAG_SIGNAL_ACQUISITION | ProgramInfo::FLAG_STEREO;
    }
    info.signalQuality = dabSignalQuality(synced, dm.dabSnr(), dm.dabFibOk(), dm.dabFibTotal());

    std::vector<Metadata> md;
    const std::string ensembleName = dm.dabEnsembleLabel();
    if (!ensembleName.empty()) {
        md.push_back(Metadata::make<Metadata::dabEnsembleName>(ensembleName));
    }
    if (!serviceName.empty()) {
        md.push_back(Metadata::make<Metadata::dabServiceName>(serviceName));
        md.push_back(Metadata::make<Metadata::programName>(serviceName));
    }
    const std::string dls = dm.dabDynamicLabel();
    if (!dls.empty()) {
        md.push_back(Metadata::make<Metadata::rdsRt>(dls));
    }
    // MOT slideshow art belongs to the live programme, so advertise it only on the
    // now-playing info (Browse rows would otherwise all show the tuned station's slide).
    // The framework fetches the bytes lazily via getImage(albumArt).
    if (nowPlaying) {
        const uint32_t slide = dm.dabSlideId();
        if (slide != 0) {
            md.push_back(Metadata::make<Metadata::albumArt>(static_cast<int32_t>(slide)));
        }
    }
    info.metadata = std::move(md);
    return info;
}

void BroadcastRadio::dabTuneWorker(uint32_t blockFreqKhz, uint32_t ensemble, uint64_t sidExt,
                                   std::shared_ptr<ITunerCallback> cb) {
    // Diagnostic/recovery: when vendor.broadcastradio.dab.scansweep=1, ignore the
    // requested block and sweep the whole Band III table on this one tune, logging
    // sync/snr/services per block, then land on (and select the first service of) the
    // first ensemble that actually decodes. Used to find which mux has signal when the
    // app's restore-tune keeps hitting a dead block. Clear the prop for normal use.
    if (::android::base::GetBoolProperty("vendor.broadcastradio.dab.scansweep", false)) {
        dabSweepWorker(cb);
        return;
    }
    mDabBlockKhz = blockFreqKhz;
    mDabEnsemble = ensemble;
    mDabSidExt = sidExt;
    mCurrentFreqKhz = blockFreqKhz;

    auto& dm = rtlsdr::DeviceManager::getInstance();
    if (!dm.tuneDab(static_cast<int>(blockFreqKhz) * 1000, /*doScan=*/false)) {
        if (cb != nullptr) cb->onTuneFailed(Result::TIMEOUT, utils::makeSelectorDab(sidExt));
        return;
    }

    const uint32_t targetSid = utils::getDabSId(utils::makeSelectorDab(sidExt));
    const auto deadline = std::chrono::steady_clock::now() + kDabTuneTimeout;
    std::string serviceName;
    bool selected = false;
    auto nextDiag = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline && !mDabCancel.load()) {
        // Periodically log demod health so a tune timeout is diagnosable: synced=0
        // throughout means the block never locked (reception/gain), while synced=1 with
        // the target absent means a service-list/decode issue rather than a dead block.
        if (std::chrono::steady_clock::now() >= nextDiag) {
            LOG(INFO) << "DAB tune wait " << blockFreqKhz << " kHz sid=0x" << std::hex
                      << targetSid << std::dec << ": synced=" << dm.dabIsSynced()
                      << " snr=" << dm.dabSnr() << " iqLevel=" << dm.dabIqLevel()
                      << " services=" << dm.dabServices().size();
            nextDiag += std::chrono::seconds(2);
        }
        for (const auto& s : dm.dabServices()) {
            if (s.sid == targetSid) {
                serviceName = s.label;
                if (dm.selectDabService(targetSid)) selected = true;
                break;
            }
        }
        if (selected) break;
        std::this_thread::sleep_for(kDabPollInterval);
    }

    if (cb == nullptr) return;
    if (selected) {
        reportAntennaState(true);  // a successful tune proves the dongle is present
        cb->onCurrentProgramInfoChanged(
                makeDabProgramInfo(sidExt, ensemble, blockFreqKhz, serviceName,
                                   /*nowPlaying=*/true));
        // Tuning decodes the whole ensemble's FIC, so publish every service welle has
        // found on this block as the program list. This is what populates Browse: the
        // app's restore-tune cancels the band-sweep scan, so without this the list stays
        // empty. Further services that arrive as the FIC accumulates are pushed by the
        // DAB update callback (see setTunerCallback).
        emitDabProgramList(cb, /*purge=*/true);
    } else {
        cb->onTuneFailed(mDabCancel.load() ? Result::CANCELED : Result::TIMEOUT,
                         utils::makeSelectorDab(sidExt));
    }
}

void BroadcastRadio::dabSweepWorker(std::shared_ptr<ITunerCallback> cb) {
    auto& dm = rtlsdr::DeviceManager::getInstance();
    LOG(INFO) << "DAB sweep: probing all " << std::size(kDabBlocks) << " Band III blocks";
    for (const auto& block : kDabBlocks) {
        if (mDabCancel.load()) {
            LOG(INFO) << "DAB sweep cancelled";
            return;
        }
        mDabBlockKhz = block.freqKhz;
        if (!dm.tuneDab(static_cast<int>(block.freqKhz) * 1000, /*doScan=*/true)) continue;
        // Wait up to 3 s for OFDM sync (marginal Band III needs longer than a quick probe).
        bool synced = false;
        for (int i = 0; i < 12 && !mDabCancel.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (dm.dabIsSynced()) { synced = true; break; }
        }
        LOG(INFO) << "DAB sweep " << block.label << " @ " << block.freqKhz
                  << " kHz: synced=" << synced << " snr=" << dm.dabSnr()
                  << " iqLevel=" << dm.dabIqLevel();
        if (!synced) continue;
        // Synced: dwell up to 6 s for the FIC to yield services.
        std::vector<rtlsdr::WelleDabSession::ServiceInfo> svcs;
        for (int i = 0; i < 24 && !mDabCancel.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            svcs = dm.dabServices();
            if (!svcs.empty()) break;
        }
        LOG(INFO) << "DAB sweep " << block.label << ": services=" << svcs.size()
                  << " ensemble=\"" << dm.dabEnsembleLabel() << "\" fib=" << dm.dabFibOk() << "/"
                  << dm.dabFibTotal();
        if (svcs.empty()) continue;
        // Found a live ensemble: land here, select its first service, report now-playing,
        // and publish the list so Browse fills in. This is also a full pipeline check.
        const auto& first = svcs.front();
        mDabSidExt = first.sid;
        mDabEnsemble = 0;
        if (dm.selectDabService(first.sid) && cb != nullptr) {
            reportAntennaState(true);
            cb->onCurrentProgramInfoChanged(makeDabProgramInfo(
                    first.sid, 0, block.freqKhz, first.label, /*nowPlaying=*/true));
        }
        if (cb != nullptr) emitDabProgramList(cb, /*purge=*/true);
        LOG(INFO) << "DAB sweep: landed on " << block.label << " (" << block.freqKhz
                  << " kHz), service \"" << first.label << "\" sid=0x" << std::hex << first.sid
                  << std::dec << " of " << svcs.size();
        return;
    }
    LOG(WARNING) << "DAB sweep: no syncable ensemble found across Band III";
    if (cb != nullptr) cb->onTuneFailed(Result::TIMEOUT, utils::makeSelectorDab(0));
}

void BroadcastRadio::dabScanWorker(std::shared_ptr<ITunerCallback> cb) {
    auto& dm = rtlsdr::DeviceManager::getInstance();

    // Start the list fresh.
    ProgramListChunk purge = {};
    purge.purge = true;
    purge.complete = false;
    cb->onProgramListUpdated(purge);

    for (const auto& block : kDabBlocks) {
        if (mDabCancel.load()) break;
        mDabBlockKhz = block.freqKhz;

        if (!dm.tuneDab(static_cast<int>(block.freqKhz) * 1000, /*doScan=*/true)) {
            continue;
        }
        // Give the demod time to acquire null-symbol sync; skip dead blocks.
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        LOG(INFO) << "DAB probe " << block.label << " @ " << block.freqKhz
                  << " kHz: synced=" << dm.dabIsSynced() << " signal=" << dm.dabSignalPresent()
                  << " snr=" << dm.dabSnr() << " iqLevel=" << dm.dabIqLevel();
        if (!dm.dabIsSynced()) continue;

        // Adaptive front-end gain. The optimal RTL-SDR gain for DAB depends on the
        // install (indoor whip vs roof antenna, local interference): too much gain
        // clips the OFDM and *lowers* SNR, too little buries it in the noise floor
        // (measured indoors: 34 dB -> SNR 2.3, 30 dB -> 4.2, 26 dB -> 2.6). On a synced
        // block, sweep candidate gains and keep the one with the best welle SNR before
        // the service-decode dwell; the chosen gain becomes sticky for service tuning.
        // Disable with `setprop vendor.broadcastradio.dab.autogain 0` (then the fixed
        // vendor.broadcastradio.dab.gain is used).
        if (::android::base::GetBoolProperty("vendor.broadcastradio.dab.autogain", true)) {
            static const int kGainSteps[] = {220, 260, 280, 300, 320, 340, 380, 420};
            // welle's per-frame SNR estimate is noisy, so average a few reads per step.
            // Also reject steps that clip the ADC (mean |IQ| near full-scale): clipping
            // destroys the OFDM constellation even when the instantaneous SNR estimate
            // looks fine, so a clean lower-gain step is always preferable on a tie.
            constexpr float kClipIqLevel = 0.85f;
            int bestGain = -1;
            float bestSnr = -1.0e9f;
            for (int g : kGainSteps) {
                if (mDabCancel.load()) break;
                dm.setDabGainTenthDb(g);
                std::this_thread::sleep_for(std::chrono::milliseconds(400));  // settle
                float snrSum = 0.0f;
                float iqMax = 0.0f;
                constexpr int kReads = 3;
                for (int r = 0; r < kReads; ++r) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    snrSum += dm.dabSnr();
                    iqMax = std::max(iqMax, dm.dabIqLevel());
                }
                const float snr = snrSum / kReads;
                const bool clipping = iqMax >= kClipIqLevel;
                LOG(INFO) << "DAB autogain " << block.label << ": gain=" << g
                          << " snr=" << snr << " iqMax=" << iqMax
                          << (clipping ? " (clipping, skipped)" : "");
                // Tie-break toward lower gain: '>' keeps the first (lowest) gain that
                // reaches the peak SNR, which is the cleanest front-end setting.
                if (!clipping && snr > bestSnr) {
                    bestSnr = snr;
                    bestGain = g;
                }
            }
            if (bestGain >= 0 && !mDabCancel.load()) {
                dm.setDabGainTenthDb(bestGain);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                LOG(INFO) << "DAB autogain " << block.label << " best gain=" << bestGain
                          << " (0.1 dB) snr=" << bestSnr;
            }
        }
        // Synced: dwell, polling the FIC for the ensemble's services. Reception is
        // marginal/fading on a Band III whip, so the FIGs that define services may
        // take several seconds (and several CRC-good FIBs) to come through. Keep
        // dwelling until services appear or we hit the budget; the welle service DB
        // accumulates while we stay tuned. Budget tunable via property.
        const int dwellMs =
                ::android::base::GetIntProperty("vendor.broadcastradio.dab.dwellms", 12000);
        const auto dwellDeadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(dwellMs);
        size_t emitted = 0;
        std::string ensembleName;
        while (std::chrono::steady_clock::now() < dwellDeadline && !mDabCancel.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            ensembleName = dm.dabEnsembleLabel();
            std::vector<ProgramInfo> found;
            for (const auto& s : dm.dabServices()) {
                const uint64_t sidExt = s.sid;  // programme SId fits the SID_EXT low bits
                found.push_back(
                        makeDabProgramInfo(sidExt, /*ensemble=*/0, block.freqKhz, s.label));
            }
            if (found.size() > emitted) {
                ProgramListChunk chunk = {};
                chunk.purge = false;
                chunk.complete = false;
                chunk.modified = std::move(found);
                emitted = chunk.modified.size();
                cb->onProgramListUpdated(chunk);
                LOG(INFO) << "DAB scan: " << block.label << " (" << ensembleName << ") "
                          << emitted << " services";
            }
        }
        if (mDabCancel.load()) break;
        LOG(INFO) << "DAB synced " << block.label << ": snr=" << dm.dabSnr()
                  << " iqLevel=" << dm.dabIqLevel() << " fib=" << dm.dabFibOk() << "/"
                  << dm.dabFibTotal() << " services=" << emitted;
    }

    ProgramListChunk done = {};
    done.purge = false;
    done.complete = true;
    cb->onProgramListUpdated(done);
    LOG(INFO) << "DAB ensemble scan complete";
}

void BroadcastRadio::reportAntennaState(bool connected) {
    const int want = connected ? 1 : 0;
    if (mAntennaState.exchange(want) == want) return;  // no change
    std::shared_ptr<ITunerCallback> cb;
    {
        std::lock_guard<std::mutex> lk(mLock);
        cb = mCallback;
    }
    if (cb != nullptr) {
        LOG(INFO) << "antenna " << (connected ? "connected" : "disconnected");
        cb->onAntennaStateChange(connected);
    }
}

void BroadcastRadio::meterLoop() {
    while (mMeterRun.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (!mMeterRun.load()) break;
        std::shared_ptr<ITunerCallback> cb;
        uint32_t freqKhz;
        {
            std::lock_guard<std::mutex> lk(mLock);
            cb = mCallback;
            freqKhz = mCurrentFreqKhz.load();
        }
        if (cb == nullptr || mActiveBand.load() != Band::kAmFm) continue;
        const double pwr = rtlsdr::DeviceManager::getInstance().fmChannelPower();
        LOG(VERBOSE) << "FM meter " << freqKhz << " kHz power=" << pwr
                     << " quality=" << fmSignalQuality(pwr);
        if (pwr > 0.0) reportAntennaState(true);  // IQ flowing => dongle present
        cb->onCurrentProgramInfoChanged(makeFmProgramInfo(freqKhz));
    }
}

void BroadcastRadio::emitDabProgramList(const std::shared_ptr<ITunerCallback>& cb, bool purge) {
    if (cb == nullptr) return;
    auto& dm = rtlsdr::DeviceManager::getInstance();
    const uint32_t blockKhz = mDabBlockKhz.load();
    const auto svc = dm.dabServices();
    if (svc.empty()) return;
    // The update callback fires on every welle change (including frequent DLS text
    // updates), but the Browse list only needs re-publishing when the set of services
    // or their names actually changes. Re-emitting the whole list on every DLS tick
    // churns the app's RecyclerView (it briefly renders unnamed "DAB" rows). Dedup on a
    // (sid:label) signature; a purge (post-tune seed) always publishes.
    std::string sig = std::to_string(blockKhz) + "|";
    for (const auto& s : svc) {
        sig += std::to_string(s.sid) + ':' + s.label + ';';
    }
    {
        std::lock_guard<std::mutex> lk(mLock);
        if (!purge && sig == mLastDabListSig) return;
        mLastDabListSig = std::move(sig);
    }
    std::vector<ProgramInfo> services;
    for (const auto& s : svc) {
        services.push_back(
                makeDabProgramInfo(s.sid, /*ensemble=*/0, blockKhz, s.label));
    }
    ProgramListChunk chunk = {};
    chunk.purge = purge;
    chunk.complete = true;
    chunk.modified = std::move(services);
    const size_t count = chunk.modified.size();
    cb->onProgramListUpdated(chunk);
    LOG(INFO) << "DAB program list: published " << count << " services for block " << blockKhz
              << " kHz (" << dm.dabEnsembleLabel() << ")" << (purge ? " [purge]" : "");
}

// static
uint32_t BroadcastRadio::clampFmKhz(int64_t freqKhz) {
    if (freqKhz < kFmLowerKhz) return kFmLowerKhz;
    if (freqKhz > kFmUpperKhz) return kFmUpperKhz;
    return static_cast<uint32_t>(freqKhz);
}

ndk::ScopedAStatus BroadcastRadio::getAmFmRegionConfig(bool /*full*/,
                                                       AmFmRegionConfig* returnConfigs) {
    AmFmRegionConfig config = {};
    config.ranges = {AmFmBandRange{static_cast<int>(kFmLowerKhz), static_cast<int>(kFmUpperKhz),
                                   static_cast<int>(kFmSpacingKhz),
                                   static_cast<int>(kFmSpacingKhz)}};
    config.fmDeemphasis = AmFmRegionConfig::DEEMPHASIS_D50;  // EU 50us
    config.fmRds = AmFmRegionConfig::RDS;
    *returnConfigs = config;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::getDabRegionConfig(std::vector<DabTableEntry>* returnConfigs) {
    std::vector<DabTableEntry> table;
    table.reserve(std::size(kDabBlocks));
    for (const auto& b : kDabBlocks) {
        table.push_back(DabTableEntry{b.label, static_cast<int>(b.freqKhz)});
    }
    *returnConfigs = std::move(table);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::getImage(int32_t id, std::vector<uint8_t>* returnImage) {
    // id 0 is reserved ("no image"); the framework must never request it.
    if (id == 0) {
        return resultToStatus(Result::INVALID_ARGUMENTS, "image id 0 is reserved");
    }
    std::vector<uint8_t> img;
    if (rtlsdr::DeviceManager::getInstance().dabGetImage(static_cast<uint32_t>(id), img)) {
        LOG(INFO) << __func__ << ": id=" << id << " -> " << img.size() << " bytes";
        *returnImage = std::move(img);
    } else {
        LOG(DEBUG) << __func__ << ": id=" << id << " -> not available";
        *returnImage = {};
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::getProperties(Properties* returnProperties) {
    Properties props = {};
    props.maker = "LineageOS";
    props.product = "RTL-SDR Radio";
    props.version = "1.0";
    props.serial = "rtlsdr";
    // Combined receiver: one module supports both AM/FM and DAB. CarRadioApp builds its
    // band selector from a single module's supported program types, so we must advertise
    // both here for the DAB band to appear in the UI. The shared dongle is arbitrated by
    // DeviceManager (one band streams at a time).
    props.supportedIdentifierTypes = {IdentifierType::AMFM_FREQUENCY_KHZ,
                                      IdentifierType::DAB_SID_EXT, IdentifierType::DAB_ENSEMBLE,
                                      IdentifierType::DAB_FREQUENCY_KHZ};
    *returnProperties = props;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::setTunerCallback(
        const std::shared_ptr<ITunerCallback>& callback) {
    if (callback == nullptr) {
        return resultToStatus(Result::INVALID_ARGUMENTS, "null callback");
    }
    std::lock_guard<std::mutex> lk(mLock);
    mCallback = callback;
    {
        // Push refreshed DAB metadata (DLS / service name) whenever welle reports a change.
        // Harmless on FM (only fires while a DAB service is selected, i.e. mDabSidExt != 0).
        rtlsdr::DeviceManager::getInstance().setDabUpdateCallback([this]() {
            std::shared_ptr<ITunerCallback> cb;
            uint64_t sidExt;
            {
                std::lock_guard<std::mutex> g(mLock);
                cb = mCallback;
                sidExt = mDabSidExt.load();
            }
            if (cb == nullptr) return;
            // Refresh Browse first, and unconditionally: the ensemble's service list (and
            // its FIC labels) fills in over the seconds after a tune. A block-only restore
            // tune leaves sidExt==0, but Browse still needs its label-less "DAB" rows
            // rewritten once welle decodes the names — so this must NOT be gated on a
            // service being selected. Grow, don't purge (the tune already seeded it).
            emitDabProgramList(cb, /*purge=*/false);
            // Now-playing metadata only applies once a specific service is selected.
            if (sidExt == 0) return;
            std::string name;
            for (const auto& s : rtlsdr::DeviceManager::getInstance().dabServices()) {
                if (s.sid == utils::getDabSId(utils::makeSelectorDab(sidExt))) {
                    name = s.label;
                    break;
                }
            }
            cb->onCurrentProgramInfoChanged(makeDabProgramInfo(
                    sidExt, mDabEnsemble.load(), mDabBlockKhz.load(), name,
                    /*nowPlaying=*/true));
        });
    }
    // Report antenna-lost when the dongle disappears mid-stream. Recovery happens on
    // the next tune (open() reacquires the replugged dongle).
    rtlsdr::DeviceManager::getInstance().setUnplugCallback([this]() {
        reportAntennaState(false);
    });
    // Start the FM signal-quality metering thread once a client is attached.
    if (!mMeterRun.exchange(true)) {
        mMeterThread = std::thread([this]() { meterLoop(); });
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::unsetTunerCallback() {
    {
        std::lock_guard<std::mutex> lk(mLock);
        mCallback = nullptr;
        rtlsdr::DeviceManager::getInstance().setDabUpdateCallback(nullptr);
        rtlsdr::DeviceManager::getInstance().setUnplugCallback(nullptr);
    }
    mMeterRun = false;
    if (mMeterThread.joinable()) mMeterThread.join();
    return ndk::ScopedAStatus::ok();
}

ProgramInfo BroadcastRadio::makeFmProgramInfo(uint32_t freqKhz) const {
    ProgramInfo info = {};
    info.selector = utils::makeSelectorAmfm(freqKhz);
    info.logicallyTunedTo = utils::makeIdentifier(IdentifierType::AMFM_FREQUENCY_KHZ, freqKhz);
    info.physicallyTunedTo = info.logicallyTunedTo;
    info.infoFlags = ProgramInfo::FLAG_LIVE | ProgramInfo::FLAG_TUNABLE |
                     ProgramInfo::FLAG_SIGNAL_ACQUISITION;
    auto& dm = rtlsdr::DeviceManager::getInstance();
    if (dm.isStereo()) {
        info.infoFlags |= ProgramInfo::FLAG_STEREO;
    }
    info.signalQuality = fmSignalQuality(dm.fmChannelPower());
    return info;
}

ndk::ScopedAStatus BroadcastRadio::tuneFmLocked(uint32_t freqKhz) {
    // Per the AIDL contract tuning is asynchronous: return ok and report the outcome
    // via the callback. Returning a service-specific error here makes the framework
    // rethrow and crashes CarRadioApp (seen when tuning a dead frequency).
    if (!rtlsdr::DeviceManager::getInstance().tuneFm(freqKhz * 1000)) {
        LOG(WARNING) << "FM tune to " << freqKhz << " kHz failed";
        if (mCallback != nullptr) {
            mCallback->onTuneFailed(Result::TIMEOUT, utils::makeSelectorAmfm(freqKhz));
        }
        return ndk::ScopedAStatus::ok();
    }
    mCurrentFreqKhz = freqKhz;
    if (mCallback != nullptr) {
        mCallback->onCurrentProgramInfoChanged(makeFmProgramInfo(freqKhz));
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::tune(const ProgramSelector& program) {
    stopSeekThread();  // a manual tune supersedes any in-flight scan/seek/dab worker

    // Route by selector: a DAB identifier means the DAB band, otherwise AM/FM.
    const bool isDab = utils::hasId(program, IdentifierType::DAB_SID_EXT) ||
                       utils::hasId(program, IdentifierType::DAB_FREQUENCY_KHZ);
    if (isDab) {
        mActiveBand = Band::kDab;
        if (!utils::hasId(program, IdentifierType::DAB_FREQUENCY_KHZ) ||
            !utils::hasId(program, IdentifierType::DAB_SID_EXT)) {
            std::lock_guard<std::mutex> lk(mLock);
            if (mCallback != nullptr) mCallback->onTuneFailed(Result::INVALID_ARGUMENTS, program);
            return ndk::ScopedAStatus::ok();
        }
        const uint32_t freqKhz =
                static_cast<uint32_t>(utils::getId(program, IdentifierType::DAB_FREQUENCY_KHZ));
        const uint32_t ensemble = static_cast<uint32_t>(
                utils::getId(program, IdentifierType::DAB_ENSEMBLE, 0));
        const uint64_t sidExt =
                static_cast<uint64_t>(utils::getId(program, IdentifierType::DAB_SID_EXT));
        std::shared_ptr<ITunerCallback> cb;
        {
            std::lock_guard<std::mutex> lk(mLock);
            cb = mCallback;
        }
        if (cb == nullptr) return ndk::ScopedAStatus::ok();
        std::lock_guard<std::mutex> g(mSeekMutex);
        mDabThread = std::thread(
                [this, freqKhz, ensemble, sidExt, cb]() {
                    dabTuneWorker(freqKhz, ensemble, sidExt, cb);
                });
        return ndk::ScopedAStatus::ok();
    }

    mActiveBand = Band::kAmFm;
    std::lock_guard<std::mutex> lk(mLock);
    if (!utils::hasId(program, IdentifierType::AMFM_FREQUENCY_KHZ)) {
        if (mCallback != nullptr) mCallback->onTuneFailed(Result::INVALID_ARGUMENTS, program);
        return ndk::ScopedAStatus::ok();
    }
    return tuneFmLocked(clampFmKhz(utils::getAmFmFrequency(program)));
}

ndk::ScopedAStatus BroadcastRadio::step(bool directionUp) {
    stopSeekThread();
    std::lock_guard<std::mutex> lk(mLock);
    if (mActiveBand.load() != Band::kAmFm) {
        // DAB has no frequency step; the app steps through services via seek/program list.
        if (mCallback != nullptr) mCallback->onTuneFailed(Result::NOT_SUPPORTED, ProgramSelector{});
        return ndk::ScopedAStatus::ok();
    }
    int64_t next = static_cast<int64_t>(mCurrentFreqKhz.load()) +
                   (directionUp ? kFmSpacingKhz : -static_cast<int64_t>(kFmSpacingKhz));
    if (next > kFmUpperKhz) next = kFmLowerKhz;
    if (next < kFmLowerKhz) next = kFmUpperKhz;
    return tuneFmLocked(static_cast<uint32_t>(next));
}

ndk::ScopedAStatus BroadcastRadio::seek(bool directionUp, bool /*skipSubChannel*/) {
    // Real signal-based seek: scan the band for the next station. Runs on a worker
    // thread (the sweep can take a few seconds) and reports the result via callback;
    // this is what CarRadioApp's seek buttons and the "scan" feature drive.
    stopSeekThread();  // cancel + join any previous seek
    std::shared_ptr<ITunerCallback> cb;
    Band band;
    {
        std::lock_guard<std::mutex> lk(mLock);
        cb = mCallback;
        band = mActiveBand.load();
    }
    if (cb == nullptr) return ndk::ScopedAStatus::ok();

    if (band == Band::kDab) {
        // DAB seek = step to the next/previous service in the current ensemble.
        auto& dm = rtlsdr::DeviceManager::getInstance();
        const auto svcs = dm.dabServices();
        if (svcs.empty()) {
            cb->onTuneFailed(Result::INVALID_STATE, ProgramSelector{});
            return ndk::ScopedAStatus::ok();
        }
        const uint32_t curSid = utils::getDabSId(utils::makeSelectorDab(mDabSidExt.load()));
        int idx = -1;
        for (size_t i = 0; i < svcs.size(); ++i) {
            if (svcs[i].sid == curSid) { idx = static_cast<int>(i); break; }
        }
        const int n = static_cast<int>(svcs.size());
        const int next = (idx < 0) ? 0 : ((idx + (directionUp ? 1 : n - 1)) % n);
        const auto& s = svcs[next];
        if (dm.selectDabService(s.sid)) {
            mDabSidExt = s.sid;
            cb->onCurrentProgramInfoChanged(
                    makeDabProgramInfo(s.sid, mDabEnsemble.load(), mDabBlockKhz.load(), s.label,
                                       /*nowPlaying=*/true));
        } else {
            cb->onTuneFailed(Result::INVALID_STATE, utils::makeSelectorDab(s.sid));
        }
        return ndk::ScopedAStatus::ok();
    }

    std::lock_guard<std::mutex> g(mSeekMutex);
    mSeekThread = std::thread([this, directionUp, cb]() { seekWorker(directionUp, cb); });
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::cancel() {
    // Abort an in-flight seek or DAB tune/scan; the worker finishes promptly and reports.
    rtlsdr::DeviceManager::getInstance().cancelSeek();
    mDabCancel = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::startProgramListUpdates(const ProgramFilter& filter) {
    // The list is a DAB ensemble scan if we're on the DAB band, or if the client asks
    // for DAB identifiers (CarRadioApp filters the list by the selected band's id types,
    // which is how it requests DAB stations before any tune has happened).
    bool wantDab = mActiveBand.load() == Band::kDab;
    for (const auto& t : filter.identifierTypes) {
        if (t == IdentifierType::DAB_SID_EXT || t == IdentifierType::DAB_ENSEMBLE ||
            t == IdentifierType::DAB_FREQUENCY_KHZ) {
            wantDab = true;
            break;
        }
    }
    if (wantDab) {
        stopSeekThread();  // cancel any in-flight tune/scan
        std::shared_ptr<ITunerCallback> cb;
        {
            std::lock_guard<std::mutex> lk(mLock);
            cb = mCallback;
        }
        if (cb == nullptr) return ndk::ScopedAStatus::ok();
        std::lock_guard<std::mutex> g(mSeekMutex);
        mDabThread = std::thread([this, cb]() { dabScanWorker(cb); });
        return ndk::ScopedAStatus::ok();
    }

    std::lock_guard<std::mutex> lk(mLock);
    if (mCallback != nullptr) {
        // FM has no scan list yet; report an empty, complete list so clients don't wait.
        ProgramListChunk chunk = {};
        chunk.purge = true;
        chunk.complete = true;
        mCallback->onProgramListUpdated(chunk);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::stopProgramListUpdates() {
    // Cancel an in-flight DAB ensemble scan if the client drops the list (e.g. switching away
    // from the DAB band) without first issuing a tune that would preempt it.
    stopSeekThread();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::isConfigFlagSet(ConfigFlag flag, bool* returnIsSet) {
    std::lock_guard<std::mutex> lk(mLock);
    *returnIsSet = (mConfigFlags & (1 << static_cast<int>(flag))) != 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::setConfigFlag(ConfigFlag flag, bool value) {
    std::lock_guard<std::mutex> lk(mLock);
    const int bit = 1 << static_cast<int>(flag);
    if (value) {
        mConfigFlags |= bit;
    } else {
        mConfigFlags &= ~bit;
    }
    if (mCallback != nullptr) mCallback->onConfigFlagUpdated(flag, value);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::setParameters(
        const std::vector<VendorKeyValue>& /*parameters*/,
        std::vector<VendorKeyValue>* returnParameters) {
    *returnParameters = {};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::getParameters(
        const std::vector<std::string>& /*keys*/,
        std::vector<VendorKeyValue>* returnParameters) {
    *returnParameters = {};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BroadcastRadio::registerAnnouncementListener(
        const std::shared_ptr<IAnnouncementListener>& /*listener*/,
        const std::vector<AnnouncementType>& /*enabled*/,
        std::shared_ptr<ICloseHandle>* returnCloseHandle) {
    *returnCloseHandle = nullptr;
    return resultToStatus(Result::NOT_SUPPORTED, "announcements not supported");
}

}  // namespace aidl::android::hardware::broadcastradio
