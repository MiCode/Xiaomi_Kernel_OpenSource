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
LOCAL_DIR := $(GET_LOCAL_DIR)

LCM_DEFINES := $(shell echo $(CONFIG_CUSTOM_LK_LCM) | tr a-z A-Z)
DEFINES += $(foreach LCM,$(LCM_DEFINES),$(LCM))
DEFINES += MTK_LCM_PHYSICAL_ROTATION=\"$(MTK_LCM_PHYSICAL_ROTATION)\"

LCM_LISTS := $(subst ",,$(CONFIG_CUSTOM_LK_LCM))
OBJS += $(foreach LCM,$(LCM_LISTS),$(LOCAL_DIR)/$(LCM)/$(addsuffix .o, $(LCM)))
OBJS += $(LOCAL_DIR)/mt65xx_lcm_list.o \
		$(LOCAL_DIR)/lcm_common.o \
		$(LOCAL_DIR)/lcm_gpio.o \
		$(LOCAL_DIR)/lcm_i2c.o \
		$(LOCAL_DIR)/lcm_pmic.o \
		$(LOCAL_DIR)/lcm_util.o

INCLUDES += -I$(LOCAL_DIR)/inc

