/*
* Copyright (c) 2014-2015 MediaTek Inc.
* Author: Tianping.Fang <tianping.fang@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/nvmem-provider.h>


#define RTC_BBPU		0x0000
#define RTC_BBPU_KEY		0x4300
#define RTC_BBPU_PWREN		BIT(0)
#define RTC_BBPU_CLR		BIT(1)
#define RTC_BBPU_RELOAD		BIT(5)
#define RTC_BBPU_CBUSY		BIT(6)

#define RTC_WRTGR_MT6358	0x3a
#define RTC_WRTGR_MT6397	0x3c

#define RTC_IRQ_STA		0x0002
#define RTC_IRQ_STA_AL		BIT(0)
#define RTC_IRQ_STA_LP		BIT(3)

#define RTC_IRQ_EN		0x0004
#define RTC_IRQ_EN_AL		BIT(0)
#define RTC_IRQ_EN_ONESHOT	BIT(2)
#define RTC_IRQ_EN_LP		BIT(3)
#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_TC_SEC_MASK		0x3f
#define RTC_TC_MIN_MASK		0x3f
#define RTC_TC_HOU_MASK		0x1f
#define RTC_TC_DOM_MASK		0x1f
#define RTC_TC_DOW_MASK		0x7
#define RTC_TC_MTH_MASK		0xf
#define RTC_TC_YEA_MASK		0x7f

#define RTC_AL_SEC_MASK		0x003f
#define RTC_AL_MIN_MASK		0x003f
#define RTC_AL_HOU_MASK		0x001f
#define RTC_AL_DOM_MASK		0x001f
#define RTC_AL_DOW_MASK		0x0007
#define RTC_AL_MTH_MASK		0x000f
#define RTC_AL_YEA_MASK		0x007f

#define RTC_AL_MASK		0x0008
#define RTC_AL_MASK_DOW		BIT(4)

#define RTC_TC_SEC		0x000a
/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC		0
#define RTC_OFFSET_MIN		1
#define RTC_OFFSET_HOUR		2
#define RTC_OFFSET_DOM		3
#define RTC_OFFSET_DOW		4
#define RTC_OFFSET_MTH		5
#define RTC_OFFSET_YEAR		6
#define RTC_OFFSET_COUNT	7

#define RTC_AL_SEC		0x0018
#define RTC_BBPU_AUTO_PDN_SEL	BIT(6)
#define RTC_BBPU_2SEC_EN	BIT(8)
#define RTC_AL_HOU		0x001c
#define RTC_AL_MTH		0x0022
#define RTC_AL_YEA		0x0024
#define RTC_K_EOSC_RSV_0	BIT(8)
#define RTC_K_EOSC_RSV_1	BIT(9)
#define RTC_K_EOSC_RSV_2	BIT(10)

#define RTC_OSC32CON	0x0026
#define RTC_OSC32CON_UNLOCK1			0x1a57
#define RTC_OSC32CON_UNLOCK2			0x2b68
#define RTC_EMBCK_SRC_SEL	BIT(8)

#define RTC_PDN2		0x002e
#define RTC_PDN2_PWRON_ALARM	BIT(4)

#define RTC_SPAR0		0x0030

#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_NUM_YEARS		128
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)

#define SPARE_REG_WIDTH		1

enum mtk_rtc_spare_enum {
	SPARE_AL_HOU,
	SPARE_AL_MTH,
	SPARE_SPAR0,
	SPARE_RG_MAX,
};

enum rtc_eosc_cali_td {
	EOSC_CALI_TD_01_SEC = 0x3,
	EOSC_CALI_TD_02_SEC,
	EOSC_CALI_TD_04_SEC,
	EOSC_CALI_TD_08_SEC,
	EOSC_CALI_TD_16_SEC,
};

enum cali_field_enum {
	RTC_EOSC32_CK_PDN,
	EOSC_CALI_TD,
	CALI_FILED_MAX
};

enum eosc_cali_version {
	EOSC_CALI_NONE,
	EOSC_CALI_MT6357_SERIES,
	EOSC_CALI_MT6358_SERIES,
	EOSC_CALI_MT6359_SERIES,
};

struct mtk_rtc_compatible {
	u32			wrtgr_addr;
	const struct reg_field *spare_reg_fields;
	const struct reg_field *cali_reg_fields;
	u32			eosc_cali_version;
};

struct mt6397_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	struct regmap		*regmap;
	int			irq;
	u32			addr_base;
	const struct mtk_rtc_compatible *dev_comp;
	struct regmap_field	*spare[SPARE_RG_MAX];
	struct regmap_field	*cali[CALI_FILED_MAX];
	bool			cali_is_supported;
};

static const struct reg_field mt6357_cali_reg_fields[CALI_FILED_MAX] = {
	[RTC_EOSC32_CK_PDN]	= REG_FIELD(MT6357_SCK_TOP_CKPDN_CON0, 2, 2),
	[EOSC_CALI_TD]		= REG_FIELD(MT6357_EOSC_CALI_CON0, 5, 7),
};

static const struct reg_field mt6358_cali_reg_fields[CALI_FILED_MAX] = {
	[RTC_EOSC32_CK_PDN]	= REG_FIELD(MT6358_SCK_TOP_CKPDN_CON0, 2, 2),
	[EOSC_CALI_TD]		= REG_FIELD(MT6358_EOSC_CALI_CON0, 5, 7),
};

static const struct reg_field mt6359_cali_reg_fields[CALI_FILED_MAX] = {
	[RTC_EOSC32_CK_PDN]	= REG_FIELD(MT6359_SCK_TOP_CKPDN_CON0, 2, 2),
	[EOSC_CALI_TD]		= REG_FIELD(MT6359_EOSC_CALI_CON0, 5, 7),
};

static const struct reg_field mtk_rtc_spare_reg_fields[SPARE_RG_MAX] = {
	[SPARE_AL_HOU]		= REG_FIELD(RTC_AL_HOU, 8, 15),
	[SPARE_AL_MTH]		= REG_FIELD(RTC_AL_MTH, 8, 15),
	[SPARE_SPAR0]		= REG_FIELD(RTC_SPAR0, 0, 7),
};

static const struct mtk_rtc_compatible mt6359_rtc_compat = {
	.wrtgr_addr		= RTC_WRTGR_MT6358,
	.spare_reg_fields	= mtk_rtc_spare_reg_fields,
	.cali_reg_fields	= mt6359_cali_reg_fields,
	.eosc_cali_version	= EOSC_CALI_MT6359_SERIES,
};

static const struct mtk_rtc_compatible mt6358_rtc_compat = {
	.wrtgr_addr		= RTC_WRTGR_MT6358,
	.spare_reg_fields	= mtk_rtc_spare_reg_fields,
	.cali_reg_fields	= mt6358_cali_reg_fields,
	.eosc_cali_version	= EOSC_CALI_MT6358_SERIES,
};

static const struct mtk_rtc_compatible mt6357_rtc_compat = {
	.wrtgr_addr		= RTC_WRTGR_MT6358,
	.spare_reg_fields	= mtk_rtc_spare_reg_fields,
	.cali_reg_fields	= mt6357_cali_reg_fields,
	.eosc_cali_version	= EOSC_CALI_MT6357_SERIES,
};

static const struct mtk_rtc_compatible mt6397_rtc_compat = {
	.wrtgr_addr = RTC_WRTGR_MT6397,
	.eosc_cali_version	= EOSC_CALI_NONE,
};

static const struct of_device_id mt6397_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6359-rtc",
		.data = (void *)&mt6359_rtc_compat, },
	{ .compatible = "mediatek,mt6358-rtc",
		.data = (void *)&mt6358_rtc_compat, },
	{ .compatible = "mediatek,mt6357-rtc",
		.data = (void *)&mt6357_rtc_compat, },
	{ .compatible = "mediatek,mt6397-rtc",
		.data = (void *)&mt6397_rtc_compat, },
	{}
};
MODULE_DEVICE_TABLE(of, mt6397_rtc_of_match);

static int rtc_eosc_cali_td;
module_param(rtc_eosc_cali_td, int, 0644);

static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc)
{
	unsigned long timeout = jiffies + HZ;
	int ret;
	u32 data;

	ret = regmap_write(rtc->regmap,
			rtc->addr_base + rtc->dev_comp->wrtgr_addr, 1);
	if (ret < 0)
		return ret;

	while (1) {
		ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_BBPU,
				  &data);
		if (ret < 0)
			break;
		if (!(data & RTC_BBPU_CBUSY))
			break;
		if (time_after(jiffies, timeout)) {
			ret = -ETIMEDOUT;
			break;
		}
		cpu_relax();
	}

	return ret;
}

static int rtc_nvram_read(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(priv);
	unsigned int ival;
	int ret;
	u8 *buf = val;

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ret = regmap_field_read(rtc->spare[offset++], &ival);
		if (ret)
			goto out;
		*buf++ = (u8)ival;
	}
out:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int rtc_nvram_write(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(priv);
	unsigned int ival;
	int ret;
	u8 *buf = val;

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ival = *buf++;
		ret = regmap_field_write(rtc->spare[offset++], ival);
		if (ret)
			goto out;
	}
	mtk_rtc_write_trigger(rtc);
out:
	mutex_unlock(&rtc->lock);
	return ret;
}

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	u32 irqsta, irqen;
	int ret;

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if ((ret >= 0) && (irqsta & RTC_IRQ_STA_AL)) {
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
		irqen = irqsta & ~RTC_IRQ_EN_AL;
		mutex_lock(&rtc->lock);
		if (regmap_write(rtc->regmap, rtc->addr_base + RTC_IRQ_EN,
				 irqen) < 0)
			mtk_rtc_write_trigger(rtc);
		mutex_unlock(&rtc->lock);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int __mtk_rtc_read_time(struct mt6397_rtc *rtc,
			       struct rtc_time *tm, int *sec)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC, sec);
	*sec &= RTC_TC_SEC_MASK;
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int days, sec, ret;

	do {
		ret = __mtk_rtc_read_time(rtc, tm, &sec);
		if (ret < 0)
			goto exit;
	} while (sec < tm->tm_sec);

	/* HW register use 7 bits to store year data, minus
	 * RTC_MIN_YEAR_OFFSET before write year data to register, and plus
	 * RTC_MIN_YEAR_OFFSET back after read year from register
	 */
	tm->tm_year += RTC_MIN_YEAR_OFFSET;

	/* HW register start mon from one, but tm_mon start from zero. */
	tm->tm_mon--;
	time = rtc_tm_to_time64(tm);

	/* rtc_tm_to_time64 covert Gregorian date to seconds since
	 * 01-01-1970 00:00:00, and this date is Thursday.
	 */
	days = div_s64(time, 86400);
	tm->tm_wday = (days + 4) % 7;

