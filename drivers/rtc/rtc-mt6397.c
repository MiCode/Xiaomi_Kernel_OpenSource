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
#include <linux/debugfs.h>
/* For KPOC alarm */
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <linux/reboot.h>

#define RTC_BBPU		0x0000
#define RTC_BBPU_KEY		0x4300
#define RTC_BBPU_PWREN		BIT(0)
#define RTC_BBPU_CLR		BIT(1)
#define RTC_BBPU_RESET_AL	BIT(3)
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

#define RTC_POWERKEY1	0x0028
#define RTC_POWERKEY2	0x002a

#define RTC_PDN1		0x002c
#define RTC_PDN1_PWRON_TIME     BIT(7)

#define RTC_PDN2		0x002e
#define RTC_PDN2_PWRON_ALARM	BIT(4)
#define RTC_PDN2_PWRON_LOGO     BIT(15)

#define RTC_SPAR0		0x0030
#define RTC_SPAR1		0x0032

#define RTC_PROT		0x0034
#define RTC_CON			0x003c

#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_NUM_YEARS		128
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)

#define SPARE_REG_WIDTH		1

#define RTC_PWRON_YEA        RTC_PDN2
#define RTC_PWRON_YEA_MASK     0x7f00
#define RTC_PWRON_YEA_SHIFT     8

#define RTC_PWRON_MTH        RTC_PDN2
#define RTC_PWRON_MTH_MASK     0x000f
#define RTC_PWRON_MTH_SHIFT     0

#define RTC_PWRON_SEC        RTC_SPAR0
#define RTC_PWRON_SEC_MASK     0x003f
#define RTC_PWRON_SEC_SHIFT     0

#define RTC_PWRON_MIN        RTC_SPAR1
#define RTC_PWRON_MIN_MASK     0x003f
#define RTC_PWRON_MIN_SHIFT     0

#define RTC_PWRON_HOU        RTC_SPAR1
#define RTC_PWRON_HOU_MASK     0x07c0
#define RTC_PWRON_HOU_SHIFT     6

#define RTC_PWRON_DOM        RTC_SPAR1
#define RTC_PWRON_DOM_MASK     0xf800
#define RTC_PWRON_DOM_SHIFT     11

enum mtk_rtc_spare_enum {
	SPARE_AL_HOU,
	SPARE_AL_MTH,
	SPARE_SPAR0,
	SPARE_KPOC,
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

enum boot_mode_t {
	NORMAL_BOOT = 0,
	META_BOOT = 1,
	RECOVERY_BOOT = 2,
	SW_REBOOT = 3,
	FACTORY_BOOT = 4,
	ADVMETA_BOOT = 5,
	ATE_FACTORY_BOOT = 6,
	ALARM_BOOT = 7,
	KERNEL_POWER_OFF_CHARGING_BOOT = 8,
	LOW_POWER_OFF_CHARGING_BOOT = 9,
	DONGLE_BOOT = 10,
	UNKNOWN_BOOT
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

struct mtk_rtc_compatible {
	u32			wrtgr_addr;
	const struct reg_field *spare_reg_fields;
	const struct reg_field *cali_reg_fields;
	u32			eosc_cali_version;
};

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_PWRON_SEC, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_PWRON_MIN, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_PWRON_HOU, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_PWRON_DOM, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PWRON_MTH, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PWRON_YEA, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};

struct mt6397_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	struct regmap		*regmap;
	int			irq;
	u32			addr_base;
	struct work_struct work;
	struct completion comp;
	const struct mtk_rtc_compatible *dev_comp;
	struct regmap_field	*spare[SPARE_RG_MAX];
	struct regmap_field	*cali[CALI_FILED_MAX];
	bool			cali_is_supported;
#ifdef CONFIG_PM
	struct notifier_block pm_nb;
#endif
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
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
	[SPARE_KPOC]		= REG_FIELD(RTC_PDN1, 14, 14),
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

static int rtc_show_time;
static int rtc_show_alarm = 1;
static int alarm1m15s;
static u32 bootmode;
static struct wakeup_source *mt6397_rtc_suspend_lock;
/*for KPOC alarm*/
static bool rtc_pm_notifier_registered;
static bool kpoc_alarm;
static unsigned long rtc_pm_status;

module_param(rtc_show_time, int, 0644);
module_param(rtc_show_alarm, int, 0644);

static int rtc_alarm_enabled = 1;

static ssize_t mtk_rtc_debug_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	char lbuf[128];
	char option[16];
	int setting;
	ssize_t res;
	struct mt6397_rtc *rtc = file->private_data;

