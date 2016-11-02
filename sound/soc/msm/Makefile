# MSM Machine Support

snd-soc-hostless-pcm-objs := msm-pcm-hostless.o
obj-$(CONFIG_SND_SOC_MSM_HOSTLESS_PCM) += snd-soc-hostless-pcm.o

obj-$(CONFIG_SND_SOC_MSM_QDSP6V2_INTF) += qdsp6v2/

snd-soc-qdsp6v2-objs := msm-dai-fe.o
obj-$(CONFIG_SND_SOC_QDSP6V2) += snd-soc-qdsp6v2.o

#for CPE drivers
snd-soc-cpe-objs := msm-cpe-lsm.o
obj-$(CONFIG_SND_SOC_CPE) += snd-soc-cpe.o

# for MSM8996 sound card driver
snd-soc-msm8996-objs := msm8996.o
obj-$(CONFIG_SND_SOC_MSM8996) += snd-soc-msm8996.o

# for MSMCOBALT sound card driver
snd-soc-msmcobalt-objs := msmcobalt.o
obj-$(CONFIG_SND_SOC_MSMCOBALT) += snd-soc-msmcobalt.o

# for MSMFALCON sound card driver
snd-soc-msmfalcon-common-objs := msm-audio-pinctrl.o msmfalcon-common.o
obj-$(CONFIG_SND_SOC_MSMFALCON_COMMON) += snd-soc-msmfalcon-common.o

# for MSMFALCON sound card driver
snd-soc-int-codec-objs := msmfalcon-internal.o
obj-$(CONFIG_SND_SOC_INT_CODEC) += snd-soc-msmfalcon-common.o
obj-$(CONFIG_SND_SOC_INT_CODEC) += snd-soc-int-codec.o

# for MSMFALCON sound card driver
snd-soc-ext-codec-objs := msmfalcon-external.o msmfalcon-ext-dai-links.o
obj-$(CONFIG_SND_SOC_EXT_CODEC) += snd-soc-msmfalcon-common.o
obj-$(CONFIG_SND_SOC_EXT_CODEC) += snd-soc-ext-codec.o

