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
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6768" "mt6771" "mt6885" "mt8168" "mt6785" "mt6765"))
	obj-y += v3/
else ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8167"))
	obj-y += v2/
else
	obj-y += mailbox/
endif
# mdp_sw_sync
obj-y += mdp_sync/
# EOF
