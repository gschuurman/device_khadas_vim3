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


PRODUCT_PACKAGES += ScreenPowerBridge

# GPIO Configuration
# Brightness Control (PWM) - Pin 51 (Used for reference or if switching to GPIO-based PWM later)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.brightness.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.brightness.gpio.offset=51

# Backlight Enable / Screen Power - Pin 53
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.backlight.enable.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.backlight.enable.gpio.offset=53

# Reverse Gear Selection - (Not specified yet, default to -1 to disable)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gear.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gear.gpio.offset=-1

# PWM Configuration (Existing)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.pwm.period_ns=30518 \
    ro.vendor.vehicle.pwm.force_write_period=false