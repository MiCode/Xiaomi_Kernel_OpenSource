# SPDX-License-Identifier: GPL-2.0

VERSION := v1

# common include path
ccflags-y += -I$(srctree)/sound/soc/mediatek/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

# target object
obj-${CONFIG_MTK_ADSP_V1} += adsp-$(VERSION).o

adsp-$(VERSION)-objs += adsp_driver_$(VERSION).o
adsp-$(VERSION)-objs += adsp_clk_tinysys.o
