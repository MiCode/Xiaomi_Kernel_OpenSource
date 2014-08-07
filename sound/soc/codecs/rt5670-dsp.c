/*
 * rt5670-dsp.c  --  RT5670 ALSA SoC DSP driver
 *
 * Copyright 2011 Realtek Semiconductor Corp.
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#define RTK_IOCTL
#ifdef RTK_IOCTL
#include <linux/spi/spi.h>
#include "rt_codec_ioctl.h"
#endif

#include "rt5670.h"
#include "rt5670-dsp.h"

#define DSP_CLK_RATE RT5670_DSP_CLK_96K

/* DSP init */
static unsigned short rt5670_dsp_init[][2] = {
	{0x2260, 0x30d9}, {0x2261, 0x30d9}, {0x2289, 0x7fff}, {0x2290, 0x7fff},
	{0x2288, 0x0002}, {0x22b2, 0x0002}, {0x2295, 0x0001}, {0x22b3, 0x0001},
	{0x22d7, 0x0008}, {0x22d8, 0x0009}, {0x22d9, 0x0000}, {0x22da, 0x0001},
	{0x22fd, 0x009e}, {0x22c1, 0x1006}, {0x22c2, 0x1006}, {0x22c3, 0x1007},
	{0x22c4, 0x1007}
};
#define RT5670_DSP_INIT_NUM  ARRAY_SIZE(rt5670_dsp_init)

/* MCLK rate */
static unsigned short rt5670_dsp_4096000[][2] = {
	{0x226c, 0x000c}, {0x226d, 0x000c}, {0x226e, 0x0022},
};
#define RT5670_DSP_4096000_NUM  ARRAY_SIZE(rt5670_dsp_4096000)

static unsigned short rt5670_dsp_12288000[][2] = {
	{0x226c, 0x000c}, {0x226d, 0x000c}, {0x226e, 0x0026},
};
#define RT5670_DSP_12288000_NUM ARRAY_SIZE(rt5670_dsp_12288000)

static unsigned short rt5670_dsp_11289600[][2] = {
	{0x226c, 0x0031}, {0x226d, 0x0050}, {0x226e, 0x0009},
};
#define RT5670_DSP_11289600_NUM ARRAY_SIZE(rt5670_dsp_11289600)

static unsigned short rt5670_dsp_24576000[][2] = {
	{0x226c, 0x000c}, {0x226d, 0x000c}, {0x226e, 0x002c},
};
#define RT5670_DSP_24576000_NUM ARRAY_SIZE(rt5670_dsp_24576000)

/* sample rate */
static unsigned short rt5670_dsp_48_441[][2] = {
	{0x22f2, 0x0058}, {0x2301, 0x0016}
};
#define RT5670_DSP_48_441_NUM ARRAY_SIZE(rt5670_dsp_48_441)

static unsigned short rt5670_dsp_24[][2] = {
	{0x22f2, 0x0058}, {0x2301, 0x0004}
};
#define RT5670_DSP_24_NUM ARRAY_SIZE(rt5670_dsp_24)

static unsigned short rt5670_dsp_16[][2] = {
	{0x22f2, 0x004c}, {0x2301, 0x0002}
};
#define RT5670_DSP_16_NUM ARRAY_SIZE(rt5670_dsp_16)

static unsigned short rt5670_dsp_8[][2] = {
	{0x22f2, 0x004c}, {0x2301, 0x0000}
};
#define RT5670_DSP_8_NUM ARRAY_SIZE(rt5670_dsp_8)

/* DSP mode */
static unsigned short rt5670_dsp_ns[][2] = {
	{0x22f8, 0x8005}, {0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332},
	{0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238a, 0x0000}, {0x238b, 0x1000},
	{0x238c, 0x1000}, {0x23a1, 0x2000}, {0x2303, 0x0200}, {0x2304, 0x0032},
	{0x2305, 0x0000}, {0x230c, 0x0200}, {0x22fb, 0x0000}
};
#define RT5670_DSP_NS_NUM ARRAY_SIZE(rt5670_dsp_ns)

