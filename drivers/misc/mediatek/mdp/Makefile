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
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768" "mt6771" "mt6885" "mt8168" "mt6785" "mt6765"))
	CMDQ_VER := v3
else
	CMDQ_VER := v2
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/$(CMDQ_VER) \
	     -I$(srctree)/drivers/iommu/ \
	     -I$(srctree)/drivers/staging/android/ion \
	     -I$(srctree)/drivers/staging/android/ion/mtk

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/$(CMDQ_VER)/$(MTK_PLATFORM)

ifeq ($(or $(strip $(CONFIG_TRUSTONIC_TEE_SUPPORT)), $(strip $(CONFIG_MICROTRUST_TEE_SUPPORT))),y)
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif	# end of CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT or CONFIG_MTK_CAM_SECURITY_SUPPORT
endif	# end of CONFIG_TRUSTONIC_TEE_SUPPORT or CONFIG_MICROTRUST_TEE_SUPPORT

# driver module
obj-y += mdp_ioctl_ex.o
obj-y += mdp_m4u.o
