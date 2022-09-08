// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_machine.c
 *
 * Project:
 * --------
 *   Audio soc machine driver
 *
 * Description:
 * ------------
 *   Audio machine driver
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"

#include <asm/div64.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <stdarg.h>

#include "mtk-soc-codec-63xx.h"
#include <linux/clk.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "mtk-soc-speaker-amp.h"

#if IS_ENABLED(CONFIG_SND_SOC_CS43130)
#include "mtk-cs43130-machine-ops.h"
#endif
#if IS_ENABLED(CONFIG_SND_SOC_CS35L35)
#include "mtk-cs35l35-machine-ops.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>

static struct dentry *mt_sco_audio_debugfs;
#define DEBUG_FS_NAME "mtksocaudio"
#define DEBUG_ANA_FS_NAME "mtksocanaaudio"

static int mt_soc_ana_debug_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mt_soc_ana_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	const int size = 8192;
	/* char buffer[size]; */
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		kfree(buffer);
		return -ENOMEM;
	}

	AudDrv_Clk_On();
	audckbufEnable(true);

	n = Ana_Debug_Read(buffer, size);

	audckbufEnable(false);
	AudDrv_Clk_Off();

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static int mt_soc_debug_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	const int size = 12288;
	/* char buffer[size]; */
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		kfree(buffer);
		return -ENOMEM;
	}

	AudDrv_Clk_On();

	n = AudDrv_Reg_Dump(buffer, size);

	AudDrv_Clk_Off();

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static char const ParSetkeyAfe[] = "Setafereg";
static char const ParSetkeyAna[] = "Setanareg";
static char const PareGetkeyAfe[] = "Getafereg";
static char const PareGetkeyAna[] = "Getanareg";

static ssize_t mt_soc_debug_write(struct file *f, const char __user *buf,
				  size_t count, loff_t *offset)
{
#define MAX_DEBUG_WRITE_INPUT 256
	int ret = 0;
	char InputBuf[MAX_DEBUG_WRITE_INPUT];
	char *token1 = NULL;
	char *token2 = NULL;
	char *token3 = NULL;
	char *token4 = NULL;
	char *token5 = NULL;
	char *temp = NULL;
	char *str_begin = NULL;

	unsigned long regaddr = 0;
	unsigned long regvalue = 0;
	char delim[] = " ,";

	if (!count) {
		pr_debug("%s(), count is 0, return directly\n", __func__);
		goto exit;
	}

	if (count > MAX_DEBUG_WRITE_INPUT)
		count = MAX_DEBUG_WRITE_INPUT;

	memset_io((void *)InputBuf, 0, MAX_DEBUG_WRITE_INPUT);

	if (copy_from_user((InputBuf), buf, count)) {
		pr_debug("%s(), copy_from_user fail, count = %zu\n",
			 __func__, count);
		goto exit;
	}

	str_begin = kstrndup(InputBuf, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);
	if (!str_begin) {
		pr_warn("%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	pr_debug(
		"copy_from_user count = %zu, temp = %s, pointer = %p\n",
		count, str_begin, str_begin);
	token1 = strsep(&temp, delim);
	token2 = strsep(&temp, delim);
	token3 = strsep(&temp, delim);
	token4 = strsep(&temp, delim);
	token5 = strsep(&temp, delim);
	pr_debug("token1 = %s token2 = %s token3 = %s token4 = %s token5 = %s\n",
		token1, token2, token3, token4, token5);

	AudDrv_Clk_On();
	if (strcmp(token1, ParSetkeyAfe) == 0) {
		if ((token3 != NULL) && (token5 != NULL)) {
			ret = kstrtoul(token3, 16, &regaddr);
			ret = kstrtoul(token5, 16, &regvalue);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 ParSetkeyAfe, (unsigned int)regaddr,
				 (unsigned int)regvalue);
			Afe_Set_Reg(regaddr, regvalue, 0xffffffff);
			regvalue = Afe_Get_Reg(regaddr);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 ParSetkeyAfe, (unsigned int)regaddr,
				 (unsigned int)regvalue);
		} else {
			pr_debug("token3 or token5 is NULL!\n");
		}
	}

	if (strcmp(token1, ParSetkeyAna) == 0) {
		if ((token3 != NULL) && (token5 != NULL)) {
			ret = kstrtoul(token3, 16, &regaddr);
			ret = kstrtoul(token5, 16, &regvalue);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 ParSetkeyAna, (unsigned int)regaddr,
				 (unsigned int)regvalue);
			audckbufEnable(true);
			Ana_Set_Reg(regaddr, regvalue, 0xffffffff);
			regvalue = Ana_Get_Reg(regaddr);
			audckbufEnable(false);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 ParSetkeyAna, (unsigned int)regaddr,
				 (unsigned int)regvalue);
		} else {
			pr_debug("token3 or token5 is NULL!\n");
		}
	}

	if (strcmp(token1, PareGetkeyAfe) == 0) {
		if (token3 != NULL) {
			ret = kstrtoul(token3, 16, &regaddr);
			regvalue = Afe_Get_Reg(regaddr);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 PareGetkeyAfe, (unsigned int)regaddr,
				 (unsigned int)regvalue);
		} else {
			pr_debug("token3 is NULL!\n");
		}
	}

	if (strcmp(token1, PareGetkeyAna) == 0) {
		if (token3 != NULL) {
			ret = kstrtoul(token3, 16, &regaddr);
			regvalue = Ana_Get_Reg(regaddr);
			pr_debug("%s, regaddr = 0x%x, regvalue = 0x%x\n",
				 PareGetkeyAna, (unsigned int)regaddr,
				 (unsigned int)regvalue);
		} else {
			pr_debug("token3 is NULL!\n");
		}
	}
	AudDrv_Clk_Off();

	kfree(str_begin);
