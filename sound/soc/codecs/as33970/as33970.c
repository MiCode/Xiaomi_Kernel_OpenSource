/*
 * cx33970.c -- CX33970 ALSA SoC Audio driver
 *
 * Copyright:   (C) 2020 Synaptics Systems, Inc.
 *
 * This is based on Alexander Sverdlin's CS4271 driver code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 ************************************************************************
 *  Modified Date:  2020/11/20
 *  File Version:   1.0.0
 ************************************************************************/
#define DEBUG
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "as33970.h"

#define CX33970_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

#define CX33970_ID(a, b, c, d)  (((a) - 0x20) << 8 | \
				      ((b) - 0x20) << 14| \
				      ((c) - 0x20) << 20| \
				      ((d) - 0x20) << 26)

#define CX33970_ID2CH_A(id)  (((((unsigned int)(id)) >> 8) & 0x3f) + 0x20)
#define CX33970_ID2CH_B(id)  (((((unsigned int)(id)) >> 14) & 0x3f) + 0x20)
#define CX33970_ID2CH_C(id)  (((((unsigned int)(id)) >> 20) & 0x3f) + 0x20)
#define CX33970_ID2CH_D(id)  (((((unsigned int)(id)) >> 26) & 0x3f) + 0x20)

#define CX33970_CONTROL(xname, xinfo, xget, xput, xaccess) { \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = xaccess, .info = xinfo, .get = xget, .put = xput, \
	}

#define CX33970_CMD_GET(item)   ((item) |  0x0100)
#define CX33970_CMD_SET(item)   (item)
#define CX33970_CMD_SIZE 13

#define CX33970_SYS_CMD_GET_PARAMETER_VALUE   \
	CX33970_CMD_GET(SYS_CMD_PARAMETER_VALUE)
#define CX33970_SYS_CMD_SET_PARAMETER_VALUE   \
	CX33970_CMD_SET(SYS_CMD_PARAMETER_VALUE)

#define CX33970_DAI_DSP  1
#define CX33970_READY  0x8badd00d
#define CX33970_ID_ARM        CX33970_ID('M', 'C', 'U', ' ')
/*
 * Defines the command format which is used to communicate with cx33970 device.
 */
struct cx33970_cmd {
	int	num_32b_words:16;   /* Indicates how many data to be sent.
				     * If operation is successful, this will
				     * be updated with the number of returned
				     * data in word. one word == 4 bytes.
				     */
	u32	command_id:15;
	u32	reply:1;            /* The device will set this flag once
				     * the operation is complete.
				     */
	u32	app_module_id;
	u32	data[CX33970_CMD_SIZE]; /* Used for storing parameters and
					 * receiving the returned data from
					 * device.
					 */
};

/* codec private data*/
struct cx33970_priv {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *gpiod_reset;
	struct cx33970_cmd cmd;
	int cmd_res;
	unsigned char current_mode[4];
	unsigned int mclk_rate;
	unsigned int tx_dai_fmt;
	unsigned int rx_dai_fmt;
	unsigned int is_tx_master;
	unsigned int is_rx_master;
	unsigned int output_signal;
	unsigned int enable_gpio;
	unsigned int rst_gpio;
};
static int fac_test;
static int is_need_reset_dsp;

static ssize_t show_reset_dsp(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("cx33970 show reset dsp = %d\n", is_need_reset_dsp);
	return sprintf(buf, "%u\n", is_need_reset_dsp);
}

static ssize_t store_reset_dsp(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t size)
{
	int ret;
	int val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	is_need_reset_dsp = val;

	if (is_need_reset_dsp) {
		pr_info("cx33970 is_need_reset_dsp = %d reset gpio pull up\n",
					is_need_reset_dsp);
		gpio_set_value_cansleep(423, 1);
	} else {
		pr_info("cx33970 is_need_reset_dsp = %d reset gpio pull down\n",
					is_need_reset_dsp);
		gpio_set_value_cansleep(423, 0);
	}
	return size;
}

