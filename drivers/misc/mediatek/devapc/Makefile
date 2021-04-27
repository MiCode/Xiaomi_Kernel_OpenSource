# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

LOCAL_PATH := $(srctree)/drivers/misc/mediatek/devapc

subdir-ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDBG_ENABLE

ifneq ($(wildcard $(LOCAL_PATH)/$(MTK_PLATFORM)/Makefile),)
  obj-$(CONFIG_DEVAPC_LEGACY) += $(MTK_PLATFORM)/
else
  # Core
  obj-$(CONFIG_DEVAPC_ARCH_V1) += devapc-mtk-common.o
  obj-$(CONFIG_DEVAPC_ARCH_V2) += devapc-mtk-multi-ao.o

  # SoC
  obj-$(CONFIG_DEVAPC_MT6779) += devapc-mt6779.o
  obj-$(CONFIG_DEVAPC_MT6768) += devapc-mt6768.o
  obj-$(CONFIG_DEVAPC_MT6785) += devapc-mt6785.o
  obj-$(CONFIG_DEVAPC_MT6781) += devapc-mt6781.o
  obj-$(CONFIG_DEVAPC_MT6885) += devapc-mt6885.o
  obj-$(CONFIG_DEVAPC_MT6873) += devapc-mt6873.o
  obj-$(CONFIG_DEVAPC_MT6853) += devapc-mt6853.o
  obj-$(CONFIG_DEVAPC_MT6833) += devapc-mt6833.o
  obj-$(CONFIG_DEVAPC_MT6877) += devapc-mt6877.o
endif
