/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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
**     tas2557-core.c
**
** Description:
**     TAS2557 common functions for Android Linux
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
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/crc8.h>
#include "tas2557.h"
#include "tas2557-core.h"
#define	PPC_DRIVER_CRCCHK			0x00000200
#define	PPC_DRIVER_CONFDEV			0x00000300
#define	PPC_DRIVER_CFGDEV_NONCRC	0x00000101
#define TAS2557_CAL_NAME    "/persist/spkr_calibration.bin"
static int tas2557_load_calibration(struct tas2557_priv *pTAS2557,
	char *pFileName);
static int tas2557_load_data(struct tas2557_priv *pTAS2557, struct TData *pData,
	unsigned int nType);
static void tas2557_clear_firmware(struct TFirmware *pFirmware);
static int tas2557_load_block(struct tas2557_priv *pTAS2557, struct TBlock *pBlock);
static int tas2557_load_configuration(struct tas2557_priv *pTAS2557,
	unsigned int nConfiguration, bool bLoadSame);
#define TAS2557_UDELAY 0xFFFFFFFE
#define TAS2557_MDELAY 0xFFFFFFFD
#define TAS2557_BLOCK_PLL				0x00
#define TAS2557_BLOCK_PGM_ALL			0x0d
#define TAS2557_BLOCK_PGM_DEV_A			0x01
#define TAS2557_BLOCK_PGM_DEV_B			0x08
#define TAS2557_BLOCK_CFG_COEFF_DEV_A	0x03
#define TAS2557_BLOCK_CFG_COEFF_DEV_B	0x0a
#define TAS2557_BLOCK_CFG_PRE_DEV_A		0x04
#define TAS2557_BLOCK_CFG_PRE_DEV_B		0x0b
#define TAS2557_BLOCK_CFG_POST			0x05
#define TAS2557_BLOCK_CFG_POST_POWER	0x06
static unsigned int p_tas2557_default_data[] = {
	TAS2557_SAR_ADC2_REG, 0x05,
	TAS2557_CLK_ERR_CTRL2, 0x21,
	TAS2557_CLK_ERR_CTRL3, 0x21,
	TAS2557_SAFE_GUARD_REG, TAS2557_SAFE_GUARD_PATTERN,
	0xFFFFFFFF, 0xFFFFFFFF
};
static unsigned int p_tas2557_irq_config[] = {
	TAS2557_CLK_HALT_REG, 0x71,
	TAS2557_INT_GEN1_REG, 0x11,
	TAS2557_INT_GEN2_REG, 0x11,
	TAS2557_INT_GEN3_REG, 0x11,
	TAS2557_INT_GEN4_REG, 0x01,
	TAS2557_GPIO4_PIN_REG, 0x07,
	TAS2557_INT_MODE_REG, 0x80,
	0xFFFFFFFF, 0xFFFFFFFF
};
static unsigned int p_tas2557_startup_data[] = {
	TAS2557_GPI_PIN_REG, 0x15,
	TAS2557_GPIO1_PIN_REG, 0x01,
	TAS2557_GPIO2_PIN_REG, 0x01,
	TAS2557_POWER_CTRL2_REG, 0xA0,
	TAS2557_POWER_CTRL2_REG, 0xA3,
	TAS2557_POWER_CTRL1_REG, 0xF8,
	TAS2557_UDELAY, 2000,
	TAS2557_CLK_ERR_CTRL, 0x2b,
	0xFFFFFFFF, 0xFFFFFFFF
};
static unsigned int p_tas2557_unmute_data[] = {
	TAS2557_MUTE_REG, 0x00,
	TAS2557_SOFT_MUTE_REG, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};
static unsigned int p_tas2557_shutdown_data[] = {
	TAS2557_CLK_ERR_CTRL, 0x00,
	TAS2557_SOFT_MUTE_REG, 0x01,
	TAS2557_UDELAY, 10000,
	TAS2557_MUTE_REG, 0x03,
	TAS2557_POWER_CTRL1_REG, 0x60,
	TAS2557_UDELAY, 2000,
	TAS2557_POWER_CTRL2_REG, 0x00,
	TAS2557_POWER_CTRL1_REG, 0x00,
	TAS2557_GPIO1_PIN_REG, 0x00,
	TAS2557_GPIO2_PIN_REG, 0x00,
	TAS2557_GPI_PIN_REG, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};
static int tas2557_dev_load_data(struct tas2557_priv *pTAS2557,
	unsigned int *pData)
{
	int ret = 0;
	unsigned int n = 0;
	unsigned int nRegister;
	unsigned int nData;
	do {
		nRegister = pData[n * 2];
		nData = pData[n * 2 + 1];
		if (nRegister == TAS2557_UDELAY)
			udelay(nData);
		else if (nRegister != 0xFFFFFFFF) {
			ret = pTAS2557->write(pTAS2557, nRegister, nData);
			if (ret < 0)
				break;
		}
		n++;
	} while (nRegister != 0xFFFFFFFF);
	return ret;
}
int tas2557_configIRQ(struct tas2557_priv *pTAS2557)
{
	return tas2557_dev_load_data(pTAS2557, p_tas2557_irq_config);
}
int tas2557_set_bit_rate(struct tas2557_priv *pTAS2557, unsigned int nBitRate)
{
	int ret = 0, n = -1;
	dev_dbg(pTAS2557->dev, "tas2557_set_bit_rate: nBitRate = %d\n", nBitRate);
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
	if (n >= 0)
		ret = pTAS2557->update_bits(pTAS2557, TAS2557_ASI1_DAC_FORMAT_REG, 0x18, n<<3);
	return ret;
}
int tas2557_get_bit_rate(struct tas2557_priv *pTAS2557, unsigned char *pBitRate)
{
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char bitRate;
	ret = pTAS2557->read(pTAS2557, TAS2557_ASI1_DAC_FORMAT_REG, &nValue);
	if (ret >= 0) {
		bitRate = (nValue&0x18)>>3;
		if (bitRate == 0)
			bitRate = 16;
		else if (bitRate == 1)
			bitRate = 20;
		else if (bitRate == 2)
			bitRate = 24;
		else if (bitRate == 3)
			bitRate = 32;
		*pBitRate = bitRate;
	}
	return ret;
}
int tas2557_get_DAC_gain(struct tas2557_priv *pTAS2557, unsigned char *pnGain)
{
	int ret = 0;
	unsigned int nValue = 0;
	ret = pTAS2557->read(pTAS2557, TAS2557_SPK_CTRL_REG, &nValue);
	if (ret >= 0)
		*pnGain = ((nValue&TAS2557_DAC_GAIN_MASK)>>TAS2557_DAC_GAIN_SHIFT);
	return ret;
}
int tas2557_set_DAC_gain(struct tas2557_priv *pTAS2557, unsigned int nGain)
{
	int ret = 0;
	ret = pTAS2557->update_bits(pTAS2557, TAS2557_SPK_CTRL_REG, TAS2557_DAC_GAIN_MASK,
		(nGain<<TAS2557_DAC_GAIN_SHIFT));
	return ret;
}
int tas2557_get_die_temperature(struct tas2557_priv *pTAS2557, int *pTemperature)
{
	int nResult = 0;
	unsigned char nBuf[4];
	int temp;
	if (!pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}
	if (!pTAS2557->mbPowerUp) {
		dev_err(pTAS2557->dev, "%s, device not powered on\n", __func__);
		goto end;
	}
	nResult = pTAS2557->bulk_read(pTAS2557, TAS2557_DIE_TEMP_REG, nBuf, 4);
	if (nResult >= 0) {
		temp = ((int)nBuf[0] << 24) | ((int)nBuf[1] << 16) | ((int)nBuf[2] << 8) | nBuf[3];
		*pTemperature = temp;
	}
end:
	return nResult;
}
int tas2557_load_platdata(struct tas2557_priv *pTAS2557)
{
	int nResult = 0;
	if (gpio_is_valid(pTAS2557->mnGpioINT)) {
		nResult = tas2557_configIRQ(pTAS2557);
		if (nResult < 0)
			goto end;
	}
	nResult = tas2557_set_bit_rate(pTAS2557, pTAS2557->mnI2SBits);
end:
	return nResult;
}
int tas2557_load_default(struct tas2557_priv *pTAS2557)
{
	int nResult = 0;
	nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_default_data);
	if (nResult < 0)
		goto end;
	nResult = tas2557_load_platdata(pTAS2557);
	if (nResult < 0)
		goto end;
	nResult = pTAS2557->update_bits(pTAS2557, TAS2557_ASI1_DAC_FORMAT_REG, 0x01, 0x01);
