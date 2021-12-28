# Build camera kernel driver
ifneq ($(TARGET_USES_QMAA),true)
ifneq ($(TARGET_BOARD_AUTO),true)
ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/camera.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/cameralog.ko
endif
endif
endif
