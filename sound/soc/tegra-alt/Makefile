GCOV_PROFILE := y

subdir-ccflags-y := -Werror

# Tegra platform Support
snd-soc-tegra-alt-pcm-objs := tegra_pcm_alt.o
snd-soc-tegra-alt-utils-objs := tegra_asoc_utils_alt.o
snd-soc-tegra30-alt-apbif-objs := tegra30_apbif_alt.o
snd-soc-tegra30-alt-xbar-objs := tegra30_xbar_alt.o
snd-soc-tegra30-alt-i2s-objs := tegra30_i2s_alt.o
snd-soc-tegra30-alt-spdif-objs := tegra30_spdif_alt.o
snd-soc-tegra114-alt-amx-objs := tegra114_amx_alt.o
snd-soc-tegra114-alt-adx-objs := tegra114_adx_alt.o

obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra-alt-pcm.o
obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra-alt-utils.o
obj-$(CONFIG_SND_SOC_TEGRA30_APBIF_ALT) += snd-soc-tegra30-alt-apbif.o
obj-$(CONFIG_SND_SOC_TEGRA30_XBAR_ALT) += snd-soc-tegra30-alt-xbar.o
obj-$(CONFIG_SND_SOC_TEGRA30_I2S_ALT) += snd-soc-tegra30-alt-i2s.o
obj-$(CONFIG_SND_SOC_TEGRA30_SPDIF_ALT) += snd-soc-tegra30-alt-spdif.o
obj-$(CONFIG_SND_SOC_TEGRA114_AMX_ALT) += snd-soc-tegra114-alt-amx.o
obj-$(CONFIG_SND_SOC_TEGRA114_ADX_ALT) += snd-soc-tegra114-alt-adx.o

# Tegra machine Support
snd-soc-tegra-alt-vcm30t124-objs := tegra_vcm30t124_alt.o
obj-$(CONFIG_SND_SOC_TEGRA_VCM30T124_ALT) += snd-soc-tegra-alt-vcm30t124.o
