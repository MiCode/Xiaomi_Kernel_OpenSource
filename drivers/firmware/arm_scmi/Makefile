# SPDX-License-Identifier: GPL-2.0-only
scmi-bus-y = bus.o
scmi-driver-y = driver.o notify.o
scmi-transport-y = shmem.o
scmi-transport-$(CONFIG_MAILBOX) += mailbox.o
scmi-transport-$(CONFIG_HAVE_ARM_SMCCC_DISCOVERY) += smc.o
scmi-protocols-y = base.o clock.o perf.o power.o reset.o sensors.o system.o voltage.o
scmi-module-objs := $(scmi-bus-y) $(scmi-driver-y) $(scmi-protocols-y) \
		    $(scmi-transport-y)
obj-$(CONFIG_ARM_SCMI_PROTOCOL) += scmi-module.o
obj-$(CONFIG_ARM_SCMI_POWER_DOMAIN) += scmi_pm_domain.o
obj-$(CONFIG_QTI_SCMI_PMU_PROTOCOL) += pmu_vendor.o
obj-$(CONFIG_QTI_SCMI_C1DCVS_PROTOCOL) += c1dcvs_vendor.o
obj-$(CONFIG_QTI_SCMI_PLH_PROTOCOL) += plh_vendor.o
obj-$(CONFIG_QTI_SCMI_MEMLAT_PROTOCOL) += memlat_vendor.o
obj-$(CONFIG_QTI_SCMI_GPLAF_PROTOCOL) += gplaf_vendor.o
obj-$(CONFIG_QTI_SCMI_CPUFREQ_STATS_PROTOCOL) += cpufreq_stats_vendor.o
obj-$(CONFIG_QTI_SCMI_SHARED_RAIL_PROTOCOL) += shared_rail_vendor.o
