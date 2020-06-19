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

# common driver code for CMDQ
ifeq ($(CONFIG_MTK_CMDQ_V3),y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768" "mt6771" "mt8168" "mt6785"))
	obj-y += v3/
endif
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167"))
	obj-y += v2/
endif
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mailbox/
# mdp_sw_sync
ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885" "mt6873" "mt6853" "mt6893"))
	obj-y += mdp_sync/
endif
# EOF
