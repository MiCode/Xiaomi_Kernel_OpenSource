/*
 * Spreadtrum watchdog driver
 * Copyright (C) 2017 Spreadtrum - http://www.spreadtrum.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/alarmtimer.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/seq_buf.h>
#include <linux/syscore_ops.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>

#define SPRD_WDT_FIQ_LOAD_LOW		0x0
#define SPRD_WDT_FIQ_LOAD_HIGH		0x4
#define SPRD_WDT_FIQ_CTRL			0x8
#define SPRD_WDT_FIQ_INT_CLR		0xc
#define SPRD_WDT_FIQ_INT_RAW		0x10
#define SPRD_WDT_FIQ_INT_MSK		0x14
#define SPRD_WDT_FIQ_CNT_LOW		0x18
#define SPRD_WDT_FIQ_CNT_HIGH		0x1c
#define SPRD_WDT_FIQ_LOCK			0x20
#define SPRD_WDT_FIQ_CNT_READ_LOW		0x24
#define SPRD_WDT_FIQ_CNT_READ_HIGH		0x28
#define SPRD_WDT_FIQ_IRQ_LOAD_LOW		0x2c
#define SPRD_WDT_FIQ_IRQ_LOAD_HIGH		0x30

/* WDT_CTRL */
#define SPRD_WDT_FIQ_INT_EN_BIT		BIT(0)
#define SPRD_WDT_FIQ_CNT_EN_BIT		BIT(1)
#define SPRD_WDT_FIQ_NEW_VER_EN		BIT(2)
#define SPRD_WDT_FIQ_RST_EN_BIT		BIT(3)

/* WDT_INT_CLR */
#define SPRD_WDT_FIQ_INT_CLEAR_BIT		BIT(0)
#define SPRD_WDT_FIQ_RST_CLEAR_BIT		BIT(3)

/* WDT_INT_RAW */
#define SPRD_WDT_FIQ_INT_RAW_BIT		BIT(0)
#define SPRD_WDT_FIQ_RST_RAW_BIT		BIT(3)
#define SPRD_WDT_FIQ_LD_BUSY_BIT		BIT(4)

/* 1s equal to 32768 counter steps */
#define SPRD_WDT_FIQ_CNT_STEP		32768
#define SPRD_WDT_FIQ_PRINT_LOG		131072

#define SPRD_WDT_FIQ_UNLOCK_KEY		0xe551
#define SPRD_WDT_FIQ_MIN_TIMEOUT		3000
#define SPRD_WDT_FIQ_MAX_TIMEOUT		60000

#define SPRD_WDT_FIQ_CNT_HIGH_SHIFT		16
#define SPRD_WDT_FIQ_LOW_VALUE_MASK		GENMASK(15, 0)
#define SPRD_WDT_FIQ_LOAD_TIMEOUT		1000

#define SPRD_DSWDTEN_MAGIC "enabled"
#define SPRD_DSWDTEN_MAGIC_LEN_MAX  10

#define SPRD_WDT_SLEEP_KICKTIME		540000
#define SPRD_WDT_SLEEP_PRETIMEOUT	(600000-570000)
#define SPRD_WDT_SLEEP_TIMEOUT		600000

/* used for ap hang reboot */
#define SPRD_WDT_RESET_TIMEOUT          0xe551

#define SPRD_PRINT_BUF_LEN              10240
#define WDT_printf(m, x...)			\
	do {                                              \
		if (!m)                                   \
			pr_debug(x);                      \
		else if (seq_buf_printf(m, x)) {         \
			seq_buf_clear(m);                 \
			seq_buf_printf(m, x);             \
		}                                         \
} while (0)

struct sprd_wdt_fiq {
	void __iomem *base;
	struct watchdog_device wdd;
	struct clk *enable;
	struct clk *rtc_enable;
	struct mutex *lock;
	struct alarm sleep_tmr;
	bool sleep_en;
	u32 wdt_ctrl;
	u64 wdt_load;
	const struct sprd_wdt_fiq_data *data;
};

struct sprd_wdt_fiq_data {
	bool eb_always_on;
};

static char *sprd_wdt_buf;
static struct seq_buf *sprd_wdt_seq_buf;

