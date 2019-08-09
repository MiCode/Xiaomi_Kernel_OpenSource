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

ifeq ($(CONFIG_FTRACE),y)
ifeq ($(CONFIG_TRACING),y)

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT),y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6779"))
ccflags-$(CONFIG_MACH_MT6779) += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
ccflags-y += -DSMI_MET_IPI
endif
endif
obj-y += met_smi.o
obj-$(CONFIG_MACH_MT6758) += met_smi_config_mt6758.o
obj-$(CONFIG_MACH_MT6765) += met_smi_config_mt6765.o
obj-$(CONFIG_MACH_MT6761) += met_smi_config_mt6761.o
obj-$(CONFIG_MACH_MT3967) += met_smi_config_mt3967.o
obj-$(CONFIG_MACH_MT6779) += met_smi_config_mt6779.o
endif
endif
