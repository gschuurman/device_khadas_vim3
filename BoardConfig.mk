TARGET_VIM3 := true

# 1. Inherit Platform Config
include device/amlogic/yukawa/BoardConfig.mk


BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs

# 5. SEPolicy
BOARD_SEPOLICY_DIRS += device/khadas/vim3_car/sepolicy