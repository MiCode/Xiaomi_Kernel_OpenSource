# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v0/
fhctl-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_SSPM) += clk-fhctl-sspm.o


