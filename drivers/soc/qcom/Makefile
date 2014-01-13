# When adding new entries keep the list in alphabetical order
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-y		+= qdsp6v2/

obj-$(CONFIG_MSM_BAM_DMUX) += bam_dmux.o
obj-$(CONFIG_MSM_IPC_ROUTER_HSIC_XPRT) += ipc_router_hsic_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT) += ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_MEMORY_DUMP) += memory_dump.o
obj-$(CONFIG_MSM_JTAG) += jtag-fuse.o jtag.o
obj-$(CONFIG_MSM_JTAG_MM) +=  jtag-fuse.o jtag-mm.o
obj-$(CONFIG_MSM_QMI_INTERFACE) += qmi_interface.o

obj-$(CONFIG_MSM_SCM) += scm.o

obj-$(CONFIG_MSM_SMD) += smd.o smd_debug.o smd_private.o smd_init_dt.o smsm_debug.o
obj-$(CONFIG_MSM_SMEM) += smem.o smem_debug.o
obj-$(CONFIG_MSM_SMEM_LOGGING) += smem_log.o
obj-$(CONFIG_MSM_SMP2P) += smp2p.o smp2p_debug.o
obj-$(CONFIG_MSM_SMP2P_TEST) += smp2p_loopback.o smp2p_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_MEM_SHARE_QMI_SERVICE)		+= memshare/
obj-y		+= socinfo.o

obj-$(CONFIG_MSM_PIL) += peripheral-loader.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL) += scm-pas.o
obj-$(CONFIG_MSM_PIL_LPASS_QDSP6V5) += pil-q6v5.o pil-q6v5-lpass.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_PIL_FEMTO) += pil-q6v5.o pil-msa.o pil-femto-modem.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
       obj-y += subsystem_notif.o
       obj-y += subsystem_restart.o
       obj-y += ramdump.o
endif
obj-$(CONFIG_MSM_SYSMON_COMM) += sysmon.o
