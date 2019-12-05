# SPDX-License-Identifier: GPL-2.0
KASAN_SANITIZE_scm.o := n
obj-$(CONFIG_QCOM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_QCOM_GLINK_SSR) +=	glink_ssr.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_MDT_LOADER)	+= mdt_loader.o
obj-$(CONFIG_QCOM_LLCC) += llcc-core.o llcc-slice.o
obj-$(CONFIG_QCOM_SM8150_LLCC) += llcc-sm8150.o
obj-$(CONFIG_QCOM_SDMSHRIKE_LLCC) += llcc-sdmshrike.o
obj-$(CONFIG_QCOM_SDXPRAIRIE_LLCC) += llcc-sdxprairie.o
obj-$(CONFIG_QCOM_SM6150_LLCC) += llcc-sm6150.o
obj-$(CONFIG_QCOM_ATOLL_LLCC) += llcc-atoll.o
obj-$(CONFIG_QCOM_LLCC_PERFMON) += llcc_perfmon.o
obj-$(CONFIG_QCOM_SDMMAGPIE_LLCC) += llcc-sdmmagpie.o
obj-$(CONFIG_QCOM_LLCC_AMON) += llcc-amon.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_QMI_HELPERS)	+= qmi_helpers.o
qmi_helpers-y += qmi_encdec.o
qmi_helpers-y += qmi_interface.o
obj-$(CONFIG_QCOM_QMI_RMNET)	+= qmi_rmnet.o
obj-$(CONFIG_QCOM_QMI_DFC)	+= dfc_qmi.o dfc_qmap.o
obj-$(CONFIG_QCOM_QMI_POWER_COLLAPSE) += wda_qmi.o
obj-$(CONFIG_MSM_APM)          += apm.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
obj-$(CONFIG_MSM_SPM) += msm-spm.o spm_devices.o
obj-$(CONFIG_QCOM_SMEM_STATE) += smem_state.o
obj-$(CONFIG_QCOM_SMP2P)	+= smp2p.o
obj-$(CONFIG_QCOM_SMSM)	+= smsm.o
obj-$(CONFIG_QCOM_WCNSS_CTRL) += wcnss_ctrl.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)
obj-$(CONFIG_QCOM_SCM)  +=      scm.o
obj-$(CONFIG_QCOM_SCM_QCPE)  += scm_qcpe.o
obj-$(CONFIG_QCOM_EARLY_RANDOM)	+= early_random.o
obj-$(CONFIG_SOC_BUS) += socinfo.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_BOOT_TIME_MARKER) += boot_marker.o
obj-$(CONFIG_MSM_CORE_HANG_DETECT) += core_hang_detect.o
obj-$(CONFIG_QCOM_DCC_V2) += dcc_v2.o
obj-$(CONFIG_MSM_GLADIATOR_HANG_DETECT) += gladiator_hang_detect.o
obj-$(CONFIG_MSM_GLADIATOR_ERP) += gladiator_erp.o
obj-$(CONFIG_QCOM_MINIDUMP) += msm_minidump.o minidump_log.o
obj-$(CONFIG_QCOM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_QCOM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_QPNP_PBS) += qpnp-pbs.o
obj-$(CONFIG_ICNSS) += icnss.o
obj-$(CONFIG_ICNSS_QMI) += icnss_qmi.o wlan_firmware_service_v01.o
obj-$(CONFIG_MSM_SPSS_UTILS) += spss_utils.o
obj-$(CONFIG_MSM_TZ_SMMU) += msm_tz_smmu.o
obj-$(CONFIG_MSM_SERVICE_NOTIFIER) += service-notifier.o
obj-$(CONFIG_MSM_SERVICE_LOCATOR) += service-locator.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o
obj-$(CONFIG_MSM_SYSMON_QMI_COMM) += sysmon-qmi.o
obj-$(CONFIG_MEM_SHARE_QMI_SERVICE)		+= memshare/
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_QCOM_RUN_QUEUE_STATS) += rq_stats.o
obj-$(CONFIG_MSM_SPCOM) += spcom.o
obj-$(CONFIG_QCOM_BUS_SCALING) += msm_bus/
obj-$(CONFIG_QCOM_COMMAND_DB) += cmd-db.o
obj-$(CONFIG_QTI_RPMH_API) += rpmh.o
obj-$(CONFIG_QTI_SYSTEM_PM) += system_pm.o
obj-$(MSM_REMOTEQDSS) += remoteqdss.o
obj-$(CONFIG_MSM_JTAGV8) += jtagv8.o jtagv8-etm.o
obj-$(CONFIG_MSM_CDSP_LOADER) += qdsp6v2/
obj-$(CONFIG_QCOM_SMCINVOKE) += smcinvoke.o
obj-$(CONFIG_SDX_EXT_IPC) += sdx_ext_ipc.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
       obj-y += subsystem_notif.o
       obj-y += subsystem_restart.o
       obj-y += ramdump.o
       obj-y += microdump_collector.o
       obj-$(CONFIG_QTI_GVM_QUIN) += subsystem_notif_virt.o
endif
obj-$(CONFIG_QCOM_EUD) += eud.o
obj-$(CONFIG_QSEE_IPC_IRQ) += qsee_ipc_irq.o
obj-$(CONFIG_QSEE_IPC_IRQ_BRIDGE) += qsee_ipc_irq_bridge.o
obj-$(CONFIG_QCOM_GLINK) += glink_probe.o
obj-$(CONFIG_QCOM_GLINK_PKT) += glink_pkt.o
obj-$(CONFIG_QCOM_QDSS_BRIDGE) += qdss_bridge.o
obj-$(CONFIG_MSM_QBT1000) += qbt1000.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
obj-$(CONFIG_MSM_IDLE_STATS)	+= lpm-stats.o
obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpm_stats.o
obj-$(CONFIG_QCOM_FSA4480_I2C)	+= fsa4480-i2c.o
ifdef CONFIG_QTI_RPMH_API
	obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpmh_master_stat.o
endif
ifdef CONFIG_MSM_RPM_SMD
	obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpm_master_stat.o
endif
obj-$(CONFIG_QMP_DEBUGFS_CLIENT) += qmp-debugfs-client.o
obj-$(CONFIG_MSM_PERFORMANCE) += msm_performance.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_RPM_SMD)   +=  rpm-smd-debug.o
endif
obj-$(CONFIG_QCOM_SMP2P_SLEEPSTATE) += smp2p_sleepstate.o
obj-$(CONFIG_QCOM_MEM_OFFLINE) += mem-offline.o
obj-$(CONFIG_QCOM_CDSP_RM) += cdsprm.o
obj-$(CONFIG_QCOM_CX_IPEAK) += cx_ipeak.o
obj-$(CONFIG_QCOM_AOP_DDR_MESSAGING) += aop_ddr_msgs.o
obj-$(CONFIG_MSM_HAB) += hab/
obj-$(CONFIG_QCOM_HYP_CORE_CTL) += hyp_core_ctl.o
obj-$(CONFIG_QCOM_AOP_DDRSS_COMMANDS) += aop_ddrss_cmds.o
obj-$(CONFIG_RMNET_CTL) += rmnet_ctl/
obj-$(CONFIG_QCOM_ADSP_MANUAL_VOTE) += adsp_vote_qmi.o adsp_lpm_voting_v01.o
