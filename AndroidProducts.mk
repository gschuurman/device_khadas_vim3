PRODUCT_MAKEFILES := \
    $(LOCAL_DIR)/lineage_vim3.mk \
    $(LOCAL_DIR)/lineage_vim3_nvme.mk

$(foreach build_type, user userdebug eng, \
    $(eval COMMON_LUNCH_CHOICES += lineage_vim3-$(build_type)))

$(foreach build_type, user userdebug eng, \
    $(eval COMMON_LUNCH_CHOICES += lineage_vim3_nvme-$(build_type)))