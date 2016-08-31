GCOV_PROFILE := y
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/camera
ccflags-y += -Wno-multichar

nvhost-t124-objs  = \
	t124.o \
	3dctx_t124.o \
	debug_t124.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost-t124.o
