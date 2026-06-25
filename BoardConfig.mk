TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

BOARD_KERNEL_IMAGE_NAME := Image

TARGET_SELINUX_ENFORCE := false

# =============================================================================
# Platform base config (VIM3 / yukawa platform)
# =============================================================================

# Primary Arch
TARGET_ARCH := arm64
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=

# VIM3 (S922X): Cortex-A73 + A53 big.LITTLE, ARMv8-A
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_VARIANT := cortex-a73

TARGET_IS_64_BIT := true
BOARD_BOOT_HEADER_VERSION := 4
BOARD_INIT_BOOT_HEADER_VERSION := 4
PRODUCT_INIT_BOOT_IMAGE_HEADER_VERSION := 4

# Puts odex files on system_other, as well as causing dex files not to get
# stripped from APKs.
BOARD_USES_SYSTEM_OTHER_ODEX := true

TARGET_BOOTLOADER_VERSION ?= 2025.10

TARGET_BOARD_PLATFORM := yukawa
TARGET_BOARD_INFO_FILE := device/khadas/vim3/board-info/board-info-vim3.txt

# Vulkan
BOARD_INSTALL_VULKAN := true

# OpenCL
BOARD_INSTALL_OPENCL := true

# BT configs
BOARD_HAVE_BLUETOOTH := true

# WiFi - mainline brcmfmac kernel driver (BCM4359).
# IMPORTANT: do NOT use lib_driver_cmd_bcmdhd here. That private-command library
# issues bcmdhd-proprietary ioctls (SIOCDEVPRIVATE+1: SET_AP_WPS_P2P_IE, COUNTRY,
# BTCOEXMODE, SETSUSPENDMODE, ...) that the brcmfmac driver does not implement, so
# every call fails. After DRV_NUMBER_SEQUENTIAL_ERRORS failures the lib raises
# CTRL-EVENT-DRIVER-STATE HANGED. STA tolerates it, but during WiFi-Direct GO/AP
# bring-up the SET_AP_WPS_P2P_IE failure + HANGED aborts hostapd interface setup
# ("Unable to setup interface" -> P2P-GROUP-FORMATION-FAILURE), breaking wireless
# Android Auto. lib_driver_cmd_fallback is a no-op stub (cmds return success, no
# ioctl, no HANGED); WPS/P2P IEs then ride the standard NL80211 beacon brcmfmac
# supports. (Root-caused live 2026-06-16; see project-wireless-android-auto memory.)
# lib_driver_cmd_fallback lives in its own soong_namespace
# (external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant), which the root namespace
# cannot resolve by bare name. That namespace also redefines the wpa_supplicant/hostapd
# binaries, so it can NOT be added to PRODUCT_SOONG_NAMESPACES (kati: "hostapd already
# defined"). Reference the lib by fully-qualified //namespace:module path instead, as
# device/generic/goldfish does for its private lib.
BOARD_WLAN_DEVICE := bcmdhd
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := //external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant:lib_driver_cmd_fallback
BOARD_HOSTAPD_PRIVATE_LIB := //external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant:lib_driver_cmd_fallback
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_HOSTAPD_DRIVER := NL80211
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true

# Treble
PRODUCT_FULL_TREBLE := true
BOARD_VNDK_VERSION := current

# AVB
BOARD_AVB_ENABLE := true
BOARD_AVB_ALGORITHM := SHA256_RSA4096
BOARD_AVB_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_KEY_PATH := device/khadas/vim3/avb/vim3_avb.pem

TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := false

# Boot Image v4 support
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := true
BOARD_MOVE_GSI_AVB_KEYS_TO_VENDOR_BOOT := true

# GKI-related variables.
BOARD_USES_GENERIC_KERNEL_IMAGE := true

# No recovery
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE :=

# AB Support
AB_OTA_UPDATER := true
AB_OTA_PARTITIONS += \
    boot \
    dtbo \
    system \
    vendor \
    vendor_boot \
    init_boot \
    vendor_dlkm \
    system_dlkm \
    vbmeta \
    vbmeta_vendor_dlkm \
    vbmeta_system_dlkm