static DEVICE_ATTR(reset_dsp, 0664, show_reset_dsp, store_reset_dsp);
/*
 * This functions takes cx33970_cmd structure as input and output parameters
 * to communicate CX33970. If operation is successfully, it returns number of
 * returned data and stored the returned data in "cmd->data" array.
 * Otherwise, it returns the error code.
 */
static int cx33970_sendcmd_ex(struct snd_soc_codec *codec,
			     struct cx33970_cmd *cmd)
{
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int num_32b_words = cmd->num_32b_words;
	unsigned long time_out;
	u32 *i2c_data = (u32 *)cmd;
	int size = num_32b_words + 2;

	/* calculate how many WORD that will be wrote to device*/
	cmd->num_32b_words = cmd->command_id & CX33970_CMD_GET(0) ?
			     CX33970_CMD_SIZE : num_32b_words;


	/* write all command data except fo frist 4 bytes*/
	ret = regmap_bulk_write(cx33970->regmap, 4, &i2c_data[1], size - 1);
	if (ret < 0) {
		dev_info(cx33970->dev, "Failed to write command data\n");
		goto LEAVE;
	}

	/* write first 4 bytes command data*/
	ret = regmap_bulk_write(cx33970->regmap, 0, i2c_data, 1);
	if (ret < 0) {
		dev_info(cx33970->dev, "Failed to write command\n");
		goto LEAVE;
	}

	/* continuously read the first bytes data from device until
	 * either timeout or the flag 'reply' is set.
	 */
	time_out = msecs_to_jiffies(2000);
	time_out += jiffies;
	do {
		regmap_bulk_read(cx33970->regmap, 0, &i2c_data[0], 1);
		if (cmd->reply == 1)
			break;
		mdelay(10);

	} while (!time_after(jiffies, time_out));

	if (cmd->reply == 1) {
		/* check if there is returned data. If yes copy the returned
		 * data to cmd->data array
		 */
		if (cmd->num_32b_words > 0)
			regmap_bulk_read(cx33970->regmap, 8, &i2c_data[2],
					 cmd->num_32b_words);
		/* return error code if operation is not successful.*/
		else if (cmd->num_32b_words < 0)
			dev_info(cx33970->dev, "SendCmd failed, err = %d\n",
				cmd->num_32b_words);

		ret = cmd->num_32b_words;
	} else {
		dev_info(cx33970->dev, "SendCmd timeout\n");
		ret = -EBUSY;
	}

LEAVE:
	return ret;
}

/*
 * cx33970_sendcmd: set/get cx33970's related configurations
 * @code : pointer variable to codec device
 * @cmd : cx33970 specified command format
 * @Variable-length argument :
 *         first parameter is command_id
 *         second parameter is app_module_id
 *         third parameter is number of data[] count
 *         following parameters are for data[]
 */
static int cx33970_sendcmd(struct snd_soc_codec *codec,
			     struct cx33970_cmd *cmd, ...)
{
	struct device *dev = codec->dev;
	va_list argp;
	int count, i;
	int ret = 0;

	va_start(argp, cmd);
	cmd->command_id = va_arg(argp, int);
	cmd->app_module_id = va_arg(argp, int);

	count = va_arg(argp, int);
	for (i = 0; i < count; i++)
		cmd->data[i] = va_arg(argp, int);
	va_end(argp);

	cmd->num_32b_words = count;
	cmd->reply = 0;

	ret = cx33970_sendcmd_ex(codec, cmd);
	if (ret < 0)
		dev_info(dev, "Failed to set %d, count = %d, ret = %d\n",
			cmd->command_id, count, ret);

	return ret;
}

static int cmd_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct cx33970_cmd);

	return 0;
}

