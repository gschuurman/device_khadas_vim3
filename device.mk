PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
PRODUCT_DISPLAY_DENSITY := 100
PRODUCT_INIT_BOOT_IMAGE_HEADER_VERSION := 4

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

TARGET_NO_TELEPHONY := true

# =============================================================================
# Core product setup
# =============================================================================

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)

ifndef TARGET_KERNEL_USE
TARGET_KERNEL_USE := 6.12
endif

$(call inherit-product, device/khadas/vim3/car.mk)

# =============================================================================
# Vendor package version (WiFi/BT firmware, GPU, video, bootloader binaries)
# =============================================================================

include device/khadas/vim3/vendor-package-ver.mk
ifneq (,$(wildcard $(YUKAWA_VENDOR_PATH)/bt-wifi-firmware))
  ifneq (,$(wildcard $(YUKAWA_VENDOR_PATH)/bt-wifi-firmware/$(EXPECTED_YUKAWA_VENDOR_VERSION)/version.mk))
    include $(YUKAWA_VENDOR_PATH)/bt-wifi-firmware/$(EXPECTED_YUKAWA_VENDOR_VERSION)/version.mk
    ifneq ($(TARGET_YUKAWA_VENDOR_VERSION), $(EXPECTED_YUKAWA_VENDOR_VERSION))
      $(warning TARGET_YUKAWA_VENDOR_VERSION ($(TARGET_YUKAWA_VENDOR_VERSION)) does not match.)
      $(warning Please run: ./device/khadas/vim3/fetch-vendor-package.sh)
    endif
  else
      $(warning TARGET_YUKAWA_VENDOR_VERSION undefined.)
      $(warning Please run: ./device/khadas/vim3/fetch-vendor-package.sh)
  endif
else
  $(warning Missing yukawa vendor package!)
  $(warning Please run: ./device/khadas/vim3/fetch-vendor-package.sh)
endif

# Vendor binary packages
$(call inherit-product-if-exists, $(YUKAWA_VENDOR_PATH)/bt-wifi-firmware/$(EXPECTED_YUKAWA_VENDOR_VERSION)/vendor.mk)
$(call inherit-product-if-exists, $(YUKAWA_VENDOR_PATH)/video_firmware/$(EXPECTED_YUKAWA_VENDOR_VERSION)/vendor.mk)
$(call inherit-product-if-exists, $(YUKAWA_VENDOR_PATH)/gpu/$(EXPECTED_YUKAWA_VENDOR_VERSION)/vendor.mk)
$(call inherit-product-if-exists, $(YUKAWA_VENDOR_PATH)/bootloader/$(EXPECTED_YUKAWA_VENDOR_VERSION)/vendor.mk)

# =============================================================================
# Soong namespaces
# =============================================================================

PRODUCT_SOONG_NAMESPACES += device/khadas/vim3
PRODUCT_SOONG_NAMESPACES += hardware/amlogic/yukawa

PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false
PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true

# =============================================================================
# Core runtime and feature inherits
# =============================================================================

$(call inherit-product, $(SRC_TARGET_DIR)/product/updatable_apex.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/android_t_baseline.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/developer_gsi_keys.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

PRODUCT_VIRTUAL_AB_COMPRESSION_METHOD := lz4

# pKVM — VIM3 has 4 GB RAM, enable virtualization
$(call inherit-product-if-exists, packages/modules/Virtualization/apex/product_packages.mk)

PRODUCT_RUNTIMES := runtime_libart_default

PRODUCT_CHARACTERISTICS := automotive
PRODUCT_IS_AUTOMOTIVE := true

OVERRIDE_PRODUCT_COMPRESSED_APEX := false

# =============================================================================
# Security patch level
# =============================================================================

VENDOR_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)
BOOT_SECURITY_PATCH = $(PLATFORM_SECURITY_PATCH)
PRODUCT_SHIPPING_API_LEVEL := 36
PRODUCT_PRODUCT_VNDK_VERSION := current

# =============================================================================
# Product identity
# =============================================================================

PRODUCT_MODEL := AAOS on VIM3
PRODUCT_BRAND := Khadas
PRODUCT_MANUFACTURER := KHADAS
PRODUCT_VENDOR_PROPERTIES += \
    ro.soc.manufacturer=KHADAS \
    ro.soc.model=VIM3
PRODUCT_PROPERTY_OVERRIDES += ro.product.device=vim3

# Speed profile services and wifi-service to reduce RAM and storage.
PRODUCT_SYSTEM_SERVER_COMPILER_FILTER := speed-profile

# =============================================================================
# AB / OTA packages
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
    update_engine_client

# Boot control
PRODUCT_PACKAGES += \
    com.android.hardware.boot \
    android.hardware.boot-service.default_recovery

# Dynamic partitions
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
    fstab.yukawa.mmc.avb \
    fstab.yukawa.mmc.avb.vendor_ramdisk

# =============================================================================
# Init / boot scripts and ueventd
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/init.yukawa.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.yukawa.rc \
    device/khadas/vim3/init.yukawa.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.yukawa.usb.rc \
    device/khadas/vim3/init.recovery.hardware.rc:$(TARGET_COPY_OUT_RECOVERY)/root/init.recovery.yukawa.rc \
    device/khadas/vim3/ueventd.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc

# =============================================================================
# Permissions and software config
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/permissions/yukawa.xml:/system/etc/sysconfig/yukawa.xml

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
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

# =============================================================================
# Input keylayout
# =============================================================================

PRODUCT_COPY_FILES += \
    device/khadas/vim3/input/Generic.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/Generic.kl

# =============================================================================
# PowerHAL
# =============================================================================

PRODUCT_PACKAGES += com.android.hardware.power