end:
	return nResult;
}
static void failsafe(struct tas2557_priv *pTAS2557)
{
	dev_err(pTAS2557->dev, "%s\n", __func__);
	pTAS2557->mnErrCode |= ERROR_FAILSAFE;
	if (hrtimer_active(&pTAS2557->mtimer))
		hrtimer_cancel(&pTAS2557->mtimer);
#ifdef I2C_RESTART
	if (pTAS2557->mnRestart < 3) {
		pTAS2557->mnRestart ++;
		msleep(100);
		dev_err(pTAS2557->dev, "I2C COMM error, restart SmartAmp.\n");
		schedule_delayed_work(&pTAS2557->irq_work, msecs_to_jiffies(100));
		return;
	}
#endif
	pTAS2557->enableIRQ(pTAS2557, false);
	tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
	pTAS2557->mbPowerUp = false;
	pTAS2557->hw_reset(pTAS2557);
	pTAS2557->write(pTAS2557, TAS2557_SW_RESET_REG, 0x01);
	udelay(1000);
	pTAS2557->write(pTAS2557, TAS2557_SPK_CTRL_REG, 0x04);
	if (pTAS2557->mpFirmware != NULL)
		tas2557_clear_firmware(pTAS2557->mpFirmware);
}
int tas2557_checkPLL(struct tas2557_priv *pTAS2557)
{
	int nResult = 0;
	return nResult;
}
static int tas2557_load_coefficient(struct tas2557_priv *pTAS2557,
	int nPrevConfig, int nNewConfig, bool bPowerOn)
{
	int nResult = 0;
	struct TPLL *pPLL;
	struct TProgram *pProgram;
	struct TConfiguration *pPrevConfiguration;
	struct TConfiguration *pNewConfiguration;
	bool bRestorePower = false;
	if (!pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}
	if (nNewConfig >= pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "%s, invalid configuration New=%d, total=%d\n",
			__func__, nNewConfig, pTAS2557->mpFirmware->mnConfigurations);
		goto end;
	}
	if (nPrevConfig < 0)
		pPrevConfiguration = NULL;
	else if (nPrevConfig == nNewConfig) {
		dev_dbg(pTAS2557->dev, "%s, config [%d] already loaded\n",
			__func__, nNewConfig);
		goto end;
	} else
		pPrevConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[nPrevConfig]);
	pNewConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[nNewConfig]);
	pTAS2557->mnCurrentConfiguration = nNewConfig;
	if (pPrevConfiguration) {
		if (pPrevConfiguration->mnPLL == pNewConfiguration->mnPLL) {
			dev_dbg(pTAS2557->dev, "%s, PLL same\n", __func__);
			goto prog_coefficient;
		}
	}
	pProgram = &(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
	if (bPowerOn) {
		dev_dbg(pTAS2557->dev, "%s, power down to load new PLL\n", __func__);
		if (hrtimer_active(&pTAS2557->mtimer))
			hrtimer_cancel(&pTAS2557->mtimer);
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE)
			pTAS2557->enableIRQ(pTAS2557, false);
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
		if (nResult < 0)
			goto end;
		bRestorePower = true;
	}
	pPLL = &(pTAS2557->mpFirmware->mpPLLs[pNewConfiguration->mnPLL]);
	dev_dbg(pTAS2557->dev, "load PLL: %s block for Configuration %s\n",
		pPLL->mpName, pNewConfiguration->mpName);
	nResult = tas2557_load_block(pTAS2557, &(pPLL->mBlock));
	if (nResult < 0)
		goto end;
	pTAS2557->mnCurrentSampleRate = pNewConfiguration->mnSamplingRate;
	dev_dbg(pTAS2557->dev, "load configuration %s conefficient pre block\n",
		pNewConfiguration->mpName);
	nResult = tas2557_load_data(pTAS2557, &(pNewConfiguration->mData), TAS2557_BLOCK_CFG_PRE_DEV_A);
	if (nResult < 0)
		goto end;
prog_coefficient:
	dev_dbg(pTAS2557->dev, "load new configuration: %s, coeff block data\n",
		pNewConfiguration->mpName);
	nResult = tas2557_load_data(pTAS2557, &(pNewConfiguration->mData),
		TAS2557_BLOCK_CFG_COEFF_DEV_A);
	if (nResult < 0)
		goto end;
	if (pTAS2557->mpCalFirmware->mnCalibrations) {
		nResult = tas2557_set_calibration(pTAS2557, pTAS2557->mnCurrentCalibration);
		if (nResult < 0)
			goto end;
	}
	if (bRestorePower) {
		pTAS2557->clearIRQ(pTAS2557);
		dev_dbg(pTAS2557->dev, "device powered up, load startup\n");
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_startup_data);
		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
			nResult = tas2557_checkPLL(pTAS2557);
			if (nResult < 0) {
				nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
				pTAS2557->mbPowerUp = false;
				goto end;
			}
		}
		dev_dbg(pTAS2557->dev,
			"device powered up, load unmute\n");
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_unmute_data);
		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
			pTAS2557->enableIRQ(pTAS2557, true);
			if (!hrtimer_active(&pTAS2557->mtimer)) {
				pTAS2557->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2557->mtimer,
					ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}
