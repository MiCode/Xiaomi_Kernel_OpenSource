ifdef CONFIG_ARCH_MSM8X60
obj-y += audio_dev_ctl.o
obj-y += board-msm8x60-audio.o
obj-$(CONFIG_TIMPANI_CODEC) += snddev_icodec.o
obj-y += snddev_ecodec.o snddev_mi2s.o snddev_virtual.o
obj-y += pcm_out.o pcm_in.o fm.o
obj-y += audio_lpa.o
obj-y += q6voice.o
obj-y += snddev_hdmi.o
obj-y += audio_mvs.o
obj-$(CONFIG_FB_MSM_HDMI_MSM_PANEL) += lpa_if_hdmi.o
endif
obj-$(CONFIG_MSM_QDSP6_APR) += apr.o apr_v1.o apr_tal.o q6core.o dsp_debug.o
obj-$(CONFIG_MSM_QDSP6_APRV2) += apr.o apr_v2.o apr_tal.o q6core.o dsp_debug.o
obj-y += audio_acdb.o
ifdef CONFIG_ARCH_MSM9615
obj-y += rtac.o
endif
obj-$(CONFIG_MSM_QDSP6_CODECS) += aac_in.o qcelp_in.o evrc_in.o amrnb_in.o audio_utils.o
obj-$(CONFIG_MSM_QDSP6_CODECS) += audio_wma.o audio_wmapro.o audio_aac.o audio_multi_aac.o audio_utils_aio.o
obj-$(CONFIG_MSM_QDSP6_CODECS) += rtac.o q6audio_v1.o q6audio_v1_aio.o
obj-$(CONFIG_MSM_QDSP6_CODECS) += audio_mp3.o audio_amrnb.o audio_amrwb.o audio_evrc.o audio_qcelp.o amrwb_in.o
obj-$(CONFIG_MSM_QDSP6V2_CODECS) += aac_in.o qcelp_in.o evrc_in.o amrnb_in.o audio_utils.o
obj-$(CONFIG_MSM_QDSP6V2_CODECS) += audio_wma.o audio_wmapro.o audio_aac.o audio_multi_aac.o audio_utils_aio.o
obj-$(CONFIG_MSM_QDSP6V2_CODECS) += rtac_v2.o q6audio_v2.o q6audio_v2_aio.o
obj-$(CONFIG_MSM_QDSP6V2_CODECS) += audio_mp3.o audio_amrnb.o audio_amrwb.o audio_evrc.o audio_qcelp.o amrwb_in.o
obj-$(CONFIG_MSM_ADSP_LOADER) += adsp-loader.o
obj-$(CONFIG_MSM_ULTRASOUND) += ultrasound/
