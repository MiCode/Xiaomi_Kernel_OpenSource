# SPDX-License-Identifier: GPL-2.0

LOCAL_PATH := $(srctree)/drivers/misc/mediatek/devapc

subdir-ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDBG_ENABLE

ifneq ($(wildcard $(LOCAL_PATH)/$(MTK_PLATFORM)/Makefile),)
   $(info DEVAPC legacy driver build...)
    obj-$(CONFIG_DEVAPC_LEGACY) += $(MTK_PLATFORM)/
else
  # Core
  obj-$(CONFIG_DEVAPC_ARCH_V1) += devapc-mtk-common.o
  obj-$(CONFIG_DEVAPC_ARCH_V2) += devapc-mtk-multi-3.o

  # SoC
  obj-$(CONFIG_DEVAPC_MT6779) += devapc-mt6779.o
  obj-$(CONFIG_DEVAPC_MT6768) += devapc-mt6768.o
  obj-$(CONFIG_DEVAPC_MT6785) += devapc-mt6785.o
  obj-$(CONFIG_DEVAPC_MT6885) += devapc-mt6885.o
endif
