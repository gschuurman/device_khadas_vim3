TARGET_VIM3 := true
PRODUCT_IS_AUTOMOTIVE := true

# CarServiceHelperService accesses the hidden api in the system server.
SYSTEM_OPTIMIZE_JAVA := false

DEVICE_FRAMEWORK_MANIFEST_FILE += device/google_car/common/manifest.xml

# generic_system.mk sets 'PRODUCT_ENFORCE_RRO_TARGETS := *'
# but this breaks phone_car. So undo it here.
PRODUCT_ENFORCE_RRO_TARGETS :=

# Enable mainline checking
# PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS := false

# Set Car Service RRO
PRODUCT_PACKAGES += CarServiceOverlayPhoneCar
GOOGLE_CAR_SERVICE_OVERLAY += CarServiceOverlayPhoneCarGoogle

# Additional selinux policy
BOARD_SEPOLICY_DIRS += device/google_car/common/sepolicy

# Exclude the testing apps
PRODUCT_IS_AUTOMOTIVE_SDK := true

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.systemserverheapsize=128m \
    dalvik.vm.systemserverheapgrowthlimit=128m \
    dalvik.vm.heapstartsize=16m \
    dalvik.vm.heapgrowthlimit=256m \
    dalvik.vm.heapsize=256m \
    dalvik.vm.heapminfree=8m \
    dalvik.vm.heapmaxfree=32m \
    dalvik.vm.heaptargetutilization=0.75

PRODUCT_SYSTEM_SERVER_COMPILER_FILTER := speed


$(call inherit-product, packages/services/Car/car_product/build/car.mk)
$(call inherit-product, frameworks/base/data/fonts/fonts.mk)

PRODUCT_PACKAGES += \
	android.hardware.broadcastradio-service.default \
	android.hardware.automotive.remoteaccess@V2-default-service \
	android.hardware.automotive.ivn@V1-default-service \
	CarConnectivityOverlay \
	librs_jni

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/aosp_excluded_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/aosp_excluded_hardware.xml \
	frameworks/native/data/etc/car_core_hardware.xml:system/etc/permissions/car_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
	frameworks/native/data/etc/android.hardware.broadcastradio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.broadcastradio.xml \
	frameworks/native/data/etc/android.hardware.screen.landscape.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.screen.landscape.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/native/data/etc/android.hardware.type.automotive.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.type.automotive.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml \
	frameworks/native/data/etc/android.software.activities_on_secondary_displays.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.activities_on_secondary_displays.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.ar.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.autofocus.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.concurrent.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.full.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.any.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.raw.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
	device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \

PRODUCT_COPY_FILES += \
    device/khadas/vim3/sysconfig/disable_customization_provider.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/disable_customization_provider.xml

PRODUCT_PROPERTY_OVERRIDES += \
	android.car.drawer.unlimited=true \
	android.car.hvac.demo=false \
	com.android.car.radio.demo=false \
	com.android.car.radio.demo.dual=false \

PRODUCT_COPY_FILES += \
	packages/services/Car/car_product/init/init.bootstat.rc:root/init.bootstat.rc \
	packages/services/Car/car_product/init/init.car.rc:root/init.car.rc

ENABLE_CAMERA_SERVICE := true
ENABLE_CARTELEMETRY_SERVICE ?= false
ENABLE_EVS_SERVICE := false
ENABLE_EVS_SAMPLE := false
ENABLE_MOCK_EVSHAL := false
ENABLE_CAREVSSERVICE_SAMPLE := false
ENABLE_SAMPLE_EVS_APP := false
CUSTOMIZE_EVS_SERVICE_PARAMETER := false
# $(call inherit-product, device/generic/car/emulator/evs/evs.mk)

# CAN bus support - We don't have a direct hardware interface on the
# VIM3, but there are dongles which can provide CAN bus connectivity

PRODUCT_PACKAGES += \
	RearViewCamera

PRODUCT_PACKAGES_DEBUG += \
	android.hardware.automotive.occupant_awareness@1.0-service \
	android.hardware.automotive.occupant_awareness@1.0-service_mock

BOARD_SEPOLICY_DIRS += device/google_car/common/sepolicy

# # Sepolicy for occupant awareness system
# include packages/services/Car/car_product/occupant_awareness/OccupantAwareness.mk

# # Sepolicy for compute pipe system
# include packages/services/Car/cpp/computepipe/products/computepipe.mk

PRODUCT_PROPERTY_OVERRIDES += \
        ro.boot.wificountrycode=00 \
        log.tag.CarTrustAgentUnlockEvent=I

# Phone car targets don't support ramdump
EXCLUDE_BUILD_RAMDUMP_UPLOADER_DEBUG_TOOL := true

# Disable RCS and EAB for phone car targets
PRODUCT_PRODUCT_PROPERTIES += \
        persist.rcs.supported=0 \
        persist.eab.supported=0

# Occupant Awareness

PRODUCT_PACKAGES_DEBUG += \
	android.hardware.automotive.occupant_awareness@1.0-service \
	android.hardware.automotive.occupant_awareness@1.0-service_mock

# Sepolicy for occupant awareness system
include packages/services/Car/car_product/occupant_awareness/OccupantAwareness.mk

PRODUCT_PROPERTY_OVERRIDES += \
        ro.boot.wificountrycode=00 \
        ro.config.media_vol_default=0 \
        log.tag.CarTrustAgentUnlockEvent=I

# Additional selinux policy
BOARD_SEPOLICY_DIRS += device/generic/car/common/sepolicy

# Whitelisted packages per user type
PRODUCT_COPY_FILES += \
    device/generic/car/common/preinstalled-packages-product-car-emulator.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/preinstalled-packages-product-car-emulator.xml

PRODUCT_PACKAGES += \
    ContactsProvider \
    CallLogBackup \
	Twelve

PRODUCT_PROPERTY_OVERRIDES += \
    ro.carrier=unknown

# Include snappmaps into build to show a map
PRODUCT_PACKAGES += \
	osmdroid \