static unsigned short rt5670_dsp_aec[][2] = {
	{0x22f8, 0x8003}, {0x232f, 0x00d0}, {0x2355, 0x2666}, {0x2356, 0x2666},
	{0x2357, 0x2666}, {0x2358, 0x6666}, {0x2359, 0x6666}, {0x235a, 0x6666},
	{0x235b, 0x7fff}, {0x235c, 0x7fff}, {0x235d, 0x7fff}, {0x235e, 0x7fff},
	{0x235f, 0x7fff}, {0x2360, 0x7fff}, {0x2361, 0x7fff}, {0x2362, 0x1000},
	{0x2367, 0x0007}, {0x2368, 0x4000}, {0x2369, 0x0008}, {0x236a, 0x2000},
	{0x236b, 0x0009}, {0x236c, 0x003c}, {0x236d, 0x0000}, {0x236f, 0x2000},
	{0x2370, 0x0008}, {0x2371, 0x000a}, {0x2373, 0x0000}, {0x2374, 0x7fff},
	{0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332}, {0x2379, 0x1000},
	{0x237a, 0x1000}, {0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238a, 0x4000},
	{0x238b, 0x1000}, {0x238c, 0x1000}, {0x2398, 0x4668}, {0x23a1, 0x2000},
	{0x23a3, 0x4000}, {0x23ad, 0x2000}, {0x23ae, 0x2000}, {0x23af, 0x2000},
	{0x23b4, 0x2000}, {0x23b5, 0x2000}, {0x23b6, 0x2000}, {0x23bb, 0x6000},
	{0x2303, 0x0710}, {0x2304, 0x0332}, {0x230c, 0x0200}, {0x230d, 0x0080},
	{0x2310, 0x0010}, {0x22fb, 0x0000}
};
#define RT5670_DSP_AEC_NUM ARRAY_SIZE(rt5670_dsp_aec)

static unsigned short rt5670_dsp_vt[][2] = {
	{0x22f8, 0x8003}, {0x2371, 0x000a}, {0x2373, 0x0000}, {0x2374, 0x7fff},
	{0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332}, {0x2379, 0x1000},
	{0x237a, 0x1000}, {0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238b, 0x1000},
	{0x238c, 0x1000}, {0x23a1, 0x2000}, {0x2304, 0x0332}, {0x230c, 0x0200},
	{0x22fb, 0x0000}
};
#define RT5670_DSP_VT_NUM ARRAY_SIZE(rt5670_dsp_vt)

static unsigned short rt5670_dsp_vr[][2] = {
	{0x22f8, 0x8003}, {0x2304, 0x0332}, {0x2305, 0x0000}, {0x2309, 0x0400},
	{0x230c, 0x0200}, {0x230d, 0x0080}, {0x2310, 0x0004}, {0x232f, 0x0100},
	{0x2371, 0x000a}, {0x2373, 0x3000}, {0x2374, 0x5000}, {0x2375, 0x7ff0},
	{0x2376, 0x7990}, {0x2377, 0x7332}, {0x2379, 0x1000}, {0x237a, 0x1000},
	{0x2386, 0x0200}, {0x2388, 0x4000}, {0x2389, 0x4000}, {0x238a, 0x0000},
	{0x238b, 0x2000}, {0x238c, 0x1800}, {0x239b, 0x0000}, {0x239c, 0x0a00},
	{0x239d, 0x0000}, {0x239e, 0x7fff}, {0x239f, 0x0001}, {0x23a1, 0x3000},
	{0x23a2, 0x1000}, {0x23ad, 0x0000}, {0x23ae, 0x0000}, {0x23af, 0x0000},
	{0x23c4, 0x2000}, {0x23b0, 0x0000}, {0x23b1, 0x0000}, {0x23b2, 0x0000},
	{0x23b3, 0x0000}, {0x23b4, 0x0000}, {0x23b5, 0x0000}, {0x23b6, 0x0000},
	{0x23b7, 0x0000}, {0x23b8, 0x0000}, {0x23b9, 0x0000}, {0x23ba, 0x0000},
	{0x23bb, 0x0000}, {0x22fb, 0x0000}
};
#define RT5670_DSP_VR_NUM ARRAY_SIZE(rt5670_dsp_vr)

