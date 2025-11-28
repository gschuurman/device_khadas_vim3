TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

BOARD_KERNEL_IMAGE_NAME := Image.lz4
# 2. Laad DAARNA pas de platform config
# Nu kan yukawa de bovenstaande variabelen gebruiken om het pad correct te maken (met .txt)
include device/amlogic/yukawa/BoardConfig.mk

# 3. Overige instellingen
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SEPOLICY_DIRS += device/khadas/vim3/sepolicy