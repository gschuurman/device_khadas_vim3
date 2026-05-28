# =============================================================================
# Bluetooth and WiFi HAL — BCM4359 (brcmfmac kernel driver + bcmdhd userspace)
# =============================================================================

# Bluetooth service
PRODUCT_PACKAGES += android.hardware.bluetooth-service.default

PRODUCT_PROPERTY_OVERRIDES += \
    bluetooth.core.gap.le.privacy.enabled=false \
    bluetooth.profile.asha.central.enabled=true \
    bluetooth.profile.a2dp.source.enabled=true \
    bluetooth.profile.avrcp.target.enabled=true \
    bluetooth.profile.bap.broadcast.assist.enabled=true \
    bluetooth.profile.bap.broadcast.source.enabled=true \
    bluetooth.profile.bap.unicast.client.enabled=true \
    bluetooth.profile.bas.client.enabled=true \
    bluetooth.profile.ccp.server.enabled=true \
    bluetooth.profile.csip.set_coordinator.enabled=true \
    bluetooth.profile.gatt.enabled=true \
    bluetooth.profile.hap.client.enabled=true \
    bluetooth.profile.hfp.ag.enabled=true \
    bluetooth.profile.hid.host.enabled=true \
    bluetooth.profile.mcp.server.enabled=true \
    bluetooth.profile.opp.enabled=true \
    bluetooth.profile.pan.nap.enabled=true \
    bluetooth.profile.pan.panu.enabled=true \
    bluetooth.profile.vcp.controller.enabled=true

# WiFi service and supplicant
PRODUCT_PACKAGES += \
    libwpa_client \
    wificond \
    wpa_cli \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.supplicant_scan_interval=15

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    device/khadas/vim3/hal/connectivity/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    device/khadas/vim3/hal/connectivity/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf
