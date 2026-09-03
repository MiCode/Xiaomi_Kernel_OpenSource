# SPDX-License-Identifier: GPL-2.0

sprd-gsp-$(CONFIG_UNISOC_GSP) += gsp_r8p0/gsp_r8p0_core.o \
	gsp_r9p0/gsp_r9p0_core.o \
	gsp_coef_cal.o

sprd-gsp-$(CONFIG_UNISOC_GSP) += gsp_interface/gsp_interface_sharkl5pro.o \
	gsp_interface/gsp_interface_qogirn6pro.o

sprd-gsp-$(CONFIG_UNISOC_GSP) += gsp_core.o gsp_dev.o gsp_interface.o gsp_kcfg.o \
	gsp_layer.o gsp_sync.o gsp_sysfs.o gsp_workqueue.o

obj-$(CONFIG_UNISOC_GSP) += sprd-gsp.o
#sprd-gsp-$(CONFIG_DRM_SPRD_GSP_IPC_TRUSTY) += gsp_ipc_trusty.o
#sprd-gsp-$(CONFIG_DRM_SPRD_GSP_DVFS) += gsp_r9p0/gsp_r9p0_dvfs.o
