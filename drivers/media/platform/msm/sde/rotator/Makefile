ccflags-y += -I$(src) -Idrivers/staging/android

obj-y := \
		sde_rotator_dev.o \
		sde_rotator_core.o \
		sde_rotator_base.o \
		sde_rotator_formats.o \
		sde_rotator_util.o \
		sde_rotator_io_util.o \
		sde_rotator_smmu.o

obj-y += \
		sde_rotator_r1_wb.o \
		sde_rotator_r1_pipe.o \
		sde_rotator_r1_ctl.o \
		sde_rotator_r1.o

obj-y += \
		sde_rotator_r3.o

obj-$(CONFIG_SYNC) += \
		sde_rotator_sync.o

obj-$(CONFIG_DEBUG_FS) += \
		sde_rotator_debug.o \
		sde_rotator_r1_debug.o \
		sde_rotator_r3_debug.o
