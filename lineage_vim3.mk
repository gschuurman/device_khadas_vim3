
# Inherit some common AOSP stuff
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)

$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common.mk)

# Inherit device configuration
$(call inherit-product, $(LOCAL_PATH)/device.mk)

WITH_GMS := false

# 5. Product Definities
PRODUCT_BRAND := Khadas
PRODUCT_DEVICE := vim3
PRODUCT_MANUFACTURER := Khadas
PRODUCT_MODEL := VIM3 Automotive
PRODUCT_NAME := lineage_vim3
