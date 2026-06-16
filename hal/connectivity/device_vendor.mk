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
# android.hardware.wifi-service is the IWifi vendor HAL. Without it the framework
# logs "Vendor Hal not supported" and can't manage STA/AP/P2P ifaces over nl80211.
# It is required, but it is NOT sufficient for WiFi-Direct on brcmfmac: the HAL's
# P2P path never creates a netdev (it assumes a driver-provided "p2p0"), so P2P also
# needs the wifi.direct.interface override below to target brcmfmac's real P2P-device
# interface. Both together make wireless Android Auto's P2P come up.
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    libwpa_client \
    wificond \
    wpa_cli \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

# wifi.direct.interface: the mainline brcmfmac driver exposes its P2P management
# interface as a non-netdev P2P-device wdev named "p2p-dev-wlan0", NOT as a "p2p0"
# netdev. The default (p2p0) makes the framework/wpa_supplicant hunt for a netdev that
# never exists ("Could not read interface p2p0 flags: No such device"), so WiFi-Direct
# stays in P2pDisabledState and wireless Android Auto (HeadUnit Revived) can't form a
# group. Pointing the P2P stack at the real P2P-device iface fixes it (verified live:
# P2P enables, discovers, and the phone connects). The HAL reads this in
# getPredefinedP2pIfaceName() and honors the "p2p-dev-" prefix.
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.direct.interface=p2p-dev-wlan0 \
    wifi.supplicant_scan_interval=15

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    device/khadas/vim3/hal/connectivity/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    device/khadas/vim3/hal/connectivity/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf
