obj-$(CONFIG_SND_SOC_MSM_QDSP6V2_INTF) += audio_calibration.o audio_cal_utils.o \
						q6adm.o q6afe.o q6asm.o q6audio-v2.o q6voice.o q6core.o \
						rtac.o q6lsm.o audio_slimslave.o adsp_err.o msm_audio_ion.o
obj-$(CONFIG_MSM_ADSP_LOADER) += adsp-loader.o
obj-$(CONFIG_MSM_QDSP6_SSR) += audio_ssr.o
obj-$(CONFIG_MSM_QDSP6_PDR) += audio_pdr.o
obj-$(CONFIG_MSM_QDSP6_NOTIFIER) += audio_notifier.o
obj-$(CONFIG_MSM_ULTRASOUND) += usf.o usfcdev.o q6usm.o

ifeq ($(CONFIG_ARCH_SDM845), y)
obj-$(CONFIG_MSM_AVTIMER) += avtimer.o
endif
obj-$(CONFIG_MSM_QDSP6V2_CODECS)  += codecs/
