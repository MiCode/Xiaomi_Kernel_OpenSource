CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

ccflags-$(CONFIG_MSM_QBT1000) += -Idrivers/misc/

obj-$(CONFIG_MSM_GLADIATOR_ERP) += gladiator_erp.o
obj-$(CONFIG_ARM64) += idle-v8.o cpu_ops.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_RUN_QUEUE_STATS) += msm_rq_stats.o
obj-$(CONFIG_CPU_V7) += idle-v7.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_MSM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_MSM_MEMORY_DUMP) += memory_dump.o
obj-$(CONFIG_MSM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_MSM_DDR_HEALTH) += ddr-health.o
obj-$(CONFIG_MSM_DCC) += dcc.o
obj-$(CONFIG_MSM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_MSM_COMMON_LOG) += common_log.o
obj-$(CONFIG_MSM_HVC) += hvc.o
obj-$(CONFIG_MSM_CPU_PWR_CTL) += cpu_pwr_ctl.o
obj-$(CONFIG_MSM_CACHE_M4M_ERP64) += cache_m4m_erp64.o
obj-$(CONFIG_SOC_BUS)  +=      socinfo.o
obj-$(CONFIG_MSM_HYP_DEBUG) += hyp-debug.o
obj-$(CONFIG_ARCH_MSM8996) += kryo-l2-accessors.o
obj-$(CONFIG_MSM_RPM_SMD) +=	rpm-smd.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD) +=	rpm-smd-debug.o
endif
obj-$(CONFIG_MSM_SPM) += spm.o spm_devices.o
obj-$(CONFIG_SOC_BUS)	+=	socinfo.o
obj-$(CONFIG_MSM_SCM)  +=      scm.o scm-boot.o
obj-$(CONFIG_MSM_MPM_OF) += mpm-of.o
obj-$(CONFIG_MSM_SMEM)  +=smem.o smem_debug.o
obj-$(CONFIG_MSM_SMD)   += smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_GLINK) += glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER) += glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_SMD_XPRT) += glink_smd_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT) += glink_smem_native_xprt.o
obj-$(CONFIG_MSM_SMEM_LOGGING) += smem_log.o
obj-$(CONFIG_MSM_SMP2P) += smp2p.o smp2p_debug.o smp2p_sleepstate.o
obj-$(CONFIG_MSM_SMP2P_TEST) += smp2p_loopback.o smp2p_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_QMI_INTERFACE) += qmi_interface.o
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT) += ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_HSIC_XPRT) += ipc_router_hsic_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_MHI_XPRT) += ipc_router_mhi_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_GLINK_XPRT) += ipc_router_glink_xprt.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o sysmon-qmi.o
obj-$(CONFIG_MSM_SHARED_HEAP_ACCESS) += shared_memory.o
obj-y			+=	qdsp6v2/
obj-$(CONFIG_MSM_RPM_RBCPR_STATS_V2_LOG) += rpm_rbcpr_stats_v2.o
obj-$(CONFIG_MEM_SHARE_QMI_SERVICE)		+= memshare/
obj-$(CONFIG_CP_ACCESS64) += cpaccess64.o
obj-$(CONFIG_MSM_RPM_STATS_LOG) += rpm_stats.o rpm_master_stat.o rpm_rail_stats.o
obj-$(CONFIG_HW_PERF_EVENTS) += perf_event_kryo.o perf_event_l2.o
obj-$(CONFIG_MSM_RPM_LOG) += rpm_log.o
obj-$(CONFIG_MSM_JTAG) += jtag-fuse.o jtag.o
obj-$(CONFIG_MSM_JTAG_MM) +=  jtag-fuse.o jtag-mm.o
obj-$(CONFIG_MSM_JTAGV8) += jtag-fuse.o jtagv8.o jtagv8-etm.o
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR) += system_health_monitor_v01.o
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR) += system_health_monitor.o
obj-$(CONFIG_MSM_GLINK_PKT) += msm_glink_pkt.o
obj-$(CONFIG_MSM_TZ_SMMU) += msm_tz_smmu.o
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_CORE_CTL_HELPER) += core_ctl_helper.o
obj-$(CONFIG_MSM_SCM_ERRATA) += scm-errata.o
obj-$(CONFIG_MSM_PFE_WA) += pfe-wa.o
obj-$(CONFIG_MSM_PERFORMANCE) += msm_performance.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	obj-y += subsystem_notif.o
	obj-y += subsystem_restart.o
	obj-y += ramdump.o
endif

obj-$(CONFIG_MSM_SERVICE_NOTIFIER) += service-notifier.o
obj-$(CONFIG_MSM_SYSMON_COMM) += sysmon.o sysmon-qmi.o
obj-$(CONFIG_MSM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_TRACER_PKT) += tracer_pkt.o
obj-$(CONFIG_ICNSS) += icnss.o wlan_firmware_service_v01.o
obj-$(CONFIG_MSM_BAM_DMUX) += bam_dmux.o
obj-$(CONFIG_MSM_SERVICE_LOCATOR) += service-locator.o
obj-$(CONFIG_MSM_PACMAN)        += msm_pacman.o
obj-$(CONFIG_MSM_QBT1000) += qbt1000.o
obj-$(CONFIG_MSM_SCM_XPU) += scm-xpu.o
obj-$(CONFIG_MSM_KERNEL_PROTECT) += kernel_protect.o
