#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# drivers/image/jpeg/Makefile

ccflags-y = -DSDEBUG_T

ifneq ($(CONFIG_ARCH_MT6735M),y)
  ccflags-y += -DJPEG_DEC_DRIVER
endif

#ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/jpeg/

ccflags-y += \
  -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
  -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/

ifeq ($(CONFIG_ARCH_MT8173),y)
#ccflags-y += -DJPEG_DEV
ccflags-y += -DJPEG_PM_DOMAIN_ENABLE
endif

ifeq ($(CONFIG_ARCH_MT6753),y)
  ccflags-y += -DJPEG_DEC_IRQ_ENHANCE
endif

#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

jpeg_driver-objs := jpeg_cmdq.o jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o

# EOF
