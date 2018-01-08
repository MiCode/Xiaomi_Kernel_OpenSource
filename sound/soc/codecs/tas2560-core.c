/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2017 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2560-core.c
**
** Description:
**     TAS2560 common functions for Android Linux
**
** =============================================================================
*/
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/mfd/spk-id.h>

#include "tas2560.h"

#define TAS2560_MDELAY 0xFFFFFFFE
#define TAS2560_MSLEEP 0xFFFFFFFD

#define MAX_CLIENTS 8

static unsigned int p_tas2560_boost_headroom_data[] = {
	TAS2560_BOOST_HEAD, 0x04, 0x06, 0x66, 0x66, 0x00,

	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_thermal_foldback[] = {
	TAS2560_THERMAL_FOLDBACK, 0x04, 0x39, 0x80, 0x00, 0x00,

	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_HPF_data[] = {
	/* reg address			size	values */
	/*Isense path HPF cut off -> 2Hz*/
	TAS2560_ISENSE_PATH_CTL1, 0x04, 0x7F, 0xFB, 0xB5, 0x00,
	TAS2560_ISENSE_PATH_CTL2, 0x04, 0x80, 0x04, 0x4C, 0x00,
	TAS2560_ISENSE_PATH_CTL3, 0x04, 0x7F, 0xF7, 0x6A, 0x00,
	/*all pass*/
	TAS2560_HPF_CUTOFF_CTL1, 0x04, 0x7F, 0xFF, 0xFF, 0xFF,
	TAS2560_HPF_CUTOFF_CTL2, 0x04, 0x00, 0x00, 0x00, 0x00,
	TAS2560_HPF_CUTOFF_CTL3, 0x04, 0x00, 0x00, 0x00, 0x00,

	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_Vsense_biquad_data[] = {
		/* vsense delay in biquad = 3/8 sample @48KHz */
	TAS2560_VSENSE_DEL_CTL1, 0x04, 0x3a, 0x46, 0x74, 0x00,
	TAS2560_VSENSE_DEL_CTL2, 0x04, 0x22, 0xf3, 0x07, 0x00,
	TAS2560_VSENSE_DEL_CTL3, 0x04, 0x80, 0x77, 0x61, 0x00,
	TAS2560_VSENSE_DEL_CTL4, 0x04, 0x22, 0xa7, 0xcc, 0x00,
	TAS2560_VSENSE_DEL_CTL5, 0x04, 0x3a, 0x0c, 0x93, 0x00,

	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_48khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x08,
	TAS2560_SR_CTRL3,		0x01,	0x10,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_16khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x18,
	TAS2560_SR_CTRL3,		0x01,	0x20,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_8khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x30,
	TAS2560_SR_CTRL3,		0x01,	0x20,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_4Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x6f, 0x5c, 0x28, 0xf5,
	TAS2560_BOOST_OFF,		0x04,	0x67, 0xae, 0x14, 0x7a,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1c, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x1f, 0x0a, 0x3d, 0x70,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x22, 0x14, 0x7a, 0xe1,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x25, 0x1e, 0xb8, 0x51,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x28, 0x28, 0xf5, 0xc2,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2b, 0x33, 0x33, 0x33,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x2e, 0x3d, 0x70, 0xa3,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x31, 0x47, 0xae, 0x14,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_6Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x73, 0x33, 0x33, 0x33,
	TAS2560_BOOST_OFF,		0x04,	0x6b, 0x85, 0x1e, 0xb8,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1d, 0x99, 0x99, 0x99,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x20, 0xcc, 0xcc, 0xcc,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x24, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x27, 0x33, 0x33, 0x33,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x2a, 0x66, 0x66, 0x66,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2d, 0x99, 0x99, 0x99,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x30, 0xcc, 0xcc, 0xcc,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x34, 0x00, 0x00, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_8Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x75, 0xC2, 0x8E, 0x00,
	TAS2560_BOOST_OFF,		0x04,	0x6E, 0x14, 0x79, 0x00,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1E, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x21, 0x3d, 0x71, 0x00,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x24, 0x7a, 0xe1, 0x00,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x27, 0xb8, 0x52, 0x00,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x2a, 0xf5, 0xc3, 0x00,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2e, 0x33, 0x33, 0x00,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x31, 0x70, 0xa4, 0x00,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x34, 0xae, 0x14, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static unsigned int p_tas2560_irq_config[] = {
	TAS2560_IRQ_PIN_REG,	0x01,	0x41,
	TAS2560_INT_MODE_REG,	0x01,	0x80,/* active high until INT_STICKY_1 and INT_STICKY_2 are read to be cleared. */
	0xFFFFFFFF, 0xFFFFFFFF
};

static void tas2560_i2c_load_data(struct tas2560_priv *pTAS2560,
			enum channel chn, unsigned int *pData) {
	unsigned int nRegister;
	unsigned int *nData;
	unsigned int nLength = 0;
	unsigned int nLoop = 0;
	unsigned int i = 0;

	do {
		nRegister = pData[nLength];
		nLoop = pData[nLength + 1];
		nData = &pData[nLength + 2];
		if (nRegister == TAS2560_MDELAY) {
			mdelay(nData[0]);
		} else {
			if (nRegister != 0xFFFFFFFF) {
				i = 0;
				while (nLoop != 0) {
				pTAS2560->write(pTAS2560, chn, ((nRegister+i)), nData[i]);
				nLoop--;
				i++;
				}
			}
		}
		nLength = nLength + 2 + pData[nLength+1] ;
	} while (nRegister != 0xFFFFFFFF);
}

void tas2560_dr_boost(struct tas2560_priv *pTAS2560)
{
	pTAS2560->write(pTAS2560, channel_both, TAS2560_DR_BOOST_REG_1, 0x0c);
	pTAS2560->write(pTAS2560, channel_both, TAS2560_DR_BOOST_REG_2, 0x33);
	pTAS2560->write(pTAS2560, channel_both, TAS2560_DEV_MODE_REG, 0x02);
}

void tas2560_sw_shutdown(struct tas2560_priv *pTAS2560, int sw_shutdown)
{
	if (sw_shutdown)
		pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_PWR_REG,
				    TAS2560_PWR_BIT_MASK, 0);
	else
		pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_PWR_REG,
				    TAS2560_PWR_BIT_MASK, TAS2560_PWR_BIT_MASK);
}

int tas2560_irq_detect(struct tas2560_priv *pTAS2560)
{
	unsigned int nLeftDevInt1Status = 0, nLeftDevInt2Status = 0, nRightDevInt1Status = 0, nRightDevInt2Status = 0;
	int nResult = 0;

	nResult = pTAS2560->read(pTAS2560, channel_left, TAS2560_FLAGS_1, &nLeftDevInt1Status);
	if (nResult >= 0)
		nResult = pTAS2560->read(pTAS2560, channel_left, TAS2560_FLAGS_2, &nLeftDevInt2Status);
	else {
		dev_err(pTAS2560->dev, "cannot read register, i2c problem");
		goto end;
	}

	nResult = pTAS2560->read(pTAS2560, channel_right, TAS2560_FLAGS_1, &nRightDevInt1Status);
	if (nResult >= 0)
		nResult = pTAS2560->read(pTAS2560, channel_right, TAS2560_FLAGS_2, &nRightDevInt2Status);
	else {
		dev_err(pTAS2560->dev, "cannot read register, i2c problem");
		goto end;
	}

	dev_dbg(pTAS2560->dev, "IRQ reg: 0x%x, 0x%x, 0x%x, 0x%x\n",
	nLeftDevInt1Status, nLeftDevInt2Status, nRightDevInt1Status, nRightDevInt2Status);
	if (((nLeftDevInt1Status & 0xfc) != 0) || ((nLeftDevInt2Status & 0xc0) != 0)
		|| ((nRightDevInt1Status & 0xfc) != 0) || ((nRightDevInt2Status & 0xc0) != 0)) {
	/* in case of INT_OC, INT_UV, INT_OT, INT_BO, INT_CL, INT_CLK1, INT_CLK2 */
		dev_err(pTAS2560->dev, "IRQ critical Error : 0x%x, 0x%x, 0x%x, 0x%x\n",
		nLeftDevInt1Status, nLeftDevInt2Status, nRightDevInt1Status, nRightDevInt2Status);
		nResult = -EINVAL;
	}

end:
	return nResult;
}

int tas2560_set_SampleRate(struct tas2560_priv *pTAS2560, unsigned int nSamplingRate)
{
	int ret = 0;

	switch (nSamplingRate) {
	case 48000:
		dev_dbg(pTAS2560->dev, "Sampling rate = 48 khz\n");
		tas2560_i2c_load_data(pTAS2560, channel_both, p_tas2560_48khz_data);
		break;
	case 44100:
		dev_dbg(pTAS2560->dev, "Sampling rate = 44.1 khz\n");
		pTAS2560->write(pTAS2560, channel_both, TAS2560_SR_CTRL1, 0x11);
		break;
	case 16000:
		dev_dbg(pTAS2560->dev, "Sampling rate = 16 khz\n");
		tas2560_i2c_load_data(pTAS2560, channel_both,  p_tas2560_16khz_data);
		break;
	case 8000:
		dev_dbg(pTAS2560->dev, "Sampling rate = 8 khz\n");
		tas2560_i2c_load_data(pTAS2560, channel_both, p_tas2560_8khz_data);
		break;
	default:
		dev_err(pTAS2560->dev, "Invalid Sampling rate, %d\n", nSamplingRate);
		ret = -1;
		break;
	}

	if (ret >= 0)
		pTAS2560->mnSamplingRate = nSamplingRate;

	return ret;
}

int tas2560_set_bit_rate(struct tas2560_priv *pTAS2560, unsigned int nBitRate)
{
	int ret = 0, n = -1;

	dev_dbg(pTAS2560->dev, " nBitRate = %d \n",
		nBitRate);
	switch (nBitRate) {
	case 16:
		n = 0;
	break;
	case 20:
		n = 1;
	break;
	case 24:
		n = 2;
	break;
	case 32:
		n = 3;
	break;
	}

	if (n >= 0) {
		ret = pTAS2560->update_bits(pTAS2560, channel_both,
			TAS2560_DAI_FMT, 0x03, n);
		pTAS2560->mnBitRate = nBitRate;
	}

	return ret;
}

int tas2560_get_bit_rate(struct tas2560_priv *pTAS2560)
{
	int nBitRate = -1, value = -1, ret = 0;

	ret = pTAS2560->read(pTAS2560, channel_left, TAS2560_DAI_FMT, &value);
	value &= 0x03;

	switch (value) {
	case 0:
		nBitRate = 16;
	break;
	case 1:
		nBitRate = 20;
	break;
	case 2:
		nBitRate = 24;
	break;
	case 3:
		nBitRate = 32;
	break;
	default:
	break;
	}

	return nBitRate;
}

int tas2560_set_ASI_fmt(struct tas2560_priv *pTAS2560, unsigned int fmt)
{
	u8 serial_format = 0, asi_cfg_1 = 0;
	int ret = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		asi_cfg_1 = 0x00;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		asi_cfg_1 = TAS2560_WCLKDIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		asi_cfg_1 = TAS2560_BCLKDIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		asi_cfg_1 = (TAS2560_BCLKDIR | TAS2560_WCLKDIR);
		break;
	default:
		dev_err(pTAS2560->dev, "ASI format master is not found\n");
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		asi_cfg_1 |= 0x00;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		asi_cfg_1 |= TAS2560_WCLKINV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		asi_cfg_1 |= TAS2560_BCLKINV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		asi_cfg_1 = (TAS2560_WCLKINV | TAS2560_BCLKINV);
		break;
	default:
		dev_err(pTAS2560->dev, "ASI format Inverse is not found\n");
		ret = -EINVAL;
	}

	pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_ASI_CFG_1, TAS2560_ASICFG_MASK,
			    asi_cfg_1);


	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case (SND_SOC_DAIFMT_I2S):
		serial_format |= TAS2560_DATAFORMAT_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_A):
	case (SND_SOC_DAIFMT_DSP_B):
		serial_format |= TAS2560_DATAFORMAT_DSP;
		break;
	case (SND_SOC_DAIFMT_RIGHT_J):
		serial_format |= TAS2560_DATAFORMAT_RIGHT_J;
		break;
	case (SND_SOC_DAIFMT_LEFT_J):
		serial_format |= TAS2560_DATAFORMAT_LEFT_J;
		break;
	default:
		dev_err(pTAS2560->dev, "DAI Format is not found, fmt=0x%x\n", fmt);
		ret = -EINVAL;
		break;
	}

	pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_DAI_FMT, TAS2560_DAI_FMT_MASK,
			    serial_format);

	/*channel select*/
	pTAS2560->update_bits(pTAS2560, channel_left, TAS2560_ASI_CHANNEL, TAS2560_ASI_CHANNEL_MASK, TAS2560_ASI_CHANNEL_LEFT);
	pTAS2560->update_bits(pTAS2560, channel_right, TAS2560_ASI_CHANNEL, TAS2560_ASI_CHANNEL_MASK, TAS2560_ASI_CHANNEL_RIGHT);

	return ret;
}

