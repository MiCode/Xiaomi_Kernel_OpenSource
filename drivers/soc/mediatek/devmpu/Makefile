# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
	GCOV_PROFILE := y
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_DEVMPU) += devmpu.o
