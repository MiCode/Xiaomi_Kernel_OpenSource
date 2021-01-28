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
	obj-y += v3/
else ifeq ($(CONFIG_MTK_CMDQ_TAB), y)
	obj-y += v2/
endif
# mdp_sw_sync
obj-y += mdp_sync/
# EOF
