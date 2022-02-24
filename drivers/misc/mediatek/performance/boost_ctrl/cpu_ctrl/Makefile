#
# Copyright (C) 2018 MediaTek Inc.
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

ifeq ($(CONFIG_MTK_PPM),y)
obj-y += cpu_ctrl.o
else

# no CONFIG_MTK_PPM but specific lagacy platform
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6761" "mt6765" "mt6768" "mt6873" "mt6885" "mt6893" "mt6771" "mt6785" "mt6853" "mt6739"))
ifeq ($(CONFIG_MTK_GKI_COMPAT),y)
obj-y += cpu_ctrl_dummy.o
else
obj-y += cpu_ctrl.o
endif # CONFIG_MTK_GKI_COMPAT

else
obj-y += cpu_ctrl_dummy.o

endif
endif # CONFIG_MTK_PPM

ifeq ($(CONFIG_MACH_MT6893),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6885),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6739),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6833),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
else ifeq ($(CONFIG_MACH_MT6768),y)
ccflags-y += \
		-DMTK_K14_CPU_BOOST
endif

obj-$(CONFIG_MTK_CPU_CTRL_CFP) += cpu_ctrl_cfp.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)drivers/misc/mediatek/performance/include/
