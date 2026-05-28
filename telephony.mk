# =============================================================================
# Telephony stubs — AAOS build without a SIM/cellular radio
# =============================================================================

# Declare that there is no radio interface
PRODUCT_VENDOR_PROPERTIES += \
    ro.radio.noril=true

# Stub telephony stack required by CarService and system apps
PRODUCT_PACKAGES += \
    com.android.tethering \
    NetworkStack \
    CaptivePortalLogin \
    Telecom \
    TeleService \
    TelephonyProvider \
    MmsService \
    ContactsProvider \
    liblargeparcelablejni

# Permission XMLs for telephony features we declare but don't use for calls
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.telephony.subscription.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.subscription.xml \
    frameworks/native/data/etc/android.hardware.telephony.messaging.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.messaging.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml
