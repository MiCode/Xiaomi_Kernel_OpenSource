# SPDX-License-Identifier: GPL-2.0

LOCAL_PATH := $(srctree)/drivers/misc/mediatek/devapc

subdir-ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDBG_ENABLE

ifneq ($(wildcard $(LOCAL_PATH)/$(MTK_PLATFORM)/Makefile),)
   $(info DEVAPC legacy driver build...)
    obj-y += $(MTK_PLATFORM)/
else
  # Core
  obj-$(CONFIG_MTK_DEVAPC) += devapc-mtk-common.o

  # SoC
  obj-$(CONFIG_DEVAPC_MT6779) += devapc-mt6779.o
  obj-$(CONFIG_DEVAPC_MT6768) += devapc-mt6768.o
  obj-$(CONFIG_DEVAPC_MT6785) += devapc-mt6785.o
endif
