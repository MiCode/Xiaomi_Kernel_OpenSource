AUDIO_ROOT=$(PWD)
UAPI_OUT=$(PWD)
HEADER_INSTALL_DIR=$(KERNEL_SRC)/scripts
KERNEL_BINARY_DIR=$(KERNEL_SRC)/../kernel-build-artifacts

KBUILD_OPTIONS := AUDIO_ROOT=$(PWD)
KBUILD_OPTIONS += MODNAME=audio
KBUILD_OPTIONS += UAPI_OUT=$(PWD)

AUDIO_KERNEL_HEADERS_PATH1 =  $(shell ls ./include/uapi/linux/*.h)
AUDIO_KERNEL_HEADERS_PATH2 =  $(shell ls ./include/uapi/linux/mfd/wcd9xxx/*.h)
AUDIO_KERNEL_HEADERS_PATH3 =  $(shell ls ./include/uapi/sound/*.h)

ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sdm670 qcs605))
KBUILD_OPTIONS += CONFIG_ARCH_SDM670=y
endif
ifeq ($(TARGET_SUPPORT),sdm845)
KBUILD_OPTIONS += CONFIG_ARCH_SDM845=y
endif
ifeq ($(TARGET_SUPPORT),apq8053)
KBUILD_OPTIONS += CONFIG_ARCH_SDM450=y
endif
ifeq ($(TARGET_SUPPORT),qcs40x)
KBUILD_OPTIONS += CONFIG_ARCH_QCS405=y
endif
ifeq ($(TARGET_SUPPORT), sdmsteppe))
KBUILD_OPTIONS += CONFIG_ARCH_SM6150=y
endif

obj-m := ipc/
obj-m += dsp/
obj-m += dsp/codecs/
obj-m += soc/
obj-m += asoc/
obj-m += asoc/codecs/
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sdm670 qcs605 sdmsteppe))
obj-m += asoc/codecs/wcd934x/
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), apq8053 sdm670 qcs605))
obj-m += asoc/codecs/sdm660_cdc/
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), sdm670 qcs605))
obj-m += asoc/codecs/msm_sdw/
endif
ifeq ($(TARGET_SUPPORT), $(filter $(TARGET_SUPPORT), qcs40x))
obj-m += asoc/codecs/bolero/
obj-m += asoc/codecs/csra66x0/
obj-m += asoc/codecs/ep92/
endif
ifeq ($(TARGET_SUPPORT), sdmsteppe))
obj-m += asoc/codecs/bolero/
obj-m += asoc/codecs/wcd937x/
endif

all:
	$(shell rm -fr $(shell pwd)/soc/core.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/core.h $(shell pwd)/soc/core.h)
	$(shell rm -fr $(shell pwd)/include/soc/internal.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/base/regmap/internal.h $(shell pwd)/include/soc/internal.h)
	$(shell rm -fr $(shell pwd)/soc/pinctrl-utils.h)
	$(shell ln -s $(KERNEL_SRC)/drivers/pinctrl/pinctrl-utils.h $(shell pwd)/soc/pinctrl-utils.h)
	$(shell mkdir $(shell pwd)/linux)
	$(shell mkdir $(shell pwd)/sound)
	$(shell mkdir $(shell pwd)/linux/mfd)
	$(shell mkdir $(shell pwd)/linux/mfd/wcd9xxx)
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(UAPI_OUT)/linux/ $(AUDIO_ROOT)/include/uapi/linux/ $(notdir $(AUDIO_KERNEL_HEADERS_PATH1)))
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(UAPI_OUT)/linux/mfd/wcd9xxx/ $(AUDIO_ROOT)/include/uapi/linux/mfd/wcd9xxx/ $(notdir $(AUDIO_KERNEL_HEADERS_PATH2)))
	$(shell cd $(KERNEL_BINARY_DIR) && $(KERNEL_SRC)/scripts/headers_install.sh $(UAPI_OUT)/sound/ $(AUDIO_ROOT)/include/uapi/sound/ $(notdir $(AUDIO_KERNEL_HEADERS_PATH3)))
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(shell pwd) modules_install

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
