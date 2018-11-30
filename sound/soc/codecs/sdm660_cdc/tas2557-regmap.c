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
**     tas2557-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2557 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2557_REGMAP

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
#include <linux/interrupt.h>
#include "tas2557.h"
#include "tas2557-core.h"

#ifdef CONFIG_TAS2557_CODEC
#include "tas2557-codec.h"
#endif

#ifdef CONFIG_TAS2557_MISC
#include "tas2557-misc.h"
#endif

#define ENABLE_TILOAD
#ifdef ENABLE_TILOAD
#include "tiload.h"
#endif


#if defined(CONFIG_SND_SOC_TAS2557) && defined(CONFIG_SND_SOC_TFA98XX)
static int btas2557 = 1;
#endif

#define LOW_TEMPERATURE_GAIN 6
#define LOW_TEMPERATURE_COUNTER 12

static int tas2557_change_book_page(
	struct tas2557_priv *pTAS2557,
	unsigned char nBook,
	unsigned char nPage)
{
	int nResult = 0;

	if ((pTAS2557->mnCurrentBook == nBook)
		&& pTAS2557->mnCurrentPage == nPage)
		goto end;

	if (pTAS2557->mnCurrentBook != nBook) {
		nResult = regmap_write(pTAS2557->mpRegmap, TAS2557_BOOKCTL_PAGE, 0);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2557->mnCurrentPage = 0;
		nResult = regmap_write(pTAS2557->mpRegmap, TAS2557_BOOKCTL_REG, nBook);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2557->mnCurrentBook = nBook;
		if (nPage != 0) {
			nResult = regmap_write(pTAS2557->mpRegmap, TAS2557_BOOKCTL_PAGE, nPage);
			if (nResult < 0) {
				dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
					__func__, __LINE__, nResult);
				goto end;
			}
			pTAS2557->mnCurrentPage = nPage;
		}
	} else if (pTAS2557->mnCurrentPage != nPage) {
		nResult = regmap_write(pTAS2557->mpRegmap, TAS2557_BOOKCTL_PAGE, nPage);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2557->mnCurrentPage = nPage;
	}

end:
	if (nResult < 0)
		pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
	else
		pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;

	return nResult;
}

static int tas2557_dev_read(
	struct tas2557_priv *pTAS2557,
	unsigned int nRegister,
	unsigned int *pValue)
{
	int nResult = 0;
	unsigned int Value = 0;

	mutex_lock(&pTAS2557->dev_lock);

	if (pTAS2557->mbTILoadActive) {
		if (!(nRegister & 0x80000000))
			goto end; /* let only reads from TILoad pass. */
		nRegister &= ~0x80000000;

		dev_dbg(pTAS2557->dev, "TiLoad R REG B[%d]P[%d]R[%d]\n",
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister),
				TAS2557_PAGE_REG(nRegister));
	}

	nResult = tas2557_change_book_page(pTAS2557,
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister));
	if (nResult >= 0) {
		nResult = regmap_read(pTAS2557->mpRegmap, TAS2557_PAGE_REG(nRegister), &Value);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
			goto end;
		} else
			pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
		*pValue = Value;
	}

end:

	mutex_unlock(&pTAS2557->dev_lock);
	return nResult;
}

static int tas2557_dev_write(
	struct tas2557_priv *pTAS2557,
	unsigned int nRegister,
	unsigned int nValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2557->dev_lock);
	if ((nRegister == 0xAFFEAFFE) && (nValue == 0xBABEBABE)) {
		pTAS2557->mbTILoadActive = true;
		goto end;
	}

	if ((nRegister == 0xBABEBABE) && (nValue == 0xAFFEAFFE)) {
		pTAS2557->mbTILoadActive = false;
		goto end;
	}

	if (pTAS2557->mbTILoadActive) {
		if (!(nRegister & 0x80000000))
			goto end;/* let only writes from TILoad pass. */
		nRegister &= ~0x80000000;

		dev_dbg(pTAS2557->dev, "TiLoad W REG B[%d]P[%d]R[%d] =0x%x\n",
						TAS2557_BOOK_ID(nRegister),
						TAS2557_PAGE_ID(nRegister),
						TAS2557_PAGE_REG(nRegister),
						nValue);
	}

	nResult = tas2557_change_book_page(pTAS2557,
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister));
	if (nResult >= 0) {
		nResult = regmap_write(pTAS2557->mpRegmap, TAS2557_PAGE_REG(nRegister), nValue);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
		} else
			pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}