exit:
	return count;
}

static const struct file_operations mtaudio_debug_ops = {
	.open = mt_soc_debug_open,
	.read = mt_soc_debug_read,
	.write = mt_soc_debug_write,
};

static const struct file_operations mtaudio_ana_debug_ops = {
	.open = mt_soc_ana_debug_open, .read = mt_soc_ana_debug_read,
};
#endif

/* snd_soc_ops */
static int mt_machine_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		EnableAfe(true);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		EnableAfe(false);
		return 0;
	}
	return -EINVAL;
}

static struct snd_soc_ops mt_machine_audio_ops = {
	.trigger = mt_machine_trigger,
};

/* FE */
SND_SOC_DAILINK_DEFS(multimedia1,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_DL1DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_TXDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_DL1_PCM)));
SND_SOC_DAILINK_DEFS(multimedia2,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_UL1DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_RXDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_UL1_PCM)));
SND_SOC_DAILINK_DEFS(voice_md1,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOICE_MD1_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_VOICE_MD1DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_MD1)));
#if IS_ENABLED(CONFIG_MTK_HDMI_TDM)
SND_SOC_DAILINK_DEFS(hdmi_out,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_HDMI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_HDMI_PCM)));
#endif
SND_SOC_DAILINK_DEFS(uldloopback,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_ULDLLOOPBACK_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_ULDLLOOPBACK_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_ULDLLOOPBACK_PCM)));
SND_SOC_DAILINK_DEFS(i2s0output,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_I2S0_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_I2S0_PCM)));
SND_SOC_DAILINK_DEFS(mrgrx,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_MRGRX_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_MRGRX_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_MRGRX_PCM)));
SND_SOC_DAILINK_DEFS(mrgrxcapture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_MRGRX_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_MRGRX_AWB_PCM)));
SND_SOC_DAILINK_DEFS(i2s0dl1output,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_I2S0DL1_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_I2S0TXDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_I2S0DL1_PCM)));
SND_SOC_DAILINK_DEFS(deep_buffer_dl_output,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_DEEPBUFFER_TX_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_DEEP_BUFFER_DL_PCM)));
SND_SOC_DAILINK_DEFS(dl1awbcapture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_DL1AWB_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_DL1AWBDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_DL1_AWB_PCM)));
SND_SOC_DAILINK_DEFS(voice_md1_bt,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOICE_MD1_BT_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_MD1_BT)));
SND_SOC_DAILINK_DEFS(voip_call_bt_playback,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOIP_CALL_BT_OUT_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOIP_BT_OUT)));
SND_SOC_DAILINK_DEFS(voip_call_bt_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOIP_CALL_BT_IN_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_VOIPCALLBTINDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOIP_BT_IN)));
SND_SOC_DAILINK_DEFS(tdm_debug_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_TDMRX_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_TDMRX_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_TDMRX_PCM)));
SND_SOC_DAILINK_DEFS(fm_mrg_tx,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_FM_MRGTX_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_FM_MRGTX_PCM)));
SND_SOC_DAILINK_DEFS(multimedia3,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_UL2DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_RXDAI2_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_UL2_PCM)));
SND_SOC_DAILINK_DEFS(i2s0_awb_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_I2S0AWBDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_I2S0AWB_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_I2S0_AWB_PCM)));
SND_SOC_DAILINK_DEFS(voice_md2,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOICE_MD2_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_VOICE_MD2DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_MD2)));
SND_SOC_DAILINK_DEFS(platofrm_control,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_ROUTING_PCM)));
SND_SOC_DAILINK_DEFS(voice_md2_bt,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_VOICE_MD2_BT_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_MD2_BT)));
SND_SOC_DAILINK_DEFS(hp_impedance,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_HP_IMPEDANCE_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_HP_IMPEDANCE_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_HP_IMPEDANCE_PCM)));
SND_SOC_DAILINK_DEFS(fm_i2s_rx_playback,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_FM_I2S_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_FM_I2S_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_FM_I2S_PCM)));
SND_SOC_DAILINK_DEFS(fm_i2s_rx_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_FM_I2S_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_FM_I2S_AWB_PCM)));
SND_SOC_DAILINK_DEFS(multimedia_dl2,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_DL2DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_TXDAI2_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_DL2_PCM)));
SND_SOC_DAILINK_DEFS(multimedia_dl3,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_OFFLOAD_NAME)),
	DAILINK_COMP_ARRAY(COMP_DUMMY()));
