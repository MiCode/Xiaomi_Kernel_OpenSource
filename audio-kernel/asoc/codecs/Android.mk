# Android makefile for audio kernel modules

# Assume no targets will be supported

# Check if this driver needs be built for current target
ifeq ($(call is-board-platform,sdm845),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SDM845=m
endif

ifeq ($(call is-board-platform-in-list,sdm710 qcs605),true)
AUDIO_SELECT  := CONFIG_SND_SOC_SDM670=m
endif

ifeq ($(call is-board-platform-in-list,msm8953 msm8937),true)
AUDIO_SELECT  += CONFIG_SND_SOC_SDM450=m
AUDIO_SELECT  += CONFIG_SND_SOC_EXT_CODEC_SDM450=m
endif

ifeq ($(call is-board-platform-in-list,msm8909),true)
AUDIO_SELECT  += CONFIG_SND_SOC_BG_8909=m
AUDIO_SELECT  += CONFIG_SND_SOC_8909_DIG_CDC=m
endif

ifeq ($(strip $(FACTORY_VERSION_MODE)), true)
AUDIO_SELECT  += CONFIG_SND_SOC_MBHC_RETRY=y
endif

AUDIO_CHIPSET := audio
# Build/Package only in case of supported target
ifeq ($(call is-board-platform-in-list,msm8909 msm8953 msm8937 sdm845 sdm710 qcs605),true)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
	AUDIO_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/audio-kernel
endif # opensource

ifeq ($(AUDIO_FEATURE_ENABLED_DLKM_8909W),true)
DLKM_DIR := $(TOP)/device/qcom/msm8909w/common/dlkm
else
DLKM_DIR := $(TOP)/device/qcom/common/dlkm
endif

# Build audio.ko as $(AUDIO_CHIPSET)_audio.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := AUDIO_ROOT=$(AUDIO_BLD_DIR)

# We are actually building audio.ko here, as per the
# requirement we are specifying <chipset>_audio.ko as LOCAL_MODULE.
# This means we need to rename the module to <chipset>_audio.ko
# after audio.ko is built.
KBUILD_OPTIONS += MODNAME=wcd_core_dlkm
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(AUDIO_SELECT)

###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wcd_core.ko
LOCAL_MODULE_KBUILD_NAME  := wcd_core_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wcd9xxx.ko
LOCAL_MODULE_KBUILD_NAME  := wcd9xxx_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
ifeq ($(call is-board-platform-in-list,msm8909 msm8953 msm8937 sdm710 qcs605),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wcd_cpe.ko
LOCAL_MODULE_KBUILD_NAME  := wcd_cpe_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifeq ($(call is-board-platform-in-list,msm8909 msm8953 msm8937 sdm439),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_aw87329_audio.ko
LOCAL_MODULE_KBUILD_NAME  := aw87329_audio_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifeq ($(call is-board-platform-in-list,msm8909 msm8953 msm8937 sdm439),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_aw87519_audio.ko
LOCAL_MODULE_KBUILD_NAME  := aw87519_audio_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifeq ($(call is-board-platform-in-list, sdm710 qcs605),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wcd_spi.ko
LOCAL_MODULE_KBUILD_NAME  := wcd_spi_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifeq ($(call is-board-platform-in-list,msm8953 msm8937 sdm710 qcs605),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wcd9335.ko
LOCAL_MODULE_KBUILD_NAME  := wcd9335_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifneq ($(AUDIO_FEATURE_ENABLED_DLKM_8909W),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wsa881x.ko
LOCAL_MODULE_KBUILD_NAME  := wsa881x_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
ifeq ($(call is-board-platform-in-list,msm8953 msm8937),true)
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_wsa881x_analog.ko
LOCAL_MODULE_KBUILD_NAME  := wsa881x_analog_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
endif
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_stub.ko
LOCAL_MODULE_KBUILD_NAME  := stub_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_mbhc.ko
LOCAL_MODULE_KBUILD_NAME  := mbhc_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################
include $(CLEAR_VARS)
LOCAL_MODULE              := $(AUDIO_CHIPSET)_hdmi.ko
LOCAL_MODULE_KBUILD_NAME  := hdmi_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################

endif # DLKM check
endif # supported target check