end:

	mutex_unlock(&pTAS2557->dev_lock);

	return nResult;
}

static int tas2557_dev_bulk_read(
	struct tas2557_priv *pTAS2557,
	unsigned int nRegister,
	u8 *pData,
	unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2557->dev_lock);
	if (pTAS2557->mbTILoadActive) {
		if (!(nRegister & 0x80000000))
			goto end; /* let only writes from TILoad pass. */

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2557->dev, "TiLoad BR REG B[%d]P[%d]R[%d], count=%d\n",
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister),
				TAS2557_PAGE_REG(nRegister),
				nLength);
	}

	nResult = tas2557_change_book_page(pTAS2557,
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister));
	if (nResult >= 0) {
		nResult = regmap_bulk_read(pTAS2557->mpRegmap, TAS2557_PAGE_REG(nRegister), pData, nLength);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
		} else
			pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}

end:

	mutex_unlock(&pTAS2557->dev_lock);
	return nResult;
}

static int tas2557_dev_bulk_write(
	struct tas2557_priv *pTAS2557,
	unsigned int nRegister,
	u8 *pData,
	unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2557->dev_lock);
	if (pTAS2557->mbTILoadActive) {
		if (!(nRegister & 0x80000000))
			goto end; /* let only writes from TILoad pass. */

		nRegister &= ~0x80000000;

		dev_dbg(pTAS2557->dev, "TiLoad BW REG B[%d]P[%d]R[%d], count=%d\n",
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister),
				TAS2557_PAGE_REG(nRegister),
				nLength);
	}

	nResult = tas2557_change_book_page(pTAS2557,
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister));
	if (nResult >= 0) {
		nResult = regmap_bulk_write(pTAS2557->mpRegmap, TAS2557_PAGE_REG(nRegister), pData, nLength);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
		} else
			pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}

end:

	mutex_unlock(&pTAS2557->dev_lock);
	return nResult;
}

static int tas2557_dev_update_bits(
	struct tas2557_priv *pTAS2557,
	unsigned int nRegister,
	unsigned int nMask,
	unsigned int nValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2557->dev_lock);

	if (pTAS2557->mbTILoadActive) {
		if (!(nRegister & 0x80000000))
			goto end; /* let only writes from TILoad pass. */

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2557->dev, "TiLoad SB REG B[%d]P[%d]R[%d], mask=0x%x, value=0x%x\n",
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister),
				TAS2557_PAGE_REG(nRegister),
				nMask, nValue);
	}

	nResult = tas2557_change_book_page(pTAS2557,
				TAS2557_BOOK_ID(nRegister),
				TAS2557_PAGE_ID(nRegister));
	if (nResult >= 0) {
		nResult = regmap_update_bits(pTAS2557->mpRegmap, TAS2557_PAGE_REG(nRegister), nMask, nValue);
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s, %d, I2C error %d\n",
				__func__, __LINE__, nResult);
			pTAS2557->mnErrCode |= ERROR_DEVA_I2C_COMM;
		} else
			pTAS2557->mnErrCode &= ~ERROR_DEVA_I2C_COMM;
	}

end:
	mutex_unlock(&pTAS2557->dev_lock);
	return nResult;
}

void tas2557_clearIRQ(struct tas2557_priv *pTAS2557)
{
	unsigned int nValue;
	int nResult = 0;

	nResult = pTAS2557->read(pTAS2557, TAS2557_FLAGS_1, &nValue);
	if (nResult >= 0)
		pTAS2557->read(pTAS2557, TAS2557_FLAGS_2, &nValue);

}