#ifdef _NON_COMMON_FEATURE_READY
SND_SOC_DAILINK_DEFS(mod_dai_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_MOD_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_MOD_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_MOD_DAI_PCM)));
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
SND_SOC_DAILINK_DEFS(offload,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_OFFLOAD_PLAYBACK_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_OFFLOAD_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_PLAYBACK_OFFLOAD)));
#endif
#ifdef _NON_COMMON_FEATURE_READY
SND_SOC_DAILINK_DEFS(pcm_anc,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_ANC_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_ANC_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_ANC_PCM)));
#endif
SND_SOC_DAILINK_DEFS(anc_record,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_ANC_RECORD_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_DUMMY_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_I2S2_ADC2_PCM)));
#ifdef _NON_COMMON_FEATURE_READY
SND_SOC_DAILINK_DEFS(voice_ultrasound,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_VOICE_ULTRADAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_ULTRA)));
#endif
SND_SOC_DAILINK_DEFS(voice_usb,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_VOICE_USBDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_USB)));
SND_SOC_DAILINK_DEFS(voice_usb_echoref,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_VOICE_USB_ECHOREF_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_VOICE_USB_ECHOREF)));
#if IS_ENABLED(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)
SND_SOC_DAILINK_DEFS(dl1scpspkoutput,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_DL1SCPSPK_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_NAME,
				      MT_SOC_CODEC_SPKSCPTXDAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_DL1SCPSPK_PCM)));
