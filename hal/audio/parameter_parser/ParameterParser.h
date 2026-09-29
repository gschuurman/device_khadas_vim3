#pragma once

#include <aidl/android/media/audio/BnHalAdapterVendorExtension.h>

namespace vendor::vim3::audio {

// Converts the car audio parameters between "key=value" strings and
// VendorParameters holding an android.media.audio.common.Float. Keys:
// "car.balance", "car.fader", "car.eq.enabled", "car.eq.band<N>" and the bus
// gains "bus<N>_<name>". Other keys are left to the framework.
class ParameterParser : public ::aidl::android::media::audio::BnHalAdapterVendorExtension {
  private:
    using ParameterScope = ::aidl::android::media::audio::IHalAdapterVendorExtension::ParameterScope;
    using VendorParameter = ::aidl::android::hardware::audio::core::VendorParameter;

    ::ndk::ScopedAStatus parseVendorParameterIds(ParameterScope in_scope,
                                                 const std::string& in_rawKeys,
                                                 std::vector<std::string>* _aidl_return) override;
    ::ndk::ScopedAStatus parseVendorParameters(
            ParameterScope in_scope, const std::string& in_rawKeysAndValues,
            std::vector<VendorParameter>* out_syncParameters,
            std::vector<VendorParameter>* out_asyncParameters) override;
    ::ndk::ScopedAStatus parseBluetoothA2dpReconfigureOffload(
            const std::string& in_rawValue, std::vector<VendorParameter>* _aidl_return) override;
    ::ndk::ScopedAStatus parseBluetoothLeReconfigureOffload(
            const std::string& in_rawValue, std::vector<VendorParameter>* _aidl_return) override;
    ::ndk::ScopedAStatus processVendorParameters(ParameterScope in_scope,
                                                 const std::vector<VendorParameter>& in_parameters,
                                                 std::string* _aidl_return) override;
};

}  // namespace vendor::vim3::audio