exit:
	return ret;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	if (tm->tm_year > 195) {
		dev_err(rtc->dev, "%s: invalid year %04d > 2095\n",
					__func__, tm->tm_year + RTC_BASE_YEAR);
		return -EINVAL;
	}

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_write(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	/* Time register write to hardware after call trigger function */
	ret = mtk_rtc_write_trigger(rtc);

exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	u32 irqen, pdn2;
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	mutex_lock(&rtc->lock);
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto err_exit;
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto err_exit;

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto err_exit;

	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);
	mutex_unlock(&rtc->lock);

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_AL_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_AL_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_AL_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_AL_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_AL_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_AL_YEA_MASK;

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	return 0;
err_exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	if (tm->tm_year > 195) {
		dev_err(rtc->dev, "%s: invalid year %04d > 2095\n",
				__func__, tm->tm_year + RTC_BASE_YEAR);
		return -EINVAL;
	}

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	mutex_lock(&rtc->lock);
	if (alm->enabled) {
		ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;
		data[RTC_OFFSET_SEC] =
			((data[RTC_OFFSET_SEC] & ~(RTC_AL_SEC_MASK)) |
					(tm->tm_sec & RTC_AL_SEC_MASK));
		data[RTC_OFFSET_MIN] =
			((data[RTC_OFFSET_MIN] & ~(RTC_AL_MIN_MASK)) |
					(tm->tm_min & RTC_AL_MIN_MASK));
		data[RTC_OFFSET_HOUR] =
			((data[RTC_OFFSET_HOUR] & ~(RTC_AL_HOU_MASK)) |
					(tm->tm_hour & RTC_AL_HOU_MASK));
		data[RTC_OFFSET_DOM] =
			((data[RTC_OFFSET_DOM] & ~(RTC_AL_DOM_MASK)) |
					(tm->tm_mday & RTC_AL_DOM_MASK));
		data[RTC_OFFSET_MTH] =
			((data[RTC_OFFSET_MTH] & ~(RTC_AL_MTH_MASK)) |
					(tm->tm_mon & RTC_AL_MTH_MASK));
		data[RTC_OFFSET_YEAR] =
			((data[RTC_OFFSET_YEAR] & ~(RTC_AL_YEA_MASK)) |
				(tm->tm_year & RTC_AL_YEA_MASK));
		ret = regmap_bulk_write(rtc->regmap,
					rtc->addr_base + RTC_AL_SEC,
					data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;
		ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
		if (ret < 0)
			goto exit;
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL,
					 RTC_IRQ_EN_ONESHOT_AL);
		if (ret < 0)
			goto exit;
	} else {
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL, 0);
		if (ret < 0)
			goto exit;
	}

	/* All alarm time register write to hardware after calling
	 * mtk_rtc_write_trigger. This can avoid race condition if alarm
	 * occur happen during writing alarm time register.
	 */
	ret = mtk_rtc_write_trigger(rtc);
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static const struct rtc_class_ops mtk_rtc_ops = {
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

static int mtk_rtc_reload(struct mt6397_rtc *rtc)
{
	int ret;
	u32 bbpu;

	ret = regmap_read(rtc->regmap, rtc->addr_base +
		RTC_BBPU, &bbpu);
	if (ret == 0) {
		bbpu = bbpu | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
		ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_BBPU, bbpu);
	}
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);

	return ret;
}

