# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

ifneq ($(filter y m,$(CONFIG_MTK_EMI_LEGACY_V0)),)
ccflags-y += -I$(srctree)/drivers/memory/mediatek/emi_legacy_v0/
endif

obj-$(CONFIG_MTK_DRAMC_LEGACY) := dramc.o
