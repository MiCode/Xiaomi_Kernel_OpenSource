MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

snd-soc-mt8173-pcm-objs := \
    mt_afe_reg.o mt_afe_clk.o mt_afe_control.o mt_afe_connection.o mt_afe_debug.o \
    mt_afe_pcm_dl1.o mt_afe_pcm_dl2.o mt_afe_pcm_capture.o mt_afe_pcm_capture2.o \
    mt_afe_pcm_dl1_awb.o mt_afe_pcm_i2s0_awb.o \
    mt_afe_pcm_btsco.o mt_afe_pcm_mrgrx.o mt_afe_pcm_mrgrx_awb.o \
    mt_afe_pcm_hdmi.o mt_afe_pcm_hdmi_raw.o mt_afe_pcm_spdif.o \
    mt_afe_pcm_routing.o mt_afe_dai_stub.o mt_afe_pcm_btsco2.o

snd-soc-mt8173-mt6391-mach-objs := \
    mt8173_evb.o

snd-soc-mt8173-mt6391-ariel-mach-objs := \
    ariel.o

obj-$(CONFIG_SND_SOC_MT8173) += snd-soc-mt8173-pcm.o
obj-$(CONFIG_SND_SOC_MT8173_MT6391_MACH) += snd-soc-mt8173-mt6391-mach.o
obj-$(CONFIG_SND_SOC_MT8173_MT6391_ARIEL_MACH) += snd-soc-mt8173-mt6391-ariel-mach.o
