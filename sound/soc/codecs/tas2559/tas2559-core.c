/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2018 XiaoMi, Inc.
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
**     tas2559-core.c
**
** Description:
**     TAS2559 common functions for Android Linux
**
** =============================================================================
*/

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
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/crc8.h>

#include "tas2560.h"
#include "tas2559-core.h"

#define TAS2559_CAL_NAME    "/persist/audio/tas2559_cal.bin"
#define RESTART_MAX 3

static int tas2559_load_calibration(struct tas2559_priv *pTAS2559,
				    char *pFileName);
static int tas2559_load_data(struct tas2559_priv *pTAS2559, struct TData *pData,
			     unsigned int nType);
static void tas2559_clear_firmware(struct TFirmware *pFirmware);
static int tas2559_load_block(struct tas2559_priv *pTAS2559, struct TBlock *pBlock);
static int tas2559_load_configuration(struct tas2559_priv *pTAS2559,
				      unsigned int nConfiguration, bool bLoadSame);

#define TAS2559_UDELAY 0xFFFFFFFE
#define TAS2559_MDELAY 0xFFFFFFFD

#define FW_ERR_HEADER -1
#define FW_ERR_SIZE -2

#define TAS2559_BLOCK_PLL				0x00
#define TAS2559_BLOCK_PGM_ALL			0x0d
#define TAS2559_BLOCK_PGM_DEV_A			0x01
#define TAS2559_BLOCK_PGM_DEV_B			0x08
#define TAS2559_BLOCK_CFG_COEFF_DEV_A	0x03
#define TAS2559_BLOCK_CFG_COEFF_DEV_B	0x0a
#define TAS2559_BLOCK_CFG_PRE_DEV_A		0x04
#define TAS2559_BLOCK_CFG_PRE_DEV_B		0x0b
#define TAS2559_BLOCK_CFG_POST			0x05
#define TAS2559_BLOCK_CFG_POST_POWER	0x06
#define TAS2559_BLOCK_PST_POWERUP_DEV_B 0x0e

#define	PPC_DRIVER_CRCCHK			0x00000200
#define	PPC_DRIVER_CONFDEV			0x00000300
#define	PPC_DRIVER_CFGDEV_NONCRC	0x00000101