	if (*ppos != 0 || size >= sizeof(lbuf) || size == 0)
		return -EINVAL;

	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, buf, size);
	if (res <= 0)
		return -EFAULT;
	lbuf[size] = '\0';

	if (sscanf(lbuf, "%15s %d", option, &setting) != 2) {
		pr_notice("Invalid para %s\n", lbuf);
		return -EFAULT;
	}

	if (!strncmp(option, "alarm", strlen("alarm"))) {
		pr_notice("alarm = %d\n", setting);
		rtc_alarm_enabled = setting;
		if (rtc_alarm_enabled)
			enable_irq(rtc->irq);
		else
			disable_irq_nosync(rtc->irq);
	}

	return size;
}

static int mtk_rtc_debug_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "rtc alarm %s\n",
		rtc_alarm_enabled ? "enabled" : "disabled");
	return 0;
}

static int mtk_rtc_debug_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mtk_rtc_debug_show, NULL);
}

static const struct file_operations mtk_rtc_debug_ops = {
	.open    = mtk_rtc_debug_open,
	.read    = seq_read,
	.write   = mtk_rtc_debug_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

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

#ifdef CONFIG_PM

#define PM_DUMMY 0xFFFF

static int rtc_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct mt6397_rtc *rtc = container_of(notifier,
		struct mt6397_rtc, pm_nb);

	pr_notice("%s = %lu\n", __func__, pm_event);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		rtc_pm_status = PM_SUSPEND_PREPARE;
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		rtc_pm_status = PM_POST_SUSPEND;
		break;
	default:
		rtc_pm_status = PM_DUMMY;
		break;
	}

	if (kpoc_alarm) {
		pr_notice("%s trigger reboot\n", __func__);
		complete(&rtc->comp);
		kpoc_alarm = false;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

static void rtc_mark_kpoc(struct mt6397_rtc *rtc)
{
	pr_notice("%s\n", __func__);

	mutex_lock(&rtc->lock);
	regmap_field_write(rtc->spare[SPARE_KPOC], 1);
	mutex_unlock(&rtc->lock);
}

static void mtk_rtc_work_queue(struct work_struct *work)
{
	struct mt6397_rtc *rtc = container_of(work, struct mt6397_rtc, work);
	unsigned long ret;
	unsigned int msecs;

	ret = wait_for_completion_timeout(&rtc->comp, msecs_to_jiffies(30000));
	if (!ret) {
		pr_notice("%s timeout\n", __func__);
		BUG_ON(1);
	} else {
		msecs = jiffies_to_msecs(ret);
		pr_notice("%s timeleft= %d\n", __func__, msecs);
		rtc_mark_kpoc(rtc);
		kernel_restart("kpoc");
	}
}

static void mtk_rtc_reboot(struct mt6397_rtc *rtc)
{
	__pm_stay_awake(mt6397_rtc_suspend_lock);

	init_completion(&rtc->comp);
	schedule_work_on(cpumask_first(cpu_online_mask), &rtc->work);

	if (!rtc_pm_notifier_registered)
		goto reboot;

	if (rtc_pm_status != PM_SUSPEND_PREPARE)
		goto reboot;

	kpoc_alarm = true;

	pr_notice("%s:wait\n", __func__);
	return;

reboot:
	pr_notice("%s:trigger\n", __func__);
	complete(&rtc->comp);
}

#ifndef USER_BUILD_KERNEL
void mtk_rtc_lp_exception(struct mt6397_rtc *rtc)
{
	u32 bbpu, irqsta, irqen, osc32;
	u32 pwrkey1, pwrkey2, prot, con, sec1, sec2;

	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_BBPU, &bbpu);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_IRQ_STA, &irqsta);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_IRQ_EN, &irqen);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_OSC32CON, &osc32);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_POWERKEY1, &pwrkey1);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_POWERKEY2, &pwrkey2);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_PROT, &prot);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_CON, &con);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_TC_SEC, &sec1);
	mdelay(2000);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_TC_SEC, &sec2);

	dev_emerg(rtc->dev, "!!! 32K WAS STOPPED !!!\n"
		"RTC_BBPU      = 0x%x\n"
		"RTC_IRQ_STA   = 0x%x\n"
		"RTC_IRQ_EN    = 0x%x\n"
		"RTC_OSC32CON  = 0x%x\n"
		"RTC_POWERKEY1 = 0x%x\n"
		"RTC_POWERKEY2 = 0x%x\n"
		"RTC_PROT      = 0x%x\n"
		"RTC_CON       = 0x%x\n"
		"RTC_TC_SEC    = %02d\n"
		"RTC_TC_SEC    = %02d\n",
		bbpu, irqsta, irqen, osc32, pwrkey1, pwrkey2, prot, con, sec1,
		sec2);
}
#endif

