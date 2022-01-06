#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#


### include
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/scp/include \
	-I$(srctree)/drivers/misc/mediatek/scp/cm4 \
	-I$(srctree)/drivers/misc/mediatek/adsp/common_v1 \
	-I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/audio_ipi/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/utility

### DO
ifeq ($(CONFIG_MTK_DO),y)
ccflags-y += -I$(srctree)/drivers/staging/dyn_obj/$(MTK_PLATFORM)
endif

### call
ifeq ($(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/call
endif

### mp3
ifeq ($(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/mp3
endif


### spkprotect for headfile include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/spkprotect
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common_v1/tasks/audplayback

### c files
obj-y += audio_ringbuf.o
obj-y += audio_ipi_dma.o
obj-y += audio_ipi_driver.o
obj-y += audio_ipi_queue.o
obj-y += audio_messenger_ipi.o
obj-y += audio_task_manager.o
obj-y += adsp_ipi_queue.o

ifeq ($(CONFIG_MT_ENG_BUILD),y)
CFLAGS_audio_ringbuf.o += -DDEBUG
CFLAGS_audio_ipi_dma.o += -DDEBUG
CFLAGS_audio_ipi_driver.o += -DDEBUG
CFLAGS_audio_ipi_queue.o += -DDEBUG
CFLAGS_audio_messenger_ipi.o += -DDEBUG
CFLAGS_audio_task_manager.o += -DDEBUG
CFLAGS_adsp_ipi_queue.o += -DDEBUG
endif



