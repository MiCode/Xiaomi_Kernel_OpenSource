GCOV_PROFILE := y
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/devfreq
ccflags-y += -Wno-multichar

#
# Arrange for code sharing among configurations.
#
config_present = $(if $(findstring y,$(1)),y,)

all_configs = $(call config_present,$(CONFIG_ARCH_TEGRA_2x_SOC) \
 $(CONFIG_ARCH_TEGRA_3x_SOC) $(CONFIG_ARCH_TEGRA_11x_SOC) \
 $(CONFIG_ARCH_TEGRA_12x_SOC) $(CONFIG_ARCH_TEGRA_14x_SOC))

t3x_or_higher_config = $(call config_present,$(CONFIG_ARCH_TEGRA_3x_SOC)\
  $(t11x_or_higher_config))

t11x_or_higher_config = $(call config_present,$(CONFIG_ARCH_TEGRA_11x_SOC)\
  $(t14x_or_higher_config))

t14x_or_higher_config = $(call config_present,$(CONFIG_ARCH_TEGRA_14x_SOC)\
  $(t12x_or_higher_config))

t12x_or_higher_config = $(call config_present,$(CONFIG_ARCH_TEGRA_12x_SOC))

nvhost-objs = \
	nvhost_acm.o \
	nvhost_as.o \
	nvhost_syncpt.o \
	nvhost_cdma.o \
	nvhost_intr.o \
	nvhost_channel.o \
	nvhost_job.o \
	dev.o \
	debug.o \
	bus_client.o \
	chip_support.o \
	nvhost_memmgr.o \
	nvhost_scale.o \
	user_hwctx.o

obj-$(all_configs) += nvhost.o
obj-$(all_configs) += host1x/
obj-$(all_configs) += gr3d/
obj-$(t11x_or_higher_config) += t114/
obj-$(t14x_or_higher_config) += t148/
obj-$(t11x_or_higher_config) += msenc/
obj-$(t11x_or_higher_config) += tsec/
obj-$(CONFIG_TEGRA_GRHOST) += gr2d/
obj-$(CONFIG_TEGRA_GRHOST) += isp/
obj-$(CONFIG_TEGRA_GRHOST) += vi/
obj-$(t12x_or_higher_config) += t124/
ifeq ($(CONFIG_TEGRA_GK20A),y)
obj-$(t12x_or_higher_config) += gk20a/
endif
obj-$(t12x_or_higher_config) += vic03/
obj-$(t12x_or_higher_config) += nvhost_allocator.o
obj-$(CONFIG_TEGRA_GRHOST_USE_NVMAP) += nvmap.o
obj-$(CONFIG_TEGRA_GRHOST_USE_DMABUF) += dmabuf.o

obj-$(CONFIG_TEGRA_GRHOST_SYNC) += nvhost_sync.o