static unsigned short rt5670_dsp_ffp_ns[][2] = {
	{0x22f8, 0x8003}, {0x2304, 0x8332}, {0x2371, 0x000a}, {0x2373, 0x0000},
	{0x2374, 0x7fff}, {0x2379, 0x1800}, {0x237a, 0x1800}, {0x230c, 0x0200},
	{0x23a2, 0x1000}, {0x2388, 0x7000}, {0x238b, 0x2000}, {0x238c, 0x2000},
	{0x23a8, 0x2000}, {0x23a9, 0x4000}, {0x23aa, 0x0100}, {0x23ab, 0x7800},
	{0x22fb, 0x0000}
};
#define RT5670_DSP_FFP_NS_NUM ARRAY_SIZE(rt5670_dsp_ffp_ns)

static unsigned short rt5670_dsp_48k_sto_ffp[][2] = {
	{0x22c1, 0x1025}, {0x22c2, 0x1026}, {0x22f8, 0x8004}, {0x22ea, 0x0001},
	{0x230c, 0x0100}, {0x230d, 0x0100}, {0x2301, 0x0010}, {0x2303, 0x0200},
	{0x2304, 0x8000}, {0x2305, 0x0000}, {0x2388, 0x6500}, {0x238b, 0x4000},
	{0x238c, 0x4000}, {0x23a9, 0x2000}, {0x23aa, 0x0200}, {0x23ab, 0x7c00},
	{0x22fb, 0x0000}
};
#define RT5670_DSP_48K_STO_FFP_NUM ARRAY_SIZE(rt5670_dsp_48k_sto_ffp)

static unsigned short rt5670_dsp_2mic_handset[][2] = {
	{0x22f8, 0x8002}, {0x2301, 0x0002}, {0x2302, 0x0002}, {0x2303, 0x0710},
	{0x2304, 0x4332}, {0x2305, 0x206c}, {0x236e, 0x0000}, {0x236f, 0x0001},
	{0x237e, 0x0001}, {0x237f, 0x3800}, {0x2380, 0x3000}, {0x2381, 0x0005},
	{0x2382, 0x0040}, {0x2383, 0x7fff}, {0x2388, 0x2c00}, {0x2389, 0x2800},
	{0x238b, 0x1800}, {0x238c, 0x1800}, {0x238f, 0x2000}, {0x239b, 0x0002},
	{0x239c, 0x0a00}, {0x239f, 0x0001}, {0x230c, 0x0200}, {0x22fb, 0x0000}
};
#define RT5670_DSP_2MIC_HANDSET_NUM ARRAY_SIZE(rt5670_dsp_2mic_handset)

static unsigned short rt5670_dsp_2mic_handsfree[][2] = {
	{0x22f8, 0x8003}, {0x2371, 0x000a}, {0x2373, 0x0000}, {0x2374, 0x7fff},
	{0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332}, {0x2379, 0x1000},
	{0x237a, 0x1000}, {0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238b, 0x1000},
	{0x238c, 0x1000}, {0x23a1, 0x2000}, {0x2304, 0x0332}, {0x230c, 0x0200},
	{0x22fb, 0x0000}
};
#define RT5670_DSP_2MIC_HANDSFREE_NUM ARRAY_SIZE(rt5670_dsp_2mic_handsfree)

static unsigned short rt5670_dsp_aec_handsfree[][2] = {
	{0x22f8, 0x8003}, {0x232f, 0x00d0}, {0x2355, 0x2666}, {0x2356, 0x2666},
	{0x2357, 0x2666}, {0x2358, 0x6666}, {0x2359, 0x6666}, {0x235a, 0x6666},
	{0x235b, 0x7fff}, {0x235c, 0x7fff}, {0x235d, 0x7fff}, {0x235e, 0x7fff},
	{0x235f, 0x7fff}, {0x2360, 0x7fff}, {0x2361, 0x7fff}, {0x2362, 0x1000},
	{0x2367, 0x0007}, {0x2368, 0x4000}, {0x2369, 0x0008}, {0x236a, 0x2000},
	{0x236b, 0x0009}, {0x236c, 0x003c}, {0x236d, 0x0000}, {0x236f, 0x2000},
	{0x2370, 0x0008}, {0x2371, 0x000a}, {0x2373, 0x0000}, {0x2374, 0x7fff},
	{0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332}, {0x2379, 0x1000},
	{0x237a, 0x1000}, {0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238a, 0x4000},
	{0x238b, 0x1000}, {0x238c, 0x1000}, {0x2398, 0x4668}, {0x23a1, 0x2000},
	{0x23a3, 0x4000}, {0x23ad, 0x2000}, {0x23ae, 0x2000}, {0x23af, 0x2000},
	{0x23b4, 0x2000}, {0x23b5, 0x2000}, {0x23b6, 0x2000}, {0x23bb, 0x6000},
	{0x2303, 0x0710}, {0x2304, 0x0332}, {0x230c, 0x0200}, {0x230d, 0x0080},
	{0x2310, 0x0010}, {0x22fb, 0x0000}
};
#define RT5670_DSP_AEC_HANDSFREE_NUM ARRAY_SIZE(rt5670_dsp_aec_handsfree)

