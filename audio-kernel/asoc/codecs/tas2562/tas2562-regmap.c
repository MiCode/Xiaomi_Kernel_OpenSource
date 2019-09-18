/*
 * ALSA SoC Texas Instruments TAS2562 High Performance 4W Smart Amplifier
 *
 * Copyright (C) 2016 Texas Instruments, Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Author: saiprasad
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
//#ifdef CONFIG_TAS2562_REGMAP

#define DEBUG 5
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
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
#include <sound/soc.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include "tas2562.h"
#include "tas2562-codec.h"
//#include "tas2562-misc.h" //for mtk

//FOR TWO SMARTPA
//#define CHANNEL_RIGHT

static char pICN[] = {0x00, 0x03, 0x46, 0xdc};

static int tas2562_change_book_page(struct tas2562_priv *pTAS2562, enum channel chn,
	int book, int page)
{
	int nResult = 0;


	if((chn&channel_left) || (pTAS2562->mnChannels == 1))
	{
		pTAS2562->client->addr = pTAS2562->mnLAddr;
		if (pTAS2562->mnLCurrentBook != book) {
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, 0);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnLCurrentPage = 0;
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_REG, book);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnLCurrentBook = book;
		}

		if (pTAS2562->mnLCurrentPage != page) {
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, page);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnLCurrentPage = page;
		}
	}

#ifdef CHANNEL_RIGHT
	if((chn&channel_right) && (pTAS2562->mnChannels == 2))
	{
		pTAS2562->client->addr = pTAS2562->mnRAddr;
		if (pTAS2562->mnLCurrentBook != book) {
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, 0);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnRCurrentPage = 0;
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_REG, book);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnRCurrentBook = book;
		}

		if (pTAS2562->mnRCurrentPage != page) {
			nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, page);
			if (nResult < 0) {
				dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2562->mnRCurrentPage = page;
		}
	}
#endif
end:
	return nResult;
}

static int tas2562_dev_read(struct tas2562_priv *pTAS2562, enum channel chn,
	unsigned int reg, unsigned int *pValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562, chn,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	if((chn == channel_left) || (pTAS2562->mnChannels == 1))
		pTAS2562->client->addr = pTAS2562->mnLAddr;
#ifdef CHANNEL_RIGHT
	else if(chn == channel_right)
		pTAS2562->client->addr = pTAS2562->mnRAddr;
#endif
	else
	{
		dev_err(pTAS2562->dev, "%s, wrong channel number\n", __func__);
	}

	nResult = regmap_read(pTAS2562->regmap, TAS2562_PAGE_REG(reg), pValue);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2562->dev, "%s: chn:%x:BOOK:PAGE:REG %u:%u:%u,0x%x\n", __func__,
			pTAS2562->client->addr, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), *pValue);

end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_write(struct tas2562_priv *pTAS2562, enum channel chn,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562, chn,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	if((chn&channel_left) || (pTAS2562->mnChannels == 1))
	{
		pTAS2562->client->addr = pTAS2562->mnLAddr;

		nResult = regmap_write(pTAS2562->regmap, TAS2562_PAGE_REG(reg),
			value);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
				__func__, pTAS2562->client->addr,
				TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
				TAS2562_PAGE_REG(reg), value);
	}
#ifdef CHANNEL_RIGHT
	if((chn&channel_right) && (pTAS2562->mnChannels == 2))
	{
		pTAS2562->client->addr = pTAS2562->mnRAddr;

		nResult = regmap_write(pTAS2562->regmap, TAS2562_PAGE_REG(reg),
			value);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
				__func__, pTAS2562->client->addr, TAS2562_BOOK_ID(reg),
				TAS2562_PAGE_ID(reg), TAS2562_PAGE_REG(reg), value);
	}
#endif
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_bulk_write(struct tas2562_priv *pTAS2562, enum channel chn,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562, chn,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	if((chn&channel_left) || (pTAS2562->mnChannels == 1))
	{
		pTAS2562->client->addr = pTAS2562->mnLAddr;
		nResult = regmap_bulk_write(pTAS2562->regmap,
			TAS2562_PAGE_REG(reg), pData, nLength);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
				__func__, pTAS2562->client->addr,
				TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
				TAS2562_PAGE_REG(reg), nLength);
	}
#ifdef CHANNEL_RIGHT
	if((chn&channel_right) && (pTAS2562->mnChannels == 2))
	{
		pTAS2562->client->addr = pTAS2562->mnRAddr;
				nResult = regmap_bulk_write(pTAS2562->regmap,
			TAS2562_PAGE_REG(reg), pData, nLength);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: %x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
				__func__, pTAS2562->client->addr,
				TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
				TAS2562_PAGE_REG(reg), nLength);
	}
#endif
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_bulk_read(struct tas2562_priv *pTAS2562, enum channel chn,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	if((chn == channel_left) || (pTAS2562->mnChannels == 1))
		pTAS2562->client->addr = pTAS2562->mnLAddr;
#ifdef CHANNEL_RIGHT
	else if(chn == channel_right)
		pTAS2562->client->addr = pTAS2562->mnRAddr;
#endif
	else
	{
		dev_err(pTAS2562->dev, "%s, wrong channel number\n", __func__);
	}

	nResult = tas2562_change_book_page(pTAS2562, chn,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_bulk_read(pTAS2562->regmap,
	TAS2562_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, pTAS2562->client->addr,
			TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), nLength);
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_update_bits(struct tas2562_priv *pTAS2562, enum channel chn,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);
	nResult = tas2562_change_book_page(pTAS2562, chn,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	if((chn&channel_left) || (pTAS2562->mnChannels == 1))
	{
		pTAS2562->client->addr = pTAS2562->mnLAddr;
		nResult = regmap_update_bits(pTAS2562->regmap,
			TAS2562_PAGE_REG(reg), mask, value);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val: 0x%x\n",
				__func__, pTAS2562->client->addr,
				TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
				TAS2562_PAGE_REG(reg), mask, value);
	}
#ifdef CHANNEL_RIGHT
	if((chn&channel_right) && (pTAS2562->mnChannels == 2))
	{
		pTAS2562->client->addr = pTAS2562->mnRAddr;
		nResult = regmap_update_bits(pTAS2562->regmap,
			TAS2562_PAGE_REG(reg), mask, value);
		if (nResult < 0)
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
		else
			dev_dbg(pTAS2562->dev, "%s: chn%x:BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val: 0x%x\n",
				__func__, pTAS2562->client->addr,
				TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
				TAS2562_PAGE_REG(reg), mask, value);
	}
#endif
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static bool tas2562_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static bool tas2562_writeable(struct device *dev, unsigned int reg)
{
	return true;
}
static const struct regmap_config tas2562_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2562_writeable,
	.volatile_reg = tas2562_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 1 * 128,
};


static void tas2562_hw_reset(struct tas2562_priv *pTAS2562)
{
	if (gpio_is_valid(pTAS2562->mnResetGPIO)) {
		gpio_direction_output(pTAS2562->mnResetGPIO, 0);
#ifdef CHANNEL_RIGHT
		gpio_direction_output(pTAS2562->mnResetGPIO2, 0);
#endif
		msleep(5);
		gpio_direction_output(pTAS2562->mnResetGPIO, 1);
#ifdef CHANNEL_RIGHT
		gpio_direction_output(pTAS2562->mnResetGPIO2, 1);
#endif
		msleep(2);
	}
	dev_err(pTAS2562->dev, "gpio up !!\n");

	pTAS2562->mnLCurrentBook = -1;
	pTAS2562->mnLCurrentPage = -1;
	pTAS2562->mnRCurrentBook = -1;
	pTAS2562->mnRCurrentPage = -1;
}

void tas2562_enableIRQ(struct tas2562_priv *pTAS2562, bool enable)
{
	if (enable) {
		if (pTAS2562->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2562->mnIRQGPIO))
			enable_irq(pTAS2562->mnIRQ);
#ifdef CHANNEL_RIGHT
		if (gpio_is_valid(pTAS2562->mnIRQGPIO2))
			enable_irq(pTAS2562->mnIRQ2);
#endif
		schedule_delayed_work(&pTAS2562->irq_work, msecs_to_jiffies(10));
		pTAS2562->mbIRQEnable = true;
	} else {
		if (gpio_is_valid(pTAS2562->mnIRQGPIO))
			disable_irq_nosync(pTAS2562->mnIRQ);
#ifdef CHANNEL_RIGHT
		if (gpio_is_valid(pTAS2562->mnIRQGPIO2))
			disable_irq_nosync(pTAS2562->mnIRQ2);
#endif
		pTAS2562->mbIRQEnable = false;
	}
}

static void irq_work_routine(struct work_struct *work)
{
	struct tas2562_priv *pTAS2562 =
		container_of(work, struct tas2562_priv, irq_work.work);
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0, nDevInt3Status = 0, nDevInt4Status = 0;
	int nCounter = 2;
	int nResult = 0;
	int irqreg;
	enum channel chn;

	dev_info(pTAS2562->dev, "%s\n", __func__);
//#ifdef CONFIG_TAS2562_CODEC
	mutex_lock(&pTAS2562->codec_lock);
//#endif

	if (pTAS2562->mbRuntimeSuspend) {
		dev_info(pTAS2562->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (pTAS2562->mnPowerState == TAS2562_POWER_SHUTDOWN) {
		dev_info(pTAS2562->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	nResult = pTAS2562->write(pTAS2562, channel_both, TAS2562_InterruptMaskReg0,
				TAS2562_InterruptMaskReg0_Disable);
	nResult = pTAS2562->write(pTAS2562, channel_both, TAS2562_InterruptMaskReg1,
				TAS2562_InterruptMaskReg1_Disable);

	if (nResult < 0)
		goto reload;

	if((pTAS2562->spk_l_control == 1) && (pTAS2562->spk_r_control == 1) && (pTAS2562->mnChannels == 2))
		chn = channel_both;
	else if(pTAS2562->spk_l_control == 1)
		chn = channel_left;
#ifdef CHANNEL_RIGHT
	else if((pTAS2562->spk_r_control == 1) && (pTAS2562->mnChannels == 2))
		chn = channel_right;
#endif
	else
		chn = channel_left; 

	if(chn && channel_left)
		nResult = pTAS2562->read(pTAS2562, channel_left, TAS2562_LatchedInterruptReg0, &nDevInt1Status);
	if (nResult >= 0)
		nResult = pTAS2562->read(pTAS2562, channel_left, TAS2562_LatchedInterruptReg1, &nDevInt2Status);
	else
		goto reload;

	//if(chn && channel_right) chen
#ifdef CHANNEL_RIGHT
	if(chn == channel_right)
		nResult = pTAS2562->read(pTAS2562, channel_right, TAS2562_LatchedInterruptReg0, &nDevInt3Status);
	if (nResult >= 0)
		nResult = pTAS2562->read(pTAS2562, channel_right, TAS2562_LatchedInterruptReg1, &nDevInt4Status);
	else
		goto reload;
#endif

	dev_dbg(pTAS2562->dev, "IRQ status : 0x%x, 0x%x, 0x%x, 0x%x\n",
			nDevInt3Status, nDevInt4Status, nDevInt3Status, nDevInt4Status);

	if (((nDevInt1Status & 0x7) != 0) || ((nDevInt2Status & 0x0f) != 0) ||
		((nDevInt3Status & 0x7) != 0) || ((nDevInt4Status & 0x0f) != 0)) {
		/* in case of INT_CLK, INT_OC, INT_OT, INT_OVLT, INT_UVLT, INT_BO */

		if ((nDevInt1Status & TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_Interrupt) ||
		 	(nDevInt3Status & TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_CLOCK;
			dev_err(pTAS2562->dev, "TDM clock error!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_OVER_CURRENT;

		if ((nDevInt1Status & TAS2562_LatchedInterruptReg0_OCEFlagSticky_Interrupt) ||
		 	(nDevInt3Status & TAS2562_LatchedInterruptReg0_OCEFlagSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2562->dev, "SPK over current!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_OVER_CURRENT;

		if ((nDevInt1Status & TAS2562_LatchedInterruptReg0_OTEFlagSticky_Interrupt) ||
			(nDevInt3Status & TAS2562_LatchedInterruptReg0_OTEFlagSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2562->dev, "die over temperature!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if ((nDevInt2Status & TAS2562_LatchedInterruptReg1_VBATOVLOSticky_Interrupt) ||
			(nDevInt4Status & TAS2562_LatchedInterruptReg1_VBATOVLOSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_OVER_VOLTAGE;
			dev_err(pTAS2562->dev, "SPK over voltage!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if ((nDevInt2Status & TAS2562_LatchedInterruptReg1_VBATUVLOSticky_Interrupt) ||
			(nDevInt4Status & TAS2562_LatchedInterruptReg1_VBATUVLOSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2562->dev, "SPK under voltage!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if ((nDevInt2Status & TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_Interrupt) ||
			(nDevInt4Status & TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_Interrupt)) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "brownout!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;

		goto reload;
	} else {
		nCounter = 2;

		while (nCounter > 0) {
			if(chn && channel_left)
				nResult = pTAS2562->read(pTAS2562, channel_left, TAS2562_PowerControl, &nDevInt1Status);
			if (nResult < 0)
				goto reload;
#ifdef CHANNEL_RIGHT
			if(chn == channel_right) //chen &&
			nResult = pTAS2562->read(pTAS2562, channel_right, TAS2562_PowerControl, &nDevInt3Status);
			if (nResult < 0)
				goto reload;
#endif
			if ((nDevInt1Status & TAS2562_PowerControl_OperationalMode10_Mask)
				!= TAS2562_PowerControl_OperationalMode10_Shutdown) {
				/* If only left should be power on */
				if(chn == channel_left)
					break;
				/* If both should be power on */
				if ((nDevInt3Status & TAS2562_PowerControl_OperationalMode10_Mask)
					!= TAS2562_PowerControl_OperationalMode10_Shutdown)			
				break;
			}
#ifdef CHANNEL_RIGHT
			/*If only right should be power on */
			else if (chn == channel_right) {
				if ((nDevInt3Status & TAS2562_PowerControl_OperationalMode10_Mask)
					!= TAS2562_PowerControl_OperationalMode10_Shutdown)				
				break;
			}
#endif

			pTAS2562->read(pTAS2562, channel_left, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s %d, %d\n", __func__, irqreg, __LINE__);
#ifdef CHANNEL_RIGHT
			pTAS2562->read(pTAS2562, channel_right, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s %d, %d\n", __func__, irqreg, __LINE__);
#endif
			nResult = pTAS2562->update_bits(pTAS2562, chn, TAS2562_PowerControl,
				TAS2562_PowerControl_OperationalMode10_Mask,
				TAS2562_PowerControl_OperationalMode10_Active);
			if (nResult < 0)
				goto reload;

			dev_info(pTAS2562->dev, "set ICN to -80dB\n");
			nResult = pTAS2562->bulk_write(pTAS2562, chn, TAS2562_ICN_REG, pICN, 4);

			pTAS2562->read(pTAS2562, channel_left, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s, %d, %d\n", __func__, irqreg, __LINE__);
#ifdef CHANNEL_RIGHT
			pTAS2562->read(pTAS2562, channel_right, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s %d, %d\n", __func__, irqreg, __LINE__);
#endif
			nCounter--;
			if (nCounter > 0) {
				/* in case check pow status just after power on TAS2562 */
				dev_dbg(pTAS2562->dev, "PowSts B: 0x%x, check again after 10ms\n",
					nDevInt1Status);
				msleep(10);
			}
		}

		if ((nDevInt1Status & TAS2562_PowerControl_OperationalMode10_Mask)
			== TAS2562_PowerControl_OperationalMode10_Shutdown) {
			dev_err(pTAS2562->dev, "%s, Critical ERROR REG[0x%x] = 0x%x\n",
				__func__,
				TAS2562_PowerControl,
				nDevInt1Status);
			pTAS2562->mnErrCode |= ERROR_CLASSD_PWR;
			goto reload;
		}
		pTAS2562->mnErrCode &= ~ERROR_CLASSD_PWR;
	}

	nResult = pTAS2562->write(pTAS2562, channel_left, TAS2562_InterruptMaskReg0, 0xf8);
	if (nResult < 0)
		goto reload;

	nResult = pTAS2562->write(pTAS2562, channel_left, TAS2562_InterruptMaskReg1, 0xb1);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	nResult = -1;
	tas2562_LoadConfig(pTAS2562);

	if (nResult >= 0) {
		tas2562_enableIRQ(pTAS2562, true);
	}

end:
//#ifdef CONFIG_TAS2562_CODEC
	mutex_unlock(&pTAS2562->codec_lock);
//#endif
}

static enum hrtimer_restart timer_func(struct hrtimer *timer)
{
	struct tas2562_priv *pTAS2562 = container_of(timer,
		struct tas2562_priv, mtimer);

	if (pTAS2562->mbPowerUp) {
		if (!delayed_work_pending(&pTAS2562->irq_work))
			schedule_delayed_work(&pTAS2562->irq_work,
				msecs_to_jiffies(20));
	}

	return HRTIMER_NORESTART;
}

static irqreturn_t tas2562_irq_handler(int irq, void *dev_id)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)dev_id;

	tas2562_enableIRQ(pTAS2562, false);
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&pTAS2562->irq_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static int tas2562_runtime_suspend(struct tas2562_priv *pTAS2562)
{
	dev_dbg(pTAS2562->dev, "%s\n", __func__);

	pTAS2562->mbRuntimeSuspend = true;

	if (hrtimer_active(&pTAS2562->mtimer)) {
		dev_dbg(pTAS2562->dev, "cancel die temp timer\n");
		hrtimer_cancel(&pTAS2562->mtimer);
	}

	if (delayed_work_pending(&pTAS2562->irq_work)) {
		dev_dbg(pTAS2562->dev, "cancel IRQ work\n");
		cancel_delayed_work_sync(&pTAS2562->irq_work);
	}

	return 0;
}

static int tas2562_runtime_resume(struct tas2562_priv *pTAS2562)
{
	dev_dbg(pTAS2562->dev, "%s\n", __func__);

	if (pTAS2562->mbPowerUp) {
/*		if (!hrtimer_active(&pTAS2562->mtimer)) {
		dev_dbg(pTAS2562->dev, "%s, start check timer\n", __func__);
			hrtimer_start(&pTAS2562->mtimer,
				ns_to_ktime((u64)CHECK_PERIOD * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
		}
*/
	}

	pTAS2562->mbRuntimeSuspend = false;

	return 0;
}

static int tas2562_parse_dt(struct device *dev, struct tas2562_priv *pTAS2562)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;

	rc = of_property_read_u32(np, "ti,asi-format", &pTAS2562->mnASIFormat);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,asi-format", np->full_name, rc);
	} else {
		dev_dbg(pTAS2562->dev, "ti,asi-format=%d",
			pTAS2562->mnASIFormat);
	}

	rc = of_property_read_u32(np, "ti,channels", &pTAS2562->mnChannels);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,channels", np->full_name, rc);
	} else {
		dev_dbg(pTAS2562->dev, "ti,channels=%d",
			pTAS2562->mnChannels);
	}

	rc = of_property_read_u32(np, "ti,left-channel", &pTAS2562->mnLAddr);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,left-channel", np->full_name, rc);
	} else {
		dev_dbg(pTAS2562->dev, "ti,left-channel=0x%x",
			pTAS2562->mnLAddr);
	}
#ifdef CHANNEL_RIGHT
	rc = of_property_read_u32(np, "ti,right-channel", &pTAS2562->mnRAddr);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,right-channel", np->full_name, rc);
	} else {
		dev_dbg(pTAS2562->dev, "ti,right-channel=0x%x",
			pTAS2562->mnRAddr);
	}
#endif
	pTAS2562->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnResetGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2562->mnResetGPIO);
	} else {
		dev_dbg(pTAS2562->dev, "ti,reset-gpio=%d",
			pTAS2562->mnResetGPIO);
	}
#ifdef CHANNEL_RIGHT
	pTAS2562->mnResetGPIO2 = of_get_named_gpio(np, "ti,reset-gpio2", 0);
	if (!gpio_is_valid(pTAS2562->mnResetGPIO2)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio2", np->full_name, pTAS2562->mnResetGPIO2);
	} else {
		dev_dbg(pTAS2562->dev, "ti,reset-gpio2=%d",
			pTAS2562->mnResetGPIO2);
	}
#endif
	pTAS2562->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2562->mnIRQGPIO);
	} else {
		dev_dbg(pTAS2562->dev, "ti,irq-gpio=%d", pTAS2562->mnIRQGPIO);
	}
#ifdef CHANNEL_RIGHT
	pTAS2562->mnIRQGPIO2 = of_get_named_gpio(np, "ti,irq-gpio2", 0);
	if (!gpio_is_valid(pTAS2562->mnIRQGPIO2)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio2", np->full_name, pTAS2562->mnIRQGPIO2);
	} else {
		dev_dbg(pTAS2562->dev, "ti,irq-gpio2=%d", pTAS2562->mnIRQGPIO2);
	}
#endif
	return ret;
}

static int tas2562_i2c_probe(struct i2c_client *pClient,
			const struct i2c_device_id *id)
{
	struct tas2562_priv *pTAS2562;
	int nResult;

	dev_err(&pClient->dev, "Driver ID: %s\n", TAS2562_DRIVER_ID);
	dev_info(&pClient->dev, "%s enter\n", __func__);

	pTAS2562 = devm_kzalloc(&pClient->dev,
		sizeof(struct tas2562_priv), GFP_KERNEL);
	if (pTAS2562 == NULL) {
		dev_err(&pClient->dev, "failed to get i2c device\n");
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2562->client = pClient;
	pTAS2562->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2562);
	dev_set_drvdata(&pClient->dev, pTAS2562);

	pTAS2562->regmap = devm_regmap_init_i2c(pClient, &tas2562_i2c_regmap);
	if (IS_ERR(pTAS2562->regmap)) {
		nResult = PTR_ERR(pTAS2562->regmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
					nResult);
		goto err;
	}

	if (pClient->dev.of_node)
		tas2562_parse_dt(&pClient->dev, pTAS2562);

	if (gpio_is_valid(pTAS2562->mnResetGPIO)) {
		nResult = gpio_request(pTAS2562->mnResetGPIO, "TAS2562_RESET");
		if (nResult) {
			dev_err(pTAS2562->dev, "%s: Failed to request gpio %d\n",
				__func__, pTAS2562->mnResetGPIO);
			nResult = -EINVAL;
			goto err; //chen gpio for tas
		}
		//tas2562_hw_reset(pTAS2562);
	}
#ifdef CHANNEL_RIGHT
	if (gpio_is_valid(pTAS2562->mnResetGPIO2)) {
		nResult = gpio_request(pTAS2562->mnResetGPIO2, "TAS2562_RESET2");
		if (nResult) {
			dev_err(pTAS2562->dev, "%s: Failed to request gpio %d\n",
				__func__, pTAS2562->mnResetGPIO2);
			nResult = -EINVAL;
			goto err;
		}
		tas2562_hw_reset(pTAS2562);
	}
#endif
	pTAS2562->read = tas2562_dev_read;
	pTAS2562->write = tas2562_dev_write;
	pTAS2562->bulk_read = tas2562_dev_bulk_read;
	pTAS2562->bulk_write = tas2562_dev_bulk_write;
	pTAS2562->update_bits = tas2562_dev_update_bits;
	pTAS2562->hw_reset = tas2562_hw_reset;
	pTAS2562->enableIRQ = tas2562_enableIRQ;
	pTAS2562->runtime_suspend = tas2562_runtime_suspend;
	pTAS2562->runtime_resume = tas2562_runtime_resume;
	pTAS2562->mnPowerState = TAS2562_POWER_SHUTDOWN;
	pTAS2562->spk_l_control = 1;

	mutex_init(&pTAS2562->dev_lock);

	dev_info(&pClient->dev, "Before SW reset\n");
	/* Reset the chip */
	nResult = tas2562_dev_write(pTAS2562, channel_both, TAS2562_SoftwareReset, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);
		goto err;
	}
	dev_info(&pClient->dev, "After SW reset\n");

	if (gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2562->mnIRQGPIO, "TAS2562-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2562->mnIRQGPIO);
			goto err;
		}
		gpio_direction_input(pTAS2562->mnIRQGPIO);
		tas2562_dev_write(pTAS2562, channel_both, TAS2562_MiscConfigurationReg0, 0xce);

		pTAS2562->mnIRQ = gpio_to_irq(pTAS2562->mnIRQGPIO);
		dev_info(pTAS2562->dev, "irq = %d\n", pTAS2562->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2562->irq_work, irq_work_routine);
		nResult = request_threaded_irq(pTAS2562->mnIRQ, tas2562_irq_handler,
				NULL, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
				pClient->name, pTAS2562);
		if (nResult < 0) {
			dev_err(pTAS2562->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2562->mnIRQ);
	}
#ifdef CHANNEL_RIGHT
	if (gpio_is_valid(pTAS2562->mnIRQGPIO2) && (pTAS2562->mnChannels == 2)) {
		nResult = gpio_request(pTAS2562->mnIRQGPIO2, "TAS2562-IRQ2");
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2562->mnIRQGPIO2);
			goto err;
		}
		gpio_direction_input(pTAS2562->mnIRQGPIO2);
		tas2562_dev_write(pTAS2562, channel_both, TAS2562_MiscConfigurationReg0, 0xce);

		pTAS2562->mnIRQ2 = gpio_to_irq(pTAS2562->mnIRQGPIO2);
		dev_info(pTAS2562->dev, "irq = %d\n", pTAS2562->mnIRQ2);
		INIT_DELAYED_WORK(&pTAS2562->irq_work, irq_work_routine);
		nResult = request_threaded_irq(pTAS2562->mnIRQ2, tas2562_irq_handler,
				NULL, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
				pClient->name, pTAS2562);
		if (nResult < 0) {
			dev_err(pTAS2562->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2562->mnIRQ2);
	}
