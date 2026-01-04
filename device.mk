PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
PRODUCT_DISPLAY_DENSITY := 100

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

$(call inherit-product, device/khadas/vim3/car.mk)

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

$(call inherit-product, vendor/partner_gms/products/gms_64bit_only.mk)

$(call inherit-product, device/khadas/vim3/audio.mk)


DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml


PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@schuurman-service \
    schuurman_evs_configuration  \

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gpio.offset=51 \
    ro.vendor.vehicle.pwm.period_ns=30518 \
    ro.vendor.vehicle.pwm.force_write_period=false \
    ro.secure=0 \
    ro.adb.secure=0 \
    ro.force.debuggable=1 \
    ro.debuggable=1 \
    persist.sys.usb.config=adb \
    service.adb.root=1
