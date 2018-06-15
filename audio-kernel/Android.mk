# Android makefile for audio kernel modules
MY_LOCAL_PATH := $(call my-dir)

UAPI_OUT := $(OUT)/obj/vendor/qcom/opensource/audio-kernel/include

ifeq ($(call is-board-platform-in-list,msm8953 sdm845 sdm710 qcs605),true)
$(shell mkdir -p $(UAPI_OUT)/linux;)
$(shell mkdir -p $(UAPI_OUT)/sound;)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/ipc/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/dsp/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/dsp/codecs/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/soc/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd934x/Module.symvers)

include $(MY_LOCAL_PATH)/ipc/Android.mk
include $(MY_LOCAL_PATH)/dsp/Android.mk
include $(MY_LOCAL_PATH)/dsp/codecs/Android.mk
include $(MY_LOCAL_PATH)/soc/Android.mk
include $(MY_LOCAL_PATH)/asoc/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/wcd934x/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/tas2557/Android.mk
endif

ifeq ($(call is-board-platform-in-list,msm8953 sdm710 qcs605),true)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/sdm660_cdc/Module.symvers)
$(shell rm -rf $(OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/msm_sdw/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/sdm660_cdc/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/msm_sdw/Android.mk
endif
