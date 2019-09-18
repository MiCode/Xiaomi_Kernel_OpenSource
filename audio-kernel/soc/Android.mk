# Android makefile for audio kernel modules

# Assume no targets will be supported

# Check if this driver needs be built for current target
ifeq ($(call is-board-platform,sdm845),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SDM845=m
endif

ifeq ($(call is-board-platform-in-list,msm8953 sdm670 qcs605),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SDM670=m
endif

ifeq ($(call is-board-platform,msmnile),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SM8150=m
endif

ifeq ($(call is-board-platform-in-list,$(MSMSTEPPE) $(TRINKET) atoll),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SM6150=m
endif

AUDIO_CHIPSET := audio
# Build/Package only in case of supported target
ifeq ($(call is-board-platform-in-list,msm8953 sdm845 sdm670 qcs605 msmnile $(MSMSTEPPE) $(TRINKET) atoll),true)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
	AUDIO_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/audio-kernel
endif # opensource

DLKM_DIR := $(TOP)/device/qcom/common/dlkm

# Build audio.ko as $(AUDIO_CHIPSET)_audio.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := AUDIO_ROOT=$(AUDIO_BLD_DIR)

# We are actually building audio.ko here, as per the
# requirement we are specifying <chipset>_audio.ko as LOCAL_MODULE.
# This means we need to rename the module to <chipset>_audio.ko
# after audio.ko is built.
KBUILD_OPTIONS += MODNAME=soc_dlkm
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(AUDIO_SELECT)

###########################################################
ifeq ($(call is-board-platform-in-list,msm8953 sdm670 qcs605 $(MSMSTEPPE) atoll),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_pinctrl_lpi.ko
LOCAL_MODULE_KBUILD_NAME  := pinctrl_lpi_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_pinctrl_wcd.ko
LOCAL_MODULE_KBUILD_NAME  := pinctrl_wcd_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_swr.ko
LOCAL_MODULE_KBUILD_NAME  := swr_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_swr_ctrl.ko
LOCAL_MODULE_KBUILD_NAME  := swr_ctrl_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
ifeq ($(call is-board-platform-in-list, $(MSMSTEPPE) atoll $(TRINKET)),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_snd_event.ko
LOCAL_MODULE_KBUILD_NAME  := snd_event_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################

endif # DLKM check
endif # supported target check
