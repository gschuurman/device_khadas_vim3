# Native broadcast radio backed by an RTL-SDR USB dongle.
#
# P0 (bring-up): librtlsdr + CLI tools for on-device hardware validation.
# P2 (FM): the BroadcastRadio HAL service that replaces
# android.hardware.broadcastradio-service.default (swapped in car.mk).

# The rtlsdr BroadcastRadio HAL (FM now, DAB in P3). Statically links librtlsdr
# + libfmdemod; runtime shared deps are libusb.so + libtinyalsa.so.
PRODUCT_PACKAGES += \
    android.hardware.broadcastradio-service.rtlsdr

# Bring-up/debug tools. They statically link librtlsdr; the only runtime
# shared dep is libusb.so (already a vendor lib).
PRODUCT_PACKAGES += \
    rtl_test \
    rtl_fm \
    rtl_sdr \
    rtl_tcp \
    rtl_eeprom \
    rtl_biast