int tas2560_set_pll_clkin(struct tas2560_priv *pTAS2560, int clk_id,
				  unsigned int freq)
{
	int ret = 0;
	unsigned char pll_in = 0;
	dev_dbg(pTAS2560->dev, "%s, clkid=%d\n", __func__, clk_id);

	switch (clk_id) {
	case TAS2560_PLL_CLKIN_BCLK:
		pll_in = 0;
		break;
	case TAS2560_PLL_CLKIN_MCLK:
		pll_in = 1;
		break;
	case TAS2560_PLL_CLKIN_PDMCLK:
		pll_in = 2;
		break;
	default:
		dev_err(pTAS2560->dev, "Invalid clk id: %d\n", clk_id);
		ret = -EINVAL;
		break;
	}

	if (ret >= 0) {
		pTAS2560->update_bits(pTAS2560,
			channel_both,
			TAS2560_CLK_SEL,
			TAS2560_PLL_SRC_MASK,
			pll_in<<6);
		pTAS2560->mnClkid = clk_id;
		pTAS2560->mnClkin = freq;
	}

	return ret;
}

int tas2560_setupPLL(struct tas2560_priv *pTAS2560, int pll_clkin)
{
	unsigned int pll_clk = pTAS2560->mnSamplingRate * 1024;
	unsigned int powerL = 0, powerR = 0, temp;
	unsigned int d, pll_clkin_divide;
	u8 j, p;
	int ret = 0;

	if (!pll_clkin || (pll_clkin < 0)) {
		if (pTAS2560->mnClkid != TAS2560_PLL_CLKIN_BCLK) {
			dev_err(pTAS2560->dev,
				"pll_in %d, pll_clkin frequency err:%d\n",
				pTAS2560->mnClkid, pll_clkin);
			return -EINVAL;
		}

		pll_clkin = pTAS2560->mnSamplingRate * pTAS2560->mnFrameSize;
	}

	pTAS2560->read(pTAS2560, channel_left, TAS2560_PWR_REG, &powerL);
	if (powerL&TAS2560_PWR_BIT_MASK) {
		dev_dbg(pTAS2560->dev, "power down to update PLL\n");
		pTAS2560->write(pTAS2560, channel_left, TAS2560_PWR_REG, TAS2560_PWR_BIT_MASK|TAS2560_MUTE_MASK);
	}
	pTAS2560->read(pTAS2560, channel_right, TAS2560_PWR_REG, &powerR);
	if (powerR&TAS2560_PWR_BIT_MASK) {
		dev_dbg(pTAS2560->dev, "power down to update PLL\n");
		pTAS2560->write(pTAS2560, channel_right, TAS2560_PWR_REG, TAS2560_PWR_BIT_MASK|TAS2560_MUTE_MASK);
	}

	/* Fill in the PLL control registers for J & D
	 * pll_clk = (pll_clkin * J.D) / P
	 * Need to fill in J and D here based on incoming freq
	 */
	if (pll_clkin <= 40000000)
		p = 1;
	else if (pll_clkin <= 80000000)
		p = 2;
	else if (pll_clkin <= 160000000)
		p = 3;
	else {
		dev_err(pTAS2560->dev, "PLL Clk In %d not covered here\n", pll_clkin);
		ret = -EINVAL;
	}

	if (ret >= 0) {
		j = (pll_clk * p) / pll_clkin;
		d = (pll_clk * p) % pll_clkin;
		d /= (pll_clkin / 10000);

		pll_clkin_divide = pll_clkin/(1<<p);

		if ((d == 0)
			&& ((pll_clkin_divide < 512000) || (pll_clkin_divide > 20000000))) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, pll_in=%d\n", pll_clkin);
			ret = -EINVAL;
		}

		if ((d != 0)
			&& ((pll_clkin_divide < 10000000) || (pll_clkin_divide > 20000000))) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, pll_in=%d\n", pll_clkin);
			ret = -EINVAL;
		}

		if (j == 0) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, j ZERO\n");
			ret = -EINVAL;
		}
	}

	if (ret >= 0) {
		dev_info(pTAS2560->dev,
			"PLL clk_in = %d, P=%d, J.D=%d.%d\n", pll_clkin, p, j, d);
		/*update P*/
		if (p == 64)
			temp = 0;
		else
			temp = p;
		pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_CLK_SEL, TAS2560_PLL_P_MASK, temp);

		/*Update J*/
		temp = j;
		if (pll_clkin < 1000000)
			temp |= 0x80;
		pTAS2560->write(pTAS2560, channel_both, TAS2560_SET_FREQ, temp);

		/*Update D*/
		temp = (d&0x00ff);
		pTAS2560->write(pTAS2560, channel_both, TAS2560_PLL_D_LSB, temp);
		temp = ((d&0x3f00)>>8);
		pTAS2560->write(pTAS2560, channel_both, TAS2560_PLL_D_MSB, temp);
	}

	/* Restore PLL status */
	if (powerL&TAS2560_PWR_BIT_MASK) {
		pTAS2560->write(pTAS2560, channel_left, TAS2560_PWR_REG, powerL);
	}
	if (powerR&TAS2560_PWR_BIT_MASK) {
		pTAS2560->write(pTAS2560, channel_right, TAS2560_PWR_REG, powerR);
	}

	return ret;
}