static int rtc_xosc_write(struct mt6397_rtc *rtc, u32 reg)
{
	int ret;

	ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_OSC32CON, RTC_OSC32CON_UNLOCK1);
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);
	if (ret == 0)
		ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_OSC32CON, RTC_OSC32CON_UNLOCK2);
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);
	if (ret == 0)
		ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_OSC32CON, reg);
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);

	return ret;
}

static void mtk_rtc_disable_2sec_reboot(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u32 reg;

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC, &reg);
	if (ret == 0) {
		reg = (reg & ~RTC_BBPU_2SEC_EN) & ~RTC_BBPU_AUTO_PDN_SEL;
		ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_AL_SEC, reg);
	}
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);
}

static void mtk_rtc_enable_k_eosc(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u32 td;
	u32 reg;

	if (!rtc->cali_is_supported)
		return;

	/* Truning on eosc cali mode clock */
	regmap_field_write(rtc->cali[RTC_EOSC32_CK_PDN], 0);

	if (rtc_eosc_cali_td) {
		dev_notice(dev, "%s: rtc_eosc_cali_td = %d\n",
				__func__, rtc_eosc_cali_td);
		switch (rtc_eosc_cali_td) {
		case 1:
			td = EOSC_CALI_TD_01_SEC;
			break;
		case 2:
			td = EOSC_CALI_TD_02_SEC;
			break;
		case 4:
			td = EOSC_CALI_TD_04_SEC;
			break;
		case 16:
			td = EOSC_CALI_TD_16_SEC;
			break;
		default:
			td = EOSC_CALI_TD_08_SEC;
			break;
		}
		regmap_field_write(rtc->cali[EOSC_CALI_TD], td);
	}

	if (rtc->dev_comp->eosc_cali_version ==
		EOSC_CALI_MT6357_SERIES) {
		struct reg_field r_field_xo_en32k_man =
			REG_FIELD(MT6357_DCXO_CW02, 0, 0);
		struct regmap_field	*rm_field_xo_en32k_man =
			devm_regmap_field_alloc(dev, rtc->regmap,
			r_field_xo_en32k_man);
		/*RTC mode will have only OFF mode and FPM */
		ret = regmap_field_write(rm_field_xo_en32k_man, 0);
		devm_regmap_field_free(dev, rm_field_xo_en32k_man);

		if (ret == 0)
			ret = mtk_rtc_reload(rtc);
		/* Enable K EOSC mode for normal power
		 * off and then plug out battery
		 */
		if (ret == 0)
			ret = regmap_read(rtc->regmap, rtc->addr_base +
				RTC_AL_YEA, &reg);
		if (ret == 0) {
			reg = ((reg | RTC_K_EOSC_RSV_0) &
				(~RTC_K_EOSC_RSV_1)) | RTC_K_EOSC_RSV_2;
		    ret = regmap_write(rtc->regmap, rtc->addr_base +
				RTC_AL_YEA, reg);
		}
		if (ret == 0)
			ret = mtk_rtc_write_trigger(rtc);

		if (ret == 0)
			ret = regmap_read(rtc->regmap, rtc->addr_base +
				RTC_OSC32CON, &reg);
		if (ret == 0) {
			reg = reg | RTC_EMBCK_SRC_SEL;
			rtc_xosc_write(rtc, reg);
		}
	}
}

