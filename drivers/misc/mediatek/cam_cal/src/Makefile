#
# Copyright (C) 2018 MediaTek Inc.
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

MTK_PLATFORM_EXT := isp4

ifeq ($(CONFIG_MTK_CAM_CAL), m)
obj-$(CONFIG_MTK_CAM_CAL) += custom/
$(foreach platform,$(MTK_PLATFORM_EXT),$(eval obj-$(CONFIG_MTK_CAM_CAL) += $(platform)/))
else ifeq ($(CONFIG_MTK_CAM_CAL_ISP4_C), m)
# No isp6s driver is built-in.
obj-$(CONFIG_MTK_CAM_CAL_ISP4_C) += isp4_c/
else ifeq ($(CONFIG_MTK_CAM_CAL), y)
# No isp6s driver is built-in.
obj-$(CONFIG_MTK_CAM_CAL) += isp4/
obj-$(CONFIG_MTK_CAM_CAL_ISP4_C) += isp4_c/
endif
