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

#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-y += nt35695_fhd_dsi_vdo_truly_rt5081_hdp_20_9.o

ifeq ($(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES), "k97v1_64_om_lwctg_fhdvd_lswap")
ccflags-$(CONFIG_MTK_LCM) += -DCONFIG_NT35695_LANESWAP
endif

ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
