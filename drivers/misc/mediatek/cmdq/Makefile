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

ifeq ($(CONFIG_MACH_MT6781),y)
CMDQ_PLATFORM := "mt6781"
else
CMDQ_PLATFORM := $(CONFIG_MTK_PLATFORM)
endif

# common driver code for CMDQ
ifeq ($(CONFIG_MTK_CMDQ_V3),y)
ifneq (,$(filter $(CMDQ_PLATFORM), "mt6739" "mt6768" "mt6771" "mt8168" "mt6785"))
	obj-y += v3/
endif
else ifneq (,$(filter $(CMDQ_PLATFORM), "mt8167"))
	obj-y += v2/
endif
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mailbox/
obj-$(CONFIG_MTK_MT6382_BDG) += bridge/
# mdp_sw_sync
ifeq (,$(filter $(CMDQ_PLATFORM), "mt6885" "mt6873" "mt6853" "mt6893" "mt6833" "mt6877" "mt6781"))
	obj-y += mdp_sync/
endif
# EOF
