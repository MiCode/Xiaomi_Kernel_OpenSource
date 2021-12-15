# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

obj-$(CONFIG_MEDIATEK_DRAMC)	+= dramc_old.o
obj-$(CONFIG_MEDIATEK_DRAMC)	+= dramc_bin_old.o
ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6739" "mt6785"))
obj-$(CONFIG_MTK_DRAMC)	+= dramc.o
obj-$(CONFIG_MTK_DRAMC)	+= dramc_bin.o
endif

obj-$(CONFIG_MEDIATEK_EMI)	+= emicen_old.o
obj-$(CONFIG_MEDIATEK_EMI)	+= emimpu_old.o
obj-$(CONFIG_MEDIATEK_EMI)	+= emiisu_old.o

ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6785"))
obj-$(CONFIG_MTK_EMI)	+= emicen.o
obj-$(CONFIG_MTK_EMI)	+= emimpu.o
obj-$(CONFIG_MTK_EMI)	+= emiisu.o
endif