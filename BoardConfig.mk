TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

# --- KERNEL BUILD CONFIG ---
TARGET_KERNEL_SOURCE := kernel/khadas/vim3
TARGET_KERNEL_VERSION := 6.12

# Defconfig: Voor Amlogic mainline is dit vaak 'yukawa_defconfig' of 'gki_defconfig'.
# We gokken hier op yukawa_defconfig, maar check stap 3 als dit faalt.
TARGET_KERNEL_CONFIG := gki_defconfig
TARGET_KERNEL_ADDITIONAL_CONFIG := amlogic_gki.fragment

TARGET_KERNEL_CLANG_COMPILE := true

# DTB Instellingen voor VIM3
TARGET_KERNEL_DTB := meson-g12b-a311d-khadas-vim3
BOARD_INCLUDE_DTB_IN_BOOTIMG := true
BOARD_BOOT_HEADER_VERSION := 4

BOARD_KERNEL_IMAGE_NAME := Image

# 2. Laad DAARNA pas de platform config
# Nu kan yukawa de bovenstaande variabelen gebruiken om het pad correct te maken (met .txt)
include device/amlogic/yukawa/BoardConfig.mk

# 3. Overige instellingen
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SEPOLICY_DIRS += device/khadas/vim3/sepolicy