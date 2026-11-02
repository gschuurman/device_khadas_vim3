# Audio properties for VIM3 (Amlogic A311D / yukawa platform)
#
# These properties configure the BayLibre Generic Audio HAL for Yukawa.

# ALSA card and device configuration
# Primary resolution is by NAME (PrimaryMixer::getAlsaCard greps /proc/asound/cards).
# Card indices are also pinned in BoardConfig.mk (snd_usb_audio ... index=5,6; snd_aloop
# index=7) so ICUSBAUDIO7D=5, MS210x=6, Loopback=7, onboard axg=0. The explicit
# persist.vendor.audio.primary.card=<n> is the FALLBACK if name resolution ever misses.
#
# VIM3_AUDIO_OUTPUT (set in hal/audio/device_vendor.mk, also read by BoardConfig.mk):
#   module: rev 0.6 car audio module on the onboard axg card ("KHADAS-VIM3", id KHADASVIM3).
#           Playback PCM 0 = FRDDR_A -> TDM-B (front + rear PCM5102A), capture PCM 3 =
#           TODDR_A <- TDM-B lane 3 (PCM1808 mic ADC, mic on the left channel). The FRDDR/TDM
#           routing comes from mixer_controls.xml.
#   usb:    the old ICUSBAUDIO7D 7.1 USB card.

ifeq ($(VIM3_AUDIO_OUTPUT),usb)
AUDIO_CARD_NAME := ICUSBAUDIO7D
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.primary.card_name=$(AUDIO_CARD_NAME) \
    persist.vendor.audio.primary.card=5 \
    persist.vendor.audio.primary.device=0
else
AUDIO_CARD_NAME := KHADASVIM3
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.audio.primary.card_name=$(AUDIO_CARD_NAME) \
    persist.vendor.audio.primary.card=0 \
    persist.vendor.audio.primary.device=0 \
    persist.vendor.audio.mic.card_name=$(AUDIO_CARD_NAME) \
    persist.vendor.audio.mic.device=3 \
    ro.vendor.audio.mic.channel=left
# Optional digital mic gain in dB (default 0: the MAX9814's AGC sets the level).
#   ro.vendor.audio.mic.gain_db=6
endif

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
    persist.vendor.audio.use_aidl=true

# Product: vendor_init may not set these (dropped from vendor build.prop under SELinux enforcing).
PRODUCT_PRODUCT_PROPERTIES += \
    persist.audio.car_audio_service.enabled=true \
    audio.usb.enabled=true