PRODUCT_IS_AUTOMOTIVE := true

# GMS
ifeq ($(WITH_GMS),true)
GMS_MAKEFILE=gms_minimal.mk
WITH_GMS_COMMS_SUITE := false
endif

$(call inherit-product, device/amlogic/yukawa/device.mk)