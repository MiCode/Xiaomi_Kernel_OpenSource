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

# drivers/cmdq
$(warning start to build cmdq normal)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/include/linux
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
$(warning start to build cmdq secure)
	ccflags-y += -I$(srctree)/include
	ccflags-y += -I$(srctree)/include/trustzone

#	ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT

	ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT

	ccflags-y += -DDEBUG_SVP_INFO
	obj-y += cmdq_sec.o
else

endif
ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),yes)
   # ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT
endif
#ccflags-y += -DMTK_SEC_VIDEO_PATH_SUPPORT	#need to modify jiaguang.zhang
#ccflags-y += -DCONFIG_MTK_IN_HOUSE_TEE_SUPPORT
# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT
ccflags-y += -DDEBUG_INFO

obj-y += cmdq_record.o
obj-y += cmdq_core.o
obj-y += cmdq_driver.o
obj-y += cmdq_mmp.o
obj-y += cmdq_mdp_common.o
#obj-y += cmdq_prof.o
obj-y += cmdq_test.o
#obj-y += cmdq_mdp.o
obj-y += cmdq_device.o
obj-y += cmdq_platform.o
obj-y += cmdq_mutex.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
