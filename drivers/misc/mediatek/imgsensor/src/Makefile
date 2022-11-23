# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MTK_PLATFORM := isp4 isp6s
subdir-ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/ \
	-I$(srctree)/drivers/i2c/busses/ \
	-I$(srctree)/drivers/misc/mediatek/ccu/inc \
	-I$(srctree)/drivers/misc/mediatek/imgsensor/inc \
	-I$(srctree)/drivers/misc/mediatek/cam_cal/inc \
$(foreach platform,$(MTK_PLATFORM),$(eval obj-$(CONFIG_MTK_IMGSENSOR) += $(platform)/))
