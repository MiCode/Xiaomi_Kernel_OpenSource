# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))


# scp spk include path
ifeq ($(CONFIG_SND_SOC_MTK_SCP_SMARTPA),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/scp_spk/
endif

# platform code
obj-$(CONFIG_SND_SOC_MTK_SCP_SMARTPA) += mtk-scp-spk-platform-mem-control.o

