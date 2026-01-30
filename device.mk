PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
PRODUCT_DISPLAY_DENSITY := 100

PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

$(call inherit-product, device/khadas/vim3/car.mk)

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

# $(call inherit-product, vendor/partner_gms/products/gms_64bit_only.mk)

$(call inherit-product, vendor/google/gapps_auto/gapps-core.mk)

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


PRODUCT_PROPERTY_OVERRIDES += \
    ro.zram.mark_idle_delay_mins=60 \
    ro.zram.first_wb_delay_mins=1440 \
    vendor.zram.size=50%

# Stop LMK from killing background services so aggressively
PRODUCT_PROPERTY_OVERRIDES += \
    ro.lmk.critical_upgrade=false \
    ro.lmk.upgrade_pressure=40 \
    ro.lmk.downgrade_pressure=60 \
    ro.lmk.kill_heaviest_task=true


PRODUCT_PACKAGES += android.hardware.gnss-service.usb
PRODUCT_PACKAGES += android.hardware.gnss-service.usb.rc
