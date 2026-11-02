# =============================================================================
# Developer / debug configuration
# =============================================================================

# ADB always on, root shell, network ADB on port 5555. Root comes from service.adb.root=1 (adbd is
# built debuggable on userdebug), not ro.secure=0: an insecure ro.secure flags the device to Google's
# DroidGuard attestation. These must live in /product: init loads vendor build.prop as vendor_init,
# which with SELinux enforcing drops the adb props ("Do not have permissions to set"), and product
# loads after system_ext, overriding Lineage's ro.adb.secure=1 there.
PRODUCT_PRODUCT_PROPERTIES += \
    ro.adb.secure=0 \
    persist.sys.usb.config=adb \
    service.adb.root=1 \
    service.adb.tcp.port=5555

# Default timezone for deployment region (product: vendor_init may not set persist.sys.*)
PRODUCT_PRODUCT_PROPERTIES += \
    persist.sys.timezone=Europe/Amsterdam

# Silence noisy HWC DRM log tag
PRODUCT_PROPERTY_OVERRIDES += \
    log.tag.drmhwc=SILENT

# Disable power stats collection (reduces background CPU usage)
PRODUCT_PRODUCT_PROPERTIES += \
    persist.sys.powerstats.enabled=false