void tas2557_enableIRQ(struct tas2557_priv *pTAS2557, bool enable, bool startup_chk)
{
	if (enable) {
		if (!pTAS2557->mbIRQEnable) {
			if (gpio_is_valid(pTAS2557->mnGpioINT)) {
				enable_irq(pTAS2557->mnIRQ);
				if (startup_chk) {
					/* check after 10 ms */
					schedule_delayed_work(&pTAS2557->irq_work, msecs_to_jiffies(10));
				}
				pTAS2557->mbIRQEnable = true;
			}
		}
	} else {
		if (pTAS2557->mbIRQEnable) {
			if (gpio_is_valid(pTAS2557->mnGpioINT))
				disable_irq_nosync(pTAS2557->mnIRQ);
			pTAS2557->mbIRQEnable = false;
		}
	}
}

static void tas2557_hw_reset(struct tas2557_priv *pTAS2557)
{
	if (gpio_is_valid(pTAS2557->mnResetGPIO)) {
		gpio_direction_output(pTAS2557->mnResetGPIO, 0);
		msleep(5);
		gpio_direction_output(pTAS2557->mnResetGPIO, 1);
		msleep(2);
	}

	pTAS2557->mnCurrentBook = -1;
	pTAS2557->mnCurrentPage = -1;
	if (pTAS2557->mnErrCode)
		dev_info(pTAS2557->dev, "before reset, ErrCode=0x%x\n", pTAS2557->mnErrCode);
	pTAS2557->mnErrCode = 0;
}