static bool mtk_rtc_is_alarm_irq(struct mt6397_rtc *rtc)
{
	u32 irqsta, bbpu;
	int ret;

	/* read clear */
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_AL)) {
		bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN;
		ret = regmap_write(rtc->regmap,
					rtc->addr_base + RTC_BBPU, bbpu);
		if (ret < 0)
			dev_err(rtc->dev, "%s error\n", __func__);
		mtk_rtc_write_trigger(rtc);

		return true;
	}
#ifndef USER_BUILD_KERNEL
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_LP))
		mtk_rtc_lp_exception(rtc);
#endif

	return false;
}

static void mtk_rtc_update_pwron_alarm_flag(struct mt6397_rtc *rtc)
{
	int ret;

	dev_notice(rtc->dev, "%s\n", __func__);

	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, 0);
	if (ret < 0)
		goto exit;

	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN2,
				RTC_PDN2_PWRON_ALARM, RTC_PDN2_PWRON_ALARM);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	return;
exit:
	dev_err(rtc->dev, "%s error\n", __func__);
}

static void mtk_rtc_reset_bbpu_alarm_status(struct mt6397_rtc *rtc)
{
	u32 bbpu;
	int ret;

	bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN | RTC_BBPU_RESET_AL;
	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	return;
exit:
	dev_err(rtc->dev, "%s error\n", __func__);

}

static int mtk_rtc_restore_alarm(struct mt6397_rtc *rtc, struct rtc_time *tm)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

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

	dev_notice(rtc->dev,
		"restore al time = %04d/%02d/%02d %02d:%02d:%02d\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	ret = regmap_bulk_write(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
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
	mtk_rtc_write_trigger(rtc);

	return ret;

exit:
	dev_err(rtc->dev, "%s error\n", __func__);
	return ret;
}

bool mtk_rtc_is_pwron_alarm(struct mt6397_rtc *rtc,
	struct rtc_time *nowtm, struct rtc_time *tm)
{
	u32 pdn1, spar1, pdn2, spar0;
	int ret, sec;
	u16 data[RTC_OFFSET_COUNT];

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	dev_notice(rtc->dev, "pdn1 = 0x%x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {	/* power-on time is available */

		/*get current rtc time*/
		do {
			ret = regmap_bulk_read(rtc->regmap,
						rtc->addr_base + RTC_TC_SEC,
						data, RTC_OFFSET_COUNT);
			if (ret < 0)
				goto exit;
			nowtm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
			nowtm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
			nowtm->tm_hour =
				data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
			nowtm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
			nowtm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
			nowtm->tm_year =
				data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

			ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_TC_SEC, &sec);
			if (ret < 0)
				goto exit;
			sec &= RTC_TC_SEC_MASK;

		} while (sec < nowtm->tm_sec);