static int cmd_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct cx33970_priv *cx33970 =
		snd_soc_component_get_drvdata(component);

	memcpy(ucontrol->value.bytes.data, &cx33970->cmd,
	       sizeof(cx33970->cmd));

	return 0;
}

static int cmd_put(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct cx33970_priv *cx33970 = snd_soc_component_get_drvdata(component);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);

	memcpy(&cx33970->cmd, ucontrol->value.bytes.data,
	       sizeof(cx33970->cmd));

	cx33970->cmd_res = cx33970_sendcmd_ex(codec, &cx33970->cmd);

	if (cx33970->cmd_res < 0)
		dev_info(codec->dev, "Failed to send cmd, ret = %d\n",
			cx33970->cmd_res);

	return cx33970->cmd_res < 0 ? cx33970->cmd_res : 0;
}


static int mode_info(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 4;

	return 0;
}

static int mode_get(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	int ret = 0;
	struct cx33970_cmd cmd;

	ret = cx33970_sendcmd(codec, &cmd, CONTROL_APP_GET_MODE,
			      CX33970_ID('S', 'O', 'S', ' '),
			      1,
			      CX33970_ID('C', 'T', 'R', 'L'));
	if (ret <= 0)
		dev_info(codec->dev, "Failed to get current mode, ret = %d\n",
			ret);
	else {
		ucontrol->value.bytes.data[0] = CX33970_ID2CH_A(cmd.data[0]);
		ucontrol->value.bytes.data[1] = CX33970_ID2CH_B(cmd.data[0]);
		ucontrol->value.bytes.data[2] = CX33970_ID2CH_C(cmd.data[0]);
		ucontrol->value.bytes.data[3] = CX33970_ID2CH_D(cmd.data[0]);

		dev_info(codec->dev, "Current mode = %c%c%c%c\n",
			ucontrol->value.bytes.data[0],
			ucontrol->value.bytes.data[1],
			ucontrol->value.bytes.data[2],
			ucontrol->value.bytes.data[3]);

		ret = 0;
	}

	return ret;
}

static int mode_put(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct cx33970_cmd cmd;
	int ret = -1;

	ret = cx33970_sendcmd(codec, &cmd, 4,
			      CX33970_ID_ARM,
			      1,
			      CX33970_ID(ucontrol->value.bytes.data[0],
					      ucontrol->value.bytes.data[1],
					      ucontrol->value.bytes.data[2],
					      ucontrol->value.bytes.data[3]));
	if (ret < 0)
		dev_info(codec->dev, "Failed to set mode, ret =%d\n", ret);
	else
		dev_info(codec->dev, "Set mode successfully, ret = %d\n", ret);

	return ret;
}
#if 0
static int cx33970_set_mode(struct snd_soc_codec *codec)
{
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	unsigned char *cmd_data = cx33970->current_mode;
	struct cx33970_cmd cmd;
	int ret = 0;

	ret = cx33970_sendcmd(codec, &cmd, 4,
			      CX33970_ID_ARM,
			      1,
			      CX33970_ID(cmd_data[0],
					      cmd_data[1],
					      cmd_data[2],
					      cmd_data[3]));
	if (ret < 0)
		dev_info(dev, "Failed to set mode, ret =%d\n", ret);
	else
		dev_info(dev, "Set mode successfully, ret = %d\n", ret);

	return ret;
}
#endif
static int cx33970_get_output_signals(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct cx33970_priv *cx33970 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
			(cx33970->output_signal & 0x300) >> 8;

	return 0;
}

static int cx33970_set_output_signals(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct cx33970_priv *cx33970 = snd_soc_component_get_drvdata(component);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct cx33970_cmd cmd;
	int ret = 0, select = 0;

	select = (ucontrol->value.integer.value[0] << 8) | 0xc0;
	if (select == cx33970->output_signal)
		return 0;

	ret = cx33970_sendcmd(codec, &cmd, CHANNEL_MIXER_CMD_CONFIG,
			      CX33970_ID('C', 'A', 'P', 'T') | 0x2c,
			      2, select, select + 1);
	if (ret < 0) {
		dev_info(codec->dev,
			"Failed to set output signal, ret =%d\n", ret);
	} else {
		dev_info(codec->dev,
			"Set output signal(0x%02x) successfully, ret = %d\n",
			select, ret);
		cx33970->output_signal = select;
	}

	return ret;
}

