# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/s5k2t7spmipiraw_Sensor.o \

ifeq ($(CONFIG_MTK_IMGSENSOR), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR), y)
obj-$(CONFIG_MTK_IMGSENSOR) += $(IMGSENSOR_SUBDRV_OBJ)
endif
