# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/s5k3m3mipiraw_Sensor.o \
	$(subdrv-rpath)/s5k3m3mipiraw_pdaf.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_C), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_C), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_C) += $(IMGSENSOR_SUBDRV_OBJ)
endif
