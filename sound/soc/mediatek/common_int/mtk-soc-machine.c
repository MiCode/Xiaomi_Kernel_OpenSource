/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "mtk-soc-speaker-amp.h"

#include "mtk-hw-component.h"
#if defined(CONFIG_SND_SOC_CS43130)
#include "mtk-cs43130-machine-ops.h"
#endif
#if defined(CONFIG_SND_SOC_CS35L35)
#include "mtk-cs35l35-machine-ops.h"
#endif

static struct dentry *mt_sco_audio_debugfs;
#define DEBUG_FS_NAME "mtksocaudio"
#define DEBUG_ANA_FS_NAME "mtksocanaaudio"

static int mt_soc_ana_debug_open(struct inode *inode, struct file *file)
{
	pr_debug("mt_soc_ana_debug_open\n");
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

	pr_debug("mt_soc_ana_debug_read len = %d\n", n);

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
	pr_debug("mt_soc_debug_read len = %d\n", n);

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
		pr_debug("%s(), copy_from_user fail, mt_soc_debug_write count = %zu\n",
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
		"copy_from_user mt_soc_debug_write count = %zu, temp = %s, pointer = %p\n",
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

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt_soc_dai_common[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MultiMedia1",
		.stream_name = MT_SOC_DL1_STREAM_NAME,
		.cpu_dai_name = MT_SOC_DL1DAI_NAME,
		.platform_name = MT_SOC_DL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_TXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "MultiMedia2",
		.stream_name = MT_SOC_UL1_STREAM_NAME,
		.cpu_dai_name = MT_SOC_UL1DAI_NAME,
		.platform_name = MT_SOC_UL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_RXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "Voice_MD1",
		.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOICE_MD1_NAME,
		.platform_name = MT_SOC_VOICE_MD1,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#ifdef CONFIG_MTK_HDMI_TDM
	{
		.name = "HDMI_OUT",
		.stream_name = MT_SOC_HDMI_STREAM_NAME,
		.cpu_dai_name = MT_SOC_HDMI_NAME,
		.platform_name = MT_SOC_HDMI_PCM,
		.codec_dai_name = MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
#endif
	{
		.name = "ULDLOOPBACK",
		.stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		.cpu_dai_name = MT_SOC_ULDLLOOPBACK_NAME,
		.platform_name = MT_SOC_ULDLLOOPBACK_PCM,
		.codec_dai_name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "I2S0OUTPUT",
		.stream_name = MT_SOC_I2S0_STREAM_NAME,
		.cpu_dai_name = MT_SOC_I2S0_NAME,
		.platform_name = MT_SOC_I2S0_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MRGRX",
		.stream_name = MT_SOC_MRGRX_STREAM_NAME,
		.cpu_dai_name = MT_SOC_MRGRX_NAME,
		.platform_name = MT_SOC_MRGRX_PCM,
		.codec_dai_name = MT_SOC_CODEC_MRGRX_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "MRGRXCAPTURE",
		.stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
		.cpu_dai_name = MT_SOC_MRGRX_NAME,
		.platform_name = MT_SOC_MRGRX_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "I2S0DL1OUTPUT",
		.stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		.cpu_dai_name = MT_SOC_I2S0DL1_NAME,
		.platform_name = MT_SOC_I2S0DL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0TXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "DEEP_BUFFER_DL_OUTPUT",
		.stream_name = MT_SOC_DEEP_BUFFER_DL_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_DEEP_BUFFER_DL_PCM,
		.codec_dai_name = MT_SOC_CODEC_DEEPBUFFER_TX_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "DL1AWBCAPTURE",
		.stream_name = MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
		.cpu_dai_name = MT_SOC_DL1AWB_NAME,
		.platform_name = MT_SOC_DL1_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_DL1AWBDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD1_BT",
		.stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOICE_MD1_BT_NAME,
		.platform_name = MT_SOC_VOICE_MD1_BT,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "VOIP_CALL_BT_PLAYBACK",
		.stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOIP_CALL_BT_OUT_NAME,
		.platform_name = MT_SOC_VOIP_BT_OUT,
		.codec_dai_name = MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "VOIP_CALL_BT_CAPTURE",
		.stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOIP_CALL_BT_IN_NAME,
		.platform_name = MT_SOC_VOIP_BT_IN,
		.codec_dai_name = MT_SOC_CODEC_VOIPCALLBTINDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "TDM_Debug_CAPTURE",
		.stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		.cpu_dai_name = MT_SOC_TDMRX_NAME,
		.platform_name = MT_SOC_TDMRX_PCM,
		.codec_dai_name = MT_SOC_CODEC_TDMRX_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "FM_MRG_TX",
		.stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
		.cpu_dai_name = MT_SOC_FM_MRGTX_NAME,
		.platform_name = MT_SOC_FM_MRGTX_PCM,
		.codec_dai_name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MultiMedia3",
		.stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
		.cpu_dai_name = MT_SOC_UL2DAI_NAME,
		.platform_name = MT_SOC_UL2_PCM,
		.codec_dai_name = MT_SOC_CODEC_RXDAI2_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "I2S0_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
		.cpu_dai_name = MT_SOC_I2S0AWBDAI_NAME,
		.platform_name = MT_SOC_I2S0_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0AWB_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD2",
		.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOICE_MD2_NAME,
		.platform_name = MT_SOC_VOICE_MD2,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "PLATOFRM_CONTROL",
		.stream_name = MT_SOC_ROUTING_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_ROUTING_PCM,
		.codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD2_BT",
		.stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
		.cpu_dai_name = MT_SOC_VOICE_MD2_BT_NAME,
		.platform_name = MT_SOC_VOICE_MD2_BT,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "HP_IMPEDANCE",
		.stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
		.cpu_dai_name = MT_SOC_HP_IMPEDANCE_NAME,
		.platform_name = MT_SOC_HP_IMPEDANCE_PCM,
		.codec_dai_name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "FM_I2S_RX_Playback",
		.stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		.cpu_dai_name = MT_SOC_FM_I2S_NAME,
		.platform_name = MT_SOC_FM_I2S_PCM,
		.codec_dai_name = MT_SOC_CODEC_FM_I2S_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "FM_I2S_RX_Capture",
		.stream_name = MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
		.cpu_dai_name = MT_SOC_FM_I2S_NAME,
		.platform_name = MT_SOC_FM_I2S_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MultiMedia_DL2",
		.stream_name = MT_SOC_DL2_STREAM_NAME,
		.cpu_dai_name = MT_SOC_DL2DAI_NAME,
		.platform_name = MT_SOC_DL2_PCM,
		.codec_dai_name = MT_SOC_CODEC_TXDAI2_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "MultiMedia_DL3",
		.stream_name = MT_SOC_DL3_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = MT_SOC_CODEC_OFFLOAD_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
	{
		.name = "BTCVSD_RX",
		.stream_name = MT_SOC_BTCVSD_CAPTURE_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_BTCVSD_RX_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "BTCVSD_TX",
		.stream_name = MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_BTCVSD_TX_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#endif
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "MOD_DAI_CAPTURE",
		.stream_name = MT_SOC_MODDAI_STREAM_NAME,
		.cpu_dai_name = MT_SOC_MOD_DAI_NAME,
		.platform_name = MT_SOC_MOD_DAI_PCM,
		.codec_dai_name = MT_SOC_CODEC_MOD_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
#endif
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	{
		.name = "OFFLOAD",
		.stream_name = MT_SOC_OFFLOAD_STREAM_NAME,
		.cpu_dai_name = MT_SOC_OFFLOAD_PLAYBACK_DAI_NAME,
		.platform_name = MT_SOC_PLAYBACK_OFFLOAD,
		.codec_dai_name = MT_SOC_CODEC_OFFLOAD_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "PCM_ANC",
		.stream_name = MT_SOC_ANC_STREAM_NAME,
		.cpu_dai_name = MT_SOC_ANC_NAME,
		.platform_name = MT_SOC_ANC_PCM,
		.codec_dai_name = MT_SOC_CODEC_ANC_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
	{
		.name = "ANC_RECORD",
		.stream_name = MT_SOC_ANC_RECORD_STREAM_NAME,
		.cpu_dai_name = MT_SOC_ANC_RECORD_DAI_NAME,
		.platform_name = MT_SOC_I2S2_ADC2_PCM,
		.codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
		.ops = &mt_machine_audio_ops,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "Voice_Ultrasound",
		.stream_name = MT_SOC_VOICE_ULTRA_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_VOICE_ULTRA,
		.codec_dai_name = MT_SOC_CODEC_VOICE_ULTRADAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
	{
		.name = "Voice_USB",
		.stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_VOICE_USB,
		.codec_dai_name = MT_SOC_CODEC_VOICE_USBDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "Voice_USB_ECHOREF",
		.stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = MT_SOC_VOICE_USB_ECHOREF,
		.codec_dai_name = MT_SOC_CODEC_VOICE_USB_ECHOREF_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
		.playback_only = true,
	},
#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	{
		.name = "DL1SCPSPKOUTPUT",
		.stream_name = MT_SOC_DL1SCPSPK_STREAM_NAME,
		.cpu_dai_name = MT_SOC_DL1SCPSPK_NAME,
		.platform_name = MT_SOC_DL1SCPSPK_PCM,
		.codec_dai_name = MT_SOC_CODEC_SPKSCPTXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "VOICE_SCP",
		.stream_name = MT_SOC_SCPVOICE_STREAM_NAME,
		.cpu_dai_name = MT_SOC_SCPVOICE_NAME,
		.platform_name = MT_SOC_SCP_VOICE_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#endif
};

static struct snd_soc_dai_link mt_soc_exthp_dai[] = {
	{
		.name = "ext_Headphone_Multimedia",
		.stream_name = MT_SOC_HEADPHONE_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
#ifdef CONFIG_SND_SOC_CS43130
		.codec_dai_name = "cs43130-hifi",
		.codec_name = "cs43130.2-0030",
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS |
			   SND_SOC_DAIFMT_NB_NF,
		.ops = &cs43130_ops,
#else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
	},
};

static struct snd_soc_dai_link mt_soc_extspk_dai[] = {
	{
		.name = "ext_Speaker_Multimedia",
		.stream_name = MT_SOC_SPEAKER_STREAM_NAME,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
#ifdef CONFIG_SND_SOC_MAX98926
		.codec_dai_name = "max98926-aif1",
		.codec_name = "MAX98926_MT",
#elif defined(CONFIG_SND_SOC_CS35L35)
		.codec_dai_name = "cs35l35-pcm",
		.codec_name = "cs35l35.2-0040",
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS |
			   SND_SOC_DAIFMT_NB_NF,
		.ops = &cs35l35_ops,
#else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
	},
	{
		.name = "I2S1_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S2ADC2_STREAM_NAME,
		.cpu_dai_name = MT_SOC_I2S2ADC2DAI_NAME,
		.platform_name = MT_SOC_I2S2_ADC2_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &mt_machine_audio_ops,
	},
};

static struct snd_soc_dai_link
	mt_soc_dai_component[ARRAY_SIZE(mt_soc_dai_common) +
			     ARRAY_SIZE(mt_soc_exthp_dai) +
			     ARRAY_SIZE(mt_soc_extspk_dai)];

static struct snd_soc_card mt_snd_soc_card_mt = {
	.name = "mt-snd-card",
	.dai_link = mt_soc_dai_common,
	.num_links = ARRAY_SIZE(mt_soc_dai_common),
};

static void get_ext_dai_codec_name(void)
{
	get_extspk_dai_codec_name(mt_soc_extspk_dai);
	get_exthp_dai_codec_name(mt_soc_exthp_dai);
}

static int mt_soc_snd_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt_snd_soc_card_mt;
	int ret;
	int daiLinkNum = 0;

	ret = mtk_spk_update_dai_link(mt_soc_extspk_dai, pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s(), mtk_spk_update_dai_link error\n",
			__func__);
		return -EINVAL;
	}

	get_ext_dai_codec_name();
	pr_debug("mt_soc_snd_probe dai_link = %p\n",
		mt_snd_soc_card_mt.dai_link);

	/* DEAL WITH DAI LINK */
	memcpy(mt_soc_dai_component, mt_soc_dai_common,
	       sizeof(mt_soc_dai_common));
	daiLinkNum += ARRAY_SIZE(mt_soc_dai_common);
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

	/* create debug file */
	mt_sco_audio_debugfs =
		debugfs_create_file(DEBUG_FS_NAME, S_IFREG | 0444, NULL,
				    (void *)DEBUG_FS_NAME, &mtaudio_debug_ops);

	/* create analog debug file */
	mt_sco_audio_debugfs = debugfs_create_file(
		DEBUG_ANA_FS_NAME, S_IFREG | 0444, NULL,
		(void *)DEBUG_ANA_FS_NAME, &mtaudio_ana_debug_ops);

	return ret;
}

static int mt_soc_snd_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
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
#ifdef CONFIG_OF
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
