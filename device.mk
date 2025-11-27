PRODUCT_IS_AUTOMOTIVE := true
TARGET_KERNEL_USE := 6.12
TARGET_VIM3 := true
TARGET_USE_TABLET_LAUNCHER := true
PRODUCT_IS_AUTOMOTIVE_SDK := true
PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml

# GMS
ifeq ($(WITH_GMS),true)
	GMS_MAKEFILE=gms_minimal.mk
	WITH_GMS_COMMS_SUITE := false
endif

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)