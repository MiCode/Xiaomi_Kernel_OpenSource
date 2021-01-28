# Copyright (c) 2015-2020, MICROTRUST Incorporated
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

#include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(src) -I$(src)/include
ccflags-y += -I$(srctree)

ifeq ($(CONFIG_MTK_TEE_SANITY),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/tee_sanity/
endif

teei-objs += teei_client_main.o
teei-objs += fdrv.o
teei-objs += backward_driver.o irq_register.o notify_queue.o teei_fp.o teei_smc_call.o tz_log.o switch_queue.o teei_keymaster.o teei_cancel_cmd.o teei_client_transfer_data.o
teei-objs += sysfs.o teei_task_link.o

obj-$(CONFIG_MICROTRUST_TZ_DRIVER) += teei.o

ccflags-$(CONFIG_MTK_ENG_BUILD) += -DENABLED_TEEI_BOOT_LOG

ccflags-y += -D DEBUG_DO
ccflags-y += -Wno-error
ccflags-y += -D UT_DMA_ZONE
