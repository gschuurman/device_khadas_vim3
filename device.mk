PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
PRODUCT_DISPLAY_DENSITY := 100

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

$(call inherit-product, device/khadas/vim3/car.mk)

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

$(call inherit-product, vendor/partner_gms/products/gms_64bit_only.mk)

$(call inherit-product, device/khadas/vim3/hal/audio/device_vendor.mk)

$(call inherit-product, device/khadas/vim3/hal/camera/camera.mk)


DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml


PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@schuurman-service \


PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam

PRODUCT_VENDOR_PROPERTIES += \
    ro.secure=0 \
    ro.adb.secure=0 \
    ro.force.debuggable=1 \
    ro.debuggable=1 \
    persist.sys.usb.config=adb \
    service.adb.root=1


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