static void mtk_rtc_spar_alarm_clear_wait(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u32 reg;
	unsigned long long timeout = sched_clock() + 500000000;

	do {
		ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_BBPU, &reg);
		if (ret == 0 && (reg & RTC_BBPU_CLR) == 0)
			break;
		else if (sched_clock() > timeout) {
			ret = regmap_read(rtc->regmap, rtc->addr_base +
				RTC_BBPU, &reg);
			pr_notice("%s, spar/alarm clear time out, %x,\n",
				__func__, reg);
			break;
		}
	} while (1);
}

static int rtc_lpsd_restore_al_mask(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u32 reg;

	pr_notice("%s\n", __func__);

	ret = mtk_rtc_reload(rtc);
	if (ret == 0)
		ret = regmap_read(rtc->regmap, rtc->addr_base +
			RTC_AL_MASK, &reg);
	if (ret == 0) {
		pr_notice("1st RTC_AL_MASK = 0x%x\n", reg);
		/* mask DOW */
		ret = regmap_write(rtc->regmap, rtc->addr_base +
			RTC_AL_MASK, RTC_AL_MASK_DOW);
	}
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);
	if (ret == 0)
		ret = regmap_read(rtc->regmap, rtc->addr_base +
			RTC_AL_MASK, &reg);
	if (ret == 0)
		pr_notice("2nd RTC_AL_MASK = 0x%x\n", reg);

	return ret;
}

