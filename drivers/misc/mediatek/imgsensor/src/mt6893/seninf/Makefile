# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# $(info seninf for LD2.0 mt6893)
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
imgsensor_mt6893-objs += \
	$(subdrv-rpath)/seninf_impl.o

# $(info seninf for LD1.0)
else
imgsensor-objs += \
	$(subdrv-rpath)/seninf_impl.o
endif