/**
 * rt5670_dsp_done - Wait until DSP is ready.
 * @codec: SoC Audio Codec device.
 *
 * To check voice DSP status and confirm it's ready for next work.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_done(struct snd_soc_codec *codec)
{
	unsigned int count = 0, dsp_val;

	dsp_val = snd_soc_read(codec, RT5670_DSP_CTRL1);
	while (dsp_val & RT5670_DSP_BUSY_MASK) {
		if (count > 10)
			return -EBUSY;
		dsp_val = snd_soc_read(codec, RT5670_DSP_CTRL1);
		count++;
	}

	return 0;
}

/**
 * rt5670_dsp_write - Write DSP register.
 * @codec: SoC audio codec device.
 * @param: DSP parameters.
  *
 * Modify voice DSP register for sound effect. The DSP can be controlled
 * through DSP command format (0xfc), addr (0xc4), data (0xc5) and cmd (0xc6)
 * register. It has to wait until the DSP is ready.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_write(struct snd_soc_codec *codec,
		unsigned int addr, unsigned int data)
{
	unsigned int dsp_val;
	int ret;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, addr);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5670_DSP_CTRL3, data);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP data reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_I2C_AL_16 | RT5670_DSP_DL_2 |
		RT5670_DSP_CMD_MW | DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}
	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;
}

/**
 * rt5670_dsp_read - Read DSP register.
 * @codec: SoC audio codec device.
 * @reg: DSP register index.
 *
 * Read DSP setting value from voice DSP. The DSP can be controlled
 * through DSP addr (0xc4), data (0xc5) and cmd (0xc6) register. Each
 * command has to wait until the DSP is ready.
 *
 * Returns DSP register value or negative error code.
 */
static unsigned int rt5670_dsp_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int dsp_val;
	int ret = 0;

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_I2C_AL_16 | RT5670_DSP_DL_0 | RT5670_DSP_RW_MASK |
		RT5670_DSP_CMD_MR | DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, 0x26);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_DL_1 | RT5670_DSP_CMD_RR | RT5670_DSP_RW_MASK |
		DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, 0x25);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}

	dsp_val = RT5670_DSP_DL_1 | RT5670_DSP_CMD_RR | RT5670_DSP_RW_MASK |
		DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_read(codec, RT5670_DSP_CTRL5);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read DSP data reg: %d\n", ret);
		goto err;
	}

err:
	return ret;
}

static int rt5670_dsp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5670->dsp_sw;

	return 0;
}

static int rt5670_dsp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	if (rt5670->dsp_sw != ucontrol->value.integer.value[0])
		rt5670->dsp_sw = ucontrol->value.integer.value[0];

	return 0;
}

/* DSP Path Control 1 */
static const char * const rt5670_src_rxdp_mode[] = {
	"Normal", "Divided by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_src_rxdp_enum, RT5670_DSP_PATH1,
	RT5670_RXDP_SRC_SFT, rt5670_src_rxdp_mode);

static const char * const rt5670_src_txdp_mode[] = {
	"Normal", "Multiplied by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_src_txdp_enum, RT5670_DSP_PATH1,
	RT5670_TXDP_SRC_SFT, rt5670_src_txdp_mode);

/* Sound Effect */
static const char * const rt5670_dsp_mode[] = {
	"Disable", "NS", "AEC", "VT", "VR", "FFP+NS", "48K-stereo+FFP",
	"2MIC Handset", "2MIC Handsfree", "AEC Handsfree"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_dsp_enum, 0, 0,
	rt5670_dsp_mode);