SND_SOC_DAILINK_DEFS(voice_scp,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_SCPVOICE_NAME)),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_SCP_VOICE_PCM)));
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
SND_SOC_DAILINK_DEFS(btcvsd,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_BTCVSD_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_CODEC(MT_SOC_CODEC_DUMMY_NAME,
				      MT_SOC_CODEC_BTCVSD_DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif
SND_SOC_DAILINK_DEFS(ext_headphone_multimedia,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()));
SND_SOC_DAILINK_DEFS(ext_speaker_multimedia,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()));
SND_SOC_DAILINK_DEFS(i2s1_awb_capture,
	DAILINK_COMP_ARRAY(COMP_CPU(MT_SOC_I2S2ADC2DAI_NAME)),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(MT_SOC_I2S2_ADC2_PCM)));

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt_soc_dai_common[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MultiMedia1",
		.stream_name = MT_SOC_DL1_STREAM_NAME,
		SND_SOC_DAILINK_REG(multimedia1),
	},
	{
		.name = "MultiMedia2",
		.stream_name = MT_SOC_UL1_STREAM_NAME,
		SND_SOC_DAILINK_REG(multimedia2),
	},
	{
		.name = "Voice_MD1",
		.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_md1),
	},
#if IS_ENABLED(CONFIG_MTK_HDMI_TDM)
	{
		.name = "HDMI_OUT",
		.stream_name = MT_SOC_HDMI_STREAM_NAME,
		SND_SOC_DAILINK_REG(hdmi_out),
	},
#endif
	{
		.name = "ULDLOOPBACK",
		.stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		SND_SOC_DAILINK_REG(uldloopback),
	},
	{
		.name = "I2S0OUTPUT",
		.stream_name = MT_SOC_I2S0_STREAM_NAME,
		SND_SOC_DAILINK_REG(i2s0output),
	},
	{
		.name = "MRGRX",
		.stream_name = MT_SOC_MRGRX_STREAM_NAME,
		SND_SOC_DAILINK_REG(mrgrx),
	},
	{
		.name = "MRGRXCAPTURE",
		.stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
		SND_SOC_DAILINK_REG(mrgrxcapture),
	},
	{
		.name = "I2S0DL1OUTPUT",
		.stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		SND_SOC_DAILINK_REG(i2s0dl1output),
	},
	{
		.name = "DEEP_BUFFER_DL_OUTPUT",
		.stream_name = MT_SOC_DEEP_BUFFER_DL_STREAM_NAME,
		SND_SOC_DAILINK_REG(deep_buffer_dl_output),
	},
	{
		.name = "DL1AWBCAPTURE",
		.stream_name = MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
		SND_SOC_DAILINK_REG(dl1awbcapture),
	},
	{
		.name = "Voice_MD1_BT",
		.stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_md1_bt),
	},
	{
		.name = "VOIP_CALL_BT_PLAYBACK",
		.stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
		SND_SOC_DAILINK_REG(voip_call_bt_playback),
	},
	{
		.name = "VOIP_CALL_BT_CAPTURE",
		.stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
		SND_SOC_DAILINK_REG(voip_call_bt_capture),
	},
	{
		.name = "TDM_Debug_CAPTURE",
		.stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		SND_SOC_DAILINK_REG(tdm_debug_capture),
	},
	{
		.name = "FM_MRG_TX",
		.stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
		SND_SOC_DAILINK_REG(fm_mrg_tx),
	},
	{
		.name = "MultiMedia3",
		.stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
		SND_SOC_DAILINK_REG(multimedia3),
	},
	{
		.name = "I2S0_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
		SND_SOC_DAILINK_REG(i2s0_awb_capture),
	},
	{
		.name = "Voice_MD2",
		.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_md2),
	},
	{
		.name = "PLATOFRM_CONTROL",
		.stream_name = MT_SOC_ROUTING_STREAM_NAME,
		SND_SOC_DAILINK_REG(platofrm_control),
	},
	{
		.name = "Voice_MD2_BT",
		.stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_md2_bt),
	},
	{
		.name = "HP_IMPEDANCE",
		.stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
		SND_SOC_DAILINK_REG(hp_impedance),
	},
	{
		.name = "FM_I2S_RX_Playback",
		.stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		SND_SOC_DAILINK_REG(fm_i2s_rx_playback),
	},
	{
		.name = "FM_I2S_RX_Capture",
		.stream_name = MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
		SND_SOC_DAILINK_REG(fm_i2s_rx_capture),
	},
	{
		.name = "MultiMedia_DL2",
		.stream_name = MT_SOC_DL2_STREAM_NAME,
		SND_SOC_DAILINK_REG(multimedia_dl2),
	},
	{
		.name = "MultiMedia_DL3",
		.stream_name = MT_SOC_DL3_STREAM_NAME,
		SND_SOC_DAILINK_REG(multimedia_dl3),
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "MOD_DAI_CAPTURE",
		.stream_name = MT_SOC_MODDAI_STREAM_NAME,
		SND_SOC_DAILINK_REG(mod_dai_capture),
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	{
		.name = "OFFLOAD",
		.stream_name = MT_SOC_OFFLOAD_STREAM_NAME,
		SND_SOC_DAILINK_REG(offload),
	},
#endif
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "PCM_ANC",
		.stream_name = MT_SOC_ANC_STREAM_NAME,
		SND_SOC_DAILINK_REG(pcm_anc),
	},
