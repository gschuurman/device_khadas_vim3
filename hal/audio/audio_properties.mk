# Audio properties for Amlogic Yukawa (Khadas VIM3/VIM3L)
#
# These properties configure the BayLibre Generic Audio HAL for Yukawa.

# ALSA card and device configuration
# Use card name for dynamic detection (handles USB devices changing card indices)
# Fall back to card 0, device 0 if name not found

AUDIO_CARD_NAME := ICUSBAUDIO7D

PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.primary.card_name=$(AUDIO_CARD_NAME) \
    persist.vendor.audio.primary.device=0

# Mixer controls configuration file location
# Points to the Yukawa-specific mixer controls XML
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.mixer.config=/vendor/etc/mixer_controls.xml

# Audio HAL debug (set to true for verbose logging)
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.hal.debug=false

# Enable audio modules
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.usb.enabled=true \
    persist.vendor.audio.bluetooth.enabled=true \
    persist.vendor.audio.rsubmix.enabled=true

# Amlogic-specific audio properties
# HDMI audio configuration
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.audio.hdmi.enabled=true

# Sample rate and buffer configuration
# Amlogic G12A supports up to 192kHz for HDMI
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.audio.default.sample_rate=48000 \
    ro.vendor.audio.hdmi.sample_rates=48000,96000,192000

PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.audio.period_size=1024 \
    ro.vendor.audio.period_count=8

PRODUCT_PROPERTY_OVERRIDES += \
    ro.hardware.type=automotive \
    persist.audio.car_audio_service.enabled=true \
    audio.usb.enabled=true \
    persist.vendor.audio.use_aidl=true