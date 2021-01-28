# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

UBSAN_SANITIZE := n

ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/debug_latch/plat_dbg_info
ccflags-y += -I$(srctree)/include/memory/mediatek

ifeq ($(CONFIG_MTK_DEVMPU),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/devmpu
ccflags-y += -I$(srctree)/drivers/misc/mediatek/devmpu/$(MTK_PLATFORM)
endif

obj-y += bwl_platform.o
obj-y += mpu_platform.o
obj-y += plat_debug_api.o

obj-y += ../emi_ctrl_v1.o
obj-y += ../submodule/bwl_v1.o
obj-y += ../submodule/mpu_v1.o
obj-y += ../submodule/elm_v2.o
obj-y += ../submodule/pasr_api_v1.o

