# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# $(info camera_hw for LD2.0 mt6893)
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
imgsensor_mt6893-objs += \
	../common/v1_1/camera_hw/imgsensor_cfg_table.o \
	../common/v1_1/camera_hw/regulator/regulator.o \
	../common/v1_1/camera_hw/gpio/gpio.o \
	../common/v1_1/camera_hw/mclk/mclk.o

# $(info camera_hw for LD1.0)
else
imgsensor-objs += \
	../common/v1_1/camera_hw/imgsensor_cfg_table.o \
	../common/v1_1/camera_hw/regulator/regulator.o \
	../common/v1_1/camera_hw/gpio/gpio.o \
	../common/v1_1/camera_hw/mclk/mclk.o
endif