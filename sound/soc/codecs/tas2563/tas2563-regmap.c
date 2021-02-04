/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2021 XiaoMi, Inc.
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
**     tas2563-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2563 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2563_REGMAP

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include "tas2563.h"

#ifdef CONFIG_TAS2563_CODEC
#include "tas2563-codec.h"
#endif

#ifdef CONFIG_TAS2563_MISC
#include "tas2563-misc.h"
#endif

#define ENABLE_TILOAD
#ifdef ENABLE_TILOAD
#include "tiload.h"
#endif

#define LOW_TEMPERATURE_GAIN 6
#define LOW_TEMPERATURE_COUNTER 12
static char pICN[] = {0x00, 0x00, 0x2f, 0x2c};
static char pICNDelay[] = {0x00, 0x00, 0x70, 0x80};

static int tas2563_change_book_page(struct tas2563_priv *pTAS2563,
	int book, int page)
{
	int nResult = 0;
	dev_dbg(pTAS2563->dev, "%s, %d", __func__, __LINE__);

	if ((pTAS2563->mnCurrentBook == book)
		&& (pTAS2563->mnCurrentPage == page))
		goto end;

	if (pTAS2563->mnCurrentBook != book) {
	nResult = regmap_write(pTAS2563->regmap, TAS2563_BOOKCTL_PAGE, 0);
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
	pTAS2563->mnCurrentPage = 0;
	nResult = regmap_write(pTAS2563->regmap, TAS2563_BOOKCTL_REG, book);
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2563->mnCurrentBook = book;
	}

	if (pTAS2563->mnCurrentPage != page) {
	nResult = regmap_write(pTAS2563->regmap, TAS2563_BOOKCTL_PAGE, page);
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2563->mnCurrentPage = page;
	}

end:
	return nResult;
}

static int tas2563_dev_read(struct tas2563_priv *pTAS2563,
	unsigned int reg, unsigned int *pValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2563->dev_lock);

	nResult = tas2563_change_book_page(pTAS2563,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_read(pTAS2563->regmap, TAS2563_PAGE_REG(reg), pValue);
	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2563->dev, "%s: BOOK:PAGE:REG %u:%u:%u,%x\n", __func__,
			TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), *pValue);

end:
	mutex_unlock(&pTAS2563->dev_lock);
	return nResult;
}

static int tas2563_dev_write(struct tas2563_priv *pTAS2563,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2563->dev_lock);

	nResult = tas2563_change_book_page(pTAS2563,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_write(pTAS2563->regmap, TAS2563_PAGE_REG(reg),
			value);
	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2563->dev, "%s: BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
			__func__, TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), value);

end:
	mutex_unlock(&pTAS2563->dev_lock);
	return nResult;
}

static int tas2563_dev_bulk_write(struct tas2563_priv *pTAS2563,
	unsigned int reg, u8 *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2563->dev_lock);

	nResult = tas2563_change_book_page(pTAS2563,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_bulk_write(pTAS2563->regmap,
		TAS2563_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2563->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), nLength);

end:
	mutex_unlock(&pTAS2563->dev_lock);
	return nResult;
}

