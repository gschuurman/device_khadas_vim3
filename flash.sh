#!/bin/bash

# This script flashes the device with the new partition layout.
# It uses fastbootd to handle dynamic partitions.

set -e


echo "Formatting device..."
fastboot oem format

echo "Rebooting to fastbootd..."
fastboot reboot bootloader

echo "Flashing partitions..."
fastboot flash boot_a boot.img
fastboot flash boot_b boot.img
fastboot flash super super.img
fastboot flash userdata userdata.img
fastboot flash dtbo_a dtbo.img
fastboot flash dtbo_b dtbo.img
fastboot flash vbmeta_a vbmeta.img
fastboot flash vbmeta_b vbmeta.img
fastboot flash vbmeta_vendor_dlkm_a vbmeta_vendor_dlkm.img
fastboot flash vbmeta_vendor_dlkm_b vbmeta_vendor_dlkm.img
fastboot flash vbmeta_system_dlkm_a vbmeta_system_dlkm.img
fastboot flash vbmeta_system_dlkm_b vbmeta_system_dlkm.img
fastboot flash vendor_boot_a vendor_boot.img
fastboot flash vendor_boot_b vendor_boot.img
fastboot flash init_boot_a init_boot.img
fastboot flash init_boot_b init_boot.img

echo "Erasing misc and frp partitions..."
fastboot erase misc
fastboot erase frp

echo "Formatting metadata partition..."
fastboot format metadata

echo "Flashing complete. Rebooting device..."
fastboot reboot