static const struct snd_kcontrol_new rt5670_dsp_snd_controls[] = {
	/* AEC */
	SOC_ENUM_EXT("DSP Function Switch", rt5670_dsp_enum,
		rt5670_dsp_get, rt5670_dsp_put),
};

/**
 * rt5670_dsp_conf - Set DSP basic setting.
 *
 * @codec: SoC audio codec device.
 *
 * Set parameters of basic setting to DSP.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_conf(struct snd_soc_codec *codec)
{
	int ret = 0, i;

	for (i = 0; i < RT5670_DSP_INIT_NUM; i++) {
		ret = rt5670_dsp_write(codec, rt5670_dsp_init[i][0],
			rt5670_dsp_init[i][1]);
		if (ret < 0) {
			dev_err(codec->dev, "Fail to config Dsp: %d\n", ret);
			goto conf_err;
		}
	}

conf_err:

	return ret;
}

/**
 * rt5670_dsp_rate - Set DSP rate setting.
 *
 * @codec: SoC audio codec device.
 * @sys_clk: System clock.
 * @srate: Sampling rate.
 *
 * Set parameters of system clock and sampling rate to DSP.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_rate(struct snd_soc_codec *codec, int sys_clk,
	int srate)
{
	int ret, i, tab_num;
	unsigned short (*rate_tab)[2];

	dev_dbg(codec->dev,
		"rt5670_dsp_rate sys:%d srate:%d\n", sys_clk, srate);

	switch (sys_clk) {
	case 4096000:
		rate_tab = rt5670_dsp_4096000;
		tab_num = RT5670_DSP_4096000_NUM;
		break;
	case 11289600:
		rate_tab = rt5670_dsp_11289600;
		tab_num = RT5670_DSP_11289600_NUM;
		break;
	case 12288000:
		rate_tab = rt5670_dsp_12288000;
		tab_num = RT5670_DSP_12288000_NUM;
		break;
	case 24576000:
		rate_tab = rt5670_dsp_24576000;
		tab_num = RT5670_DSP_24576000_NUM;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < tab_num; i++) {
		ret = rt5670_dsp_write(codec, rate_tab[i][0], rate_tab[i][1]);
		if (ret < 0)
			goto rate_err;
	}

	switch (srate) {
	case 8000:
		rate_tab = rt5670_dsp_8;
		tab_num = RT5670_DSP_8_NUM;
		break;
	case 16000:
		rate_tab = rt5670_dsp_16;
		tab_num = RT5670_DSP_16_NUM;
		break;
	case 24000:
		rate_tab = rt5670_dsp_24;
		tab_num = RT5670_DSP_24_NUM;
		break;
	case 44100:
	case 48000:
		rate_tab = rt5670_dsp_48_441;
		tab_num = RT5670_DSP_48_441_NUM;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < tab_num; i++) {
		ret = rt5670_dsp_write(codec, rate_tab[i][0], rate_tab[i][1]);
		if (ret < 0)
			goto rate_err;
	}

	return 0;

rate_err:

	dev_err(codec->dev, "Fail to set rate parameters: %d\n", ret);
	return ret;
}

/**
 * rt5670_dsp_set_mode - Set DSP mode parameters.
 *
 * @codec: SoC audio codec device.
 * @mode: DSP mode.
 *
 * Set parameters of mode to DSP.
 * There are three modes which includes " mic AEC + NS + FENS",
 * "HFBF" and "Far-field pickup".
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_set_mode(struct snd_soc_codec *codec, int mode)
{
	int ret, i, tab_num;
	unsigned short (*mode_tab)[2];

	switch (mode) {
	case RT5670_DSP_NS:
		dev_info(codec->dev, "NS\n");
		mode_tab = rt5670_dsp_ns;
		tab_num = RT5670_DSP_NS_NUM;
		break;

	case RT5670_DSP_AEC:
		dev_info(codec->dev, "AEC\n");
		mode_tab = rt5670_dsp_aec;
		tab_num = RT5670_DSP_AEC_NUM;
		break;

	case RT5670_DSP_VT:
		dev_info(codec->dev, "VT\n");
		mode_tab = rt5670_dsp_vt;
		tab_num = RT5670_DSP_VT_NUM;
		break;

	case RT5670_DSP_VR:
		dev_info(codec->dev, "VR\n");
		mode_tab = rt5670_dsp_vr;
		tab_num = RT5670_DSP_VR_NUM;
		break;

	case RT5670_DSP_FFP_NS:
		dev_info(codec->dev, "FFP_NS\n");
		mode_tab = rt5670_dsp_ffp_ns;
		tab_num = RT5670_DSP_FFP_NS_NUM;
		break;

	case RT5670_DSP_48K_STO_FFP:
		dev_info(codec->dev, "48K_STO_FFP\n");
		mode_tab = rt5670_dsp_48k_sto_ffp;
		tab_num = RT5670_DSP_48K_STO_FFP_NUM;
		break;

	case RT5670_DSP_2MIC_HANDSET:
		dev_info(codec->dev, "2MIC_HANDSET\n");
		mode_tab = rt5670_dsp_2mic_handset;
		tab_num = RT5670_DSP_2MIC_HANDSET_NUM;
		break;

	case RT5670_DSP_2MIC_HANDSFREE:
		dev_info(codec->dev, "2MIC_HANDSFREE\n");
		mode_tab = rt5670_dsp_2mic_handsfree;
		tab_num = RT5670_DSP_2MIC_HANDSFREE_NUM;
		break;

	case RT5670_DSP_AEC_HANDSFREE:
		dev_info(codec->dev, "AEC_HANDSFREE\n");
		mode_tab = rt5670_dsp_aec_handsfree;
		tab_num = RT5670_DSP_AEC_HANDSFREE_NUM;
		break;

	case RT5670_DSP_DIS:
	default:
		dev_info(codec->dev, "Disable\n");
		return 0;
	}

	for (i = 0; i < tab_num; i++) {
		ret = rt5670_dsp_write(codec, mode_tab[i][0], mode_tab[i][1]);
		if (ret < 0)
			goto mode_err;
	}

	return 0;

mode_err:

	dev_err(codec->dev, "Fail to set mode %d parameters: %d\n", mode, ret);
	return ret;
}

/**
 * rt5670_dsp_snd_effect - Set DSP sound effect.
 *
 * Set parameters of sound effect to DSP.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_snd_effect(struct snd_soc_codec *codec)
{
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);
	int ret;

	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP,
		RT5670_RST_DSP);
	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP, 0);

	usleep_range(10000, 11000);
/*	Expend 1.6 seconds
	ret = rt5670_dsp_do_patch(codec);
	if (ret < 0)
		goto effect_err;
*/
	ret = rt5670_dsp_rate(codec,
		rt5670->sysclk ?
		rt5670->sysclk : 11289600,
		rt5670->lrck[RT5670_AIF2] ?
		rt5670->lrck[RT5670_AIF2] : 44100);
	if (ret < 0)
		goto effect_err;

	ret = rt5670_dsp_conf(codec);
	if (ret < 0)
		goto effect_err;

	ret = rt5670_dsp_set_mode(codec, rt5670->dsp_sw);
	if (ret < 0)
		goto effect_err;

	return 0;