static int tas2563_dev_bulk_read(struct tas2563_priv *pTAS2563,
	unsigned int reg, u8 *pData, unsigned int nLength)
{
	int nResult = 0;
	int i = 0;

	mutex_lock(&pTAS2563->dev_lock);

	nResult = tas2563_change_book_page(pTAS2563,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	dev_dbg(pTAS2563->dev, "reg = %u, pData = %s, Lenth = %d", reg, pData, nLength);

	#define STRIDE 4
	/* Read chunk bytes defined by STRIDE */
	for (i = 0; i < (nLength / STRIDE); i++) {
			nResult = regmap_bulk_read(pTAS2563->regmap,
							TAS2563_PAGE_REG((reg + i*STRIDE)),
							&pData[i*STRIDE], STRIDE);
			if (nResult < 0) {
					dev_err(pTAS2563->dev, "%s, %d, I2C error %d\n",
							__func__, __LINE__, nResult);
					pTAS2563->mnErrCode |= ERROR_DEVA_I2C_COMM;
			} else
					pTAS2563->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}

	/* Read remaining bytes */
	if ((nLength % STRIDE) != 0) {
			nResult = regmap_bulk_read(pTAS2563->regmap,
							TAS2563_PAGE_REG(reg + i*STRIDE),
							&pData[i*STRIDE], (nLength % STRIDE));
			if (nResult < 0) {
					dev_err(pTAS2563->dev, "%s, %d, I2C error %d\n",
							__func__, __LINE__, nResult);
					pTAS2563->mnErrCode |= ERROR_DEVA_I2C_COMM;
			} else
					pTAS2563->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}


end:
	mutex_unlock(&pTAS2563->dev_lock);
	return nResult;
}

static int tas2563_dev_update_bits(struct tas2563_priv *pTAS2563,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2563->dev_lock);
	nResult = tas2563_change_book_page(pTAS2563,
		TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_update_bits(pTAS2563->regmap,
	TAS2563_PAGE_REG(reg), mask, value);
	if (nResult < 0)
		dev_err(pTAS2563->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2563->dev, "%s: BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val=0x%x\n",
			__func__, TAS2563_BOOK_ID(reg), TAS2563_PAGE_ID(reg),
			TAS2563_PAGE_REG(reg), mask, value);
end:
	mutex_unlock(&pTAS2563->dev_lock);
	return nResult;
}

static const struct reg_default tas2563_reg_defaults[] = {
	{ TAS2563_Page, 0x00 },
	{ TAS2563_SoftwareReset, 0x00 },
	{ TAS2563_PowerControl, 0x0e },
	{ TAS2563_PlaybackConfigurationReg0, 0x10 },
	{ TAS2563_MiscConfigurationReg0, 0x07 },
	{ TAS2563_TDMConfigurationReg1, 0x02 },
	{ TAS2563_TDMConfigurationReg2, 0x0a },
	{ TAS2563_TDMConfigurationReg3, 0x10 },
	{ TAS2563_InterruptMaskReg0, 0xfc },
	{ TAS2563_InterruptMaskReg1, 0xb1 },
	{ TAS2563_InterruptConfiguration, 0x1d },
	{ TAS2563_MiscIRQ, 0x81 },
	{ TAS2563_ClockConfiguration, 0x0c },

};

static bool tas2563_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static bool tas2563_writeable(struct device *dev, unsigned int reg)
{
	return true;
}
static const struct regmap_config tas2563_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2563_writeable,
	.volatile_reg = tas2563_volatile,
//	.reg_defaults = tas2563_reg_defaults,
//	.num_reg_defaults = ARRAY_SIZE(tas2563_reg_defaults),
	.cache_type = REGCACHE_NONE,
	.max_register = 1 * 128,
};


static void tas2563_hw_reset(struct tas2563_priv *pTAS2563)
{
	if (gpio_is_valid(pTAS2563->mnResetGPIO)) {
		gpio_direction_output(pTAS2563->mnResetGPIO, 0);
		msleep(5);
		gpio_direction_output(pTAS2563->mnResetGPIO, 1);
		msleep(2);
	}

	pTAS2563->mnCurrentBook = -1;
	pTAS2563->mnCurrentPage = -1;
}

void tas2563_enableIRQ(struct tas2563_priv *pTAS2563, bool enable)
{
	if (enable) {
		if (pTAS2563->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2563->mnIRQGPIO))
			enable_irq(pTAS2563->mnIRQ);

		schedule_delayed_work(&pTAS2563->irq_work, msecs_to_jiffies(10));
		pTAS2563->mbIRQEnable = true;
	} else {
		if (gpio_is_valid(pTAS2563->mnIRQGPIO))
			disable_irq_nosync(pTAS2563->mnIRQ);
		pTAS2563->mbIRQEnable = false;
	}
}


