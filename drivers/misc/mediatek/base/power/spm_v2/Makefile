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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/cpuidle_v1/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/irq/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cameraisp/src/$(CONFIG_MTK_PLATFORM)/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/met/$(MTK_PLATFORM)/

ifeq ($(CONFIG_ARCH_MT6797), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)/
endif

ifeq ($(CONFIG_MTK_PMIC_CHIP_MT6353), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/
ccflags-y += -I$(srctree)/sound/soc/mediatek/common_define/
endif

#ifeq ($(CONFIG_ARCH_MT6735),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/inc/
#endif

ccflags-$(CONFIG_MTK_MET) += -DMET_USER_EVENT_SUPPORT

ifeq ($(CONFIG_ARCH_MT6755), y)
ccflags-y += -I$(srctree)/sound/soc/mediatek/mt_soc_audio_6755/
ccflags-y += -I$(srctree)/sound/soc/mediatek/common_define/

ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-y += mt_idle.o
obj-y += mt_idle_profile.o
obj-y += mt_idle_mt6755.o
obj-y += mt_sleep.o
obj-y += mt_spm.o
obj-y += mt_spm_dpidle.o
obj-y += mt_spm_dpidle_mt6755.o
obj-y += mt_spm_fs.o
obj-y += mt_spm_internal.o
#obj-y += mt_spm_mcdi.o
obj-y += mt_spm_pmic_wrap.o
obj-y += mt_spm_sleep.o
obj-y += mt_spm_sodi.o
obj-y += mt_spm_sodi3p0.o
obj-y += mt_spm_sodi_cmdq.o
obj-y += mt_spm_vcorefs_mt6755.o
endif

ifeq ($(CONFIG_ARCH_MT6757), y)
#ccflags-y += -I$(srctree)/sound/soc/mediatek/mt_soc_audio_6757/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/mt6757/

obj-y += mt_sleep.o
obj-y += mt_spm.o
obj-y += mt_spm_internal.o
obj-y += mt_spm_pmic_wrap.o
obj-y += mt_spm_sleep.o
obj-y += mt_spm_fs.o
obj-y += mt_idle.o
obj-y += mt_idle_profile.o
obj-y += mt_idle_mt6757.o
obj-y += mt_spm_dpidle.o
obj-y += mt_spm_dpidle_mt6757.o
obj-y += mt_spm_sodi.o
obj-y += mt_spm_sodi3p0.o
obj-y += mt_spm_sodi_cmdq.o
obj-y += mt_spm_vcorefs_mt6757.o
endif

ifeq ($(CONFIG_ARCH_MT6797), y)

obj-y += mt_idle.o
obj-y += mt_idle_profile.o
obj-y += mt_idle_mt6797.o
obj-y += mt_spm.o
obj-y += mt_spm_sleep.o
obj-y += mt_sleep.o
obj-y += mt_spm_internal.o
obj-y += mt_spm_dpidle.o
obj-y += mt_spm_dpidle_mt6797.o
obj-y += mt_spm_fs.o
obj-y += mt_spm_sodi.o
obj-y += mt_spm_sodi3p0.o
obj-y += mt_spm_sodi_cmdq.o
obj-y += mt_spm_pmic_wrap.o
obj-y += mt_spm_vcorefs_mt6797.o

endif
