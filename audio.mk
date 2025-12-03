# audio.mk - CM6206 5.1 USB Audio Config (Software Mixing)
# Include via: $(call inherit-product, device/.../audio.mk)

LOCAL_AUDIO_PATH := device/khadas/vim3/audio

# Packages voor USB Audio HAL en mixer tools
PRODUCT_PACKAGES += \
    audio.usb.default \
    tinymix \
    tinyplay \
    tinycap

# Audio Properties
# We forceren USB als primaire output en activeren AAudio
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hardware.audio.primary=usb \
    persist.audio.use_usb_audio_hal=true \
    aaudio.mmap_policy=2 \
    aaudio.mmap_exclusive_policy=2

# Config files kopiëren naar vendor/etc
PRODUCT_COPY_FILES += \
    $(LOCAL_AUDIO_PATH)/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(LOCAL_AUDIO_PATH)/car_audio_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/car_audio_configuration.xml \
    $(LOCAL_AUDIO_PATH)/setup_mixer.sh:system/bin/setup_mixer.sh \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/a2dp_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
	$(LOCAL_AUDIO_PATH)/init.audio.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.audio.rc