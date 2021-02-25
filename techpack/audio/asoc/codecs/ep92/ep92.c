// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <linux/workqueue.h>
#include "ep92.h"

#define DRV_NAME "ep92_codec"

#define EP92_POLL_INTERVAL_OFF_MSEC 200
#define EP92_POLL_INTERVAL_ON_MSEC  20
#define EP92_POLL_RUNOUT_MSEC       5000
#define EP92_SYSFS_ENTRY_MAX_LEN 64
#define EP92_HYST_CNT 5

#define EP92_RATES (SNDRV_PCM_RATE_32000 |\
	SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
	SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 |\
	SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define EP92_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static const unsigned int ep92_samp_freq_table[8] = {
	32000, 44100, 48000, 88200, 96000, 176400, 192000, 768000
};

static const unsigned int ep92_dsd_freq_table[4] = {
	64, 128, 256, 0
};

/* EP92 register default values */
static struct reg_default ep92_reg_defaults[] = {
	{EP92_BI_VENDOR_ID_0,                   0x17},
	{EP92_BI_VENDOR_ID_1,                   0x7A},
	{EP92_BI_DEVICE_ID_0,                   0x94},
	{EP92_BI_DEVICE_ID_1,                   0xA3},
	{EP92_BI_VERSION_NUM,                   0x10},
	{EP92_BI_VERSION_YEAR,                  0x09},
	{EP92_BI_VERSION_MONTH,                 0x07},
	{EP92_BI_VERSION_DATE,                  0x06},
	{EP92_BI_GENERAL_INFO_0,                0x00},
	{EP92_BI_GENERAL_INFO_1,                0x00},
	{EP92_BI_GENERAL_INFO_2,                0x00},
	{EP92_BI_GENERAL_INFO_3,                0x00},
	{EP92_BI_GENERAL_INFO_4,                0x00},
	{EP92_BI_GENERAL_INFO_5,                0x00},
	{EP92_BI_GENERAL_INFO_6,                0x00},
	{EP92_ISP_MODE_ENTER_ISP,               0x00},
	{EP92_GENERAL_CONTROL_0,                0x20},
	{EP92_GENERAL_CONTROL_1,                0x00},
	{EP92_GENERAL_CONTROL_2,                0x00},
	{EP92_GENERAL_CONTROL_3,                0x10},
	{EP92_GENERAL_CONTROL_4,                0x00},
	{EP92_CEC_EVENT_CODE,                   0x00},
	{EP92_CEC_EVENT_PARAM_1,                0x00},
	{EP92_CEC_EVENT_PARAM_2,                0x00},
	{EP92_CEC_EVENT_PARAM_3,                0x00},
	{EP92_CEC_EVENT_PARAM_4,                0x00},
	{EP92_AUDIO_INFO_SYSTEM_STATUS_0,       0x00},
	{EP92_AUDIO_INFO_SYSTEM_STATUS_1,       0x00},
	{EP92_AUDIO_INFO_AUDIO_STATUS,          0x00},
	{EP92_AUDIO_INFO_CHANNEL_STATUS_0,      0x00},
	{EP92_AUDIO_INFO_CHANNEL_STATUS_1,      0x00},
	{EP92_AUDIO_INFO_CHANNEL_STATUS_2,      0x00},
	{EP92_AUDIO_INFO_CHANNEL_STATUS_3,      0x00},
	{EP92_AUDIO_INFO_CHANNEL_STATUS_4,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_0,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_1,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_2,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_3,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_4,      0x00},
	{EP92_AUDIO_INFO_ADO_INFO_FRAME_5,      0x00},
	{EP92_OTHER_PACKETS_HDMI_VS_0,          0x00},
	{EP92_OTHER_PACKETS_HDMI_VS_1,          0x00},
	{EP92_OTHER_PACKETS_ACP_PACKET,         0x00},
	{EP92_OTHER_PACKETS_AVI_INFO_FRAME_0,   0x00},
	{EP92_OTHER_PACKETS_AVI_INFO_FRAME_1,   0x00},
	{EP92_OTHER_PACKETS_AVI_INFO_FRAME_2,   0x00},
	{EP92_OTHER_PACKETS_AVI_INFO_FRAME_3,   0x00},
	{EP92_OTHER_PACKETS_AVI_INFO_FRAME_4,   0x00},
	{EP92_OTHER_PACKETS_GC_PACKET_0,        0x00},
	{EP92_OTHER_PACKETS_GC_PACKET_1,        0x00},
	{EP92_OTHER_PACKETS_GC_PACKET_2,        0x00},
};

static bool ep92_volatile_register(struct device *dev, unsigned int reg)
{
	/* do not cache register state in regmap */
	return true;
}

static bool ep92_writeable_registers(struct device *dev, unsigned int reg)
{
	if (reg >= EP92_ISP_MODE_ENTER_ISP && reg <= EP92_GENERAL_CONTROL_4)
		return true;

	return false;
}

static bool ep92_readable_registers(struct device *dev, unsigned int reg)
{
	if (reg >= EP92_BI_VENDOR_ID_0 && reg <= EP92_MAX_REGISTER_ADDR)
		return true;

	return false;
}

/* codec private data */
struct ep92_pdata {
	struct regmap        *regmap;
	struct snd_soc_component *component;
	struct timer_list    timer;
	struct work_struct   read_status_worker;
	int                  irq;
	int                  poll_trig;
	int                  poll_rem;
	int                  force_inactive;

	int                  hyst_tx_plug;
	int                  hyst_link_on0;
	int                  hyst_link_on1;
	int                  hyst_link_on2;
	int                  filt_tx_plug;
	int                  filt_link_on0;
	int                  filt_link_on1;
	int                  filt_link_on2;
	struct {
		u8 tx_info;
		u8 video_latency;
	} gi; /* General Info block */

	struct {
		u8 ctl;
		u8 rx_sel;
		u8 ctl2;
		u8 cec_volume;
		u8 link;
	} gc; /* General Control block */

	struct {
		u8 system_status_0;
		u8 system_status_1;
		u8 audio_status;
		u8 cs[5];
		u8 cc;
		u8 ca;
	} ai; /* Audio Info block */

	u8 old_mode;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *debugfs_dir;
	struct dentry *debugfs_file_wo;
	struct dentry *debugfs_file_ro;
#endif /* CONFIG_DEBUG_FS */
};

struct ep92_mclk_cfg_info {
	uint32_t in_sample_rate;
	uint32_t out_mclk_freq;
	uint8_t mul_val;
};