#endif
	{
		.name = "ANC_RECORD",
		.stream_name = MT_SOC_ANC_RECORD_STREAM_NAME,
		SND_SOC_DAILINK_REG(anc_record),
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "Voice_Ultrasound",
		.stream_name = MT_SOC_VOICE_ULTRA_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_ultrasound),
	},
#endif
	{
		.name = "Voice_USB",
		.stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_usb),
	},
	{
		.name = "Voice_USB_ECHOREF",
		.stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
		.playback_only = true,
		SND_SOC_DAILINK_REG(voice_usb_echoref),
	},
#if IS_ENABLED(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)
	{
		.name = "DL1SCPSPKOUTPUT",
		.stream_name = MT_SOC_DL1SCPSPK_STREAM_NAME,
		SND_SOC_DAILINK_REG(dl1scpspkoutput),
	},
	{
		.name = "VOICE_SCP",
		.stream_name = MT_SOC_SCPVOICE_STREAM_NAME,
		SND_SOC_DAILINK_REG(voice_scp),
	},
#endif
};

#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
static struct snd_soc_dai_link mt_soc_btcvsd_dai[] = {
	{
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		SND_SOC_DAILINK_REG(btcvsd),
	},
};
#endif

static struct snd_soc_dai_link mt_soc_exthp_dai[] = {
	{
		.name = "ext_Headphone_Multimedia",
		.stream_name = MT_SOC_HEADPHONE_STREAM_NAME,
		SND_SOC_DAILINK_REG(ext_headphone_multimedia),
	},
};

static struct snd_soc_dai_link mt_soc_extspk_dai[] = {
	{
		.name = "ext_Speaker_Multimedia",
		.stream_name = MT_SOC_SPEAKER_STREAM_NAME,
		SND_SOC_DAILINK_REG(ext_speaker_multimedia),
	},
	{
		.name = "I2S1_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S2ADC2_STREAM_NAME,
		.ops = &mt_machine_audio_ops,
		SND_SOC_DAILINK_REG(i2s1_awb_capture),
	},
};

static struct snd_soc_dai_link
	mt_soc_dai_component[ARRAY_SIZE(mt_soc_dai_common) +
#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
			     ARRAY_SIZE(mt_soc_btcvsd_dai) +
#endif
			     ARRAY_SIZE(mt_soc_exthp_dai) +
			     ARRAY_SIZE(mt_soc_extspk_dai)];

static struct snd_soc_card mt_snd_soc_card_mt = {
	.name = "mt-snd-card",
	.dai_link = mt_soc_dai_common,
	.num_links = ARRAY_SIZE(mt_soc_dai_common),
};

static int mt_soc_snd_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt_snd_soc_card_mt;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
	struct device_node *btcvsd_node;
