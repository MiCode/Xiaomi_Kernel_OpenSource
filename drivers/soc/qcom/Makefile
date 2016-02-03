obj-$(CONFIG_MSM_SMEM)	+=	msm_smem.o smem_debug.o
obj-$(CONFIG_MSM_SMD)	+= 	msm_smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_GLINK)	+=	glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER)	+=	glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_SMD_XPRT)	+=	glink_smd_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT)+=	glink_smem_native_xprt.o
obj-$(CONFIG_MSM_SMEM_LOGGING)	+=	smem_log.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o sysmon-qmi.o
obj-$(CONFIG_ARCH_QCOM) +=	kryo-l2-accessors.o
obj-$(CONFIG_MSM_SMP2P)	+=	smp2p.o smp2p_debug.o smp2p_sleepstate.o
obj-$(CONFIG_MSM_SMP2P_TEST)	+=	smp2p_loopback.o smp2p_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_RPM_SMD)	+=	rpm-smd.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD)	+=	rpm-smd-debug.o
endif
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT)	+=	ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_HSIC_XPRT)	+=	ipc_router_hsic_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_MHI_XPRT)	+=	ipc_router_mhi_xprt.o

obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	obj-y += subsystem_notif.o
	obj-y += subsystem_restart.o
	obj-y += ramdump.o
endif

obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_SMD) +=	smd.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
obj-$(CONFIG_MSM_SPM) += msm-spm.o spm_devices.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-$(CONFIG_QCOM_SCM_ERRATA) += scm-errata.o
obj-$(CONFIG_QCOM_SCM)  +=      scm.o scm-boot.o
obj-$(CONFIG_QCOM_SCM_XPU) += scm-xpu.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_QCOM_MEMORY_DUMP) += memory_dump.o
obj-$(CONFIG_QCOM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_TRACER_PKT)	+=	tracer_pkt.o
obj-$(CONFIG_SOC_BUS)  +=      socinfo.o
obj-$(CONFIG_QCOM_BUS_SCALING) += msm_bus/
obj-$(CONFIG_MSM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_MSM_MPM_OF) += mpm-of.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
