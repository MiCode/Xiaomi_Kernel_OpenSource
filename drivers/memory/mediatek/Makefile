# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
obj-$(CONFIG_MTK_EMI) += emi.o
emi-objs := emi-cen.o emi-isu.o emi-dbg.o
obj-$(CONFIG_MTK_EMI) += emi-mpu.o
obj-$(CONFIG_MTK_EMI_MPU_TEST) += emi-mpu-test.o
obj-$(CONFIG_MTK_DRAMC)    += mtk_dramc.o
mtk_dramc-objs	+= dramc.o dramc_bin.o
