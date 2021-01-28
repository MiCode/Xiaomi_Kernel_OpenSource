// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2014 Matthias Brugger
 *
 * Matthias Brugger <matthias.bgg@gmail.com>
 *
 * Based on sunxi_wdt.c
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <dt-bindings/soc/mediatek,boot-mode.h>

#define WDT_MAX_TIMEOUT		31
#define WDT_MIN_TIMEOUT		1
#define WDT_LENGTH_TIMEOUT(n)	((n) << 5)

#define WDT_LENGTH		0x04
#define WDT_LENGTH_KEY		0x8

#define WDT_RST			0x08
#define WDT_RST_RELOAD		0x1971

#define WDT_MODE		0x00
#define WDT_MODE_EN		(1 << 0)
#define WDT_MODE_EXT_POL_LOW	(0 << 1)
#define WDT_MODE_EXT_POL_HIGH	(1 << 1)
#define WDT_MODE_EXRST_EN	(1 << 2)
#define WDT_MODE_IRQ_EN		(1 << 3)
#define WDT_MODE_AUTO_START	(1 << 4)
#define WDT_MODE_IRQ_LEVEL_EN	(1 << 5)
#define WDT_MODE_DUAL_EN	(1 << 6)
#define WDT_MODE_DDR_RSVD	(1 << 7)
#define WDT_MODE_KEY		0x22000000

#define WDT_SWRST		0x14
#define WDT_SWRST_KEY		0x1209

#define WDT_NONRST2		0x24
#define RGU_REBOOT_MASK		0xF
#define WDT_NONRST2_STAGE_OFS	30
#define RGU_STAGE_MASK		0x3
#define RGU_STAGE_KERNEL	0x3
#define WDT_BYPASS_PWR_KEY	(1 << 13)

#define MTK_WDT_REQ_MODE        (0x30)
#define MTK_WDT_REQ_MODE_KEY    (0x33000000)
#define MTK_WDT_REQ_MODE_LEN	(32)

#define WDT_LATCH_CTL2	0x48
#define WDT_DFD_EN              (1 << 17)
#define WDT_DFD_THERMAL1_DIS    (1 << 18)
#define WDT_DFD_THERMAL2_DIS    (1 << 19)
#define WDT_DFD_TIMEOUT_MASK    0x1FFFF
#define WDT_LATCH_CTL2_KEY	0x95000000

#define DRV_NAME		"mtk-wdt"
#define DRV_VERSION		"1.0"

static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;

struct mtk_wdt_dev {
	struct watchdog_device wdt_dev;
	struct reset_controller_dev rcdev;
	void __iomem *wdt_base;
	u32 dfd_timeout;
};

/* Reset controller driver support.
 * This is used to enable and disable rgu reset source control
 */
static inline struct mtk_wdt_dev *to_mtk_wdt_dev(
						struct reset_controller_dev *rc)
{
	return container_of(rc, struct mtk_wdt_dev, rcdev);
}

static int mtk_reset_update(struct reset_controller_dev *rcdev,
				unsigned long id, bool assert)
{
	struct mtk_wdt_dev *mtk_wdt = to_mtk_wdt_dev(rcdev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	int reg_width = sizeof(u32);
	int offset = id % (reg_width * BITS_PER_BYTE);
	unsigned int mask, value, reg;

	mask = BIT(offset);
	value = assert ? mask : 0;
	reg = readl(wdt_base + MTK_WDT_REQ_MODE);
	if (assert)
		reg |= BIT(offset);
	else
		reg &= ~BIT(offset);
	writel(MTK_WDT_REQ_MODE_KEY | reg, wdt_base + MTK_WDT_REQ_MODE);
	return 0;
}

static int mtk_reset_assert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	return mtk_reset_update(rcdev, id, true);
}

static int mtk_reset_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return mtk_reset_update(rcdev, id, false);
}