#define EP92_MCLK_MUL_512		0x3
#define EP92_MCLK_MUL_384		0x2
#define EP92_MCLK_MUL_256		0x1
#define EP92_MCLK_MUL_128		0x0
#define EP92_MCLK_MUL_MASK		0x3

/**
 * ep92_set_ext_mclk - Configure the mclk based on sample freq
 *
 * @codec: handle pointer to ep92 codec
 * @mclk_freq: mclk frequency to be set
 *
 * Returns 0 for sucess or appropriate negative error code
 */
int ep92_set_ext_mclk(struct snd_soc_codec *codec, uint32_t mclk_freq)
{
	unsigned int samp_freq = 0;
	struct ep92_pdata *ep92 = NULL;
	uint8_t value = 0;
	int ret = 0;

	if (!codec)
		return -EINVAL;

	ep92 = snd_soc_codec_get_drvdata(codec);

	samp_freq = ep92_samp_freq_table[(ep92->ai.audio_status) &
						EP92_AI_RATE_MASK];

	if (!mclk_freq || (mclk_freq % samp_freq)) {
		pr_err("%s incompatbile mclk:%u and sample freq:%u\n",
				__func__, mclk_freq, samp_freq);
		return -EINVAL;
	}

	switch (mclk_freq / samp_freq) {
	case 512:
		value = EP92_MCLK_MUL_512;
		break;
	case 384:
		value = EP92_MCLK_MUL_384;
		break;
	case 256:
		value = EP92_MCLK_MUL_256;
		break;
	case 128:
		value = EP92_MCLK_MUL_128;
		break;
	default:
		dev_err(codec->dev, "unsupported mclk:%u for sample freq:%u\n",
					mclk_freq, samp_freq);
		return -EINVAL;
	}

	pr_debug("%s mclk:%u, in sample freq:%u, write reg:0x%02x val:0x%02x\n",
		__func__, mclk_freq, samp_freq,
		EP92_GENERAL_CONTROL_2, EP92_MCLK_MUL_MASK & value);

	ret = snd_soc_update_bits(codec, EP92_GENERAL_CONTROL_2,
					EP92_MCLK_MUL_MASK, value);

	return (((ret == 0) || (ret == 1)) ? 0 : ret);
}
EXPORT_SYMBOL(ep92_set_ext_mclk);

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int debugfs_codec_open_op(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int debugfs_get_parameters(char *buf, u32 *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

static ssize_t debugfs_codec_write_op(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct ep92_pdata *ep92 = (struct ep92_pdata *) filp->private_data;
	struct snd_soc_component *component = ep92->component;
	char lbuf[32];
	int rc;
	u32 param[2];

	if (!component)
		return -ENODEV;

	if (!filp || !ppos || !ubuf)
		return -EINVAL;
	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;
	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;
	lbuf[cnt] = '\0';
	rc = debugfs_get_parameters(lbuf, param, 2);
	if ((param[0] < EP92_ISP_MODE_ENTER_ISP)
		|| (param[0] > EP92_GENERAL_CONTROL_4)) {
		dev_err(component->dev, "%s: reg address 0x%02X out of range\n",
			__func__, param[0]);
		return -EINVAL;
	}
	if ((param[1] < 0) || (param[1] > 255)) {
		dev_err(component->dev, "%s: reg data 0x%02X out of range\n",
			__func__, param[1]);
		return -EINVAL;
	}
	if (rc == 0) {
		rc = cnt;
		dev_info(component->dev, "%s: reg[0x%02X]=0x%02X\n",
			__func__, param[0], param[1]);
		snd_soc_component_write(component, param[0], param[1]);
	} else {
		dev_err(component->dev, "%s: write to register addr=0x%02X failed\n",
			__func__, param[0]);
	}
	return rc;
}

static ssize_t debugfs_ep92_reg_show(struct snd_soc_component *component,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[20];

	if (!ubuf || !ppos || !component || *ppos < 0)
		return -EINVAL;

	for (i = (int) *ppos / 11; i <= EP92_MAX_REGISTER_ADDR; i++) {
		reg_val = snd_soc_component_read32(component, i);
		len = snprintf(tmp_buf, 20, "0x%02X: 0x%02X\n", i,
			(reg_val & 0xFF));
		if ((total + len) > count)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			dev_err(component->dev, "%s: fail to copy reg dump\n",
				__func__);
			total = -EFAULT;
			goto copy_err;
		}
		*ppos += len;
		total += len;
	}

copy_err:
	return total;
}

static ssize_t debugfs_codec_read_op(struct file *filp,
		char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct ep92_pdata *ep92 = (struct ep92_pdata *) filp->private_data;
	struct snd_soc_component *component = ep92->component;
	ssize_t ret_cnt;

	if (!component)
		return -ENODEV;

	if (!filp || !ppos || !ubuf || *ppos < 0)
		return -EINVAL;
	ret_cnt = debugfs_ep92_reg_show(component, ubuf, cnt, ppos);
	return ret_cnt;
}

static const struct file_operations debugfs_codec_ops = {
	.open = debugfs_codec_open_op,
	.write = debugfs_codec_write_op,
	.read = debugfs_codec_read_op,
};
#endif /* CONFIG_DEBUG_FS */

static int ep92_send_uevent(struct ep92_pdata *ep92, char *event)
{
	char *env[] = { event, NULL };

	if (!event || !ep92)
		return -EINVAL;

	if (!ep92->component)
		return -ENODEV;
	return kobject_uevent_env(&ep92->component->dev->kobj,
			KOBJ_CHANGE, env);
}

static int ep92_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static void ep92_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
}

static int ep92_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_ops ep92_dai_ops = {
	.startup = ep92_startup,
	.shutdown = ep92_shutdown,
	.hw_params = ep92_hw_params,
};

static struct snd_soc_dai_driver ep92_dai[] = {
	{
		.name = "ep92-hdmi",
		.id = 1,
		.capture = {
			.stream_name = "HDMI Capture",
			.rate_max = 192000,
			.rate_min = 32000,
			.channels_min = 1,
			.channels_max = 8,
			.rates = EP92_RATES,
			.formats = EP92_FORMATS,
		},
		.ops = &ep92_dai_ops, /* callbacks */
	},
	{
		.name = "ep92-arc",
		.id = 2,
		.capture = {
			.stream_name = "ARC Capture",
			.rate_max = 192000,
			.rate_min = 32000,
			.channels_min = 1,
			.channels_max = 2,
			.rates = EP92_RATES,
			.formats = EP92_FORMATS,
		},
		.ops = &ep92_dai_ops, /* callbacks */
	},
};

