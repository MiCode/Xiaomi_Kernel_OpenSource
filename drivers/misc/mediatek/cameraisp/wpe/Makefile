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

obj-y := camerawpe_dummy.o

ifeq ($(CONFIG_MTK_CAMERA_WPE_ISP6S),m)
	obj-y += $(subst ",,isp_6s)/
endif

ifeq ($(CONFIG_MTK_CAMERA_WPE_ISP50),m)
 	obj-y += $(subst ",,isp_50)/
endif

