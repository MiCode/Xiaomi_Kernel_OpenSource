CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-$(CONFIG_ARM64) += cpu_ops.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_MSM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_MSM_MEMORY_DUMP) += memory_dump.o
obj-$(CONFIG_MSM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_MSM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_MSM_CPU_PWR_CTL) += cpu_pwr_ctl.o
obj-$(CONFIG_MSM_CACHE_M4M_ERP64) += cache_m4m_erp64.o
obj-$(CONFIG_SOC_BUS)  +=      socinfo.o
obj-$(CONFIG_ARCH_MSM8996) += kryo-l2-accessors.o
obj-$(CONFIG_MSM_RPM_SMD) +=	rpm-smd.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD) +=	rpm-smd-debug.o
endif
obj-$(CONFIG_SOC_BUS)	+=	socinfo.o
obj-$(CONFIG_MSM_SCM)  +=      scm.o scm-boot.o
obj-$(CONFIG_MSM_MPM_OF) += mpm-of.o
obj-$(CONFIG_MSM_SMEM)  +=smem.o smem_debug.o
obj-$(CONFIG_MSM_SMD)   += smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_GLINK) += glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER) += glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_SMD_XPRT) += glink_smd_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT) += glink_smem_native_xprt.o
