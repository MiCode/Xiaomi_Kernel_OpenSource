/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
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
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"

/* #include <mach/mt_clkbuf_ctl.h> */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <stdarg.h>
#include <linux/module.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
/* #include <asm/mach-types.h> */
#include <linux/debugfs.h>
#include "mt_soc_codec_63xx.h"

static int mt_soc_lowjitter_control;
static int mt_soc_dmic_control;
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
	const int size = 4096;
	/* char buffer[size]; */
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		kfree(buffer);
		return -ENOMEM;
	}

	pr_debug("mt_soc_ana_debug_read count = %zu\n", count);
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
	pr_debug("mt_soc_debug_open\n");
	return 0;
}

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	const int size = 6144;
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
static char const ParSetkeyCfg[] = "Setcfgreg";
static char const PareGetkeyAfe[] = "Getafereg";
static char const PareGetkeyAna[] = "Getanareg";
/* static char ParGetkeyCfg[] = "Getcfgreg"; */
/* static char ParSetAddr[] = "regaddr"; */
/* static char ParSetValue[] = "regvalue"; */

static ssize_t mt_soc_debug_write(struct file *f, const char __user *buf,
				  size_t count, loff_t *offset)
{
	int ret = 0;
	char InputString[256];
	char *token1 = NULL;
	char *token2 = NULL;
	char *token3 = NULL;
	char *token4 = NULL;
	char *token5 = NULL;
	char *temp = NULL;

	unsigned int long regaddr = 0;
	unsigned int long regvalue = 0;
	char delim[] = " ,";

	memset_io((void *)InputString, 0, 256);

	if (count > 256)
		count = 256;

	if (copy_from_user((InputString), buf, count))
		pr_warn("copy_from_user mt_soc_debug_write count = %zu temp = %s\n", count, InputString);

	temp = kstrdup(InputString, GFP_KERNEL);
	pr_debug("copy_from_user mt_soc_debug_write count = %zu temp = %s pointer = %p\n",
		count, InputString, InputString);
	token1 = strsep(&temp, delim);
	pr_debug("token1\n");
	pr_debug("token1 = %s\n", token1);
	token2 = strsep(&temp, delim);
	pr_debug("token2 = %s\n", token2);
	token3 = strsep(&temp, delim);
	pr_debug("token3 = %s\n", token3);
	token4 = strsep(&temp, delim);
	pr_debug("token4 = %s\n", token4);
	token5 = strsep(&temp, delim);
	pr_debug("token5 = %s\n", token5);

	if (strcmp(token1, ParSetkeyAfe) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAfe)\n");
		ret = kstrtoul(token3, 16, &regaddr);
		ret = kstrtoul(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAfe, regaddr,
			regvalue);
		Afe_Set_Reg(regaddr,  regvalue, 0xffffffff);
		regvalue = Afe_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAfe, regaddr,
			regvalue);
	}
	if (strcmp(token1, ParSetkeyAna) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAna)\n");
		ret = kstrtoul(token3, 16, &regaddr);
		ret =  kstrtoul(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAna, regaddr,
			regvalue);
		audckbufEnable(true);
		Ana_Set_Reg(regaddr,  regvalue, 0xffffffff);
		regvalue = Ana_Get_Reg(regaddr);
		audckbufEnable(false);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAna, regaddr,
			regvalue);
	}
	if (strcmp(token1, ParSetkeyCfg) == 0) {
		pr_debug("strcmp (token1,ParSetkeyCfg)\n");
		ret =  kstrtoul(token3, 16, &regaddr);
		ret =  kstrtoul(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyCfg, regaddr,
			regvalue);
		SetClkCfg(regaddr,  regvalue, 0xffffffff);
		regvalue = GetClkCfg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyCfg, regaddr,
			regvalue);
	}
	if (strcmp(token1, PareGetkeyAfe) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAfe)\n");
		ret =  kstrtoul(token3, 16, &regaddr);
		regvalue = Afe_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", PareGetkeyAfe, regaddr,
			regvalue);
		}
	if (strcmp(token1, PareGetkeyAna) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAna)\n");
		ret =  kstrtoul(token3, 16, &regaddr);
		regvalue = Ana_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", PareGetkeyAna, regaddr,
			regvalue);
	}
	return count;
}

