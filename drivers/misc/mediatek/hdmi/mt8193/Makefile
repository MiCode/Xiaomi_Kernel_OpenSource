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

ifeq ($(CONFIG_ARCH_MT6735M), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ext_disp/mt6735m\
			 -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM)/mt6735m
endif

ifneq ($(CONFIG_ARCH_MT6735M), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ext_disp/$(CONFIG_MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/inc\
					-I$(srctree)/drivers/misc/mediatek/ext_disp\
					-I$(srctree)/drivers/misc/mediatek/video/include\
					-I$(srctree)/drivers/misc/mediatek/include/mt-plat\
					-I$(srctree)/drivers/misc/mediatek/multibridge/$(CONFIG_MTK_PLATFORM)/mt8193/inc\
					-I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM)/dispsys\
					-I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM)\
					-I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI), "MT8193")
ccflags-y += 	-DHDMI_MT8193_SUPPORT
endif

obj-y	+= hdmi_drv.o mt8193_ctrl.o mt8193avd.o mt8193cec.o mt8193ddc.o mt8193edid.o mt8193hdcp.o mt8193hdmicmd.o mt8193hdmictrl.o