static int fac_test_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);

	dev_info(codec->dev,
		"%s fac_test = %d\n", __func__, fac_test);
	ucontrol->value.integer.value[0] = fac_test;

	return 0;
}

static int fac_test_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct cx33970_priv *cx33970 = snd_soc_component_get_drvdata(component);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	int ret = 0;
	struct device *dev = codec->dev;
	struct cx33970_cmd cmd;
	unsigned int *pb_cmd = (unsigned int *)&cx33970->cmd;

	fac_test = ucontrol->value.integer.value[0];
	dev_info(codec->dev, "%s fac_test = %d\n", __func__, fac_test);
	switch (fac_test) {
	case FAC_MIC1_RECORD:
		pb_cmd[0] = 0x00400002;
		pb_cmd[1] = 0x7a385c2c;
		pb_cmd[2] = 0x000001c0;
		pb_cmd[3] = 0x000001c0;

		break;
	case FAC_MIC2_RECORD:
		pb_cmd[0] = 0x00400002;
		pb_cmd[1] = 0x7a385c2c;
		pb_cmd[2] = 0x000001c1;
		pb_cmd[3] = 0x000001c1;

		break;
	case FAC_ECHO_REF:
		pb_cmd[0] = 0x00400002;
		pb_cmd[1] = 0x7a385c2c;
		pb_cmd[2] = 0x000002c0;
		pb_cmd[3] = 0x000002c1;

		break;
	case FAC_STOP_RECORD:
	    ret = cx33970_sendcmd(codec, &cmd,
						CX33970_CMD_SET(SYS_CMD_EVENT_PARAM),
						CX33970_ID_ARM, 3,
						EVENT_USB_RECORD_STARTSTOP,
						EVENT_PAR_USB_RECORD_STATE, 0);
		if (ret < 0)
			dev_info(dev, "Failed to stop record, error code %d\n", ret);
		else
			dev_info(dev, "Success to stop record\n");
		break;
	default:
		dev_info(codec->dev, "%s Unsupported cmd\n", __func__);
		return -EINVAL;
	}
	cx33970->cmd_res = cx33970_sendcmd_ex(codec, &cx33970->cmd);
	if (ret < 0) {
		pr_info("33971 Failed to start record, error code %d\n", ret);
		ret = -EINVAL;
	} else {
		pr_info("33971 Success to start record\n", ret);
	}
	return ret;
}
#if 0
static int cx33970_update_setting(struct snd_soc_codec *codec,
				 unsigned int fmt, unsigned int value)
{
	struct device *dev = codec->dev;
	struct cx33970_cmd cmd;
	int ret = 0;

	ret = cx33970_sendcmd(codec, &cmd,
			      CX33970_SYS_CMD_SET_PARAMETER_VALUE,
			      CX33970_ID_ARM,
			      2, fmt, value);
	if (ret < 0) {
		dev_info(dev, "Failed to set %d, ret = %d\n",
			fmt, ret);
		ret = -EINVAL;
	} else
		dev_info(dev, "Success to set %d, ret = %d\n",
			fmt, ret);

	return ret;
}
#endif
static int cx33970_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				 unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct device *dev = codec->dev;
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);

	dev_info(dev, "%s-\n", __func__);
	cx33970->mclk_rate = freq;

	return 0;
}

