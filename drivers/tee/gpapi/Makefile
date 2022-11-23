# SPDX-License-Identifier: GPL-2.0
LOCAL_PATH := $(srctree)/drivers/tee/gpapi

obj-$(CONFIG_MTK_TEE_GP_COORDINATOR) += gpapi.o
gpapi-y := \
	main.o \

ccflags-y += -I$(LOCAL_PATH)/public/client
ccflags-y += -I$(LOCAL_PATH)/public/impl

obj-$(CONFIG_MTK_TEE_GP_TESTS) += tests/
