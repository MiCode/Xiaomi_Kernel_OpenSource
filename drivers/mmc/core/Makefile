# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel mmc core.
#
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/ComboA/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/devfreq/

obj-$(CONFIG_MMC)		+= mmc_core.o
mmc_core-y			:= core.o bus.o host.o \
				   mmc.o mmc_ops.o sd.o sd_ops.o \
				   sdio.o sdio_ops.o sdio_bus.o \
				   sdio_cis.o sdio_io.o sdio_irq.o \
				   slot-gpio.o
mmc_core-$(CONFIG_OF)		+= pwrseq.o
obj-$(CONFIG_PWRSEQ_SIMPLE)	+= pwrseq_simple.o
obj-$(CONFIG_PWRSEQ_SD8787)	+= pwrseq_sd8787.o
obj-$(CONFIG_PWRSEQ_EMMC)	+= pwrseq_emmc.o
mmc_core-$(CONFIG_DEBUG_FS)	+= debugfs.o
obj-$(CONFIG_MMC_BLOCK)		+= mmc_block.o
mmc_block-objs			:= block.o queue.o
obj-$(CONFIG_MMC_TEST)		+= mmc_test.o
obj-$(CONFIG_SDIO_UART)		+= sdio_uart.o
obj-$(CONFIG_MMC_FFU)		+= ffu.o
obj-$(CONFIG_MMC_BLOCK_IO_LOG)	+= mtk_mmc_block.o

obj-$(CONFIG_MTK_MMC_PWR_WP) 	+= mtk_emmc_write_protect.o
