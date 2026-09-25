TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

# U-Boot is built from source (u-boot/khadas/vim3, see
# vendor/khadas/vim3/bootloader/Android.mk) per lunch target. Both
# variants share this same device tree/BoardConfig (TARGET_DEVICE=vim3),
# so the split has to happen here rather than via a separate product's
# own BoardConfig. Only the fastboot-flash storage backend differs
# between the two defconfigs (mmc vs nvme) -- the GPT partition layout
# and everything else is shared.
ifeq ($(TARGET_PRODUCT),lineage_vim3_nvme)
TARGET_UBOOT_DEFCONFIG := khadas-vim3_android_ab_nvme_defconfig
else
TARGET_UBOOT_DEFCONFIG := khadas-vim3_android_ab_defconfig
endif

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
# Second WiFi chip (AIC8800D80 USB dongle) loaded via a /vendor/etc/wifi/
# vendor_hals/*.xml descriptor alongside the primary libwifi-hal-bcm.
WIFI_MULTIPLE_VENDOR_HALS := true
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
BOARD_SUPER_PARTITION_SIZE := $(shell echo $$(( 8192 * 1024 * 1024 )))
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
# No BOARD_USERDATAIMAGE_PARTITION_SIZE: userdata is sized dynamically from
# the GPT (size=- in the U-Boot partition table), so the same build image
# self-sizes to whatever's left on the target disk (small eMMC or a much
# larger NVMe SSD) rather than baking in a fixed capacity.
TARGET_COPY_OUT_DATA := data
TARGET_USERIMAGES_USE_F2FS := true
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
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

# Kernel messages on the debug UART (uart_AO, 115200). console=ttynull still comes from the
# GKI boot image; ttyAML0 is added next to it. SERIAL_MESON is a module (loaded in first
# stage), so there is no earlycon: the console registers when meson_uart.ko loads (~0.9s) and
# replays the whole log buffer, so nothing is lost. Costs some boot time at 115200. To turn it
# off again, drop the next line -- the in-memory log (adb shell dmesg) is unaffected.
BOARD_KERNEL_CMDLINE += no_console_suspend console=ttyAML0,115200n8
BOARD_KERNEL_CMDLINE += printk.devkmsg=on
# Debugging the OP-TEE/KeyMint TA-load failure with only the serial console available (boot never gets far
# enough for adb): the default console loglevel filters out KERN_DEBUG lines, which is exactly the level
# tee-supplicant's DMSG() writes at (via stdio_to_kmsg -> /dev/kmsg_debug, see
# vendor/khadas/vim3/optee/tee-supplicant.rc + cfg_tee_supp_log_level=3 in optee.mk). ignore_loglevel prints
# everything live instead of only what would reach `dmesg`. Revert once root-caused.
BOARD_KERNEL_CMDLINE += ignore_loglevel
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
# Bumped from 576M (2026-09-22): /proc/pagetypeinfo showed the CMA zone at 0 free pages of any order --
# fully consumed, most likely by display/GPU (meson-drm + panfrost, /dev/dma_heap/reserved). OP-TEE's
# dynamic-SHM pool for loading TAs draws from this same pool, so TA loads failed with a plain OOM
# (get_rpc_alloc_res / TEEC_ERROR_OUT_OF_MEMORY) whenever nothing was left. Test value; device has 4GB RAM.
# See device/khadas/vim3/handoff-keymint-optee.md.
BOARD_KERNEL_CMDLINE += cma=768M
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
# Boot device = the controller whose block device holds the GPT (ueventd matches this against
# the platform ancestor of the block device: eMMC -> soc/ffe07000.mmc, NVMe SSD on the PCIe
# host -> soc/fc000000.pcie). The fstab suffix picks fstab.vim3.<mmc|nvme>.avb.
ifeq ($(TARGET_PRODUCT),lineage_vim3_nvme)
BOARD_BOOTCONFIG += androidboot.boot_devices=soc/fc000000.pcie
BOARD_BOOTCONFIG += androidboot.fstab_suffix=vim3.nvme.avb
else
BOARD_BOOTCONFIG += androidboot.boot_devices=soc/ffe07000.mmc
BOARD_BOOTCONFIG += androidboot.fstab_suffix=vim3.mmc.avb
endif
BOARD_BOOTCONFIG += androidboot.load_modules_parallel=true

