# =============================================================================
# Vehicle hardware integration — VHAL, GPIO, backlight, power policy
# =============================================================================

# Custom Schuurman VHAL service
PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@schuurman-service

# GPIO bus shared by vehicle controls
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gpio.chip=gpiochip0

# Backlight enable: GPIOA_4 (physical pin 33, SoC offset 53)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.backlight.enable.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.backlight.enable.gpio.offset=53

# Reverse gear input: GPIOA_2 (physical pin 32, SoC offset 51)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.gear.gpio.chip=gpiochip0 \
    ro.vendor.vehicle.gear.gpio.offset=51

# PWM backlight — 32.768 kHz (30518 ns period)
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.pwm.period_ns=30518 \
    ro.vendor.vehicle.pwm.force_write_period=false

# HDMI output connector used for DPMS-based display power
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.display.dpms_path=/sys/class/drm/card1-HDMI-A-1/dpms

# Automotive power policy
PRODUCT_COPY_FILES += \
    device/khadas/vim3/power_policy.xml:$(TARGET_COPY_OUT_VENDOR)/etc/automotive/power_policy.xml

PRODUCT_SYSTEM_PROPERTIES += \
    ro.car.powerpolicy.group_id=default_group

# Car UX restrictions
PRODUCT_COPY_FILES += \
    device/khadas/vim3/car_ux_restrictions_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/car_ux_restrictions_config.xml

# Screen-off service (turns off display via DPMS when car is parked)
PRODUCT_PACKAGES += \
    ScreenOffService \
    Vim3PowerFrameworkOverlay

# Volume control receiver (up / down / mute buttons in the system bar)
PRODUCT_PACKAGES += \
    VolumeControl
