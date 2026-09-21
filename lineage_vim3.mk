# Inherit device configuration (brings in car.mk, vehicle.mk, wireless.mk, etc.)
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# LineageOS common configuration. NOT common_car.mk: that ships device_provisioned/user_setup_complete
# = true (CarSettingsProviderOverlay), which skips the first-boot setup wizard. We want the Google
# car setup wizard (sign in to a Google account), so the device boots unprovisioned (GAS defaults
# from car_settings_provider_config_rro) and the wizard is the HOME activity until setup is done.
$(call inherit-product, vendor/lineage/config/common.mk)

# Google Automotive Apps
$(call inherit-product, vendor/google/gapps_auto/gapps-core.mk)

# gapps-core.mk only carries the wizard's permission files; the app itself is listed in the full
# gapps-auto.mk, which cannot be inherited as a whole (it names modules that do not exist).
PRODUCT_PACKAGES += \
    com_google_android_car_setupwizard \
    com_android_managedprovisioning_googlecarui_rro

# Product identity
PRODUCT_BRAND := Khadas
PRODUCT_DEVICE := vim3
PRODUCT_MANUFACTURER := Khadas
PRODUCT_MODEL := VIM3 Automotive
PRODUCT_NAME := lineage_vim3

PRODUCT_BUILD_TYPE := user
PRODUCT_BUILD_TAGS := release-keys

WITH_GMS := false
