#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(srctree)
THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/

obj-y += mtk_thermal_monitor.o
obj-y += mtk_cooler_shutdown.o
ifeq ($(CONFIG_MTK_LEDS), y)
    # setMaxbrightness() defined in mediatek/kernel/drivers/leds/leds_drv.c only built when CONFIG_MTK_LEDS is set
    obj-y += mtk_cooler_backlight.o
endif
obj-y += mtk_cooler_kshutdown.o
obj-y += mtk_cooler_cam.o

obj-y += common/
obj-y += $(subst ",,$(CONFIG_MTK_LEGACY_THERMAL_PLATFORM))/
