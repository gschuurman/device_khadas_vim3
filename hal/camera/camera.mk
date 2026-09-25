# =============================================================================
# Camera HAL — USB external camera + VIM3 rear-view camera app
# =============================================================================

# USB external camera HAL
PRODUCT_PACKAGES += \
    android.hardware.camera.provider-V1-external-service

# Rear-view camera app (schuurman)
PRODUCT_PACKAGES += \
    RearViewCamera \
    privapp-permissions-com.schuurman.rvc

# Camera HAL configuration files
PRODUCT_COPY_FILES += \
    device/khadas/vim3/hal/camera/camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/camera_config.xml \
    device/khadas/vim3/hal/camera/external_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/external_camera_config.xml

# Rear view camera during boot: rvc_early (packages/apps/RearViewCamera/early) shows the camera in
# reverse before Android has booted, in rvc_display's layer (not the deprecated EVS car display
# proxy). cameraserver only serves it (AID_AUTOMOTIVE_EVS, before system_server) for an exterior
# system camera, which the external camera HAL reports when this property is set. Side effect:
# third-party apps can't use the USB camera.
PRODUCT_PACKAGES += \
    rvc_early

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.camera.external.automotive_location=exterior_rear