static DEFINE_MUTEX(sprd_wdt_mutex);
struct sprd_wdt_fiq *wdt_fiq;

static struct sprd_wdt_fiq_data sprd_wdt_fiq_common = {
	.eb_always_on = false,
};

static struct sprd_wdt_fiq_data sprd_wdt_fiq_sharkl3 = {
	.eb_always_on = true,
};

static bool sprd_dswdt_fiq_en(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *dswdten_name_p;
	char dswdten_value[SPRD_DSWDTEN_MAGIC_LEN_MAX] = "NULL";
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (ret) {
		pr_err("can't not parse bootargs property\n");
		return false;
	}

	dswdten_name_p = strstr(cmd_line, "sprdboot.dswdten=");
	if (!dswdten_name_p) {
		pr_err("can't find sprdboot.dswdten\n");
		return false;
	}

	sscanf(dswdten_name_p, "sprdboot.dswdten=%8s", dswdten_value);
	if (strncmp(dswdten_value, SPRD_DSWDTEN_MAGIC, strlen(SPRD_DSWDTEN_MAGIC)))
		return false;

	return true;
}

static inline struct sprd_wdt_fiq *to_sprd_wdt_fiq(struct watchdog_device *wdd)
{
	return container_of(wdd, struct sprd_wdt_fiq, wdd);
}

static inline void sprd_wdt_fiq_lock(struct sprd_wdt_fiq *wdt)
{
	void __iomem *addr = wdt->base;

	writel_relaxed(0x0, addr + SPRD_WDT_FIQ_LOCK);
	mutex_unlock(wdt->lock);
}

static inline void sprd_wdt_fiq_unlock(struct sprd_wdt_fiq *wdt)
{
	void __iomem *addr = wdt->base;

	mutex_lock(wdt->lock);
	writel_relaxed(SPRD_WDT_FIQ_UNLOCK_KEY, addr + SPRD_WDT_FIQ_LOCK);
}

static u32 sprd_wdt_fiq_get_cnt_value(struct sprd_wdt_fiq *wdt)
{
	u32 val;

	val = readl_relaxed(wdt->base + SPRD_WDT_FIQ_CNT_READ_HIGH) <<
		SPRD_WDT_FIQ_CNT_HIGH_SHIFT;
	val |= readl_relaxed(wdt->base + SPRD_WDT_FIQ_CNT_READ_LOW) &
		SPRD_WDT_FIQ_LOW_VALUE_MASK;

	return val;
}

static int sprd_wdt_fiq_load_value(struct sprd_wdt_fiq *wdt, u32 timeout,
			       u32 pretimeout)
{
	u32 val, delay_cnt = 0;
	u32 tmr_step = timeout * SPRD_WDT_FIQ_CNT_STEP / 1000;
	u32 prtmr_step = pretimeout * SPRD_WDT_FIQ_CNT_STEP / 1000;

	/*if the set timeout value is greater than 4s printk this log*/
	if (tmr_step > SPRD_WDT_FIQ_PRINT_LOG) {
		WDT_printf(sprd_wdt_seq_buf, "wdt timeout =%d,pretimeout =%d,time =%lu\n",
			   timeout, pretimeout, jiffies);
	}
	wdt->wdt_load = jiffies;

	/*
	 * Waiting the load value operation done,
	 * it needs two or three RTC clock cycles.
	 */
	do {
		val = readl_relaxed(wdt->base + SPRD_WDT_FIQ_INT_RAW);
		if (!(val & SPRD_WDT_FIQ_LD_BUSY_BIT))
			break;
		udelay(1);
	} while (delay_cnt++ < SPRD_WDT_FIQ_LOAD_TIMEOUT);

	if (delay_cnt >= SPRD_WDT_FIQ_LOAD_TIMEOUT) {
		pr_err("sprd_wdt_fiq: wdt load timeout!\n");
		return -EBUSY;
	}
	sprd_wdt_fiq_unlock(wdt);
	writel_relaxed((tmr_step >> SPRD_WDT_FIQ_CNT_HIGH_SHIFT) &
		      SPRD_WDT_FIQ_LOW_VALUE_MASK,
		      wdt->base + SPRD_WDT_FIQ_LOAD_HIGH);
	writel_relaxed((tmr_step & SPRD_WDT_FIQ_LOW_VALUE_MASK),
		       wdt->base + SPRD_WDT_FIQ_LOAD_LOW);
	writel_relaxed((prtmr_step >> SPRD_WDT_FIQ_CNT_HIGH_SHIFT) &
			SPRD_WDT_FIQ_LOW_VALUE_MASK,
		       wdt->base + SPRD_WDT_FIQ_IRQ_LOAD_HIGH);
	writel_relaxed(prtmr_step & SPRD_WDT_FIQ_LOW_VALUE_MASK,
		       wdt->base + SPRD_WDT_FIQ_IRQ_LOAD_LOW);
	sprd_wdt_fiq_lock(wdt);

	return 0;
}

