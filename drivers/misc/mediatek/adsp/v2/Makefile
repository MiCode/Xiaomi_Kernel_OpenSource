# SPDX-License-Identifier: GPL-2.0

VERSION := v2

# common include path
ccflags-y += -I$(srctree)/sound/soc/mediatek/common
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include

# target object
obj-${CONFIG_MTK_ADSP_V2} += adsp-$(VERSION).o

adsp-$(VERSION)-objs += adsp_driver_$(VERSION).o
adsp-$(VERSION)-objs += adsp_clk_audio.o
