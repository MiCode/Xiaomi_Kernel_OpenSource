/*
 * ALSA SoC Texas Instruments TAS2562 High Performance 4W Smart Amplifier
 *
 * Copyright (C) 2016 Texas Instruments, Inc.
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
#ifdef CONFIG_TAS2562_REGMAP

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
#include <linux/pm.h>

#include "tas2562.h"
#include "tas2562-codec.h"
#include "tas2562-misc.h"

static char pICN[] = {0x00, 0x03, 0x46, 0xdc};

static int tas2562_regmap_write(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;
	int retry_count = TAS2562_I2C_RETRY_COUNT;

	if(pTAS2562->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while(retry_count--)
	{
		nResult = regmap_write(pTAS2562->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if(retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2562_regmap_bulk_write(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TAS2562_I2C_RETRY_COUNT;

	if(pTAS2562->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while(retry_count --)
	{
		nResult = regmap_bulk_write(pTAS2562->regmap, reg,
			 pData, nLength);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if(retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2562_regmap_read(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int *value)
{
	int nResult = 0;
	int retry_count = TAS2562_I2C_RETRY_COUNT;

	if(pTAS2562->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while(retry_count --)
	{
		nResult = regmap_read(pTAS2562->regmap, reg,
			value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if(retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2562_regmap_bulk_read(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;
	int retry_count = TAS2562_I2C_RETRY_COUNT;

	if(pTAS2562->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while(retry_count --)
	{
		nResult = regmap_bulk_read(pTAS2562->regmap, reg,
			 pData, nLength);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if(retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2562_regmap_update_bits(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int nResult = 0;
	int retry_count = TAS2562_I2C_RETRY_COUNT;

	if(pTAS2562->i2c_suspend)
		return ERROR_I2C_SUSPEND;

	while(retry_count--)
	{
		nResult = regmap_update_bits(pTAS2562->regmap, reg,
			mask, value);
		if (nResult >= 0)
			break;
		msleep(20);
	}
	if(retry_count == -1)
		return ERROR_I2C_FAILED;
	else
		return 0;
}

static int tas2562_change_book_page(struct tas2562_priv *pTAS2562,
	int book, int page)
{
	int nResult = 0;

	if ((pTAS2562->mnCurrentBook == book)
		&& (pTAS2562->mnCurrentPage == page))
		goto end;

	if (pTAS2562->mnCurrentBook != book) {
		nResult = tas2562_regmap_write(pTAS2562, TAS2562_BOOKCTL_PAGE, 0);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
			goto end;
		}
		pTAS2562->mnCurrentPage = 0;
		nResult = tas2562_regmap_write(pTAS2562, TAS2562_BOOKCTL_REG, book);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
			goto end;
		}
		pTAS2562->mnCurrentBook = book;
	}

	if (pTAS2562->mnCurrentPage != page) {
		nResult = tas2562_regmap_write(pTAS2562, TAS2562_BOOKCTL_PAGE, page);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
			goto end;
		}
		pTAS2562->mnCurrentPage = page;
	}

end:
	return nResult;
}

static int tas2562_dev_read(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int *pValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = tas2562_regmap_read(pTAS2562, TAS2562_PAGE_REG(reg), pValue);
	if (nResult < 0) {
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
		pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
	}
	else
		dev_dbg(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u\n", __func__,
			TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg));

end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_write(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = tas2562_regmap_write(pTAS2562, TAS2562_PAGE_REG(reg),
			value);
	if (nResult < 0) {
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
		pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
	}
	else
		dev_dbg(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
			__func__, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), value);

end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_bulk_write(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = tas2562_regmap_bulk_write(pTAS2562,
		TAS2562_PAGE_REG(reg), pData, nLength);
	if (nResult < 0) {
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
		pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
	}
	else
		dev_dbg(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), nLength);

end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_bulk_read(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);

	nResult = tas2562_change_book_page(pTAS2562,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = tas2562_regmap_bulk_read(pTAS2562,
	TAS2562_PAGE_REG(reg), pData, nLength);
	if (nResult < 0) {
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
		pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
	}
	else
		dev_dbg(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), nLength);
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static int tas2562_dev_update_bits(struct tas2562_priv *pTAS2562,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2562->dev_lock);
	nResult = tas2562_change_book_page(pTAS2562,
		TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = tas2562_regmap_update_bits(pTAS2562,
	TAS2562_PAGE_REG(reg), mask, value);
	if (nResult < 0) {
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
		pTAS2562->mnErrCode |= ERROR_DEVA_I2C_COMM;
	}
	else
		dev_dbg(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val=0x%x\n",
			__func__, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), mask, value);
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
		msleep(5);
		gpio_direction_output(pTAS2562->mnResetGPIO, 1);
		msleep(2);
	}
	dev_err(pTAS2562->dev, "gpio up !!\n");

	pTAS2562->mnCurrentBook = -1;
	pTAS2562->mnCurrentPage = -1;
	pTAS2562->mnErrCode = 0;
}

void tas2562_enableIRQ(struct tas2562_priv *pTAS2562, bool enable)
{
	if (enable) {
		if (pTAS2562->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2562->mnIRQGPIO)){
			enable_irq(pTAS2562->mnIRQ);
			dev_info(pTAS2562->dev, "%s, Enable irq\n", __func__);
		}
		pTAS2562->mbIRQEnable = true;
	} else {
		if (gpio_is_valid(pTAS2562->mnIRQGPIO)){
			disable_irq_nosync(pTAS2562->mnIRQ);
			dev_info(pTAS2562->dev, "%s, Disable irq\n", __func__);
		}
		pTAS2562->mbIRQEnable = false;
	}
}

static void irq_work_routine(struct work_struct *work)
{
	struct tas2562_priv *pTAS2562 =
		container_of(work, struct tas2562_priv, irq_work.work);
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0;
	unsigned int nDevInt3Status = 0, nDevInt4Status = 0, nDevInt5Status = 0;
	int nCounter = 2;
	int nResult = 0;
	int irqreg;

	dev_info(pTAS2562->dev, "%s\n", __func__);
#ifdef CONFIG_TAS2562_CODEC
	mutex_lock(&pTAS2562->codec_lock);
#endif
	tas2562_enableIRQ(pTAS2562, false);
	nResult = gpio_get_value(pTAS2562->mnIRQGPIO);
	dev_info(pTAS2562->dev, "%s, irq GPIO state: %d\n", __func__, nResult);

	if (pTAS2562->mbRuntimeSuspend) {
		dev_info(pTAS2562->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (pTAS2562->mnPowerState == TAS2562_POWER_SHUTDOWN) {
		dev_info(pTAS2562->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	dev_info(pTAS2562->dev, "mnErrCode: 0x%x\n", pTAS2562->mnErrCode);
	if(pTAS2562->mnErrCode & ERROR_DEVA_I2C_COMM)
		goto reload;

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg0,
				TAS2562_InterruptMaskReg0_Disable);
	if (nResult < 0)
		goto reload;
	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg1,
				TAS2562_InterruptMaskReg1_Disable);
	if (nResult < 0)
		goto reload;

	pTAS2562->read(pTAS2562, TAS2562_TDMConfigurationReg4, &irqreg);
	if(irqreg != 0x01) {
		dev_info(pTAS2562->dev, "TX reg is: %s %d, %d\n", __func__, irqreg, __LINE__);
	}
	nResult = tas2562_dev_read(pTAS2562, TAS2562_LatchedInterruptReg0, &nDevInt1Status);
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LatchedInterruptReg1, &nDevInt2Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LatchedInterruptReg2, &nDevInt3Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LatchedInterruptReg3, &nDevInt4Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LatchedInterruptReg4, &nDevInt5Status);
	else
		goto reload;

	dev_info(pTAS2562->dev, "IRQ status : 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			nDevInt1Status, nDevInt2Status, nDevInt3Status, nDevInt4Status, nDevInt5Status);

	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_VBATMSB, &nDevInt3Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_VBATLSB, &nDevInt4Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_TEMP, &nDevInt5Status);
	else
		goto reload;
	dev_dbg(pTAS2562->dev, "VBAT status : 0x%x, 0x%x, temperature: 0x%x\n",
			nDevInt3Status, nDevInt4Status, nDevInt5Status);

	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LimiterConfigurationReg0, &nDevInt3Status);
	else
		goto reload;
	if (nResult >= 0)
		nResult = tas2562_dev_read(pTAS2562, TAS2562_LimiterConfigurationReg0, &nDevInt4Status);
	else
		goto reload;
	dev_dbg(pTAS2562->dev, " Thermal foldback : 0x%x, limiter status: 0x%x\n",
			nDevInt3Status, nDevInt4Status);

	if (((nDevInt1Status & 0x7) != 0) || ((nDevInt2Status & 0x0f) != 0) ||
			(gpio_get_value(pTAS2562->mnIRQGPIO) == 0)) {
		/* in case of any IRQ, INT_OC, INT_OT, INT_OVLT, INT_UVLT, INT_BO */

		if (nDevInt1Status & TAS2562_LatchedInterruptReg0_TDMClockErrorSticky_Mask) {
			pTAS2562->mnErrCode |= ERROR_DTMCLK_ERROR;
			dev_err(pTAS2562->dev, "TDM clk error!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_DTMCLK_ERROR;

		if (nDevInt1Status & TAS2562_LatchedInterruptReg0_OCEFlagSticky_Interrupt) {
			pTAS2562->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2562->dev, "SPK over current!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_OVER_CURRENT;

		if (nDevInt1Status & TAS2562_LatchedInterruptReg0_OTEFlagSticky_Interrupt) {
			pTAS2562->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2562->dev, "die over temperature!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if (nDevInt2Status & TAS2562_LatchedInterruptReg1_VBATOVLOSticky_Interrupt) {
			pTAS2562->mnErrCode |= ERROR_OVER_VOLTAGE;
			dev_err(pTAS2562->dev, "SPK over voltage!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt2Status & TAS2562_LatchedInterruptReg1_VBATUVLOSticky_Interrupt) {
			pTAS2562->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2562->dev, "SPK under voltage!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt2Status & TAS2562_LatchedInterruptReg1_BrownOutFlagSticky_Interrupt) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "brownout!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;

		dev_err(pTAS2562->dev, "before goto reload\n");
		goto reload;
	} else {
		nCounter = 2;

		while (nCounter > 0) {
			nResult = tas2562_dev_read(pTAS2562, TAS2562_PowerControl, &nDevInt1Status);
			if (nResult < 0)
				goto reload;

			if ((nDevInt1Status & TAS2562_PowerControl_OperationalMode10_Mask)
				!= TAS2562_PowerControl_OperationalMode10_Shutdown)
				break;

			pTAS2562->read(pTAS2562, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s %d, %d\n", __func__, irqreg, __LINE__);

			nResult = pTAS2562->update_bits(pTAS2562, TAS2562_PowerControl,
				TAS2562_PowerControl_OperationalMode10_Mask,
				TAS2562_PowerControl_OperationalMode10_Active);
			if (nResult < 0)
				goto reload;

			dev_info(pTAS2562->dev, "set ICN to -80dB\n");
			nResult = pTAS2562->bulk_write(pTAS2562, TAS2562_ICN_REG, pICN, 4);
			if (nResult < 0)
				goto reload;

			pTAS2562->read(pTAS2562, TAS2562_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2562->dev, "IRQ reg is: %s, %d, %d\n", __func__, irqreg, __LINE__);

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

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg0, 0xf8);
	if (nResult < 0)
		goto reload;

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg1, 0xb1);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	tas2562_LoadConfig(pTAS2562);

end:
	tas2562_enableIRQ(pTAS2562, true);

#ifdef CONFIG_TAS2562_CODEC
	mutex_unlock(&pTAS2562->codec_lock);
#endif
}

static void init_work_routine(struct work_struct *work)
{
	struct tas2562_priv *pTAS2562 =
		container_of(work, struct tas2562_priv, init_work.work);
	int nResult = 0;
	//int irqreg;
	//dev_info(pTAS2562->dev, "%s\n", __func__);
#ifdef CONFIG_TAS2562_CODEC
	mutex_lock(&pTAS2562->codec_lock);
#endif

	pTAS2562->update_bits(pTAS2562, TAS2562_PowerControl,
		TAS2562_PowerControl_OperationalMode10_Mask,
		TAS2562_PowerControl_OperationalMode10_Active);

	//dev_info(pTAS2562->dev, "set ICN to -80dB\n");
	nResult = pTAS2562->bulk_write(pTAS2562, TAS2562_ICN_REG, pICN, 4);

	nResult = gpio_get_value(pTAS2562->mnIRQGPIO);
	//dev_info(pTAS2562->dev, "%s, irq GPIO state: %d\n", __func__, nResult);

#ifdef CONFIG_TAS2562_CODEC
	mutex_unlock(&pTAS2562->codec_lock);
#endif
}


static irqreturn_t tas2562_irq_handler(int irq, void *dev_id)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)dev_id;
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&pTAS2562->irq_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static int tas2562_runtime_suspend(struct tas2562_priv *pTAS2562)
{
	dev_dbg(pTAS2562->dev, "%s\n", __func__);

	pTAS2562->mbRuntimeSuspend = true;


	if (delayed_work_pending(&pTAS2562->irq_work)) {
		dev_dbg(pTAS2562->dev, "cancel IRQ work\n");
		cancel_delayed_work_sync(&pTAS2562->irq_work);
	}

	return 0;
}

static int tas2562_runtime_resume(struct tas2562_priv *pTAS2562)
{
	dev_dbg(pTAS2562->dev, "%s\n", __func__);

	pTAS2562->mbRuntimeSuspend = false;

	return 0;
}

static int tas2562_parse_dt(struct device *dev, struct tas2562_priv *pTAS2562)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;

//	u32 debounceInfo[2] = { 0, 0 };
	rc = of_property_read_u32(np, "ti,asi-format", &pTAS2562->mnASIFormat);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,asi-format", np->full_name, rc);
	} else {
		dev_dbg(pTAS2562->dev, "ti,asi-format=%d",
			pTAS2562->mnASIFormat);
	}

	pTAS2562->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnResetGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2562->mnResetGPIO);
	} else {
		dev_dbg(pTAS2562->dev, "ti,reset-gpio=%d",
			pTAS2562->mnResetGPIO);
	}

	pTAS2562->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2562->mnIRQGPIO);
	} else {
		dev_dbg(pTAS2562->dev, "ti,irq-gpio=%d", pTAS2562->mnIRQGPIO);
	}

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
			goto err;
		}
		tas2562_hw_reset(pTAS2562);
	}

	pTAS2562->read = tas2562_dev_read;
	pTAS2562->write = tas2562_dev_write;
	pTAS2562->bulk_read = tas2562_dev_bulk_read;
	pTAS2562->bulk_write = tas2562_dev_bulk_write;
	pTAS2562->update_bits = tas2562_dev_update_bits;
	pTAS2562->hw_reset = tas2562_hw_reset;
	pTAS2562->enableIRQ = tas2562_enableIRQ;
	//pTAS2562->clearIRQ = tas2562_clearIRQ;
	pTAS2562->runtime_suspend = tas2562_runtime_suspend;
	pTAS2562->runtime_resume = tas2562_runtime_resume;
	pTAS2562->mnPowerState = TAS2562_POWER_SHUTDOWN;

	mutex_init(&pTAS2562->dev_lock);

	/* Reset the chip */
	nResult = tas2562_dev_write(pTAS2562, TAS2562_SoftwareReset, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);
		goto err;
	}

	if (gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2562->mnIRQGPIO, "TAS2562-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2562->mnIRQGPIO);
			goto err;
		}
		pTAS2562->write(pTAS2562, TAS2562_MiscConfigurationReg0, 0xcf);
		gpio_direction_input(pTAS2562->mnIRQGPIO);
		nResult = gpio_get_value(pTAS2562->mnIRQGPIO);
		dev_info(pTAS2562->dev, "irq GPIO state: %d\n", nResult);

		pTAS2562->mnIRQ = gpio_to_irq(pTAS2562->mnIRQGPIO);
		dev_dbg(pTAS2562->dev, "irq = %d\n", pTAS2562->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2562->irq_work, irq_work_routine);
		nResult = request_threaded_irq(pTAS2562->mnIRQ, tas2562_irq_handler,
				NULL, IRQF_TRIGGER_FALLING,
				pClient->name, pTAS2562);
		if (nResult < 0) {
			dev_err(pTAS2562->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		tas2562_enableIRQ(pTAS2562, true);
	}
	INIT_DELAYED_WORK(&pTAS2562->init_work, init_work_routine);

#ifdef CONFIG_TAS2562_CODEC
	mutex_init(&pTAS2562->codec_lock);
	nResult = tas2562_register_codec(pTAS2562);
	if (nResult < 0) {
		dev_err(pTAS2562->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

#ifdef CONFIG_TAS2562_MISC
	mutex_init(&pTAS2562->file_lock);
	nResult = tas2562_register_misc(pTAS2562);
	if (nResult < 0) {
		dev_err(pTAS2562->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

err:
	return nResult;
}

static int tas2562_i2c_remove(struct i2c_client *pClient)
{
	struct tas2562_priv *pTAS2562 = i2c_get_clientdata(pClient);

	dev_info(pTAS2562->dev, "%s\n", __func__);

#ifdef CONFIG_TAS2562_CODEC
	tas2562_deregister_codec(pTAS2562);
	mutex_destroy(&pTAS2562->codec_lock);
#endif

#ifdef CONFIG_TAS2562_MISC
	tas2562_deregister_misc(pTAS2562);
	mutex_destroy(&pTAS2562->file_lock);
#endif

	if (gpio_is_valid(pTAS2562->mnResetGPIO))
		gpio_free(pTAS2562->mnResetGPIO);
	if (gpio_is_valid(pTAS2562->mnIRQGPIO))
		gpio_free(pTAS2562->mnIRQGPIO);

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
MODULE_LICENSE("GPL v2");
#endif