		dev_notice(rtc->dev,
			"get now time = %04d/%02d/%02d %02d:%02d:%02d\n",
			nowtm->tm_year + RTC_MIN_YEAR, nowtm->tm_mon,
			nowtm->tm_mday, nowtm->tm_hour,
			nowtm->tm_min, nowtm->tm_sec);

		/*get power on time from SPARE */
		ret = regmap_read(rtc->regmap,
				rtc->addr_base + RTC_SPAR0, &spar0);
		if (ret < 0)
			goto exit;

		ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_SPAR1, &spar1);
		if (ret < 0)
			goto exit;

		ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_PDN2, &pdn2);
		if (ret < 0)
			goto exit;
		dev_notice(rtc->dev,
			"spar0=0x%x, spar1=0x%x, pdn2=0x%x\n",
			spar0, spar1, pdn2);

		tm->tm_sec =
			(spar0 & RTC_PWRON_SEC_MASK) >> RTC_PWRON_SEC_SHIFT;
		tm->tm_min =
			(spar1 & RTC_PWRON_MIN_MASK) >> RTC_PWRON_MIN_SHIFT;
		tm->tm_hour =
			(spar1 & RTC_PWRON_HOU_MASK) >> RTC_PWRON_HOU_SHIFT;
		tm->tm_mday =
			(spar1 & RTC_PWRON_DOM_MASK) >> RTC_PWRON_DOM_SHIFT;
		tm->tm_mon =
			(pdn2 & RTC_PWRON_MTH_MASK) >> RTC_PWRON_MTH_SHIFT;
		tm->tm_year =
			(pdn2 & RTC_PWRON_YEA_MASK) >> RTC_PWRON_YEA_SHIFT;

		dev_notice(rtc->dev,
		"get pwron time = %04d/%02d/%02d %02d:%02d:%02d\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

		return true;
	}
	return false;
exit:
	dev_err(rtc->dev, "%s error\n", __func__);
	return false;
}

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	bool pwron_alm = false, isAlarmIrq = false, pwron_alarm = false;
	struct rtc_time nowtm, tm;

	dev_notice(rtc->dev, "%s\n", __func__);

	mutex_lock(&rtc->lock);
	isAlarmIrq = mtk_rtc_is_alarm_irq(rtc);
	if (!isAlarmIrq) {
		mutex_unlock(&rtc->lock);
		return IRQ_HANDLED;
	}

	mtk_rtc_reset_bbpu_alarm_status(rtc);

	pwron_alarm = mtk_rtc_is_pwron_alarm(rtc, &nowtm, &tm);
	nowtm.tm_year += RTC_MIN_YEAR;
	tm.tm_year += RTC_MIN_YEAR;
	if (pwron_alarm) {
		time64_t now_time, time;

		now_time =
		    mktime(nowtm.tm_year, nowtm.tm_mon, nowtm.tm_mday,
			   nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);
		time =
		    mktime(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
			   tm.tm_min, tm.tm_sec);

		/* power on */
		if (now_time >= time - 1 && now_time <= time + 4) {
			if (bootmode == KERNEL_POWER_OFF_CHARGING_BOOT ||
				bootmode == LOW_POWER_OFF_CHARGING_BOOT) {
				mtk_rtc_reboot(rtc);
				mutex_unlock(&rtc->lock);
				disable_irq_nosync(rtc->irq);
				goto out;
			} else {
				mtk_rtc_update_pwron_alarm_flag(rtc);
				pwron_alm = true;
			}
		} else if (now_time < time) {	/* set power-on alarm */
			time -= 1;
			rtc_time64_to_tm(time, &tm);
			tm.tm_year -= RTC_MIN_YEAR_OFFSET;
			tm.tm_mon += 1;
			mtk_rtc_restore_alarm(rtc, &tm);
		}
	}
	mutex_unlock(&rtc->lock);
