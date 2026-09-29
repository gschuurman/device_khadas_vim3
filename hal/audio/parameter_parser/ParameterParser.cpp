#define LOG_TAG "vim3_audio_parameter_parser"

#include "ParameterParser.h"

#include <aidl/android/media/audio/common/Float.h>
#include <android-base/logging.h>
#include <media/AudioParameter.h>

namespace vendor::vim3::audio {

using ::aidl::android::media::audio::common::Float;
using ::android::AudioParameter;
using ::android::String8;

namespace {

bool isCarAudioKey(const std::string& key) {
    return key.rfind("car.", 0) == 0 || key.rfind("bus", 0) == 0;
}

}  // namespace

::ndk::ScopedAStatus ParameterParser::parseVendorParameterIds(ParameterScope in_scope,
                                                              const std::string& in_rawKeys,
                                                              std::vector<std::string>* _aidl_return) {
    if (in_scope != ParameterScope::MODULE) return ::ndk::ScopedAStatus::ok();
    AudioParameter params{String8(in_rawKeys.c_str())};
    for (size_t i = 0; i < params.size(); ++i) {
        String8 key, value;
        if (params.getAt(i, key, value) == ::android::OK && isCarAudioKey(key.c_str())) {
            _aidl_return->emplace_back(key.c_str());
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ParameterParser::parseVendorParameters(
        ParameterScope in_scope, const std::string& in_rawKeysAndValues,
        std::vector<VendorParameter>* out_syncParameters, std::vector<VendorParameter>*) {
    if (in_scope != ParameterScope::MODULE) return ::ndk::ScopedAStatus::ok();
    AudioParameter params{String8(in_rawKeysAndValues.c_str())};
    for (size_t i = 0; i < params.size(); ++i) {
        String8 key, value;
        if (params.getAt(i, key, value) != ::android::OK || !isCarAudioKey(key.c_str())) continue;
        float v = 0.0f;
        if (params.getFloat(key, v) != ::android::OK) {
            LOG(WARNING) << "not a number: " << key.c_str() << "=" << value.c_str();
            continue;
        }
        VendorParameter p;
        p.id = key.c_str();
        if (p.ext.setParcelable(Float{v}) != STATUS_OK) continue;
        out_syncParameters->push_back(std::move(p));
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ParameterParser::parseBluetoothA2dpReconfigureOffload(
        const std::string&, std::vector<VendorParameter>*) {
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ParameterParser::parseBluetoothLeReconfigureOffload(
        const std::string&, std::vector<VendorParameter>*) {
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ParameterParser::processVendorParameters(
        ParameterScope in_scope, const std::vector<VendorParameter>& in_parameters,
        std::string* _aidl_return) {
    if (in_scope != ParameterScope::MODULE) return ::ndk::ScopedAStatus::ok();
    AudioParameter result;
    for (const auto& p : in_parameters) {
        std::optional<Float> f;
        if (!isCarAudioKey(p.id) || p.ext.getParcelable(&f) != STATUS_OK || !f.has_value()) {
            continue;
        }
        result.addFloat(String8(p.id.c_str()), f->value);
    }
    *_aidl_return = result.toString().c_str();
    return ::ndk::ScopedAStatus::ok();
}

}  // namespace vendor::vim3::audio