static int cx33970_set_dai_fmt(struct snd_soc_dai *dai,
			      unsigned int fmt)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	bool inv_fs = false;

	dev_info(dev, "%s- %08x, dai->id = %d\n", __func__, fmt, dai->id);
	/* set master/slave */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		if (dai->id == CX33970_DAI_DSP)
			cx33970->is_rx_master = 0;
		else
			cx33970->is_tx_master = 0;
		break;
	default:
		dev_info(dev, "Unsupported DAI master mode\n");
		return -EINVAL;
	}

	/* set format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if (dai->id == CX33970_DAI_DSP) {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_DELAY, 1);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_JUSTIFY, 0);
		} else {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_DELAY, 1);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_JUSTIFY, 0);
		}
		inv_fs = 1;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		if (dai->id == CX33970_DAI_DSP) {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_DELAY, 0);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_JUSTIFY, 1);
		} else {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_DELAY, 0);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_JUSTIFY, 1);
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		if (dai->id == CX33970_DAI_DSP) {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_DELAY, 0);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_JUSTIFY, 0);
		} else {
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_DELAY, 0);
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_JUSTIFY, 0);
		}
		break;
	default:
		dev_info(dev, "Unsupported DAI format\n");
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_IB_NF:
		if (dai->id == CX33970_DAI_DSP)
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_LR_POLARITY,
					       inv_fs ? 0:1);
		else
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_LR_POLARITY,
					       inv_fs ? 0:1);
		break;
	case SND_SOC_DAIFMT_IB_IF:
	case SND_SOC_DAIFMT_NB_IF:
		if (dai->id == CX33970_DAI_DSP)
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_RX_LR_POLARITY,
					       inv_fs ? 1:0);
		else
			cx33970_update_setting(codec,
					       PAR_INDEX_I2S_TX_LR_POLARITY,
					       inv_fs ? 1:0);
		break;
	default:
		dev_info(dev, "Unsupported Inersion format\n");
		return -EINVAL;
	}

	if (dai->id == CX33970_DAI_DSP)
		cx33970->rx_dai_fmt = fmt;
	else
		cx33970->tx_dai_fmt = fmt;
	cx33970_set_mode(codec);
#endif
	return 0;
}

static int cx33970_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	//struct snd_soc_codec *codec = dai->codec;
	//struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	//struct device *dev = codec->dev;
	//const unsigned int sample_rate = params_rate(params);
	//unsigned int sample_size, frame_size;
	int ret = 0;
	//struct cx33970_cmd cmd;
#if 0
	if (dai->id == CX33970_DAI_DSP)
		ret = cx33970_set_dai_fmt(dai, cx33970->rx_dai_fmt);
	else
		ret = cx33970_set_dai_fmt(dai, cx33970->tx_dai_fmt);
	if (ret)
		return ret;

	dev_info(dev, "Set hw_params, dai->id = %d\n", dai->id);
	/* Data sizes if not using TDM */

	sample_size = params_width(params);

	if (sample_size < 0)
		return sample_size;

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0)
		return frame_size;

	dev_info(dev, "Sample size %d bits, frame = %d bits, rate = %d Hz\n",
		sample_size, frame_size, sample_rate);

	if (dai->id == CX33970_DAI_DSP) {
		if (cx33970->is_rx_master) {
			cx33970_update_setting(codec,
					       EVENT_PAR_RATE_EXTRA_INPUT,
					       sample_rate);
		}
		cx33970_update_setting(codec, PAR_INDEX_I2S_RX_WIDTH,
				       (sample_size/8-1));
		cx33970_update_setting(codec, PAR_INDEX_I2S_RX_NUM_OF_BITS,
				       frame_size);
	} else {
		if (cx33970->is_tx_master) {
			cx33970_update_setting(codec,
					       EVENT_PAR_RATE_MAIN_OUTPUT,
					       sample_rate);
		}
		cx33970_update_setting(codec, PAR_INDEX_I2S_TX_WIDTH,
				       (sample_size/8-1));
		cx33970_update_setting(codec, PAR_INDEX_I2S_TX_NUM_OF_BITS,
				       frame_size);
	}

	ret = cx33970_set_mode(codec);

	if (((cx33970->output_signal & 0x300) >> 8) != 0)
		cx33970_sendcmd(codec, &cmd, CHANNEL_MIXER_CMD_CONFIG,
				CX33970_ID('C', 'A', 'P', 'T') | 0x2c,
				2, cx33970->output_signal,
				cx33970->output_signal + 1);