# NVMe (WD SN520, DRAM-less) on the Amlogic DW PCIe host: with these the kernel survived heavy
# first-boot I/O; without them it hit random memory corruption (oopses in unrelated subsystems
# around the time apexd/vendor_dlkm start). Which one is the real fix is not narrowed down yet:
#   - nvme.max_host_mem_size_mb=0  no Host Memory Buffer (the device DMAs into host RAM itself)
#   - pci=pcie_bus_peer2peer       max payload 128 B instead of the 256 B the kernel picks
#   - nvme_core.default_ps_max_latency_us=0 / pcie_aspm=off  no NVMe/PCIe power-state games
ifeq ($(TARGET_PRODUCT),lineage_vim3_nvme)
BOARD_KERNEL_CMDLINE += nvme.max_host_mem_size_mb=0
BOARD_KERNEL_CMDLINE += nvme_core.default_ps_max_latency_us=0
BOARD_KERNEL_CMDLINE += pci=pcie_bus_peer2peer
BOARD_KERNEL_CMDLINE += pcie_aspm=off
endif

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

# Selects the "play store only" GAS feature bundle (no Google Maps, no Google Assistant; neither works on
# this uncertified head unit). The gas_*_overlay modules in vendor/google/gapps_auto (RRO block in
# gapps-core.mk) are static RROs gated by android:requiredSystemPropertyValue against ro.boot.hardware.sku;
# gas_playstore activates GasPlaystoreOverlay (blanks the default assistant, navigation app and speech
# recognizer) and CarLauncherGasPlaystoreOverlay.
BOARD_KERNEL_CMDLINE += androidboot.hardware.sku=gas_playstore

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

ifeq ($(TARGET_PRODUCT),lineage_vim3_nvme)
TARGET_RECOVERY_FSTAB_GENRULE := gen_fstab_vim3_nvme_avb
else
TARGET_RECOVERY_FSTAB_GENRULE := gen_fstab_vim3_mmc_avb
endif

# =============================================================================
# VIM3 kernel — inline build from kernel/khadas/vim3 (GKI android16-6.12 ACK)
# =============================================================================

TARGET_KERNEL_SOURCE := kernel/khadas/vim3
TARGET_KERNEL_CONFIG := \
    gki_defconfig
# amlogic_gki.config is a symlink (in vim3_overlay, our own tree) to the upstream
# arch/arm64/configs/amlogic_gki.fragment — kernel.mk's merge only picks up
# *.config names, so the fragment can't be listed directly here.
TARGET_KERNEL_CONFIG_EXT := \
    kernel/khadas/vim3_overlay/amlogic_gki.config \
    kernel/khadas/vim3_overlay/vim3_extra.config

# AIC8800D80 driver (Ugreen USB WiFi dongle) — no mainline driver exists,
# built out-of-tree via vendor/lineage's external-module Kbuild path.
TARGET_KERNEL_EXT_MODULE_ROOT := kernel/khadas/vim3_overlay/drivers
TARGET_KERNEL_EXT_MODULES := aic8800

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

BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := ext4

BOARD_VIRTUAL_AB_ENABLE := true
BOARD_VIRTUAL_AB_COMPRESSION := true

PRODUCT_VIRTUAL_AB_OTAPREOPT_PAYLOAD := true

DEVICE_PATH := device/khadas/vim3

# # Drop upstream debug/test apps we don't ship. This must live in BoardConfig,
# # NOT a product .mk: inherit-product only appends @inherit: tags to
# # PRODUCT_PACKAGES at parse time (build/make/core/product.mk), and the real
# # module names are substituted later by strip-product-vars in product_config.mk.
# # BoardConfig.mk is included (via envsetup.mk) AFTER product_config.mk resolves
# # those tags, so here PRODUCT_PACKAGES holds real names and filter-out works.
# #   NetworkPreferenceApp       <- packages/services/Car/car_product/build/car.mk
# #   DisplayCompat{Test,Intent}App <- .../displaycompat/display_compat_system.mk (DEBUG)
# PRODUCT_PACKAGES := $(filter-out NetworkPreferenceApp DisplayCompatTestApp DisplayCompatIntentApp, $(PRODUCT_PACKAGES))
# PRODUCT_PACKAGES_DEBUG := $(filter-out NetworkPreferenceApp DisplayCompatTestApp DisplayCompatIntentApp, $(PRODUCT_PACKAGES_DEBUG))