static void irq_work_routine(struct work_struct *work)
{
	struct tas2563_priv *pTAS2563 =
		container_of(work, struct tas2563_priv, irq_work.work);
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0;
	int nCounter = 2;
	int nResult = 0;
	int irqreg;

	dev_info(pTAS2563->dev, "%s\n", __func__);
#ifdef CONFIG_TAS2563_CODEC
	mutex_lock(&pTAS2563->codec_lock);
#endif

	if (pTAS2563->mbRuntimeSuspend) {
		dev_info(pTAS2563->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (pTAS2563->mnPowerState == TAS2563_POWER_SHUTDOWN) {
		dev_info(pTAS2563->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	nResult = tas2563_dev_write(pTAS2563, TAS2563_InterruptMaskReg0,
				TAS2563_InterruptMaskReg0_Disable);
	nResult = tas2563_dev_write(pTAS2563, TAS2563_InterruptMaskReg1,
				TAS2563_InterruptMaskReg1_Disable);

	if (nResult < 0)
		goto reload;

	nResult = tas2563_dev_read(pTAS2563, TAS2563_LatchedInterruptReg0, &nDevInt1Status);
	if (nResult >= 0)
		nResult = tas2563_dev_read(pTAS2563, TAS2563_LatchedInterruptReg1, &nDevInt2Status);
	else
		goto reload;

	dev_info(pTAS2563->dev, "IRQ status : 0x%x, 0x%x\n",
			nDevInt1Status, nDevInt2Status);

	if (((nDevInt1Status & 0x7) != 0) || ((nDevInt2Status & 0x0f) != 0)) {
		/* in case of INT_OC, INT_OT, INT_OVLT, INT_UVLT, INT_BO */

		if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OCEFlagSticky_Interrupt) {
			pTAS2563->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2563->dev, "SPK over current!\n");
		} else
			pTAS2563->mnErrCode &= ~ERROR_OVER_CURRENT;

		if (nDevInt1Status & TAS2563_LatchedInterruptReg0_OTEFlagSticky_Interrupt) {
			pTAS2563->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2563->dev, "die over temperature!\n");
		} else
			pTAS2563->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if (nDevInt2Status & TAS2563_LatchedInterruptReg1_VBATOVLOSticky_Interrupt) {
			pTAS2563->mnErrCode |= ERROR_OVER_VOLTAGE;
			dev_err(pTAS2563->dev, "SPK over voltage!\n");
		} else
			pTAS2563->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt2Status & TAS2563_LatchedInterruptReg1_VBATUVLOSticky_Interrupt) {
			pTAS2563->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2563->dev, "SPK under voltage!\n");
		} else
			pTAS2563->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt2Status & TAS2563_LatchedInterruptReg1_BrownOutFlagSticky_Interrupt) {
			pTAS2563->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2563->dev, "brownout!\n");
		} else
			pTAS2563->mnErrCode &= ~ERROR_BROWNOUT;

		goto reload;
	} else {
		nCounter = 2;

		while (nCounter > 0) {
			nResult = tas2563_dev_read(pTAS2563, TAS2563_PowerControl, &nDevInt1Status);
			if (nResult < 0)
				goto reload;

			if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
				!= TAS2563_PowerControl_OperationalMode10_Shutdown)
				break;

			pTAS2563->read(pTAS2563, TAS2563_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2563->dev, "IRQ reg is: %s %d, %d\n", __func__, irqreg, __LINE__);

			nResult = pTAS2563->update_bits(pTAS2563, TAS2563_PowerControl,
				TAS2563_PowerControl_OperationalMode10_Mask |
				TAS2563_PowerControl_ISNSPower_Mask |
				TAS2563_PowerControl_VSNSPower_Mask,
				TAS2563_PowerControl_OperationalMode10_Active |
				TAS2563_PowerControl_VSNSPower_Active |
				TAS2563_PowerControl_ISNSPower_Active);
			if (nResult < 0)
				goto reload;

			pTAS2563->read(pTAS2563, TAS2563_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2563->dev, "IRQ reg is: %s, %d, %d\n", __func__, irqreg, __LINE__);

			dev_info(pTAS2563->dev, "set ICN to -90dB\n");
			nResult = pTAS2563->bulk_write(pTAS2563, TAS2563_ICN_REG, pICN, 4);
			if(nResult < 0)
				goto reload;

			pTAS2563->read(pTAS2563, TAS2563_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2563->dev, "IRQ reg is: %d, %d\n", irqreg, __LINE__);

			dev_info(pTAS2563->dev, "set ICN delay\n");
			nResult = pTAS2563->bulk_write(pTAS2563, TAS2563_ICN_DELAY, pICNDelay, 4);

			pTAS2563->read(pTAS2563, TAS2563_LatchedInterruptReg0, &irqreg);
			dev_info(pTAS2563->dev, "IRQ reg is: %d, %d\n", irqreg, __LINE__);

			nCounter--;
			if (nCounter > 0) {
				/* in case check power status just after power on TAS2563 */
				dev_dbg(pTAS2563->dev, "PowSts B: 0x%x, check again after 10ms\n",
					nDevInt1Status);
				msleep(20);
			}
		}

		if ((nDevInt1Status & TAS2563_PowerControl_OperationalMode10_Mask)
			== TAS2563_PowerControl_OperationalMode10_Shutdown) {
			dev_err(pTAS2563->dev, "%s, Critical ERROR REG[0x%x] = 0x%x\n",
				__func__,
				TAS2563_PowerControl,
				nDevInt1Status);
			pTAS2563->mnErrCode |= ERROR_CLASSD_PWR;
			goto reload;
		}
		pTAS2563->mnErrCode &= ~ERROR_CLASSD_PWR;
	}

	nResult = tas2563_dev_write(pTAS2563, TAS2563_InterruptMaskReg0, 0xf8);
	if (nResult < 0)
		goto reload;

	nResult = tas2563_dev_write(pTAS2563, TAS2563_InterruptMaskReg1, 0xb1);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	nResult = -1;
	//tas2563_LoadConfig(pTAS2563);
	tas2563_set_program(pTAS2563, pTAS2563->mnCurrentProgram, pTAS2563->mnCurrentConfiguration);

