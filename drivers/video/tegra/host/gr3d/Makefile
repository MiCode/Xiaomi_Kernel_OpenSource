GCOV_PROFILE := y
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/devfreq

nvhost-gr3d-objs  = \
		gr3d.o \
		gr3d_t114.o \
		scale3d.o \
		pod_scaling.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost-gr3d.o
