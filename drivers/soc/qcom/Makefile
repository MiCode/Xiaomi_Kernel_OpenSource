# When adding new entries keep the list in alphabetical order
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-y		+= qdsp6v2/

obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_RUN_QUEUE_STATS) += msm_rq_stats.o
obj-$(CONFIG_DEBUG_FS) += nohlt.o
obj-$(CONFIG_ARM64) += idle-v8.o cpu_ops.o
obj-$(CONFIG_CPU_V7) += idle-v7.o
obj-$(CONFIG_MAXIMUM_CURRENT_THROTTLING) += mct.o
obj-$(CONFIG_MSM_BAM_DMUX) += bam_dmux.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
obj-$(CONFIG_MSM_GLINK) += glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER) += glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_SMD_XPRT) += glink_smd_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT) += glink_smem_native_xprt.o
obj-$(CONFIG_MSM_GLINK_PKT) += msm_glink_pkt.o
obj-$(CONFIG_MSM_IPC_ROUTER_GLINK_XPRT) += ipc_router_glink_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_HSIC_XPRT) += ipc_router_hsic_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_MHI_XPRT) += ipc_router_mhi_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT) += ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_MEMORY_DUMP) += memory_dump.o
obj-$(CONFIG_MSM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_MSM_DDR_HEALTH) += ddr-health.o
obj-$(CONFIG_MSM_MPM_OF) += mpm-of.o
obj-$(CONFIG_MSM_RPM_SMD) += rpm-smd.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD) += rpm-smd-debug.o
endif
obj-$(CONFIG_MSM_RPM_RBCPR_STATS_V2_LOG) += rpm_rbcpr_stats_v2.o
obj-$(CONFIG_MSM_RPM_STATS_LOG) += rpm_stats.o rpm_master_stat.o
obj-$(CONFIG_MSM_RPM_LOG) += rpm_log.o
obj-$(CONFIG_QCOM_NPA_DUMP) += npa-dump.o
obj-$(CONFIG_MSM_JTAG) += jtag-fuse.o jtag.o
obj-$(CONFIG_MSM_JTAG_MM) +=  jtag-fuse.o jtag-mm.o
obj-$(CONFIG_MSM_JTAGV8) += jtag-fuse.o jtagv8.o jtagv8-mm.o
obj-$(CONFIG_MSM_QMI_INTERFACE) += qmi_interface.o

obj-$(CONFIG_MSM_CACHE_DUMP) += cache_dump.o
obj-$(CONFIG_MSM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_MSM_SCM) += scm.o scm-boot.o scm-mpu.o

obj-$(CONFIG_MSM_SMD) += smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_SMEM) += smem.o smem_debug.o
obj-$(CONFIG_MSM_SMEM_LOGGING) += smem_log.o
obj-$(CONFIG_MSM_COMMON_LOG) += common_log.o
obj-$(CONFIG_MSM_SMP2P) += smp2p.o smp2p_debug.o smp2p_sleepstate.o
obj-$(CONFIG_MSM_SMP2P_TEST) += smp2p_loopback.o smp2p_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_DCC) += dcc.o
obj-$(CONFIG_MSM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_MEM_SHARE_QMI_SERVICE)		+= memshare/
obj-$(CONFIG_MSM_SPM) += spm.o spm_devices.o
obj-y		+= socinfo.o

obj-$(CONFIG_MSM_HVC) += hvc.o
obj-$(CONFIG_MSM_HYP_DEBUG) += hyp-debug.o

obj-$(CONFIG_MSM_PIL) += peripheral-loader.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_PIL_FEMTO) += pil-q6v5.o pil-msa.o pil-femto-modem.o

obj-$(CONFIG_MSM_OCMEM) += ocmem.o ocmem_allocator.o ocmem_notifier.o
obj-$(CONFIG_MSM_OCMEM) += ocmem_sched.o ocmem_api.o ocmem_rdm.o ocmem_core.o

obj-$(CONFIG_MSM_PERFORMANCE) += msm_performance.o
obj-$(CONFIG_MSM_CORE_CTL_HELPER) += core_ctl_helper.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
       obj-y += subsystem_notif.o
       obj-y += subsystem_restart.o
       obj-y += ramdump.o
endif
obj-$(CONFIG_MSM_SYSMON_COMM) += sysmon.o sysmon-qmi.o
obj-$(CONFIG_CP_ACCESS64) += cpaccess64.o
obj-$(CONFIG_MSM_CPU_PWR_CTL) += cpu_pwr_ctl.o
obj-$(CONFIG_MSM8994_V1_PMUIRQ_WA) += cti-pmu-irq.o
obj-$(CONFIG_MSM_XPU_ERR_FATAL) += scm-xpu.o

ifdef CONFIG_MSM_RTB
	obj-y += msm_rtb-hotplug.o
endif

obj-$(CONFIG_MSM_SHARED_HEAP_ACCESS) += shared_memory.o
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR) += system_health_monitor_v01.o
obj-$(CONFIG_MSM_SYSTEM_HEALTH_MONITOR) += system_health_monitor.o
obj-$(CONFIG_MSM_RTB) += msm_rtb-hotplug.o
obj-$(CONFIG_QCOM_EARLY_RANDOM)	+= early_random.o
obj-$(CONFIG_MSM_PACMAN)        += msm_pacman.o
obj-$(CONFIG_MSM_TEST_PACMAN)        += msm_test_pacman.o
