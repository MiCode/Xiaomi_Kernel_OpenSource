ifeq ($(call is-board-platform-in-list, $(TARGET_BOARD_PLATFORM)),true)

# Make target to specify building the camera.ko from within Android build system.
LOCAL_PATH := $(call my-dir)
# Path to DLKM make scripts
DLKM_DIR := $(TOP)/device/qcom/common/dlkm

# Kbuild options
KBUILD_OPTIONS := CAMERA_KERNEL_ROOT=$(shell pwd)/$(LOCAL_PATH)
KBUILD_OPTIONS += KERNEL_ROOT=$(shell pwd)/kernel/msm-$(TARGET_KERNEL_VERSION)/
KBUILD_OPTIONS += MODNAME=camera
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)

ifeq ($(TARGET_BOARD_PLATFORM), taro)
	KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(shell pwd)/$(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
endif

ifeq ($(TARGET_BOARD_PLATFORM), parrot)
	KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(shell pwd)/$(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
endif

ifeq ($(TARGET_BOARD_PLATFORM), thor)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(shell pwd)/$(KERNEL_MODULES_OUT)/Module.symvers
endif

# Clear shell environment variables from previous android module during build
include $(CLEAR_VARS)
# For incremental compilation support.
LOCAL_SRC_FILES             :=  \
                                $(shell find $(LOCAL_PATH)/config -L -type f)      \
                                $(shell find $(LOCAL_PATH)/drivers -L -type f)     \
                                $(shell find $(LOCAL_PATH)/dt-bindings -L -type f) \
                                $(shell find $(LOCAL_PATH)/include -L -type f)     \
                                $(LOCAL_PATH)/Android.mk \
                                $(LOCAL_PATH)/board.mk   \
                                $(LOCAL_PATH)/product.mk \
                                $(LOCAL_PATH)/Kbuild
LOCAL_MODULE_PATH           := $(KERNEL_MODULES_OUT)
LOCAL_MODULE                := camera.ko
LOCAL_MODULE_TAGS           := optional
#LOCAL_MODULE_KBUILD_NAME   := camera.ko
#LOCAL_MODULE_DEBUG_ENABLE  := true

ifeq ($(TARGET_BOARD_PLATFORM), taro)
	LOCAL_REQUIRED_MODULES        := mmrm-module-symvers
	LOCAL_ADDITIONAL_DEPENDENCIES := $(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
endif

ifeq ($(TARGET_BOARD_PLATFORM), parrot)
	LOCAL_REQUIRED_MODULES        := mmrm-module-symvers
	LOCAL_ADDITIONAL_DEPENDENCIES := $(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers
endif

ifeq ($(TARGET_BOARD_PLATFORM), lahaina)
# Include Kernel DLKM Android.mk target to place generated .ko file in image
include $(DLKM_DIR)/AndroidKernelModule.mk
# Include Camera UAPI Android.mk target to copy headers
include $(LOCAL_PATH)/include/uapi/Android.mk
else
include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif
include $(CLEAR_VARS)
# For incremental compilation support.
LOCAL_SRC_FILES             :=  \
                                $(shell find $(LOCAL_PATH)/config -L -type f)      \
                                $(shell find $(LOCAL_PATH)/drivers -L -type f)     \
                                $(shell find $(LOCAL_PATH)/dt-bindings -L -type f) \
                                $(shell find $(LOCAL_PATH)/include -L -type f)     \
                                $(LOCAL_PATH)/Android.mk \
                                $(LOCAL_PATH)/board.mk   \
                                $(LOCAL_PATH)/product.mk \
                                $(LOCAL_PATH)/Kbuild
LOCAL_MODULE_PATH           := $(KERNEL_MODULES_OUT)
LOCAL_MODULE                := cameralog.ko
LOCAL_MODULE_TAGS           := optional
include $(DLKM_DIR)/Build_external_kernelmodule.mk


endif # End of check for board platform