static void ep92_read_general_control(struct snd_soc_component *component,
	struct ep92_pdata *ep92)
{
	u8 old, change;
	int val;

	old = ep92->gi.tx_info;
	ep92->gi.tx_info = snd_soc_component_read32(component,
				EP92_BI_GENERAL_INFO_0);
	if (ep92->gi.tx_info == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_BI_GENERAL_INFO_0 read 0xff\n");
		ep92->gi.tx_info = old;
	}
	/* implement hysteresis to prevent events on glitches */
	if (ep92->gi.tx_info & EP92_GI_TX_HOT_PLUG_MASK) {
		if (ep92->hyst_tx_plug < EP92_HYST_CNT) {
			ep92->hyst_tx_plug++;
			if ((ep92->hyst_tx_plug == EP92_HYST_CNT) &&
			    (ep92->filt_tx_plug == 0)) {
				ep92->filt_tx_plug = 1;
				dev_dbg(component->dev, "ep92 out_plug changed to 1\n");
				ep92_send_uevent(ep92,
					"EP92EVT_OUT_PLUG=CONNECTED");
			}
		}
	} else {
		if (ep92->hyst_tx_plug > 0) {
			ep92->hyst_tx_plug--;
			if ((ep92->hyst_tx_plug == 0) &&
			    (ep92->filt_tx_plug == 1)) {
				ep92->filt_tx_plug = 0;
				dev_dbg(component->dev, "ep92 out_plug changed to 0\n");
				ep92_send_uevent(ep92,
					"EP92EVT_OUT_PLUG=DISCONNECTED");
			}
		}
	}

	old = ep92->gi.video_latency;
	ep92->gi.video_latency = snd_soc_component_read32(component,
					EP92_BI_GENERAL_INFO_4);
	if (ep92->gi.video_latency == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_BI_GENERAL_INFO_4 read 0xff\n");
		ep92->gi.video_latency = old;
	}
	change = ep92->gi.video_latency ^ old;
	if (change & EP92_GI_VIDEO_LATENCY_MASK) {
		val = ep92->gi.video_latency;
		if (val > 0)
			val = (val - 1) * 2;
		dev_dbg(component->dev, "ep92 video latency changed to %d\n", val);
		ep92_send_uevent(ep92, "EP92EVT_VIDEO_LATENCY=CHANGED");
	}

	old = ep92->gc.ctl;
	ep92->gc.ctl = snd_soc_component_read32(component,
			EP92_GENERAL_CONTROL_0);
	if (ep92->gc.ctl == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_GENERAL_CONTROL_0 read 0xff\n");
		ep92->gc.ctl = old;
	}
	change = ep92->gc.ctl ^ old;
	if (change & EP92_GC_POWER_MASK) {
		val = (ep92->gc.ctl >> EP92_GC_POWER_SHIFT) &
			EP92_2CHOICE_MASK;
		dev_dbg(component->dev, "ep92 power changed to %d\n", val);
		if (val)
			ep92_send_uevent(ep92, "EP92EVT_POWER=ON");
		else
			ep92_send_uevent(ep92, "EP92EVT_POWER=OFF");
	}
	if (change & EP92_GC_AUDIO_PATH_MASK) {
		val = (ep92->gc.ctl >> EP92_GC_AUDIO_PATH_SHIFT) &
			EP92_2CHOICE_MASK;
		dev_dbg(component->dev, "ep92 audio_path changed to %d\n", val);
		if (val)
			ep92_send_uevent(ep92, "EP92EVT_AUDIO_PATH=TV");
		else
			ep92_send_uevent(ep92, "EP92EVT_AUDIO_PATH=SPEAKER");
	}
	if (change & EP92_GC_CEC_MUTE_MASK) {
		val = (ep92->gc.ctl >> EP92_GC_CEC_MUTE_SHIFT) &
			EP92_2CHOICE_MASK;
		dev_dbg(component->dev, "ep92 cec_mute changed to %d\n", val);
		if (val)
			ep92_send_uevent(ep92, "EP92EVT_CEC_MUTE=NORMAL");
		else
			ep92_send_uevent(ep92, "EP92EVT_CEC_MUTE=MUTED");
	}
	if (change & EP92_GC_ARC_EN_MASK) {
		val = ep92->gc.ctl & EP92_2CHOICE_MASK;
		dev_dbg(component->dev, "ep92 arc_en changed to %d\n", val);
		if (val)
			ep92_send_uevent(ep92, "EP92EVT_ARC_EN=ON");
		else
			ep92_send_uevent(ep92, "EP92EVT_ARC_EN=OFF");
	}

	old = ep92->gc.rx_sel;
	ep92->gc.rx_sel = snd_soc_component_read32(component,
				EP92_GENERAL_CONTROL_1);
	if (ep92->gc.rx_sel == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_GENERAL_CONTROL_1 read 0xff\n");
		ep92->gc.rx_sel = old;
	}
	change = ep92->gc.rx_sel ^ old;
	if (change & EP92_GC_RX_SEL_MASK) {
		val = ep92->gc.rx_sel & EP92_GC_RX_SEL_MASK;
		dev_dbg(component->dev, "ep92 rx_sel changed to %d\n", val);
		ep92_send_uevent(ep92, "EP92EVT_SRC_SEL=CHANGED");
	}

	old = ep92->gc.cec_volume;
	ep92->gc.cec_volume = snd_soc_component_read32(component,
				EP92_GENERAL_CONTROL_3);
	if (ep92->gc.cec_volume == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_GENERAL_CONTROL_3 read 0xff\n");
		ep92->gc.cec_volume = old;
	}
	change = ep92->gc.cec_volume ^ old;
	if (change & EP92_GC_CEC_VOLUME_MASK) {
		val = ep92->gc.cec_volume & EP92_GC_CEC_VOLUME_MASK;
		dev_dbg(component->dev, "ep92 cec_volume changed to %d\n", val);
		ep92_send_uevent(ep92, "EP92EVT_CEC_VOLUME=CHANGED");
	}

	old = ep92->gc.link;
	ep92->gc.link = snd_soc_component_read32(component,
				EP92_GENERAL_CONTROL_4);
	if (ep92->gc.link == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_GENERAL_CONTROL_4 read 0xff\n");
		ep92->gc.link = old;
	}

