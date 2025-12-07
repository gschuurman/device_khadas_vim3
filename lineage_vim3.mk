
# Inherit some common AOSP stuff
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)

$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

$(call inherit-product, packages/services/Car/car_product/build/car.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, device/lineage/car/lineage_car_vendor.mk)
$(call inherit-product, vendor/lineage/config/common_car.mk)

# Inherit device configuration
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# WITH_GMS := true

# 5. Product Definities
PRODUCT_BRAND := Khadas
PRODUCT_DEVICE := vim3
PRODUCT_MANUFACTURER := Khadas
PRODUCT_MODEL := VIM3 Automotive
PRODUCT_NAME := lineage_vim3
