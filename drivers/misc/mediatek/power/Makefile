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
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/misc

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/mediatek/charger
ccflags-y += -I$(srctree)/drivers/misc/mediatek/flashlight/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
subdir-ccflags-$(CONFIG_USB_MU3D_DRV) += -Werror -I$(srctree)/drivers/misc/mediatek/mu3d/drv
subdir-ccflags-$(CONFIG_USB_MTK_HDRC) += -I$(srctree)/drivers/misc/mediatek/usb20/
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/u3_switch
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/inc
ccflags-y += -I$(srctree)/drivers/power/mediatek/
ccflags-y += -I$(srctree)/drivers/power/mediatek/battery
ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/u3_switch
ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/inc

ifeq ($(CONFIG_MACH_MT6761),y)
obj-y += mt6765/
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += mt6781/
else ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6885/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24196.o
else
ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24296.o
endif
endif
