PRODUCT_IS_AUTOMOTIVE := true
TARGET_VIM3 := true
TARGET_USE_TABLET_LAUNCHER := true
PRODUCT_IS_AUTOMOTIVE_SDK := true
PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay

DEVICE_MANIFEST_FILE += \
	device/khadas/vim3/manifest.xml

PRODUCT_PACKAGES += \
    android.hardware.bluetooth.socket-service.default \
    android.hardware.bluetooth.audio-impl

PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@snap-service \
    snapp_evs_configuration

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)