static unsigned int p_tas2559_default_data[] = {
	DevA, TAS2559_SAR_ADC2_REG, 0x05,/* enable SAR ADC */
	DevA, TAS2559_CLK_ERR_CTRL2, 0x21,/*clk1:clock hysteresis, 0.34ms; clock halt, 22ms*/
	DevA, TAS2559_CLK_ERR_CTRL3, 0x21,/*clk2: rampDown 15dB/us, clock hysteresis, 10.66us; clock halt, 22ms */
	DevB, TAS2560_CLK_ERR_CTRL2, 0x21,/*rampDown 15dB/us, clock1 hysteresis, 0.34ms; clock2 hysteresis, 10.6us */
	DevA, TAS2559_SAFE_GUARD_REG, TAS2559_SAFE_GUARD_PATTERN,/* safe guard */
	DevA, TAS2559_CLK_ERR_CTRL, 0x00,/*enable clock error detection*/
	DevB, TAS2560_CLK_ERR_CTRL, 0x00,/* disable clock error detection */
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_irq_config[] = {
	DevA, TAS2559_CLK_HALT_REG, 0x71,/* enable clk halt detect2 interrupt */
	DevA, TAS2559_INT_GEN1_REG, 0x11,/* enable spk OC and OV*/
	DevA, TAS2559_INT_GEN2_REG, 0x11,/* enable clk err1 and die OT*/
	DevA, TAS2559_INT_GEN3_REG, 0x11,/* enable clk err2 and brownout*/
	DevA, TAS2559_INT_GEN4_REG, 0x01,/* disable SAR, enable clk halt*/
	DevB, TAS2560_INT_GEN_REG, 0xff,/* enable spk OC and OV*/
	DevA, TAS2559_GPIO4_PIN_REG, 0x07,/* set GPIO4 as int1, default*/
	DevB, TAS2560_IRQ_PIN_REG, 0x41,
	DevA, TAS2559_INT_MODE_REG, 0x80,/* active high until INT_STICKY_1 and INT_STICKY_2 are read to be cleared. */
	DevB, TAS2560_INT_MODE_REG, 0x80,/* active high until INT_STICKY_1 and INT_STICKY_2 are read to be cleared. */
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_startup_data[] = {
	DevA, TAS2559_GPIO1_PIN_REG, 0x01,/* enable BCLK */
	DevA, TAS2559_GPIO2_PIN_REG, 0x01,/* enable WCLK */
	DevA, TAS2559_POWER_CTRL2_REG, 0xA0,/*Class-D, Boost power up*/
	DevA, TAS2559_POWER_CTRL2_REG, 0xA3,/*Class-D, Boost, IV sense power up*/
	DevA, TAS2559_POWER_CTRL1_REG, 0xF8,/*PLL, DSP, clock dividers power up*/
	DevBoth, TAS2559_UDELAY, 2000,/*delay*/
	DevB, TAS2560_DEV_MODE_REG, 0x02,
	DevB, TAS2560_MUTE_REG, 0x41,
	DevBoth, TAS2559_UDELAY, 2000,/*delay*/
	DevA, TAS2559_CLK_ERR_CTRL, 0x2B,/*enable clock error detection*/
	DevB, TAS2560_CLK_ERR_CTRL, 0x0B,/* disable clock error detection */
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_mute_data[] = {
	DevA, TAS2559_SOFT_MUTE_REG, 0x01,/*soft mute*/
	DevB, TAS2560_MUTE_REG, 0x41,
	DevA, TAS2559_MDELAY, 10,/*delay 10ms*/
	DevA, TAS2559_MUTE_REG, 0x03,/*mute*/
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_unmute_data[] = {
	DevA, TAS2559_MUTE_REG, 0x00,		/*unmute*/
	DevB, TAS2560_MUTE_REG, 0x40,
	DevA, TAS2559_SOFT_MUTE_REG, 0x00,	/*soft unmute*/
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_shutdown_data[] = {
	DevA, TAS2559_CLK_ERR_CTRL, 0x00,/* disable clock error detection */
	DevB, TAS2560_CLK_ERR_CTRL, 0x00,/* disable clock error detection */
	DevA, TAS2559_SOFT_MUTE_REG, 0x01,/*soft mute*/
	DevB, TAS2560_MUTE_REG, 0x41,
	DevB, TAS2560_MUTE_REG, 0x01,
	DevBoth, TAS2559_MDELAY, 10,/*delay 10ms*/
	DevB, TAS2559_MDELAY, 20,/*delay 20ms*/
	DevA, TAS2559_POWER_CTRL1_REG, 0x60,/*DSP power down*/
	DevA, TAS2559_MDELAY, 2,/*delay 20ms*/
	DevA, TAS2559_MUTE_REG, 0x03,/*mute*/
	DevA, TAS2559_POWER_CTRL2_REG, 0x00,/*Class-D, Boost power down*/
	DevA, TAS2559_POWER_CTRL1_REG, 0x00,/*all power down*/
	DevB, TAS2560_DEV_MODE_REG, 0x01,
	DevA, TAS2559_GPIO1_PIN_REG, 0x00,/* disable BCLK */
	DevA, TAS2559_GPIO2_PIN_REG, 0x00,/* disable WCLK */
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2559_shutdown_DevB_data[] = {
	DevA, TAS2559_CLK_ERR_CTRL, 0x00,/* disable clock error detection */
	DevB, TAS2560_CLK_ERR_CTRL, 0x00,/* disable clock error detection */
	DevB, TAS2560_MUTE_REG, 0x41,
	DevB, TAS2560_MUTE_REG, 0x01,
	DevA, TAS2559_POWER_CTRL1_REG, 0x60,/*DSP power down*/
	DevBoth, TAS2559_MDELAY, 30,/*delay 2ms*/
	DevB, TAS2560_DEV_MODE_REG, 0x01,
	DevA, TAS2559_POWER_CTRL2_REG, 0x00,/*Class-D, Boost power down*/
	DevA, TAS2559_POWER_CTRL1_REG, 0x00,/*all power down*/
	DevA, TAS2559_GPIO1_PIN_REG, 0x00,/* disable BCLK */
	DevA, TAS2559_GPIO2_PIN_REG, 0x00,/* disable WCLK */
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static int tas2559_dev_load_data(struct tas2559_priv *pTAS2559,
				 enum channel dev, unsigned int *pData)
{
	int nResult = 0;
	unsigned int n = 0;
	unsigned int nRegister;
	unsigned int nData;
	enum channel chl;

	do {
		chl = pData[n * 3];

		if (chl == 0xffffffff)
			break;

		if (dev & chl) {
			nRegister = pData[n * 3 + 1];
			nData = pData[n * 3 + 2];

			if (nRegister == TAS2559_UDELAY) {
				udelay(nData);
				dev_dbg(pTAS2559->dev, "%s, udelay %d\n", __func__, nData);
			} else if (nRegister == TAS2559_MDELAY) {
				mdelay(nData);
				dev_dbg(pTAS2559->dev, "%s, msleep %d\n", __func__, nData);
			} else if (nRegister != 0xFFFFFFFF) {
				dev_dbg(pTAS2559->dev, "%s, write chl=%d, B[%d]P[%d]R[%d]=0x%x\n",
					__func__, chl, TAS2559_BOOK_ID(nRegister),
					TAS2559_PAGE_ID(nRegister), TAS2559_PAGE_REG(nRegister), nData);
				nResult = pTAS2559->write(pTAS2559, chl, nRegister, nData);
				if (nResult < 0)
					break;
			}
		}

		n++;
	} while (nRegister != 0xFFFFFFFF);

	return nResult;
}

/* reserved
*static int tas2559_dev_load_blk_data(struct tas2559_priv *pTAS2559,
*	enum channel chl, unsigned int *pData)
*{
*	unsigned int nRegister;
*	unsigned int *nData;
*	unsigned char Buf[128];
*	unsigned int nLength = 0;
*	unsigned int i =0;
*	unsigned int nSize = 0;
*	int nResult = 0;
*
*	do {
*		nRegister = pData[nLength];
*		nSize = pData[nLength + 1];
*		nData = &pData[nLength + 2];
*		if (nRegister == TAS2559_MDELAY) {
*			mdelay(nData[0]);
*			dev_dbg(pTAS2559->dev, "%s, mDelay %d\n", __func__, nData[0]);
*		} else if (nRegister == 0xFFFFFFFF) {
*			dev_dbg(pTAS2559->dev, "%s, end\n", __func__);
*			break;
*		} else if (nSize > 128) {
*			dev_err(pTAS2559->dev, "%s, maximum is 128 bytes!\n",
*					__func__);
*			break;
*		} else if (nSize == 0) {
*			dev_err(pTAS2559->dev, "%s, minimum is 1 bytes!\n",
*					__func__);
*			break;
*		} else {
*			if (nSize > 1) {
*				for(i = 0; i < nSize; i++)
*					Buf[i] = (unsigned char)nData[i];
*				dev_dbg(pTAS2559->dev, "%s, BW B[%d]P[%d]R[%d], size=%d, D0=0x%x\n",
*					__func__, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
*					TAS2559_PAGE_REG(nRegister), nSize, Buf[0]);
*				nResult = pTAS2559->bulk_write(pTAS2559, chl, nRegister, Buf, nSize);
*			} else {
*				dev_dbg(pTAS2559->dev, "%s, W B[%d]P[%d]R[%d], Data=0x%x\n",
*					__func__, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
*					TAS2559_PAGE_REG(nRegister), nData[0]);
*				nResult = pTAS2559->write(pTAS2559,chl,nRegister, nData[0]);
*			}
*			if (nResult < 0)
*				break;
*		}
*		nLength = nLength + 2 + pData[nLength+1] ;
*	} while (nRegister != 0xFFFFFFFF);
*
*	return nResult;
*}
*/

static int tas2559_DevStartup(struct tas2559_priv *pTAS2559,
			      unsigned int dev)
{
	int nResult = 0;
	enum channel chl = dev;

	if (dev == DevB)
		chl = DevBoth;

	dev_dbg(pTAS2559->dev, "%s, chl=%d\n", __func__, chl);
	nResult = tas2559_dev_load_data(pTAS2559, chl, p_tas2559_startup_data);

	return nResult;
}

static int tas2559_DevShutdown(struct tas2559_priv *pTAS2559,
			       unsigned int dev)
{
	int nResult = 0;

	dev_dbg(pTAS2559->dev, "%s, dev=%d\n", __func__, dev);

	if (dev == DevB)
		nResult = tas2559_dev_load_data(pTAS2559, dev, p_tas2559_shutdown_DevB_data);
	else
		nResult = tas2559_dev_load_data(pTAS2559, dev, p_tas2559_shutdown_data);

	return nResult;
}

int tas2559_configIRQ(struct tas2559_priv *pTAS2559, enum channel dev)
{
	return tas2559_dev_load_data(pTAS2559, dev, p_tas2559_irq_config);
}

int tas2559_SA_DevChnSetup(struct tas2559_priv *pTAS2559, unsigned int mode)
{
	int nResult = 0;
	struct TProgram *pProgram;
	unsigned char buf_mute[16] = {0};
	unsigned char buf_DevA_Left_DevB_Right[16] = {0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40, 0, 0, 0};
	unsigned char buf_DevA_Right_DevB_Left[16] = {0, 0, 0, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0};
	unsigned char buf_DevA_MonoMix_DevB_MonoMix[16] = {0x20, 0, 0, 0, 0x20, 0, 0, 0, 0x20, 0, 0, 0, 0x20, 0, 0, 0};
	unsigned char *pDevBuf = NULL;

	dev_dbg(pTAS2559->dev, "%s, mode %d\n", __func__, mode);
	if ((pTAS2559->mpFirmware->mnPrograms == 0)
	    || (pTAS2559->mpFirmware->mnConfigurations == 0)) {
		dev_err(pTAS2559->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	pProgram = &(pTAS2559->mpFirmware->mpPrograms[pTAS2559->mnCurrentProgram]);
	if (pProgram->mnAppMode != TAS2559_APP_TUNINGMODE) {
		dev_err(pTAS2559->dev, "%s, not tuning mode\n", __func__);
		goto end;
	}

	if (pTAS2559->mbLoadConfigurationPrePowerUp) {
		dev_dbg(pTAS2559->dev, "%s, setup channel after coeff update\n", __func__);
		pTAS2559->mnChannelState = mode;
		goto end;
	}

	switch (mode) {
	case TAS2559_AD_BD:
		pDevBuf = pTAS2559->mnDefaultChlData;
		break;

	case TAS2559_AM_BM:
		pDevBuf = buf_mute;
		break;

	case TAS2559_AL_BR:
		pDevBuf = buf_DevA_Left_DevB_Right;
		break;

	case TAS2559_AR_BL:
		pDevBuf = buf_DevA_Right_DevB_Left;
		break;

	case TAS2559_AH_BH:
		pDevBuf = buf_DevA_MonoMix_DevB_MonoMix;
		break;

	default:
		goto end;
	}

	if (pDevBuf) {
		nResult = pTAS2559->bulk_write(pTAS2559, DevA,
				TAS2559_SA_CHL_CTRL_REG, pDevBuf, 16);
		if (nResult < 0)
			goto end;
		pTAS2559->mnChannelState = mode;
	}

end:
	return nResult;
}

int tas2559_SA_ctl_echoRef(struct tas2559_priv *pTAS2559)
{
	int nResult = 0;

	/*
	* by default:
	* TAS2559 echo-ref is on DOUT left channel,
	* TAS2560 echo-ref is on DOUT right channel
	*/
	return nResult;
}

int tas2559_set_DAC_gain(struct tas2559_priv *pTAS2559,
			 enum channel chl, unsigned int nGain)
{
	int nResult = 0;
	int gain = (nGain & 0x0f);

	if (chl & DevA) {
		nResult = pTAS2559->update_bits(pTAS2559, DevA,
						TAS2559_SPK_CTRL_REG, 0x78, (gain << 3));

		if (nResult < 0)
			goto end;
	}

	if (chl & DevB)
		nResult = pTAS2559->update_bits(pTAS2559, DevB,
						TAS2560_SPK_CTRL_REG, 0x0f, gain);

end:

	return nResult;
}

int tas2559_get_DAC_gain(struct tas2559_priv *pTAS2559,
			 enum channel chl, unsigned char *pnGain)
{
	int nResult = 0;
	int nGain;

	if (chl == DevA) {
		nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_SPK_CTRL_REG, &nGain);

		if (nResult >= 0)
			*pnGain = ((nGain >> 3) & 0x0f);
	} else
		if (chl == DevB) {
			nResult = pTAS2559->read(pTAS2559, DevB, TAS2560_SPK_CTRL_REG, &nGain);

			if (nResult >= 0)
				*pnGain = (nGain & 0x0f);
		}

	return nResult;
}

int tas2559_set_bit_rate(struct tas2559_priv *pTAS2559, unsigned int nBitRate)
{
	int nResult = 0, n = -1;

	dev_dbg(pTAS2559->dev, "%s: nBitRate = %d\n", __func__, nBitRate);

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
		nResult = pTAS2559->update_bits(pTAS2559, DevA,
						TAS2559_ASI1_DAC_FORMAT_REG, 0x18, n << 3);
		if (nResult >= 0) {
			/* The ASIM is always configured for 16-bits, hardcode the TAS2560 to 16-bits */
			nResult = pTAS2559->update_bits(pTAS2559, DevB,
							TAS2560_DAI_FMT, 0x03, 0);
		}
	}

	return nResult;
}

int tas2559_get_bit_rate(struct tas2559_priv *pTAS2559, unsigned char *pBitRate)
{
	int nResult = 0;
	unsigned int nValue = 0;
	unsigned char bitRate;

	nResult = pTAS2559->read(pTAS2559, DevA,
				 TAS2559_ASI1_DAC_FORMAT_REG, &nValue);

	if (nResult >= 0) {
		bitRate = (nValue & 0x18) >> 3;

		if (bitRate == 0)
			bitRate = 16;
		else
			if (bitRate == 1)
				bitRate = 20;
			else
				if (bitRate == 2)
					bitRate = 24;
				else
					bitRate = 32;

		*pBitRate = bitRate;
	}

	return nResult;
}

int tas2559_DevMute(struct tas2559_priv *pTAS2559, enum channel dev, bool mute)
{
	int nResult = 0;

	dev_dbg(pTAS2559->dev, "%s, dev=%d, mute=%d\n", __func__, dev, mute);

	if (mute)
		nResult = tas2559_dev_load_data(pTAS2559, dev, p_tas2559_mute_data);
	else
		nResult = tas2559_dev_load_data(pTAS2559, dev, p_tas2559_unmute_data);

	return nResult;
}

int tas2559_DevMuteStatus(struct tas2559_priv *pTAS2559, enum channel dev, bool *pMute)
{
	int nResult = 0;
	int nMute = 0;

	if (dev == DevA)
		nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_SOFT_MUTE_REG, &nMute);
	else if (dev == DevB)
		nResult = pTAS2559->read(pTAS2559, DevB, TAS2560_MUTE_REG, &nMute);
	else
		goto end;

	*pMute = ((nMute & 0x01) == 0x00);

end:
	return nResult;
}

/*
* die temperature calculation:
* DieTemp = readout / 2^23
*/
int tas2559_get_die_temperature(struct tas2559_priv *pTAS2559, int *pTemperature)
{
	int nResult = 0;
	unsigned char nBuf[4];
	int temp;

	if (!pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (!pTAS2559->mbPowerUp) {
		dev_err(pTAS2559->dev, "%s, device not powered on\n", __func__);
		goto end;
	}

	/* TAS2559 should always be enabled */
	nResult = pTAS2559->bulk_read(pTAS2559, DevA, TAS2559_DIE_TEMP_REG, nBuf, 4);

	if (nResult >= 0) {
		temp = ((int)nBuf[0] << 24) | ((int)nBuf[1] << 16) | ((int)nBuf[2] << 8) | nBuf[3];
		*pTemperature = temp;
	}

end:

	return nResult;
}

int tas2559_update_VBstVolt(struct tas2559_priv *pTAS2559, enum channel chn)
{
	int nResult = 0;
	int nVBstVoltSet = -1;

	switch (pTAS2559->mnVBoostVoltage) {
	case TAS2559_VBST_8P5V:
		nVBstVoltSet = 6;
		dev_warn(pTAS2559->dev, "%s, PPG of this snapshot should be 0dB\n", __func__);
	break;

	case TAS2559_VBST_8P1V:
		nVBstVoltSet = 5;
		dev_warn(pTAS2559->dev, "%s, PPG of this snapshot should be -1dB\n", __func__);
	break;

	case TAS2559_VBST_7P6V:
		nVBstVoltSet = 4;
		dev_warn(pTAS2559->dev, "%s, PPG of this snapshot should be -2dB\n", __func__);
	break;

	case TAS2559_VBST_6P6V:
		nVBstVoltSet = 2;
		dev_warn(pTAS2559->dev, "%s, PPG of this snapshot should be -3dB\n", __func__);
	break;

	case TAS2559_VBST_5P6V:
		nVBstVoltSet = 0;
		dev_warn(pTAS2559->dev, "%s, PPG of this snapshot should be -4dB\n", __func__);
	break;

	default:
		dev_err(pTAS2559->dev, "%s, error volt %d\n", __func__, pTAS2559->mnVBoostVoltage);
	break;
	}

	if (nVBstVoltSet >= 0) {
		if (chn & DevA)
			nResult = pTAS2559->update_bits(pTAS2559, DevA, TAS2559_VBST_VOLT_REG, 0xe0, (nVBstVoltSet << 5));
		if (chn & DevB) {
			nResult = pTAS2559->update_bits(pTAS2559, DevB, TAS2560_VBST_VOLT_REG, 0xe0, (nVBstVoltSet << 5));
		}
		dev_dbg(pTAS2559->dev, "%s, set vbst voltage (%d channel) 0x%x\n", __func__, chn, (nVBstVoltSet << 5));
	}

	return nResult;
}

int tas2559_get_VBoost(struct tas2559_priv *pTAS2559, int *pVBoost)
{
	int nResult = 0;

	dev_dbg(pTAS2559->dev, "%s, VBoost state %d\n", __func__, pTAS2559->mnVBoostState);
	switch (pTAS2559->mnVBoostState) {
	case TAS2559_VBST_NEED_DEFAULT:
	case TAS2559_VBST_DEFAULT:
		*pVBoost = 0;
	break;

	case TAS2559_VBST_A_ON:
	case TAS2559_VBST_B_ON:
	case TAS2559_VBST_A_ON_B_ON:
		*pVBoost = 1;
	break;
	default:
		dev_err(pTAS2559->dev, "%s, error state %d\n", __func__, pTAS2559->mnVBoostState);
	break;
	}

	return nResult;
}

static int tas2559_restore_VBstCtl(struct tas2559_priv *pTAS2559, enum channel chn)
{
	int nResult = 0;
	unsigned int nDevAVBstCtrl, nDevASlpCtrl, nDevABstLevel;
	unsigned int nDevBVBstCtrl, nDevBSlpCtrl, nDevBBstLevel;

	if (chn & DevA) {
		nDevAVBstCtrl = pTAS2559->mnVBoostDefaultCfg[0];
		nDevASlpCtrl = pTAS2559->mnVBoostDefaultCfg[1];
		nDevABstLevel = pTAS2559->mnVBoostDefaultCfg[2];
		nResult = pTAS2559->write(pTAS2559, DevA, TAS2559_VBOOST_CTL_REG, nDevAVBstCtrl);
		if (nResult < 0)
			goto DevB;
		nResult = pTAS2559->write(pTAS2559, DevA, TAS2559_SLEEPMODE_CTL_REG, nDevASlpCtrl);
		if (nResult < 0)
			goto DevB;
		nResult = pTAS2559->write(pTAS2559, DevA, TAS2559_VBST_VOLT_REG, nDevABstLevel);
		if (nResult < 0)
			goto DevB;
	}

DevB:
	if (chn & DevB) {
		nDevBVBstCtrl = pTAS2559->mnVBoostDefaultCfg[3];
		nDevBSlpCtrl = pTAS2559->mnVBoostDefaultCfg[4];
		nDevBBstLevel = pTAS2559->mnVBoostDefaultCfg[5];
		nResult = pTAS2559->write(pTAS2559, DevB, TAS2560_VBOOST_CTL_REG, nDevBVBstCtrl);
		if (nResult < 0)
			goto end;
		nResult = pTAS2559->write(pTAS2559, DevB, TAS2560_SLEEPMODE_CTL_REG, nDevBSlpCtrl);
		if (nResult < 0)
			goto end;
		nResult = pTAS2559->write(pTAS2559, DevB, TAS2560_VBST_VOLT_REG, nDevBBstLevel);
		if (nResult < 0)
			goto end;
	}
end:
	return nResult;
}

int tas2559_set_VBoost(struct tas2559_priv *pTAS2559, int vboost, bool bPowerOn)
{
	int nResult = 0;
	struct TConfiguration *pConfiguration;
	unsigned int nConfig;

	if ((!pTAS2559->mpFirmware->mnConfigurations)
		|| (!pTAS2559->mpFirmware->mnPrograms)) {
		dev_err(pTAS2559->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (bPowerOn) {
		dev_info(pTAS2559->dev, "%s, will load VBoost state next time before power on\n", __func__);
		pTAS2559->mbLoadVBoostPrePowerUp = true;
		pTAS2559->mnVBoostNewState = vboost;
		goto end;
	}

	if (pTAS2559->mbLoadConfigurationPrePowerUp)
		nConfig = pTAS2559->mnNewConfiguration;
	else
		nConfig = pTAS2559->mnCurrentConfiguration;

	pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[nConfig]);

	if (pTAS2559->mnVBoostState == TAS2559_VBST_NEED_DEFAULT) {
		if (pConfiguration->mnDevices & DevA) {
			nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_VBOOST_CTL_REG, &pTAS2559->mnVBoostDefaultCfg[0]);
			if (nResult < 0)
				goto end;
			nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_SLEEPMODE_CTL_REG, &pTAS2559->mnVBoostDefaultCfg[1]);
			if (nResult < 0)
				goto end;
			nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_VBST_VOLT_REG, &pTAS2559->mnVBoostDefaultCfg[2]);
			if (nResult < 0)
				goto end;
		}
		if (pConfiguration->mnDevices & DevB) {
			nResult = pTAS2559->read(pTAS2559, DevB, TAS2560_VBOOST_CTL_REG, &pTAS2559->mnVBoostDefaultCfg[3]);
			if (nResult < 0)
				goto end;
			nResult = pTAS2559->read(pTAS2559, DevB, TAS2560_SLEEPMODE_CTL_REG, &pTAS2559->mnVBoostDefaultCfg[4]);
			if (nResult < 0)
				goto end;
			nResult = pTAS2559->read(pTAS2559, DevB, TAS2560_VBST_VOLT_REG, &pTAS2559->mnVBoostDefaultCfg[5]);
			if (nResult < 0)
				goto end;
		}
		dev_dbg(pTAS2559->dev, "%s, get default VBoost\n", __func__);
		pTAS2559->mnVBoostState = TAS2559_VBST_DEFAULT;
		if ((vboost == TAS2559_VBST_DEFAULT)
			|| (vboost == TAS2559_VBST_NEED_DEFAULT)) {
			dev_dbg(pTAS2559->dev, "%s, already default, bypass\n", __func__);
			goto end;
		}
	}

	if (vboost) {
		if (pConfiguration->mnDevices & DevA) {
			if (!(pTAS2559->mnVBoostState & TAS2559_VBST_A_ON)) {
				nResult = tas2559_update_VBstVolt(pTAS2559, DevA);
				if (nResult < 0)
					goto end;
				nResult = pTAS2559->update_bits(pTAS2559, DevA, TAS2559_VBOOST_CTL_REG, 0x40, 0x40);
				if (nResult < 0)
					goto end;
				nResult = pTAS2559->update_bits(pTAS2559, DevA, TAS2559_SLEEPMODE_CTL_REG, 0x40, 0x00);
				if (nResult < 0)
					goto end;
				pTAS2559->mnVBoostState |= TAS2559_VBST_A_ON;
				dev_dbg(pTAS2559->dev, "%s, devA Boost On, %d\n", __func__, pTAS2559->mnVBoostState);
			}
		} else {
			if (pTAS2559->mnVBoostState & TAS2559_VBST_A_ON) {
				nResult = tas2559_restore_VBstCtl(pTAS2559, DevA);
				if (nResult < 0)
					goto end;
				pTAS2559->mnVBoostState &= ~TAS2559_VBST_A_ON;
				dev_dbg(pTAS2559->dev, "%s, devA Boost Off, %d\n", __func__, pTAS2559->mnVBoostState);
			}
		}

		if (pConfiguration->mnDevices & DevB) {
			if (!(pTAS2559->mnVBoostState & TAS2559_VBST_B_ON)) {
				nResult = tas2559_update_VBstVolt(pTAS2559, DevB);
				if (nResult < 0)
					goto end;
				nResult = pTAS2559->update_bits(pTAS2559, DevB, TAS2560_VBOOST_CTL_REG, 0x01, 0x01);
				if (nResult < 0)
					goto end;
				nResult = pTAS2559->update_bits(pTAS2559, DevB, TAS2560_SLEEPMODE_CTL_REG, 0x08, 0x08);
				if (nResult < 0)
					goto end;
				pTAS2559->mnVBoostState |= TAS2559_VBST_B_ON;
				dev_dbg(pTAS2559->dev, "%s, devB Boost On, %d\n", __func__, pTAS2559->mnVBoostState);
			}
		}  else {
			if (pTAS2559->mnVBoostState & TAS2559_VBST_B_ON) {
				nResult = tas2559_restore_VBstCtl(pTAS2559, DevB);
				if (nResult < 0)
					goto end;
				pTAS2559->mnVBoostState &= ~TAS2559_VBST_B_ON;
				dev_dbg(pTAS2559->dev, "%s, devB Boost Off, %d\n", __func__, pTAS2559->mnVBoostState);
			}
		}
	} else {
		if (pTAS2559->mnVBoostState & TAS2559_VBST_A_ON) {
			nResult = tas2559_restore_VBstCtl(pTAS2559, DevA);
			if (nResult < 0)
				goto end;
			pTAS2559->mnVBoostState &= ~TAS2559_VBST_A_ON;
			dev_dbg(pTAS2559->dev, "%s, devA Boost default, %d\n", __func__, pTAS2559->mnVBoostState);
		}
		if (pTAS2559->mnVBoostState & TAS2559_VBST_B_ON) {
			nResult = tas2559_restore_VBstCtl(pTAS2559, DevB);
			if (nResult < 0)
				goto end;
			pTAS2559->mnVBoostState &= ~TAS2559_VBST_B_ON;
			dev_dbg(pTAS2559->dev, "%s, devB Boost default, %d\n", __func__, pTAS2559->mnVBoostState);
		}
	}

end:

	return 0;
}

int tas2559_load_platdata(struct tas2559_priv *pTAS2559)
{
	int nResult = 0;
	int nDev = 0;

	dev_dbg(pTAS2559->dev, "%s\n", __func__);

	if (gpio_is_valid(pTAS2559->mnDevAGPIOIRQ))
		nDev |= DevA;

	if (gpio_is_valid(pTAS2559->mnDevBGPIOIRQ))
		nDev |= DevB;

	if (nDev) {
		nResult = tas2559_configIRQ(pTAS2559, nDev);

		if (nResult < 0)
			goto end;
	}

	nResult = tas2559_set_bit_rate(pTAS2559, pTAS2559->mnBitRate);

	if (nResult < 0)
		goto end;

	nResult = tas2559_SA_ctl_echoRef(pTAS2559);

	if (nResult < 0)
		goto end;

end:

	return nResult;
}

int tas2559_load_default(struct tas2559_priv *pTAS2559)
{
	int nResult = 0;

	dev_dbg(pTAS2559->dev, "%s\n", __func__);
	nResult = tas2559_dev_load_data(pTAS2559, DevBoth, p_tas2559_default_data);

	if (nResult < 0)
		goto end;

	nResult = tas2559_load_platdata(pTAS2559);

	if (nResult < 0)
		goto end;

	/* enable DOUT tri-state for extra BCLKs */
	nResult = pTAS2559->update_bits(pTAS2559, DevA, TAS2559_ASI1_DAC_FORMAT_REG, 0x01, 0x01);

	if (nResult < 0)
		goto end;

	nResult = pTAS2559->update_bits(pTAS2559, DevB, TAS2560_ASI_CFG_1, 0x02, 0x02);

	if (nResult < 0)
		goto end;

	/* Interrupt pin, low-highZ, high active driven */
	nResult = pTAS2559->update_bits(pTAS2559, DevA, TAS2559_GPIO_HIZ_CTRL2_REG, 0x30, 0x30);

end:

	return nResult;
}

static void failsafe(struct tas2559_priv *pTAS2559)
{
	dev_err(pTAS2559->dev, "%s\n", __func__);
	pTAS2559->mnErrCode |= ERROR_FAILSAFE;

	if (hrtimer_active(&pTAS2559->mtimer))
		hrtimer_cancel(&pTAS2559->mtimer);

	if (pTAS2559->mnRestart < RESTART_MAX) {
		pTAS2559->mnRestart++;
		msleep(100);
		dev_err(pTAS2559->dev, "I2C COMM error, restart SmartAmp.\n");
		schedule_delayed_work(&pTAS2559->irq_work, msecs_to_jiffies(100));
		return;
	}

	pTAS2559->enableIRQ(pTAS2559, DevBoth, false);
	tas2559_DevShutdown(pTAS2559, DevBoth);
	pTAS2559->mbPowerUp = false;
	pTAS2559->hw_reset(pTAS2559);
	pTAS2559->write(pTAS2559, DevBoth, TAS2559_SW_RESET_REG, 0x01);
	msleep(1);
	pTAS2559->write(pTAS2559, DevA, TAS2559_SPK_CTRL_REG, 0x04);
	pTAS2559->write(pTAS2559, DevB, TAS2560_SPK_CTRL_REG, 0x50);

	if (pTAS2559->mpFirmware != NULL)
		tas2559_clear_firmware(pTAS2559->mpFirmware);
}

int tas2559_checkPLL(struct tas2559_priv *pTAS2559)
{
	int nResult = 0;
	/*
	* TO DO
	*/

	return nResult;
}

/*
* tas2559_load_coefficient
*/
static int tas2559_load_coefficient(struct tas2559_priv *pTAS2559,
				    int nPrevConfig, int nNewConfig, bool bPowerOn)
{
	int nResult = 0;
	struct TPLL *pPLL;
	struct TProgram *pProgram;
	struct TConfiguration *pPrevConfiguration;
	struct TConfiguration *pNewConfiguration;
	enum channel chl;
	bool bRestorePower = false;

	dev_dbg(pTAS2559->dev, "%s, Prev=%d, new=%d, Pow=%d\n",
		__func__, nPrevConfig, nNewConfig, bPowerOn);

	if (!pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (nNewConfig >= pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "%s, invalid configuration New=%d, total=%d\n",
			__func__, nNewConfig, pTAS2559->mpFirmware->mnConfigurations);
		goto end;
	}

	if (nPrevConfig < 0) {
		pPrevConfiguration = NULL;
		chl = DevBoth;
	} else
		if (nPrevConfig == nNewConfig) {
			dev_dbg(pTAS2559->dev, "%d configuration is already loaded\n", nNewConfig);
			goto end;
		} else {
			pPrevConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[nPrevConfig]);
			chl = pPrevConfiguration->mnDevices;
		}

	pNewConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[nNewConfig]);
	pTAS2559->mnCurrentConfiguration = nNewConfig;

	if (pPrevConfiguration) {
		if ((pPrevConfiguration->mnPLL == pNewConfiguration->mnPLL)
		    && (pPrevConfiguration->mnDevices == pNewConfiguration->mnDevices)) {
			dev_dbg(pTAS2559->dev, "%s, PLL and device same\n", __func__);
			goto prog_coefficient;
		}
	}

	pProgram = &(pTAS2559->mpFirmware->mpPrograms[pTAS2559->mnCurrentProgram]);

	if (bPowerOn) {
		if (hrtimer_active(&pTAS2559->mtimer))
			hrtimer_cancel(&pTAS2559->mtimer);

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE)
			pTAS2559->enableIRQ(pTAS2559, DevBoth, false);

		nResult = tas2559_DevShutdown(pTAS2559, chl);

		if (nResult < 0)
			goto end;

		bRestorePower = true;
	}

	/* load PLL */
	pPLL = &(pTAS2559->mpFirmware->mpPLLs[pNewConfiguration->mnPLL]);
	dev_dbg(pTAS2559->dev, "load PLL: %s block for Configuration %s\n",
		pPLL->mpName, pNewConfiguration->mpName);
	nResult = tas2559_load_block(pTAS2559, &(pPLL->mBlock));

	if (nResult < 0)
		goto end;

	pTAS2559->mnCurrentSampleRate = pNewConfiguration->mnSamplingRate;

	dev_dbg(pTAS2559->dev, "load configuration %s conefficient pre block\n",
		pNewConfiguration->mpName);

	if (pNewConfiguration->mnDevices & DevA) {
		nResult = tas2559_load_data(pTAS2559, &(pNewConfiguration->mData), TAS2559_BLOCK_CFG_PRE_DEV_A);

		if (nResult < 0)
			goto end;
	}

	if (pNewConfiguration->mnDevices & DevB) {
		nResult = tas2559_load_data(pTAS2559, &(pNewConfiguration->mData), TAS2559_BLOCK_CFG_PRE_DEV_B);

		if (nResult < 0)
			goto end;
	}

prog_coefficient:
	dev_dbg(pTAS2559->dev, "load new configuration: %s, coeff block data\n",
		pNewConfiguration->mpName);

	if (pNewConfiguration->mnDevices & DevA) {
		nResult = tas2559_load_data(pTAS2559, &(pNewConfiguration->mData),
					    TAS2559_BLOCK_CFG_COEFF_DEV_A);
		if (nResult < 0)
			goto end;
	}

	if (pNewConfiguration->mnDevices & DevB) {
		nResult = tas2559_load_data(pTAS2559, &(pNewConfiguration->mData),
					    TAS2559_BLOCK_CFG_COEFF_DEV_B);
		if (nResult < 0)
			goto end;
	}

	if (pTAS2559->mnChannelState == TAS2559_AD_BD) {
		nResult = pTAS2559->bulk_read(pTAS2559,
				DevA, TAS2559_SA_CHL_CTRL_REG, pTAS2559->mnDefaultChlData, 16);
		if (nResult < 0)
			goto end;
	} else {
		nResult = tas2559_SA_DevChnSetup(pTAS2559, pTAS2559->mnChannelState);
		if (nResult < 0)
			goto end;
	}

	if (pTAS2559->mpCalFirmware->mnCalibrations) {
		nResult = tas2559_set_calibration(pTAS2559, pTAS2559->mnCurrentCalibration);
		if (nResult < 0)
			goto end;
	}

	if (bRestorePower) {
		dev_dbg(pTAS2559->dev, "%s, set vboost, before power on %d\n",
			__func__, pTAS2559->mnVBoostState);
		nResult = tas2559_set_VBoost(pTAS2559, pTAS2559->mnVBoostState, false);
		if (nResult < 0)
			goto end;

		pTAS2559->clearIRQ(pTAS2559);
		nResult = tas2559_DevStartup(pTAS2559, pNewConfiguration->mnDevices);
		if (nResult < 0)
			goto end;

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
			nResult = tas2559_checkPLL(pTAS2559);

			if (nResult < 0) {
				nResult = tas2559_DevShutdown(pTAS2559, pNewConfiguration->mnDevices);
				pTAS2559->mbPowerUp = false;
				goto end;
			}
		}

		if (pNewConfiguration->mnDevices & DevB) {
			nResult = tas2559_load_data(pTAS2559, &(pNewConfiguration->mData),
						    TAS2559_BLOCK_PST_POWERUP_DEV_B);

			if (nResult < 0)
				goto end;
		}

		dev_dbg(pTAS2559->dev,
			"device powered up, load unmute\n");
		nResult = tas2559_DevMute(pTAS2559, pNewConfiguration->mnDevices, false);

		if (nResult < 0)
			goto end;

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
			pTAS2559->enableIRQ(pTAS2559, pNewConfiguration->mnDevices, true);

			if (!hrtimer_active(&pTAS2559->mtimer)) {
				pTAS2559->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2559->mtimer,
					      ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}

end:

	if (nResult < 0)
		dev_err(pTAS2559->dev, "%s, load new conf %s error\n", __func__, pNewConfiguration->mpName);

	return nResult;
}

int tas2559_enable(struct tas2559_priv *pTAS2559, bool bEnable)
{
	int nResult = 0;
	struct TProgram *pProgram;
	struct TConfiguration *pConfiguration;
	unsigned int nValue;

	dev_dbg(pTAS2559->dev, "%s: %s\n", __func__, bEnable ? "On" : "Off");

	if ((pTAS2559->mpFirmware->mnPrograms == 0)
	    || (pTAS2559->mpFirmware->mnConfigurations == 0)) {
		dev_err(pTAS2559->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	/* check safe guard*/
	nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_SAFE_GUARD_REG, &nValue);
	if (nResult < 0)
		goto end;
	if ((nValue & 0xff) != TAS2559_SAFE_GUARD_PATTERN) {
		dev_err(pTAS2559->dev, "ERROR DevA safe guard (0x%x) failure!\n", nValue);
		nResult = -EPIPE;
		pTAS2559->mnErrCode = ERROR_SAFE_GUARD;
		pTAS2559->mbPowerUp = true;
		goto end;
	}

	pProgram = &(pTAS2559->mpFirmware->mpPrograms[pTAS2559->mnCurrentProgram]);
	if (bEnable) {
		if (!pTAS2559->mbPowerUp) {
			if (!pTAS2559->mbCalibrationLoaded) {
				tas2559_set_calibration(pTAS2559, 0xFF);
				pTAS2559->mbCalibrationLoaded = true;
			}

			if (pTAS2559->mbLoadConfigurationPrePowerUp) {
				pTAS2559->mbLoadConfigurationPrePowerUp = false;
				nResult = tas2559_load_coefficient(pTAS2559,
								pTAS2559->mnCurrentConfiguration, pTAS2559->mnNewConfiguration, false);

				if (nResult < 0)
					goto end;
			}

			if (pTAS2559->mbLoadVBoostPrePowerUp) {
				dev_dbg(pTAS2559->dev, "%s, cfg boost before power on new %d, current=%d\n",
					__func__, pTAS2559->mnVBoostNewState, pTAS2559->mnVBoostState);
				nResult = tas2559_set_VBoost(pTAS2559, pTAS2559->mnVBoostNewState, false);
				if (nResult < 0)
					goto end;
				pTAS2559->mbLoadVBoostPrePowerUp = false;
			}

			pTAS2559->clearIRQ(pTAS2559);
			pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);
			nResult = tas2559_DevStartup(pTAS2559, pConfiguration->mnDevices);
			if (nResult < 0)
				goto end;

			if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
				nResult = tas2559_checkPLL(pTAS2559);
				if (nResult < 0) {
					nResult = tas2559_DevShutdown(pTAS2559, pConfiguration->mnDevices);
					goto end;
				}
			}

			if (pConfiguration->mnDevices & DevB) {
				nResult = tas2559_load_data(pTAS2559, &(pConfiguration->mData),
								TAS2559_BLOCK_PST_POWERUP_DEV_B);
				if (nResult < 0)
					goto end;
			}

			nResult = tas2559_DevMute(pTAS2559, pConfiguration->mnDevices, false);
			if (nResult < 0)
				goto end;

			if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
				/* turn on IRQ */
				pTAS2559->enableIRQ(pTAS2559, pConfiguration->mnDevices, true);
				if (!hrtimer_active(&pTAS2559->mtimer)) {
					pTAS2559->mnDieTvReadCounter = 0;
					hrtimer_start(&pTAS2559->mtimer,
						ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
				}
			}

			pTAS2559->mbPowerUp = true;
			pTAS2559->mnRestart = 0;
		}
	} else {
		if (pTAS2559->mbPowerUp) {
			if (hrtimer_active(&pTAS2559->mtimer))
				hrtimer_cancel(&pTAS2559->mtimer);

			pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);

			if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
				/* turn off IRQ */
				pTAS2559->enableIRQ(pTAS2559, DevBoth, false);
			}

			nResult = tas2559_DevShutdown(pTAS2559, pConfiguration->mnDevices);
			if (nResult < 0)
				goto end;

			pTAS2559->mbPowerUp = false;
			pTAS2559->mnRestart = 0;
		}
	}

	nResult = 0;