end:
	return nResult;
}
int tas2557_enable(struct tas2557_priv *pTAS2557, bool bEnable)
{
	int nResult = 0;
	unsigned int nValue;
	struct TProgram *pProgram;
	dev_dbg(pTAS2557->dev, "Enable: %d\n", bEnable);
	if ((pTAS2557->mpFirmware->mnPrograms == 0)
		|| (pTAS2557->mpFirmware->mnConfigurations == 0)) {
		dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}
	/* check safe guard*/
	nResult = pTAS2557->read(pTAS2557, TAS2557_SAFE_GUARD_REG, &nValue);
	if (nResult < 0)
		goto end;
	if ((nValue&0xff) != TAS2557_SAFE_GUARD_PATTERN) {
		dev_err(pTAS2557->dev, "ERROR safe guard failure!\n");
		nResult = -EPIPE;
		goto end;
	}
	pProgram = &(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
	if (bEnable) {
		if (!pTAS2557->mbPowerUp) {
			if (pTAS2557->mbLoadConfigurationPrePowerUp) {
				dev_dbg(pTAS2557->dev, "load coefficient before power\n");
				pTAS2557->mbLoadConfigurationPrePowerUp = false;
				nResult = tas2557_load_coefficient(pTAS2557,
					pTAS2557->mnCurrentConfiguration, pTAS2557->mnNewConfiguration, false);
				if (nResult < 0)
					goto end;
			}
			pTAS2557->clearIRQ(pTAS2557);
			/* power on device */
			dev_dbg(pTAS2557->dev, "Enable: load startup sequence\n");
			nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_startup_data);
			if (nResult < 0)
				goto end;
			if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
				nResult = tas2557_checkPLL(pTAS2557);
				if (nResult < 0) {
					nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
					goto end;
				}
			}
			dev_dbg(pTAS2557->dev, "Enable: load unmute sequence\n");
			nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_unmute_data);
			if (nResult < 0)
				goto end;
			if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
				/* turn on IRQ */
				pTAS2557->enableIRQ(pTAS2557, true);
				if (!hrtimer_active(&pTAS2557->mtimer)) {
					pTAS2557->mnDieTvReadCounter = 0;
					hrtimer_start(&pTAS2557->mtimer,
						ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
				}
			}
			pTAS2557->mbPowerUp = true;
		}
	} else {
		if (pTAS2557->mbPowerUp) {
			if (hrtimer_active(&pTAS2557->mtimer))
				hrtimer_cancel(&pTAS2557->mtimer);
			dev_dbg(pTAS2557->dev, "Enable: load shutdown sequence\n");
			if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
				/* turn off IRQ */
				pTAS2557->enableIRQ(pTAS2557, false);
			}
			nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
			if (nResult < 0)
				goto end;
			pTAS2557->mbPowerUp = false;
		}
	}
	nResult = 0;
end:
	if (nResult < 0) {
		if (pTAS2557->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2557);
	}
	return nResult;
}
int tas2557_set_sampling_rate(struct tas2557_priv *pTAS2557, unsigned int nSamplingRate)
{
	int nResult = 0;
	struct TConfiguration *pConfiguration;
	unsigned int nConfiguration;
	dev_dbg(pTAS2557->dev, "tas2557_setup_clocks: nSamplingRate = %d [Hz]\n",
		nSamplingRate);
	if ((!pTAS2557->mpFirmware->mpPrograms) ||
		(!pTAS2557->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2557->dev, "Firmware not loaded\n");
		nResult = -EINVAL;
		goto end;
	}
	pConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration]);
	if (pConfiguration->mnSamplingRate == nSamplingRate) {
		dev_info(pTAS2557->dev, "Sampling rate for current configuration matches: %d\n",
			nSamplingRate);
		nResult = 0;
		goto end;
	}
	for (nConfiguration = 0;
		nConfiguration < pTAS2557->mpFirmware->mnConfigurations;
		nConfiguration++) {
		pConfiguration =
			&(pTAS2557->mpFirmware->mpConfigurations[nConfiguration]);
		if ((pConfiguration->mnSamplingRate == nSamplingRate)
			&& (pConfiguration->mnProgram == pTAS2557->mnCurrentProgram)) {
			dev_info(pTAS2557->dev,
				"Found configuration: %s, with compatible sampling rate %d\n",
				pConfiguration->mpName, nSamplingRate);
			nResult = tas2557_load_configuration(pTAS2557, nConfiguration, false);
			goto end;
		}
	}
	dev_err(pTAS2557->dev, "Cannot find a configuration that supports sampling rate: %d\n",
		nSamplingRate);
end:
	return nResult;
}
static void fw_print_header(struct tas2557_priv *pTAS2557, struct TFirmware *pFirmware)
{
	dev_info(pTAS2557->dev, "FW Size       = %d", pFirmware->mnFWSize);
	dev_info(pTAS2557->dev, "Checksum      = 0x%04X", pFirmware->mnChecksum);
	dev_info(pTAS2557->dev, "PPC Version   = 0x%04X", pFirmware->mnPPCVersion);
	dev_info(pTAS2557->dev, "FW  Version    = 0x%04X", pFirmware->mnFWVersion);
	dev_info(pTAS2557->dev, "Driver Version= 0x%04X", pFirmware->mnDriverVersion);
	dev_info(pTAS2557->dev, "Timestamp     = %d", pFirmware->mnTimeStamp);
	dev_info(pTAS2557->dev, "DDC Name      = %s", pFirmware->mpDDCName);
	dev_info(pTAS2557->dev, "Description   = %s", pFirmware->mpDescription);
}
inline unsigned int fw_convert_number(unsigned char *pData)
{
	return pData[3] + (pData[2] << 8) + (pData[1] << 16) + (pData[0] << 24);
}
static int fw_parse_header(struct tas2557_priv *pTAS2557,
	struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned char pMagicNumber[] = { 0x35, 0x35, 0x35, 0x32 };
	if (nSize < 104) {
		dev_err(pTAS2557->dev, "Firmware: Header too short");
		return -EINVAL;
	}
	if (memcmp(pData, pMagicNumber, 4)) {
		dev_err(pTAS2557->dev, "Firmware: Magic number doesn't match");
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
		dev_err(pTAS2557->dev, "Firmware: Header too short after DDC description");
		return -EINVAL;
	}
	pFirmware->mnDeviceFamily = fw_convert_number(pData);
	pData += 4;
	if (pFirmware->mnDeviceFamily != 0) {
		dev_err(pTAS2557->dev,
			"deviceFamily %d, not TAS device", pFirmware->mnDeviceFamily);
		return -EINVAL;
	}
	pFirmware->mnDevice = fw_convert_number(pData);
	pData += 4;
	if (pFirmware->mnDevice != 2) {
		dev_err(pTAS2557->dev,
			"device %d, not TAS2557 Dual Mono", pFirmware->mnDevice);
		return -EINVAL;
	}
	fw_print_header(pTAS2557, pFirmware);
	return pData - pDataStart;
}
static int fw_parse_block_data(struct tas2557_priv *pTAS2557, struct TFirmware *pFirmware,
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
static int fw_parse_data(struct tas2557_priv *pTAS2557, struct TFirmware *pFirmware,
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
		n = fw_parse_block_data(pTAS2557, pFirmware,
			&(pImageData->mpBlocks[nBlock]), pData);
		pData += n;
	}
	return pData - pDataStart;
}
static int fw_parse_pll_data(struct tas2557_priv *pTAS2557,
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
		n = fw_parse_block_data(pTAS2557, pFirmware, &(pPLL->mBlock), pData);
		pData += n;
	}