out:
	if (rtc->rtc_dev != NULL)
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);

	if (rtc_show_alarm)
		dev_notice(rtc->dev, "%s time is up\n",
					pwron_alm ? "power-on" : "alarm");

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

	dev_notice(rtc->dev, "%s\n", __func__);

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
	bool enable, struct rtc_time *tm, bool logo)
{
	u32 pdn1, pdn2;
	int ret;

	dev_notice(rtc->dev, "%s\n", __func__);
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

	if (logo)
		pdn2 = RTC_PDN2_PWRON_LOGO;
	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN2,
				RTC_PDN2_PWRON_LOGO, pdn2);
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

	if (rtc_show_time) {
		dev_notice(rtc->dev,
		"read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
		tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1,
		tm->tm_mday, tm->tm_wday, tm->tm_hour,
		tm->tm_min, tm->tm_sec);
	}

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

	dev_notice(rtc->dev, "set tc time = %04d/%02d/%02d %02d:%02d:%02d\n",
		  tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

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

	dev_notice(rtc->dev,
		"read al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

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
	u32 irqsta;
	ktime_t target;

	if (tm->tm_year > 195) {
		dev_err(rtc->dev, "%s: invalid year %04d > 2095\n",
				__func__, tm->tm_year + RTC_BASE_YEAR);
		return -EINVAL;
	}

	if (alm->enabled == 1) {
		/* Add one more second to postpone wake time. */
		target = rtc_tm_to_ktime(*tm);
		target = ktime_add_ns(target, NSEC_PER_SEC);
		*tm = rtc_ktime_to_tm(target);
	} else if (alm->enabled == 5) {
		/* Power on system 1 minute earlier */
		alarm1m15s = 1;
	}

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	dev_notice(rtc->dev,
		"set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	mutex_lock(&rtc->lock);

	switch (alm->enabled) {
	case 2:
		/* enable power-on alarm */
		mtk_rtc_save_pwron_time(rtc, true, tm, false);
		break;
	case 3:
	case 5:
		/* enable power-on alarm with logo */
		mtk_rtc_save_pwron_time(rtc, true, tm, true);
		break;
	case 4:
		/* disable power-on alarm */
		mtk_rtc_save_pwron_time(rtc, false, tm, false);
		alarm1m15s = 0;
		break;
	default:
		break;
	}

	ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + RTC_IRQ_EN, RTC_IRQ_EN_AL, 0);
	if (ret < 0)
		goto exit;
	ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + RTC_PDN2, RTC_PDN2_PWRON_ALARM, 0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if (ret < 0)
		goto exit;

	if (alm->enabled) {
		ret = mtk_rtc_restore_alarm(rtc, tm);
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
	struct rtc_time rtc_time_now;
	struct rtc_time rtc_time_alarm;
	ktime_t ktime_now;
	ktime_t ktime_alarm;
	bool is_pwron_alarm;

	if (alarm1m15s == 1) {
		is_pwron_alarm = mtk_rtc_is_pwron_alarm(rtc,
			&rtc_time_now, &rtc_time_alarm);
		if (is_pwron_alarm) {
			rtc_time_now.tm_year += RTC_MIN_YEAR_OFFSET;
			rtc_time_now.tm_mon--;
			rtc_time_alarm.tm_year += RTC_MIN_YEAR_OFFSET;
			rtc_time_alarm.tm_mon--;
			pr_notice("now = %04d/%02d/%02d %02d:%02d:%02d\n",
				rtc_time_now.tm_year + 1900,
				rtc_time_now.tm_mon + 1,
				rtc_time_now.tm_mday,
				rtc_time_now.tm_hour,
				rtc_time_now.tm_min,
				rtc_time_now.tm_sec);
			pr_notice("alarm = %04d/%02d/%02d %02d:%02d:%02d\n",
				rtc_time_alarm.tm_year + 1900,
				rtc_time_alarm.tm_mon + 1,
				rtc_time_alarm.tm_mday,
				rtc_time_alarm.tm_hour,
				rtc_time_alarm.tm_min,
				rtc_time_alarm.tm_sec);
			ktime_now = rtc_tm_to_ktime(rtc_time_now);
			ktime_alarm = rtc_tm_to_ktime(rtc_time_alarm);
			if (ktime_after(ktime_alarm, ktime_now)) {
				/* alarm has not happened */
				ktime_alarm = ktime_sub_ms(ktime_alarm,
					MSEC_PER_SEC * 60);
				if (ktime_after(ktime_alarm, ktime_now))
					pr_notice("Alarm will happen after 1 minute\n");
				else {
					ktime_alarm = ktime_add_ms(ktime_now,
						MSEC_PER_SEC * 15);
					pr_notice("Alarm will happen in 15 seconds\n");
				}
				rtc_time_alarm = rtc_ktime_to_tm(ktime_alarm);
				pr_notice("new alarm = %04d/%02d/%02d %02d:%02d:%02d\n",
					rtc_time_alarm.tm_year + 1900,
					rtc_time_alarm.tm_mon + 1,
					rtc_time_alarm.tm_mday,
					rtc_time_alarm.tm_hour,
					rtc_time_alarm.tm_min,
					rtc_time_alarm.tm_sec);
				rtc_time_alarm.tm_year -= RTC_MIN_YEAR_OFFSET;
				rtc_time_alarm.tm_mon++;
				mtk_rtc_set_pwron_time(rtc, &rtc_time_alarm);
				mtk_rtc_restore_alarm(rtc, &rtc_time_alarm);
			} else
				pr_notice("Alarm has happened before\n");
		} else
			pr_notice("No power-off alarm is set\n");
	}

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

static void mtk_rtc_set_lp_irq(struct mt6397_rtc *rtc)
{
	u32 irqen;

#ifndef USER_BUILD_KERNEL
	irqen = RTC_IRQ_EN_LP;
#endif

	mutex_lock(&rtc->lock);

	regmap_update_bits(rtc->regmap,
			rtc->addr_base + RTC_IRQ_EN,
			RTC_IRQ_EN_LP, irqen);
	mtk_rtc_write_trigger(rtc);

	mutex_unlock(&rtc->lock);
}

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	const struct of_device_id *of_id;
	int ret;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	struct dentry *mtk_rtc_dir;
	struct dentry *mtk_rtc_file;

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
		return ret;
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

	mtk_rtc_set_lp_irq(rtc);

	boot_node = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!boot_node) {
		dev_err(&pdev->dev,
			"%s: failed to get boot mode phandle\n", __func__);
	} else {
		tag = (struct tag_bootmode *)of_get_property(
			boot_node, "atag,boot", NULL);
		if (!tag)
			dev_err(&pdev->dev,
				"%s: failed to get atag,boot\n", __func__);
		else {
			dev_notice(&pdev->dev,
				"%s, bootmode:%d\n", __func__, tag->bootmode);
			bootmode = tag->bootmode;
		}
	}

	mtk_rtc_dir = debugfs_create_dir("mtk_rtc", NULL);
	if (!mtk_rtc_dir) {
		dev_err(&pdev->dev,
			"create /sys/kernel/debug/mtk_rtc_dir failed\n");
	} else {
		mtk_rtc_file = debugfs_create_file("mtk_rtc", 0644,
			mtk_rtc_dir, rtc,
			&mtk_rtc_debug_ops);
		if (!mtk_rtc_file) {
			dev_err(&pdev->dev,
				"create /sys/kernel/debug/mtk_rtc/mtk_rtc failed\n");
		}
	}

#ifdef CONFIG_PM
	rtc->pm_nb.notifier_call = rtc_pm_event;
	rtc->pm_nb.priority = 0;
	if (register_pm_notifier(&rtc->pm_nb))
		pr_notice("rtc pm failed\n");
	else
		rtc_pm_notifier_registered = true;
#endif /* CONFIG_PM */

	INIT_WORK(&rtc->work, mtk_rtc_work_queue);

	return 0;

out_free_irq:
	free_irq(rtc->irq, rtc);
	return ret;
}

static int mtk_rtc_remove(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = platform_get_drvdata(pdev);

	free_irq(rtc->irq, rtc);

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
