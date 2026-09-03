subdir-ccflags-y += -I$(src)
subdir-ccflags-y += -I$(src)/sys

ifdef CONFIG_ARM64
KBUILD_CFLAGS += -mstrict-align
endif

apsys-dvfs-y := \
	sprd_dvfs_apsys.o \
	sprd_dvfs_dpu.o \
	sprd_dvfs_vsp.o \
	dpu/dpu_dvfs_qogirn6pro.o \
	dpu/dpu_dvfs_qogirn6lite.o \
	dpu/dpu_dvfs_sharkl5pro.o \
	dpu/dpu_dvfs_sharkl5.o \
	dpu/dpu_dvfs_qogirl6.o \
	dpu/dpu_dvfs_roc1.o \
	sys/apsys_dvfs_qogirl6.o \
	sys/apsys_dvfs_qogirn6pro.o \
	sys/apsys_dvfs_qogirn6lite.o \
	sys/apsys_dvfs_sharkl5.o \
	sys/apsys_dvfs_sharkl5pro.o \
	sys/apsys_dvfs_roc1.o \
	vsp/vsp_dvfs_roc1.o \
	vsp/vsp_dvfs_sharkl5.o \
	vsp/vsp_dvfs_sharkl5pro.o \
	vsp/vsp_dvfs_qogirl6.o \
	vsp/vpudec_dvfs_qogirn6pro.o \
	vsp/vpuenc_dvfs_qogirn6pro.o \
	vsp/vpudec_dvfs_qogirn6lite.o \
	vsp/vpuenc_dvfs_qogirn6lite.o

apsys-dvfs-$(CONFIG_DRM_SPRD_GSP_DVFS) += \
	sprd_dvfs_gsp.o \
	gsp/gsp_dvfs_qogirn6pro.o \
	gsp/gsp_dvfs_qogirn6lite.o

apsys-dvfs-$(CONFIG_DVFS_APSYS_VDSP) += \
	sprd_dvfs_vdsp.o \
	vdsp/vdsp_dvfs_sharkl5pro.o

obj-$(CONFIG_DVFS_APSYS_SPRD) += apsys-dvfs.o