# gms_spoof_*.prop intentionally re-set several ro.build.* keys (release,
# sdk, type, flavor, ...) that gen_build_prop.py already hard-assigns from
# the real product config. Soong's post_process_props rejects duplicate
# hard assignments with differing values by default; since our override
# file is appended after the real values, the last (spoofed) one wins at
# boot once the strict check is relaxed.
BUILD_BROKEN_DUP_SYSPROP := true
# TARGET_SYSTEM_PROP += $(DEVICE_PATH)/gms_spoof_system.prop
# TARGET_PRODUCT_PROP += $(DEVICE_PATH)/gms_spoof_product.prop
# TARGET_VENDOR_PROP += $(DEVICE_PATH)/gms_spoof_vendor.prop
# TARGET_ODM_PROP += $(DEVICE_PATH)/gms_spoof_odm.prop
# TARGET_SYSTEM_EXT_PROP += $(DEVICE_PATH)/gms_spoof_system_ext.prop

# ---------------------------------------------------------------------------
# OP-TEE userspace (vendor/khadas/vim3/optee): tee-supplicant, xtest and the test TAs.
# These must exist before external/optee_test/**/Android.mk are parsed.
# ---------------------------------------------------------------------------
VIM3_OPTEE_MAKE := $(abspath prebuilts/build-tools/linux-x86/bin/make)
VIM3_OPTEE_PATH := /usr/bin:/bin:$(abspath prebuilts/build-tools/linux-x86/bin)
# the from-source OP-TEE build (module u-boot_kvim3_ab_optee) exports the TA dev kit here
VIM3_OPTEE_OBJ = $(PRODUCT_OUT)/obj/UBOOT_OPTEE_OBJ
VIM3_OPTEE_TA_DEV_KIT = $(VIM3_OPTEE_OBJ)/optee/export-ta_arm64
VIM3_OPTEE_FIP_BUILT = $(PRODUCT_OUT)/obj/ETC/u-boot_kvim3_ab_optee_intermediates/u-boot_kvim3_ab_optee.bin
VIM3_OPTEE_TA_SCRIPT := vendor/khadas/vim3/optee/scripts/build-optee-ta.sh
# hook that optee_test's TA Android.mk files include to build a TA
BUILD_OPTEE_MK := vendor/khadas/vim3/optee/build_optee_ta.mk
# xtest's Android.mk takes its headers from $(TA_DEV_KIT_DIR)/host_include and makes every object depend on
# $(OPTEE_BIN): point both at the from-source OP-TEE build so the freshly exported dev kit exists first.
TA_DEV_KIT_DIR = $(abspath $(VIM3_OPTEE_TA_DEV_KIT))
OPTEE_BIN = $(VIM3_OPTEE_FIP_BUILT)
# xtest picks sources with the OP-TEE CFG_* flags at parse time, before the dev kit exists (its conf.mk is only
# read on incremental builds). Preset the one that matters; build-g12b-optee-fip.sh fails if OP-TEE disagrees.
CFG_GP_SOCKETS := y

# Mesa from source (vendor/mesa3d-upstream), only to regenerate the prebuilts in
# vendor/khadas/vim3/gpu/mesa/a73 — see hal/graphics/device_vendor.mk.
ifeq ($(VIM3_MESA_FROM_SOURCE),true)
BOARD_MESA3D_USES_MESON_BUILD := true
# panfrost only: with etnaviv in libgallium_dri, EGL picked the NPU (GC8000,
# no 3D pipe) as the GL device and rendered everything black. Teflon (NPU) is
# built standalone with the NDK instead — see the README.
BOARD_MESA3D_GALLIUM_DRIVERS := panfrost
BOARD_MESA3D_VULKAN_DRIVERS := panfrost
BOARD_MESA3D_BUILD_LIBGBM := true
# panfrost/panvk precompile CL shaders: host tools built natively from the same
# source (see vendor/khadas/vim3/gpu/mesa/README.md), found via a native file.
MESA3D_HOST_TOOLS ?= $(HOME)/android/mesa-host-tools
BOARD_MESA3D_MESON_ARGS := -Dmesa-clc=system -Dprecomp-compiler=system \
    --native-file $(MESA3D_HOST_TOOLS)/native.ini \
    --cross-file $(MESA3D_HOST_TOOLS)/cross-python.ini
# meson >= 1.4 + mako/yaml/ply/pycparser live in a venv (see the README).
MESA3D_HOST_PATH ?= $(HOME)/android/teflon/venv/bin
endif
