DISPLAY_SELECT := CONFIG_DRM_MSM=m

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq (1, $(strip $(FACTORY_BUILD)))
	DISPLAY_FACTORY_BUILD := CONFIG_DISPLAY_FACTORY_BUILD=1
endif

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
	DISPLAY_BLD_DIR := $(TOP)/vendor/qcom/opensource/display-drivers
endif # opensource

DLKM_DIR := $(TOP)/device/qcom/common/dlkm

LOCAL_ADDITIONAL_DEPENDENCIES := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)

# Build display.ko as msm_drm.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := DISPLAY_ROOT=$(DISPLAY_BLD_DIR)
KBUILD_OPTIONS += MODNAME=msm_drm
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(DISPLAY_SELECT)
KBUILD_OPTIONS += $(DISPLAY_FACTORY_BUILD)

KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(PWD)/$(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := msm_drm-module-symvers
LOCAL_MODULE_STEM         := Module.symvers
LOCAL_MODULE_KBUILD_NAME  := Module.symvers
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
# Include kp_module.ko in the /vendor/lib/modules (vendor.img)
# BOARD_VENDOR_KERNEL_MODULES += $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

###########################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := msm_drm.ko
LOCAL_MODULE_KBUILD_NAME  := msm_drm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)

LOCAL_REQUIRED_MODULES    := mmrm-module-symvers
LOCAL_ADDITIONAL_DEPENDENCIES := $(call intermediates-dir-for,DLKM,mmrm-module-symvers)/Module.symvers

include $(DLKM_DIR)/Build_external_kernelmodule.mk
###########################################################
endif # DLKM check
