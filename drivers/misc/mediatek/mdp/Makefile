#
# Copyright (C) 2019 MediaTek Inc.
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

# driver code for MDP

MTK_PLATFORM := $(CONFIG_MTK_PLATFORM)

ifeq ($(CONFIG_MTK_CMDQ_V3),y)
	CMDQ_VER := v3
else
	CMDQ_VER := v2
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/$(CMDQ_VER) \
	     -I$(srctree)/drivers/iommu/ \
	     -I$(srctree)/drivers/staging/android/mtk_ion \
	     -I$(srctree)/drivers/staging/android/mtk_ion/mtk \
	     -I$(srctree)/drivers/misc/mediatek/trusted_mem/public

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/$(CMDQ_VER)/$(MTK_PLATFORM)

obj-y += mdp_ioctl_ex.o
obj-y += mdp_m4u.o