static void mtk_rtc_lpsd(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u32 reg;

	pr_notice("clear lpsd solution\n");
	reg = RTC_BBPU_KEY | RTC_BBPU_CLR | RTC_BBPU_PWREN;
	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_BBPU, reg);
	if (ret == 0)
		ret = mtk_rtc_write_trigger(rtc);

	mtk_rtc_spar_alarm_clear_wait(dev);

	ret = mtk_rtc_reload(rtc);
	if (ret == 0)
		ret = regmap_read(rtc->regmap, rtc->addr_base +
			RTC_AL_MASK, &reg);
	if (ret == 0) {
		pr_notice("RTC_AL_MASK = 0x%x\n", reg);
		ret = regmap_read(rtc->regmap, rtc->addr_base +
			RTC_IRQ_EN, &reg);
	}
	if (ret == 0)
		pr_notice("RTC_IRQ_EN = 0x%x\n", reg);
}

static void mtk_rtc_shutdown(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(&pdev->dev);

	if (rtc->dev_comp->eosc_cali_version == EOSC_CALI_MT6357_SERIES)
		mtk_rtc_disable_2sec_reboot(&pdev->dev);
	mtk_rtc_enable_k_eosc(&pdev->dev);
	if (rtc->dev_comp->eosc_cali_version == EOSC_CALI_MT6357_SERIES ||
		rtc->dev_comp->eosc_cali_version == EOSC_CALI_MT6358_SERIES)
		mtk_rtc_lpsd(&pdev->dev);
}