end:

	if (nResult < 0) {
		if (pTAS2559->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK | ERROR_SAFE_GUARD))
			failsafe(pTAS2559);
	}

	dev_dbg(pTAS2559->dev, "%s: exit\n", __func__);
	return nResult;
}

int tas2559_set_sampling_rate(struct tas2559_priv *pTAS2559, unsigned int nSamplingRate)
{
	int nResult = 0;
	struct TConfiguration *pConfiguration;
	unsigned int nConfiguration;

	dev_dbg(pTAS2559->dev, "%s: nSamplingRate = %d [Hz]\n", __func__,
		nSamplingRate);

	if ((!pTAS2559->mpFirmware->mpPrograms) ||
	    (!pTAS2559->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2559->dev, "Firmware not loaded\n");
		nResult = -EINVAL;
		goto end;
	}

	pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);

	if (pConfiguration->mnSamplingRate == nSamplingRate) {
		dev_info(pTAS2559->dev, "Sampling rate for current configuration matches: %d\n",
			 nSamplingRate);
		nResult = 0;
		goto end;
	}

	for (nConfiguration = 0;
	     nConfiguration < pTAS2559->mpFirmware->mnConfigurations;
	     nConfiguration++) {
		pConfiguration =
			&(pTAS2559->mpFirmware->mpConfigurations[nConfiguration]);

		if ((pConfiguration->mnSamplingRate == nSamplingRate)
		    && (pConfiguration->mnProgram == pTAS2559->mnCurrentProgram)) {
			dev_info(pTAS2559->dev,
				 "Found configuration: %s, with compatible sampling rate %d\n",
				 pConfiguration->mpName, nSamplingRate);
			nResult = tas2559_load_configuration(pTAS2559, nConfiguration, false);
			goto end;
		}
	}

	dev_err(pTAS2559->dev, "Cannot find a configuration that supports sampling rate: %d\n",
		nSamplingRate);