	/* implement hysteresis to prevent events on glitches */
	if (ep92->gc.link & EP92_GC_LINK_ON0_MASK) {
		if (ep92->hyst_link_on0 < EP92_HYST_CNT) {
			ep92->hyst_link_on0++;
			if ((ep92->hyst_link_on0 == EP92_HYST_CNT) &&
			    (ep92->filt_link_on0 == 0)) {
				ep92->filt_link_on0 = 1;
				dev_dbg(component->dev, "ep92 link_on0 changed to 1\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON0=CONNECTED");
			}
		}
	} else {
		if (ep92->hyst_link_on0 > 0) {
			ep92->hyst_link_on0--;
			if ((ep92->hyst_link_on0 == 0) &&
			    (ep92->filt_link_on0 == 1)) {
				ep92->filt_link_on0 = 0;
				dev_dbg(component->dev, "ep92 link_on0 changed to 0\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON0=DISCONNECTED");
			}
		}
	}

	/* implement hysteresis to prevent events on glitches */
	if (ep92->gc.link & EP92_GC_LINK_ON1_MASK) {
		if (ep92->hyst_link_on1 < EP92_HYST_CNT) {
			ep92->hyst_link_on1++;
			if ((ep92->hyst_link_on1 == EP92_HYST_CNT) &&
			    (ep92->filt_link_on1 == 0)) {
				ep92->filt_link_on1 = 1;
				dev_dbg(component->dev, "ep92 link_on1 changed to 1\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON1=CONNECTED");
			}
		}
	} else {
		if (ep92->hyst_link_on1 > 0) {
			ep92->hyst_link_on1--;
			if ((ep92->hyst_link_on1 == 0) &&
			    (ep92->filt_link_on1 == 1)) {
				ep92->filt_link_on1 = 0;
				dev_dbg(component->dev, "ep92 link_on1 changed to 0\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON1=DISCONNECTED");
			}
		}
	}

	/* implement hysteresis to prevent events on glitches */
	if (ep92->gc.link & EP92_GC_LINK_ON2_MASK) {
		if (ep92->hyst_link_on2 < EP92_HYST_CNT) {
			ep92->hyst_link_on2++;
			if ((ep92->hyst_link_on2 == EP92_HYST_CNT) &&
			    (ep92->filt_link_on2 == 0)) {
				ep92->filt_link_on2 = 1;
				dev_dbg(component->dev, "ep92 link_on2 changed to 1\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON2=CONNECTED");
			}
		}
	} else {
		if (ep92->hyst_link_on2 > 0) {
			ep92->hyst_link_on2--;
			if ((ep92->hyst_link_on2 == 0) &&
			    (ep92->filt_link_on2 == 1)) {
				ep92->filt_link_on2 = 0;
				dev_dbg(component->dev, "ep92 link_on2 changed to 0\n");
				ep92_send_uevent(ep92,
					"EP92EVT_LINK_ON2=DISCONNECTED");
			}
		}
	}
}

static void ep92_read_audio_info(struct snd_soc_component *component,
	struct ep92_pdata *ep92)
{
	u8 old, change;
	u8 new_mode;
	bool send_uevent = false;

	old = ep92->ai.system_status_0;
	ep92->ai.system_status_0 = snd_soc_component_read32(component,
		EP92_AUDIO_INFO_SYSTEM_STATUS_0);
	if (ep92->ai.system_status_0 == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_AUDIO_INFO_SYSTEM_STATUS_0 read 0xff\n");
		ep92->ai.system_status_0 = old;
	}
	change = ep92->ai.system_status_0 ^ old;
	if (change & EP92_AI_MCLK_ON_MASK) {
		dev_dbg(component->dev, "ep92 status changed to %d\n",
			(ep92->ai.system_status_0 >> EP92_AI_MCLK_ON_SHIFT) &
			EP92_2CHOICE_MASK);
		send_uevent = true;
	}
	if (change & EP92_AI_AVMUTE_MASK) {
		dev_dbg(component->dev, "ep92 avmute changed to %d\n",
			(ep92->ai.system_status_0 >> EP92_AI_AVMUTE_SHIFT) &
			EP92_2CHOICE_MASK);
		send_uevent = true;
	}
	if (change & EP92_AI_LAYOUT_MASK) {
		dev_dbg(component->dev, "ep92 layout changed to %d\n",
			(ep92->ai.system_status_0) & EP92_2CHOICE_MASK);
		send_uevent = true;
	}

	old = ep92->ai.system_status_1;
	ep92->ai.system_status_1 = snd_soc_read(codec,
		EP92_AUDIO_INFO_SYSTEM_STATUS_1);
	if (ep92->ai.system_status_1 == 0xff) {
		dev_dbg(codec->dev,
			"ep92 EP92_AUDIO_INFO_SYSTEM_STATUS_1 read 0xff\n");
		ep92->ai.system_status_1 = old;
	}
	change = ep92->ai.system_status_1 ^ old;
	if (change & EP92_AI_DSD_RATE_MASK) {
		dev_dbg(codec->dev, "ep92 dsd rate changed to %d\n",
			ep92_dsd_freq_table[(ep92->ai.system_status_1 &
				EP92_AI_DSD_RATE_MASK)
				>> EP92_AI_DSD_RATE_SHIFT]);
		send_uevent = true;
	}

	old = ep92->ai.audio_status;
	ep92->ai.audio_status = snd_soc_component_read32(component,
		EP92_AUDIO_INFO_AUDIO_STATUS);
	if (ep92->ai.audio_status == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_AUDIO_INFO_AUDIO_STATUS read 0xff\n");
		ep92->ai.audio_status = old;
	}
	change = ep92->ai.audio_status ^ old;
	if (change & EP92_AI_RATE_MASK) {
		dev_dbg(component->dev, "ep92 rate changed to %d\n",
			ep92_samp_freq_table[(ep92->ai.audio_status) &
			EP92_AI_RATE_MASK]);
		send_uevent = true;
	}

	old = ep92->ai.cs[0];
	ep92->ai.cs[0] = snd_soc_component_read32(component,
		EP92_AUDIO_INFO_CHANNEL_STATUS_0);
	if (ep92->ai.cs[0] == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_AUDIO_INFO_CHANNEL_STATUS_0 read 0xff\n");
		ep92->ai.cs[0] = old;
	}
	change = ep92->ai.cs[0] ^ old;
	if (change & EP92_AI_PREEMPH_MASK) {
		dev_dbg(component->dev, "ep92 preemph changed to %d\n",
			(ep92->ai.cs[0] & EP92_AI_PREEMPH_MASK) >>
			EP92_AI_PREEMPH_SHIFT);
		send_uevent = true;
	}

