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

ccflags-y += -DMET_USER_EVENT_SUPPORT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/hps_v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/iccs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/extbuck/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/ppm_v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/ppm_v3/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cpufreq_v2/inc/


obj-$(CONFIG_MTK_HPS_V3) += mtk_cpuhp.o
mtk_cpuhp-y += mtk_hotplug_cb.o
mtk_cpuhp-y += mtk_hps_main.o
mtk_cpuhp-y += mtk_hps_core.o
mtk_cpuhp-y += mtk_hps_procfs.o
mtk_cpuhp-y += mtk_hps_cpu.o
mtk_cpuhp-y += mtk_hps_api.o
mtk_cpuhp-y += mtk_hps_algo.o

mtk_cpuhp-y += mtk_hps_ops.o


# stress module
#obj-y += mtk_cpuhp_ut.o