end:

	return nResult;
}

static void fw_print_header(struct tas2559_priv *pTAS2559, struct TFirmware *pFirmware)
{
	dev_info(pTAS2559->dev, "FW Size       = %d", pFirmware->mnFWSize);
	dev_info(pTAS2559->dev, "Checksum      = 0x%04X", pFirmware->mnChecksum);
	dev_info(pTAS2559->dev, "PPC Version   = 0x%04X", pFirmware->mnPPCVersion);
	dev_info(pTAS2559->dev, "FW  Version    = 0x%04X", pFirmware->mnFWVersion);
	dev_info(pTAS2559->dev, "Driver Version= 0x%04X", pFirmware->mnDriverVersion);
	dev_info(pTAS2559->dev, "Timestamp     = %d", pFirmware->mnTimeStamp);
	dev_info(pTAS2559->dev, "DDC Name      = %s", pFirmware->mpDDCName);
	dev_info(pTAS2559->dev, "Description   = %s", pFirmware->mpDescription);
}

inline unsigned int fw_convert_number(unsigned char *pData)
{
	return pData[3] + (pData[2] << 8) + (pData[1] << 16) + (pData[0] << 24);
}

static int fw_parse_header(struct tas2559_priv *pTAS2559,
			   struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned char pMagicNumber[] = { 0x35, 0x35, 0x35, 0x32 };

	if (nSize < 104) {
		dev_err(pTAS2559->dev, "Firmware: Header too short");
		return -EINVAL;
	}

	if (memcmp(pData, pMagicNumber, 4)) {
		dev_err(pTAS2559->dev, "Firmware: Magic number doesn't match");
		return -EINVAL;
	}

	pData += 4;

	pFirmware->mnFWSize = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnChecksum = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnPPCVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnFWVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnDriverVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnTimeStamp = fw_convert_number(pData);
	pData += 4;

	memcpy(pFirmware->mpDDCName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pFirmware->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;

	if ((pData - pDataStart) >= nSize) {
		dev_err(pTAS2559->dev, "Firmware: Header too short after DDC description");
		return -EINVAL;
	}

	pFirmware->mnDeviceFamily = fw_convert_number(pData);
	pData += 4;

	if (pFirmware->mnDeviceFamily != 0) {
		dev_err(pTAS2559->dev,
			"deviceFamily %d, not TAS device", pFirmware->mnDeviceFamily);
		return -EINVAL;
	}

	pFirmware->mnDevice = fw_convert_number(pData);
	pData += 4;

	if (pFirmware->mnDevice != 4) {
		dev_err(pTAS2559->dev,
			"device %d, not TAS2559", pFirmware->mnDevice);
		return -EINVAL;
	}

	fw_print_header(pTAS2559, pFirmware);

	return pData - pDataStart;
}

static int fw_parse_block_data(struct tas2559_priv *pTAS2559, struct TFirmware *pFirmware,
			       struct TBlock *pBlock, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;

	pBlock->mnType = fw_convert_number(pData);
	pData += 4;

	if (pFirmware->mnDriverVersion >= PPC_DRIVER_CRCCHK) {
		pBlock->mbPChkSumPresent = pData[0];
		pData++;

		pBlock->mnPChkSum = pData[0];
		pData++;

		pBlock->mbYChkSumPresent = pData[0];
		pData++;

		pBlock->mnYChkSum = pData[0];
		pData++;
	} else {
		pBlock->mbPChkSumPresent = 0;
		pBlock->mbYChkSumPresent = 0;
	}

	pBlock->mnCommands = fw_convert_number(pData);
	pData += 4;

	n = pBlock->mnCommands * 4;
	pBlock->mpData = kmemdup(pData, n, GFP_KERNEL);
	pData += n;

	return pData - pDataStart;
}

static int fw_parse_data(struct tas2559_priv *pTAS2559, struct TFirmware *pFirmware,
			 struct TData *pImageData, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int nBlock;
	unsigned int n;

	memcpy(pImageData->mpName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pImageData->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;

	pImageData->mnBlocks = (pData[0] << 8) + pData[1];
	pData += 2;

	pImageData->mpBlocks =
		kmalloc(sizeof(struct TBlock) * pImageData->mnBlocks, GFP_KERNEL);

	for (nBlock = 0; nBlock < pImageData->mnBlocks; nBlock++) {
		n = fw_parse_block_data(pTAS2559, pFirmware,
					&(pImageData->mpBlocks[nBlock]), pData);
		pData += n;
	}

	return pData - pDataStart;
}

static int fw_parse_pll_data(struct tas2559_priv *pTAS2559,
			     struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nPLL;
	struct TPLL *pPLL;

	pFirmware->mnPLLs = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnPLLs == 0)
		goto end;

	pFirmware->mpPLLs = kmalloc_array(pFirmware->mnPLLs, sizeof(struct TPLL), GFP_KERNEL);

	for (nPLL = 0; nPLL < pFirmware->mnPLLs; nPLL++) {
		pPLL = &(pFirmware->mpPLLs[nPLL]);

		memcpy(pPLL->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pPLL->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		n = fw_parse_block_data(pTAS2559, pFirmware, &(pPLL->mBlock), pData);
		pData += n;
	}

end:
	return pData - pDataStart;
}

static int fw_parse_program_data(struct tas2559_priv *pTAS2559,
				 struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nProgram;
	struct TProgram *pProgram;

	pFirmware->mnPrograms = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnPrograms == 0)
		goto end;

	pFirmware->mpPrograms =
		kmalloc(sizeof(struct TProgram) * pFirmware->mnPrograms, GFP_KERNEL);

	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		pProgram = &(pFirmware->mpPrograms[nProgram]);
		memcpy(pProgram->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pProgram->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pProgram->mnAppMode = pData[0];
		pData++;

		pProgram->mnBoost = (pData[0] << 8) + pData[1];
		pData += 2;

		n = fw_parse_data(pTAS2559, pFirmware, &(pProgram->mData), pData);
		pData += n;
	}

end:

	return pData - pDataStart;
}

static int fw_parse_configuration_data(struct tas2559_priv *pTAS2559,
				       struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nConfiguration;
	struct TConfiguration *pConfiguration;

	pFirmware->mnConfigurations = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnConfigurations == 0)
		goto end;

	pFirmware->mpConfigurations =
		kmalloc(sizeof(struct TConfiguration) * pFirmware->mnConfigurations,
			GFP_KERNEL);

	for (nConfiguration = 0; nConfiguration < pFirmware->mnConfigurations;
	     nConfiguration++) {
		pConfiguration = &(pFirmware->mpConfigurations[nConfiguration]);
		memcpy(pConfiguration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pConfiguration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		if ((pFirmware->mnDriverVersion >= PPC_DRIVER_CONFDEV)
		    || ((pFirmware->mnDriverVersion >= PPC_DRIVER_CFGDEV_NONCRC)
			&& (pFirmware->mnDriverVersion < PPC_DRIVER_CRCCHK))) {
			pConfiguration->mnDevices = (pData[0] << 8) + pData[1];
			pData += 2;
		} else
			pConfiguration->mnDevices = DevBoth;

		pConfiguration->mnProgram = pData[0];
		pData++;

		pConfiguration->mnPLL = pData[0];
		pData++;

		pConfiguration->mnSamplingRate = fw_convert_number(pData);
		pData += 4;

		n = fw_parse_data(pTAS2559, pFirmware, &(pConfiguration->mData), pData);
		pData += n;
	}

end:

	return pData - pDataStart;
}

int fw_parse_calibration_data(struct tas2559_priv *pTAS2559,
			      struct TFirmware *pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nCalibration;
	struct TCalibration *pCalibration;

	pFirmware->mnCalibrations = (pData[0] << 8) + pData[1];
	pData += 2;

	if (pFirmware->mnCalibrations == 0)
		goto end;

	pFirmware->mpCalibrations =
		kmalloc(sizeof(struct TCalibration) * pFirmware->mnCalibrations, GFP_KERNEL);

	for (nCalibration = 0;
	     nCalibration < pFirmware->mnCalibrations;
	     nCalibration++) {
		pCalibration = &(pFirmware->mpCalibrations[nCalibration]);
		memcpy(pCalibration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pCalibration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pCalibration->mnProgram = pData[0];
		pData++;

		pCalibration->mnConfiguration = pData[0];
		pData++;

		n = fw_parse_data(pTAS2559, pFirmware, &(pCalibration->mData), pData);
		pData += n;
	}

end:

	return pData - pDataStart;
}

static int fw_parse(struct tas2559_priv *pTAS2559,
		    struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	int nPosition = 0;

	nPosition = fw_parse_header(pTAS2559, pFirmware, pData, nSize);

	if (nPosition < 0) {
		dev_err(pTAS2559->dev, "Firmware: Wrong Header");
		return -EINVAL;
	}

	if (nPosition >= nSize) {
		dev_err(pTAS2559->dev, "Firmware: Too short");
		return -EINVAL;
	}

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_pll_data(pTAS2559, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_program_data(pTAS2559, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_configuration_data(pTAS2559, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	if (nSize > 64)
		nPosition = fw_parse_calibration_data(pTAS2559, pFirmware, pData);

	return 0;
}


static const unsigned char crc8_lookup_table[CRC8_TABLE_SIZE] = {
	0x00, 0x4D, 0x9A, 0xD7, 0x79, 0x34, 0xE3, 0xAE, 0xF2, 0xBF, 0x68, 0x25, 0x8B, 0xC6, 0x11, 0x5C,
	0xA9, 0xE4, 0x33, 0x7E, 0xD0, 0x9D, 0x4A, 0x07, 0x5B, 0x16, 0xC1, 0x8C, 0x22, 0x6F, 0xB8, 0xF5,
	0x1F, 0x52, 0x85, 0xC8, 0x66, 0x2B, 0xFC, 0xB1, 0xED, 0xA0, 0x77, 0x3A, 0x94, 0xD9, 0x0E, 0x43,
	0xB6, 0xFB, 0x2C, 0x61, 0xCF, 0x82, 0x55, 0x18, 0x44, 0x09, 0xDE, 0x93, 0x3D, 0x70, 0xA7, 0xEA,
	0x3E, 0x73, 0xA4, 0xE9, 0x47, 0x0A, 0xDD, 0x90, 0xCC, 0x81, 0x56, 0x1B, 0xB5, 0xF8, 0x2F, 0x62,
	0x97, 0xDA, 0x0D, 0x40, 0xEE, 0xA3, 0x74, 0x39, 0x65, 0x28, 0xFF, 0xB2, 0x1C, 0x51, 0x86, 0xCB,
	0x21, 0x6C, 0xBB, 0xF6, 0x58, 0x15, 0xC2, 0x8F, 0xD3, 0x9E, 0x49, 0x04, 0xAA, 0xE7, 0x30, 0x7D,
	0x88, 0xC5, 0x12, 0x5F, 0xF1, 0xBC, 0x6B, 0x26, 0x7A, 0x37, 0xE0, 0xAD, 0x03, 0x4E, 0x99, 0xD4,
	0x7C, 0x31, 0xE6, 0xAB, 0x05, 0x48, 0x9F, 0xD2, 0x8E, 0xC3, 0x14, 0x59, 0xF7, 0xBA, 0x6D, 0x20,
	0xD5, 0x98, 0x4F, 0x02, 0xAC, 0xE1, 0x36, 0x7B, 0x27, 0x6A, 0xBD, 0xF0, 0x5E, 0x13, 0xC4, 0x89,
	0x63, 0x2E, 0xF9, 0xB4, 0x1A, 0x57, 0x80, 0xCD, 0x91, 0xDC, 0x0B, 0x46, 0xE8, 0xA5, 0x72, 0x3F,
	0xCA, 0x87, 0x50, 0x1D, 0xB3, 0xFE, 0x29, 0x64, 0x38, 0x75, 0xA2, 0xEF, 0x41, 0x0C, 0xDB, 0x96,
	0x42, 0x0F, 0xD8, 0x95, 0x3B, 0x76, 0xA1, 0xEC, 0xB0, 0xFD, 0x2A, 0x67, 0xC9, 0x84, 0x53, 0x1E,
	0xEB, 0xA6, 0x71, 0x3C, 0x92, 0xDF, 0x08, 0x45, 0x19, 0x54, 0x83, 0xCE, 0x60, 0x2D, 0xFA, 0xB7,
	0x5D, 0x10, 0xC7, 0x8A, 0x24, 0x69, 0xBE, 0xF3, 0xAF, 0xE2, 0x35, 0x78, 0xD6, 0x9B, 0x4C, 0x01,
	0xF4, 0xB9, 0x6E, 0x23, 0x8D, 0xC0, 0x17, 0x5A, 0x06, 0x4B, 0x9C, 0xD1, 0x7F, 0x32, 0xE5, 0xA8
};

static int DevAPageYRAM(struct tas2559_priv *pTAS2559,
			struct TYCRC *pCRCData,
			unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2559_YRAM_BOOK1) {
		if (nPage == TAS2559_YRAM1_PAGE) {
			if (nReg >= TAS2559_YRAM1_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else if ((nReg + len) > TAS2559_YRAM1_START_REG) {
				pCRCData->mnOffset = TAS2559_YRAM1_START_REG;
				pCRCData->mnLen = len - (TAS2559_YRAM1_START_REG - nReg);
				nResult = 1;
			} else
				nResult = 0;
		} else if (nPage == TAS2559_YRAM3_PAGE) {
			if (nReg > TAS2559_YRAM3_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2559_YRAM3_START_REG) {
				if ((nReg + len) > TAS2559_YRAM3_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2559_YRAM3_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2559_YRAM3_START_REG) {
					nResult = 0;
				} else if ((nReg + (len - 1)) <= TAS2559_YRAM3_END_REG) {
					pCRCData->mnOffset = TAS2559_YRAM3_START_REG;
					pCRCData->mnLen = len - (TAS2559_YRAM3_START_REG - nReg);
					nResult = 1;
				} else {
					pCRCData->mnOffset = TAS2559_YRAM3_START_REG;
					pCRCData->mnLen = TAS2559_YRAM3_END_REG - TAS2559_YRAM3_START_REG + 1;
					nResult = 1;
				}
			}
		}
	} else if (nBook == TAS2559_YRAM_BOOK2) {
		if (nPage == TAS2559_YRAM5_PAGE) {
			if (nReg > TAS2559_YRAM5_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2559_YRAM5_START_REG) {
				if ((nReg + len) > TAS2559_YRAM5_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2559_YRAM5_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2559_YRAM5_START_REG) {
					nResult = 0;
				} else if ((nReg + (len - 1)) <= TAS2559_YRAM5_END_REG) {
					pCRCData->mnOffset = TAS2559_YRAM5_START_REG;
					pCRCData->mnLen = len - (TAS2559_YRAM5_START_REG - nReg);
					nResult = 1;
				} else {
					pCRCData->mnOffset = TAS2559_YRAM5_START_REG;
					pCRCData->mnLen = TAS2559_YRAM5_END_REG - TAS2559_YRAM5_START_REG + 1;
					nResult = 1;
				}
			}
		}
	} else if (nBook == TAS2559_YRAM_BOOK3) {
		if (nPage == TAS2559_YRAM6_PAGE) {
			if (nReg > TAS2559_YRAM6_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2559_YRAM6_START_REG) {
				if ((nReg + len) > TAS2559_YRAM6_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2559_YRAM6_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2559_YRAM6_START_REG) {
					nResult = 0;
				} else if ((nReg + (len - 1)) <= TAS2559_YRAM6_END_REG) {
					pCRCData->mnOffset = TAS2559_YRAM6_START_REG;
					pCRCData->mnLen = len - (TAS2559_YRAM6_START_REG - nReg);
					nResult = 1;
				} else {
					pCRCData->mnOffset = TAS2559_YRAM6_START_REG;
					pCRCData->mnLen = TAS2559_YRAM6_END_REG - TAS2559_YRAM6_START_REG + 1;
					nResult = 1;
				}
			}
		}
	} else {
		nResult = 0;
	}

	return nResult;
}

static int isInPageYRAM(struct tas2559_priv *pTAS2559,
			enum channel dev, struct TYCRC *pCRCData,
			unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (dev == DevA)
		nResult = DevAPageYRAM(pTAS2559, pCRCData, nBook, nPage, nReg, len);

	return nResult;
}

static int DevABlockYRAM(struct tas2559_priv *pTAS2559,
			 struct TYCRC *pCRCData,
			 unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2559_YRAM_BOOK1) {
		if (nPage < TAS2559_YRAM2_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2559_YRAM2_END_PAGE) {
			if (nReg > TAS2559_YRAM2_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2559_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2559_YRAM2_START_REG) {
					nResult = 0;
				} else {
					pCRCData->mnOffset = TAS2559_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2559_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else {
			nResult = 0;
		}
	} else if (nBook == TAS2559_YRAM_BOOK2) {
		if (nPage < TAS2559_YRAM4_START_PAGE) {
			nResult = 0;
		} else if (nPage <= TAS2559_YRAM4_END_PAGE) {
			if ((nPage == TAS2559_PAGE_ID(TAS2559_SA_COEFF_SWAP_REG))
				&& (nReg == TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG))
				&& (len == 4)) {
				dev_dbg(pTAS2559->dev, "bypass swap\n");
				nResult = 0;
			} else if (nReg > TAS2559_YRAM2_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2559_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2559_YRAM2_START_REG) {
					nResult = 0;
				} else {
					pCRCData->mnOffset = TAS2559_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2559_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else {
			nResult = 0;
		}
	} else {
			nResult = 0;
	}

	return nResult;
}

static int DevBBlockYRAM(struct tas2559_priv *pTAS2559,
			 struct TYCRC *pCRCData,
			 unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (nBook == TAS2560_YRAM_BOOK) {
		if (nPage < TAS2560_YRAM_START_PAGE) {
			nResult = 0;
		} else if (nPage <= TAS2560_YRAM_END_PAGE) {
			if (nReg > TAS2560_YRAM_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2560_YRAM_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2560_YRAM_START_REG) {
					nResult = 0;
				} else {
					pCRCData->mnOffset = TAS2560_YRAM_START_REG;
					pCRCData->mnLen = nReg + len - TAS2560_YRAM_START_REG;
					nResult = 1;
				}
			}
		} else {
				nResult = 0;
		}
	}

	return nResult;
}

static int isInBlockYRAM(struct tas2559_priv *pTAS2559,
			 enum channel dev, struct TYCRC *pCRCData,
			 unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;

	if (dev == DevA)
		nResult = DevABlockYRAM(pTAS2559, pCRCData, nBook, nPage, nReg, len);
	else
		if (dev == DevB)
			nResult = DevBBlockYRAM(pTAS2559, pCRCData, nBook, nPage, nReg, len);

	return nResult;
}


static int isYRAM(struct tas2559_priv *pTAS2559,
		  enum channel dev, struct TYCRC *pCRCData,
		  unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult;

	nResult = isInPageYRAM(pTAS2559, dev, pCRCData, nBook, nPage, nReg, len);

	if (nResult == 0)
		nResult = isInBlockYRAM(pTAS2559, dev, pCRCData, nBook, nPage, nReg, len);

	return nResult;
}

/*
 * crc8 - calculate a crc8 over the given input data.
 *
 * table: crc table used for calculation.
 * pdata: pointer to data buffer.
 * nbytes: number of bytes in data buffer.
 * crc:	previous returned crc8 value.
 */
static u8 ti_crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc)
{
	/* loop over the buffer data */
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];

	return crc;
}

static int doSingleRegCheckSum(struct tas2559_priv *pTAS2559, enum channel chl,
			       unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char nValue)
{
	int nResult = 0;
	struct TYCRC sCRCData;
	unsigned int nData1 = 0, nData2 = 0;
	unsigned char nRegVal;

	if (chl == DevA) {
		if ((nBook == TAS2559_BOOK_ID(TAS2559_SA_COEFF_SWAP_REG))
		    && (nPage == TAS2559_PAGE_ID(TAS2559_SA_COEFF_SWAP_REG))
		    && (nReg >= TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG))
		    && (nReg <= (TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG) + 4))) {
			/* DSP swap command, pass */
			nResult = 0;
			goto end;
		}
	}

	nResult = isYRAM(pTAS2559, chl, &sCRCData, nBook, nPage, nReg, 1);

	if (nResult == 1) {
		if (chl == DevA) {
			nResult = pTAS2559->read(pTAS2559, DevA, TAS2559_REG(nBook, nPage, nReg), &nData1);

			if (nResult < 0)
				goto end;
		} else if (chl == DevB) {
			nResult = pTAS2559->read(pTAS2559, DevB, TAS2559_REG(nBook, nPage, nReg), &nData2);

			if (nResult < 0)
				goto end;
		}

		if (chl == DevA) {
			if (nData1 != nValue) {
				dev_err(pTAS2559->dev,
					"error2 (line %d),B[0x%x]P[0x%x]R[0x%x] W[0x%x], R[0x%x]\n",
					__LINE__, nBook, nPage, nReg, nValue, nData1);
				nResult = -EAGAIN;
				pTAS2559->mnErrCode |= ERROR_YRAM_CRCCHK;
				goto end;
			}

			nRegVal = nData1;
		} else if (chl == DevB) {
			if (nData2 != nValue) {
				dev_err(pTAS2559->dev,
					"error (line %d),B[0x%x]P[0x%x]R[0x%x] W[0x%x], R[0x%x]\n",
					__LINE__, nBook, nPage, nReg, nValue, nData2);
				nResult = -EAGAIN;
				pTAS2559->mnErrCode |= ERROR_YRAM_CRCCHK;
				goto end;
			}

			nRegVal = nData2;
		} else {
			nResult = -EINVAL;
			goto end;
		}

		nResult = ti_crc8(crc8_lookup_table, &nRegVal, 1, 0);
	}

end:

	return nResult;
}

static int doMultiRegCheckSum(struct tas2559_priv *pTAS2559, enum channel chl,
			      unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned int len)
{
	int nResult = 0, i;
	unsigned char nCRCChkSum = 0;
	unsigned char nBuf1[128];
	unsigned char nBuf2[128];
	struct TYCRC TCRCData;
	unsigned char *pRegVal;

	if ((nReg + len - 1) > 127) {
		nResult = -EINVAL;
		dev_err(pTAS2559->dev, "firmware error\n");
		goto end;
	}

	if ((nBook == TAS2559_BOOK_ID(TAS2559_SA_COEFF_SWAP_REG))
	    && (nPage == TAS2559_PAGE_ID(TAS2559_SA_COEFF_SWAP_REG))
	    && (nReg == TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG))
	    && (len == 4)) {
		/* DSP swap command, pass */
		nResult = 0;
		goto end;
	}

	nResult = isYRAM(pTAS2559, chl, &TCRCData, nBook, nPage, nReg, len);

	if (nResult == 1) {
		if (len == 1) {
			dev_err(pTAS2559->dev, "firmware error\n");
			nResult = -EINVAL;
			goto end;
		} else {
			if (chl == DevA) {
				nResult = pTAS2559->bulk_read(pTAS2559, DevA,
							      TAS2559_REG(nBook, nPage, TCRCData.mnOffset), nBuf1, TCRCData.mnLen);

				if (nResult < 0)
					goto end;
			} else if (chl == DevB) {
				nResult = pTAS2559->bulk_read(pTAS2559, DevB,
							      TAS2559_REG(nBook, nPage, TCRCData.mnOffset), nBuf2, TCRCData.mnLen);

				if (nResult < 0)
					goto end;
			}

			if (chl == DevA) {
				pRegVal = nBuf1;
			} else if (chl == DevB)
				pRegVal = nBuf2;
			else {
				dev_err(pTAS2559->dev, "channel error %d\n", chl);
				nResult = -EINVAL;
				goto end;
			}

			for (i = 0; i < TCRCData.mnLen; i++) {
				if ((nBook == TAS2559_BOOK_ID(TAS2559_SA_COEFF_SWAP_REG))
				    && (nPage == TAS2559_PAGE_ID(TAS2559_SA_COEFF_SWAP_REG))
				    && ((i + TCRCData.mnOffset)
					>= TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG))
				    && ((i + TCRCData.mnOffset)
					<= (TAS2559_PAGE_REG(TAS2559_SA_COEFF_SWAP_REG) + 4))) {
					/* DSP swap command, bypass */
					continue;
				} else {
					nCRCChkSum += ti_crc8(crc8_lookup_table, &pRegVal[i], 1, 0);
				}
			}

			nResult = nCRCChkSum;
		}
	}

end:

	return nResult;
}