# FS Configuration
BOARD_BOOTIMAGE_PARTITION_SIZE := $(shell echo $$(( 64 * 1024 * 1024 )))
BOARD_DTBOIMG_PARTITION_SIZE := $(shell echo $$(( 8 * 1024 * 1024 )))
TARGET_COPY_OUT_VENDOR := vendor

# Super partition
TARGET_USE_DYNAMIC_PARTITIONS := true
BOARD_BUILD_SUPER_IMAGE_BY_DEFAULT := true
BOARD_SUPER_PARTITION_GROUPS := db_dynamic_partitions
BOARD_DB_DYNAMIC_PARTITIONS_PARTITION_LIST := system vendor
BOARD_SUPER_PARTITION_SIZE := $(shell echo $$(( 4096 * 1024 * 1024 )))
BOARD_DB_DYNAMIC_PARTITIONS_SIZE := $(shell echo $$(( $(BOARD_SUPER_PARTITION_SIZE)/2 - (10 * 1024 * 1024) )))

BOARD_USES_METADATA_PARTITION := true

# Vendor DLKM partition
BOARD_USES_VENDOR_DLKMIMAGE := true
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm
BOARD_DB_DYNAMIC_PARTITIONS_PARTITION_LIST += vendor_dlkm

# System DLKM partition
BOARD_USES_SYSTEM_DLKMIMAGE := true
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
BOARD_DB_DYNAMIC_PARTITIONS_PARTITION_LIST += system_dlkm

# Userdata partition
TARGET_COPY_OUT_DATA := data
TARGET_USERIMAGES_USE_F2FS := true
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
BOARD_USERDATAIMAGE_PARTITION_SIZE := $(shell echo $$(( 10240 * 1024 * 1024 )))
TARGET_USERIMAGES_SPARSE_F2FS_DISABLED ?= false

# Recovery
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
BOARD_AVB_RECOVERY_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_RECOVERY_ALGORITHM := SHA256_RSA2048
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 2
TARGET_NO_RECOVERY := true
TARGET_RECOVERY_WIPE := device/khadas/vim3/recovery.wipe

BOARD_INCLUDE_RECOVERY_DTBO := true

# Init Boot partition
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 0x800000
BOARD_MKBOOTIMG_INIT_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := $(shell echo $$(( 32 * 1024 * 1024 )))

BOARD_KERNEL_OFFSET      := 0x1080000
BOARD_KERNEL_TAGS_OFFSET := 0x1000000
BOARD_RAMDISK_OFFSET     := 0x4000000
BOARD_INCLUDE_DTB_IN_BOOTIMG := true
BOARD_MKBOOTIMG_ARGS := --kernel_offset $(BOARD_KERNEL_OFFSET)
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset $(BOARD_RAMDISK_OFFSET)
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_MKBOOTIMG_ARGS += --pagesize 4096