#endif
	return ret;
}

static int cx33970_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	//struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	int ret = 0;
	struct cx33970_cmd cmd;

	dev_info(dev, "%s enter\n", __func__);

	//Don't reset Tahiti if it has no dedicated SPI flash,
	//or else it will lose firmware code in RAM
	//cx33970_reset(codec);

//add by wyf begain
	ret = cx33970_sendcmd(codec, &cmd,
			      CX33970_CMD_GET(SYS_CMD_VERSION),
			      CX33970_ID_ARM, 0);
	if (ret > 0) {
		dev_info(codec->dev, "Firmware version = %d.%d.%d.%d\n",
			 cmd.data[0], cmd.data[1],
			 cmd.data[2], cmd.data[3]);
	} else {
		dev_info(codec->dev, "Failed to get firmware version, ret =%d\n",
			ret);
		return  0;
	}
//add by wyf end

	/*I2S format: sample rate 48k, sample size 16bit, framesize 32bit*/
	ret = cx33970_sendcmd(codec, &cmd,
					CX33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					CX33970_ID_ARM, 9,
					EVENT_USB_RECORD_STARTSTOP,
					EVENT_PAR_USB_RECORD_STATE, 1,
					EVENT_PAR_RATE_HOST_RECORD, 48000,
					PAR_INDEX_I2S_RX_WIDTH, 1,
					PAR_INDEX_I2S_RX_NUM_OF_BITS, 64);

	if (ret < 0) {
		dev_info(dev, "Failed to start record, error code %d\n", ret);
		ret = -EINVAL;
	} else {
		dev_info(dev, "Success to start record\n");
	}

	return ret;
}

static void cx33970_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	//struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;
	int ret = 0;
	struct cx33970_cmd cmd;

	dev_info(dev, "%s enter\n", __func__);
	ret = cx33970_sendcmd(codec, &cmd,
					CX33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					CX33970_ID_ARM, 3,
					EVENT_USB_RECORD_STARTSTOP,
					EVENT_PAR_USB_RECORD_STATE, 0);
	if (ret < 0)
		dev_info(dev, "Failed to stop record, error code %d\n", ret);
	else
		dev_info(dev, "Success to stop record\n");
}

static const char * const cx33970_output_signals[] = {
		"Processed", "MIC In", "Echo REF"};

static const struct soc_enum cx33970_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cx33970_output_signals),
			    cx33970_output_signals),
};

static const char * const cx33970_fac_test[] = {
		"Off", "MIC1", "MIC2", "Enable_Echo REF"};

static const struct soc_enum fac_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cx33970_fac_test),
			    cx33970_fac_test),
};

static const struct snd_kcontrol_new cx33970_snd_controls[] = {
	CX33970_CONTROL("SendCmd", cmd_info, cmd_get, cmd_put,
			SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE),
	CX33970_CONTROL("Mode", mode_info, mode_get, mode_put,
			SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE),
	SOC_ENUM_EXT("Output Signals", cx33970_enum[0],
		     cx33970_get_output_signals, cx33970_set_output_signals),
	SOC_ENUM_EXT("Dsp_Fac_Test", fac_test_enum[0],
			fac_test_get, fac_test_set),
};

static const struct snd_soc_dapm_widget cx33970_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("Mic AIF", "Capture", 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AEC AIF", NULL, 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_OUTPUT("AEC DATA"),
};

