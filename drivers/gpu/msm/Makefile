ccflags-y := -Iinclude/drm

msm_kgsl_core-y = \
	kgsl.o \
	kgsl_sharedmem.o \
	kgsl_pwrctrl.o \
	kgsl_pwrscale.o

msm_kgsl_core-$(CONFIG_DEBUG_FS) += kgsl_debugfs.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_MMU) += kgsl_mmu.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_CFF_DUMP) += kgsl_cffdump.o
msm_kgsl_core-$(CONFIG_MSM_KGSL_DRM) += kgsl_drm.o
msm_kgsl_core-$(CONFIG_MSM_SCM) += kgsl_pwrscale_trustzone.o
msm_kgsl_core-$(CONFIG_MSM_SLEEP_STATS) += kgsl_pwrscale_idlestats.o

msm_adreno-y += \
	adreno_ringbuffer.o \
	adreno_drawctxt.o \
	adreno_postmortem.o \
	adreno_a2xx.o \
	adreno.o

msm_adreno-$(CONFIG_DEBUG_FS) += adreno_debugfs.o

msm_z180-y += z180.o

msm_kgsl_core-objs = $(msm_kgsl_core-y)
msm_adreno-objs = $(msm_adreno-y)
msm_z180-objs = $(msm_z180-y)

obj-$(CONFIG_MSM_KGSL) += msm_kgsl_core.o
obj-$(CONFIG_MSM_KGSL) += msm_adreno.o
obj-$(CONFIG_MSM_KGSL_2D) += msm_z180.o