#endif
	int ret;
	int daiLinkNum = 0;

	ret = mtk_spk_update_dai_link(mt_soc_extspk_dai, pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s(), mtk_spk_update_dai_link error\n",
			__func__);
		return -EINVAL;
	}

	/* get_ext_dai_codec_name(); */
	pr_debug("%s(), dai_link = %p\n",
		 __func__, mt_snd_soc_card_mt.dai_link);

	/* DEAL WITH DAI LINK */
	memcpy(mt_soc_dai_component, mt_soc_dai_common,
	       sizeof(mt_soc_dai_common));
	daiLinkNum += ARRAY_SIZE(mt_soc_dai_common);

#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
	/* assign btcvsd platform_node */
	btcvsd_node = of_parse_phandle(pdev->dev.of_node,
				       "mediatek,btcvsd_snd", 0);
	if (!btcvsd_node) {
		dev_err(&pdev->dev, "Property 'btcvsd_snd' missing or invalid\n");
		return -EINVAL;
	}
	mt_soc_btcvsd_dai[0].platforms->of_node = btcvsd_node;

	memcpy(mt_soc_dai_component + daiLinkNum,
	mt_soc_btcvsd_dai, sizeof(mt_soc_btcvsd_dai));
	daiLinkNum += ARRAY_SIZE(mt_soc_btcvsd_dai);
#endif

	memcpy(mt_soc_dai_component + daiLinkNum, mt_soc_exthp_dai,
	       sizeof(mt_soc_exthp_dai));
	daiLinkNum += ARRAY_SIZE(mt_soc_exthp_dai);

	memcpy(mt_soc_dai_component + daiLinkNum, mt_soc_extspk_dai,
	       sizeof(mt_soc_extspk_dai));
	daiLinkNum += ARRAY_SIZE(mt_soc_extspk_dai);

	mt_snd_soc_card_mt.dai_link = mt_soc_dai_component;
	mt_snd_soc_card_mt.num_links = daiLinkNum;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	else
		dev_err(&pdev->dev, "%s snd_soc_register_card %s pass %d\n",
			__func__, card->name, ret);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* create debug file */
	mt_sco_audio_debugfs =
		debugfs_create_file(DEBUG_FS_NAME, S_IFREG | 0444, NULL,
				    (void *)DEBUG_FS_NAME, &mtaudio_debug_ops);

	/* create analog debug file */
	mt_sco_audio_debugfs = debugfs_create_file(
		DEBUG_ANA_FS_NAME, S_IFREG | 0444, NULL,
		(void *)DEBUG_ANA_FS_NAME, &mtaudio_ana_debug_ops);
#endif
	return ret;
}

static int mt_soc_snd_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt_audio_driver_dt_match[] = {
	{
		.compatible = "mediatek,audio",
	},
	{} };
#endif

static struct platform_driver mt_audio_driver = {
	.driver = {

			.name = "mtk-audio",
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = mt_audio_driver_dt_match,
#endif
		},
	.probe = mt_soc_snd_probe,
	.remove = mt_soc_snd_remove,
};

#ifndef CONFIG_OF
static struct platform_device *mtk_soc_snd_dev;
#endif

static int __init mt_soc_snd_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	mtk_soc_snd_dev = platform_device_alloc("mtk-audio", -1);
	if (!mtk_soc_snd_dev)
		return -ENOMEM;

	ret = platform_device_add(mtk_soc_snd_dev);
	if (ret != 0) {
		platform_device_put(mtk_soc_snd_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mt_audio_driver);
	pr_debug("-%s\n", __func__);

	return ret;
}
module_init(mt_soc_snd_init);

static void __exit mt_soc_snd_exit(void)
{
	platform_driver_unregister(&mt_audio_driver);
}
module_exit(mt_soc_snd_exit);

/* Module information */
MODULE_AUTHOR("ChiPeng <chipeng.chang@mediatek.com>");
MODULE_DESCRIPTION("ALSA SoC driver ");
MODULE_LICENSE("GPL");
