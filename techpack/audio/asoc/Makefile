# MSM Machine Support

snd-soc-hostless-pcm-objs := msm-pcm-hostless.o
obj-$(CONFIG_SND_SOC_MSM_HOSTLESS_PCM) += snd-soc-hostless-pcm.o


snd-soc-qdsp6v2-objs := msm-dai-fe.o
obj-$(CONFIG_SND_SOC_QDSP6V2) += snd-soc-qdsp6v2.o

#for CPE drivers
snd-soc-cpe-objs := msm-cpe-lsm.o
obj-$(CONFIG_SND_SOC_CPE) += snd-soc-cpe.o

# for MSM8998 sound card driver
snd-soc-msm8998-objs := msm8998.o
obj-$(CONFIG_SND_SOC_MACHINE_MSM8998) += snd-soc-msm8998.o

# for SDM670 sound card driver
snd-soc-sdm670-objs := sdm660-common.o sdm660-internal.o sdm660-external.o sdm660-ext-dai-links.o
obj-$(CONFIG_SND_SOC_SDM670) += snd-soc-sdm670.o

# for SDM845 sound card driver
snd-soc-sdm845-objs := sdm845.o
obj-$(CONFIG_SND_SOC_MACHINE_SDM845) += snd-soc-sdm845.o

# for SDXPOORWILLS sound card driver
snd-soc-sdxpoorwills-objs := sdxpoorwills.o
obj-$(CONFIG_SND_SOC_MACHINE_SDXPOORWILLS) += snd-soc-sdxpoorwills.o

snd-soc-qdsp6v2-objs += msm-dai-q6-v2.o msm-pcm-q6-v2.o \
			msm-pcm-routing-v2.o msm-compress-q6-v2.o \
			msm-pcm-afe-v2.o msm-pcm-voip-v2.o \
			msm-pcm-voice-v2.o msm-dai-q6-hdmi-v2.o \
			msm-lsm-client.o msm-pcm-host-voice-v2.o \
			msm-audio-effects-q6-v2.o msm-pcm-loopback-v2.o \
			msm-dai-slim.o msm-transcode-loopback-q6-v2.o msm-pcm-q6-noirq.o \
			msm-elliptic.o apr_elliptic.o
obj-$(CONFIG_SND_SOC_QDSP6V2) += snd-soc-qdsp6v2.o msm-pcm-dtmf-v2.o \
				 msm-dai-stub-v2.o
obj-$(CONFIG_SND_HWDEP) += msm-pcm-routing-devdep.o
obj-$(CONFIG_DOLBY_DAP) += msm-dolby-dap-config.o
obj-$(CONFIG_DOLBY_DS2) += msm-ds2-dap-config.o
obj-$(CONFIG_DOLBY_LICENSE) += msm-ds2-dap-config.o
obj-$(CONFIG_DTS_SRS_TM) += msm-dts-srs-tm-config.o
obj-$(CONFIG_QTI_PP) += msm-qti-pp-config.o
obj-y += codecs/
obj-y += elliptic/
