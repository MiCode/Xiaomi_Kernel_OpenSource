/*
 * ALSA SoC Texas Instruments TAS2562 High Performance 4W Smart Amplifier
 *
 * Copyright (C) 2016 Texas Instruments, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define DEBUG
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
//Added/Modified 060356-PP
#include "tas2562-misc.h"

static int tas2562_change_book_page(struct tas2562_priv *pTAS2562,
	int book, int page)
{
	int nResult = 0;

	if ((pTAS2562->mnCurrentBook == book)
		&& (pTAS2562->mnCurrentPage == page))
		goto end;

	if (pTAS2562->mnCurrentBook != book) {
	nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, 0);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
	pTAS2562->mnCurrentPage = 0;
	nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_REG, book);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2562->mnCurrentBook = book;
	}

	if (pTAS2562->mnCurrentPage != page) {
	nResult = regmap_write(pTAS2562->regmap, TAS2562_BOOKCTL_PAGE, page);
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
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

	nResult = regmap_read(pTAS2562->regmap, TAS2562_PAGE_REG(reg), pValue);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_err(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u\n", __func__,
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

	nResult = regmap_write(pTAS2562->regmap, TAS2562_PAGE_REG(reg),
			value);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_err(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
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

	nResult = regmap_bulk_write(pTAS2562->regmap,
		TAS2562_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_err(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
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

	nResult = regmap_bulk_read(pTAS2562->regmap,
	TAS2562_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_err(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
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

	nResult = regmap_update_bits(pTAS2562->regmap,
	TAS2562_PAGE_REG(reg), mask, value);
	if (nResult < 0)
		dev_err(pTAS2562->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_err(pTAS2562->dev, "%s: BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val=0x%x\n",
			__func__, TAS2562_BOOK_ID(reg), TAS2562_PAGE_ID(reg),
			TAS2562_PAGE_REG(reg), mask, value);
end:
	mutex_unlock(&pTAS2562->dev_lock);
	return nResult;
}

static const struct reg_default tas2562_reg_defaults[] = {
	{ TAS2562_Page, 0x00 },
	{ TAS2562_SoftwareReset, 0x00 },
	{ TAS2562_PowerControl, 0x0e },
	{ TAS2562_PlaybackConfigurationReg0, 0x10 },
	{ TAS2562_PlaybackConfigurationReg1, 0x01 },
	{ TAS2562_PlaybackConfigurationReg2, 0x00 },
	{ TAS2562_MiscConfigurationReg0, 0x07 },
	{ TAS2562_TDMConfigurationReg1, 0x02 },
	{ TAS2562_TDMConfigurationReg2, 0x0a },
	{ TAS2562_TDMConfigurationReg3, 0x10 },
	{ TAS2562_InterruptMaskReg0, 0xfc },
	{ TAS2562_InterruptMaskReg1, 0xb1 },
	{ TAS2562_InterruptConfiguration, 0x05 },
	{ TAS2562_MiscIRQ, 0x81 },
	{ TAS2562_ClockConfiguration, 0x0c },

};

static bool tas2562_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS2562_Page: /* regmap implementation requires this */
	case TAS2562_SoftwareReset: /* always clears after write */
	case TAS2562_BrownOutPreventionReg0:/* has a self clearing bit */
	case TAS2562_LiveInterruptReg0:
	case TAS2562_LiveInterruptReg1:
	case TAS2562_LatchedInterruptReg0:/* Sticky interrupt flags */
	case TAS2562_LatchedInterruptReg1:/* Sticky interrupt flags */
	case TAS2562_VBATMSB:
	case TAS2562_VBATLSB:
	case TAS2562_TEMPMSB:
	case TAS2562_TEMPLSB:
		return true;
	}
	return false;
}

static bool tas2562_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS2562_LiveInterruptReg0:
	case TAS2562_LiveInterruptReg1:
	case TAS2562_LatchedInterruptReg0:
	case TAS2562_LatchedInterruptReg1:
	case TAS2562_VBATMSB:
	case TAS2562_VBATLSB:
	case TAS2562_TEMPMSB:
	case TAS2562_TEMPLSB:
	case TAS2562_TDMClockdetectionmonitor:
	case TAS2562_RevisionandPGID:
		return false;
	}
	return true;
}
static const struct regmap_config tas2562_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2562_writeable,
	.volatile_reg = tas2562_volatile,
	.reg_defaults = tas2562_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tas2562_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
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

	pTAS2562->mnCurrentBook = -1;
	pTAS2562->mnCurrentPage = -1;
}