	new_mode = ep92->old_mode;
	if (ep92->ai.audio_status & EP92_AI_DSD_ADO_MASK)
		new_mode = 2; /* One bit audio */
	else if (ep92->ai.audio_status & EP92_AI_STD_ADO_MASK) {
		if (ep92->ai.cs[0] & EP92_AI_NPCM_MASK)
			new_mode = 1; /* Compr */
		else
			new_mode = 0; /* LPCM */
	} else if (ep92->ai.audio_status & EP92_AI_HBR_ADO_MASK)
		new_mode = 1; /* Compr */

	if (ep92->old_mode != new_mode) {
		dev_dbg(component->dev, "ep92 mode changed to %d\n", new_mode);
		send_uevent = true;
	}
	ep92->old_mode = new_mode;

	old = ep92->ai.cc;
	ep92->ai.cc = snd_soc_component_read32(component,
				EP92_AUDIO_INFO_ADO_INFO_FRAME_1);
	if (ep92->ai.cc == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_AUDIO_INFO_ADO_INFO_FRAME_1 read 0xff\n");
		ep92->ai.cc = old;
	}
	change = ep92->ai.cc ^ old;
	if (change & EP92_AI_CH_COUNT_MASK) {
		dev_dbg(component->dev, "ep92 ch_count changed to %d (%d)\n",
			ep92->ai.cc & EP92_AI_CH_COUNT_MASK,
			(ep92->ai.cc & EP92_AI_CH_COUNT_MASK) == 0 ? 0 :
			(ep92->ai.cc & EP92_AI_CH_COUNT_MASK) + 1);
		send_uevent = true;
	}

	old = ep92->ai.ca;
	ep92->ai.ca = snd_soc_component_read32(component,
				EP92_AUDIO_INFO_ADO_INFO_FRAME_4);
	if (ep92->ai.ca == 0xff) {
		dev_dbg(component->dev, "ep92 EP92_AUDIO_INFO_ADO_INFO_FRAME_4 read 0xff\n");
		ep92->ai.ca = old;
	}
	change = ep92->ai.ca ^ old;
	if (change & EP92_AI_CH_ALLOC_MASK) {
		dev_dbg(component->dev, "ep92 ch_alloc changed to 0x%02x\n",
			(ep92->ai.ca) & EP92_AI_CH_ALLOC_MASK);
		send_uevent = true;
	}

	if (send_uevent)
		ep92_send_uevent(ep92, "EP92EVT_AUDIO=MEDIA_CONFIG_CHANGE");
}

static void ep92_init(struct snd_soc_component *component,
		      struct ep92_pdata *ep92)
{
	int reg0 = 0;
	int reg1 = 0;
	int reg2 = 0;
	int reg3 = 0;

	if (!ep92 || !component)
		return;

	reg0 = snd_soc_component_read32(component, EP92_BI_VERSION_YEAR);
	reg1 = snd_soc_component_read32(component, EP92_BI_VERSION_MONTH);
	reg2 = snd_soc_component_read32(component, EP92_BI_VERSION_DATE);
	reg3 = snd_soc_component_read32(component, EP92_BI_VERSION_NUM);

	dev_info(compoent->dev, "ep92 version info %02d/%02d/%02d %d\n",
		reg0, reg1, reg2, reg3);

	/* update the format information in mixer controls */
	ep92_read_general_control(component, ep92);
	ep92_read_audio_info(component, ep92);
}

static int ep92_probe(struct snd_soc_component *component)
{
	struct ep92_pdata *ep92 = snd_soc_component_get_drvdata(component);

	ep92->component = component;
	ep92_init(component, ep92);

	/* start polling when codec is registered */
	mod_timer(&ep92->timer, jiffies +
		msecs_to_jiffies(EP92_POLL_INTERVAL_OFF_MSEC));

	return 0;
}

static void ep92_remove(struct snd_soc_component *component)
{
	return;
}

static const struct snd_soc_component_driver soc_codec_drv_ep92 = {
	.name = DRV_NAME,
	.probe  = ep92_probe,
	.remove = ep92_remove,
};

static struct regmap_config ep92_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = ep92_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(ep92_reg_defaults),
	.max_register = EP92_MAX_REGISTER_ADDR,
	.volatile_reg = ep92_volatile_register,
	.writeable_reg = ep92_writeable_registers,
	.readable_reg = ep92_readable_registers,
};

void ep92_read_status(struct work_struct *work)
{
	struct ep92_pdata *ep92 = container_of(work, struct ep92_pdata,
		read_status_worker);
	struct snd_soc_component *component = ep92->component;
	u8 val;

	/* No polling before component is initialized */
	if (component == NULL)
		return;

	if (ep92->force_inactive)
		return;

	/* check ADO_CHF that is set when audio format has changed */
	val = snd_soc_component_read32(component, EP92_BI_GENERAL_INFO_1);
	if (val == 0xff) {
		/* workaround for Nak'ed first read */
		val = snd_soc_component_read32(component,
				EP92_BI_GENERAL_INFO_1);
		if (val == 0xff)
			return;	/* assume device not present */
	}

	if (val & EP92_GI_ADO_CHF_MASK)
		dev_dbg(component->dev, "ep92 audio mode change trigger.\n");

	if (val & EP92_GI_CEC_ECF_MASK)
		dev_dbg(component->dev, "ep92 CEC change trigger.\n");

	/* check for general control changes */
	ep92_read_general_control(component, ep92);

	/* update the format information in mixer controls */
	ep92_read_audio_info(component, ep92);
}

static irqreturn_t ep92_irq(int irq, void *data)
{
	struct ep92_pdata *ep92 = data;
	struct snd_soc_component *component = ep92->component;

	/* Treat interrupt before component is initialized as spurious */
	if (component == NULL)
		return IRQ_NONE;

	dev_dbg(component->dev, "ep92_interrupt\n");

	ep92->poll_trig = 1;
	mod_timer(&ep92->timer, jiffies +
		msecs_to_jiffies(EP92_POLL_INTERVAL_ON_MSEC));

	schedule_work(&ep92->read_status_worker);

	return IRQ_HANDLED;
};