end:
	return pData - pDataStart;
}
static int fw_parse_program_data(struct tas2557_priv *pTAS2557,
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
		n = fw_parse_data(pTAS2557, pFirmware, &(pProgram->mData), pData);
		pData += n;
	}
end:
	return pData - pDataStart;
}
static int fw_parse_configuration_data(struct tas2557_priv *pTAS2557,
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
			pConfiguration->mnDevices = 1;
		pConfiguration->mnProgram = pData[0];
		pData++;
		pConfiguration->mnPLL = pData[0];
		pData++;
		pConfiguration->mnSamplingRate = fw_convert_number(pData);
		pData += 4;
		n = fw_parse_data(pTAS2557, pFirmware, &(pConfiguration->mData), pData);
		pData += n;
	}
end:
	return pData - pDataStart;
}
int fw_parse_calibration_data(struct tas2557_priv *pTAS2557,
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
		n = fw_parse_data(pTAS2557, pFirmware, &(pCalibration->mData), pData);
		pData += n;
	}
end:
	return pData - pDataStart;
}
static int fw_parse(struct tas2557_priv *pTAS2557,
	struct TFirmware *pFirmware, unsigned char *pData, unsigned int nSize)
{
	int nPosition = 0;
	nPosition = fw_parse_header(pTAS2557, pFirmware, pData, nSize);
	if (nPosition < 0) {
		dev_err(pTAS2557->dev, "Firmware: Wrong Header");
		return -EINVAL;
	}
	if (nPosition >= nSize) {
		dev_err(pTAS2557->dev, "Firmware: Too short");
		return -EINVAL;
	}
	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;
	nPosition = fw_parse_pll_data(pTAS2557, pFirmware, pData);
	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;
	nPosition = fw_parse_program_data(pTAS2557, pFirmware, pData);
	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;
	nPosition = fw_parse_configuration_data(pTAS2557, pFirmware, pData);
	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;
	if (nSize > 64)
		nPosition = fw_parse_calibration_data(pTAS2557, pFirmware, pData);
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
static int isInPageYRAM(struct tas2557_priv *pTAS2557, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult = 0;
	if (nBook == TAS2557_YRAM_BOOK1) {
		if (nPage == TAS2557_YRAM1_PAGE) {
			if (nReg >= TAS2557_YRAM1_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else if ((nReg + len) > TAS2557_YRAM1_START_REG) {
				pCRCData->mnOffset = TAS2557_YRAM1_START_REG;
				pCRCData->mnLen = len - (TAS2557_YRAM1_START_REG - nReg);
				nResult = 1;
			} else
				nResult = 0;
		} else if (nPage == TAS2557_YRAM3_PAGE) {
			if (nReg > TAS2557_YRAM3_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2557_YRAM3_START_REG) {
				if ((nReg + len) > TAS2557_YRAM3_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2557_YRAM3_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2557_YRAM3_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2557_YRAM3_START_REG;
					pCRCData->mnLen = len - (TAS2557_YRAM3_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else if (nBook == TAS2557_YRAM_BOOK2) {
		if (nPage == TAS2557_YRAM5_PAGE) {
			if (nReg > TAS2557_YRAM5_END_REG) {
				nResult = 0;
			} else if (nReg >= TAS2557_YRAM5_START_REG) {
				if ((nReg + len) > TAS2557_YRAM5_END_REG) {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = TAS2557_YRAM5_END_REG - nReg + 1;
					nResult = 1;
				} else {
					pCRCData->mnOffset = nReg;
					pCRCData->mnLen = len;
					nResult = 1;
				}
			} else {
				if ((nReg + (len - 1)) < TAS2557_YRAM5_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2557_YRAM5_START_REG;
					pCRCData->mnLen = len - (TAS2557_YRAM5_START_REG - nReg);
					nResult = 1;
				}
			}
		}
	} else
		nResult = 0;
	return nResult;
}
static int isInBlockYRAM(struct tas2557_priv *pTAS2557, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult;
	if (nBook == TAS2557_YRAM_BOOK1) {
		if (nPage < TAS2557_YRAM2_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2557_YRAM2_END_PAGE) {
			if (nReg > TAS2557_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2557_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2557_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2557_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2557_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else if (nBook == TAS2557_YRAM_BOOK2) {
		if (nPage < TAS2557_YRAM4_START_PAGE)
			nResult = 0;
		else if (nPage <= TAS2557_YRAM4_END_PAGE) {
			if (nReg > TAS2557_YRAM2_END_REG)
				nResult = 0;
			else if (nReg >= TAS2557_YRAM2_START_REG) {
				pCRCData->mnOffset = nReg;
				pCRCData->mnLen = len;
				nResult = 1;
			} else {
				if ((nReg + (len - 1)) < TAS2557_YRAM2_START_REG)
					nResult = 0;
				else {
					pCRCData->mnOffset = TAS2557_YRAM2_START_REG;
					pCRCData->mnLen = nReg + len - TAS2557_YRAM2_START_REG;
					nResult = 1;
				}
			}
		} else
			nResult = 0;
	} else
		nResult = 0;
	return nResult;
}
static int isYRAM(struct tas2557_priv *pTAS2557, struct TYCRC *pCRCData,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char len)
{
	int nResult;
	nResult = isInPageYRAM(pTAS2557, pCRCData, nBook, nPage, nReg, len);
	if (nResult == 0)
		nResult = isInBlockYRAM(pTAS2557, pCRCData, nBook, nPage, nReg, len);
	return nResult;
}
static u8 ti_crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc)
{
	while (nbytes-- > 0)
		crc = table[(crc ^ *pdata++) & 0xff];
	return crc;
}
static int doSingleRegCheckSum(struct tas2557_priv *pTAS2557,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned char nValue)
{
	int nResult = 0;
	struct TYCRC sCRCData;
	unsigned int nData1 = 0;
	if ((nBook == TAS2557_BOOK_ID(TAS2557_SA_COEFF_SWAP_REG))
		&& (nPage == TAS2557_PAGE_ID(TAS2557_SA_COEFF_SWAP_REG))
		&& (nReg >= TAS2557_PAGE_REG(TAS2557_SA_COEFF_SWAP_REG))
		&& (nReg <= (TAS2557_PAGE_REG(TAS2557_SA_COEFF_SWAP_REG) + 4))) {
		nResult = 0;
		goto end;
	}
	nResult = isYRAM(pTAS2557, &sCRCData, nBook, nPage, nReg, 1);
	if (nResult == 1) {
		nResult = pTAS2557->read(pTAS2557, TAS2557_REG(nBook, nPage, nReg), &nData1);
		if (nResult < 0)
			goto end;
		if (nData1 != nValue) {
			dev_err(pTAS2557->dev, "error2 (line %d),B[0x%x]P[0x%x]R[0x%x] W[0x%x], R[0x%x]\n",
				__LINE__, nBook, nPage, nReg, nValue, nData1);
			nResult = -EAGAIN;
			goto end;
		}
		nResult = ti_crc8(crc8_lookup_table, &nValue, 1, 0);
	}
end:
	return nResult;
}
static int doMultiRegCheckSum(struct tas2557_priv *pTAS2557,
	unsigned char nBook, unsigned char nPage, unsigned char nReg, unsigned int len)
{
	int nResult = 0, i;
	unsigned char nCRCChkSum = 0;
	unsigned char nBuf1[128];
	struct TYCRC TCRCData;
	if ((nReg + len-1) > 127) {
		nResult = -EINVAL;
		dev_err(pTAS2557->dev, "firmware error\n");
		goto end;
	}
	if ((nBook == TAS2557_BOOK_ID(TAS2557_SA_COEFF_SWAP_REG))
		&& (nPage == TAS2557_PAGE_ID(TAS2557_SA_COEFF_SWAP_REG))
		&& (nReg == TAS2557_PAGE_REG(TAS2557_SA_COEFF_SWAP_REG))
		&& (len == 4)) {
		nResult = 0;
		goto end;
	}
	nResult = isYRAM(pTAS2557, &TCRCData, nBook, nPage, nReg, len);
	if (nResult == 1) {
		if (len == 1) {
			dev_err(pTAS2557->dev, "firmware error\n");
			nResult = -EINVAL;
			goto end;
		} else {
			nResult = pTAS2557->bulk_read(pTAS2557, TAS2557_REG(nBook, nPage, TCRCData.mnOffset), nBuf1, TCRCData.mnLen);
			if (nResult < 0)
				goto end;
			for (i = 0; i < TCRCData.mnLen; i++) {
				if ((nBook == TAS2557_BOOK_ID(TAS2557_SA_COEFF_SWAP_REG))
					&& (nPage == TAS2557_PAGE_ID(TAS2557_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
						>= TAS2557_PAGE_REG(TAS2557_SA_COEFF_SWAP_REG))
					&& ((i + TCRCData.mnOffset)
						<= (TAS2557_PAGE_REG(TAS2557_SA_COEFF_SWAP_REG) + 4))) {
					/* DSP swap command, bypass */
					continue;
				} else
					nCRCChkSum += ti_crc8(crc8_lookup_table, &nBuf1[i], 1, 0);
			}
			nResult = nCRCChkSum;
		}
	}
end:
	return nResult;
}
static int tas2557_load_block(struct tas2557_priv *pTAS2557, struct TBlock *pBlock)
{
	int nResult = 0;
	unsigned int nCommand = 0;
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nOffset;
	unsigned char nData;
	unsigned int nLength;
	unsigned int nSleep;
	unsigned char nCRCChkSum = 0;
	unsigned int nValue1;
	int nRetry = 6;
	unsigned char *pData = pBlock->mpData;
	dev_dbg(pTAS2557->dev, "TAS2557 load block: Type = %d, commands = %d\n",
		pBlock->mnType, pBlock->mnCommands);
start:
	if (pBlock->mbPChkSumPresent) {
		nResult = pTAS2557->write(pTAS2557, TAS2557_CRC_RESET_REG, 1);
		if (nResult < 0)
			goto end;
	}
	if (pBlock->mbYChkSumPresent)
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
			nResult = pTAS2557->write(pTAS2557, TAS2557_REG(nBook, nPage, nOffset), nData);
			if (nResult < 0)
				goto end;
			if (pBlock->mbYChkSumPresent) {
				nResult = doSingleRegCheckSum(pTAS2557, nBook, nPage, nOffset, nData);
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
				nResult = pTAS2557->bulk_write(pTAS2557, TAS2557_REG(nBook, nPage, nOffset), pData + 3, nLength);
				if (nResult < 0)
					goto end;
				if (pBlock->mbYChkSumPresent) {
					nResult = doMultiRegCheckSum(pTAS2557, nBook, nPage, nOffset, nLength);
					if (nResult < 0)
						goto check;
					nCRCChkSum += (unsigned char)nResult;
				}
			} else {
				nResult = pTAS2557->write(pTAS2557, TAS2557_REG(nBook, nPage, nOffset), pData[3]);
				if (nResult < 0)
					goto end;
				if (pBlock->mbYChkSumPresent) {
					nResult = doSingleRegCheckSum(pTAS2557, nBook, nPage, nOffset, pData[3]);
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
		nResult = pTAS2557->read(pTAS2557, TAS2557_CRC_CHECKSUM_REG, &nValue1);
		if (nResult < 0)
			goto end;
		if ((nValue1&0xff) != pBlock->mnPChkSum) {
			dev_err(pTAS2557->dev, "Block PChkSum Error: FW = 0x%x, Reg = 0x%x\n",
				pBlock->mnPChkSum, (nValue1&0xff));
			nResult = -EAGAIN;
				pTAS2557->mnErrCode |= ERROR_PRAM_CRCCHK;
			goto check;
		}
		nResult = 0;
		pTAS2557->mnErrCode &= ~ERROR_PRAM_CRCCHK;
		dev_dbg(pTAS2557->dev, "Block[0x%x] PChkSum match\n", pBlock->mnType);
	}
	if (pBlock->mbYChkSumPresent) {
		if (nCRCChkSum != pBlock->mnYChkSum) {
			dev_err(pTAS2557->dev, "Block YChkSum Error: FW = 0x%x, YCRC = 0x%x\n",
				pBlock->mnYChkSum, nCRCChkSum);
			nResult = -EAGAIN;
			pTAS2557->mnErrCode |= ERROR_YRAM_CRCCHK;
			goto check;
		}
		pTAS2557->mnErrCode &= ~ERROR_YRAM_CRCCHK;
		nResult = 0;
		dev_dbg(pTAS2557->dev, "Block[0x%x] YChkSum match\n", pBlock->mnType);
	}
check:
	if (nResult == -EAGAIN) {
		nRetry--;
		if (nRetry > 0)
			goto start;
	}
end:
	if (nResult < 0) {
		dev_err(pTAS2557->dev, "Block (%d) load error\n",
				pBlock->mnType);
	}
	return nResult;
}
static int tas2557_load_data(struct tas2557_priv *pTAS2557, struct TData *pData, unsigned int nType)
{
	int nResult = 0;
	unsigned int nBlock;
	struct TBlock *pBlock;
	dev_dbg(pTAS2557->dev,
		"TAS2557 load data: %s, Blocks = %d, Block Type = %d\n", pData->mpName, pData->mnBlocks, nType);
	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);
		if (pBlock->mnType == nType) {
			nResult = tas2557_load_block(pTAS2557, pBlock);
			if (nResult < 0)
				break;
		}
	}
	return nResult;
}
static int tas2557_load_configuration(struct tas2557_priv *pTAS2557,
	unsigned int nConfiguration, bool bLoadSame)
{
	int nResult = 0;
	struct TConfiguration *pCurrentConfiguration = NULL;
	struct TConfiguration *pNewConfiguration = NULL;
	dev_dbg(pTAS2557->dev, "%s: %d\n", __func__, nConfiguration);
	if ((!pTAS2557->mpFirmware->mpPrograms) ||
		(!pTAS2557->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2557->dev, "Firmware not loaded\n");
		nResult = 0;
		goto end;
	}
	if (nConfiguration >= pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = 0;
		goto end;
	}
	if ((!pTAS2557->mbLoadConfigurationPrePowerUp)
		&& (nConfiguration == pTAS2557->mnCurrentConfiguration)
		&& (!bLoadSame)) {
		dev_info(pTAS2557->dev, "Configuration %d is already loaded\n",
			nConfiguration);
		nResult = 0;
		goto end;
	}
	pCurrentConfiguration =
		&(pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration]);
	pNewConfiguration =
		&(pTAS2557->mpFirmware->mpConfigurations[nConfiguration]);
	if (pNewConfiguration->mnProgram != pCurrentConfiguration->mnProgram) {
		dev_err(pTAS2557->dev, "Configuration %d, %s doesn't share the same program as current %d\n",
			nConfiguration, pNewConfiguration->mpName, pCurrentConfiguration->mnProgram);
		nResult = 0;
		goto end;
	}
	if (pNewConfiguration->mnPLL >= pTAS2557->mpFirmware->mnPLLs) {
		dev_err(pTAS2557->dev, "Configuration %d, %s doesn't have a valid PLL index %d\n",
			nConfiguration, pNewConfiguration->mpName, pNewConfiguration->mnPLL);
		nResult = 0;
		goto end;
	}
	if (pTAS2557->mbPowerUp) {
		pTAS2557->mbLoadConfigurationPrePowerUp = false;
		nResult = tas2557_load_coefficient(pTAS2557, pTAS2557->mnCurrentConfiguration, nConfiguration, true);
	} else {
		dev_dbg(pTAS2557->dev,
			"TAS2557 was powered down, will load coefficient when power up\n");
		pTAS2557->mbLoadConfigurationPrePowerUp = true;
		pTAS2557->mnNewConfiguration = nConfiguration;
	}
end:
	if (nResult < 0) {
		if (pTAS2557->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2557);
	}
	return nResult;
}
int tas2557_set_config(struct tas2557_priv *pTAS2557, int config)
{
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	unsigned int nProgram = pTAS2557->mnCurrentProgram;
	unsigned int nConfiguration = config;
	int nResult = 0;
	if ((!pTAS2557->mpFirmware->mpPrograms) ||
		(!pTAS2557->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2557->dev, "Firmware not loaded\n");
		nResult = -EINVAL;
		goto end;
	}
	if (nConfiguration >= pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		nResult = -EINVAL;
		goto end;
	}
	pConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[nConfiguration]);
	pProgram = &(pTAS2557->mpFirmware->mpPrograms[nProgram]);
	if (nProgram != pConfiguration->mnProgram) {
		dev_err(pTAS2557->dev,
			"Configuration %d, %s with Program %d isn't compatible with existing Program %d, %s\n",
			nConfiguration, pConfiguration->mpName, pConfiguration->mnProgram,
			nProgram, pProgram->mpName);
		nResult = -EINVAL;
		goto end;
	}
	nResult = tas2557_load_configuration(pTAS2557, nConfiguration, false);
end:
	return nResult;
}
void tas2557_clear_firmware(struct TFirmware *pFirmware)
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
static int tas2557_load_calibration(struct tas2557_priv *pTAS2557,	char *pFileName)
{
	int nResult = 0;
	int nFile;
	mm_segment_t fs;
	unsigned char pBuffer[1000];
	int nSize = 0;
	dev_dbg(pTAS2557->dev, "%s:\n", __func__);
	fs = get_fs();
	set_fs(KERNEL_DS);
	nFile = sys_open(pFileName, O_RDONLY, 0);
	dev_info(pTAS2557->dev, "TAS2557 calibration file = %s, handle = %d\n",
		pFileName, nFile);
	if (nFile >= 0) {
		nSize = sys_read(nFile, pBuffer, 1000);
		sys_close(nFile);
	} else {
		dev_err(pTAS2557->dev, "TAS2557 cannot open calibration file: %s\n",
			pFileName);
	}
	set_fs(fs);
	if (!nSize)
		goto end;
	tas2557_clear_firmware(pTAS2557->mpCalFirmware);
	dev_info(pTAS2557->dev, "TAS2557 calibration file size = %d\n", nSize);
	nResult = fw_parse(pTAS2557, pTAS2557->mpCalFirmware, pBuffer, nSize);
	if (nResult)
		dev_err(pTAS2557->dev, "TAS2557 calibration file is corrupt\n");
	else
		dev_info(pTAS2557->dev, "TAS2557 calibration: %d calibrations\n",
			pTAS2557->mpCalFirmware->mnCalibrations);
end:
	return nResult;
}
static bool tas2557_get_coefficient_in_block(struct tas2557_priv *pTAS2557,
	struct TBlock *pBlock, int nReg, int *pnValue)
{
	int nCoefficient = 0;
	bool bFound = false;
	unsigned char *pCommands;
	int nBook, nPage, nOffset, len;
	int i, n;
	pCommands = pBlock->mpData;
	for (i = 0 ; i < pBlock->mnCommands;) {
		nBook = pCommands[4 * i + 0];
		nPage = pCommands[4 * i + 1];
		nOffset = pCommands[4 * i + 2];
		if ((nOffset < 0x7f) || (nOffset == 0x81))
			i++;
		else if (nOffset == 0x85) {
			len = ((int)nBook << 8) | nPage;
			nBook = pCommands[4 * i + 4];
			nPage = pCommands[4 * i + 5];
			nOffset = pCommands[4 * i + 6];
			n = 4 * i + 7;
			i += 2;
			i += ((len - 1) / 4);
			if ((len - 1) % 4)
				i++;
			if ((nBook != TAS2557_BOOK_ID(nReg))
				|| (nPage != TAS2557_PAGE_ID(nReg)))
				continue;
			if (nOffset > TAS2557_PAGE_REG(nReg))
				continue;
			if ((len + nOffset) >= (TAS2557_PAGE_REG(nReg) + 4)) {
				n += (TAS2557_PAGE_REG(nReg) - nOffset);
				nCoefficient = ((int)pCommands[n] << 24)
						| ((int)pCommands[n + 1] << 16)
						| ((int)pCommands[n + 2] << 8)
						| (int)pCommands[n + 3];
				bFound = true;
				break;
			}
		} else {
			dev_err(pTAS2557->dev, "%s, format error %d\n", __func__, nOffset);
			break;
		}
	}
	if (bFound) {
		*pnValue = nCoefficient;
		dev_dbg(pTAS2557->dev, "%s, B[0x%x]P[0x%x]R[0x%x]=0x%x\n", __func__,
			TAS2557_BOOK_ID(nReg), TAS2557_PAGE_ID(nReg), TAS2557_PAGE_REG(nReg),
			nCoefficient);
	}
	return bFound;
}
static bool tas2557_get_coefficient_in_data(struct tas2557_priv *pTAS2557,
	struct TData *pData, int blockType, int nReg, int *pnValue)
{
	bool bFound = false;
	struct TBlock *pBlock;
	int i;
	for (i = 0; i < pData->mnBlocks; i++) {
		pBlock = &(pData->mpBlocks[i]);
		if (pBlock->mnType == blockType) {
			bFound = tas2557_get_coefficient_in_block(pTAS2557,
						pBlock, nReg, pnValue);
			if (bFound)
				break;
		}
	}
	return bFound;
}
static bool tas2557_find_Tmax_in_configuration(struct tas2557_priv *pTAS2557,
	struct TConfiguration *pConfiguration, int *pnTMax)
{
	struct TData *pData;
	bool bFound = false;
	int nBlockType, nReg, nCoefficient;
	if (pTAS2557->mnPGID == TAS2557_PG_VERSION_2P1)
		nReg = TAS2557_PG2P1_CALI_T_REG;
	else
		nReg = TAS2557_PG1P0_CALI_T_REG;
	nBlockType = TAS2557_BLOCK_CFG_COEFF_DEV_A;
	pData = &(pConfiguration->mData);
	bFound = tas2557_get_coefficient_in_data(pTAS2557, pData, nBlockType, nReg, &nCoefficient);
	if (bFound)
		*pnTMax = nCoefficient;
	return bFound;
}
void tas2557_fw_ready(const struct firmware *pFW, void *pContext)
{
	struct tas2557_priv *pTAS2557 = (struct tas2557_priv *) pContext;
	int nResult;
	unsigned int nProgram = 0;
	unsigned int nSampleRate = 0;
#ifdef CONFIG_TAS2557_CODEC
	mutex_lock(&pTAS2557->codec_lock);
#endif
#ifdef CONFIG_TAS2557_MISC
	mutex_lock(&pTAS2557->file_lock);
#endif
	dev_info(pTAS2557->dev, "%s:\n", __func__);
	if (unlikely(!pFW) || unlikely(!pFW->data)) {
		dev_err(pTAS2557->dev, "%s firmware is not loaded.\n",
			TAS2557_FW_NAME);
		goto end;
	}
	if (pTAS2557->mpFirmware->mpConfigurations) {
		nProgram = pTAS2557->mnCurrentProgram;
		nSampleRate = pTAS2557->mnCurrentSampleRate;
		dev_dbg(pTAS2557->dev, "clear current firmware\n");
		tas2557_clear_firmware(pTAS2557->mpFirmware);
	}
	nResult = fw_parse(pTAS2557, pTAS2557->mpFirmware, (unsigned char *)(pFW->data), pFW->size);
	release_firmware(pFW);
	if (nResult < 0) {
		dev_err(pTAS2557->dev, "firmware is corrupt\n");
		goto end;
	}
	if (!pTAS2557->mpFirmware->mnPrograms) {
		dev_err(pTAS2557->dev, "firmware contains no programs\n");
		nResult = -EINVAL;
		goto end;
	}
	if (!pTAS2557->mpFirmware->mnConfigurations) {
		dev_err(pTAS2557->dev, "firmware contains no configurations\n");
		nResult = -EINVAL;
		goto end;
	}
	if (nProgram >= pTAS2557->mpFirmware->mnPrograms) {
		dev_info(pTAS2557->dev,
			"no previous program, set to default\n");
		nProgram = 0;
	}
	pTAS2557->mnCurrentSampleRate = nSampleRate;
	nResult = tas2557_set_program(pTAS2557, nProgram, -1);
end:
	printk("ready end !");
#ifdef CONFIG_TAS2557_CODEC
	mutex_unlock(&pTAS2557->codec_lock);
#endif
#ifdef CONFIG_TAS2557_MISC
	mutex_unlock(&pTAS2557->file_lock);
#endif
}
int tas2557_set_program(struct tas2557_priv *pTAS2557,
	unsigned int nProgram, int nConfig)
{
	struct TProgram *pProgram;
	unsigned int nConfiguration = 0;
	unsigned int nSampleRate = 0;
	unsigned char nGain;
	bool bFound = false;
	int nResult = 0;
	if ((!pTAS2557->mpFirmware->mpPrograms) ||
		(!pTAS2557->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2557->dev, "Firmware not loaded\n");
		nResult = 0;
		goto end;
	}
	if (nProgram >= pTAS2557->mpFirmware->mnPrograms) {
		dev_err(pTAS2557->dev, "TAS2557: Program %d doesn't exist\n",
			nProgram);
		nResult = 0;
		goto end;
	}
	if (nConfig < 0) {
		nConfiguration = 0;
		nSampleRate = pTAS2557->mnCurrentSampleRate;
		while (!bFound && (nConfiguration < pTAS2557->mpFirmware->mnConfigurations)) {
			if (pTAS2557->mpFirmware->mpConfigurations[nConfiguration].mnProgram == nProgram) {
				if (nSampleRate == 0) {
					bFound = true;
					dev_info(pTAS2557->dev, "find default configuration %d\n", nConfiguration);
				} else if (nSampleRate == pTAS2557->mpFirmware->mpConfigurations[nConfiguration].mnSamplingRate) {
					bFound = true;
					dev_info(pTAS2557->dev, "find matching configuration %d\n", nConfiguration);
				} else {
					nConfiguration++;
				}
			} else {
				nConfiguration++;
			}
		}
		if (!bFound) {
			dev_err(pTAS2557->dev,
				"Program %d, no valid configuration found for sample rate %d, ignore\n",
				nProgram, nSampleRate);
			nResult = 0;
			goto end;
		}
	} else {
		if (pTAS2557->mpFirmware->mpConfigurations[nConfig].mnProgram != nProgram) {
			dev_err(pTAS2557->dev, "%s, configuration program doesn't match\n", __func__);
			nResult = 0;
			goto end;
		}
		nConfiguration = nConfig;
	}
	pProgram = &(pTAS2557->mpFirmware->mpPrograms[nProgram]);
	if (pTAS2557->mbPowerUp) {
		dev_info(pTAS2557->dev,
			"device powered up, power down to load program %d (%s)\n",
			nProgram, pProgram->mpName);
		if (hrtimer_active(&pTAS2557->mtimer))
			hrtimer_cancel(&pTAS2557->mtimer);
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE)
			pTAS2557->enableIRQ(pTAS2557, false);
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
		if (nResult < 0)
			goto end;
	}
	pTAS2557->hw_reset(pTAS2557);
	nResult = pTAS2557->write(pTAS2557, TAS2557_SW_RESET_REG, 0x01);
	if (nResult < 0)
		goto end;
	msleep(1);
	nResult = tas2557_load_default(pTAS2557);
	if (nResult < 0)
		goto end;
	dev_info(pTAS2557->dev, "load program %d (%s)\n", nProgram, pProgram->mpName);
	nResult = tas2557_load_data(pTAS2557, &(pProgram->mData), TAS2557_BLOCK_PGM_DEV_A);
	if (nResult < 0)
		goto end;
	pTAS2557->mnCurrentProgram = nProgram;
	nResult = tas2557_get_DAC_gain(pTAS2557, &nGain);
	if (nResult < 0)
		goto end;
	pTAS2557->mnDevGain = nGain;
	pTAS2557->mnDevCurrentGain = nGain;
	nResult = tas2557_load_coefficient(pTAS2557, -1, nConfiguration, false);
	if (nResult < 0)
		goto end;
	if (pTAS2557->mbPowerUp) {
		pTAS2557->clearIRQ(pTAS2557);
		dev_dbg(pTAS2557->dev, "device powered up, load startup\n");
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_startup_data);
		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
			nResult = tas2557_checkPLL(pTAS2557);
			if (nResult < 0) {
				nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_shutdown_data);
				pTAS2557->mbPowerUp = false;
				goto end;
			}
		}
		dev_dbg(pTAS2557->dev, "device powered up, load unmute\n");
		nResult = tas2557_dev_load_data(pTAS2557, p_tas2557_unmute_data);
		if (nResult < 0)
			goto end;
		if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
			pTAS2557->enableIRQ(pTAS2557, true);
			if (!hrtimer_active(&pTAS2557->mtimer)) {
				pTAS2557->mnDieTvReadCounter = 0;
				hrtimer_start(&pTAS2557->mtimer,
					ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
		}
	}
end:
	if (nResult < 0) {
		if (pTAS2557->mnErrCode & (ERROR_DEVA_I2C_COMM | ERROR_PRAM_CRCCHK | ERROR_YRAM_CRCCHK))
			failsafe(pTAS2557);
	}
	return nResult;
}
int tas2557_set_calibration(struct tas2557_priv *pTAS2557, int nCalibration)
{
	struct TCalibration *pCalibration = NULL;
	struct TConfiguration *pConfiguration;
	struct TProgram *pProgram;
	int nTmax = 0;
	bool bFound = false;
	int nResult = 0;
	if ((!pTAS2557->mpFirmware->mpPrograms)
		|| (!pTAS2557->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2557->dev, "Firmware not loaded\n\r");
		nResult = 0;
		goto end;
	}
	if (nCalibration == 0x00FF) {
		nResult = tas2557_load_calibration(pTAS2557, TAS2557_CAL_NAME);
		if (nResult < 0) {
			dev_info(pTAS2557->dev, "load new calibration file %s fail %d\n",
				TAS2557_CAL_NAME, nResult);
			goto end;
		}
		nCalibration = 0;
	}
	if (nCalibration >= pTAS2557->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2557->dev,
			"Calibration %d doesn't exist\n", nCalibration);
		nResult = 0;
		goto end;
	}
	pTAS2557->mnCurrentCalibration = nCalibration;
	if (pTAS2557->mbLoadConfigurationPrePowerUp)
		goto end;
	pCalibration = &(pTAS2557->mpCalFirmware->mpCalibrations[nCalibration]);
	pProgram = &(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
	pConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration]);
	if (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE) {
		if (pTAS2557->mbBypassTMax) {
			bFound = tas2557_find_Tmax_in_configuration(pTAS2557, pConfiguration, &nTmax);
			if (bFound && (nTmax == TAS2557_COEFFICIENT_TMAX)) {
				dev_dbg(pTAS2557->dev, "%s, config[%s] bypass load calibration\n",
					__func__, pConfiguration->mpName);
				goto end;
			}
		}
		dev_dbg(pTAS2557->dev, "%s, load calibration\n", __func__);
		nResult = tas2557_load_data(pTAS2557, &(pCalibration->mData), TAS2557_BLOCK_CFG_COEFF_DEV_A);
		if (nResult < 0)
			goto end;
	}
end:
	if (nResult < 0) {
		tas2557_clear_firmware(pTAS2557->mpCalFirmware);
		nResult = tas2557_set_program(pTAS2557, pTAS2557->mnCurrentProgram, pTAS2557->mnCurrentConfiguration);
	}
	return nResult;
}
bool tas2557_get_Cali_prm_r0(struct tas2557_priv *pTAS2557, int *prm_r0)
{
	struct TCalibration *pCalibration;
	struct TData *pData;
	int nReg;
	int nCali_Re;
	bool bFound = false;
	int nBlockType;
	if (!pTAS2557->mpCalFirmware->mnCalibrations) {
		dev_err(pTAS2557->dev, "%s, no calibration data\n", __func__);
		goto end;
	}
	if (pTAS2557->mnPGID == TAS2557_PG_VERSION_2P1)
		nReg = TAS2557_PG2P1_CALI_R0_REG;
	else
		nReg = TAS2557_PG1P0_CALI_R0_REG;
	nBlockType = TAS2557_BLOCK_CFG_COEFF_DEV_A;
	pCalibration = &(pTAS2557->mpCalFirmware->mpCalibrations[pTAS2557->mnCurrentCalibration]);
	pData = &(pCalibration->mData);
	bFound = tas2557_get_coefficient_in_data(pTAS2557, pData, nBlockType, nReg, &nCali_Re);
end:
	if (bFound)
		*prm_r0 = nCali_Re;
	return bFound;
}
int tas2557_parse_dt(struct device *dev, struct tas2557_priv *pTAS2557)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;
	unsigned int value;
	pTAS2557->mnResetGPIO = of_get_named_gpio(np, "ti,cdc-reset-gpio", 0);
	if (!gpio_is_valid(pTAS2557->mnResetGPIO)) {
		dev_err(pTAS2557->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-reset-gpio", np->full_name,
			pTAS2557->mnResetGPIO);
		ret = -EINVAL;
		goto end;
	} else
		dev_dbg(pTAS2557->dev, "ti,cdc-reset-gpio=%d\n", pTAS2557->mnResetGPIO);
	pTAS2557->mnGpioINT = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2557->mnGpioINT))
		dev_err(pTAS2557->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name,
			pTAS2557->mnGpioINT);
	rc = of_property_read_u32(np, "ti,i2s-bits", &value);
	if (rc)
		dev_err(pTAS2557->dev, "Looking up %s property in node %s failed %d\n",
			"ti,i2s-bits", np->full_name, rc);
	else
		pTAS2557->mnI2SBits = value;
	rc = of_property_read_u32(np, "ti,bypass-tmax", &value);
	if (rc)
		dev_err(pTAS2557->dev, "Looking up %s property in node %s failed %d\n",
			"ti,bypass-tmax", np->full_name, rc);
	else
		pTAS2557->mbBypassTMax = (value > 0);
end:
	return ret;
}
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2557 common functions for Android Linux");
MODULE_LICENSE("GPL v2");
