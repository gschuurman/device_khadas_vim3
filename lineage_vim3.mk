# Inherit device configuration (brings in car.mk, vehicle.mk, wireless.mk, etc.)
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# LineageOS common configuration for Android Automotive: common.mk plus device/lineage/car
# (CarSettingsProviderOverlay: device_provisioned / user_setup_complete / wifi_on default to true,
# CarServiceOverlay: no user-notice UI). Every Lineage car target (emulator, GSI, cuttlefish)
# inherits this. Without it the device boots unprovisioned and the first-boot flow (CarProvision /
# FallbackHome) leaves the driver user locked forever: no launcher, black screen.
$(call inherit-product, vendor/lineage/config/common_car.mk)

# Google Automotive Apps
$(call inherit-product, vendor/google/gapps_auto/gapps-core.mk)

# Product identity
PRODUCT_BRAND := Khadas
PRODUCT_DEVICE := vim3
PRODUCT_MANUFACTURER := Khadas
PRODUCT_MODEL := VIM3 Automotive
PRODUCT_NAME := lineage_vim3

PRODUCT_BUILD_TYPE := user
PRODUCT_BUILD_TAGS := release-keys

WITH_GMS := false