void ep92_poll_status(struct timer_list *t)
{
	struct ep92_pdata *ep92 = from_timer(ep92, t, timer);
	struct snd_soc_component *component = ep92->component;

	if (ep92->force_inactive)
		return;

	/* if no IRQ is configured, always keep on polling */
	if (ep92->irq == 0)
		ep92->poll_rem = EP92_POLL_RUNOUT_MSEC;

	/* on interrupt, start polling for some time */
	if (ep92->poll_trig) {
		if (ep92->poll_rem == 0)
			dev_info(component->dev, "status checking activated\n");

		ep92->poll_trig = 0;
		ep92->poll_rem = EP92_POLL_RUNOUT_MSEC;
	}

	/*
	 * If power_on == 0, poll only until poll_rem reaches zero and stop.
	 * This allows to system to go to low power sleep mode.
	 * Otherwise (power_on == 1) always re-arm timer to keep on polling.
	 */
	if ((ep92->gc.ctl & EP92_GC_POWER_MASK) == 0) {
		if (ep92->poll_rem) {
			mod_timer(&ep92->timer, jiffies +
				msecs_to_jiffies(EP92_POLL_INTERVAL_OFF_MSEC));
			if (ep92->poll_rem > EP92_POLL_INTERVAL_OFF_MSEC) {
				ep92->poll_rem -= EP92_POLL_INTERVAL_OFF_MSEC;
			} else {
				dev_info(component->dev, "status checking stopped\n");
				ep92->poll_rem = 0;
			}
		}
	} else {
		ep92->poll_rem = EP92_POLL_RUNOUT_MSEC;
		mod_timer(&ep92->timer, jiffies +
			msecs_to_jiffies(EP92_POLL_INTERVAL_ON_MSEC));
	}

	schedule_work(&ep92->read_status_worker);
}

static const struct of_device_id ep92_of_match[] = {
	{ .compatible = "explore,ep92a6", },
	{ }
};
MODULE_DEVICE_TABLE(of, ep92_of_match);

static ssize_t ep92_sysfs_rda_chipid(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int reg0 = 0;
	int reg1 = 0;
	int reg2 = 0;
	int reg3 = 0;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	reg0 = snd_soc_component_read32(ep92->component, EP92_BI_VENDOR_ID_0);
	reg1 = snd_soc_component_read32(ep92->component, EP92_BI_VENDOR_ID_1);
	reg2 = snd_soc_component_read32(ep92->component, EP92_BI_DEVICE_ID_0);
	reg3 = snd_soc_component_read32(ep92->component, EP92_BI_DEVICE_ID_1);

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%02x%02x/%02x%02x\n",
		reg0, reg1, reg2, reg3);
	dev_dbg(dev, "%s: '%s'\n", __func__, buf);

	return ret;
}