int tas2560_setLoad(struct tas2560_priv *pTAS2560, enum channel chn, int load)
{
	int ret = 0;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s:0x%x\n", __func__, load);

	switch (load) {
	case LOAD_8OHM:
		value = 0;
		tas2560_i2c_load_data(pTAS2560, chn, p_tas2560_8Ohm_data);
		break;
	case LOAD_6OHM:
		value = 1;
		tas2560_i2c_load_data(pTAS2560, chn, p_tas2560_6Ohm_data);
		break;
	case LOAD_4OHM:
		value = 2;
		tas2560_i2c_load_data(pTAS2560, chn, p_tas2560_4Ohm_data);
		break;
	default:
		break;
	}

	if (value >= 0) {
		pTAS2560->update_bits(pTAS2560, chn, TAS2560_LOAD, LOAD_MASK, value<<3);
	}
	return ret;
}

int tas2560_getLoad(struct tas2560_priv *pTAS2560)
{
	int ret = -1;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->read(pTAS2560, channel_left, TAS2560_LOAD, &value);

	value = (value&0x18)>>3;

	switch (value) {
	case 0:
		ret = LOAD_8OHM;
		break;
	case 1:
		ret = LOAD_6OHM;
		break;
	case 2:
		ret = LOAD_4OHM;
		break;
	default:
		break;
	}

	return ret;
}

