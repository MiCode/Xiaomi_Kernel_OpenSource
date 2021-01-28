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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/cm_mgr/ \
	-I$(srctree)/drivers/misc/mediatek/dvfsrc/ \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/include \

MTK_CM_MGR_MODULE_LOCAL_NAME = mtk_cm_mgr
MTK_CM_MGR_MODULE_SRC = mtk_cm_mgr_common.o \
			mtk_cm_mgr_platform.o
$(MTK_CM_MGR_MODULE_LOCAL_NAME)-objs = $(MTK_CM_MGR_MODULE_SRC:.c=.o)
obj-${CONFIG_MTK_CM_MGR} += $(MTK_CM_MGR_MODULE_LOCAL_NAME).o

