# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(src)

msm_kgsl_core-y = \
	kgsl.o \
	kgsl_trace.o \
	kgsl_drawobj.o \
	kgsl_ioctl.o \
	kgsl_sharedmem.o \
	kgsl_pwrctrl.o \
	kgsl_pwrscale.o \
	kgsl_mmu.o \
	kgsl_snapshot.o \
	kgsl_events.o \
	kgsl_gmu_core.o \
	kgsl_gmu.o \
	kgsl_rgmu.o \
	kgsl_hfi.o \
	kgsl_pool.o \
	kgsl_reclaim.o

msm_kgsl_core-$(CONFIG_QCOM_KGSL_IOMMU) += kgsl_iommu.o
msm_kgsl_core-$(CONFIG_DEBUG_FS) += kgsl_debugfs.o
msm_kgsl_core-$(CONFIG_SYNC_FILE) += kgsl_sync.o
msm_kgsl_core-$(CONFIG_COMPAT) += kgsl_compat.o

msm_adreno-y += \
	adreno_ioctl.o \
	adreno_ringbuffer.o \
	adreno_drawctxt.o \
	adreno_dispatch.o \
	adreno_snapshot.o \
	adreno_coresight.o \
	adreno_trace.o \
	adreno_a3xx.o \
	adreno_a5xx.o \
	adreno_a6xx.o \
	adreno_a3xx_snapshot.o \
	adreno_a5xx_snapshot.o \
	adreno_a6xx_snapshot.o \
	adreno_a5xx_preempt.o \
	adreno_a6xx_preempt.o \
	adreno_a6xx_gmu.o \
	adreno_a6xx_rgmu.o \
	adreno_sysfs.o \
	adreno.o \
	adreno_cp_parser.o \
	adreno_perfcounter.o

msm_adreno-$(CONFIG_QCOM_KGSL_IOMMU) += adreno_iommu.o
msm_adreno-$(CONFIG_DEBUG_FS) += adreno_debugfs.o adreno_profile.o
msm_adreno-$(CONFIG_COMPAT) += adreno_compat.o

msm_kgsl_core-objs = $(msm_kgsl_core-y)
msm_adreno-objs = $(msm_adreno-y)

obj-$(CONFIG_QCOM_KGSL) += msm_kgsl_core.o
obj-$(CONFIG_QCOM_KGSL) += msm_adreno.o