effect_err:

	return ret;
}

static int rt5670_dsp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, "%s(): PMD\n", __func__);
		rt5670_dsp_write(codec, 0x22f9, 1);
		break;

	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, "%s(): PMU\n", __func__);
		rt5670_dsp_snd_effect(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt5670_dsp_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("Voice DSP", 1, SND_SOC_NOPM,
		0, 0, rt5670_dsp_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DSP Downstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP Upstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route rt5670_dsp_dapm_routes[] = {
	{"DSP Downstream", NULL, "Voice DSP"},
	{"DSP Downstream", NULL, "RxDP Mux"},
	{"DSP Upstream", NULL, "Voice DSP"},
	{"DSP Upstream", NULL, "8CH TDM Data"},
	{"DSP DL Mux", "DSP", "DSP Downstream"},
	{"DSP UL Mux", "DSP", "DSP Upstream"},
};

/**
 * rt5670_dsp_show - Dump DSP registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all DSP registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5670_dsp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned short (*rt5670_dsp_tab)[2];
	unsigned int val;
	int cnt = 0, i, tab_num;

	switch (rt5670->dsp_sw) {
	case RT5670_DSP_NS:
		cnt += sprintf(buf, "[ RT5642 DSP 'NS' ]\n");
		rt5670_dsp_tab = rt5670_dsp_ns;
		tab_num = RT5670_DSP_NS_NUM;
		break;

	case RT5670_DSP_AEC:
		cnt += sprintf(buf, "[ RT5642 DSP 'AEC' ]\n");
		rt5670_dsp_tab = rt5670_dsp_aec;
		tab_num = RT5670_DSP_AEC_NUM;
		break;

	case RT5670_DSP_VT:
		cnt += sprintf(buf, "[ RT5642 DSP 'VT' ]\n");
		rt5670_dsp_tab = rt5670_dsp_vt;
		tab_num = RT5670_DSP_VT_NUM;
		break;

	case RT5670_DSP_VR:
		cnt += sprintf(buf, "[ RT5642 DSP 'VR' ]\n");
		rt5670_dsp_tab = rt5670_dsp_vr;
		tab_num = RT5670_DSP_VR_NUM;
		break;

	case RT5670_DSP_FFP_NS:
		cnt += sprintf(buf, "[ RT5642 DSP 'FFP_NS' ]\n");
		rt5670_dsp_tab = rt5670_dsp_ffp_ns;
		tab_num = RT5670_DSP_FFP_NS_NUM;
		break;

	case RT5670_DSP_48K_STO_FFP:
		cnt += sprintf(buf, "[ RT5642 DSP '48K_STO_FFP' ]\n");
		rt5670_dsp_tab = rt5670_dsp_48k_sto_ffp;
		tab_num = RT5670_DSP_48K_STO_FFP_NUM;
		break;

	case RT5670_DSP_2MIC_HANDSET:
		cnt += sprintf(buf, "[ RT5642 DSP '2MIC_HANDSET' ]\n");
		rt5670_dsp_tab = rt5670_dsp_2mic_handset;
		tab_num = RT5670_DSP_2MIC_HANDSET_NUM;
		break;

	case RT5670_DSP_2MIC_HANDSFREE:
		cnt += sprintf(buf, "[ RT5642 DSP '2MIC_HANDSFREE' ]\n");
		rt5670_dsp_tab = rt5670_dsp_2mic_handsfree;
		tab_num = RT5670_DSP_2MIC_HANDSFREE_NUM;
		break;

	case RT5670_DSP_AEC_HANDSFREE:
		cnt += sprintf(buf, "[ RT5642 DSP 'AEC_HANDSFREE' ]\n");
		rt5670_dsp_tab = rt5670_dsp_aec_handsfree;
		tab_num = RT5670_DSP_AEC_HANDSFREE_NUM;
		break;

	case RT5670_DSP_DIS:
	default:
		cnt += sprintf(buf, "RT5642 DSP Disabled\n");
		goto dsp_done;
	}

	for (i = 0; i < tab_num; i++) {
		if (cnt + RT5670_DSP_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5670_dsp_read(codec, rt5670_dsp_tab[i][0]);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5670_DSP_REG_DISP_LEN,
			"%04x: %04x\n", rt5670_dsp_tab[i][0], val);
	}

	rt5670_dsp_tab = rt5670_dsp_init;
	tab_num = RT5670_DSP_INIT_NUM;
	for (i = 0; i < tab_num; i++) {
		if (cnt + RT5670_DSP_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5670_dsp_read(codec, rt5670_dsp_tab[i][0]);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5670_DSP_REG_DISP_LEN,
			"%04x: %04x\n",
			rt5670_dsp_tab[i][0], val);
	}

dsp_done:

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t dsp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5670_priv *rt5670 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5670->codec;
	unsigned int val = 0, addr = 0;
	int i;

	pr_debug("register \"%s\" count = %zu\n", buf, count);

	/* address */
	for (i = 0; i < count; i++)
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'A' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	/* Value*/
	for (i = i + 1; i < count; i++)
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;

	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (i == count)
		pr_debug("0x%04x = 0x%04x\n",
			addr, rt5670_dsp_read(codec, addr));
	else
		rt5670_dsp_write(codec, addr, val);

	return count;
}
static DEVICE_ATTR(dsp_reg, 0600, rt5670_dsp_show, dsp_reg_store);

/**
 * rt5670_dsp_probe - register DSP for rt5670
 * @codec: audio codec
 *
 * To register DSP function for rt5670.
 *
 * Returns 0 for success or negative error code.
 */
int rt5670_dsp_probe(struct snd_soc_codec *codec)
{
	int ret;

	if (codec == NULL)
		return -EINVAL;

	snd_soc_update_bits(codec, RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP, RT5670_PWR_I2S_DSP);

	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP,
		RT5670_RST_DSP);
	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP, 0);

	usleep_range(10000, 11000);

	rt5670_dsp_write(codec, 0x22fb, 0);
	/* power down DSP*/
	rt5670_dsp_write(codec, 0x22f9, 1);

	snd_soc_update_bits(codec, RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP, 0);

	snd_soc_add_codec_controls(codec, rt5670_dsp_snd_controls,
			ARRAY_SIZE(rt5670_dsp_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5670_dsp_dapm_widgets,
			ARRAY_SIZE(rt5670_dsp_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5670_dsp_dapm_routes,
			ARRAY_SIZE(rt5670_dsp_dapm_routes));

	ret = device_create_file(codec->dev, &dev_attr_dsp_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rt5670_dsp_probe);

#ifdef RTK_IOCTL
int rt5670_dsp_ioctl_common(struct snd_hwdep *hw,
	struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rt_codec_cmd rt_codec;
	int *buf;
	int *p;
	int ret = 0;

	struct rt_codec_cmd __user *_rt_codec = (struct rt_codec_cmd *)arg;
	struct snd_soc_codec *codec = hw->private_data;
	struct rt5670_priv *rt5670;

	if (codec == NULL)
		return -EINVAL;

	rt5670 = snd_soc_codec_get_drvdata(codec);
	if (!rt5670)
		return -EFAULT;

	if (copy_from_user(&rt_codec, _rt_codec, sizeof(rt_codec))) {
		dev_err(codec->dev, "copy_from_user faild\n");
		return -EFAULT;
	}
	dev_dbg(codec->dev, "rt_codec.number=%zu\n", rt_codec.number);
	buf = kmalloc(sizeof(*buf) * rt_codec.number, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;
	if (copy_from_user(buf, rt_codec.buf, sizeof(*buf) * rt_codec.number))
		goto err;

	ret = snd_soc_update_bits(codec, RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP, RT5670_PWR_I2S_DSP);
	if (ret < 0) {
		dev_err(codec->dev,
			"Failed to power up DSP IIS interface: %d\n", ret);
		goto err;
	}

	switch (cmd) {
	case RT_READ_CODEC_DSP_IOCTL:
		for (p = buf; p < buf + rt_codec.number / 2; p++)
			*(p + rt_codec.number / 2) = rt5670_dsp_read(codec, *p);
		if (copy_to_user(rt_codec.buf, buf,
			sizeof(*buf) * rt_codec.number)) {
			ret = -EFAULT;
			goto err;
		}
		break;

	case RT_WRITE_CODEC_DSP_IOCTL:
		for (p = buf; p < buf + rt_codec.number / 2; p++)
			rt5670_dsp_write(codec, *p, *(p + rt_codec.number / 2));
		break;

	case RT_GET_CODEC_DSP_MODE_IOCTL:
		*buf = rt5670->dsp_sw;
		if (copy_to_user(rt_codec.buf, buf,
			sizeof(*buf) * rt_codec.number)) {
			ret = -EFAULT;
			goto err;
		}
		break;

	default:
		dev_info(codec->dev, "unsported dsp command\n");
		break;
	}
err:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL_GPL(rt5670_dsp_ioctl_common);
#endif

#ifdef CONFIG_PM
int rt5670_dsp_suspend(struct snd_soc_codec *codec)
{
	return 0;
}
EXPORT_SYMBOL_GPL(rt5670_dsp_suspend);

int rt5670_dsp_resume(struct snd_soc_codec *codec)
{
	return 0;
}
EXPORT_SYMBOL_GPL(rt5670_dsp_resume);
#endif
