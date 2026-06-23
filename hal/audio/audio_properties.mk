# Audio properties for VIM3 (Amlogic A311D / yukawa platform)
#
# These properties configure the BayLibre Generic Audio HAL for Yukawa.

# ALSA card and device configuration
# Primary resolution is by NAME (PrimaryMixer::getAlsaCard greps /proc/asound/cards).
# Card indices are also pinned in BoardConfig.mk (snd_usb_audio ... index=5,6; snd_aloop
# index=7) so ICUSBAUDIO7D=5, MS210x=6, Loopback=7, onboard axg=0. The explicit
# persist.vendor.audio.primary.card=5 is the FALLBACK if name resolution ever misses, so we
# fall back to ICUSBAUDIO7D (card 5) rather than the HAL default card 0 (= onboard axg).

AUDIO_CARD_NAME := ICUSBAUDIO7D

PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.primary.card_name=$(AUDIO_CARD_NAME) \
    persist.vendor.audio.primary.card=5 \
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