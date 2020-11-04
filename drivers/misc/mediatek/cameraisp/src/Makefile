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

ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt6885")
  obj-y += isp_6s/
endif

ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt6873")
  obj-y += isp_6s/
  subdir-ccflags-y += -DCAMERA_ISP_MT6873
  subdir-ccflags-y += -DDISABLE_CAMSV_TOP0=0
endif

ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt6853")
  obj-y += isp_6s/
  subdir-ccflags-y += -DCAMERA_ISP_MT6853
  subdir-ccflags-y += -DDISABLE_CAMSV_TOP0=1
endif
