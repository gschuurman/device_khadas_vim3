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
    snapp_evs_configuration \

PRODUCT_PACKAGES += \
    Updater \
    DocumentsUI

$(call inherit-product, device/amlogic/yukawa/yukawa.mk)

$(call inherit-product, vendor/partner_gms/products/gms_64bit_only.mk)

$(call inherit-product, device/khadas/vim3/audio.mk)

PRODUCT_PACKAGES := $(filter-out CarRadio,$(PRODUCT_PACKAGES))

PRODUCT_PROPERTY_OVERRIDES += \
    service.adb.tcp.port=5555 \
    persist.adb.tcp.port=5555


PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam