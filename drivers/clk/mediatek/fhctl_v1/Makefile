# SPDX-License-Identifier: GPL-2.0
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1/
else ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v3/
endif

obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
fhctl-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_SSPM) += clk-fhctl-sspm.o
