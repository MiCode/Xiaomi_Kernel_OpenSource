# MSM Machine Support

snd-soc-hostless-pcm-objs := msm-pcm-hostless.o audio_test_mod.o
obj-$(CONFIG_SND_SOC_MSM_HOSTLESS_PCM) += snd-soc-hostless-pcm.o

obj-$(CONFIG_SND_SOC_MSM_QDSP6V2_INTF) += qdsp6v2/

snd-soc-qdsp6v2-objs := msm-dai-fe.o
obj-$(CONFIG_SND_SOC_QDSP6V2) += snd-soc-qdsp6v2.o

#for CPE drivers
snd-soc-cpe-objs := msm-cpe-lsm.o
obj-$(CONFIG_SND_SOC_CPE) += snd-soc-cpe.o

# for MSM8996 sound card driver
snd-soc-msm8996-objs := msm8996.o apq8096-auto.o
obj-$(CONFIG_SND_SOC_MSM8996) += snd-soc-msm8996.o

# for MSM8998 sound card driver
snd-soc-msm8998-objs := msm8998.o
obj-$(CONFIG_SND_SOC_MSM8998) += snd-soc-msm8998.o

# for SDM660 sound card driver
snd-soc-sdm660-common-objs := sdm660-common.o
obj-$(CONFIG_SND_SOC_SDM660_COMMON) += snd-soc-sdm660-common.o

# for SDM660 sound card driver
snd-soc-int-codec-objs := sdm660-internal.o
obj-$(CONFIG_SND_SOC_INT_CODEC) += snd-soc-sdm660-common.o
obj-$(CONFIG_SND_SOC_INT_CODEC) += snd-soc-int-codec.o

# for SDM660 sound card driver
snd-soc-ext-codec-objs := sdm660-external.o sdm660-ext-dai-links.o
obj-$(CONFIG_SND_SOC_EXT_CODEC) += snd-soc-sdm660-common.o
obj-$(CONFIG_SND_SOC_EXT_CODEC) += snd-soc-ext-codec.o
