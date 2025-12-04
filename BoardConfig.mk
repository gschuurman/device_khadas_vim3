TARGET_VIM3 := true
TARGET_DEV_BOARD := vim3
TARGET_BOOTLOADER_BOARD_NAME := vim3

BOARD_KERNEL_IMAGE_NAME := Image.lz4

TARGET_NO_KERNEL := true

TARGET_SELINUX_ENFORCE := false
# TARGET_KERNEL_SOURCE := kernel/khadas/vim3
# TARGET_KERNEL_DTB := meson-g12b-a311d-khadas-vim3
# TARGET_KERNEL_CONFIG := gki_defconfig
# TARGET_KERNEL_ADDITIONAL_CONFIG := amlogic_gki.fragment

# 2. Laad DAARNA pas de platform config
# Nu kan yukawa de bovenstaande variabelen gebruiken om het pad correct te maken (met .txt)
include device/amlogic/yukawa/BoardConfig.mk

# 3. Overige instellingen
# BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
# BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SEPOLICY_DIRS += device/khadas/vim3/sepolicy


BOARD_SUPER_PARTITION_GROUPS := db_dynamic_partitions
BOARD_DB_DYNAMIC_PARTITIONS_PARTITION_LIST := system vendor
BOARD_SUPER_PARTITION_SIZE := $(shell echo $$(( 6144 * 1024 * 1024 )))
BOARD_DB_DYNAMIC_PARTITIONS_SIZE := $(shell echo $$(( $(BOARD_SUPER_PARTITION_SIZE)/2 - (10 * 1024 * 1024) )))  # Reserve 10M for DAP metadata