static int mtk_reset_status(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	struct mtk_wdt_dev *mtk_wdt = to_mtk_wdt_dev(rcdev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	int reg_width = sizeof(u32);
	int offset = id % (reg_width * BITS_PER_BYTE);
	unsigned int reg = 0;

	reg = readl(wdt_base + MTK_WDT_REQ_MODE);
	return (reg & BIT(offset));
}

static const struct reset_control_ops mtk_reset_ops = {
	.assert     = mtk_reset_assert,
	.deassert   = mtk_reset_deassert,
	.status     = mtk_reset_status,
};

static void mtk_wdt_mark_stage(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg = readl(wdt_base + WDT_NONRST2);

	reg = (reg & ~(RGU_STAGE_MASK << WDT_NONRST2_STAGE_OFS)) |
	      (RGU_STAGE_KERNEL << WDT_NONRST2_STAGE_OFS);

	writel(reg, wdt_base + WDT_NONRST2);
}

static void mtk_wdt_parse_dt(struct device_node *np,
				struct mtk_wdt_dev *mtk_wdt)
{
	int ret;
	void __iomem *wdt_base = NULL;
	unsigned int reg = 0, tmp = 0;

	if (!np || !mtk_wdt)
		return;

	ret = of_property_read_u32(np, "mediatek,rg_dfd_timeout",
				      &mtk_wdt->dfd_timeout);

	wdt_base = mtk_wdt->wdt_base;

	if (!ret && mtk_wdt->dfd_timeout) {
		tmp = mtk_wdt->dfd_timeout & WDT_DFD_TIMEOUT_MASK;

		/* enable dfd_en and setup timeout */
		reg = readl(wdt_base + WDT_LATCH_CTL2);
		reg &= ~(WDT_DFD_THERMAL2_DIS | WDT_DFD_TIMEOUT_MASK);
		reg |= (WDT_DFD_EN | WDT_DFD_THERMAL1_DIS |
			 WDT_LATCH_CTL2_KEY | tmp);
		writel(reg, wdt_base + WDT_LATCH_CTL2);
	}
}

static int mtk_wdt_restart(struct watchdog_device *wdt_dev,
			   unsigned long action, void *cmd)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;
	u32 mode, nonrst2;

	wdt_base = mtk_wdt->wdt_base;
	mode = readl(wdt_base + WDT_MODE) & ~WDT_MODE_DDR_RSVD;
	nonrst2 = readl(wdt_base + WDT_MODE) & ~RGU_REBOOT_MASK;

	if (cmd && !strcmp(cmd, "charger"))
		nonrst2 |= BOOT_CHARGER;
	else if (cmd && !strcmp(cmd, "recovery"))
		nonrst2 |= BOOT_RECOVERY;
	else if (cmd && !strcmp(cmd, "bootloader"))
		nonrst2 |= BOOT_BOOTLOADER;
	else if (cmd && !strcmp(cmd, "dm-verity device corrupted"))
		nonrst2 |= BOOT_DM_VERITY | WDT_BYPASS_PWR_KEY;
	else if (cmd && !strcmp(cmd, "kpoc"))
		nonrst2 |= BOOT_KPOC;
	else if (cmd && !strcmp(cmd, "ddr-reserve"))
		nonrst2 |= BOOT_DDR_RSVD;
	else
		nonrst2 |= WDT_BYPASS_PWR_KEY;

	if ((nonrst2 & RGU_REBOOT_MASK) == BOOT_DDR_RSVD)
		mode |= WDT_MODE_DDR_RSVD;

	writel(WDT_MODE_KEY | mode, wdt_base + WDT_MODE);
	writel(nonrst2, wdt_base + WDT_NONRST2);

	while (1) {
		writel(WDT_SWRST_KEY, wdt_base + WDT_SWRST);
		mdelay(5);
	}

	return 0;
}

static int mtk_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;

	iowrite32(WDT_RST_RELOAD, wdt_base + WDT_RST);

	return 0;
}

static int mtk_wdt_set_timeout(struct watchdog_device *wdt_dev,
				unsigned int timeout)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	wdt_dev->timeout = timeout;

	/*
	 * One bit is the value of 512 ticks
	 * The clock has 32 KHz
	 */
	reg = WDT_LENGTH_TIMEOUT(timeout << 6) | WDT_LENGTH_KEY;
	iowrite32(reg, wdt_base + WDT_LENGTH);

	mtk_wdt_ping(wdt_dev);

	return 0;
}

static int mtk_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	reg = readl(wdt_base + WDT_MODE);
	reg &= ~WDT_MODE_EN;
	reg |= WDT_MODE_KEY;
	iowrite32(reg, wdt_base + WDT_MODE);

	clear_bit(WDOG_HW_RUNNING, &mtk_wdt->wdt_dev.status);

	return 0;
}