end:
	if (nResult >= 0) {
		tas2563_enableIRQ(pTAS2563, true);
	}

#ifdef CONFIG_TAS2563_CODEC
	mutex_unlock(&pTAS2563->codec_lock);
#endif
}

static enum hrtimer_restart timer_func(struct hrtimer *timer)
{
	struct tas2563_priv *pTAS2563 = container_of(timer,
		struct tas2563_priv, mtimerwork);

	if (pTAS2563->mnPowerState != TAS2563_POWER_SHUTDOWN) {
		if (!delayed_work_pending(&pTAS2563->irq_work))
			schedule_delayed_work(&pTAS2563->irq_work,
				msecs_to_jiffies(20));
	}

	return HRTIMER_NORESTART;
}

static irqreturn_t tas2563_irq_handler(int irq, void *dev_id)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)dev_id;

	tas2563_enableIRQ(pTAS2563, false);
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&pTAS2563->irq_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}


static int tas2563_parse_dt(struct device *dev, struct tas2563_priv *pTAS2563)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;

	rc = of_property_read_u32(np, "ti,asi-format", &pTAS2563->mnASIFormat);
	if (rc) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,asi-format", np->full_name, rc);
	} else {
		dev_dbg(pTAS2563->dev, "ti,asi-format=%d",
			pTAS2563->mnASIFormat);
	}

	pTAS2563->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2563->mnResetGPIO)) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2563->mnResetGPIO);
	} else {
		dev_dbg(pTAS2563->dev, "ti,reset-gpio=%d",
			pTAS2563->mnResetGPIO);
	}

	pTAS2563->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2563->mnIRQGPIO);
	} else {
		dev_dbg(pTAS2563->dev, "ti,irq-gpio=%d", pTAS2563->mnIRQGPIO);
	}

	of_property_read_u32(np, "ti,left-slot", &pTAS2563->mnLeftSlot);
	if (rc) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,left-slot", np->full_name, rc);
	} else {
		dev_dbg(pTAS2563->dev, "ti,left-slot=%d",
			pTAS2563->mnLeftSlot);
	}

	of_property_read_u32(np, "ti,right-slot", &pTAS2563->mnRightSlot);
	if (rc) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,right-slot", np->full_name, rc);
	} else {
		dev_dbg(pTAS2563->dev, "ti,right-slot=%d",
			pTAS2563->mnRightSlot);
	}

	of_property_read_u32(np, "ti,imon-slot-no", &pTAS2563->mnImon_slot_no);
	if (rc) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,imon-slot-no", np->full_name, rc);
	} else {
		dev_dbg(pTAS2563->dev, "ti,imon-slot-no=%d",
			pTAS2563->mnImon_slot_no);
	}

	of_property_read_u32(np, "ti,vmon-slot-no", &pTAS2563->mnVmon_slot_no);
	if (rc) {
		dev_err(pTAS2563->dev, "Looking up %s property in node %s failed %d\n",
			"ti,vmon-slot-no", np->full_name, rc);
	} else {
		dev_dbg(pTAS2563->dev, "ti,vmon-slot-no=%d",
			pTAS2563->mnVmon_slot_no);
	}

	return ret;
}


