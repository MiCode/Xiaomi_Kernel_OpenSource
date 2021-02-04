#
# Copyright (C) 2016 MediaTek Inc.
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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/hps_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/

obj-y += mtk_hps_main.o
obj-y += mtk_hps_core.o
obj-y += mtk_hps_procfs.o
obj-y += mtk_hps_cpu.o
obj-y += mtk_hps_api.o
obj-y += mtk_hps_algo.o

obj-y += mtk_hps_ops.o

# platform dependent files
#obj-y += mach/

# stress module
# obj-y += mtk_cpuhp_ut.o