void sprd_wdt_fiq_for_reset(void)
{
	if (!wdt_fiq)
		return;

	if (watchdog_active(&wdt_fiq->wdd)) {
		writel_relaxed(SPRD_WDT_FIQ_UNLOCK_KEY, wdt_fiq->base +
			       SPRD_WDT_FIQ_LOCK);
		writel_relaxed(0, wdt_fiq->base + SPRD_WDT_FIQ_LOAD_HIGH);
		writel_relaxed((SPRD_WDT_RESET_TIMEOUT & SPRD_WDT_FIQ_LOW_VALUE_MASK),
			       wdt_fiq->base + SPRD_WDT_FIQ_LOAD_LOW);
		writel_relaxed(0,  wdt_fiq->base + SPRD_WDT_FIQ_LOCK);
	}
}
EXPORT_SYMBOL_GPL(sprd_wdt_fiq_for_reset);

static int sprd_wdt_fiq_enable(struct sprd_wdt_fiq *wdt)
{
	u32 val;
	int ret;

	ret = clk_prepare_enable(wdt->enable);
	if (ret)
		return ret;
	ret = clk_prepare_enable(wdt->rtc_enable);
	if (ret) {
		clk_disable_unprepare(wdt->enable);
		return ret;
	}

	sprd_wdt_fiq_unlock(wdt);
	val = readl_relaxed(wdt->base + SPRD_WDT_FIQ_CTRL);
	val |= SPRD_WDT_FIQ_NEW_VER_EN;
	writel_relaxed(val, wdt->base + SPRD_WDT_FIQ_CTRL);
	sprd_wdt_fiq_lock(wdt);
	return 0;
}

static void sprd_wdt_fiq_disable(void *_data)
{
	struct sprd_wdt_fiq *wdt = _data;

	sprd_wdt_fiq_unlock(wdt);
	writel_relaxed(0x0, wdt->base + SPRD_WDT_FIQ_CTRL);
	sprd_wdt_fiq_lock(wdt);

	clk_disable_unprepare(wdt->rtc_enable);
	clk_disable_unprepare(wdt->enable);
}

static int sprd_wdt_fiq_start(struct watchdog_device *wdd)
{
	struct sprd_wdt_fiq *wdt = to_sprd_wdt_fiq(wdd);
	u32 val;
	int ret;

	ret = sprd_wdt_fiq_load_value(wdt, wdd->timeout, wdd->pretimeout);
	if (ret)
		return ret;

	if (watchdog_active(wdd))
		return 0;

	sprd_wdt_fiq_unlock(wdt);
	val = readl_relaxed(wdt->base + SPRD_WDT_FIQ_CTRL);
	val |= SPRD_WDT_FIQ_CNT_EN_BIT | SPRD_WDT_FIQ_INT_EN_BIT | SPRD_WDT_FIQ_RST_EN_BIT;
	writel_relaxed(val, wdt->base + SPRD_WDT_FIQ_CTRL);
	set_bit(WDOG_HW_RUNNING, &wdd->status);
	set_bit(WDOG_ACTIVE, &wdd->status);
	wdt_fiq->wdt_ctrl = readl_relaxed(wdt->base + SPRD_WDT_FIQ_CTRL);
	sprd_wdt_fiq_lock(wdt);

	return 0;
}

