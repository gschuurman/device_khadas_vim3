# BayLibre Generic Audio HAL configuration for Yukawa

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.low_latency.xml

PRODUCT_PACKAGES += \
    tinyplay2 \
    tinycap2 \
    tinymix2 \
    tinypcminfo2 \
    cplay

# audio policy configuration
USE_XML_AUDIO_POLICY_CONF := 1

# Generic Audio HAL AIDL packages
PRODUCT_PACKAGES += \
    com.android.hardware.audio.generic \
    android.hardware.bluetooth.audio-impl

PRODUCT_PACKAGES += \
    android.hardware.automotive.audiocontrol-service \

# EQ / bassboost / virtualizer / loudness control panel (attached to bus0_media_out
# via deviceEffects in audio_effects.xml; given a launcher entry since there's no
# stock music player here to fire DISPLAY_AUDIO_EFFECT_CONTROL_PANEL)
PRODUCT_PACKAGES += \
    MusicFX \

# PRODUCT_PACKAGES += \
#     CarAudioTuner \

# Copy BayLibre audio configuration files for Yukawa
PRODUCT_COPY_FILES += \
    device/khadas/vim3/hal/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    device/khadas/vim3/hal/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    device/khadas/vim3/hal/audio/mixer_controls.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_controls.xml \
    device/khadas/vim3/hal/audio/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects_config.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    device/khadas/vim3/hal/audio/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    device/khadas/vim3/hal/audio/bluetooth_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_audio_policy_configuration.xml \
    device/khadas/vim3/hal/audio/audio_policy_engine_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_engine_configuration.xml \
    device/khadas/vim3/hal/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths.xml \
    vendor/gschuurman/vehicle_interfaces/automotive/audiocontrol/conf/car_audio_fade_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/car_audio_fade_configuration.xml \

PRODUCT_COPY_FILES += \
    device/khadas/vim3/hal/audio/car_audio_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/car_audio_configuration.xml
#     device/khadas/vim3/hal/audio/usb_audio_policy_configuration_vim3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration_vim3.xml \

# Include BayLibre Audio HAL properties
-include device/khadas/vim3/hal/audio/audio_properties.mk