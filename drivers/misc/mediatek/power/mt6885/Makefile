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
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/flashlight/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
subdir-ccflags-$(CONFIG_USB_MU3D_DRV) += -Werror -I$(srctree)/drivers/misc/mediatek/mu3d/drv
subdir-ccflags-$(CONFIG_USB_MTK_HDRC) += -I$(srctree)/drivers/misc/mediatek/usb20/
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/u3_switch
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/inc
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/
subdir-ccflags-$(CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL) += -I$(srctree)/drivers/power/supply/k6885v1_64_alpha

ifeq ($(CONFIG_AGATE_CHARGER),y)
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery_agate
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/charger_agate
else ifeq ($(CONFIG_ARES_CHARGER),y)
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery_ares
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/charger_ares
else
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/charger
endif

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mtk_battery_intf.o
else
obj-y += misc.o
endif
