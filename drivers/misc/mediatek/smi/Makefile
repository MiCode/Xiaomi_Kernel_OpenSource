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

ifneq ($(CONFIG_MTK_SMI_VARIANT),y)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
obj-y += smi_common.o
obj-y += smi_debug.o
obj-y += smi_info_util.o
obj-y += smi_config_util.o
obj-y += smi_configuration.o
obj-y += smi_internal.o

ifeq ($(CONFIG_ARCH_MT6735),y)
ccflags-y += -I$(srctree)/drivers/clk/mediatek
ccflags-y += -DSMI_D1
obj-y += mmdvfs_mgr.o
else ifeq ($(CONFIG_ARCH_MT6735M),y)
ccflags-y += -DSMI_D2
obj-y += mmdvfs_mgr.o
else ifeq ($(CONFIG_ARCH_MT6753),y)
ccflags-y += -DSMI_D3
obj-y += mmdvfs_mgr.o
else ifeq ($(CONFIG_ARCH_MT6580),y)
ccflags-y += -DSMI_R
else ifeq ($(CONFIG_ARCH_MT6755),y)
ccflags-y += -DSMI_J
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v2.o
else ifeq ($(CONFIG_ARCH_MT6797),y)
ccflags-y += -DSMI_EV
ccflags-y += -I$(srctree)/drivers/clk/mediatek
obj-y += mmdvfs_mgr_v2.o
else 
ccflags-y += -DSMI_BRINGUP
endif

else
obj-y += $(subst ",,variant)/
endif