static int sprd_wdt_fiq_stop(struct watchdog_device *wdd)
{
	struct sprd_wdt_fiq *wdt = to_sprd_wdt_fiq(wdd);
	u32 val;

	sprd_wdt_fiq_unlock(wdt);
	val = readl_relaxed(wdt->base + SPRD_WDT_FIQ_CTRL);
	val &= ~(SPRD_WDT_FIQ_CNT_EN_BIT | SPRD_WDT_FIQ_RST_EN_BIT |
		SPRD_WDT_FIQ_INT_EN_BIT);
	writel_relaxed(val, wdt->base + SPRD_WDT_FIQ_CTRL);
	clear_bit(WDOG_ACTIVE, &wdd->status);
	sprd_wdt_fiq_lock(wdt);

	return 0;
}

static int sprd_wdt_fiq_set_timeout(struct watchdog_device *wdd,
				u32 timeout)
{
	struct sprd_wdt_fiq *wdt = to_sprd_wdt_fiq(wdd);

	if (timeout < wdd->pretimeout)
		return -EINVAL;
	if (timeout == wdd->timeout)
		return 0;

	wdd->timeout = timeout;

	return sprd_wdt_fiq_load_value(wdt, timeout, wdd->pretimeout);
}

static int sprd_wdt_fiq_set_pretimeout(struct watchdog_device *wdd,
				   u32 new_pretimeout)
{
	struct sprd_wdt_fiq *wdt = to_sprd_wdt_fiq(wdd);

	if (wdd->timeout < new_pretimeout)
		return -EINVAL;

	wdd->pretimeout = new_pretimeout;

	return sprd_wdt_fiq_load_value(wdt, wdd->timeout, new_pretimeout);
}

static u32 sprd_wdt_fiq_get_timeleft(struct watchdog_device *wdd)
{
	struct sprd_wdt_fiq *wdt = to_sprd_wdt_fiq(wdd);
	u32 val;

	val = sprd_wdt_fiq_get_cnt_value(wdt);
	val = val / SPRD_WDT_FIQ_CNT_STEP;

	return val;
}

