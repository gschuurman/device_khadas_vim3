# Build VIM3 Android DTBO image from the inline kernel build output.
# The main kernel build already compiles the dtbo (via dtb-y in the amlogic
# DTS Makefile); this task packs it into dtbo.img for AVB signing.

MKDTBOIMG_VIM3 := $(HOST_OUT_EXECUTABLES)/mkdtboimg
VIM3_DTBO_OBJ := $(KERNEL_OUT)/arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3-android.dtbo

$(BOARD_PREBUILT_DTBOIMAGE): $(MKDTBOIMG_VIM3) $(TARGET_PREBUILT_INT_KERNEL)
	mkdir -p $(dir $@)
	$(MKDTBOIMG_VIM3) create $@ --page_size=4096 $(VIM3_DTBO_OBJ)
