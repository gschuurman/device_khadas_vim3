#!/bin/bash

# This script flashes the device with the new partition layout.
# It uses fastbootd to handle dynamic partitions.
#
# Usage: flash.sh [--clean]
#   --clean  Repartition with `fastboot oem format` (clean partitions). Without it, only the images are
#            flashed and user data is kept. No partition is ever erased/formatted manually here.

set -e

CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=1 ;;
        *) echo "Usage: $0 [--clean]" >&2; exit 1 ;;
    esac
done

if [ "$CLEAN" = 1 ]; then
    echo "Formatting device..."
    fastboot oem format

    echo "Rebooting to fastbootd..."
    fastboot reboot bootloader
fi

echo "Flashing partitions..."
fastboot flash boot_a boot.img
fastboot flash boot_b boot.img
fastboot flash super super.img
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

echo "Flashing complete. Rebooting device..."
fastboot reboot
