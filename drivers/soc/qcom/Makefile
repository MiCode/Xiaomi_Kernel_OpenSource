obj-$(CONFIG_MSM_SMEM)	+=	msm_smem.o smem_debug.o
obj-$(CONFIG_MSM_SMD)	+= 	msm_smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_GLINK)	+=	glink.o glink_debugfs.o glink_ssr.o
obj-$(CONFIG_MSM_GLINK_LOOPBACK_SERVER)	+=	glink_loopback_server.o
obj-$(CONFIG_MSM_GLINK_SMD_XPRT)	+=	glink_smd_xprt.o
obj-$(CONFIG_MSM_GLINK_SMEM_NATIVE_XPRT)+=	glink_smem_native_xprt.o
obj-$(CONFIG_ARCH_QCOM) +=	kryo-l2-accessors.o
obj-$(CONFIG_MSM_SMP2P)	+=	smp2p.o smp2p_debug.o smp2p_sleepstate.o
obj-$(CONFIG_MSM_SMP2P_TEST)	+=	smp2p_loopback.o smp2p_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_RPM_SMD)	+=	rpm-smd.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD)	+=	rpm-smd-debug.o
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