static int tas2559_load_block(struct tas2559_priv *pTAS2559, struct TBlock *pBlock)
{
	int nResult = 0;
	unsigned int nCommand = 0;
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nOffset;
	unsigned char nData;
	unsigned int nValue1;
	unsigned int nLength;
	unsigned int nSleep;
	bool bDoYCRCChk = false;
	enum channel chl;
	unsigned char nCRCChkSum = 0;
	int nRetry = 6;
	unsigned char *pData = pBlock->mpData;

	dev_dbg(pTAS2559->dev, "%s: Type = %d, commands = %d\n", __func__,
		pBlock->mnType, pBlock->mnCommands);

	if (pBlock->mnType == TAS2559_BLOCK_PLL) {
		chl = DevA;
	} else if ((pBlock->mnType == TAS2559_BLOCK_PGM_DEV_A)
		    || (pBlock->mnType == TAS2559_BLOCK_CFG_COEFF_DEV_A)
		    || (pBlock->mnType == TAS2559_BLOCK_CFG_PRE_DEV_A)) {
		chl = DevA;
	} else if ((pBlock->mnType == TAS2559_BLOCK_PGM_DEV_B)
		    || (pBlock->mnType == TAS2559_BLOCK_PST_POWERUP_DEV_B)
		    || (pBlock->mnType == TAS2559_BLOCK_CFG_PRE_DEV_B)) {
		chl = DevB;
	} else {
		dev_err(pTAS2559->dev, "block type error %d\n", pBlock->mnType);
		nResult = -EINVAL;
		goto end;
	}

	if (pBlock->mbYChkSumPresent && pTAS2559->mbYCRCEnable)
		bDoYCRCChk = true;

start:

	if (pBlock->mbPChkSumPresent) {
		if (chl == DevA) {
			nResult = pTAS2559->write(pTAS2559, DevA, TAS2559_CRC_RESET_REG, 1);
			if (nResult < 0)
				goto end;
		} else {
			nResult = pTAS2559->write(pTAS2559, DevB, TAS2560_CRC_CHK_REG, 1);
			if (nResult < 0)
				goto end;
		}
	}

	if (bDoYCRCChk)
		nCRCChkSum = 0;

	nCommand = 0;

	while (nCommand < pBlock->mnCommands) {
		pData = pBlock->mpData + nCommand * 4;
		nBook = pData[0];
		nPage = pData[1];
		nOffset = pData[2];
		nData = pData[3];
		nCommand++;

		if (nOffset <= 0x7F) {
			nResult = pTAS2559->write(pTAS2559,
						chl, TAS2559_REG(nBook, nPage, nOffset), nData);
			if (nResult < 0)
				goto end;

			if (bDoYCRCChk) {
				nResult = doSingleRegCheckSum(pTAS2559,
							chl, nBook, nPage, nOffset, nData);
				if (nResult < 0)
					goto check;
				nCRCChkSum += (unsigned char)nResult;
			}
		} else if (nOffset == 0x81) {
			nSleep = (nBook << 8) + nPage;
			msleep(nSleep);
		} else if (nOffset == 0x85) {
			pData += 4;
			nLength = (nBook << 8) + nPage;
			nBook = pData[0];
			nPage = pData[1];
			nOffset = pData[2];

			if (nLength > 1) {
				nResult = pTAS2559->bulk_write(pTAS2559,
							chl, TAS2559_REG(nBook, nPage, nOffset), pData + 3, nLength);
				if (nResult < 0)
					goto end;

				if (bDoYCRCChk) {
					nResult = doMultiRegCheckSum(pTAS2559,
								chl, nBook, nPage, nOffset, nLength);
					if (nResult < 0)
						goto check;

					nCRCChkSum += (unsigned char)nResult;
				}
			} else {
				nResult = pTAS2559->write(pTAS2559,
							chl, TAS2559_REG(nBook, nPage, nOffset), pData[3]);
				if (nResult < 0)
					goto end;

				if (bDoYCRCChk) {
					nResult = doSingleRegCheckSum(pTAS2559,
								chl, nBook, nPage, nOffset, pData[3]);
					if (nResult < 0)
						goto check;

					nCRCChkSum += (unsigned char)nResult;
				}
			}

			nCommand++;

			if (nLength >= 2)
				nCommand += ((nLength - 2) / 4) + 1;
		}
	}

	if (pBlock->mbPChkSumPresent) {
		if (chl == DevA)
			nResult = pTAS2559->read(pTAS2559, DevA,
						TAS2559_CRC_CHECKSUM_REG, &nValue1);
		else
			nResult = pTAS2559->read(pTAS2559, DevB,
						TAS2560_CRC_CHK_REG, &nValue1);

		if (nResult < 0)
			goto end;

		if (nValue1 != pBlock->mnPChkSum) {
			dev_err(pTAS2559->dev, "Block PChkSum Error: FW = 0x%x, Reg = 0x%x\n",
				pBlock->mnPChkSum, (nValue1 & 0xff));
			nResult = -EAGAIN;
			pTAS2559->mnErrCode |= ERROR_PRAM_CRCCHK;
			goto check;
		}

		nResult = 0;
		pTAS2559->mnErrCode &= ~ERROR_PRAM_CRCCHK;
		dev_dbg(pTAS2559->dev, "Block[0x%x] PChkSum match\n", pBlock->mnType);
	}

	if (bDoYCRCChk) {
		if (nCRCChkSum != pBlock->mnYChkSum) {
			dev_err(pTAS2559->dev, "Block YChkSum Error: FW = 0x%x, YCRC = 0x%x\n",
				pBlock->mnYChkSum, nCRCChkSum);
			nResult = -EAGAIN;
			pTAS2559->mnErrCode |= ERROR_YRAM_CRCCHK;
			goto check;
		}

		pTAS2559->mnErrCode &= ~ERROR_YRAM_CRCCHK;
		nResult = 0;
		dev_dbg(pTAS2559->dev, "Block[0x%x] YChkSum match\n", pBlock->mnType);
	}

check:

	if (nResult == -EAGAIN) {
		nRetry--;

		if (nRetry > 0)
			goto start;
	}

end:

	if (nResult < 0)
		dev_err(pTAS2559->dev, "Block (%d) load error\n",
			pBlock->mnType);

	return nResult;
}

