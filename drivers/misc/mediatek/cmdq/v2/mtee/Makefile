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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2 \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)\
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone/ \

# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT

# Common Clock Framework
ifneq ($(CONFIG_MTK_CLKMGR),y)
ccflags-y += -DCMDQ_USE_CCF
endif

ifeq ($(CONFIG_MTK_CMDQ_TAB), y)
ccflags-y += -DCMDQ_USE_LEGACY
endif

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
	ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif
	ccflags-y += -I$(srctree)/include
	ccflags-y += -I$(srctree)/include/trustzone
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/mtee
	ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_dr \
		 -I$(srctree)/drivers/misc/mediatek/cmdq/v2/inc/cmdq_sec_tl \
	             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
	             -I$(srctree)/include/trustzone
#endif

obj-y += cmdq_sec.o
