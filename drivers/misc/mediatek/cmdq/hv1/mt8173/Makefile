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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/hv1/$(MTK_PLATFORM)
# Enable MET
# ccflags-y += -DMET_USER_EVENT_SUPPORT

obj-y += cmdq_mdp.o
obj-y += cmdq_core_idv.o
obj-y += cmdq_platform_idv.o