static ssize_t ep92_sysfs_rda_version(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int reg0 = 0;
	int reg1 = 0;
	int reg2 = 0;
	int reg3 = 0;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	reg0 = snd_soc_component_read32(ep92->component,
				EP92_BI_VERSION_YEAR);
	reg1 = snd_soc_component_read32(ep92->component,
				EP92_BI_VERSION_MONTH);
	reg2 = snd_soc_component_read32(ep92->component,
				EP92_BI_VERSION_DATE);
	reg3 = snd_soc_component_read32(ep92->component,
				EP92_BI_VERSION_NUM);

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%02d/%02d/%02d %d\n",
		reg0, reg1, reg2, reg3);
	dev_dbg(dev, "%s: '%s'\n", __func__, buf);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->ai.system_status_0 & EP92_AI_MCLK_ON_MASK) >>
		EP92_AI_MCLK_ON_SHIFT;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_format(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->old_mode;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_dsd_rate(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->codec) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92_dsd_freq_table[(ep92->ai.system_status_1 &
			EP92_AI_DSD_RATE_MASK) >> EP92_AI_DSD_RATE_SHIFT];

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_rate(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92_samp_freq_table[(ep92->ai.audio_status) &
				   EP92_AI_RATE_MASK];
	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_layout(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->ai.system_status_0 & EP92_AI_LAYOUT_MASK) >>
		EP92_AI_LAYOUT_SHIFT;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_ch_count(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->ai.cc & EP92_AI_CH_COUNT_MASK;
	/* mapping is ch_count = reg_val + 1, with exception: 0 = unknown */
	if (val > 0)
		val += 1;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_ch_alloc(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->ai.ca & EP92_AI_CH_ALLOC_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_audio_preemph(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->ai.cs[0] & EP92_AI_PREEMPH_MASK) >>
		EP92_AI_PREEMPH_SHIFT;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_avmute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->ai.system_status_0 >> EP92_AI_AVMUTE_SHIFT) &
		EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_link_on0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->filt_link_on0;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_link_on1(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->filt_link_on1;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_link_on2(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->filt_link_on2;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_out_plug(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->filt_tx_plug;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_video_latency(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->gi.video_latency & EP92_GI_VIDEO_LATENCY_MASK;
	if (val > 0)
		val = (val - 1) * 2;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_arc_disable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.ctl2 >> EP92_GC_ARC_DIS_SHIFT) &
		EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_arc_disable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component,
			EP92_GENERAL_CONTROL_2);
	reg &= ~EP92_GC_ARC_DIS_MASK;
	reg |= ((val << EP92_GC_ARC_DIS_SHIFT) & EP92_GC_ARC_DIS_MASK);
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_2, reg);
	ep92->gc.ctl2 &= ~EP92_GC_ARC_DIS_MASK;
	ep92->gc.ctl2 |= (val << EP92_GC_ARC_DIS_SHIFT) & EP92_GC_ARC_DIS_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_power(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.ctl >> EP92_GC_POWER_SHIFT) & EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_power(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component, EP92_GENERAL_CONTROL_0);
	reg &= ~EP92_GC_POWER_MASK;
	reg |= (val << EP92_GC_POWER_SHIFT) & EP92_GC_POWER_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_0, reg);
	ep92->gc.ctl &= ~EP92_GC_POWER_MASK;
	ep92->gc.ctl |= (val << EP92_GC_POWER_SHIFT) & EP92_GC_POWER_MASK;

	if (val == 1) {
		ep92->poll_trig = 1;
		mod_timer(&ep92->timer, jiffies +
			msecs_to_jiffies(EP92_POLL_INTERVAL_ON_MSEC));
	}
	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_audio_path(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.ctl >> EP92_GC_AUDIO_PATH_SHIFT) & EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_audio_path(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component,
			EP92_GENERAL_CONTROL_0);
	reg &= ~EP92_GC_AUDIO_PATH_MASK;
	reg |= (val << EP92_GC_AUDIO_PATH_SHIFT) & EP92_GC_AUDIO_PATH_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_0, reg);
	ep92->gc.ctl &= ~EP92_GC_AUDIO_PATH_MASK;
	ep92->gc.ctl |= (val << EP92_GC_AUDIO_PATH_SHIFT) &
		EP92_GC_AUDIO_PATH_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_src_sel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->gc.rx_sel & EP92_GC_RX_SEL_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_src_sel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 7)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component,
			EP92_GENERAL_CONTROL_1);
	reg &= ~EP92_GC_RX_SEL_MASK;
	reg |= (val << EP92_GC_RX_SEL_SHIFT) & EP92_GC_RX_SEL_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_1, reg);
	ep92->gc.rx_sel &= ~EP92_GC_RX_SEL_MASK;
	ep92->gc.rx_sel |= (val << EP92_GC_RX_SEL_SHIFT) & EP92_GC_RX_SEL_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_arc_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.ctl >> EP92_GC_ARC_EN_SHIFT) & EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_arc_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component, EP92_GENERAL_CONTROL_0);
	reg &= ~EP92_GC_ARC_EN_MASK;
	reg |= (val << EP92_GC_ARC_EN_SHIFT) & EP92_GC_ARC_EN_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_0, reg);
	ep92->gc.ctl &= ~EP92_GC_ARC_EN_MASK;
	ep92->gc.ctl |= (val << EP92_GC_ARC_EN_SHIFT) &
		EP92_GC_ARC_EN_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_cec_mute(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.ctl >> EP92_GC_CEC_MUTE_SHIFT) & EP92_2CHOICE_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_cec_mute(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = snd_soc_component_read32(ep92->component, EP92_GENERAL_CONTROL_0);
	reg &= ~EP92_GC_CEC_MUTE_MASK;
	reg |= (val << EP92_GC_CEC_MUTE_SHIFT) & EP92_GC_CEC_MUTE_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_0, reg);
	ep92->gc.ctl &= ~EP92_GC_CEC_MUTE_MASK;
	ep92->gc.ctl |= (val << EP92_GC_CEC_MUTE_SHIFT) &
		EP92_GC_CEC_MUTE_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_cec_volume(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = (ep92->gc.cec_volume >> EP92_GC_CEC_VOLUME_SHIFT) &
		EP92_GC_CEC_VOLUME_MASK;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_cec_volume(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > EP92_GC_CEC_VOLUME_MAX)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	reg = val & EP92_GC_CEC_VOLUME_MASK;
	snd_soc_component_write(ep92->component, EP92_GENERAL_CONTROL_3, reg);
	ep92->gc.cec_volume = val & EP92_GC_CEC_VOLUME_MASK;

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static ssize_t ep92_sysfs_rda_runout(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->poll_rem;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_rda_force_inactive(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int val;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	val = ep92->force_inactive;

	ret = snprintf(buf, EP92_SYSFS_ENTRY_MAX_LEN, "%d\n", val);
	dev_dbg(dev, "%s: '%d'\n", __func__, val);

	return ret;
}

static ssize_t ep92_sysfs_wta_force_inactive(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val, rc;
	struct ep92_pdata *ep92 = dev_get_drvdata(dev);

	if (!ep92 || !ep92->component) {
		dev_err(dev, "%s: device error\n", __func__);
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if ((val < 0) || (val > 1)) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if (val == 0) {
		ep92->force_inactive = 0;
		ep92->poll_trig = 1;
		mod_timer(&ep92->timer, jiffies +
			msecs_to_jiffies(EP92_POLL_INTERVAL_ON_MSEC));
	} else {
		ep92->force_inactive = 1;
		ep92->poll_rem = 0;
	}

	rc = strnlen(buf, EP92_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static DEVICE_ATTR(chipid, 0444, ep92_sysfs_rda_chipid, NULL);
static DEVICE_ATTR(version, 0444, ep92_sysfs_rda_version, NULL);
static DEVICE_ATTR(audio_state, 0444, ep92_sysfs_rda_audio_state, NULL);
static DEVICE_ATTR(audio_format, 0444, ep92_sysfs_rda_audio_format, NULL);
static DEVICE_ATTR(audio_rate, 0444, ep92_sysfs_rda_audio_rate, NULL);
static DEVICE_ATTR(audio_layout, 0444, ep92_sysfs_rda_audio_layout, NULL);
static DEVICE_ATTR(audio_ch_count, 0444, ep92_sysfs_rda_audio_ch_count, NULL);
static DEVICE_ATTR(audio_ch_alloc, 0444, ep92_sysfs_rda_audio_ch_alloc, NULL);
static DEVICE_ATTR(audio_preemph, 0444, ep92_sysfs_rda_audio_preemph, NULL);
static DEVICE_ATTR(audio_avmute, 0444, ep92_sysfs_rda_avmute, NULL);
static DEVICE_ATTR(link_on0, 0444, ep92_sysfs_rda_link_on0, NULL);
static DEVICE_ATTR(link_on1, 0444, ep92_sysfs_rda_link_on1, NULL);
static DEVICE_ATTR(link_on2, 0444, ep92_sysfs_rda_link_on2, NULL);
static DEVICE_ATTR(out_plug, 0444, ep92_sysfs_rda_out_plug, NULL);
static DEVICE_ATTR(video_latency, 0444, ep92_sysfs_rda_video_latency, NULL);
static DEVICE_ATTR(arc_disable, 0644, ep92_sysfs_rda_arc_disable,
	ep92_sysfs_wta_arc_disable);
static DEVICE_ATTR(power_on, 0644, ep92_sysfs_rda_power, ep92_sysfs_wta_power);
static DEVICE_ATTR(audio_path, 0644, ep92_sysfs_rda_audio_path,
	ep92_sysfs_wta_audio_path);
static DEVICE_ATTR(src_sel, 0644, ep92_sysfs_rda_src_sel,
	ep92_sysfs_wta_src_sel);
static DEVICE_ATTR(arc_enable, 0644, ep92_sysfs_rda_arc_enable,
	ep92_sysfs_wta_arc_enable);
static DEVICE_ATTR(cec_mute, 0644, ep92_sysfs_rda_cec_mute,
	ep92_sysfs_wta_cec_mute);
static DEVICE_ATTR(cec_volume, 0644, ep92_sysfs_rda_cec_volume,
	ep92_sysfs_wta_cec_volume);
static DEVICE_ATTR(runout, 0444, ep92_sysfs_rda_runout, NULL);
static DEVICE_ATTR(force_inactive, 0644, ep92_sysfs_rda_force_inactive,
	ep92_sysfs_wta_force_inactive);
static DEVICE_ATTR(dsd_rate, 0444, ep92_sysfs_rda_dsd_rate, NULL);

static struct attribute *ep92_fs_attrs[] = {
	&dev_attr_chipid.attr,
	&dev_attr_version.attr,
	&dev_attr_audio_state.attr,
	&dev_attr_audio_format.attr,
	&dev_attr_audio_rate.attr,
	&dev_attr_audio_layout.attr,
	&dev_attr_audio_ch_count.attr,
	&dev_attr_audio_ch_alloc.attr,
	&dev_attr_audio_preemph.attr,
	&dev_attr_audio_avmute.attr,
	&dev_attr_link_on0.attr,
	&dev_attr_link_on1.attr,
	&dev_attr_link_on2.attr,
	&dev_attr_out_plug.attr,
	&dev_attr_video_latency.attr,
	&dev_attr_arc_disable.attr,
	&dev_attr_power_on.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_src_sel.attr,
	&dev_attr_arc_enable.attr,
	&dev_attr_cec_mute.attr,
	&dev_attr_cec_volume.attr,
	&dev_attr_runout.attr,
	&dev_attr_force_inactive.attr,
	&dev_attr_dsd_rate.attr,
	NULL,
};

static struct attribute_group ep92_fs_attrs_group = {
	.attrs = ep92_fs_attrs,
};

static int ep92_sysfs_create(struct i2c_client *client,
	struct ep92_pdata *ep92)
{
	int rc;

	rc = sysfs_create_group(&client->dev.kobj, &ep92_fs_attrs_group);

	return rc;
}

static void ep92_sysfs_remove(struct i2c_client *client,
	struct ep92_pdata *ep92)
{
	sysfs_remove_group(&client->dev.kobj, &ep92_fs_attrs_group);
}

static int ep92_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ep92_pdata *ep92;
	int ret;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	char debugfs_dir_name[32];
#endif

	ep92 = devm_kzalloc(&client->dev, sizeof(struct ep92_pdata),
		GFP_KERNEL);
	if (ep92 == NULL)
		return -ENOMEM;

	ep92->regmap = devm_regmap_init_i2c(client, &ep92_regmap_config);
	if (IS_ERR(ep92->regmap)) {
		ret = PTR_ERR(ep92->regmap);
		dev_err(&client->dev,
			"%s: Failed to allocate regmap for I2C device: %d\n",
			__func__, ret);
		return ret;
	}

	i2c_set_clientdata(client, ep92);

	/* register interrupt handler */
	INIT_WORK(&ep92->read_status_worker, ep92_read_status);
	ep92->irq = client->irq;
	if (ep92->irq) {
		ret = devm_request_threaded_irq(&client->dev, ep92->irq,
			NULL, ep92_irq, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"ep92_irq", ep92);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to request IRQ %d: %d\n",
				__func__, ep92->irq, ret);
			ep92->irq = 0;
		}
	}
	/* prepare timer */
	timer_setup(&ep92->timer, ep92_poll_status, 0);
	ep92->poll_rem = EP92_POLL_RUNOUT_MSEC;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs interface */
	snprintf(debugfs_dir_name, sizeof(debugfs_dir_name), "%s-%s",
		client->name, dev_name(&client->dev));
	ep92->debugfs_dir = debugfs_create_dir(debugfs_dir_name, NULL);
	if (!ep92->debugfs_dir) {
		dev_dbg(&client->dev,
			"%s: Failed to create /sys/kernel/debug/%s for debugfs\n",
			__func__, debugfs_dir_name);
		return -ENOMEM;
	}
	ep92->debugfs_file_wo = debugfs_create_file(
		"write_reg_val", S_IFREG | 0444, ep92->debugfs_dir,
		(void *) ep92,
		&debugfs_codec_ops);
	if (!ep92->debugfs_file_wo) {
		dev_dbg(&client->dev,
			"%s: Failed to create /sys/kernel/debug/%s/write_reg_val\n",
			__func__, debugfs_dir_name);
		return -ENOMEM;
	}
	ep92->debugfs_file_ro = debugfs_create_file(
		"show_reg_dump", S_IFREG | 0444, ep92->debugfs_dir,
		(void *) ep92,
		&debugfs_codec_ops);
	if (!ep92->debugfs_file_ro) {
		dev_dbg(&client->dev,
			"%s: Failed to create /sys/kernel/debug/%s/show_reg_dump\n",
			__func__, debugfs_dir_name);
		return -ENOMEM;
	}
#endif /* CONFIG_DEBUG_FS */

	/* register component */
	ret = snd_soc_register_component(&client->dev, &soc_codec_drv_ep92,
		ep92_dai, ARRAY_SIZE(ep92_dai));
	if (ret) {
		dev_err(&client->dev, "%s %d: Failed to register CODEC: %d\n",
			__func__, __LINE__, ret);
		goto err_reg;
	}

	ret = ep92_sysfs_create(client, ep92);
	if (ret) {
		dev_err(&client->dev, "%s: sysfs creation failed ret=%d\n",
			__func__, ret);
		goto err_sysfs;
	}

	return 0;

err_sysfs:
	snd_soc_unregister_component(&client->dev);
err_reg:
	del_timer(&ep92->timer);

	return ret;
}

static int ep92_i2c_remove(struct i2c_client *client)
{
	struct ep92_pdata *ep92;

	ep92 = i2c_get_clientdata(client);
	if (ep92) {
		del_timer(&ep92->timer);

#if IS_ENABLED(CONFIG_DEBUG_FS)
		debugfs_remove_recursive(ep92->debugfs_dir);
#endif
	}
	snd_soc_unregister_component(&client->dev);

	ep92_sysfs_remove(client, ep92);

	return 0;
}

static const struct i2c_device_id ep92_i2c_id[] = {
	{ "ep92-dev", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ep92_i2c_id);

static struct i2c_driver ep92_i2c_driver = {
	.probe =    ep92_i2c_probe,
	.remove =   ep92_i2c_remove,
	.id_table = ep92_i2c_id,
	.driver = {
		.name = "ep92",
		.owner = THIS_MODULE,
		.of_match_table = ep92_of_match
	},
};

static int __init ep92_codec_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ep92_i2c_driver);
	if (ret)
		pr_err("Failed to register EP92 I2C driver: %d\n", ret);

	return ret;
}
module_init(ep92_codec_init);

static void __exit ep92_codec_exit(void)
{
	i2c_del_driver(&ep92_i2c_driver);
}
module_exit(ep92_codec_exit);

MODULE_DESCRIPTION("EP92 HDMI repeater/switch driver");
MODULE_LICENSE("GPL v2");
