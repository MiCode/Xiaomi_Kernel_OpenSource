# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# $(info jd1 for LD2.0 mt6853)
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6853),m)
imgsensor_mt6853-objs += \
	$(subdrv-rpath)/s5kjd1mipiraw_Sensor.o
endif

# $(info jd1 for LD2.0 mt6893)
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
imgsensor_mt6893-objs += \
	$(subdrv-rpath)/s5kjd1mipiraw_Sensor.o
endif

# $(info jd1 for LD1.0)
ifneq ($(CONFIG_MTK_IMGSENSOR_PLATFORM_SUPPORT),m)
imgsensor-objs += \
	$(subdrv-rpath)/s5kjd1mipiraw_Sensor.o
endif