static int tas2559_load_data(struct tas2559_priv *pTAS2559, struct TData *pData, unsigned int nType)
{
	int nResult = 0;
	unsigned int nBlock;
	struct TBlock *pBlock;

	dev_dbg(pTAS2559->dev,
		"TAS2559 load data: %s, Blocks = %d, Block Type = %d\n", pData->mpName, pData->mnBlocks, nType);

	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);

		if (pBlock->mnType == nType) {
			nResult = tas2559_load_block(pTAS2559, pBlock);

			if (nResult < 0)
				break;
		}
	}

	return nResult;
}

static int tas2559_load_configuration(struct tas2559_priv *pTAS2559,
				      unsigned int nConfiguration, bool bLoadSame)
{
	int nResult = 0;
	struct TConfiguration *pCurrentConfiguration = NULL;
	struct TConfiguration *pNewConfiguration = NULL;

	dev_dbg(pTAS2559->dev, "%s: %d\n", __func__, nConfiguration);

	if ((!pTAS2559->mpFirmware->mpPrograms) ||
	    (!pTAS2559->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2559->dev, "Firmware not loaded\n");
		nResult = 0;
		goto end;
	}

	if (nConfiguration >= pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = 0;
		goto end;
	}

	if ((!pTAS2559->mbLoadConfigurationPrePowerUp)
	    && (nConfiguration == pTAS2559->mnCurrentConfiguration)
	    && (!bLoadSame)) {
		dev_info(pTAS2559->dev, "Configuration %d is already loaded\n",
			 nConfiguration);
		nResult = 0;
		goto end;
	}

	pCurrentConfiguration =
		&(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);
	pNewConfiguration =
		&(pTAS2559->mpFirmware->mpConfigurations[nConfiguration]);

	if (pNewConfiguration->mnProgram != pCurrentConfiguration->mnProgram) {
		dev_err(pTAS2559->dev, "Configuration %d, %s doesn't share the same program as current %d\n",
			nConfiguration, pNewConfiguration->mpName, pCurrentConfiguration->mnProgram);
		nResult = 0;
		goto end;
	}

	if (pNewConfiguration->mnPLL >= pTAS2559->mpFirmware->mnPLLs) {
		dev_err(pTAS2559->dev, "Configuration %d, %s doesn't have a valid PLL index %d\n",
			nConfiguration, pNewConfiguration->mpName, pNewConfiguration->mnPLL);
		nResult = 0;
		goto end;
	}

	if (pTAS2559->mbPowerUp) {
		dev_err(pTAS2559->dev, "%s, device power on, load new conf[%d] %s\n", __func__,
			nConfiguration, pNewConfiguration->mpName);
		nResult = tas2559_load_coefficient(pTAS2559, pTAS2559->mnCurrentConfiguration, nConfiguration, true);
		pTAS2559->mbLoadConfigurationPrePowerUp = false;
	} else {
		dev_dbg(pTAS2559->dev,
			"TAS2559 was powered down, will load coefficient when power up\n");
		pTAS2559->mbLoadConfigurationPrePowerUp = true;
		pTAS2559->mnNewConfiguration = nConfiguration;
	}

end:

	if (nResult < 0) {
		if (pTAS2559->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2559);
	}

	return nResult;
}