# =============================================================================
# Health HAL
# =============================================================================

PRODUCT_PACKAGES += \
    com.google.cf.health \
    android.hardware.health-service.cuttlefish_recovery \
    com.google.cf.health.storage

# =============================================================================
# Security HALs (AuthSecret, KeyMint, Gatekeeper)
# =============================================================================

PRODUCT_PACKAGES += \
    com.android.hardware.authsecret

PRODUCT_PACKAGES += \
    com.android.hardware.keymint.rust_nonsecure

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.keystore.app_attest_key.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.keystore.app_attest_key.xml

PRODUCT_PACKAGES += \
    com.android.hardware.gatekeeper.nonsecure

# =============================================================================
# USB HAL
# =============================================================================

BOARD_VENDOR_SEPOLICY_DIRS += hardware/amlogic/yukawa/usb/aidl/sepolicy

PRODUCT_PACKAGES += \
    com.android.hardware.usb.generic

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml

# =============================================================================
# Virtualization
# =============================================================================

$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)

PRODUCT_VENDOR_PROPERTIES += ro.frp.pst=/dev/block/by-name/frp

# HDMI display
PRODUCT_PROPERTY_OVERRIDES += ro.hdmi.device_type=4 \
    persist.sys.hdmi.keep_awake=false

PRODUCT_PROPERTY_OVERRIDES += persist.wm.debug.predictive_back=0 \
    persist.wm.debug.predictive_back_anim=0

# Flash script
PRODUCT_COPY_FILES += \
    device/khadas/vim3/flash.sh:$(TARGET_OUT)/flash.sh

# =============================================================================
# HAL — Graphics, Connectivity, Camera, Audio, Media, Thermal
# =============================================================================

$(call inherit-product, device/khadas/vim3/hal/graphics/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/connectivity/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/camera/camera.mk)

$(call inherit-product, device/khadas/vim3/hal/audio/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/media/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/display/display_wake.mk)

# Thermal HAL package
PRODUCT_PACKAGES += \
    com.android.hardware.thermal.rs.generic.v3

PRODUCT_VENDOR_PROPERTIES += \
    vendor.thermal.hardware=g12

# DRM Service
PRODUCT_PACKAGES += \
    android.hardware.drm@latest-service.clearkey

# =============================================================================
# GApps
# =============================================================================

$(call inherit-product, vendor/google/gapps_auto/gapps-core.mk)

# =============================================================================
# VIM3-specific packages and properties
# =============================================================================

DEVICE_MANIFEST_FILE += \
    device/khadas/vim3/manifest.xml

BOARD_VENDOR_RAMDISK_PACKAGES += \
    snapuserd \
    snapuserd_ramdisk \
    init_first_stage

PRODUCT_PACKAGES += \
    snapuserd \
    snapuserd_ramdisk \
    init_first_stage \
    snapuserd.vendor_ramdisk

PRODUCT_PACKAGES += \
    bootctrl.default

PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@schuurman-service \

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam

PRODUCT_VENDOR_PROPERTIES += \
    ro.secure=0 \
    ro.adb.secure=0 \
    persist.sys.usb.config=adb \
    service.adb.root=1 \
    service.adb.tcp.port=5555

PRODUCT_VENDOR_PROPERTIES += \
    ro.radio.noril=true

PRODUCT_PROPERTY_OVERRIDES += \
    log.tag.drmhwc=SILENT

# GPIO Configuration
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gpio.chip=gpiochip0

# Backlight Enable / Screen Power - Pin 53 (GPIOA_4, Physical pin 33)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.backlight.enable.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.backlight.enable.gpio.offset=53

# Reverse Gear Selection (GPIOA_2, Physical pin 32)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gear.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gear.gpio.offset=51

# PWM Configuration
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.pwm.period_ns=30518 \
    ro.vendor.vehicle.pwm.force_write_period=false

# Display DPMS path for backlight-off on screen sleep
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.display.dpms_path=/sys/class/drm/card1-HDMI-A-1/dpms

PRODUCT_PACKAGES += android.hardware.gnss-service.usb
PRODUCT_PACKAGES += android.hardware.gnss-service.usb.rc

PRODUCT_PACKAGES += \
    com.android.tethering \
    NetworkStack \
    CaptivePortalLogin \
    Telecom \
    TeleService \
    TelephonyProvider \
    MmsService \
    ContactsProvider\
    liblargeparcelablejni

PRODUCT_VENDOR_PROPERTIES += \
    persist.sys.powerstats.enabled=false

# WiFi country code — NL enables 5 GHz channels 36-64 and 100-165 (ETSI)
PRODUCT_VENDOR_PROPERTIES += \
    ro.boot.wificountrycode=NL

# WiFi regulatory database — cfg80211 requires this for NL country rules
PRODUCT_COPY_FILES += \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db:$(TARGET_COPY_OUT_VENDOR)/firmware/regulatory.db \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db.p7s:$(TARGET_COPY_OUT_VENDOR)/firmware/regulatory.db.p7s

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.companion_device_setup.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.companion_device_setup.xml \
    frameworks/native/data/etc/car_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/car_core_hardware.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.telephony.subscription.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.subscription.xml \
    frameworks/native/data/etc/android.hardware.telephony.messaging.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.messaging.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml

PRODUCT_PACKAGES += \
    Vim3PowerFrameworkOverlay \
    AndroidAutoProjectionRro \
    ScreenOffService

# Wireless Android Auto — WiFi Direct permission
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml

PRODUCT_PACKAGES += \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_SYSTEM_PROPERTIES += \
    android.car.internal.version.platform=1

PRODUCT_SYSTEM_PROPERTIES += \
    ro.fw.multiuser.headless_system_user=true
