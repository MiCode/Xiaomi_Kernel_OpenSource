ccflags-y := -Iinclude/uapi/drm -Iinclude/drm -Idrivers/gpu/msm

msm_kgsl_core-y = \
	kgsl.o \
	kgsl_trace.o \
	kgsl_cmdbatch.o \
	kgsl_ioctl.o \
	kgsl_sharedmem.o \
	kgsl_pwrctrl.o \
	kgsl_pwrscale.o \
	kgsl_mmu.o \
	kgsl_snapshot.o \
	kgsl_events.o

msm_kgsl_core-$(CONFIG_MSM_KGSL_IOMMU) += kgsl_iommu.o
msm_kgsl_core-$(CONFIG_DEBUG_FS) += kgsl_debugfs.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_CFF_DUMP) += kgsl_cffdump.o
msm_kgsl_core-$(CONFIG_SYNC) += kgsl_sync.o
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
	adreno_a4xx.o \
	adreno_a5xx.o \
	adreno_a3xx_snapshot.o \
	adreno_a4xx_snapshot.o \
	adreno_a5xx_snapshot.o \
	adreno_sysfs.o \
	adreno.o \
	adreno_cp_parser.o \
	adreno_perfcounter.o

msm_adreno-$(CONFIG_MSM_KGSL_IOMMU) += adreno_iommu.o
msm_adreno-$(CONFIG_DEBUG_FS) += adreno_debugfs.o adreno_profile.o
msm_adreno-$(CONFIG_COMPAT) += adreno_compat.o

msm_kgsl_core-objs = $(msm_kgsl_core-y)
msm_adreno-objs = $(msm_adreno-y)

obj-$(CONFIG_MSM_KGSL) += msm_kgsl_core.o
obj-$(CONFIG_MSM_KGSL) += msm_adreno.o
