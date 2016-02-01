# SPDX-License-Identifier: GPL-2.0
KASAN_SANITIZE_scm.o := n
obj-$(CONFIG_QCOM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_QCOM_GLINK_SSR) +=	glink_ssr.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_MDT_LOADER)	+= mdt_loader.o
obj-$(CONFIG_QCOM_LLCC) += llcc-core.o llcc-slice.o
obj-$(CONFIG_QCOM_SDM855_LLCC) += llcc-sdm855.o
obj-$(CONFIG_QCOM_LLCC_AMON) += llcc-amon.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_QMI_HELPERS)	+= qmi_helpers.o
qmi_helpers-y += qmi_encdec.o
qmi_helpers-y += qmi_interface.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
obj-$(CONFIG_QCOM_SMEM_STATE) += smem_state.o
obj-$(CONFIG_QCOM_SMP2P)	+= smp2p.o
obj-$(CONFIG_QCOM_SMSM)	+= smsm.o
obj-$(CONFIG_QCOM_WCNSS_CTRL) += wcnss_ctrl.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)
obj-$(CONFIG_QCOM_SCM)  +=      scm.o
obj-$(CONFIG_QCOM_EARLY_RANDOM)	+= early_random.o
obj-$(CONFIG_SOC_BUS) += socinfo.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_CORE_HANG_DETECT) += core_hang_detect.o
obj-$(CONFIG_QCOM_DCC_V2) += dcc_v2.o
obj-$(CONFIG_MSM_GLADIATOR_HANG_DETECT) += gladiator_hang_detect.o
obj-$(CONFIG_MSM_GLADIATOR_ERP) += gladiator_erp.o
obj-$(CONFIG_QCOM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_QCOM_MEMORY_DUMP_V2) += memory_dump_v2.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
obj-$(CONFIG_ICNSS) += icnss.o
obj-$(CONFIG_MSM_SERVICE_NOTIFIER) += service-notifier.o
obj-$(CONFIG_MSM_SERVICE_LOCATOR) += service-locator.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o sysmon-qmi.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o
obj-$(CONFIG_QCOM_RUN_QUEUE_STATS) += rq_stats.o
obj-$(CONFIG_QCOM_BUS_SCALING) += msm_bus/
obj-$(CONFIG_QCOM_COMMAND_DB) += cmd-db.o
obj-$(CONFIG_QTI_RPMH_API) += rpmh.o
obj-$(CONFIG_QTI_SYSTEM_PM) += system_pm.o
obj-$(MSM_REMOTEQDSS) += remoteqdss.o
obj-$(CONFIG_MSM_JTAGV8) += jtagv8.o jtagv8-etm.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
       obj-y += subsystem_notif.o
       obj-y += subsystem_restart.o
       obj-y += ramdump.o
endif
obj-$(CONFIG_QCOM_EUD) += eud.o
obj-$(CONFIG_QCOM_GLINK) += glink_probe.o
obj-$(CONFIG_QCOM_QDSS_BRIDGE) += qdss_bridge.o
obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o
obj-$(CONFIG_MSM_IDLE_STATS)	+= lpm-stats.o
obj-$(CONFIG_QTI_RPM_STATS_LOG) += rpm_stats.o