static const struct snd_soc_dapm_route cx33970_intercon[] = {
	{"Mic AIF", NULL, "MIC"},
	{"AEC AIF", NULL, "Playback"},
	{"AEC DATA", NULL, "AEC AIF"},
};

static struct snd_soc_dai_ops cx33970_dai_ops = {
	.set_sysclk = cx33970_set_dai_sysclk,
	.set_fmt = cx33970_set_dai_fmt,
	.hw_params = cx33970_hw_params,
	.startup = cx33970_startup,
	.shutdown = cx33970_shutdown,
};

static struct snd_soc_dai_driver soc_codec_cx33970_dai[] = {
	{
		.name = "cx33970-aif",
		.id = 0,
		.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = CX33970_FORMATS,
		},
		.ops = &cx33970_dai_ops,
	},
#if 0
	{
		.name = "cx33970-dsp",
		.id = CX33970_DAI_DSP,
		.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = CX33970_FORMATS,
		},
		.ops = &cx33970_dai_ops,
	},
#endif
};
#if 0
static int cx33970_reset(struct snd_soc_codec *codec)
{
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;
	int ret = 0;
	unsigned long time_out;

	if (cx33970->gpiod_reset) {
		gpiod_set_value_cansleep(cx33970->gpiod_reset, 0);
		msleep(100);
		gpiod_set_value_cansleep(cx33970->gpiod_reset, 1);
		msleep(500);
		devm_gpiod_put(cx33970->dev, cx33970->gpiod_reset);

		/* continuously read the first bytes data from device until
		 * either timeout or the device ready.
		 */
		time_out = msecs_to_jiffies(1000);
		time_out += jiffies;
		do {
			ret = regmap_bulk_read(cx33970->regmap, 4, &val, 1);
			if (val == CX33970_READY)
				break;
			msleep(20);

		} while (!time_after(jiffies, time_out));

		if (val != CX33970_READY) {
			dev_info(codec->dev, "\nFailed to reset cx33970!\n");
			ret =  -ENODEV;
		}
	}

	return ret;
}
#endif
static int cx33970_probe(struct snd_soc_codec *codec)
{
#if 0
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);
	unsigned char cur_mode[4];
	struct cx33970_cmd cmd, fw_cmd;

	int ret = 0;

	//Don't reset Tahiti if it has no dedicated SPI flash,
	//or else it will lose firmware code in RAM
	//cx33970_reset(codec);
//add by wyf
	ret = cx33970_sendcmd(codec, &fw_cmd,
			      CX33970_CMD_GET(SYS_CMD_VERSION),
			      CX33970_ID_ARM, 0);
	if (ret > 0) {
		dev_info(codec->dev, "Firmware version = %d.%d.%d.%d\n",
			 fw_cmd.data[0], fw_cmd.data[1],
			 fw_cmd.data[2], fw_cmd.data[3]);
	} else {
		dev_info(codec->dev, "Failed to get firmware version, ret =%d\n",
			ret);
		return  -EIO;
	}

#endif
	return 0;
}

static int cx33970_remove(struct snd_soc_codec *codec)
{
	struct cx33970_priv *cx33970 = snd_soc_codec_get_drvdata(codec);

	if (cx33970->gpiod_reset) {
		/* Set codec to the reset state */
		gpiod_set_value_cansleep(cx33970->gpiod_reset, 0);
	}
	return 0;
}

static const struct snd_soc_codec_driver soc_codec_driver_cx33970 = {
	.probe = cx33970_probe,
	.remove = cx33970_remove,
	.component_driver = {
		.controls = cx33970_snd_controls,
		.num_controls = ARRAY_SIZE(cx33970_snd_controls),
		.dapm_widgets = cx33970_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(cx33970_dapm_widgets),
		.dapm_routes = cx33970_intercon,
		.num_dapm_routes = ARRAY_SIZE(cx33970_intercon),
	},
};

static bool cx33970_volatile_register(struct device *dev, unsigned int reg)
{
	return true; /*all register are volatile*/
}

