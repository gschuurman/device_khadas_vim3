TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3

# =============================================================================
# Core product setup
# =============================================================================

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)

ifndef TARGET_KERNEL_USE
TARGET_KERNEL_USE := 6.12
endif

# =============================================================================
# Feature areas
# =============================================================================

$(call inherit-product, device/khadas/vim3/car.mk)
$(call inherit-product, device/khadas/vim3/vehicle.mk)
$(call inherit-product, device/khadas/vim3/wireless.mk)
$(call inherit-product, device/khadas/vim3/gnss.mk)
$(call inherit-product, device/khadas/vim3/telephony.mk)
$(call inherit-product, device/khadas/vim3/developer.mk)

# =============================================================================
# Vendor binary packages (WiFi/BT firmware, GPU, video, bootloader)
# =============================================================================

include device/khadas/vim3/vendor-package-ver.mk
$(call inherit-product-if-exists, $(VENDOR_VIM3_PATH)/vendor.mk)

# =============================================================================
# Soong namespaces
# =============================================================================

PRODUCT_SOONG_NAMESPACES += device/khadas/vim3
PRODUCT_SOONG_NAMESPACES += hardware/amlogic/yukawa

PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false
PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true

# =============================================================================
# Platform runtime and feature base
# =============================================================================

$(call inherit-product, $(SRC_TARGET_DIR)/product/updatable_apex.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/android_t_baseline.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/developer_gsi_keys.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

PRODUCT_VIRTUAL_AB_COMPRESSION_METHOD := lz4
PRODUCT_RUNTIMES := runtime_libart_default
OVERRIDE_PRODUCT_COMPRESSED_APEX := false
PRODUCT_SYSTEM_SERVER_COMPILER_FILTER := speed-profile

# =============================================================================
# Product identity
# =============================================================================

PRODUCT_IS_AUTOMOTIVE := true
PRODUCT_CHARACTERISTICS := automotive
PRODUCT_DISPLAY_DENSITY := 100
PRODUCT_INIT_BOOT_IMAGE_HEADER_VERSION := 4

PRODUCT_MODEL := AAOS on VIM3
PRODUCT_BRAND := Khadas
PRODUCT_MANUFACTURER := KHADAS
PRODUCT_VENDOR_PROPERTIES += \
    ro.soc.manufacturer=KHADAS \
    ro.soc.model=VIM3
PRODUCT_PROPERTY_OVERRIDES += ro.product.device=vim3

VENDOR_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)
BOOT_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)
PRODUCT_SHIPPING_API_LEVEL := 36
PRODUCT_PRODUCT_VNDK_VERSION := current

TARGET_NO_TELEPHONY := true

# =============================================================================
# AB / OTA
# =============================================================================

PRODUCT_PACKAGES += \
    otapreopt_script \
    cppreopts.sh \
    update_engine \
    update_engine_sideload \
    update_verifier \
    sg_write_buffer \
    f2fs_io \
    check_f2fs \
    checkpoint_gc

PRODUCT_PACKAGES_DEBUG += \
    bootctl \
    update_engine_client \
    apply_ota

PRODUCT_PACKAGES += \
    com.android.hardware.boot \
    android.hardware.boot-service.default_recovery

PRODUCT_BUILD_SUPER_PARTITION := true
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

PRODUCT_PACKAGES += \
    android.hardware.fastboot@1.1 \
    android.hardware.fastboot@1.1-impl-mock \
    fastbootd

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.verified_boot.xml:system/etc/permissions/android.software.verified_boot.xml

# =============================================================================
# fstab
# =============================================================================

PRODUCT_PACKAGES += \
    fstab.vim3.mmc.avb \
    fstab.vim3.mmc.avb.vendor_ramdisk

# =============================================================================
# Init scripts and ueventd
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/init.vim3.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.vim3.rc \
    device/khadas/vim3/init.vim3.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.vim3.usb.rc \
    device/khadas/vim3/init.recovery.hardware.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.vim3.rc \
    device/khadas/vim3/ueventd.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc

