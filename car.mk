# =============================================================================
# AAOS platform setup — car service stack and car-level configuration
# =============================================================================

SYSTEM_OPTIMIZE_JAVA := true

DEVICE_FRAMEWORK_MANIFEST_FILE += device/google_car/common/manifest.xml

# generic_system.mk sets PRODUCT_ENFORCE_RRO_TARGETS := * which breaks phone_car
PRODUCT_ENFORCE_RRO_TARGETS :=

PRODUCT_IS_AUTOMOTIVE_SDK := true

ENABLE_CAMERA_SERVICE := true
ENABLE_CARTELEMETRY_SERVICE ?= false
ENABLE_EVS_SERVICE := false
ENABLE_EVS_SAMPLE := false
ENABLE_MOCK_EVSHAL := false
ENABLE_CAREVSSERVICE_SAMPLE := false
ENABLE_SAMPLE_EVS_APP := false
CUSTOMIZE_EVS_SERVICE_PARAMETER := false

# Dalvik heap sizing for automotive (car service runs in system_server)
PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.systemserverheapsize=128m \
    dalvik.vm.systemserverheapgrowthlimit=128m \
    dalvik.vm.heapstartsize=16m \
    dalvik.vm.heapgrowthlimit=256m \
    dalvik.vm.heapsize=256m \
    dalvik.vm.heapminfree=8m \
    dalvik.vm.heapmaxfree=32m \
    dalvik.vm.heaptargetutilization=0.75

$(call inherit-product, packages/services/Car/car_product/build/car.mk)
$(call inherit-product-if-exists, packages/services/Car/car_product/rro/ThemeSamples/product.mk)
$(call inherit-product-if-exists, packages/apps/Car/SystemUI/samples/systemui_sample_rros.mk)

PRODUCT_PACKAGES += \
    CarSettingsIntelligence

$(call inherit-product-if-exists, frameworks/base/data/sounds/AllAudio.mk)
$(call inherit-product, frameworks/base/data/fonts/fonts.mk)

PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/car_product/sepolicy/public
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/car_product/sepolicy/private
PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/cpp/power/sepolicy/public
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/cpp/power/sepolicy/private
PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/cpp/watchdog/sepolicy/public
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/cpp/watchdog/sepolicy/private

BOARD_SEPOLICY_DIRS += device/google_car/common/sepolicy
BOARD_SEPOLICY_DIRS += device/generic/car/common/sepolicy

PRODUCT_PACKAGES += \
    CarNotification \
    android.hardware.broadcastradio-service.default \
    android.hardware.automotive.remoteaccess@V2-default-service \
    android.hardware.automotive.ivn@V1-default-service \
    CarConnectivityOverlay \
    CarServiceOverlayPhoneCar \
    librs_jni

GOOGLE_CAR_SERVICE_OVERLAY += CarServiceOverlayPhoneCarGoogle

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/aosp_excluded_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/aosp_excluded_hardware.xml \
    frameworks/native/data/etc/car_core_hardware.xml:system/etc/permissions/car_core_hardware.xml \
    frameworks/native/data/etc/car_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/car_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.screen.landscape.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.screen.landscape.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.type.automotive.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.type.automotive.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    frameworks/native/data/etc/android.hardware.broadcastradio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.broadcastradio.xml \
    frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.ar.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.autofocus.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.concurrent.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.full.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.any.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.raw.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

PRODUCT_COPY_FILES += \
    device/khadas/vim3/sysconfig/disable_customization_provider.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/disable_customization_provider.xml

PRODUCT_PROPERTY_OVERRIDES += \
    android.car.drawer.unlimited=true \
    android.car.hvac.demo=false \
    com.android.car.radio.demo=false \
    com.android.car.radio.demo.dual=false

PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/init/init.bootstat.rc:root/init.bootstat.rc \
    packages/services/Car/car_product/init/init.car.rc:root/init.car.rc

EXCLUDE_BUILD_RAMDUMP_UPLOADER_DEBUG_TOOL := true

PRODUCT_PRODUCT_PROPERTIES += \
    persist.rcs.supported=0 \
    persist.eab.supported=0

PRODUCT_PACKAGES_DEBUG += \
    android.hardware.automotive.occupant_awareness@1.0-service \
    android.hardware.automotive.occupant_awareness@1.0-service_mock

include packages/services/Car/car_product/occupant_awareness/OccupantAwareness.mk

PRODUCT_PROPERTY_OVERRIDES += \
    log.tag.CarTrustAgentUnlockEvent=I \
    log.tag.AHAL_StreamAlsa=E \
    log.tag.android.hardware.gnss-service.usb=E

PRODUCT_COPY_FILES += \
    device/generic/car/common/preinstalled-packages-product-car-emulator.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/preinstalled-packages-product-car-emulator.xml

PRODUCT_PACKAGES += \
    ContactsProvider \
    CallLogBackup \
    Twelve


PRODUCT_SYSTEM_PROPERTIES += \
    android.car.internal.version.platform=1 \
    ro.fw.multiuser.headless_system_user=true

PRODUCT_PRODUCT_PROPERTIES += \
    persist.settings.large_screen_opt.enabled=true
