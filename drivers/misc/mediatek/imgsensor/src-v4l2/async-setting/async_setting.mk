# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/async-setting

imgsensor-objs += async-setting/async-setting.o
