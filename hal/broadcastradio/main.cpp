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

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "BroadcastRadio.h"

using ::aidl::android::hardware::broadcastradio::BroadcastRadio;

namespace {
void registerInstance(const std::shared_ptr<BroadcastRadio>& radio, const std::string& suffix) {
    const std::string name = std::string(BroadcastRadio::descriptor) + "/" + suffix;
    binder_status_t status = AServiceManager_addService(radio->asBinder().get(), name.c_str());
    CHECK_EQ(status, STATUS_OK) << "Failed to register " << name;
    LOG(INFO) << "registered " << name;
}
}  // namespace

int main() {
    android::base::SetDefaultTag("BcRadioRtlSdr");
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    auto amfm = ::ndk::SharedRefBase::make<BroadcastRadio>(BroadcastRadio::Band::kAmFm);
    registerInstance(amfm, "amfm");

    auto dab = ::ndk::SharedRefBase::make<BroadcastRadio>(BroadcastRadio::Band::kDab);
    registerInstance(dab, "dab");

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should never reach here
}
