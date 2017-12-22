snd-soc-qdsp6v2-objs += msm-dai-q6-v2.o msm-pcm-q6-v2.o msm-pcm-routing-v2.o \
			msm-compress-q6-v2.o \
			msm-pcm-afe-v2.o msm-pcm-voip-v2.o \
			msm-pcm-voice-v2.o msm-dai-q6-hdmi-v2.o \
			msm-lsm-client.o msm-pcm-host-voice-v2.o \
			msm-audio-effects-q6-v2.o msm-pcm-loopback-v2.o \
			msm-transcode-loopback-q6-v2.o \
			adsp_err.o
obj-$(CONFIG_SLIMBUS) += msm-dai-slim.o audio_slimslave.o
obj-$(CONFIG_SND_SOC_QDSP6V2) += snd-soc-qdsp6v2.o msm-pcm-dtmf-v2.o \
				 msm-dai-stub-v2.o
obj-$(CONFIG_SND_SOC_QDSP6V2_VM) += snd-soc-qdsp6v2.o msm-pcm-dtmf-v2.o \
				 msm-dai-stub-v2.o
obj-$(CONFIG_SND_HWDEP) += msm-pcm-routing-devdep.o
obj-$(CONFIG_DOLBY_DAP) += msm-dolby-dap-config.o
obj-$(CONFIG_DOLBY_DS2) += msm-ds2-dap-config.o
obj-$(CONFIG_DOLBY_LICENSE) += msm-ds2-dap-config.o
obj-$(CONFIG_DTS_SRS_TM) += msm-dts-srs-tm-config.o
obj-$(CONFIG_QTI_PP) += msm-qti-pp-config.o
obj-y += audio_calibration.o audio_cal_utils.o q6adm.o q6afe.o q6asm.o \
	q6audio-v2.o q6voice.o q6core.o rtac.o q6lsm.o \
	msm-pcm-q6-noirq.o q6common.o
ocmem-audio-objs += audio_ocmem.o
obj-$(CONFIG_AUDIO_OCMEM) += ocmem-audio.o