void tas2562_enableIRQ(struct tas2562_priv *pTAS2562, bool enable)
{
	if (enable) {
		if (pTAS2562->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2562->mnIRQGPIO))
			enable_irq(pTAS2562->mnIRQ);

		schedule_delayed_work(&pTAS2562->irq_work, msecs_to_jiffies(10));
		pTAS2562->mbIRQEnable = true;
	} else {
		if (!pTAS2562->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2562->mnIRQGPIO))
			disable_irq_nosync(pTAS2562->mnIRQ);
		pTAS2562->mbIRQEnable = false;
	}
}

static void irq_work_routine(struct work_struct *work)
{
	struct tas2562_priv *pTAS2562 =
		container_of(work, struct tas2562_priv, irq_work.work);
	 unsigned int nDevInt1Status = 0, nDevInt2Status = 0; 
// int nCounter = 2; 
 int nResult = 0; 

#ifdef CONFIG_TAS2562_CODEC
	mutex_lock(&pTAS2562->codec_lock);
#endif

#ifdef CONFIG_TAS2562_MISC
	mutex_lock(&pTAS2562->file_lock);
#endif
	if (pTAS2562->mbRuntimeSuspend) {
		dev_info(pTAS2562->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}
/*
	if (!pTAS2562->mbPowerUp) {
		dev_info(pTAS2562->dev, "%s, device not powered\n", __func__);
		goto end;
	}
*/
#if 1
	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg0, 0x00);
	if (nResult < 0)
		goto reload;

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg1, 0x00);
	if (nResult < 0)
		goto reload;
	nResult = tas2562_dev_read(pTAS2562, TAS2562_LiveInterruptReg0, &nDevInt1Status);
	if (nResult >= 0)
	nResult = tas2562_dev_read(pTAS2562, TAS2562_LiveInterruptReg1, &nDevInt2Status);
	else
		goto reload;

	if (((nDevInt1Status & 0xff) != 0) || ((nDevInt2Status & 0x8f) != 0)) {
 /*in case of INT_OC, INT_UV, INT_OT, INT_BO, INT_CL, INT_CLK1, INT_CLK2*/
	dev_err(pTAS2562->dev, "IRQ critical Error : 0x%x, 0x%x\n",
			nDevInt1Status, nDevInt2Status);

		if (nDevInt1Status & 0x02) {
			pTAS2562->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2562->dev, "SPK over current!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_OVER_CURRENT;

		if (nDevInt1Status & 0x10) {
			pTAS2562->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2562->dev, "VBAT below limiter inflection point!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt1Status & 0x04) {
			pTAS2562->mnErrCode |= ERROR_CLK_HALT;
			dev_err(pTAS2562->dev, "TDM clock error!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_HALT;

		if (nDevInt1Status & 0x01) {
			pTAS2562->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2562->dev, "die over temperature!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if (nDevInt1Status & 0x80) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "limiter mute!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;

		if (nDevInt1Status & 0x40) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "limiter infinite hold!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;
		if (nDevInt1Status & 0x20) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "limiter max attenuation!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;
		if (nDevInt1Status & 0x04) {
			pTAS2562->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2562->dev, "limiter active!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_BROWNOUT;
		if (nDevInt2Status & 0x80) {
			pTAS2562->mnErrCode |= ERROR_CLK_DET1;
			dev_err(pTAS2562->dev, "PDM audio data invalid!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_DET1;

		if (nDevInt2Status & 0x08) {
			pTAS2562->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2562->dev, "VBAT OVLO flag!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_DET2;

		if (nDevInt2Status & 0x04) {
			pTAS2562->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2562->dev, "VBAT UVLO flag!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_DET2;

		if (nDevInt2Status & 0x02) {
			pTAS2562->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2562->dev, "VBAT brown out flag!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_DET2;

		if (nDevInt2Status & 0x01) {
			pTAS2562->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2562->dev, "PDM clock error!\n");
		} else
			pTAS2562->mnErrCode &= ~ERROR_CLK_DET2;
		goto reload;
	} else {
#if 0
		dev_err(pTAS2562->dev, "IRQ status : 0x%x, 0x%x\n",
				nDevInt1Status, nDevInt2Status);
		nCounter = 2;

		while (nCounter > 0) {
		nResult = tas2562_dev_read(pTAS2562,
			TAS2562_POWER_UP_FLAG_REG, &nDevInt1Status);
			if (nResult < 0)
				goto reload;

			if ((nDevInt1Status & 0xc0) == 0xc0)
				break;

			nCounter--;
			if (nCounter > 0) {
	/* in case check pow status just after power on TAS2562 */
				dev_err(pTAS2562->dev, "PowSts B: 0x%x, check again after 10ms\n",
					nDevInt1Status);
				msleep(20);
			}
		}

		if ((nDevInt1Status & 0xc0) != 0xc0) {
			dev_err(pTAS2562->dev, "%s, Critical ERROR B[%d]_P[%d]_R[%d]= 0x%x\n",
				__func__,
				TAS2562_BOOK_ID(TAS2562_POWER_UP_FLAG_REG),
				TAS2562_PAGE_ID(TAS2562_POWER_UP_FLAG_REG),
				TAS2562_PAGE_REG(TAS2562_POWER_UP_FLAG_REG),
				nDevInt1Status);
			pTAS2562->mnErrCode |= ERROR_CLASSD_PWR;
			goto reload;
		}
		pTAS2562->mnErrCode &= ~ERROR_CLASSD_PWR;
#endif
	}

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg0, 0xff);
	if (nResult < 0)
		goto reload;

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptMaskReg1, 0xff);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	//tas2562_LoadConfig(pTAS2562, true);
#endif

end:
/*
	if (!hrtimer_active(&pTAS2562->mtimer)) {
		dev_err(pTAS2562->dev, "%s, start timer\n", __func__);
		hrtimer_start(&pTAS2562->mtimer,
			ns_to_ktime((u64)CHECK_PERIOD * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
*/

#ifdef CONFIG_TAS2562_MISC
	mutex_unlock(&pTAS2562->file_lock);
#endif

#ifdef CONFIG_TAS2562_CODEC
	mutex_unlock(&pTAS2562->codec_lock);
#endif
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
static inline void irqd_set(struct irq_data *d, unsigned int mask)
{
	//Debug Addded/Modified 060356-PP
	//__irqd_to_state(d) |= mask;
}

static void irq_state_set_masked(struct irq_desc *desc)
{
	irqd_set(&desc->irq_data, IRQD_IRQ_MASKED);
}

static void irq_state_set_disabled(struct irq_desc *desc)
{
	irqd_set(&desc->irq_data, IRQD_IRQ_DISABLED);
}

static void irq_shutdown(struct irq_desc *desc)
{
	irq_state_set_disabled(desc);
	desc->depth = 1;
	if (desc->irq_data.chip->irq_shutdown)
		desc->irq_data.chip->irq_shutdown(&desc->irq_data);
	else if (desc->irq_data.chip->irq_disable)
		desc->irq_data.chip->irq_disable(&desc->irq_data);
	else
		desc->irq_data.chip->irq_mask(&desc->irq_data);
	irq_domain_deactivate_irq(&desc->irq_data);
	irq_state_set_masked(desc);
}

static irqreturn_t tas2562_irq_handler(int irq, void *dev_id)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)dev_id;
	struct irq_desc *desc = irq_to_desc(irq);

#if 1
	/* get IRQ status after 100 ms */
	if (!delayed_work_pending(&pTAS2562->irq_work))
		schedule_delayed_work(&pTAS2562->irq_work,
			msecs_to_jiffies(100));
#endif
	/* avoid interrupt storm, mask corresponding gic interrupt controller bit*/
	irq_shutdown(desc);
	return IRQ_HANDLED;
}

static int tas2562_runtime_suspend(struct tas2562_priv *pTAS2562)
{
	dev_err(pTAS2562->dev, "%s\n", __func__);

	pTAS2562->mbRuntimeSuspend = true;

	if (hrtimer_active(&pTAS2562->mtimer)) {
		dev_err(pTAS2562->dev, "cancel die temp timer\n");
		hrtimer_cancel(&pTAS2562->mtimer);
	}

	if (delayed_work_pending(&pTAS2562->irq_work)) {
		dev_err(pTAS2562->dev, "cancel IRQ work\n");
		cancel_delayed_work_sync(&pTAS2562->irq_work);
	}

	return 0;
}

static int tas2562_runtime_resume(struct tas2562_priv *pTAS2562)
{
	dev_err(pTAS2562->dev, "%s\n", __func__);

	if (pTAS2562->mbPowerUp) {
/*		if (!hrtimer_active(&pTAS2562->mtimer)) {
		dev_err(pTAS2562->dev, "%s, start check timer\n", __func__);
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

	u32 debounceInfo[2] = { 0, 0 };
	rc = of_property_read_u32(np, "ti,asi-format", &pTAS2562->mnASIFormat);
	if (rc) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,asi-format", np->full_name, rc);
	} else {
		dev_err(pTAS2562->dev, "ti,asi-format=%d",
			pTAS2562->mnASIFormat);
	}

	pTAS2562->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnResetGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2562->mnResetGPIO);
	} else {
		dev_err(pTAS2562->dev, "ti,reset-gpio=%d",
			pTAS2562->mnResetGPIO);
	}

	of_property_read_u32_array(np, "debounce",
			    debounceInfo, ARRAY_SIZE(debounceInfo));
	gpio_set_debounce(37, debounceInfo[1]);
	pTAS2562->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		dev_err(pTAS2562->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2562->mnIRQGPIO);
	} else {
		dev_err(pTAS2562->dev, "ti,irq-gpio=%d", pTAS2562->mnIRQGPIO);
	}

	return ret;
}

static int tas2562_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tas2562_priv *pTAS2562;
	int nResult;

	//struct pinctrl *pinctrl;
	//struct pinctrl_state *pin_state;
	dev_info(&client->dev, "%s enter\n", __func__);
	
	printk(KERN_ERR "TI probe begin !!\n");

	pTAS2562 = devm_kzalloc(&client->dev,
		sizeof(struct tas2562_priv), GFP_KERNEL);
	if (pTAS2562 == NULL) {
		nResult = -ENOMEM;
		goto end;
	}

	pTAS2562->dev = &client->dev;
	i2c_set_clientdata(client, pTAS2562);
	dev_set_drvdata(&client->dev, pTAS2562);

	pTAS2562->regmap = devm_regmap_init_i2c(client, &tas2562_i2c_regmap);
	if (IS_ERR(pTAS2562->regmap)) {
		nResult = PTR_ERR(pTAS2562->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
					nResult);
		goto end;
	}
#if 0
pinctrl = devm_pinctrl_get(&client->dev);
if (IS_ERR(pinctrl)) {
	    dev_err(&client->dev,
				       "Unable to acquire pinctrl: Error = %lu",
					           (unsigned long)pinctrl);
} else {
				     /* Configure GPIO interrupt pin */
				     pin_state = pinctrl_lookup_state(pinctrl, "ti_int");
					 pinctrl_select_state(pinctrl, pin_state);
}
#endif
/* TODO */
	if (client->dev.of_node)
		tas2562_parse_dt(&client->dev, pTAS2562);
	if (gpio_is_valid(pTAS2562->mnResetGPIO)) {
		nResult = gpio_request(pTAS2562->mnResetGPIO, "TAS2562_RESET");
		if (nResult) {
			dev_err(pTAS2562->dev, "%s: Failed to request gpio %d\n",
				__func__, pTAS2562->mnResetGPIO);
			nResult = -EINVAL;
			goto free_gpio;
		}
	}
#if 0
	if (gpio_is_valid(pTAS2562->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2562->mnIRQGPIO, "TAS2562-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2562->mnIRQGPIO);
			goto free_gpio;
		}
		gpio_direction_input(pTAS2562->mnIRQGPIO);
#endif
if (gpio_is_valid(pTAS2562->mnIRQGPIO)) {
	nResult = devm_gpio_request_one(&client->dev, pTAS2562->mnIRQGPIO,
		GPIOF_DIR_IN, "TAS2562_INT");

		if (nResult < 0) {
			dev_err(pTAS2562->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2562->mnIRQGPIO);
			goto free_gpio;
		}
		dev_err(pTAS2562->dev, "irq test point = %d\n", pTAS2562->mnIRQGPIO);
		pTAS2562->mnIRQ = gpio_to_irq(pTAS2562->mnIRQGPIO);
		dev_err(pTAS2562->dev, "irq = %d\n", pTAS2562->mnIRQ);
		nResult =devm_request_threaded_irq(&client->dev, pTAS2562->mnIRQ,
	    NULL, tas2562_irq_handler, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
					client->name, pTAS2562);
		if (nResult < 0) {
			dev_err(pTAS2562->dev,
				"request_irq failed, %d\n", nResult);
			goto free_gpio;
		}
		disable_irq_nosync(pTAS2562->mnIRQ);
		enable_irq(pTAS2562->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2562->irq_work, irq_work_routine);
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
	mutex_init(&pTAS2562->dev_lock);
	if (nResult < 0)
		goto destroy_mutex;

	tas2562_hw_reset(pTAS2562);

	nResult = tas2562_dev_write(pTAS2562, TAS2562_InterruptConfiguration, 0x00);
	if (nResult < 0)
		pr_err("Write TAS2562_InterruptConfiguration failed\n");
#ifdef CONFIG_TAS2562_CODEC
	mutex_init(&pTAS2562->codec_lock);
	tas2562_register_codec(pTAS2562);
#endif

#ifdef CONFIG_TAS2562_MISC
	mutex_init(&pTAS2562->file_lock);
	tas2562_register_misc(pTAS2562);
#endif

	hrtimer_init(&pTAS2562->mtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2562->mtimer.function = timer_func;

destroy_mutex:
	if (nResult < 0)
		mutex_destroy(&pTAS2562->dev_lock);

free_gpio:
	if (nResult < 0) {
		if (gpio_is_valid(pTAS2562->mnResetGPIO))
			gpio_free(pTAS2562->mnResetGPIO);
		if (gpio_is_valid(pTAS2562->mnIRQGPIO))
			gpio_free(pTAS2562->mnIRQGPIO);
	}

end:
	return nResult;
}

static int tas2562_i2c_remove(struct i2c_client *client)
{
	struct tas2562_priv *pTAS2562 = i2c_get_clientdata(client);

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