int sprd_wdt_fiq_get_dev(struct watchdog_device **wdd)
{
	int ret = -ENODEV;

	if (wdt_fiq) {
		*wdd = &wdt_fiq->wdd;
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_wdt_fiq_get_dev);

int sprd_wdt_fiq_syscore_suspend(void)
{
	if (!wdt_fiq)
		return -ENODEV;

	if (!wdt_fiq->sleep_en) {
		if (watchdog_active(&wdt_fiq->wdd))
			sprd_wdt_fiq_stop(&wdt_fiq->wdd);

		if (!wdt_fiq->data->eb_always_on)
			sprd_wdt_fiq_disable(wdt_fiq);
	}
	return 0;
}
EXPORT_SYMBOL(sprd_wdt_fiq_syscore_suspend);

void sprd_wdt_fiq_syscore_resume(void)
{
	int ret;

	if (!wdt_fiq)
		return;

	ret = sprd_wdt_fiq_enable(wdt_fiq);
	if (ret)
		return;

	if (watchdog_active(&wdt_fiq->wdd)) {
		ret = sprd_wdt_fiq_start(&wdt_fiq->wdd);
		if (ret)
			return;
	}
}
EXPORT_SYMBOL(sprd_wdt_fiq_syscore_resume);

static struct syscore_ops sprd_wdt_fiq_syscore_ops = {
	.resume = sprd_wdt_fiq_syscore_resume,
	.suspend = sprd_wdt_fiq_syscore_suspend
};

static const struct watchdog_ops sprd_wdt_fiq_ops = {
	.owner = THIS_MODULE,
	.start = sprd_wdt_fiq_start,
	.stop = sprd_wdt_fiq_stop,
	.set_timeout = sprd_wdt_fiq_set_timeout,
	.set_pretimeout = sprd_wdt_fiq_set_pretimeout,
	.get_timeleft = sprd_wdt_fiq_get_timeleft,
};

static const struct watchdog_info sprd_wdt_fiq_info = {
	.options = WDIOF_SETTIMEOUT |
		   WDIOF_PRETIMEOUT |
		   WDIOF_MAGICCLOSE |
		   WDIOF_KEEPALIVEPING,
	.identity = "Spreadtrum Watchdog Timer",
};

static int sprd_wdt_fiq_probe(struct platform_device *pdev)
{
	struct resource *wdt_res;
	struct sprd_wdt_fiq *wdt;
	int ret;

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->data = of_device_get_match_data(&pdev->dev);
	if (!wdt->data) {
		dev_err(&pdev->dev, "can not get private data!\n");
		return -ENODEV;
	}

	wdt->lock = &sprd_wdt_mutex;
	mutex_init(wdt->lock);

	wdt_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt->base = devm_ioremap_resource(&pdev->dev, wdt_res);
	if (IS_ERR(wdt->base)) {
		dev_err(&pdev->dev, "failed to map memory resource\n");
		return PTR_ERR(wdt->base);
	}

	wdt->enable = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(wdt->enable)) {
		dev_err(&pdev->dev, "can't get the enable clock\n");
		return PTR_ERR(wdt->enable);
	}

	wdt->rtc_enable = devm_clk_get(&pdev->dev, "rtc_enable");
	if (IS_ERR(wdt->rtc_enable)) {
		dev_err(&pdev->dev, "can't get the rtc enable clock\n");
		return PTR_ERR(wdt->rtc_enable);
	}

	wdt->wdd.info = &sprd_wdt_fiq_info;
	wdt->wdd.ops = &sprd_wdt_fiq_ops;
	wdt->wdd.parent = &pdev->dev;
	wdt->wdd.min_timeout = SPRD_WDT_FIQ_MIN_TIMEOUT;
	wdt->wdd.max_timeout = SPRD_WDT_FIQ_MAX_TIMEOUT;
	wdt->wdd.timeout = SPRD_WDT_FIQ_MAX_TIMEOUT;

	sprd_wdt_buf = kzalloc(SPRD_PRINT_BUF_LEN, GFP_KERNEL);
	if (!sprd_wdt_buf)
		return -ENOMEM;

	sprd_wdt_seq_buf = kzalloc(sizeof(*sprd_wdt_seq_buf), GFP_KERNEL);
	if (!sprd_wdt_seq_buf) {
		ret = -ENOMEM;
		goto free_wdt_buf;
	}

	ret = minidump_save_extend_information("sprd_wdt_fiq",
					__pa((unsigned long)(sprd_wdt_buf)),
					 __pa((unsigned long)(sprd_wdt_buf) +
					      SPRD_PRINT_BUF_LEN));

	if (ret) {
		dev_err(&pdev->dev, "alloc sprd wdt fiq fail\n");
		goto free_seq_buf;
	}

	seq_buf_init(sprd_wdt_seq_buf, sprd_wdt_buf, SPRD_PRINT_BUF_LEN);

	wdt->sleep_en = sprd_dswdt_fiq_en();
	ret = sprd_wdt_fiq_enable(wdt);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable wdt\n");
		return ret;
	}
	ret = devm_add_action(&pdev->dev, sprd_wdt_fiq_disable, wdt);
	if (ret) {
		sprd_wdt_fiq_disable(wdt);
		dev_err(&pdev->dev, "Failed to add wdt disable action\n");
		goto free_seq_buf;
	}

	register_syscore_ops(&sprd_wdt_fiq_syscore_ops);
	wdt_fiq = wdt;

	platform_set_drvdata(pdev, wdt);

	return 0;

free_seq_buf:
	kfree(sprd_wdt_seq_buf);
	sprd_wdt_seq_buf = NULL;
free_wdt_buf:
	kfree(sprd_wdt_buf);
	sprd_wdt_buf = NULL;
	return ret;
}

static const struct of_device_id sprd_wdt_fiq_match_table[] = {
	{
		.compatible = "sprd,wdt-r2p0-fiq",
		.data = &sprd_wdt_fiq_common,
	},
	{
		.compatible = "sprd,wdt-r2p0-fiq-sharkl3",
		.data = &sprd_wdt_fiq_sharkl3,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_wdt_fiq_match_table);

static struct platform_driver sprd_watchdog_fiq_driver = {
	.probe	= sprd_wdt_fiq_probe,
	.driver	= {
		.name = "sprd-wdt-fiq",
		.of_match_table = sprd_wdt_fiq_match_table,
	},
};
module_platform_driver(sprd_watchdog_fiq_driver);

MODULE_AUTHOR("Ling Xu <ling_ling.xu@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum Watchdog Timer Controller Driver");
MODULE_LICENSE("GPL v2");
