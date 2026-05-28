# =============================================================================
# Developer / debug configuration
# =============================================================================

# ADB always on, root shell, network ADB on port 5555
PRODUCT_VENDOR_PROPERTIES += \
    ro.secure=0 \
    ro.adb.secure=0 \
    persist.sys.usb.config=adb \
    service.adb.root=1 \
    service.adb.tcp.port=5555

# Default timezone for deployment region
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.timezone=Europe/Amsterdam

# Silence noisy HWC DRM log tag
PRODUCT_PROPERTY_OVERRIDES += \
    log.tag.drmhwc=SILENT

# Disable power stats collection (reduces background CPU usage)
PRODUCT_VENDOR_PROPERTIES += \
    persist.sys.powerstats.enabled=false