static const struct file_operations mtaudio_debug_ops = {
	.open = mt_soc_debug_open,
	.read = mt_soc_debug_read,
	.write = mt_soc_debug_write,
};


static const struct file_operations mtaudio_ana_debug_ops = {
	.open = mt_soc_ana_debug_open,
	.read = mt_soc_ana_debug_read,
};

/* snd_soc_ops */
static int mt_machine_trigger(struct snd_pcm_substream *substream,
				     int cmd)
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
		.cpu_dai_name   = MT_SOC_DL1DAI_NAME,
		.platform_name  = MT_SOC_DL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_TXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "MultiMedia2",
		.stream_name = MT_SOC_UL1_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_UL1DAI_NAME,
		.platform_name  = MT_SOC_UL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_RXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "Voice_MD1",
		.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOICE_MD1_NAME,
		.platform_name  = MT_SOC_VOICE_MD1,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "HDMI_OUT",
		.stream_name = MT_SOC_HDMI_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_HDMI_NAME,
		.platform_name  = MT_SOC_HDMI_PCM,
		.codec_dai_name = MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
#endif
	{
		.name = "ULDLOOPBACK",
		.stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_ULDLLOOPBACK_NAME,
		.platform_name  = MT_SOC_ULDLLOOPBACK_PCM,
		.codec_dai_name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "I2S0OUTPUT",
		.stream_name = MT_SOC_I2S0_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_I2S0_NAME,
		.platform_name  = MT_SOC_I2S0_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MRGRX",
		.stream_name = MT_SOC_MRGRX_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_MRGRX_NAME,
		.platform_name  = MT_SOC_MRGRX_PCM,
		.codec_dai_name = MT_SOC_CODEC_MRGRX_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "MRGRXCAPTURE",
		.stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_MRGRX_NAME,
		.platform_name  = MT_SOC_MRGRX_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "I2S0DL1OUTPUT",
		.stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_I2S0DL1_NAME,
		.platform_name  = MT_SOC_I2S0DL1_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0TXDAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "DL1AWBCAPTURE",
		.stream_name = MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_DL1AWB_NAME,
		.platform_name  = MT_SOC_DL1_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_DL1AWBDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD1_BT",
		.stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOICE_MD1_BT_NAME,
		.platform_name  = MT_SOC_VOICE_MD1_BT,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "VOIP_CALL_BT_PLAYBACK",
		.stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOIP_CALL_BT_OUT_NAME,
		.platform_name  = MT_SOC_VOIP_BT_OUT,
		.codec_dai_name = MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "VOIP_CALL_BT_CAPTURE",
		.stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOIP_CALL_BT_IN_NAME,
		.platform_name  = MT_SOC_VOIP_BT_IN,
		.codec_dai_name = MT_SOC_CODEC_VOIPCALLBTINDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "TDM_Debug_CAPTURE",
		.stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_TDMRX_NAME,
		.platform_name  = MT_SOC_TDMRX_PCM,
		.codec_dai_name = MT_SOC_CODEC_TDMRX_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "FM_MRG_TX",
		.stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_FM_MRGTX_NAME,
		.platform_name  = MT_SOC_FM_MRGTX_PCM,
		.codec_dai_name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MultiMedia3",
		.stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_UL2DAI_NAME,
		.platform_name  = MT_SOC_UL2_PCM,
		.codec_dai_name = MT_SOC_CODEC_RXDAI2_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "I2S0_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_I2S0AWBDAI_NAME,
		.platform_name  = MT_SOC_I2S0_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_I2S0AWB_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD2",
		.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOICE_MD2_NAME,
		.platform_name  = MT_SOC_VOICE_MD2,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "PLATOFRM_CONTROL",
		.stream_name = MT_SOC_ROUTING_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_ROUTING_DAI_NAME,
		.platform_name  = MT_SOC_ROUTING_PCM,
		.codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "Voice_MD2_BT",
		.stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_VOICE_MD2_BT_NAME,
		.platform_name  = MT_SOC_VOICE_MD2_BT,
		.codec_dai_name = MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "HP_IMPEDANCE",
		.stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_HP_IMPEDANCE_NAME,
		.platform_name  = MT_SOC_HP_IMPEDANCE_PCM,
		.codec_dai_name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "FM_I2S_RX_Playback",
		.stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_FM_I2S_NAME,
		.platform_name  = MT_SOC_FM_I2S_PCM,
		.codec_dai_name = MT_SOC_CODEC_FM_I2S_DAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
	{
		.name = "FM_I2S_RX_Capture",
		.stream_name = MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_FM_I2S_NAME,
		.platform_name  = MT_SOC_FM_I2S_AWB_PCM,
		.codec_dai_name = MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
	{
		.name = "MultiMedia_DL2",
		.stream_name = MT_SOC_DL2_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_DL2DAI_NAME,
		.platform_name  = MT_SOC_DL2_PCM,
		.codec_dai_name = MT_SOC_CODEC_TXDAI2_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#ifdef CONFIG_MTK_BTCVSD_ALSA
	{
		.name = "BTCVSD_RX",
		.stream_name = MT_SOC_BTCVSD_CAPTURE_STREAM_NAME,
		.cpu_dai_name   = "snd-soc-dummy-dai",
		.platform_name  = MT_SOC_BTCVSD_RX_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		},
	{
		.name = "BTCVSD_TX",
		.stream_name = MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME,
		.cpu_dai_name   = "snd-soc-dummy-dai",
		.platform_name  = MT_SOC_BTCVSD_TX_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#endif
	{
		.name = "MOD_DAI_CAPTURE",
		.stream_name = MT_SOC_MODDAI_STREAM_NAME,
		.cpu_dai_name	= MT_SOC_MOD_DAI_NAME,
		.platform_name	= MT_SOC_MOD_DAI_PCM,
		.codec_dai_name = MT_SOC_CODEC_MOD_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "OFFLOAD",
		.stream_name = MT_SOC_OFFLOAD_STREAM_NAME,
		.cpu_dai_name	= "snd-soc-dummy-dai",
		.platform_name	= "snd-soc-dummy",
		.codec_dai_name = MT_SOC_CODEC_OFFLOAD_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "PCM_ANC",
		.stream_name = MT_SOC_ANC_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_ANC_NAME,
		.platform_name  = MT_SOC_ANC_PCM,
		.codec_dai_name = MT_SOC_CODEC_ANC_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
	{
		.name = "ANC_RECORD",
		.stream_name = MT_SOC_ANC_RECORD_STREAM_NAME,
		.cpu_dai_name	= MT_SOC_ANC_RECORD_DAI_NAME,
		.platform_name	= MT_SOC_I2S2_ADC2_PCM,
		.codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
		.codec_name = MT_SOC_CODEC_DUMMY_NAME,
		.ops = &mt_machine_audio_ops,
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = "Voice_Ultrasound",
		.stream_name = MT_SOC_VOICE_ULTRA_STREAM_NAME,
		.cpu_dai_name	= "snd-soc-dummy-dai",
		.platform_name	= MT_SOC_VOICE_ULTRA,
		.codec_dai_name = MT_SOC_CODEC_VOICE_ULTRADAI_NAME,
		.codec_name = MT_SOC_CODEC_NAME,
	},
#endif
};

static struct snd_soc_dai_link mt_soc_extspk_dai[] = {
	{
		.name = "ext_Speaker_Multimedia",
		.stream_name = MT_SOC_SPEAKER_STREAM_NAME,
		.cpu_dai_name   = "snd-soc-dummy-dai",
		.platform_name  = "snd-soc-dummy",
#ifdef CONFIG_SND_SOC_MAX98926
		.codec_dai_name = "max98926-aif1",
		.codec_name = "MAX98926_MT",
#elif defined(CONFIG_SND_SOC_RT5509)
		.codec_dai_name = "rt5509-aif1",
		.codec_name = "RT5509_MT",
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
#else
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
#endif
	},
	{
		.name = "I2S1_AWB_CAPTURE",
		.stream_name = MT_SOC_I2S2ADC2_STREAM_NAME,
		.cpu_dai_name   = MT_SOC_I2S2ADC2DAI_NAME,
		.platform_name  = MT_SOC_I2S2_ADC2_PCM,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &mt_machine_audio_ops,
	},
};

static struct snd_soc_dai_link mt_soc_dai_component[
	ARRAY_SIZE(mt_soc_dai_common) +
	ARRAY_SIZE(mt_soc_extspk_dai)];

static const char const *I2S_low_jittermode[] = {"Off", "On"};
static const char const *on_off[] = {"Off", "On"};


static const struct soc_enum mt_soc_machine_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(I2S_low_jittermode), I2S_low_jittermode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(on_off), on_off),
};


static int mt6595_get_lowjitter(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s:  mt_soc_lowjitter_control = %d\n", __func__, mt_soc_lowjitter_control);
	ucontrol->value.integer.value[0] = mt_soc_lowjitter_control;
	return 0;
}

static int mt6595_set_lowjitter(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);

	mt_soc_lowjitter_control = ucontrol->value.integer.value[0];
	return 0;
}

static int mt_get_dmic_path(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mt_soc_dmic_control;
	return 0;
}

static int mt_set_dmic_path(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);

	mt_soc_dmic_control = ucontrol->value.integer.value[0];
	setDmicPath(mt_soc_dmic_control);
	return 0;
}