static int tas2563_runtime_suspend(struct tas2563_priv *pTAS2563)
{
	dev_dbg(pTAS2563->dev, "%s\n", __func__);

	pTAS2563->mbRuntimeSuspend = true;

	if (gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		if (delayed_work_pending(&pTAS2563->irq_work)) {
			dev_dbg(pTAS2563->dev, "cancel IRQ work\n");
			cancel_delayed_work_sync(&pTAS2563->irq_work);
		}
	}

	return 0;
}

static int tas2563_runtime_resume(struct tas2563_priv *pTAS2563)
{
	struct TProgram *pProgram;

	dev_dbg(pTAS2563->dev, "%s\n", __func__);
	if (!pTAS2563->mpFirmware->mpPrograms) {
		dev_dbg(pTAS2563->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (pTAS2563->mnCurrentProgram >= pTAS2563->mpFirmware->mnPrograms) {
		dev_err(pTAS2563->dev, "%s, firmware corrupted\n", __func__);
		goto end;
	}

	pProgram = &(pTAS2563->mpFirmware->mpPrograms[pTAS2563->mnCurrentProgram]);

	pTAS2563->mbRuntimeSuspend = false;
end:

	return 0;
}


/* tas2563_i2c_probe :
* platform dependent
* should implement hardware reset functionality
*/
static int tas2563_i2c_probe(struct i2c_client *pClient,
	const struct i2c_device_id *pID)
{
	struct tas2563_priv *pTAS2563;
	int nResult = 0;
	unsigned int nValue = 0;
	const char *pFWName;

	dev_info(&pClient->dev, "%s enter\n", __func__);

	pTAS2563 = devm_kzalloc(&pClient->dev, sizeof(struct tas2563_priv), GFP_KERNEL);
	if (!pTAS2563) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2563->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2563);
	dev_set_drvdata(&pClient->dev, pTAS2563);

	pTAS2563->regmap = devm_regmap_init_i2c(pClient, &tas2563_i2c_regmap);
	if (IS_ERR(pTAS2563->regmap)) {
		nResult = PTR_ERR(pTAS2563->regmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		goto err;
	}

	if (pClient->dev.of_node)
		tas2563_parse_dt(&pClient->dev, pTAS2563);

	if (gpio_is_valid(pTAS2563->mnResetGPIO)) {
		nResult = gpio_request(pTAS2563->mnResetGPIO, "TAS2563-RESET");
		if (nResult < 0) {
			dev_err(pTAS2563->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2563->mnResetGPIO);
			goto err;
		}
		tas2563_hw_reset(pTAS2563);
	}

	pTAS2563->read = tas2563_dev_read;
	pTAS2563->write = tas2563_dev_write;
	pTAS2563->bulk_read = tas2563_dev_bulk_read;
	pTAS2563->bulk_write = tas2563_dev_bulk_write;
	pTAS2563->update_bits = tas2563_dev_update_bits;
	pTAS2563->enableIRQ = tas2563_enableIRQ;
//	pTAS2563->clearIRQ = tas2563_clearIRQ;
	pTAS2563->hw_reset = tas2563_hw_reset;
	pTAS2563->runtime_suspend = tas2563_runtime_suspend;
	pTAS2563->runtime_resume = tas2563_runtime_resume;
	pTAS2563->mnRestart = 0;
	pTAS2563->mnPowerState = TAS2563_POWER_SHUTDOWN;

	mutex_init(&pTAS2563->dev_lock);

	/* Reset the chip */
	nResult = tas2563_dev_write(pTAS2563, TAS2563_SoftwareReset, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);
//		goto err;
	}

	msleep(1);
	nResult = tas2563_dev_read(pTAS2563, TAS2563_RevisionandPGID, &nValue);
	pTAS2563->mnPGID = nValue;
	dev_info(pTAS2563->dev, "PGID: %d\n", pTAS2563->mnPGID);
	pFWName = TAS2563_FW_NAME;

	if (gpio_is_valid(pTAS2563->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2563->mnIRQGPIO, "TAS2563-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"%s: GPIO %d request INT error\n",
				__func__, pTAS2563->mnIRQGPIO);
			goto err;
		}

		gpio_direction_input(pTAS2563->mnIRQGPIO);
		pTAS2563->mnIRQ = gpio_to_irq(pTAS2563->mnIRQGPIO);
		dev_dbg(pTAS2563->dev, "irq = %d\n", pTAS2563->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2563->irq_work, irq_work_routine);
		nResult = request_threaded_irq(pTAS2563->mnIRQ, tas2563_irq_handler,
					NULL, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					pClient->name, pTAS2563);
		if (nResult < 0) {
			dev_err(pTAS2563->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2563->mnIRQ);
	}

	pTAS2563->mpFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2563->mpFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2563->mpCalFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2563->mpCalFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_TAS2563_CODEC
	mutex_init(&pTAS2563->codec_lock);
	nResult = tas2563_register_codec(pTAS2563);
	if (nResult < 0) {
		dev_err(pTAS2563->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

#ifdef CONFIG_TAS2563_MISC
	mutex_init(&pTAS2563->file_lock);
	nResult = tas2563_register_misc(pTAS2563);
	if (nResult < 0) {
		dev_err(pTAS2563->dev,
			"register codec failed, %d\n", nResult);
		goto err;
	}
#endif

#ifdef ENABLE_TILOAD
	tiload_driver_init(pTAS2563);
#endif

	hrtimer_init(&pTAS2563->mtimerwork, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2563->mtimerwork.function = timer_func;
	//INIT_WORK(&pTAS2563->mtimerwork, timer_func);

	request_firmware_nowait(THIS_MODULE, 1, pFWName,
		pTAS2563->dev, GFP_KERNEL, pTAS2563, tas2563_fw_ready);

err:

	return nResult;
}

static int tas2563_i2c_remove(struct i2c_client *pClient)
{
	struct tas2563_priv *pTAS2563 = i2c_get_clientdata(pClient);

	dev_info(pTAS2563->dev, "%s\n", __func__);

#ifdef CONFIG_TAS2563_CODEC
	tas2563_deregister_codec(pTAS2563);
	mutex_destroy(&pTAS2563->codec_lock);
#endif

#ifdef CONFIG_TAS2563_MISC
	tas2563_deregister_misc(pTAS2563);
	mutex_destroy(&pTAS2563->file_lock);
#endif

	mutex_destroy(&pTAS2563->dev_lock);
	return 0;
}

static const struct i2c_device_id tas2563_i2c_id[] = {
	{"tas2563", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas2563_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2563_of_match[] = {
	{.compatible = "ti,tas2563"},
	{},
};

MODULE_DEVICE_TABLE(of, tas2563_of_match);
#endif

static struct i2c_driver tas2563_i2c_driver = {
	.driver = {
			.name = "tas2563",
			.owner = THIS_MODULE,
#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(tas2563_of_match),
#endif
		},
	.probe = tas2563_i2c_probe,
	.remove = tas2563_i2c_remove,
	.id_table = tas2563_i2c_id,
};

module_i2c_driver(tas2563_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2563 I2C Smart Amplifier driver");
MODULE_LICENSE("GPL v2");

#endif