const struct regmap_config cx33970_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = CX33970_REG_MAX,
	.cache_type = REGCACHE_NONE,
	.volatile_reg = cx33970_volatile_register,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};
EXPORT_SYMBOL_GPL(cx33970_regmap_config);

static int cx33970_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct cx33970_priv *cx33970;
	int ret;
	int ret_device_file = 0;
	struct device *dev = &i2c->dev;
	struct device_node *np = i2c->dev.of_node;
	struct regmap *regmap;

	dev_info(dev, "%s: enter.\n", __func__);
	regmap = devm_regmap_init_i2c(i2c, &cx33970_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	cx33970 = devm_kzalloc(dev, sizeof(*cx33970), GFP_KERNEL);
	if (!cx33970)
		return -ENOMEM;

	cx33970->enable_gpio = of_get_named_gpio(np, "enable-gpio", 0);
	if (cx33970->enable_gpio < 0)
		dev_info(dev, "cx33970 get enable-gpio failed\n");

	if (gpio_is_valid(cx33970->enable_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, cx33970->enable_gpio,
			GPIOF_OUT_INIT_HIGH, "CX33970_ENABLE_GPIO");
		if (ret)
			return ret;
	}
	dev_info(dev, "%s: enable_gpio = %d.\n", __func__, cx33970->enable_gpio);
	gpio_set_value_cansleep(cx33970->enable_gpio, 1);

////////////////////
	cx33970->rst_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (cx33970->rst_gpio < 0)
		dev_info(dev, "cx33970 get reset-gpios failed\n");

	if (gpio_is_valid(cx33970->rst_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, cx33970->rst_gpio,
			GPIOF_OUT_INIT_HIGH, "CX33970_RST_GPIO");
		if (ret)
			return ret;
	}
	dev_info(dev, "%s: rst_gpio = %d.\n", __func__, cx33970->rst_gpio);
////////////////////
	ret_device_file = device_create_file(dev, &dev_attr_reset_dsp);
//Disable Tahiti reset since it has no dedicated flash
#if 0
	/* GPIOs */
	cx33970->gpiod_reset = devm_gpiod_get_optional(dev, "reset",
						       GPIOD_OUT_HIGH);
	dev_info(dev, "%s: gpiod_reset = %d.\n", __func__, cx33970->gpiod_reset);
	if (IS_ERR(cx33970->gpiod_reset))
		return PTR_ERR(cx33970->gpiod_reset);
#endif

	dev_set_drvdata(dev, cx33970);
	cx33970->regmap = regmap;
	cx33970->dev = dev;

	ret = snd_soc_register_codec(cx33970->dev, &soc_codec_driver_cx33970,
				     soc_codec_cx33970_dai,
				     ARRAY_SIZE(soc_codec_cx33970_dai));
	if (ret < 0)
		dev_info(dev, "Failed to register codec: %d\n", ret);
	else
		dev_info(dev, "%s: Register codec.\n", __func__);

	return ret;
}

static int cx33970_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

const struct of_device_id cx33970_dt_ids[] = {
	{ .compatible = "syna,cx33970", },
	{ }
};
MODULE_DEVICE_TABLE(of, cx33970_dt_ids);

static const struct i2c_device_id cx33970_i2c_id[] = {
	{"cx33970", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cx33970_i2c_id);

static struct i2c_driver cx33970_i2c_driver = {
	.driver = {
		.name = "cx33970",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cx33970_dt_ids),
	},
	.id_table = cx33970_i2c_id,
	.probe = cx33970_i2c_probe,
	.remove = cx33970_i2c_remove,
};

module_i2c_driver(cx33970_i2c_driver);

MODULE_DESCRIPTION("ASoC CX33970 ALSA SoC Driver");
MODULE_AUTHOR("Rui Qiao <Rui.Qiao@Synaptics.com>");
MODULE_LICENSE("GPL");
