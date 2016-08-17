GCOV_PROFILE := y
EXTRA_CFLAGS += -Idrivers/video/tegra/host -Idrivers/devfreq

nvhost-gr3d-objs  = \
		gr3d.o \
		gr3d_t20.o \
		gr3d_t30.o \
		gr3d_t114.o \
		scale3d.o \
		scale3d_actmon.o \
		pod_scaling.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost-gr3d.o
