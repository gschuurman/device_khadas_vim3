# Select the correct Mesa variant for VIM3 (A311D = Cortex-A73)
PRODUCT_SOONG_NAMESPACES += vendor/amlogic/yukawa/gpu/$(EXPECTED_YUKAWA_VENDOR_VERSION)/mesa/a73
PRODUCT_SOONG_NAMESPACES += external/minigbm/gbm_mesa_driver/a73
PRODUCT_PACKAGES += libgbm_mesa_wrapper_a73

PRODUCT_VENDOR_PROPERTIES += ro.sf.lcd_density=100

# opengles features
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.software.opengles.deqp.level-2021-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.opengles.deqp.level.xml

# Vulkan
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml:vendor/etc/permissions/android.hardware.vulkan.version.xml \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:vendor/etc/permissions/android.hardware.vulkan.compute.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:vendor/etc/permissions/android.hardware.vulkan.level.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2020-03-01.xml:vendor/etc/permissions/android.software.vulkan.deqp.level.xml

# Minigbm mapper/allocator
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator-service.minigbm \
    gralloc.minigbm \
    libminigbm_gralloc \
    mapper.minigbm

BOARD_VENDOR_SEPOLICY_DIRS += external/minigbm/cros_gralloc/sepolicy

# Mesa GBM backend path
PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.hwcomposer=drm \
    ro.hardware.egl=mesa \
    ro.hardware.vulkan=mesa \
    graphics.gpu.profiler.support=true \
    vendor.hwc.drm.device=/dev/dri/card1 \
    ro.hardware.gralloc=minigbm \
    vendor.gralloc.minigbm.backend=gbm_mesa \
    vendor.mesa.gbm_backends_path=/vendor/lib64/gbm \
    debug.renderengine.backend=skiaglthreaded \
    ro.vendor.hwc.use_overlay_planes=1 \
    vendor.hwc.drm.scale_with_gpu=0 \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=2 \
    ro.surface_flinger.max_graphics_buffers=2 \
    debug.sf.disable_client_composition_cache=1 \
    debug.sf.latch_unsignaled=0 \
    ro.surface_flinger.use_content_detection_for_refresh_rate=false \
    ro.surface_flinger.running_without_sync_framework=false \
    ro.surface_flinger.force_hwc_copy_for_virtual_displays=true

PRODUCT_PACKAGES += android.hardware.composer.hwc3-service.drm

PRODUCT_COPY_FILES += \
    device/khadas/vim3/hal/graphics/display_settings.xml:$(TARGET_COPY_OUT_VENDOR)/etc/display_settings.xml \
    device/khadas/vim3/hal/graphics/android.hardware.screen.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.screen.xml

PRODUCT_VENDOR_PROPERTIES += \
    debug.stagefright.c2inputsurface=-1

PRODUCT_VENDOR_PROPERTIES += \
    ro.opengles.version=196864

$(call inherit-product-if-exists, $(YUKAWA_VENDOR_PATH)/gpu/$(EXPECTED_YUKAWA_VENDOR_VERSION)/vendor.mk)
