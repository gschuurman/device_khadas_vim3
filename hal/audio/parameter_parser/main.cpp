#define LOG_TAG "vim3_audio_parameter_parser"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "ParameterParser.h"

int main() {
    auto parser = ndk::SharedRefBase::make<vendor::vim3::audio::ParameterParser>();
    const std::string name =
            std::string(::aidl::android::media::audio::IHalAdapterVendorExtension::descriptor) +
            "/default";
    if (AServiceManager_addService(parser->asBinder().get(), name.c_str()) != STATUS_OK) {
        LOG(ERROR) << "failed to register " << name;
        return EXIT_FAILURE;
    }
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
