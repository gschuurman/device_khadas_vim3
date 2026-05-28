TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

BOARD_KERNEL_IMAGE_NAME := Image.lz4

TARGET_SELINUX_ENFORCE := false


include device/amlogic/yukawa/BoardConfig.mk

# ---------------------------------------------------------------------------
# Kernel — inline build from kernel/khadas/vim3 (GKI android16-6.12 ACK)
# ---------------------------------------------------------------------------
TARGET_KERNEL_SOURCE := kernel/khadas/vim3
TARGET_KERNEL_CONFIG := \
    gki_defconfig \
    amlogic_gki.config
TARGET_KERNEL_CONFIG_EXT := kernel/khadas/vim3_overlay/vim3_extra.config

# DTB path within KERNEL_OUT for boot image assembly
TARGET_DTB_LIST_WILDCARD := arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3

# DTBO — pack from the dtbo compiled as part of the main kernel build
# (dtb-y += meson-g12b-a311d-khadas-vim3-android.dtbo in the amlogic DTS Makefile)
TARGET_NEEDS_DTBOIMAGE := true
BOARD_CUSTOM_DTBOIMG_MK := device/khadas/vim3/build/tasks/dtboimage.mk
BOARD_PREBUILT_DTBOIMAGE = $(TARGET_OUT_INTERMEDIATES)/dtbo-kernel.img

# ---------------------------------------------------------------------------
# Kernel modules — aligned with vim3_overlay/BUILD.bazel module lists
# ---------------------------------------------------------------------------
BOOT_KERNEL_MODULES := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_boot))

BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_boot))

SYSTEM_KERNEL_MODULES := $(strip $(shell cat device/khadas/vim3/modules.load.system_dlkm))
BOARD_SYSTEM_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.system_dlkm))

BOARD_VENDOR_KERNEL_MODULES_LOAD := $(strip $(shell cat device/khadas/vim3/modules.load.vendor_dlkm))

# Clear prebuilt-path module vars set by yukawa/BoardConfig.mk;
# the inline kernel build populates partitions via BOOT/SYSTEM/VENDOR_KERNEL_MODULES above.
BOARD_VENDOR_RAMDISK_KERNEL_MODULES :=
BOARD_VENDOR_DLKM_MODULES :=
BOARD_SYSTEM_DLKM_MODULES :=
BOARD_VENDOR_KERNEL_MODULES :=

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

# Tells the build to expect VAB payload pre-optimization
PRODUCT_VIRTUAL_AB_OTAPREOPT_PAYLOAD := true

# BOARD_SUPER_PARTITION_SIZE := $(shell echo $$(( 6144 * 1024 * 1024 )))
# BOARD_DB_DYNAMIC_PARTITIONS_SIZE := $(shell echo $$(( $(BOARD_SUPER_PARTITION_SIZE)/2 - (10 * 1024 * 1024) )))  # Reserve 10M for DAP metadata

# BOARD_KERNEL_CMDLINE += console=ttyS4,115200

DEVICE_PATH := device/khadas/vim3

BOARD_VENDOR_KERNEL_MODULES_BLOCKLIST := device/khadas/vim3/modules.blocklist

# TARGET_SYSTEM_PROP += $(DEVICE_PATH)/gms_spoof_system.prop
# TARGET_PRODUCT_PROP += $(DEVICE_PATH)/gms_spoof_product.prop