int tas2559_set_config(struct tas2559_priv *pTAS2559, int config)
{
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	unsigned int nProgram = pTAS2559->mnCurrentProgram;
	unsigned int nConfiguration = config;
	int nResult = 0;

	if ((!pTAS2559->mpFirmware->mpPrograms) ||
	    (!pTAS2559->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2559->dev, "Firmware not loaded\n");
		nResult = -EINVAL;
		goto end;
	}

	if (nConfiguration >= pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = -EINVAL;
		goto end;
	}

	pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[nConfiguration]);
	pProgram = &(pTAS2559->mpFirmware->mpPrograms[nProgram]);

	if (nProgram != pConfiguration->mnProgram) {
		dev_err(pTAS2559->dev,
			"Configuration %d, %s with Program %d isn't compatible with existing Program %d, %s\n",
			nConfiguration, pConfiguration->mpName, pConfiguration->mnProgram,
			nProgram, pProgram->mpName);
		nResult = -EINVAL;
		goto end;
	}

	dev_dbg(pTAS2559->dev, "%s, load new conf %s\n", __func__, pConfiguration->mpName);
	nResult = tas2559_load_configuration(pTAS2559, nConfiguration, false);

end:

	return nResult;
}

void tas2559_clear_firmware(struct TFirmware *pFirmware)
{
	unsigned int n, nn;

	if (!pFirmware)
		return;

	kfree(pFirmware->mpDescription);

	if (pFirmware->mpPLLs != NULL) {
		for (n = 0; n < pFirmware->mnPLLs; n++) {
			kfree(pFirmware->mpPLLs[n].mpDescription);
			kfree(pFirmware->mpPLLs[n].mBlock.mpData);
		}

		kfree(pFirmware->mpPLLs);
	}

	if (pFirmware->mpPrograms != NULL) {
		for (n = 0; n < pFirmware->mnPrograms; n++) {
			kfree(pFirmware->mpPrograms[n].mpDescription);
			kfree(pFirmware->mpPrograms[n].mData.mpDescription);

			for (nn = 0; nn < pFirmware->mpPrograms[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpPrograms[n].mData.mpBlocks[nn].mpData);

			kfree(pFirmware->mpPrograms[n].mData.mpBlocks);
		}

		kfree(pFirmware->mpPrograms);
	}

	if (pFirmware->mpConfigurations != NULL) {
		for (n = 0; n < pFirmware->mnConfigurations; n++) {
			kfree(pFirmware->mpConfigurations[n].mpDescription);
			kfree(pFirmware->mpConfigurations[n].mData.mpDescription);

			for (nn = 0; nn < pFirmware->mpConfigurations[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpConfigurations[n].mData.mpBlocks[nn].mpData);

			kfree(pFirmware->mpConfigurations[n].mData.mpBlocks);
		}

		kfree(pFirmware->mpConfigurations);
	}

	if (pFirmware->mpCalibrations != NULL) {
		for (n = 0; n < pFirmware->mnCalibrations; n++) {
			kfree(pFirmware->mpCalibrations[n].mpDescription);
			kfree(pFirmware->mpCalibrations[n].mData.mpDescription);

			for (nn = 0; nn < pFirmware->mpCalibrations[n].mData.mnBlocks; nn++)
				kfree(pFirmware->mpCalibrations[n].mData.mpBlocks[nn].mpData);

			kfree(pFirmware->mpCalibrations[n].mData.mpBlocks);
		}

		kfree(pFirmware->mpCalibrations);
	}

	memset(pFirmware, 0x00, sizeof(struct TFirmware));
}

static int tas2559_load_calibration(struct tas2559_priv *pTAS2559,	char *pFileName)
{
	int nResult = 0;
	int nFile;
	mm_segment_t fs;
	unsigned char pBuffer[1000];
	int nSize = 0;

	dev_dbg(pTAS2559->dev, "%s:\n", __func__);

	fs = get_fs();
	set_fs(KERNEL_DS);
	nFile = sys_open(pFileName, O_RDONLY, 0);

	dev_info(pTAS2559->dev, "TAS2559 calibration file = %s, handle = %d\n",
		 pFileName, nFile);

	if (nFile >= 0) {
		nSize = sys_read(nFile, pBuffer, 1000);
		sys_close(nFile);
	} else {
		dev_err(pTAS2559->dev, "TAS2559 cannot open calibration file: %s\n",
			pFileName);
		nResult = -EINVAL;
	}

	set_fs(fs);

	if (!nSize)
		goto end;

	tas2559_clear_firmware(pTAS2559->mpCalFirmware);
	dev_info(pTAS2559->dev, "TAS2559 calibration file size = %d\n", nSize);
	nResult = fw_parse(pTAS2559, pTAS2559->mpCalFirmware, pBuffer, nSize);

	if (nResult)
		dev_err(pTAS2559->dev, "TAS2559 calibration file is corrupt\n");
	else
		dev_info(pTAS2559->dev, "TAS2559 calibration: %d calibrations\n",
			 pTAS2559->mpCalFirmware->mnCalibrations);

end:

	return nResult;
}

void tas2559_fw_ready(const struct firmware *pFW, void *pContext)
{
	struct tas2559_priv *pTAS2559 = (struct tas2559_priv *) pContext;
	int nResult;
	unsigned int nProgram = 0;
	unsigned int nSampleRate = 0;

#ifdef CONFIG_TAS2559_CODEC
	mutex_lock(&pTAS2559->codec_lock);
#endif

#ifdef CONFIG_TAS2559_MISC
	mutex_lock(&pTAS2559->file_lock);
#endif
	dev_info(pTAS2559->dev, "%s:\n", __func__);

	if (unlikely(!pFW) || unlikely(!pFW->data)) {
		dev_err(pTAS2559->dev, "%s firmware is not loaded.\n",
			TAS2559_FW_NAME);
		goto end;
	}

	if (pTAS2559->mpFirmware->mpConfigurations) {
		nProgram = pTAS2559->mnCurrentProgram;
		nSampleRate = pTAS2559->mnCurrentSampleRate;
		dev_dbg(pTAS2559->dev, "clear current firmware\n");
		tas2559_clear_firmware(pTAS2559->mpFirmware);
	}

	nResult = fw_parse(pTAS2559, pTAS2559->mpFirmware, (unsigned char *)(pFW->data), pFW->size);
	release_firmware(pFW);

	if (nResult < 0) {
		dev_err(pTAS2559->dev, "firmware is corrupt\n");
		goto end;
	}

	if (!pTAS2559->mpFirmware->mnPrograms) {
		dev_err(pTAS2559->dev, "firmware contains no programs\n");
		nResult = -EINVAL;
		goto end;
	}

	if (!pTAS2559->mpFirmware->mnConfigurations) {
		dev_err(pTAS2559->dev, "firmware contains no configurations\n");
		nResult = -EINVAL;
		goto end;
	}

	if (nProgram >= pTAS2559->mpFirmware->mnPrograms) {
		dev_info(pTAS2559->dev,
			 "no previous program, set to default\n");
		nProgram = 0;
	}

	pTAS2559->mnCurrentSampleRate = nSampleRate;
	nResult = tas2559_set_program(pTAS2559, nProgram, -1);

end:

#ifdef CONFIG_TAS2559_CODEC
	mutex_unlock(&pTAS2559->codec_lock);
#endif

#ifdef CONFIG_TAS2559_MISC
	mutex_unlock(&pTAS2559->file_lock);
#endif
}

int tas2559_set_program(struct tas2559_priv *pTAS2559,
			unsigned int nProgram, int nConfig)
{
	struct TProgram *pProgram;
	struct TConfiguration *pConfiguration;
	unsigned int nConfiguration = 0;
	unsigned int nSampleRate = 0;
	bool bFound = false;
	int nResult = 0;

	if ((!pTAS2559->mpFirmware->mpPrograms) ||
	    (!pTAS2559->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2559->dev, "Firmware not loaded\n");
		nResult = 0;
		goto end;
	}

	if (nProgram >= pTAS2559->mpFirmware->mnPrograms) {
		dev_err(pTAS2559->dev, "TAS2559: Program %d doesn't exist\n",
			nProgram);
		nResult = 0;
		goto end;
	}

	if (nConfig < 0) {
		nConfiguration = 0;
		nSampleRate = pTAS2559->mnCurrentSampleRate;

		while (!bFound && (nConfiguration < pTAS2559->mpFirmware->mnConfigurations)) {
			if (pTAS2559->mpFirmware->mpConfigurations[nConfiguration].mnProgram == nProgram) {
				if (nSampleRate == 0) {
					bFound = true;
					dev_info(pTAS2559->dev, "find default configuration %d\n", nConfiguration);
				} else if (nSampleRate == pTAS2559->mpFirmware->mpConfigurations[nConfiguration].mnSamplingRate) {
					bFound = true;
					dev_info(pTAS2559->dev, "find matching configuration %d\n", nConfiguration);
				} else {
					nConfiguration++;
				}
			} else {
				nConfiguration++;
			}
		}

		if (!bFound) {
			dev_err(pTAS2559->dev,
				"Program %d, no valid configuration found for sample rate %d, ignore\n",
				nProgram, nSampleRate);
			nResult = 0;
			goto end;
		}
	} else {
		if (pTAS2559->mpFirmware->mpConfigurations[nConfig].mnProgram != nProgram) {
			dev_err(pTAS2559->dev, "%s, configuration program doesn't match\n", __func__);
			nResult = 0;
			goto end;
		}

		nConfiguration = nConfig;
	}

	pProgram = &(pTAS2559->mpFirmware->mpPrograms[nProgram]);

	if (pTAS2559->mbPowerUp) {
		dev_info(pTAS2559->dev,
			 "device powered up, power down to load program %d (%s)\n",
			 nProgram, pProgram->mpName);

		if (hrtimer_active(&pTAS2559->mtimer))
			hrtimer_cancel(&pTAS2559->mtimer);

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE)
			pTAS2559->enableIRQ(pTAS2559, DevBoth, false);

		nResult = tas2559_DevShutdown(pTAS2559, DevBoth);

		if (nResult < 0)
			goto end;
	}

	pTAS2559->hw_reset(pTAS2559);
	nResult = pTAS2559->write(pTAS2559, DevBoth, TAS2559_SW_RESET_REG, 0x01);
	if (nResult < 0)
		goto end;

	msleep(1);
	nResult = tas2559_load_default(pTAS2559);
	if (nResult < 0)
		goto end;

	dev_info(pTAS2559->dev, "load program %d (%s)\n", nProgram, pProgram->mpName);
	nResult = tas2559_load_data(pTAS2559, &(pProgram->mData), TAS2559_BLOCK_PGM_DEV_A);
	if (nResult < 0)
		goto end;

	nResult = tas2559_load_data(pTAS2559, &(pProgram->mData), TAS2559_BLOCK_PGM_DEV_B);
	if (nResult < 0)
		goto end;

	pTAS2559->mnCurrentProgram = nProgram;
	pTAS2559->mnDevGain = 15;
	pTAS2559->mnDevCurrentGain = 15;

	nResult = tas2559_load_coefficient(pTAS2559, -1, nConfiguration, false);
	if (nResult < 0)
		goto end;

	if (pTAS2559->mbPowerUp) {
		dev_info(pTAS2559->dev, "%s, load VBoost before power on %d\n", __func__, pTAS2559->mnVBoostState);
		nResult = tas2559_set_VBoost(pTAS2559, pTAS2559->mnVBoostState, false);
		if (nResult < 0)
			goto end;

		pTAS2559->clearIRQ(pTAS2559);
		pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);
		nResult = tas2559_DevStartup(pTAS2559, pConfiguration->mnDevices);
		if (nResult < 0)
			goto end;

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
			nResult = tas2559_checkPLL(pTAS2559);
			if (nResult < 0) {
				nResult = tas2559_DevShutdown(pTAS2559, pConfiguration->mnDevices);
				pTAS2559->mbPowerUp = false;
				goto end;
			}
		}

		if (pConfiguration->mnDevices & DevB) {
			nResult = tas2559_load_data(pTAS2559, &(pConfiguration->mData),
						TAS2559_BLOCK_PST_POWERUP_DEV_B);
			if (nResult < 0)
				goto end;
		}

		nResult = tas2559_DevMute(pTAS2559, pConfiguration->mnDevices, false);
		if (nResult < 0)
			goto end;

		if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
			pTAS2559->enableIRQ(pTAS2559, pConfiguration->mnDevices, true);

			if (!hrtimer_active(&pTAS2559->mtimer)) {
				pTAS2559->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2559->mtimer,
					      ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}

end:

	if (nResult < 0) {
		if (pTAS2559->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_DEVB_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2559);
	}

	return nResult;
}

