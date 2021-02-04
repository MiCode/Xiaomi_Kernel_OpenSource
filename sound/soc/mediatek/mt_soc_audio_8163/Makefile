# MTK Platform Support

#include $(srctree)/drivers/misc/mediatek/Makefile.custom
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
# ccflags-y += -D_GIT318_READY
# ccflags-y += -D_GIT318_PMIC_READY
#ccflags-y += -D_WCN_SUPPORT
ccflags-y += -DAUDIO_USING_WRAP_DRIVER
#ccflags-y += -D_MT_IDLE_HEADER

EXTRA_CFLAGS := -I$(src)/../common
obj-$(CONFIG_SND_SOC_MT8163) += AudDrv_Afe.o AudDrv_Ana.o AudDrv_Clk.o AudDrv_Gpio.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_afe_control.o mt_soc_afe_connection.o mt_soc_pcm_common.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_capture.o mt_soc_pcm_dl1.o mt_soc_pcm_dummy.o mt_soc_pcm_routing.o mt_soc_pcm_capture2.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_voice_md1.o mt_soc_pcm_voice_md2.o mt_soc_pcm_voice_md1_bt.o mt_soc_pcm_voice_md2_bt.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_hdmi.o mt_soc_pcm_dl1_i2s0.o mt_soc_pcm_dl1_i2s0Dl1.o mt_soc_pcm_i2s0_awb.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_uldlloopback.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_mrgrx.o mt_soc_pcm_mrgrx_awb.o mt_soc_pcm_fm_i2s.o mt_soc_pcm_fm_i2s_awb.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_dl1_awb.o mt_soc_pcm_dl1_bt.o mt_soc_pcm_bt_dai.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_dai_stub.o mt_soc_dai_routing.o mt_soc_codec_dummy.o mt_soc_codec_63xx.o mt_soc_pcm_fmtx.o mt_soc_tdm_capture.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_pcm_hp_impedance.o
obj-$(CONFIG_SND_SOC_MT8163) += mt_soc_machine.o
obj-$(CONFIG_MTK_SPEAKER) += mt_soc_codec_speaker_63xx.o

ifeq ($(CONFIG_MT_ENG_BUILD),y)
ifeq ($(CONFIG_SND_SOC_MT8163),y)
CFLAGS_AudDrv_Afe.o += -DDEBUG
CFLAGS_AudDrv_Ana.o += -DDEBUG
CFLAGS_AudDrv_Clk.o += -DDEBUG
CFLAGS_AudDrv_Gpio.o += -DDEBUG
CFLAGS_mt_soc_afe_control.o += -DDEBUG
CFLAGS_mt_soc_afe_connection.o += -DDEBUG
CFLAGS_mt_soc_pcm_common.o += -DDEBUG
CFLAGS_mt_soc_pcm_capture.o += -DDEBUG
CFLAGS_mt_soc_pcm_dl1.o += -DDEBUG
CFLAGS_mt_soc_pcm_dummy.o += -DDEBUG
CFLAGS_mt_soc_pcm_routing.o += -DDEBUG
CFLAGS_mt_soc_pcm_capture2.o += -DDEBUG
CFLAGS_mt_soc_pcm_voice_md1.o += -DDEBUG
CFLAGS_mt_soc_pcm_voice_md2.o += -DDEBUG
CFLAGS_mt_soc_pcm_voice_md1_bt.o += -DDEBUG
CFLAGS_mt_soc_pcm_voice_md2_bt.o += -DDEBUG
CFLAGS_mt_soc_pcm_hdmi.o += -DDEBUG
CFLAGS_mt_soc_pcm_dl1_i2s0.o += -DDEBUG
CFLAGS_mt_soc_pcm_dl1_i2s0Dl1.o += -DDEBUG
CFLAGS_mt_soc_pcm_i2s0_awb.o += -DDEBUG
CFLAGS_mt_soc_pcm_uldlloopback.o += -DDEBUG
CFLAGS_mt_soc_pcm_mrgrx.o += -DDEBUG
CFLAGS_mt_soc_pcm_mrgrx_awb.o += -DDEBUG
CFLAGS_mt_soc_pcm_fm_i2s.o += -DDEBUG
CFLAGS_mt_soc_pcm_fm_i2s_awb.o += -DDEBUG
CFLAGS_mt_soc_pcm_dl1_awb.o += -DDEBUG
CFLAGS_mt_soc_pcm_dl1_bt.o += -DDEBUG
CFLAGS_mt_soc_pcm_bt_dai.o += -DDEBUG
CFLAGS_mt_soc_dai_stub.o += -DDEBUG
CFLAGS_mt_soc_dai_routing.o += -DDEBUG
CFLAGS_mt_soc_codec_dummy.o += -DDEBUG
CFLAGS_mt_soc_codec_63xx.o += -DDEBUG
CFLAGS_mt_soc_pcm_fmtx.o += -DDEBUG
CFLAGS_mt_soc_tdm_capture.o += -DDEBUG
CFLAGS_mt_soc_pcm_hp_impedance.o += -DDEBUG
CFLAGS_mt_soc_machine.o += -DDEBUG
CFLAGS_mt_soc_codec_speaker_63xx.o += -DDEBUG
endif
endif