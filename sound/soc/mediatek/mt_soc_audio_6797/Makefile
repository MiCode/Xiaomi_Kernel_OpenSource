#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# MTK Platform Support
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/mt6797/
# flags for not ready feature, uncomment after ready
ccflags-y += -D_GIT318_READY
ccflags-y += -D_GIT318_PMIC_READY
ccflags-y += -DAUDIO_USING_WRAP_DRIVER
ccflags-y += -D_MT_IDLE_HEADER

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/$(MTK_PLATFORM)
ccflags-y += -DMTK_AURISYS_PHONE_CALL_SUPPORT
ccflags-y += -DMTK_AUDIO_TUNNELING_SUPPORT
endif

#
EXTRA_CFLAGS := -I$(src)/../common
obj-$(CONFIG_MT_SND_SOC_6797) += AudDrv_Afe.o AudDrv_Ana.o AudDrv_Clk.o AudDrv_Gpio.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_afe_control.o mt_soc_afe_connection.o mt_soc_pcm_common.o mt_soc_offload_common.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_capture.o mt_soc_pcm_dl1.o mt_soc_pcm_dummy.o mt_soc_pcm_routing.o mt_soc_pcm_capture2.o mt_soc_pcm_i2s2_adc2.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_voice_md1.o mt_soc_pcm_voice_md2.o mt_soc_pcm_voice_md1_bt.o mt_soc_pcm_voice_md2_bt.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_hdmi.o mt_soc_pcm_dl1_i2s0.o mt_soc_pcm_dl1_i2s0Dl1.o mt_soc_pcm_i2s0_awb.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_uldlloopback.o mt_soc_pcm_dl2.o mt_soc_pcm_mod_dai.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_anc.o mt_soc_pcm_voice_ultra.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_mrgrx.o mt_soc_pcm_mrgrx_awb.o mt_soc_pcm_fm_i2s.o mt_soc_pcm_fm_i2s_awb.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_dl1_awb.o mt_soc_pcm_dl1_bt.o mt_soc_pcm_bt_dai.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_dai_stub.o mt_soc_dai_routing.o mt_soc_codec_dummy.o mt_soc_codec_63xx.o mt_soc_pcm_fmtx.o mt_soc_tdm_capture.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_pcm_hp_impedance.o
obj-$(CONFIG_MT_SND_SOC_6797) += mt_soc_machine.o
obj-$(CONFIG_MTK_SPEAKER) += mt_soc_codec_speaker_63xx.o
