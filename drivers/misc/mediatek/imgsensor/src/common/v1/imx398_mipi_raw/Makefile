# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/imx398mipiraw_Sensor.o \
	$(subdrv-rpath)/imx398_eeprom.o \

ifeq ($(CONFIG_MTK_IMGSENSOR), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR), y)
obj-$(CONFIG_MTK_IMGSENSOR) += $(IMGSENSOR_SUBDRV_OBJ)
endif
