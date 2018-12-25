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

ifeq ($(CONFIG_MACH_MT6757),y)
  ccflags-y += -DJPEG_DEC_DRIVER
endif

ifeq ($(CONFIG_MACH_MT6799),y)
  ccflags-y += -DJPEG_DEC_DRIVER
endif

ifeq ($(CONFIG_MACH_MT6765),y)
  ccflags-y += -DSMI_CG_SUPPORT
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

#jpeg_driver-objs := jpeg_cmdq.o jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o
jpeg_driver-objs := jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o

# EOF
