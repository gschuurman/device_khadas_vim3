# Inherit device configuration (brings in car.mk, vehicle.mk, wireless.mk, etc.)
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# LineageOS common configuration. NOT common_car.mk: that ships device_provisioned/user_setup_complete
# = true (CarSettingsProviderOverlay), which skips first-boot setup. The device boots unprovisioned
# (GAS defaults from car_settings_provider_config_rro) and LineageSetupWizard (vehicle.mk) is the HOME
# activity until the user finishes setup.
$(call inherit-product, vendor/lineage/config/common.mk)

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
