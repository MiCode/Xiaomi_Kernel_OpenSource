# Makefile for Tegra SoC camera driver

tegra_camera-objs += common.o vi.o vi2.o

obj-$(CONFIG_VIDEO_TEGRA) += tegra_camera.o

ccflags-y += -I$(srctree)/drivers/video/tegra/host