BOARD_KERNEL_CMDLINE += no_console_suspend console=ttyAML0,115200 earlycon
BOARD_KERNEL_CMDLINE += printk.devkmsg=on
BOARD_KERNEL_CMDLINE += init=/init
BOARD_KERNEL_CMDLINE += firmware_class.path=/vendor/firmware
BOARD_KERNEL_CMDLINE += log_buf_len=1M
# Disable: FWSUP (0x2000) to fix WPA2 handshake, SAE (0x80000) unsupported by 2017 fw, WOWL (0x8) causes scan storms
BOARD_KERNEL_CMDLINE += brcmfmac.feature_disable=0x82008
# Do NOT set brcmfmac.p2pon=1. It does NOT fix WiFi-Direct (tested live 2026-06-15):
# on this firmware (BCM4359, brcmfmac 2017) p2pon=1 does not create a static p2p0
# netdev at init, and even p2pon=1 plus a manually-created p2p0 still fails to bring
# P2P up. The real WiFi-Direct fix is wifi.direct.interface=p2p-dev-wlan0, set in
# hal/connectivity/device_vendor.mk (the brcmfmac P2P-device is a non-netdev wdev, not
# a p2p0 netdev). Leave p2pon unset so brcmfmac uses its dynamic P2P-device model.
BOARD_KERNEL_CMDLINE += cma=576M
# Pin ALSA card indices so they're deterministic across boots/replug. snd-aloop and
# snd-usb-audio are both built-in (=y), so their module params go on the kernel cmdline.
# snd-aloop (native radio loopback) -> card 7. USB audio pinned by VID:PID:
#   ICUSBAUDIO7D (0d8c:0102, car media output) -> card 5
#   MS210x       (534d:0021, HDMI capture)      -> card 6
# Onboard axg sound card loads later as a module and takes a low free index (0).
# NOTE: media output is also name-resolved (persist.vendor.audio.primary.card_name) so it
# tracks ICUSBAUDIO7D regardless; these pins just make the whole layout predictable.
BOARD_KERNEL_CMDLINE += snd_aloop.index=7
# Slave the loopback's PCM timer to the USB sound card (card 5, pcm0, sub0) instead of letting
# it free-run on the jiffies system timer. The radio HAL writes decoded audio into the loopback
# and the audio HAL captures it for the speakers; with an independent timer the loopback's 48kHz
# drifts against the USB card's real 48kHz, periodically over/underrunning the ~85ms capture
# buffer -> DAB audio stutters then "catches up" fast. Slaving locks both to one clock.
BOARD_KERNEL_CMDLINE += snd_aloop.timer_source=5.0.0
BOARD_KERNEL_CMDLINE += snd_usb_audio.vid=0x0d8c,0x534d snd_usb_audio.pid=0x0102,0x0021 snd_usb_audio.index=5,6
BOARD_BOOTCONFIG += androidboot.hardware=vim3
BOARD_BOOTCONFIG += androidboot.boot_devices=soc/ffe07000.mmc
BOARD_BOOTCONFIG += androidboot.fstab_suffix=vim3.mmc.avb
BOARD_BOOTCONFIG += androidboot.load_modules_parallel=true

ifneq ($(TARGET_SELINUX_ENFORCE), true)
BOARD_BOOTCONFIG += androidboot.selinux=permissive
endif
ifeq ($(TARGET_BUILTIN_EDID), true)
BOARD_KERNEL_CMDLINE += drm.edid_firmware=edid/1920x1080.bin
endif
ifneq ($(TARGET_SENSOR_MEZZANINE),)
BOARD_KERNEL_CMDLINE += overlay_mgr.overlay_dt_entry=hardware_cfg_$(TARGET_SENSOR_MEZZANINE)
endif
ifneq ($(TARGET_MEM_SIZE),)
BOARD_KERNEL_CMDLINE += mem=$(TARGET_MEM_SIZE)
endif
ifneq ($(TARGET_KERNEL_CFG),)
BOARD_KERNEL_CMDLINE += $(TARGET_KERNEL_CFG)
endif

BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := build/make/target/board/mainline_arm64/bluetooth

BOARD_VENDOR_SEPOLICY_DIRS += \
    device/khadas/vim3/sepolicy-vendor

PRODUCT_PRIVATE_SEPOLICY_DIRS += \
    device/khadas/vim3/sepolicy-private

DEVICE_MANIFEST_FILE += device/khadas/vim3/manifest.xml

# Enable chained vbmeta for boot images
BOARD_AVB_BOOT_KEY_PATH := device/khadas/vim3/avb/vim3_avb.pem
BOARD_AVB_BOOT_ALGORITHM := SHA256_RSA4096
BOARD_AVB_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 2

# Enable chained vbmeta for init_boot images
BOARD_AVB_INIT_BOOT_KEY_PATH := device/khadas/vim3/avb/vim3_avb.pem
BOARD_AVB_INIT_BOOT_ALGORITHM := SHA256_RSA4096
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_INIT_BOOT_ROLLBACK_INDEX_LOCATION := 3

# Enabled chained vbmeta for vendor_dlkm
BOARD_AVB_VBMETA_CUSTOM_PARTITIONS := vendor_dlkm system_dlkm
BOARD_AVB_VBMETA_VENDOR_DLKM := vendor_dlkm
BOARD_AVB_VBMETA_VENDOR_DLKM_KEY_PATH := device/khadas/vim3/avb/vim3_avb.pem
BOARD_AVB_VBMETA_VENDOR_DLKM_ALGORITHM := SHA256_RSA4096
BOARD_AVB_VBMETA_VENDOR_DLKM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_VENDOR_DLKM_ROLLBACK_INDEX_LOCATION := 4