static const struct snd_kcontrol_new mt_soc_controls[] = {
	SOC_ENUM_EXT("I2S low Jitter function", mt_soc_machine_enum[0],
		     mt6595_get_lowjitter, mt6595_set_lowjitter),
	SOC_ENUM_EXT("dmic path", mt_soc_machine_enum[1],
		     mt_get_dmic_path, mt_set_dmic_path),
};

static struct snd_soc_card snd_soc_card_mt = {
	.name       = "mt-snd-card",
	.dai_link   = mt_soc_dai_common,
	.num_links  = ARRAY_SIZE(mt_soc_dai_common),
	.controls = mt_soc_controls,
	.num_controls = ARRAY_SIZE(mt_soc_controls),
};

static struct platform_device *mt_snd_device;

static int __init mt_soc_snd_init(void)
{
	int ret = 0;
	int daiLinkNum = 0;
	struct device_node *node = NULL;

	pr_debug("mt_soc_snd_init dai_link = %p\n", snd_soc_card_mt.dai_link);
	/* DEAL WITH DAI LINK */
	memcpy(mt_soc_dai_component, mt_soc_dai_common, sizeof(mt_soc_dai_common));

	daiLinkNum += ARRAY_SIZE(mt_soc_dai_common);
	node = of_find_compatible_node(NULL, NULL, "maxim,max98926L");

	memcpy(mt_soc_dai_component + ARRAY_SIZE(mt_soc_dai_common),
	mt_soc_extspk_dai, sizeof(mt_soc_extspk_dai));
	daiLinkNum += ARRAY_SIZE(mt_soc_extspk_dai);


	snd_soc_card_mt.dai_link = mt_soc_dai_component;
	snd_soc_card_mt.num_links = daiLinkNum;
	snd_soc_card_mt.controls = mt_soc_controls;
	snd_soc_card_mt.num_controls = ARRAY_SIZE(mt_soc_controls);
	mt_snd_device = platform_device_alloc("soc-audio", -1);

	if (!mt_snd_device) {
		pr_err("mt6797_probe  platform_device_alloc fail\n");
		return -ENOMEM;
	}

	platform_set_drvdata(mt_snd_device, &snd_soc_card_mt);
	ret = platform_device_add(mt_snd_device);

	if (ret != 0) {
		pr_err("mt_soc_snd_init goto put_device fail\n");
		goto put_device;
	}

	pr_warn("mt_soc_snd_init dai_link = %p\n", snd_soc_card_mt.dai_link);

	/* create debug file */
	mt_sco_audio_debugfs = debugfs_create_file(DEBUG_FS_NAME,
	   S_IFREG | S_IRUGO, NULL, (void *) DEBUG_FS_NAME, &mtaudio_debug_ops);


	/* create analog debug file */
	mt_sco_audio_debugfs = debugfs_create_file(DEBUG_ANA_FS_NAME,
	   S_IFREG | S_IRUGO, NULL, (void *) DEBUG_ANA_FS_NAME, &mtaudio_ana_debug_ops);

	return 0;
put_device:
	platform_device_put(mt_snd_device);
	return ret;

}

static void __exit mt_soc_snd_exit(void)
{
	platform_device_unregister(mt_snd_device);
}

late_initcall(mt_soc_snd_init);
module_exit(mt_soc_snd_exit);

/* Module information */
MODULE_AUTHOR("ChiPeng <chipeng.chang@mediatek.com>");
MODULE_DESCRIPTION("ALSA SoC driver ");

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
