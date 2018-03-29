MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

snd-soc-mt8167-pcm-objs := \
    mtk-afe-pcm.o mtk-afe-util.o mtk-afe-controls.o mtk-afe-debug.o

snd-soc-mt8167-mt6392-mach-objs := \
    mt8167_evb.o

obj-$(CONFIG_SND_SOC_MT8167) += snd-soc-mt8167-pcm.o
obj-$(CONFIG_SND_SOC_MT8167_MT6392_MACH) += snd-soc-mt8167-mt6392-mach.o
