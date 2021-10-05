AUDIO_ROOT=$(PWD)
UAPI_OUT=$(PWD)

KBUILD_OPTIONS := AUDIO_ROOT=$(PWD)
KBUILD_OPTIONS += MODNAME=audio
KBUILD_OPTIONS += UAPI_OUT=$(PWD)

ifeq ($(TARGET_SUPPORT),qcs40x)
KBUILD_OPTIONS += CONFIG_ARCH_QCS405=y
endif
ifeq ($(TARGET_SUPPORT), sdmsteppe)
KBUILD_OPTIONS += CONFIG_ARCH_SM6150=y
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
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(shell pwd) modules_install

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
