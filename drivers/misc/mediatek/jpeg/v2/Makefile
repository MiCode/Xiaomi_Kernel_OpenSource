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
  ccflags-y += -DQOS_MT6765_SUPPORT
  ccflags-y += -DPLATFORM_MT6765
endif

ifeq ($(CONFIG_MACH_MT6761),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DQOS_MT6761_SUPPORT
  ccflags-y += -DPLATFORM_MT6761
endif

ifeq ($(CONFIG_MACH_MT6779),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DQOS_MT6779_SUPPORT
  ccflags-y += -DPLATFORM_MT6779
endif

ifeq ($(CONFIG_MACH_MT6785),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DQOS_MT6785_SUPPORT
  ccflags-y += -DPLATFORM_MT6785
endif

ifeq ($(CONFIG_MACH_MT6768),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DPLATFORM_MT6767
endif

ifeq ($(CONFIG_MACH_MT6739),y)
  ccflags-y += -DPLATFORM_MT6739
endif

ifeq ($(CONFIG_MACH_MT6771),y)
  ccflags-y += -DPLATFORM_MT6771
endif

ifeq ($(CONFIG_MACH_MT6767),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DPLATFORM_MT6768
endif

ifeq ($(CONFIG_MACH_MT8163),y)
  ccflags-y += -DJPEG_PM_DOMAIN_ENABLE
endif

ifeq ($(CONFIG_MACH_MT8173),y)
  ccflags-y += -DJPEG_DEC_DRIVER
  ccflags-y += -DJPEG_PM_DOMAIN_ENABLE
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk
#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

#jpeg_driver-objs := jpeg_cmdq.o jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o
jpeg_driver-objs := jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o

# EOF
