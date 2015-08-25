obj-$(CONFIG_QCOM_GSBI)	+=	qcom_gsbi.o
obj-$(CONFIG_QCOM_PM)	+=	spm.o
obj-$(CONFIG_QCOM_SMD) +=	smd.o
obj-$(CONFIG_QCOM_SMD_RPM)	+= smd-rpm.o
obj-$(CONFIG_QCOM_SMEM) +=	smem.o
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-$(CONFIG_QCOM_SCM)  +=      scm.o scm-boot.o
obj-$(CONFIG_QCOM_XPU_ERR_FATAL) += scm-xpu.o
