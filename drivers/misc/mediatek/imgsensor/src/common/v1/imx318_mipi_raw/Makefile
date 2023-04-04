# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/imx318mipi_Sensor.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP3_M), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP3_M), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP3_M) += $(IMGSENSOR_SUBDRV_OBJ)
endif