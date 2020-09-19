# SPDX-License-Identifier: GPL-2.0

obj-y += adsp.o
adsp-y := adsp_core.o adsp_fs.o adsp_semaphore.o adsp_logger.o adsp_feature_table.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_adsp_core.o += -DDEBUG
CFLAGS_adsp_fs.o += -DDEBUG
CFLAGS_adsp_logger.o += -DDEBUG
CFLAGS_adsp_semaphore.o += -DDEBUG
CFLAGS_adsp_feature_table.o += -DDEBUG
endif