int tas2560_get_volume(struct tas2560_priv *pTAS2560)
{
	int ret = -1;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	ret = pTAS2560->read(pTAS2560, channel_left, TAS2560_SPK_CTRL_REG, &value);
	if (ret >= 0)
		return value&0x0f;

	return ret;
}

int tas2560_set_volume(struct tas2560_priv *pTAS2560, int volume)
{
	int ret = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	ret = pTAS2560->update_bits(pTAS2560, channel_both, TAS2560_SPK_CTRL_REG, 0x0f, volume&0x0f);

	return ret;
}

int spk_id_get(struct device_node *np)
{
	int id;
	int state;

	state = spk_id_get_pin_3state(np);
	if (state < 0) {
		pr_err("%s: Can not get id pin state, %d\n", __func__, state);
		return VENDOR_ID_NONE;
	}

	switch (state) {
	case PIN_PULL_DOWN:
		id = VENDOR_ID_SSI;
		break;
	case PIN_PULL_UP:
		id = VENDOR_ID_UNKNOWN;
		break;
	case PIN_FLOAT:
		id = VENDOR_ID_AAC;
		break;
	default:
		id = VENDOR_ID_UNKNOWN;
		break;
	}
	return id;
}


int tas2560_parse_dt(struct device *dev,
			struct tas2560_priv *pTAS2560)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;
	unsigned int value;

	pTAS2560->spk_id_gpio_p = of_parse_phandle(np,
					"ti,spk-id-pin", 0);
	if (!pTAS2560->spk_id_gpio_p)
			dev_dbg(pTAS2560->dev, "property %s not detected in node %s",
					"ti,spk-id-pin", np->full_name);
	pTAS2560->mnSpkType = spk_id_get(pTAS2560->spk_id_gpio_p);
	dev_dbg(pTAS2560->dev, "spk is is %d", pTAS2560->mnSpkType);
	rc = of_property_read_u32(np, "ti,left-load", &pTAS2560->mnLeftLoad);
	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,left-load", np->full_name, rc);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,left-load=%d", pTAS2560->mnLeftLoad);
	}

	if (pTAS2560->mnSpkType == VENDOR_ID_AAC)
		rc = of_property_read_u32(np, "ti,right-load-aac", &pTAS2560->mnRightLoad);
	else
		rc = of_property_read_u32(np, "ti,right-load-ssi", &pTAS2560->mnRightLoad);

	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,right-load", np->full_name, rc);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,right-load=%d", pTAS2560->mnRightLoad);
	}

	pTAS2560->mnLeftResetGPIO = of_get_named_gpio(np,
				"ti,cdc-left-reset-gpio", 0);
	if (pTAS2560->mnLeftResetGPIO < 0) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-left-reset-gpio", np->full_name,
			pTAS2560->mnLeftResetGPIO);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,cdc-left-reset-gpio=%d", pTAS2560->mnLeftResetGPIO);
	}

	pTAS2560->mnRightResetGPIO = of_get_named_gpio(np,
				"ti,cdc-right-reset-gpio", 0);
	if (pTAS2560->mnRightResetGPIO < 0) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-right-reset-gpio", np->full_name,
			pTAS2560->mnRightResetGPIO);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,cdc-right-reset-gpio=%d", pTAS2560->mnRightResetGPIO);
	}

	pTAS2560->mnLeftIRQGPIO = of_get_named_gpio(np,
				"ti,cdc-left-irq-gpio", 0);
	if (pTAS2560->mnLeftIRQGPIO < 0) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-left-irq-gpio", np->full_name,
			pTAS2560->mnLeftIRQGPIO);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,cdc-left-irq-gpio=%d", pTAS2560->mnLeftIRQGPIO);
	}

	pTAS2560->mnRightIRQGPIO = of_get_named_gpio(np,
				"ti,cdc-right-irq-gpio", 0);
	if (pTAS2560->mnRightIRQGPIO < 0) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-right-irq-gpio", np->full_name,
			pTAS2560->mnRightIRQGPIO);
		ret = -1;
	} else {
		dev_dbg(pTAS2560->dev, "ti,cdc-right-irq-gpio=%d", pTAS2560->mnRightIRQGPIO);
	}

	if (ret >= 0) {
		rc = of_property_read_u32(np, "ti,left-channel", &value);
		if (rc) {
			dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
				"ti,left-channel", np->full_name, rc);
			ret = -2;
		} else {
			pTAS2560->mnLAddr = value;
			dev_dbg(pTAS2560->dev, "ti,left-channel=0x%x\n", pTAS2560->mnLAddr);
		}
	}

	if (ret >= 0) {
		rc = of_property_read_u32(np, "ti,right-channel", &value);
		if (rc) {
			dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
				"ti,right-channel", np->full_name, rc);
			ret = -3;
		} else {
			pTAS2560->mnRAddr = value;
			dev_dbg(pTAS2560->dev, "ti,right-channel=0x%x", pTAS2560->mnRAddr);
		}
	}

	return ret;
}

