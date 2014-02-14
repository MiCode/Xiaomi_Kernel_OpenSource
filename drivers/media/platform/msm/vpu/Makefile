GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

obj-$(CONFIG_MSM_VPU) := 	\
		vpu_v4l2.o \
		vpu_ioctl.o \
		vpu_configuration.o \
		vpu_translate.o \
		vpu_vb2_queue.o \
		vpu_resources.o \
		vpu_channel.o \
		vpu_hfi.o \
		vpu_bus_clock.o \

obj-$(CONFIG_DEBUG_FS) += vpu_debug.o

obj-$(CONFIG_MSM_VPU_IN_VCAP) += vpu_in_vcap.o
obj-$(CONFIG_MSM_VPU_OUT_MDSS) += vpu_out_mdss.o
