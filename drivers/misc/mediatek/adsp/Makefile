# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Wformat

ifeq ($(CONFIG_MACH_MT6893), y)
MTK_PLATFORM := mt6885
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

obj-y += $(MTK_PLATFORM)/

ifeq ($(CONFIG_MACH_MT6785), y)
MTK_ADSP_HW_VER := 0
else
MTK_ADSP_HW_VER := 1
endif
subdir-ccflags-y += -DMTK_ADSP_HW_VER=$(MTK_ADSP_HW_VER)

