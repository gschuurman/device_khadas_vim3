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