static int mtk_rtc_config_eosc_cali(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < CALI_FILED_MAX; i++) {
		rtc->cali[i] = devm_regmap_field_alloc(dev, rtc->regmap,
					rtc->dev_comp->cali_reg_fields[i]);
		if (IS_ERR(rtc->cali[i])) {
			dev_err(rtc->dev, "cali regmap field[%d] err= %ld\n",
						i, PTR_ERR(rtc->cali[i]));
			return PTR_ERR(rtc->cali[i]);
		}
	}
	rtc->cali_is_supported = true;

	return 0;
}

static int mtk_rtc_set_spare(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	struct reg_field tmp[SPARE_RG_MAX];
	int i, ret;
	struct nvmem_config nvmem_cfg = {
		.name = "mtk_rtc_nvmem",
		.word_size = SPARE_REG_WIDTH,
		.stride = 1,
		.size = SPARE_RG_MAX * SPARE_REG_WIDTH,
		.reg_read = rtc_nvram_read,
		.reg_write = rtc_nvram_write,
		.priv = dev,
	};

	memcpy(tmp, rtc->dev_comp->spare_reg_fields, sizeof(tmp));

	for (i = 0; i < SPARE_RG_MAX; i++) {
		tmp[i].reg += rtc->addr_base;
		rtc->spare[i] = devm_regmap_field_alloc(rtc->dev,
							rtc->regmap,
							tmp[i]);
		if (IS_ERR(rtc->spare[i])) {
			dev_err(rtc->dev, "spare regmap field[%d] err= %ld\n",
						i, PTR_ERR(rtc->spare[i]));
			return PTR_ERR(rtc->spare[i]);
		}
	}

	ret = rtc_nvmem_register(rtc->rtc_dev, &nvmem_cfg);
	if (ret)
		dev_err(rtc->dev, "nvmem register failed\n");

	return ret;
}

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	const struct of_device_id *of_id;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->addr_base = res->start;

	of_id = of_match_device(mt6397_rtc_of_match, &pdev->dev);
	if (!of_id) {
		dev_err(&pdev->dev, "Failed to probe of_node\n");
		return -EINVAL;
	}
	rtc->dev_comp = of_id->data;

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->regmap = mt6397_chip->regmap;
	rtc->dev = &pdev->dev;
	mutex_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(rtc->dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

	ret = request_threaded_irq(rtc->irq, NULL,
				   mtk_rtc_irq_handler_thread,
				   IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				   "mt6397-rtc", rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		goto out_dispose_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev->ops = &mtk_rtc_ops;

	ret = rtc_register_device(rtc->rtc_dev);
	if (ret) {
		dev_err(&pdev->dev, "register rtc device failed\n");
		goto out_free_irq;
	}

	if (rtc->dev_comp->spare_reg_fields)
		if (mtk_rtc_set_spare(&pdev->dev))
			dev_err(&pdev->dev, "spare is not supported\n");

	if (rtc->dev_comp->cali_reg_fields)
		if (mtk_rtc_config_eosc_cali(&pdev->dev))
			dev_err(&pdev->dev, "config eosc cali failed\n");

	if (rtc->dev_comp->eosc_cali_version == EOSC_CALI_MT6357_SERIES ||
		rtc->dev_comp->eosc_cali_version == EOSC_CALI_MT6358_SERIES)
		rtc_lpsd_restore_al_mask(&pdev->dev);

	return 0;

out_free_irq:
	free_irq(rtc->irq, rtc->rtc_dev);
out_dispose_irq:
	irq_dispose_mapping(rtc->irq);
	return ret;
}

static int mtk_rtc_remove(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = platform_get_drvdata(pdev);

	free_irq(rtc->irq, rtc->rtc_dev);
	irq_dispose_mapping(rtc->irq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt6397_rtc_suspend(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rtc->irq);

	return 0;
}

static int mt6397_rtc_resume(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rtc->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt6397_pm_ops, mt6397_rtc_suspend,
			mt6397_rtc_resume);

static struct platform_driver mtk_rtc_driver = {
	.driver = {
		.name = "mt6397-rtc",
		.of_match_table = mt6397_rtc_of_match,
		.pm = &mt6397_pm_ops,
	},
	.probe	= mtk_rtc_probe,
	.remove = mtk_rtc_remove,
	.shutdown = mtk_rtc_shutdown,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");