int tas2559_set_calibration(struct tas2559_priv *pTAS2559, int nCalibration)
{
	struct TCalibration *pCalibration = NULL;
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	int nResult = 0;

	if ((!pTAS2559->mpFirmware->mpPrograms)
	    || (!pTAS2559->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2559->dev, "Firmware not loaded\n\r");
		nResult = 0;
		goto end;
	}

	if (nCalibration == 0x00FF) {
		dev_info(pTAS2559->dev, "load new calibration file %s\n", TAS2559_CAL_NAME);
		nResult = tas2559_load_calibration(pTAS2559, TAS2559_CAL_NAME);

		if (nResult < 0) {
			nResult = 0;
			goto end;
		}

		nCalibration = 0;
	}

	if (nCalibration >= pTAS2559->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2559->dev,
			"Calibration %d doesn't exist\n", nCalibration);
		nResult = 0;
		goto end;
	}

	pTAS2559->mnCurrentCalibration = nCalibration;

	if (pTAS2559->mbLoadConfigurationPrePowerUp)
		goto end;

	pCalibration = &(pTAS2559->mpCalFirmware->mpCalibrations[nCalibration]);
	pProgram = &(pTAS2559->mpFirmware->mpPrograms[pTAS2559->mnCurrentProgram]);
	pConfiguration = &(pTAS2559->mpFirmware->mpConfigurations[pTAS2559->mnCurrentConfiguration]);

	if (pProgram->mnAppMode == TAS2559_APP_TUNINGMODE) {
		dev_dbg(pTAS2559->dev, "Enable: load calibration\n");
		nResult = tas2559_load_data(pTAS2559, &(pCalibration->mData), TAS2559_BLOCK_CFG_COEFF_DEV_A);
	}

end:

	if (nResult < 0) {
		tas2559_clear_firmware(pTAS2559->mpCalFirmware);
		nResult = tas2559_set_program(pTAS2559, pTAS2559->mnCurrentProgram, pTAS2559->mnCurrentConfiguration);
	}

	return nResult;
}

int tas2559_get_Cali_prm_r0(struct tas2559_priv *pTAS2559, enum channel chl, int *prm_r0)
{
	int nResult = 0;
	int n, nn;
	struct TCalibration *pCalibration;
	struct TData *pData;
	struct TBlock *pBlock;
	int nReg;
	int nBook, nPage, nOffset;
	unsigned char *pCommands;
	int nCali_Re;
	bool bFound = false;
	int len;

	if (!pTAS2559->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2559->dev, "%s, no calibration data\n", __func__);
		goto end;
	}

	if (chl == DevA)
		nReg = TAS2559_DEVA_CALI_R0_REG;
	else if (chl == DevB)
		nReg = TAS2559_DEVB_CALI_R0_REG;
	else
		goto end;

	pCalibration = &(pTAS2559->mpCalFirmware->mpCalibrations[pTAS2559->mnCurrentCalibration]);
	pData = &(pCalibration->mData);

	for (n = 0; n < pData->mnBlocks; n++) {
		pBlock = &(pData->mpBlocks[n]);
		pCommands = pBlock->mpData;

		for (nn = 0 ; nn < pBlock->mnCommands;) {
			nBook = pCommands[4 * nn + 0];
			nPage = pCommands[4 * nn + 1];
			nOffset = pCommands[4 * nn + 2];

			if ((nOffset < 0x7f) || (nOffset == 0x81))
				nn++;
			else
				if (nOffset == 0x85) {
					len = ((int)nBook << 8) | nPage;

					nBook = pCommands[4 * nn + 4];
					nPage = pCommands[4 * nn + 5];
					nOffset = pCommands[4 * nn + 6];

					if ((nBook == TAS2559_BOOK_ID(nReg))
					    && (nPage == TAS2559_PAGE_ID(nReg))
					    && (nOffset == TAS2559_PAGE_REG(nReg))) {
						nCali_Re = ((int)pCommands[4 * nn + 7] << 24)
							   | ((int)pCommands[4 * nn + 8] << 16)
							   | ((int)pCommands[4 * nn + 9] << 8)
							   | (int)pCommands[4 * nn + 10];
						bFound = true;
						goto end;
					}

					nn += 2;
					nn += ((len - 1) / 4);

					if ((len - 1) % 4)
						nn++;
				} else {
					dev_err(pTAS2559->dev, "%s, format error %d\n", __func__, nOffset);
					break;
				}
		}
	}

end:

	if (bFound)
		*prm_r0 = nCali_Re;

	return nResult;
}

int tas2559_parse_dt(struct device *dev, struct tas2559_priv *pTAS2559)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;
	unsigned int value;

	pTAS2559->mnDevAGPIORST = of_get_named_gpio(np, "ti,tas2559-reset-gpio", 0);
	if (!gpio_is_valid(pTAS2559->mnDevAGPIORST))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2559-reset-gpio", np->full_name, pTAS2559->mnDevAGPIORST);
	else
		dev_dbg(pTAS2559->dev, "%s, tas2559 reset gpio %d\n", __func__, pTAS2559->mnDevAGPIORST);

	pTAS2559->mnDevBGPIORST = of_get_named_gpio(np, "ti,tas2560-reset-gpio", 0);
	if (!gpio_is_valid(pTAS2559->mnDevBGPIORST))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2560-reset-gpio", np->full_name, pTAS2559->mnDevBGPIORST);
	else
		dev_dbg(pTAS2559->dev, "%s, tas2560 reset gpio %d\n", __func__, pTAS2559->mnDevBGPIORST);

	pTAS2559->mnDevAGPIOIRQ = of_get_named_gpio(np, "ti,tas2559-irq-gpio", 0);
	if (!gpio_is_valid(pTAS2559->mnDevAGPIOIRQ))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2559-irq-gpio", np->full_name, pTAS2559->mnDevAGPIOIRQ);

	pTAS2559->mnDevBGPIOIRQ = of_get_named_gpio(np, "ti,tas2560-irq-gpio", 0);
	if (!gpio_is_valid(pTAS2559->mnDevBGPIOIRQ))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2560-irq-gpio", np->full_name, pTAS2559->mnDevBGPIOIRQ);

	rc = of_property_read_u32(np, "ti,tas2559-addr", &value);
	if (rc) {
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2559-addr", np->full_name, rc);
		ret = -EINVAL;
		goto end;
	} else {
		pTAS2559->mnDevAAddr = value;
		dev_dbg(pTAS2559->dev, "ti,tas2559 addr=0x%x\n", pTAS2559->mnDevAAddr);
	}

	rc = of_property_read_u32(np, "ti,tas2560-addr", &value);
	if (rc) {
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2560-addr", np->full_name, rc);
		ret = -EINVAL;
		goto end;
	} else {
		pTAS2559->mnDevBAddr = value;
		dev_dbg(pTAS2559->dev, "ti,tas2560-addr=0x%x\n", pTAS2559->mnDevBAddr);
	}

	rc = of_property_read_u32(np, "ti,tas2559-channel", &value);
	if (rc)
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2559-channel", np->full_name, rc);
	else
		pTAS2559->mnDevAChl = value;

	rc = of_property_read_u32(np, "ti,tas2560-channel", &value);
	if (rc)
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,tas2560-channel", np->full_name, rc);
	else
		pTAS2559->mnDevBChl = value;

	rc = of_property_read_u32(np, "ti,echo-ref", &value);
	if (rc)
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,echo-ref", np->full_name, rc);
	else
		pTAS2559->mnEchoRef = value;

	rc = of_property_read_u32(np, "ti,bit-rate", &value);
	if (rc)
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,i2s-bits", np->full_name, rc);
	else
		pTAS2559->mnBitRate = value;

	rc = of_property_read_u32(np, "ti,ycrc-enable", &value);
	if (rc)
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n",
			"ti,ycrc-enable", np->full_name, rc);
	else
		pTAS2559->mbYCRCEnable = (value != 0);

end:

	return ret;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2559 common functions for Android Linux");
MODULE_LICENSE("GPL v2");
