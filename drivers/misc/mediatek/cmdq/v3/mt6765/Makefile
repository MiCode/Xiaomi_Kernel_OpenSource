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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3 \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v3/inc/cmdq_sec_dr/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/iommu
ifeq ($(strip $(CONFIG_MTK_PQ_COLOR_MODE)),"MDP")
ccflags-y += -DCMDQ_MDP_COLOR
endif
ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)
ccflags-y += -DCMDQ_SECURE_PATH_SUPPORT
endif
# Enable MET
ccflags-y += -DMET_USER_EVENT_SUPPORT
# Enable AEE
ccflags-y += -DCMDQ_AEE_READY
obj-y += cmdq_mdp.o
