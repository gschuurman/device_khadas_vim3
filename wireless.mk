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

# WiFi Aware (NAN) — enables faster peer discovery for Wireless Android Auto
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.aware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.aware.xml

# CONFIG_MODPROBE_PATH target (see modprobe_vim3/). NOT actually needed for
# brcmfmac's fwvid vendor-plugin (brcmfmac_wcc) anymore -- that's now
# statically linked into brcmfmac.ko (see kernel_khadas_vim3's brcmfmac
# Makefile/fwvid.c fork patch), so it never calls request_module() at all.
# Kept as a general-purpose safety net for any future driver on this board
# that does rely on runtime module autoload -- see the matching
# CONFIG_MODPROBE_PATH/CONFIG_STATIC_USERMODEHELPER_PATH comment in
# kernel_khadas_vim3_overlay's vim3_extra.fragment.
PRODUCT_PACKAGES += \
    modprobe_vim3

# AIC8800D80 USB WiFi dongle (Ugreen adapter, secondary radio alongside the
# onboard BCM4359). Enumerates as a fake mass-storage device (a69c:5723)
# until mode-switched; see aic8800_modeswitch/.
PRODUCT_PACKAGES += \
    aic8800_modeswitch

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,device/khadas/vim3/aic8800_modeswitch/fw/aic8800D80,$(TARGET_COPY_OUT_VENDOR)/firmware/aic8800D80)

