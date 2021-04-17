# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# $(info camera_hw for LD2.0 mt6853)
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6853),m)
imgsensor_mt6853-objs += \
	$(subdrv-rpath)/imgsensor_cfg_table.o \
	$(subdrv-rpath)/regulator/regulator.o \
	$(subdrv-rpath)/gpio/gpio.o \
	$(subdrv-rpath)/mclk/mclk.o

# $(info camera_hw for LD1.0)
else
imgsensor-objs += \
	$(subdrv-rpath)/imgsensor_cfg_table.o \
	$(subdrv-rpath)/regulator/regulator.o \
	$(subdrv-rpath)/gpio/gpio.o \
	$(subdrv-rpath)/mclk/mclk.o
endif