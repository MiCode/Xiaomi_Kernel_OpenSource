# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2017 MediaTek Inc.
#

ifneq ($(CONFIG_FPGA_EARLY_PORTING), y)
obj-$(CONFIG_MACH_MT6765) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6761) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

endif
obj-$(CONFIG_MACH_MT6779) += drcc_v1/
obj-$(CONFIG_MACH_MT6765) += spm/
obj-$(CONFIG_MACH_MT6761) += spm/
obj-$(CONFIG_MACH_MT6779) += spm/
obj-$(CONFIG_MTK_STATIC_POWER) += leakage_table_v2/
obj-$(CONFIG_MTK_CPU_FREQ) += cpufreq_v1/
ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-$(CONFIG_MTK_PTPOD) += eem_v2/
endif
obj-$(CONFIG_MTK_UNIFY_POWER) += upower_v2/
obj-$(CONFIG_MACH_MT6761) += ppm_v3/
#obj-$(CONFIG_MACH_MT6765) += ppm_v3/
obj-$(CONFIG_MACH_MT6779) += ppm_v3/

obj-$(CONFIG_MACH_MT6765) += dcm_v1/
obj-$(CONFIG_MACH_MT6779) += dcm_v1/
ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-$(CONFIG_MACH_MT6765) += clkbuf_v1/
#obj-$(CONFIG_MACH_MT6761) += clkbuf_v1/
#obj-$(CONFIG_MACH_MT6779) += clkbuf_v1/
obj-$(CONFIG_MACH_MT6765) += pbm_v4/
obj-$(CONFIG_MACH_MT6761) += pbm_v4/
obj-$(CONFIG_MACH_MT6779) += pbm_v4/
obj-$(CONFIG_MACH_MT6765) += mdpm_v1/
obj-$(CONFIG_MACH_MT6761) += mdpm_v1/
obj-$(CONFIG_MACH_MT6779) += mdpm_v2/
endif
obj-$(CONFIG_MACH_MT6765) += hps_v3/
obj-$(CONFIG_MACH_MT6761) += hps_v3/
obj-$(CONFIG_MACH_MT6765) += cpuidle_v3/
obj-$(CONFIG_MACH_MT6761) += cpuidle_v3/
obj-$(CONFIG_MACH_MT6779) += cpuidle_v3/
obj-$(CONFIG_MACH_MT6779) += cpuhotplug/
obj-$(CONFIG_MACH_MT6765) += power_gs_v1/
obj-$(CONFIG_MACH_MT6761) += power_gs_v1/
obj-$(CONFIG_MACH_MT6779) += power_gs_v1/
obj-$(CONFIG_MACH_MT6765) += udi_v1/
obj-$(CONFIG_MACH_MT6761) += udi_v1/
obj-$(CONFIG_MACH_MT6779) += udi_v2/
obj-$(CONFIG_MACH_MT6765) += mcdi/
obj-$(CONFIG_MACH_MT6761) += mcdi/
obj-$(CONFIG_MACH_MT6779) += mcdi/
ifeq ($(CONFIG_MTK_CM_MGR_LEGACY), y)
obj-$(CONFIG_MACH_MT6765) += cm_mgr_v1/
obj-$(CONFIG_MACH_MT6761) += cm_mgr_v1/
endif
# obj-$(CONFIG_MTK_QOS_FRAMEWORK) += qos/