# =============================================================================
# Platform permissions
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/permissions/vim3.xml:/system/etc/sysconfig/vim3.xml

PRODUCT_COPY_FILES += \
    device/khadas/vim3/permissions/android.software.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.xml \
    frameworks/native/data/etc/android.software.cts.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.cts.xml \
    frameworks/native/data/etc/android.software.app_widgets.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.app_widgets.xml \
    frameworks/native/data/etc/android.software.backup.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.backup.xml \
    frameworks/native/data/etc/android.software.voice_recognizers.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.voice_recognizers.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.software.device_admin.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.device_admin.xml \
    frameworks/native/data/etc/android.software.secure_lock_screen.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.secure_lock_screen.xml \
    frameworks/native/data/etc/android.software.activities_on_secondary_displays.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.activities_on_secondary_displays.xml \
    frameworks/native/data/etc/android.software.companion_device_setup.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.companion_device_setup.xml

# =============================================================================
# Input keylayout
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/input/Generic.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/Generic.kl

# =============================================================================
# HALs
# =============================================================================

$(call inherit-product, device/khadas/vim3/hal/graphics/device_vendor.mk)
$(call inherit-product, device/khadas/vim3/hal/connectivity/device_vendor.mk)
$(call inherit-product, device/khadas/vim3/hal/camera/camera.mk)
$(call inherit-product, device/khadas/vim3/hal/audio/device_vendor.mk)
$(call inherit-product, device/khadas/vim3/hal/media/device_vendor.mk)
$(call inherit-product, device/khadas/vim3/hal/display/display_wake.mk)
$(call inherit-product, device/khadas/vim3/hal/broadcastradio/device_vendor.mk)

# Thermal HAL
PRODUCT_PACKAGES += \
    com.android.hardware.thermal.rs.generic.v3
PRODUCT_VENDOR_PROPERTIES += \
    vendor.thermal.hardware=g12

# DRM
PRODUCT_PACKAGES += \
    android.hardware.drm@latest-service.clearkey

# PowerHAL
PRODUCT_PACKAGES += \
    com.android.hardware.power

# Health HAL
PRODUCT_PACKAGES += \
    com.google.cf.health \
    android.hardware.health-service.cuttlefish_recovery \
    com.google.cf.health.storage

# Security HALs
PRODUCT_PACKAGES += \
    com.android.hardware.authsecret \
    com.android.hardware.keymint.rust_nonsecure \
    com.android.hardware.gatekeeper.nonsecure

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.keystore.app_attest_key.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.keystore.app_attest_key.xml

# USB HAL
BOARD_VENDOR_SEPOLICY_DIRS += hardware/amlogic/yukawa/usb/aidl/sepolicy

PRODUCT_PACKAGES += \
    com.android.hardware.usb.generic

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml

# =============================================================================
# Virtualization (pKVM — VIM3 has 4 GB RAM)
# =============================================================================

$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)

PRODUCT_VENDOR_PROPERTIES += ro.frp.pst=/dev/block/by-name/frp

# =============================================================================
# HDMI
# =============================================================================

PRODUCT_PROPERTY_OVERRIDES += \
    ro.hdmi.device_type=4 \
    persist.sys.hdmi.keep_awake=false \
    persist.wm.debug.predictive_back=0 \
    persist.wm.debug.predictive_back_anim=0

# =============================================================================
# Ramdisk / first-stage boot packages
# =============================================================================

BOARD_VENDOR_RAMDISK_PACKAGES += \
    snapuserd \
    snapuserd_ramdisk \
    init_first_stage

PRODUCT_PACKAGES += \
    snapuserd \
    snapuserd_ramdisk \
    init_first_stage \
    snapuserd.vendor_ramdisk \
    bootctrl.default

# =============================================================================
# Miscellaneous
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/flash.sh:$(TARGET_OUT)/flash.sh

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay
