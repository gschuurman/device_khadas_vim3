PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
PRODUCT_DISPLAY_DENSITY := 100
PRODUCT_INIT_BOOT_IMAGE_HEADER_VERSION := 4

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

TARGET_NO_TELEPHONY := true

$(call inherit-product, device/khadas/vim3/car.mk)

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

$(call inherit-product, vendor/google/gapps_auto/gapps-core.mk)

$(call inherit-product, device/khadas/vim3/hal/audio/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/camera/camera.mk)


DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml

BOARD_VENDOR_RAMDISK_PACKAGES += \
    snapuserd \
    snapuserd_ramdisk \
    init_first_stage

PRODUCT_PACKAGES += \
    update_engine \
    update_engine_sideload \
    update_verifier \
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
    service.adb.root=1


PRODUCT_VENDOR_PROPERTIES += \
    ro.radio.noril=true

PRODUCT_PROPERTY_OVERRIDES += \
    log.tag.drmhwc=SILENT

# GPIO Configuration
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gpio.chip=gpiochip0

# Backlight Enable / Screen Power - Pin 53
# GPIOA_4 (Physical pin 33)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.backlight.enable.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.backlight.enable.gpio.offset=53

# Reverse Gear Selection - (Not specified yet, default to -1 to disable)
# GPIOA_2 (Physical pin 32)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gear.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gear.gpio.offset=51

# PWM Configuration (Existing)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.pwm.period_ns=30518 \
    ro.vendor.vehicle.pwm.force_write_period=false

# Display DPMS path for backlight-off on screen sleep
# card1 = VIM3 HDMI output, connector name HDMI-A-1
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

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.companion_device_setup.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.software.companion_device_setup.xml \
    frameworks/native/data/etc/car_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/car_core_hardware.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.telephony.subscription.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.subscription.xml \
    frameworks/native/data/etc/android.hardware.telephony.messaging.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.messaging.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml

PRODUCT_PACKAGES += \
    Vim3PowerFrameworkOverlay \
    init.wifi_fix.rc