static int mtk_wdt_start(struct watchdog_device *wdt_dev)
{
	u32 reg;
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	int ret;

	ret = mtk_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
	if (ret < 0)
		return ret;

	reg = ioread32(wdt_base + WDT_MODE);
	reg &= ~WDT_MODE_IRQ_LEVEL_EN;
	reg |= (WDT_MODE_EN | WDT_MODE_KEY);
	iowrite32(reg, wdt_base + WDT_MODE);

	set_bit(WDOG_HW_RUNNING, &mtk_wdt->wdt_dev.status);

	return 0;
}

static const struct watchdog_info mtk_wdt_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops mtk_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= mtk_wdt_start,
	.stop		= mtk_wdt_stop,
	.ping		= mtk_wdt_ping,
	.set_timeout	= mtk_wdt_set_timeout,
	.restart	= mtk_wdt_restart,
};

static int mtk_wdt_probe(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt;
	struct resource *res;
	int err;

	mtk_wdt = devm_kzalloc(&pdev->dev, sizeof(*mtk_wdt), GFP_KERNEL);
	if (!mtk_wdt)
		return -ENOMEM;

	platform_set_drvdata(pdev, mtk_wdt);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mtk_wdt->wdt_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mtk_wdt->wdt_base))
		return PTR_ERR(mtk_wdt->wdt_base);

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	mtk_wdt->wdt_dev.info = &mtk_wdt_info;
	mtk_wdt->wdt_dev.ops = &mtk_wdt_ops;
	mtk_wdt->wdt_dev.timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.max_timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.min_timeout = WDT_MIN_TIMEOUT;
	mtk_wdt->wdt_dev.parent = &pdev->dev;

	watchdog_init_timeout(&mtk_wdt->wdt_dev, timeout, &pdev->dev);
	watchdog_set_nowayout(&mtk_wdt->wdt_dev, nowayout);
	watchdog_set_restart_priority(&mtk_wdt->wdt_dev, 128);

	watchdog_set_drvdata(&mtk_wdt->wdt_dev, mtk_wdt);

	mtk_wdt_mark_stage(&mtk_wdt->wdt_dev);

	mtk_wdt_parse_dt(pdev->dev.of_node, mtk_wdt);

	if (readl(mtk_wdt->wdt_base + WDT_MODE) & WDT_MODE_EN)
		mtk_wdt_start(&mtk_wdt->wdt_dev);
	else
		mtk_wdt_stop(&mtk_wdt->wdt_dev);

	err = watchdog_register_device(&mtk_wdt->wdt_dev);
	if (unlikely(err))
		return err;

	/* register reset controller for reset source setting */
	mtk_wdt->rcdev.owner = THIS_MODULE;
	mtk_wdt->rcdev.nr_resets =  MTK_WDT_REQ_MODE_LEN;
	mtk_wdt->rcdev.ops = &mtk_reset_ops;
	mtk_wdt->rcdev.of_node = pdev->dev.of_node;

	err = devm_reset_controller_register(&pdev->dev, &mtk_wdt->rcdev);
	if (unlikely(err))
		dev_info(&pdev->dev, "toprgu does not support as reset controller\n");

	dev_info(&pdev->dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
			mtk_wdt->wdt_dev.timeout, nowayout);

	return 0;
}

static void mtk_wdt_shutdown(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt = platform_get_drvdata(pdev);

	if (watchdog_active(&mtk_wdt->wdt_dev))
		mtk_wdt_stop(&mtk_wdt->wdt_dev);
}

static int mtk_wdt_remove(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt = platform_get_drvdata(pdev);

	watchdog_unregister_device(&mtk_wdt->wdt_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_wdt_suspend(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev))
		mtk_wdt_stop(&mtk_wdt->wdt_dev);

	return 0;
}

static int mtk_wdt_resume(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev)) {
		mtk_wdt_start(&mtk_wdt->wdt_dev);
		mtk_wdt_ping(&mtk_wdt->wdt_dev);
	}

	return 0;
}
#endif

static const struct of_device_id mtk_wdt_dt_ids[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_wdt_dt_ids);

static const struct dev_pm_ops mtk_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_wdt_suspend,
				mtk_wdt_resume)
};

static struct platform_driver mtk_wdt_driver = {
	.probe		= mtk_wdt_probe,
	.remove		= mtk_wdt_remove,
	.shutdown	= mtk_wdt_shutdown,
	.driver		= {
		.name		= DRV_NAME,
		.pm		= &mtk_wdt_pm_ops,
		.of_match_table	= mtk_wdt_dt_ids,
	},
};

module_platform_driver(mtk_wdt_driver);

module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthias Brugger <matthias.bgg@gmail.com>");
MODULE_DESCRIPTION("Mediatek WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