# Enabled chained vbmeta for system_dlkm
BOARD_AVB_VBMETA_SYSTEM_DLKM := system_dlkm
BOARD_AVB_VBMETA_SYSTEM_DLKM_KEY_PATH := device/khadas/vim3/avb/vim3_avb.pem
BOARD_AVB_VBMETA_SYSTEM_DLKM_ALGORITHM := SHA256_RSA4096
BOARD_AVB_VBMETA_SYSTEM_DLKM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_DLKM_ROLLBACK_INDEX_LOCATION := 5

BOARD_AVB_SYSTEM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256

TARGET_RECOVERY_FSTAB_GENRULE := gen_fstab_vim3_mmc_avb

# =============================================================================
# VIM3 kernel — inline build from kernel/khadas/vim3 (GKI android16-6.12 ACK)
# =============================================================================

TARGET_KERNEL_SOURCE := kernel/khadas/vim3
TARGET_KERNEL_CONFIG := \
    gki_defconfig \
    amlogic_gki.config
TARGET_KERNEL_CONFIG_EXT := kernel/khadas/vim3_overlay/vim3_extra.config

# Paths relative to $(DTB_OUT)/arch/arm64/boot/dts/ — order matters: U-Boot adtb_idx=1 selects index 1.
# Mirrors original vendor_boot layout: VIM3L at index 0, VIM3 (HDMI) at index 1.
TARGET_DTB_LIST_WILDCARD := amlogic/meson-sm1-khadas-vim3l amlogic/meson-g12b-a311d-khadas-vim3

# DTBO — pack from the dtbo compiled as part of the main kernel build
TARGET_NEEDS_DTBOIMAGE := true
BOARD_CUSTOM_DTBOIMG_MK := device/khadas/vim3/build/dtboimage.mk
BOARD_PREBUILT_DTBOIMAGE = $(TARGET_OUT_INTERMEDIATES)/dtbo-kernel.img

# ---------------------------------------------------------------------------
# Kernel modules — aligned with vim3_overlay/BUILD.bazel module lists
# ---------------------------------------------------------------------------
BOOT_KERNEL_MODULES := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_boot))

BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_boot))

SYSTEM_KERNEL_MODULES := $(strip $(shell cat device/khadas/vim3/modules.load.system_dlkm))
BOARD_SYSTEM_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.system_dlkm))

BOARD_VENDOR_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_dlkm))

# Inline kernel build — clear any prebuilt-path module vars
BOARD_VENDOR_RAMDISK_KERNEL_MODULES :=
BOARD_VENDOR_DLKM_MODULES :=
BOARD_SYSTEM_DLKM_MODULES :=
BOARD_VENDOR_KERNEL_MODULES :=

# =============================================================================
# VIM3 sepolicy, filesystem, and partition config
# =============================================================================

BOARD_SEPOLICY_DIRS += \
    device/khadas/vim3/sepolicy

BOARD_VENDOR_SEPOLICY_DIRS += \
    vendor/gschuurman/vehicle_interfaces/usb_gnss_hal/android.hardware.gnss-service.usb/sepolicy/vendor

BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
TARGET_USERIMAGES_USE_EROFS := true
BOARD_EROFS_COMPRESSOR := lz4hc
BOARD_EROFS_PCLUSTER_SIZE := 65536

BOARD_VIRTUAL_AB_ENABLE := true
BOARD_VIRTUAL_AB_COMPRESSION := true

PRODUCT_VIRTUAL_AB_OTAPREOPT_PAYLOAD := true

DEVICE_PATH := device/khadas/vim3

BOARD_VENDOR_KERNEL_MODULES_BLOCKLIST := device/khadas/vim3/modules.blocklist

# TARGET_SYSTEM_PROP += $(DEVICE_PATH)/gms_spoof_system.prop
# TARGET_PRODUCT_PROP += $(DEVICE_PATH)/gms_spoof_product.prop
