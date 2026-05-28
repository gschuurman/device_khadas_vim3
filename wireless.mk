# =============================================================================
# Wireless connectivity tuning — WiFi regulatory, Android Auto
# =============================================================================

# NL (Netherlands) country code: enables 5 GHz ETSI channels 36-64 and 100-165
PRODUCT_VENDOR_PROPERTIES += \
    ro.boot.wificountrycode=NL

# WiFi regulatory database for cfg80211 — required for NL domain to be applied.
# Without this the kernel falls back to world domain (00) and marks all
# 5 GHz channels NO-IR, making 5 GHz non-functional.
PRODUCT_COPY_FILES += \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db:$(TARGET_COPY_OUT_VENDOR)/firmware/regulatory.db \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db.p7s:$(TARGET_COPY_OUT_VENDOR)/firmware/regulatory.db.p7s

# WiFi Direct — required for Wireless Android Auto.
# car.mk disables this by default; we explicitly re-enable it here.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml

# Android Auto Projection RRO
PRODUCT_PACKAGES += \
    AndroidAutoProjectionRro

# WiFi stability fix RC (loaded by init from vendor/etc/init/)
PRODUCT_PACKAGES += \
    init.wifi_fix.rc