#endif
//#ifdef CONFIG_TAS2562_CODEC
	mutex_init(&pTAS2562->codec_lock);
	nResult = tas2562_register_codec(pTAS2562);
	if (nResult < 0) {
		dev_err(pTAS2562->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
//#endif

#ifdef CONFIG_TAS2562_MISC
	mutex_init(&pTAS2562->file_lock);
	nResult = tas2562_register_misc(pTAS2562);
	if (nResult < 0) {
		dev_err(pTAS2562->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

	hrtimer_init(&pTAS2562->mtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2562->mtimer.function = timer_func;

	dev_info(&pClient->dev, "%s end\n", __func__);
err:
	return nResult;
}

static int tas2562_i2c_remove(struct i2c_client *pClient)
{
	struct tas2562_priv *pTAS2562 = i2c_get_clientdata(pClient);

	dev_info(pTAS2562->dev, "%s\n", __func__);

//#ifdef CONFIG_TAS2562_CODEC
	tas2562_deregister_codec(pTAS2562);
	mutex_destroy(&pTAS2562->codec_lock);
//#endif

#ifdef CONFIG_TAS2562_MISC
	tas2562_deregister_misc(pTAS2562);
	mutex_destroy(&pTAS2562->file_lock);
#endif

	if (gpio_is_valid(pTAS2562->mnResetGPIO))
		gpio_free(pTAS2562->mnResetGPIO);
	if (gpio_is_valid(pTAS2562->mnIRQGPIO))
		gpio_free(pTAS2562->mnIRQGPIO);
#ifdef CHANNEL_RIGHT
	if (gpio_is_valid(pTAS2562->mnResetGPIO2))
		gpio_free(pTAS2562->mnResetGPIO2);
	if (gpio_is_valid(pTAS2562->mnIRQGPIO2))
		gpio_free(pTAS2562->mnIRQGPIO2);
#endif
	return 0;
}


static const struct i2c_device_id tas2562_i2c_id[] = {
	{ "tas2562", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas2562_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2562_of_match[] = {
	{ .compatible = "ti,tas2562" },
	{},
};
MODULE_DEVICE_TABLE(of, tas2562_of_match);
#endif


static struct i2c_driver tas2562_i2c_driver = {
	.driver = {
		.name   = "tas2562",
		.owner  = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tas2562_of_match),
#endif
	},
	.probe      = tas2562_i2c_probe,
	.remove     = tas2562_i2c_remove,
	.id_table   = tas2562_i2c_id,
};

module_i2c_driver(tas2562_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2562 I2C Smart Amplifier driver");
MODULE_LICENSE("GPL");
//#endif
