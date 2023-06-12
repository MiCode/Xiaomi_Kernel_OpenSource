AUDIO_ROOT=$(PWD)
UAPI_OUT=$(PWD)
KERNEL_BINARY_DIR=$(KERNEL_SRC)/../kernel-build-artifacts

KBUILD_OPTIONS := AUDIO_ROOT=$(PWD)
KBUILD_OPTIONS += MODNAME=audio
KBUILD_OPTIONS += UAPI_OUT=$(PWD)

ifeq ($(TARGET_SUPPORT),qcs40x)
KBUILD_OPTIONS += CONFIG_ARCH_QCS405=y
endif
ifeq ($(TARGET_SUPPORT), sdmsteppe)
KBUILD_OPTIONS += CONFIG_ARCH_SM6150=y
endif
ifeq ($(TARGET_SUPPORT), qrbx210)
KBUILD_OPTIONS += CONFIG_ARCH_BENGAL=y
endif

subdir-ccflags-y += -I$(AUDIO_ROOT)/include/uapi/

obj-m := ipc/
obj-m += dsp/
obj-m += dsp/codecs/
obj-m += soc/
obj-m += asoc/
obj-m += asoc/codecs/
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sdmsteppe))
obj-m += asoc/codecs/wcd934x/
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), qcs40x))
obj-m += asoc/codecs/bolero/
obj-m += asoc/codecs/csra66x0/
obj-m += asoc/codecs/ep92/
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), qrbx210))
obj-m += asoc/codecs/rouleur/
obj-m += asoc/codecs/bolero/
obj-m += asoc/codecs/wcd937x/
KBUILD_CFLAGS += -Wno-error
endif
ifeq ($(TARGET_SUPPORT), sdmsteppe)
obj-m += asoc/codecs/bolero/
obj-m += asoc/codecs/wcd937x/
endif

ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sa8155 sa8155ivi sa6155 sa8195 qtiquingvm qtiquingvm8295))
KBUILD_OPTIONS += CONFIG_SND_SOC_AUTO=y
obj-m := ipc/
obj-m += dsp/
obj-m += asoc/
obj-m += asoc/codecs/
obj-m += soc/
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sa8155 sa8155ivi sa8195))
KBUILD_OPTIONS += CONFIG_SND_SOC_SA8155=m
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sa6155))
KBUILD_OPTIONS += CONFIG_SND_SOC_SA6155=m
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), qtiquingvm))
KBUILD_OPTIONS += CONFIG_SND_SOC_GVM=m
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), qtiquingvm8295))
KBUILD_OPTIONS += CONFIG_SND_SOC_GVM_SA8295=m
endif
endif

all:
	$(shell rm -fr $(shell pwd)/soc/core.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/core.h $(shell pwd)/soc/core.h)
	$(shell rm -fr $(shell pwd)/include/soc/internal.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/base/regmap/internal.h $(shell pwd)/include/soc/internal.h)
	$(shell rm -fr $(shell pwd)/soc/pinctrl-utils.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/pinctrl-utils.h $(shell pwd)/soc/pinctrl-utils.h)
	$(shell rm -fr $(shell pwd)/include/soc/qcom/secure_buffer.h)
	$(shell ln -s $(KERNEL_SRC)/include/soc/qcom/secure_buffer.h $(shell pwd)/include/soc/qcom/secure_buffer.h)
	$(shell mkdir -p $(shell pwd)/linux)
	$(shell mkdir -p $(shell pwd)/sound)
	$(shell mkdir -p $(shell pwd)/linux/mfd)
	$(shell mkdir -p $(shell pwd)/linux/mfd/wcd9xxx)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/avtimer.h $(UAPI_OUT)/linux/avtimer.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_aac.h $(UAPI_OUT)/linux/msm_audio_aac.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_ac3.h $(UAPI_OUT)/linux/msm_audio_ac3.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_alac.h $(UAPI_OUT)/linux/msm_audio_alac.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_amrnb.h $(UAPI_OUT)/linux/msm_audio_amrnb.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_amrwb.h $(UAPI_OUT)/linux/msm_audio_amrwb.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_amrwbplus.h $(UAPI_OUT)/linux/msm_audio_amrwbplus.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_ape.h $(UAPI_OUT)/linux/msm_audio_ape.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_calibration.h $(UAPI_OUT)/linux/msm_audio_calibration.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_g711_dec.h $(UAPI_OUT)/linux/msm_audio_g711_dec.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_g711.h $(UAPI_OUT)/linux/msm_audio_g711.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio.h $(UAPI_OUT)/linux/msm_audio.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_mvs.h $(UAPI_OUT)/linux/msm_audio_mvs.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_qcp.h $(UAPI_OUT)/linux/msm_audio_qcp.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_sbc.h $(UAPI_OUT)/linux/msm_audio_sbc.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_voicememo.h $(UAPI_OUT)/linux/msm_audio_voicememo.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_wma.h $(UAPI_OUT)/linux/msm_audio_wma.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/msm_audio_wmapro.h $(UAPI_OUT)/linux/msm_audio_wmapro.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/wcd-spi-ac-params.h $(UAPI_OUT)/linux/wcd-spi-ac-params.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/mfd/wcd9xxx/wcd9320_registers.h $(UAPI_OUT)/linux/mfd/wcd9xxx/wcd9320_registers.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/linux/mfd/wcd9xxx/wcd9xxx_registers.h $(UAPI_OUT)/linux/mfd/wcd9xxx/wcd9xxx_registers.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/audio_compressed_formats.h $(UAPI_OUT)/sound/audio_compressed_formats.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/audio_effects.h $(UAPI_OUT)/sound/audio_effects.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/audio_slimslave.h $(UAPI_OUT)/sound/audio_slimslave.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/devdep_params.h $(UAPI_OUT)/sound/devdep_params.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/lsm_params.h $(UAPI_OUT)/sound/lsm_params.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/msmcal-hwdep.h $(UAPI_OUT)/sound/msmcal-hwdep.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/voice_params.h $(UAPI_OUT)/sound/voice_params.h)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(AUDIO_ROOT)/include/uapi/audio/sound/wcd-dsp-glink.h $(UAPI_OUT)/sound/wcd-dsp-glink.h)
	$(shell mkdir -p $(KERNEL_BINARY_DIR)/usr/include/audio/sound)
	$(shell mkdir -p $(KERNEL_BINARY_DIR)/usr/include/audio/linux/mfd)
	$(shell mkdir -p $(KERNEL_BINARY_DIR)/usr/include/audio/linux/mfd/wcd9xxx)
	$(shell cp $(UAPI_OUT)/linux/*.h $(KERNEL_BINARY_DIR)/usr/include/audio/linux/)
	$(shell cp $(UAPI_OUT)/linux/mfd/wcd9xxx/*.h $(KERNEL_BINARY_DIR)/usr/include/audio/linux/mfd/wcd9xxx/)
	$(shell cp $(UAPI_OUT)/sound/*.h $(KERNEL_BINARY_DIR)/usr/include/audio/sound/)
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(shell pwd) modules_install

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
