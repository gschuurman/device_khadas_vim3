PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
TARGET_USE_TABLET_LAUNCHER := true
PRODUCT_IS_AUTOMOTIVE_SDK := true
PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml

PRODUCT_PACKAGES += \
    android.hardware.bluetooth.socket-service.default \
    android.hardware.bluetooth.audio-impl

PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@schuurman-service \
    schuurman_evs_configuration \

PRODUCT_PACKAGES += \
    Updater \
    DocumentsUI

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

# $(call inherit-product, vendor/gapps/arm64/arm64-vendor.mk)
# $(call inherit-product, vendor/partner_gms/products/gms_64bit_only.mk)

$(call inherit-product, device/khadas/vim3/audio.mk)

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gpio.offset=16 \
    ro.vendor.vehicle.path.pwm.duty=/sys/class/pwm/pwmchip1/pwm1/duty_cycle \
    ro.vendor.vehicle.path.pwm.enable=/sys/class/pwm/pwmchip1/pwm1/enable 

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.serial.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.serial.rc