void tas2560_enable(struct tas2560_priv *pTAS2560, bool bEnable, enum channel mchannel)
{
	if (bEnable) {
		if (!pTAS2560->mbPowerUp[mchannel-1]) {
			dev_dbg(pTAS2560->dev, "%s power up\n", __func__);

			pTAS2560->enableIRQ(pTAS2560, false);

			pTAS2560->write(pTAS2560, mchannel, TAS2560_PWR_REG, 0x41);
			mdelay(10);
			tas2560_i2c_load_data(pTAS2560, mchannel, p_tas2560_HPF_data);
			if (mchannel&channel_left) {
				tas2560_i2c_load_data(pTAS2560, channel_left, p_tas2560_8Ohm_data);
				if (gpio_is_valid(pTAS2560->mnLeftIRQGPIO)) {
					tas2560_i2c_load_data(pTAS2560, channel_left, p_tas2560_irq_config);
				}
			}
			if (mchannel&channel_right) {
				tas2560_setLoad(pTAS2560, channel_right, pTAS2560->mnRightLoad);
				if (gpio_is_valid(pTAS2560->mnRightIRQGPIO)) {
					tas2560_i2c_load_data(pTAS2560, channel_right, p_tas2560_irq_config);
				}
			}

			tas2560_i2c_load_data(pTAS2560, mchannel, p_tas2560_boost_headroom_data);
			tas2560_i2c_load_data(pTAS2560, mchannel, p_tas2560_thermal_foldback);
			tas2560_i2c_load_data(pTAS2560, mchannel, p_tas2560_Vsense_biquad_data);
			tas2560_i2c_load_data(pTAS2560, mchannel, p_tas2560_48khz_data);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_CLK_ERR_CTRL, 0x0b);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_INT_GEN_REG, 0xff);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_PWR_REG, 0x40);
			if (mchannel&channel_left)
				pTAS2560->mbPowerUp[0] = true;
			if (mchannel&channel_right)
				pTAS2560->mbPowerUp[1] = true;
			pTAS2560->enableIRQ(pTAS2560, true);
		}
	} else {
		if (pTAS2560->mbPowerUp[mchannel-1]) {
			dev_dbg(pTAS2560->dev, "%s power down\n", __func__);
			pTAS2560->enableIRQ(pTAS2560, false);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_INT_GEN_REG, 0x00);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_CLK_ERR_CTRL, 0x00);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_PWR_REG, 0x41);
			mdelay(30);
			pTAS2560->write(pTAS2560, mchannel, TAS2560_PWR_REG, 0x01);
			mdelay(10);
			if (mchannel&channel_left)
				pTAS2560->mbPowerUp[0] = false;
			if (mchannel&channel_right)
				pTAS2560->mbPowerUp[1] = false;
		}
	}
}


MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 common functions for Android Linux");
MODULE_LICENSE("GPLv2");