static void irq_work_routine(struct work_struct *work)
{
	int nResult = 0;
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0;
	unsigned int nDevPowerUpFlag = 0;
	int nCounter = 2;
	struct tas2557_priv *pTAS2557 =
		container_of(work, struct tas2557_priv, irq_work.work);

#ifdef CONFIG_TAS2557_CODEC
	mutex_lock(&pTAS2557->codec_lock);
#endif

#ifdef CONFIG_TAS2557_MISC
	mutex_lock(&pTAS2557->file_lock);
#endif

	if (pTAS2557->mbRuntimeSuspend) {
		dev_info(pTAS2557->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (!pTAS2557->mbPowerUp) {
		dev_info(pTAS2557->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	if ((!pTAS2557->mpFirmware->mnConfigurations)
		|| (!pTAS2557->mpFirmware->mnPrograms)) {
		dev_info(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}
	nResult = tas2557_dev_write(pTAS2557, TAS2557_GPIO4_PIN_REG, 0x00);
	if (nResult < 0)
		goto program;
	nResult = tas2557_dev_read(pTAS2557, TAS2557_FLAGS_1, &nDevInt1Status);
	if (nResult >= 0)
		nResult = tas2557_dev_read(pTAS2557, TAS2557_FLAGS_2, &nDevInt2Status);
	if (nResult < 0)
		goto program;

	if (((nDevInt1Status & 0xfc) != 0) || ((nDevInt2Status & 0x0c) != 0)) {
		/* in case of INT_OC, INT_UV, INT_OT, INT_BO, INT_CL, INT_CLK1, INT_CLK2 */
		dev_err(pTAS2557->dev, "critical error: 0x%x, 0x%x\n", nDevInt1Status, nDevInt2Status);
		if (nDevInt1Status & 0x80) {
			pTAS2557->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2557->dev, "DEVA SPK over current!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_OVER_CURRENT;

		if (nDevInt1Status & 0x40) {
			pTAS2557->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2557->dev, "DEVA SPK under voltage!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt1Status & 0x20) {
			pTAS2557->mnErrCode |= ERROR_CLK_HALT;
			dev_err(pTAS2557->dev, "DEVA clk halted!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_CLK_HALT;

		if (nDevInt1Status & 0x10) {
			pTAS2557->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2557->dev, "DEVA die over temperature!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if (nDevInt1Status & 0x08) {
			pTAS2557->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2557->dev, "DEVA brownout!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_BROWNOUT;

		if (nDevInt1Status & 0x04) {
			pTAS2557->mnErrCode |= ERROR_CLK_LOST;
			dev_err(pTAS2557->dev, "DEVA clock lost!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_CLK_LOST;

		if (nDevInt2Status & 0x08) {
			pTAS2557->mnErrCode |= ERROR_CLK_DET1;
			dev_err(pTAS2557->dev, "DEVA clk detection 1!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_CLK_DET1;

		if (nDevInt2Status & 0x04) {
			pTAS2557->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2557->dev, "DEVA clk detection 2!\n");
		} else
			pTAS2557->mnErrCode &= ~ERROR_CLK_DET2;

		goto program;
	} else {
		dev_dbg(pTAS2557->dev, "IRQ Status: 0x%x, 0x%x\n", nDevInt1Status, nDevInt2Status);
		nCounter = 2;
		while (nCounter > 0) {
			nResult = tas2557_dev_read(pTAS2557, TAS2557_POWER_UP_FLAG_REG, &nDevPowerUpFlag);
			if (nResult < 0)
				goto program;
			if ((nDevPowerUpFlag & 0xc0) == 0xc0)
				break;
			nCounter--;
			if (nCounter > 0) {
				/* in case check pow status just after power on TAS2557 */
				dev_dbg(pTAS2557->dev, "PowSts: 0x%x, check again after 10ms\n",
					nDevPowerUpFlag);
				msleep(10);
			}
		}
		if ((nDevPowerUpFlag & 0xc0) != 0xc0) {
			dev_err(pTAS2557->dev, "%s, Critical ERROR B[%d]_P[%d]_R[%d]= 0x%x\n",
				__func__,
				TAS2557_BOOK_ID(TAS2557_POWER_UP_FLAG_REG),
				TAS2557_PAGE_ID(TAS2557_POWER_UP_FLAG_REG),
				TAS2557_PAGE_REG(TAS2557_POWER_UP_FLAG_REG),
				nDevPowerUpFlag);
			pTAS2557->mnErrCode |= ERROR_CLASSD_PWR;
			goto program;
		}
		pTAS2557->mnErrCode &= ~ERROR_CLASSD_PWR;

		dev_dbg(pTAS2557->dev, "%s: INT1=0x%x, INT2=0x%x; PowerUpFlag=0x%x\n",
			__func__, nDevInt1Status, nDevInt2Status, nDevPowerUpFlag);
		goto end;
	}

program:
	/* hardware reset and reload */
	nResult = -1;
	tas2557_set_program(pTAS2557, pTAS2557->mnCurrentProgram, pTAS2557->mnCurrentConfiguration);

end:
	if (nResult >= 0) {
		tas2557_dev_write(pTAS2557, TAS2557_GPIO4_PIN_REG, 0x07);
		tas2557_enableIRQ(pTAS2557, true, false);
	}
#ifdef CONFIG_TAS2557_MISC
	mutex_unlock(&pTAS2557->file_lock);
#endif

#ifdef CONFIG_TAS2557_CODEC
	mutex_unlock(&pTAS2557->codec_lock);
#endif
}

static irqreturn_t tas2557_irq_handler(int irq, void *dev_id)
{
	struct tas2557_priv *pTAS2557 = (struct tas2557_priv *)dev_id;

	tas2557_enableIRQ(pTAS2557, false, false);
	/* get IRQ status after 100 ms */
	schedule_delayed_work(&pTAS2557->irq_work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static enum hrtimer_restart temperature_timer_func(struct hrtimer *timer)
{
	struct tas2557_priv *pTAS2557 = container_of(timer, struct tas2557_priv, mtimer);

	if (pTAS2557->mbPowerUp) {
		schedule_work(&pTAS2557->mtimerwork);
		if (gpio_is_valid(pTAS2557->mnGpioINT)) {
			tas2557_enableIRQ(pTAS2557, false, false);
			schedule_delayed_work(&pTAS2557->irq_work, msecs_to_jiffies(1));
		}
	}
	return HRTIMER_NORESTART;
}

static void timer_work_routine(struct work_struct *work)
{
	struct tas2557_priv *pTAS2557 = container_of(work, struct tas2557_priv, mtimerwork);
	int nResult, nTemp, nActTemp;
	struct TProgram *pProgram;
	static int nAvg;

#ifdef CONFIG_TAS2557_CODEC
	mutex_lock(&pTAS2557->codec_lock);
#endif

#ifdef CONFIG_TAS2557_MISC
	mutex_lock(&pTAS2557->file_lock);
#endif

	if (pTAS2557->mbRuntimeSuspend) {
		dev_info(pTAS2557->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (!pTAS2557->mpFirmware->mnConfigurations) {
		dev_info(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	pProgram = &(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
	if (!pTAS2557->mbPowerUp
		|| (pProgram->mnAppMode != TAS2557_APP_TUNINGMODE)) {
		dev_info(pTAS2557->dev, "%s, pass, Pow=%d, program=%s\n",
			__func__, pTAS2557->mbPowerUp, pProgram->mpName);
		goto end;
	}

	nResult = tas2557_get_die_temperature(pTAS2557, &nTemp);
	if (nResult >= 0) {
		nActTemp = (int)(nTemp >> 23);
		dev_dbg(pTAS2557->dev, "Die=0x%x, degree=%d\n", nTemp, nActTemp);
		if (!pTAS2557->mnDieTvReadCounter)
			nAvg = 0;
		pTAS2557->mnDieTvReadCounter++;
		nAvg += nActTemp;
		if (!(pTAS2557->mnDieTvReadCounter % LOW_TEMPERATURE_COUNTER)) {
			nAvg /= LOW_TEMPERATURE_COUNTER;
			dev_dbg(pTAS2557->dev, "check : avg=%d\n", nAvg);
			if ((nAvg & 0x80000000) != 0) {
				/* if Die temperature is below ZERO */
				if (pTAS2557->mnDevCurrentGain != LOW_TEMPERATURE_GAIN) {
					nResult = tas2557_set_DAC_gain(pTAS2557, LOW_TEMPERATURE_GAIN);
					if (nResult < 0)
						goto end;
					pTAS2557->mnDevCurrentGain = LOW_TEMPERATURE_GAIN;
					dev_dbg(pTAS2557->dev, "LOW Temp: set gain to %d\n", LOW_TEMPERATURE_GAIN);
				}
			} else if (nAvg > 5) {
				/* if Die temperature is above 5 degree C */
				if (pTAS2557->mnDevCurrentGain != pTAS2557->mnDevGain) {
					nResult = tas2557_set_DAC_gain(pTAS2557, pTAS2557->mnDevGain);
					if (nResult < 0)
						goto end;
					pTAS2557->mnDevCurrentGain = pTAS2557->mnDevGain;
					dev_dbg(pTAS2557->dev, "LOW Temp: set gain to original\n");
				}
			}
			nAvg = 0;
		}

		if (pTAS2557->mbPowerUp)
			hrtimer_start(&pTAS2557->mtimer,
				ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

end:

#ifdef CONFIG_TAS2557_MISC
	mutex_unlock(&pTAS2557->file_lock);
#endif

#ifdef CONFIG_TAS2557_CODEC
	mutex_unlock(&pTAS2557->codec_lock);
#endif
}

static int tas2557_runtime_suspend(struct tas2557_priv *pTAS2557)
{
	dev_dbg(pTAS2557->dev, "%s\n", __func__);

	pTAS2557->mbRuntimeSuspend = true;

	if (hrtimer_active(&pTAS2557->mtimer)) {
		dev_dbg(pTAS2557->dev, "cancel die temp timer\n");
		hrtimer_cancel(&pTAS2557->mtimer);
	}
	if (work_pending(&pTAS2557->mtimerwork)) {
		dev_dbg(pTAS2557->dev, "cancel timer work\n");
		cancel_work_sync(&pTAS2557->mtimerwork);
	}
	if (gpio_is_valid(pTAS2557->mnGpioINT)) {
		if (delayed_work_pending(&pTAS2557->irq_work)) {
			dev_dbg(pTAS2557->dev, "cancel IRQ work\n");
			cancel_delayed_work_sync(&pTAS2557->irq_work);
		}
	}

	return 0;
}

static int tas2557_runtime_resume(struct tas2557_priv *pTAS2557)
{
	struct TProgram *pProgram;

	dev_dbg(pTAS2557->dev, "%s\n", __func__);
	if (!pTAS2557->mpFirmware->mpPrograms) {
		dev_dbg(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		goto end;
	}

	if (pTAS2557->mnCurrentProgram >= pTAS2557->mpFirmware->mnPrograms) {
		dev_err(pTAS2557->dev, "%s, firmware corrupted\n", __func__);
		goto end;
	}

	pProgram = &(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
	if (pTAS2557->mbPowerUp && (pProgram->mnAppMode == TAS2557_APP_TUNINGMODE)) {
		if (!hrtimer_active(&pTAS2557->mtimer)) {
			dev_dbg(pTAS2557->dev, "%s, start Die Temp check timer\n", __func__);
			pTAS2557->mnDieTvReadCounter = 0;
			hrtimer_start(&pTAS2557->mtimer,
				ns_to_ktime((u64)LOW_TEMPERATURE_CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	}

	pTAS2557->mbRuntimeSuspend = false;
end:

	return 0;
}

static bool tas2557_volatile(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static bool tas2557_writeable(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static const struct regmap_config tas2557_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2557_writeable,
	.volatile_reg = tas2557_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};

/* tas2557_i2c_probe :
* platform dependent
* should implement hardware reset functionality
*/
static int tas2557_i2c_probe(struct i2c_client *pClient,
	const struct i2c_device_id *pID)
{
	struct tas2557_priv *pTAS2557;
	int nResult = 0;
	unsigned int nValue = 0;
	const char *pFWName;

	dev_info(&pClient->dev, "%s enter\n", __func__);

	pTAS2557 = devm_kzalloc(&pClient->dev, sizeof(struct tas2557_priv), GFP_KERNEL);
	if (!pTAS2557) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2557->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2557);
	dev_set_drvdata(&pClient->dev, pTAS2557);

	pTAS2557->mpRegmap = devm_regmap_init_i2c(pClient, &tas2557_i2c_regmap);
	if (IS_ERR(pTAS2557->mpRegmap)) {
		nResult = PTR_ERR(pTAS2557->mpRegmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		goto err;
	}

	if (pClient->dev.of_node)
		tas2557_parse_dt(&pClient->dev, pTAS2557);

	if (gpio_is_valid(pTAS2557->mnResetGPIO)) {
		nResult = gpio_request(pTAS2557->mnResetGPIO, "TAS2557-RESET");
		if (nResult < 0) {
			dev_err(pTAS2557->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2557->mnResetGPIO);
			goto err;
		}
		tas2557_hw_reset(pTAS2557);
	}

	pTAS2557->read = tas2557_dev_read;
	pTAS2557->write = tas2557_dev_write;
	pTAS2557->bulk_read = tas2557_dev_bulk_read;
	pTAS2557->bulk_write = tas2557_dev_bulk_write;
	pTAS2557->update_bits = tas2557_dev_update_bits;
	pTAS2557->enableIRQ = tas2557_enableIRQ;
	pTAS2557->clearIRQ = tas2557_clearIRQ;
	pTAS2557->set_config = tas2557_set_config;
	pTAS2557->set_calibration = tas2557_set_calibration;
	pTAS2557->hw_reset = tas2557_hw_reset;
	pTAS2557->runtime_suspend = tas2557_runtime_suspend;
	pTAS2557->runtime_resume = tas2557_runtime_resume;

	mutex_init(&pTAS2557->dev_lock);

	/* Reset the chip */
	nResult = tas2557_dev_write(pTAS2557, TAS2557_SW_RESET_REG, 0x01);
	if (nResult < 0) {
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);
		goto err;
	}

	msleep(1);
	tas2557_dev_read(pTAS2557, TAS2557_REV_PGID_REG, &nValue);
	pTAS2557->mnPGID = nValue;
	if (pTAS2557->mnPGID == TAS2557_PG_VERSION_2P1) {
		dev_info(pTAS2557->dev, "PG2.1 Silicon found\n");
		pFWName = TAS2557_FW_NAME;
	} else if (pTAS2557->mnPGID == TAS2557_PG_VERSION_1P0) {
		dev_info(pTAS2557->dev, "PG1.0 Silicon found\n");
		pFWName = TAS2557_PG1P0_FW_NAME;
	} else {
		nResult = -ENOTSUPP;
		dev_info(pTAS2557->dev, "unsupport Silicon 0x%x\n", pTAS2557->mnPGID);
		goto err;
	}

	if (gpio_is_valid(pTAS2557->mnGpioINT)) {
		nResult = gpio_request(pTAS2557->mnGpioINT, "TAS2557-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2557->dev,
				"%s: GPIO %d request INT error\n",
				__func__, pTAS2557->mnGpioINT);
			goto err;
		}

		gpio_direction_input(pTAS2557->mnGpioINT);
		pTAS2557->mnIRQ = gpio_to_irq(pTAS2557->mnGpioINT);
		dev_dbg(pTAS2557->dev, "irq = %d\n", pTAS2557->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2557->irq_work, irq_work_routine);
		nResult = request_threaded_irq(pTAS2557->mnIRQ, tas2557_irq_handler,
					NULL, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				pClient->name, pTAS2557);
		if (nResult < 0) {
			dev_err(pTAS2557->dev,
				"request_irq failed, %d\n", nResult);
			goto err;
		}
		disable_irq_nosync(pTAS2557->mnIRQ);
	}

	pTAS2557->mpFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2557->mpFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2557->mpCalFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);
	if (!pTAS2557->mpCalFirmware) {
		nResult = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_TAS2557_CODEC
	mutex_init(&pTAS2557->codec_lock);
	tas2557_register_codec(pTAS2557);
#endif

#ifdef CONFIG_TAS2557_MISC
	mutex_init(&pTAS2557->file_lock);
	tas2557_register_misc(pTAS2557);
#endif

#ifdef ENABLE_TILOAD
	tiload_driver_init(pTAS2557);
#endif

	hrtimer_init(&pTAS2557->mtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2557->mtimer.function = temperature_timer_func;
	INIT_WORK(&pTAS2557->mtimerwork, timer_work_routine);

	nResult = request_firmware_nowait(THIS_MODULE, 1, pFWName,
		pTAS2557->dev, GFP_KERNEL, pTAS2557, tas2557_fw_ready);

	return nResult;
err:

#if defined(CONFIG_SND_SOC_TAS2557) && defined(CONFIG_SND_SOC_TFA98XX)
	btas2557 = 0;
#endif
	return nResult;
}

static int tas2557_i2c_remove(struct i2c_client *pClient)
{
	struct tas2557_priv *pTAS2557 = i2c_get_clientdata(pClient);

	dev_info(pTAS2557->dev, "%s\n", __func__);

#ifdef CONFIG_TAS2557_CODEC
	tas2557_deregister_codec(pTAS2557);
	mutex_destroy(&pTAS2557->codec_lock);
#endif

#ifdef CONFIG_TAS2557_MISC
	tas2557_deregister_misc(pTAS2557);
	mutex_destroy(&pTAS2557->file_lock);
#endif

	mutex_destroy(&pTAS2557->dev_lock);
	return 0;
}

static const struct i2c_device_id tas2557_i2c_id[] = {
	{"tas2557", 0},
	{}
};


#if defined(CONFIG_SND_SOC_TAS2557) && defined(CONFIG_SND_SOC_TFA98XX)
int smartpa_is_tas2557(void)
{
	return btas2557;
}
#endif

MODULE_DEVICE_TABLE(i2c, tas2557_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2557_of_match[] = {
	{.compatible = "ti,tas2557"},
	{},
};

MODULE_DEVICE_TABLE(of, tas2557_of_match);
#endif

static struct i2c_driver tas2557_i2c_driver = {
	.driver = {
			.name = "tas2557",
			.owner = THIS_MODULE,
#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(tas2557_of_match),
#endif
		},
	.probe = tas2557_i2c_probe,
	.remove = tas2557_i2c_remove,
	.id_table = tas2557_i2c_id,
};

module_i2c_driver(tas2557_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2557 I2C Smart Amplifier driver");
MODULE_LICENSE("GPL v2");


#if defined(CONFIG_SND_SOC_TAS2557) && defined(CONFIG_SND_SOC_TFA98XX)
EXPORT_SYMBOL(smartpa_is_tas2557);
#endif

#endif
