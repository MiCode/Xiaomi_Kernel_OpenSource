// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) 2014-2015 MediaTek Inc.
* Author: Tianping.Fang <tianping.fang@mediatek.com>
*/

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/mod_devicetable.h>
#include <linux/nvmem-provider.h>

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_PWRON_SEC, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_PWRON_MIN, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_PWRON_HOU, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_PWRON_DOM, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PWRON_MTH, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PWRON_YEA, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};

static const struct reg_field mtk_rtc_spare_reg_fields[SPARE_RG_MAX] = {
	[SPARE_AL_HOU] = REG_FIELD(RTC_AL_HOU, 8, 15),
	[SPARE_AL_MTH] = REG_FIELD(RTC_AL_MTH, 8, 15),
	[SPARE_SPAR0] = REG_FIELD(RTC_SPAR0, 0, 7),
};

static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc)
{
	int ret;
	u32 data;

	ret = regmap_write(rtc->regmap, rtc->addr_base + rtc->data->wrtgr, 1);
	if (ret < 0)
		return ret;

	ret = regmap_read_poll_timeout(rtc->regmap,
					rtc->addr_base + RTC_BBPU, data,
					!(data & RTC_BBPU_CBUSY),
					MTK_RTC_POLL_DELAY_US,
					MTK_RTC_POLL_TIMEOUT);
	if (ret < 0)
		dev_err(rtc->dev, "failed to write WRTGE: %d\n", ret);

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
				 irqen) == 0)
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

static void mtk_rtc_set_pwron_time(struct mt6397_rtc *rtc, struct rtc_time *tm)
{
	u32 data[RTC_OFFSET_COUNT];
	int ret, i;

	data[RTC_OFFSET_SEC] =
		((tm->tm_sec << RTC_PWRON_SEC_SHIFT) & RTC_PWRON_SEC_MASK);
	data[RTC_OFFSET_MIN] =
		((tm->tm_min << RTC_PWRON_MIN_SHIFT) & RTC_PWRON_MIN_MASK);
	data[RTC_OFFSET_HOUR] =
		((tm->tm_hour << RTC_PWRON_HOU_SHIFT) & RTC_PWRON_HOU_MASK);
	data[RTC_OFFSET_DOM] =
		((tm->tm_mday << RTC_PWRON_DOM_SHIFT) & RTC_PWRON_DOM_MASK);
	data[RTC_OFFSET_MTH] =
		((tm->tm_mon << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK);
	data[RTC_OFFSET_YEAR] =
		((tm->tm_year << RTC_PWRON_YEA_SHIFT) & RTC_PWRON_YEA_MASK);

	for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
		if (i == RTC_OFFSET_DOW)
			continue;
		ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + rtc_pwron_reg[i][RTC_REG],
			rtc_pwron_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
		mtk_rtc_write_trigger(rtc);
	}
	return;
exit:
	dev_err(rtc->dev, "%s error\n", __func__);
}

void mtk_rtc_save_pwron_time(struct mt6397_rtc *rtc,
	bool enable, struct rtc_time *tm)
{
	u32 pdn1 = 0;
	int ret;

	/* set power on time */
	mtk_rtc_set_pwron_time(rtc, tm);

	/* update power on alarm related flags */
	if (enable)
		pdn1 = RTC_PDN1_PWRON_TIME;
	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, pdn1);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);

	return;

