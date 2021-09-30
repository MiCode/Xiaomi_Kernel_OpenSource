# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for kernel SPMI framework.
#
obj-$(CONFIG_SPMI)	+= spmi.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
obj-$(CONFIG_SPMI_MSM_PMIC_ARB)	+= spmi-pmic-arb.o
obj-$(CONFIG_SPMI_MTK_PMIF)	+= spmi-mtk-pmif.o spmi-mtk-mpu.o
spmi-mtk-pmif-objs := spmi-mtk-pmif-core.o spmi-mtk-debug.o
