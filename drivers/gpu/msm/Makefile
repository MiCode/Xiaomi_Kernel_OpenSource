# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(src)

obj-$(CONFIG_QCOM_KGSL) += msm_kgsl.o

msm_kgsl-y = \
	kgsl.o \
	kgsl_bus.o \
	kgsl_drawobj.o \
	kgsl_events.o \
	kgsl_gmu_core.o \
	kgsl_ioctl.o \
	kgsl_mmu.o \
	kgsl_pool.o \
	kgsl_pwrctrl.o \
	kgsl_pwrscale.o \
	kgsl_sharedmem.o \
	kgsl_snapshot.o \
	kgsl_timeline.o \
	kgsl_trace.o \
	kgsl_util.o

msm_kgsl-$(CONFIG_COMPAT) += kgsl_compat.o
msm_kgsl-$(CONFIG_DEBUG_FS) += kgsl_debugfs.o
msm_kgsl-$(CONFIG_ARM_SMMU) += kgsl_iommu.o
msm_kgsl-$(CONFIG_SYNC_FILE) += kgsl_sync.o

msm_kgsl-y += \
	adreno.o \
	adreno_a3xx.o \
	adreno_a3xx_perfcounter.o \
	adreno_a3xx_snapshot.o \
	adreno_a5xx.o \
	adreno_a5xx_perfcounter.o \
	adreno_a5xx_preempt.o \
	adreno_a5xx_snapshot.o \
	adreno_a6xx.o \
	adreno_a6xx_gmu.o \
	adreno_a6xx_gmu_snapshot.o \
	adreno_a6xx_hfi.o \
	adreno_a6xx_hwsched.o \
	adreno_a6xx_hwsched_hfi.o \
	adreno_a6xx_perfcounter.o \
	adreno_a6xx_preempt.o \
	adreno_a6xx_rgmu.o \
	adreno_a6xx_rpmh.o \
	adreno_a6xx_snapshot.o \
	adreno_cp_parser.o \
	adreno_dispatch.o \
	adreno_drawctxt.o \
	adreno_hwsched.o \
	adreno_ioctl.o \
	adreno_perfcounter.o \
	adreno_ringbuffer.o \
	adreno_snapshot.o \
	adreno_sysfs.o \
	adreno_trace.o

msm_kgsl-$(CONFIG_COMPAT) += adreno_compat.o
msm_kgsl-$(CONFIG_QCOM_KGSL_CORESIGHT) += adreno_coresight.o
msm_kgsl-$(CONFIG_DEBUG_FS) += adreno_debugfs.o adreno_profile.o
msm_kgsl-$(CONFIG_ARM_SMMU) += adreno_iommu.o

obj-$(CONFIG_DEVFREQ_GOV_QCOM_ADRENO_TZ) += governor_msm_adreno_tz.o
obj-$(CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON) += governor_gpubw_mon.o
