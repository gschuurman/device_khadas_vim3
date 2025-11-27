TARGET_VIM3 := true
TARGET_USE_TABLET_LAUNCHER := true
BOARD_IS_AUTOMOTIVE := true
PRODUCT_DISPLAY_DENSITY := 160

PRODUCT_DEVICE := vim3

PRODUCT_NAME := lineage_vim3
PRODUCT_BRAND := Khadas
PRODUCT_MODEL := VIM3 (Snapp Edition)
PRODUCT_MANUFACTURER := Khadas
PRODUCT_CHARACTERISTICS := automotive

# --- INHERITANCE ---
# 1. LineageOS Common (Geeft je de Lineage basis)
$(call inherit-product, vendor/lineage/config/common.mk)
# 2. Yukawa Platform Base
$(call inherit-product, device/amlogic/yukawa/yukawa.mk)
# 3. Android Automotive Defaults
$(call inherit-product, packages/services/Car/car_product/build/car.mk)
# 4. Fonts
$(call inherit-product, frameworks/base/data/fonts/fonts.mk)

$(call inherit-product-if-exists, vendor/partner_gms/products/gms.mk)

# --- A/B UPDATE PACKAGES ---
# 'bootctrl.yukawa' bestond niet, we gebruiken de generieke of die uit yukawa.mk
PRODUCT_PACKAGES += \
    update_engine \
    update_engine_client \
    update_verifier \
    checkpoint_gc \
    SystemUpdaterSample

# --- BOOT HAL (FIX) ---
# De AIDL default service bestond niet, we vallen terug op de HIDL 1.2 die Yukawa gebruikt
PRODUCT_PACKAGES += \
    android.hardware.boot@1.2-service \
    android.hardware.boot@1.2-impl-recovery

# --- AUTOMOTIVE PACKAGES ---
PRODUCT_PACKAGES += \
    android.hardware.automotive.remoteaccess@V2-default-service \
    android.hardware.automotive.ivn@V1-default-service \
    CarConnectivityOverlay \
    librs_jni
    # CarWifiOverlay verwijderd (veroorzaakte error)

# --- EVS (CAMERA) ---
# De standaard Google services bestonden niet in jouw tree.
# We voegen WEL jouw configuratie toe. De services moeten we later toevoegen 
# zodra we weten hoe ze heten in LineageOS (vaak android.hardware.automotive.evs-aidl-default).
PRODUCT_PACKAGES += \
    snapp_evs_configuration
    # EvsApp en manager-service tijdelijk uitgeschakeld om build te fixen

# --- CUSTOM HAL ---
PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@snap-service

# --- USB & AUDIO ---
PRODUCT_PACKAGES += \
    audio.usb.default \
    tinyplay \
    tinycap \
    tinymix \
    android.hardware.camera.provider@2.5-external-service \
    camera.device@3.5-external-impl \
    camera.device@3.5-impl
    # gnss-service.usb verwijderd (veroorzaakte error)

# --- PROPERTIES ---
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=256m \
    ro.boot.wificountrycode=00 \
    ro.config.media_vol_default=0 \
    log.tag.CarTrustAgentUnlockEvent=I \
    android.car.drawer.unlimited=true \
    android.car.hvac.demo=true \
    com.android.car.radio.demo=true \
    com.android.car.radio.demo.dual=true

PRODUCT_PRODUCT_PROPERTIES += \
    persist.rcs.supported=0 \
    persist.eab.supported=0

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.vehicle.path.pwm.duty=/sys/class/pwm/pwmchip0/pwm0/duty_cycle \
    ro.vendor.vehicle.path.pwm.enable=/sys/class/pwm/pwmchip0/pwm0/enable \
    ro.vendor.vehicle.path.pwm.period=/sys/class/pwm/pwmchip0/pwm0/period \
    ro.vendor.vehicle.path.gpio.reverse=/sys/class/gpio/gpio496/value \
    ro.automotive.evs.config_file=/vendor/etc/automotive/evs/evs_configuration_override.xml

# --- OVERLAYS & POLICY ---
PRODUCT_PACKAGE_OVERLAYS += device/khadas/vim3/overlay
BOARD_SEPOLICY_DIRS += device/google_car/common/sepolicy
BOARD_SEPOLICY_DIRS += device/khadas/vim3/sepolicy

DEVICE_MANIFEST_FILE += device/khadas/vim3/manifest.xml

# --- BUILD SETTINGS ---
SYSTEM_OPTIMIZE_JAVA := false
PRODUCT_IS_AUTOMOTIVE_SDK := true
EXCLUDE_BUILD_RAMDUMP_UPLOADER_DEBUG_TOOL := true

PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/init/init.bootstat.rc:root/init.bootstat.rc \
    packages/services/Car/car_product/init/init.car.rc:root/init.car.rc

# Permissions (De errors hierover zijn opgelost door de niet-bestaande services te verwijderen)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/car_core_hardware.xml:system/etc/permissions/car_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.screen.landscape.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.screen.landscape.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml

