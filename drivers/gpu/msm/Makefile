ccflags-y := -Iinclude/uapi/drm -Iinclude/drm -Idrivers/gpu/msm

msm_kgsl_core-y = \
	kgsl.o \
	kgsl_trace.o \
	kgsl_sharedmem.o \
	kgsl_pwrctrl.o \
	kgsl_pwrscale.o \
	kgsl_mmu.o \
	kgsl_gpummu.o \
	kgsl_iommu.o \
	kgsl_snapshot.o \
	kgsl_events.o

msm_kgsl_core-$(CONFIG_DEBUG_FS) += kgsl_debugfs.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_CFF_DUMP) += kgsl_cffdump.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_DRM) += kgsl_drm.o
msm_kgsl_core-$(CONFIG_MSM_SCM) += kgsl_pwrscale_trustzone.o
msm_kgsl_core-$(CONFIG_MSM_SLEEP_STATS_DEVICE) += kgsl_pwrscale_idlestats.o
msm_kgsl_core-$(CONFIG_SYNC) += kgsl_sync.o

msm_adreno-y += \
	adreno_ringbuffer.o \
	adreno_drawctxt.o \
	adreno_postmortem.o \
	adreno_snapshot.o \
	adreno_coresight.o \
	adreno_a2xx.o \
	adreno_a2xx_trace.o \
	adreno_a2xx_snapshot.o \
	adreno_a3xx.o \
	adreno_a3xx_trace.o \
	adreno_a3xx_snapshot.o \
	adreno.o

msm_adreno-$(CONFIG_DEBUG_FS) += adreno_debugfs.o

msm_z180-y += \
	z180.o \
	z180_postmortem.o \
	z180_trace.o

msm_kgsl_core-objs = $(msm_kgsl_core-y)
msm_adreno-objs = $(msm_adreno-y)
msm_z180-objs = $(msm_z180-y)

obj-$(CONFIG_MSM_KGSL) += msm_kgsl_core.o
obj-$(CONFIG_MSM_KGSL) += msm_adreno.o
obj-$(CONFIG_MSM_KGSL_2D) += msm_z180.o