exit:
	dev_err(rtc->dev, "%s error\n", __func__);
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
	ktime_t target;

	if (alm->enabled == 1) {
		/* Add one more second to postpone wake time. */
		target = rtc_tm_to_ktime(*tm);
		target = ktime_add_ns(target, NSEC_PER_SEC);
		*tm = rtc_ktime_to_tm(target);
	}

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	mutex_lock(&rtc->lock);

	switch (alm->enabled) {
	case 3:
		/* enable power-on alarm with logo */
		mtk_rtc_save_pwron_time(rtc, true, tm);
		break;
	case 4:
		/* disable power-on alarm */
		mtk_rtc_save_pwron_time(rtc, false, tm);
		break;
	default:
		break;
	}

	ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + RTC_PDN2, RTC_PDN2_PWRON_ALARM, 0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	data[RTC_OFFSET_SEC] = ((data[RTC_OFFSET_SEC] & ~(RTC_AL_SEC_MASK)) |
				(tm->tm_sec & RTC_AL_SEC_MASK));
	data[RTC_OFFSET_MIN] = ((data[RTC_OFFSET_MIN] & ~(RTC_AL_MIN_MASK)) |
				(tm->tm_min & RTC_AL_MIN_MASK));
	data[RTC_OFFSET_HOUR] = ((data[RTC_OFFSET_HOUR] & ~(RTC_AL_HOU_MASK)) |
				(tm->tm_hour & RTC_AL_HOU_MASK));
	data[RTC_OFFSET_DOM] = ((data[RTC_OFFSET_DOM] & ~(RTC_AL_DOM_MASK)) |
				(tm->tm_mday & RTC_AL_DOM_MASK));
	data[RTC_OFFSET_MTH] = ((data[RTC_OFFSET_MTH] & ~(RTC_AL_MTH_MASK)) |
				(tm->tm_mon & RTC_AL_MTH_MASK));
	data[RTC_OFFSET_YEAR] = ((data[RTC_OFFSET_YEAR] & ~(RTC_AL_YEA_MASK)) |
				(tm->tm_year & RTC_AL_YEA_MASK));

	if (alm->enabled) {
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

int alarm_set_power_on(struct device *dev, struct rtc_wkalrm *alm)
{
	int err = 0;
	struct rtc_time tm;
	time64_t now, scheduled;

	err = rtc_valid_tm(&alm->time);
	if (err != 0)
		return err;
	scheduled = rtc_tm_to_time64(&alm->time);

	err = mtk_rtc_read_time(dev, &tm);
	if (err != 0)
		return err;
	now = rtc_tm_to_time64(&tm);

	if (scheduled <= now)
		alm->enabled = 4;
	else
		alm->enabled = 3;

	mtk_rtc_set_alarm(dev, alm);

	return err;
}

static int mtk_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *) arg;
	int err = 0;
	struct rtc_wkalrm alm;

	switch (cmd) {
	case RTC_POFF_ALM_SET:
		if (copy_from_user(&alm.time, uarg, sizeof(alm.time)))
			return -EFAULT;
		err = alarm_set_power_on(dev, &alm);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct rtc_class_ops mtk_rtc_ops = {
	.ioctl      = mtk_rtc_ioctl,
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

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

	memcpy(tmp, rtc->data->spare_reg_fields, sizeof(tmp));

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
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->dev = &pdev->dev;
	rtc->addr_base = res->start;

	rtc->data = of_device_get_match_data(&pdev->dev);

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->regmap = mt6397_chip->regmap;
	mutex_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL,
					mtk_rtc_irq_handler_thread,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"mt6397-rtc", rtc);

	if (ret) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		return ret;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev->ops = &mtk_rtc_ops;

	if (rtc->data->spare_reg_fields)
		if (mtk_rtc_set_spare(&pdev->dev))
			dev_err(&pdev->dev, "spare is not supported\n");

	return rtc_register_device(rtc->rtc_dev);
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

static const struct mtk_rtc_data mt6358_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6358,
	.spare_reg_fields = mtk_rtc_spare_reg_fields,
};

static const struct mtk_rtc_data mt6397_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6397,
};

static const struct of_device_id mt6397_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6323-rtc", .data = &mt6397_rtc_data },
	{ .compatible = "mediatek,mt6358-rtc", .data = &mt6358_rtc_data },
	{ .compatible = "mediatek,mt6359p-rtc", .data = &mt6358_rtc_data },
	{ .compatible = "mediatek,mt6397-rtc", .data = &mt6397_rtc_data },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_rtc_of_match);

static struct platform_driver mtk_rtc_driver = {
	.driver = {
		.name = "mt6397-rtc",
		.of_match_table = mt6397_rtc_of_match,
		.pm = &mt6397_pm_ops,
	},
	.probe	= mtk_rtc_probe,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");
