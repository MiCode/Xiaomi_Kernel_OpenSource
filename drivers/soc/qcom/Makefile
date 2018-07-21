# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_QCOM_GENI_SE) +=	qcom-geni-se.o
obj-$(CONFIG_QCOM_COMMAND_DB) += cmd-db.o
obj-$(CONFIG_QCOM_GLINK_SSR) +=	glink_ssr.o
obj-$(CONFIG_QCOM_CPUSS_DUMP) += cpuss_dump.o
obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_MDT_LOADER)	+= mdt_loader.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_QMI_HELPERS)	+= qmi_helpers.o
qmi_helpers-y	+= qmi_encdec.o qmi_interface.o
obj-$(CONFIG_QCOM_RMTFS_MEM)	+= rmtfs_mem.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
obj-$(CONFIG_QCOM_SMEM_STATE) += smem_state.o
obj-$(CONFIG_QCOM_SMP2P)	+= smp2p.o
obj-$(CONFIG_QCOM_SMSM)	+= smsm.o
obj-$(CONFIG_QCOM_WCNSS_CTRL) += wcnss_ctrl.o
obj-$(CONFIG_QCOM_APR) += apr.o
obj-$(CONFIG_QCOM_SECURE_BUFFER) += secure_buffer.o
obj-$(CONFIG_MSM_TZ_SMMU) += msm_tz_smmu.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)
obj-$(CONFIG_QCOM_SCM)  +=      scm.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o
obj-$(CONFIG_MSM_CORE_HANG_DETECT) += core_hang_detect.o
obj-$(CONFIG_MSM_SERVICE_NOTIFIER) += service-notifier.o
obj-$(CONFIG_MSM_SERVICE_LOCATOR) += service-locator.o
obj-$(CONFIG_MSM_SYSMON_GLINK_COMM) += sysmon-glink.o sysmon-qmi.o
obj-$(CONFIG_MSM_PIL_SSR_GENERIC) += subsys-pil-tz.o
obj-$(CONFIG_MSM_PIL)   +=      peripheral-loader.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	obj-y += subsystem_notif.o
	obj-y += subsystem_restart.o
	obj-y += ramdump.o
endif
obj-$(CONFIG_QCOM_EUD) += eud.o
obj-$(CONFIG_SOC_BUS) += socinfo.o
obj-$(CONFIG_QCOM_WATCHDOG_V2) += watchdog_v2.o
