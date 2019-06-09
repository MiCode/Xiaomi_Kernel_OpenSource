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
obj-$(CONFIG_MTK_STATIC_POWER) += leakage_table_v2/
obj-$(CONFIG_MTK_CPU_FREQ) += cpufreq_v1/
obj-$(CONFIG_MTK_PTPOD) += eem_v2/
obj-$(CONFIG_MACH_MT6739) += eem_v2/
obj-$(CONFIG_MACH_MT6771) += eem_v2/
ifeq ($(CONFIG_MTK_UNIFY_POWER), y)
obj-$(CONFIG_MACH_MT6771) += upower_v2/
obj-$(CONFIG_MACH_MT6739) += upower_v2/
endif
ifneq ($(CONFIG_FPGA_EARLY_PORTING), y)
obj-$(CONFIG_MACH_MT6771) += cpuidle_v3/
obj-$(CONFIG_MACH_MT6739) += cpuidle_v3/
obj-$(CONFIG_MACH_MT6771) += pbm_v3/
obj-$(CONFIG_MACH_MT6739) += pbm_v3/
obj-$(CONFIG_MACH_MT6771) += spm_v4/
obj-$(CONFIG_MACH_MT6739) += spm_v4/
obj-$(CONFIG_MACH_MT6771) += vcorefs_v3/
obj-$(CONFIG_MACH_MT6739) += vcorefs_v3/
obj-$(CONFIG_MACH_MT6739) += cpufreq_v2/
obj-$(CONFIG_MACH_MT6771) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6739) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6771) += clkbuf_v1/
obj-$(CONFIG_MACH_MT6739) += clkbuf_v1/
obj-$(CONFIG_MACH_MT6739) += power_gs_v1/
endif
ifeq ($(CONFIG_MTK_CM_MGR), y)
obj-$(CONFIG_MTK_CM_MGR) += cm_mgr_v1/
endif
obj-$(CONFIG_MTK_SWPM) += swpm_v1/

obj-$(CONFIG_MACH_MT6771) += mcdi/
obj-$(CONFIG_MACH_MT6739) += mcdi/
obj-$(CONFIG_MACH_MT6771) += cpuhotplug/
obj-$(CONFIG_MACH_MT6739) += cpuhotplug/
obj-$(CONFIG_MACH_MT6771) += ppm_v3/
obj-$(CONFIG_MACH_MT6739) += ppm_v3/
obj-$(CONFIG_MACH_MT6739) += dcm_v1/
