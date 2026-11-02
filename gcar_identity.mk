# =============================================================================
# Build identity of Google's Android 15 car Play image (GMS device-key test)
# =============================================================================
# Values copied from the partitions of system-images;android-35-ext15;android-automotive-playstore;arm64-v8a r3
# (sdk_gcar_arm64 AAI5.260516.001.A1). Every partition (system, system_ext, product, vendor, odm, *_dlkm) reports
# this identity. Included (not inherited, so its := win) last by lineage_vim3_nvme.mk; drop that include to go back to the Khadas identity.
#
# Deliberately NOT changed: ro.build.version.{sdk,release,security_patch} (the framework is Android 16 and KeyMint
# binds keys to the OS version/patch level: a lower value is a rollback, keystore refuses the keys and FBE fails),
# ro.hardware / ro.boot.hardware (init.vim3.rc import, HAL selection) and ro.lineage.* (updater).
#
# PRODUCT_DEVICE / TARGET_PRODUCT stay vim3 / lineage_vim3_nvme: only the emitted properties change.

GCAR_FINGERPRINT := google/sdk_gcar_arm64/emulator_car64_arm64:15/AAI5.260516.001.A1/15446886:user/release-keys

PRODUCT_BRAND := google
PRODUCT_MANUFACTURER := Google
PRODUCT_MODEL := Car on arm64 emulator

# The system partition of the Google image is generic (GSI-style).
PRODUCT_SYSTEM_BRAND := Android
PRODUCT_SYSTEM_DEVICE := generic
PRODUCT_SYSTEM_MANUFACTURER := Android
PRODUCT_SYSTEM_MODEL := car_generic
PRODUCT_SYSTEM_NAME := car_generic

# Keys of build/soong/scripts/gen_build_prop.py (all Soong-generated build.prop files) and shell variables of
# build/make/core/sysprop.mk generate-common-build-props (vendor/build.prop). No values with spaces: the Make
# path splits on them.
PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildFingerprint=$(GCAR_FINGERPRINT) \
    BuildId=AAI5.260516.001.A1 \
    BuildNumber=15446886 \
    BuildVariant=user \
    BuildVersionTags=release-keys \
    BuildKeys=release-keys \
    BuildFlavor=sdk_gcar_arm64-user \
    DeviceName=emulator_car64_arm64 \
    DeviceProduct=sdk_gcar_arm64

# Values the generators compute before the overrides apply, or hard-code. Duplicates of generated "x=" lines need
# BUILD_BROKEN_DUP_SYSPROP (BoardConfig.mk); init keeps the last definition, which is these.
# ro.build.description and ro.build.display.id: gcar_identity.prop (BoardConfig.mk), they contain spaces / are
# re-emitted by the product partition.
PRODUCT_SYSTEM_PROPERTIES += \
    ro.build.product=emulator_car64_arm64

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.build.id=AAI5.260516.001.A1 \
    ro.vendor.build.tags=release-keys \
    ro.vendor.build.type=user \
    ro.vendor.build.version.incremental=15446886 \
    ro.product.board=goldfish_arm64 \
    ro.soc.manufacturer=AOSP \
    ro.soc.model=ranchu
