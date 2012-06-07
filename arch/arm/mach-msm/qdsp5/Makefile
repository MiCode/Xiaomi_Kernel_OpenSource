obj-y += adsp.o adsp_driver.o adsp_info.o adsp_rm.o dsp_debug.o adsp_debug.o
obj-y += adsp_video_verify_cmd.o
obj-y += adsp_videoenc_verify_cmd.o
obj-y += adsp_jpeg_verify_cmd.o adsp_jpeg_patch_event.o
obj-y += adsp_vfe_verify_cmd.o adsp_vfe_patch_event.o
obj-y += adsp_lpm_verify_cmd.o
ifdef CONFIG_MSM7X27A_AUDIO
obj-y += audio_pcm_in.o
obj-$(CONFIG_DEBUG_FS) += audio_voice_lb.o
else
obj-y += audio_in.o snd_pcm_client.o
endif
obj-$(CONFIG_MSM7X27A_AUDIO) += audio_aac_in.o audio_evrc_in.o audio_qcelp_in.o
obj-y += audio_out.o audio_mp3.o audmgr.o audpp.o audrec.o audpreproc.o
obj-y += audio_evrc.o audio_qcelp.o audio_amrnb.o audio_aac.o audio_amrnb_in.o
obj-y += audio_wma.o audio_voicememo.o audio_pcm.o audio_amrwb.o audio_wmapro.o
obj-y += snd.o snd_adie.o
obj-$(CONFIG_ARCH_MSM7X27A) += audio_fm.o
obj-$(CONFIG_ARCH_MSM7X27A) += audio_mvs.o
obj-$(CONFIG_ARCH_MSM7X27A) += audio_lpa.o
