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

ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), m)
	ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_T), m)
		obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_T) += isp_4/
	else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_C), m)
		obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_C) += isp_4/
	else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M), m)
		obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M) += isp_3/
	else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z), m)
		obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z) += isp_3/
	else
		obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += isp_6s/
	endif
else ifeq ($(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT), y)
# No isp6s driver is built-in.
	obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_T) += isp_4/
	obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP4_C) += isp_4/
	obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_M) += isp_3/
	obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT_ISP3_Z) += isp_3/
endif
