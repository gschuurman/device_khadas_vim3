# NVMe (Khadas M2X) variant of the vim3 product. Same device tree/
# BoardConfig as lineage_vim3 (PRODUCT_DEVICE stays vim3) -- only
# TARGET_PRODUCT differs, which BoardConfig.mk uses to pick the NVMe
# U-Boot defconfig (fastboot flash targets the SSD instead of eMMC).
# See device/khadas/vim3/BoardConfig.mk TARGET_UBOOT_DEFCONFIG.

$(call inherit-product, $(LOCAL_PATH)/lineage_vim3.mk)

PRODUCT_MODEL := VIM3 Automotive (NVMe)
PRODUCT_NAME := lineage_vim3_nvme
