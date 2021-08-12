// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#define INTR_SET_OFS	0x0
#define INTR_CLR_OFS	0x4

#define MBOX_CHANS	2
#define MBOX_DEBUG	0


struct mhu_link {
	unsigned irq;
	void __iomem *tx_reg;
	void __iomem *rx_reg;
};

struct mtk_mbu {
	void __iomem *base;
	struct mhu_link mlink[MBOX_CHANS];
	struct mbox_chan chan[MBOX_CHANS];
	struct mbox_controller mbox;
};

static irqreturn_t tinysys_mbox_rx_interrupt(int irq, void *p)
{
	struct mbox_chan *chan = p;
	struct mhu_link *mlink = chan->con_priv;
	u32 val;

	val = readl_relaxed(mlink->rx_reg + INTR_CLR_OFS);
#if MBOX_DEBUG
	dev_notice(chan->mbox->dev,"[scmi] tinysys_mbox_rx_interrupt chan:%px txdone_method:%x\n", chan, chan->txdone_method);
#endif
	if (!val)
		return IRQ_NONE;

	writel_relaxed(1, mlink->rx_reg + INTR_CLR_OFS);
	mbox_chan_received_data(chan, (void *)&val);

	return IRQ_HANDLED;
}

static bool tinysys_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct mhu_link *mlink = chan->con_priv;
	u32 val = readl_relaxed(mlink->tx_reg + INTR_SET_OFS);

#if MBOX_DEBUG
	if(val == 0)
		dev_notice(chan->mbox->dev, "[scmi] last_tx_done\n");
#endif
	return (val == 0);
}

static int tinysys_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct mhu_link *mlink = chan->con_priv;
#if MBOX_DEBUG
	u32 *arg = data;

	dev_notice(chan->mbox->dev, "[scmi] send_data %x\n", *arg);
#endif
	smp_mb();
	writel_relaxed(1, mlink->tx_reg + INTR_SET_OFS);

	return 0;
}

static int tinysys_mbox_startup(struct mbox_chan *chan)
{
	struct mhu_link *mlink = chan->con_priv;
	int ret;

	ret = request_irq(mlink->irq, tinysys_mbox_rx_interrupt,
			  IRQF_TRIGGER_NONE, "mtk_tinysys_mbox", chan);
	if (ret) {
		dev_notice(chan->mbox->dev,
			"Unable to acquire IRQ %d\n", mlink->irq);
		return ret;
	}
	return 0;
}

static void tinysys_mbox_shutdown(struct mbox_chan *chan)
{
	struct mhu_link *mlink = chan->con_priv;

	free_irq(mlink->irq, chan);
}

static const struct mbox_chan_ops tinysys_mbox_chan_ops = {
	.send_data = tinysys_mbox_send_data,
	.startup = tinysys_mbox_startup,
	.shutdown = tinysys_mbox_shutdown,
	.last_tx_done = tinysys_mbox_last_tx_done,
};

static int tinysys_mbox_probe(struct platform_device *pdev)
{
	int err, i;
	struct mtk_mbu *mbu;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct tinysys_mbox_plat *plat_data;

	/* Allocate memory for device */
	mbu = devm_kzalloc(dev, sizeof(*mbu), GFP_KERNEL);
	if (!mbu)
		return -ENOMEM;



	for (i = 0; i < MBOX_CHANS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		mbu->base = devm_ioremap_resource(dev, res);

		if (IS_ERR(mbu->base)) {
			dev_notice(dev, "failed to ioremap mbu");
			return PTR_ERR(mbu->base);
		}
		mbu->chan[i].con_priv = &mbu->mlink[i];
		mbu->mlink[i].irq = platform_get_irq(pdev, i);
		if (!mbu->mlink[i].irq) {
			dev_notice(dev, "failed to get irq");
		}
		mbu->mlink[i].rx_reg = mbu->base;
		mbu->mlink[i].tx_reg = mbu->base;
	}

	plat_data = (struct tinysys_mbox_plat *)of_device_get_match_data(dev);
	if (!plat_data) {
		dev_notice(dev, "failed to get match data\n");
		return -EINVAL;
	}

	mbu->mbox.dev = dev;
	mbu->mbox.chans = &mbu->chan[0];
	mbu->mbox.num_chans = MBOX_CHANS;
	mbu->mbox.ops = &tinysys_mbox_chan_ops;
	mbu->mbox.txdone_irq = false;
	mbu->mbox.txdone_poll = true;
	mbu->mbox.txpoll_period = 1;

	platform_set_drvdata(pdev, mbu);

	err = devm_mbox_controller_register(dev, &mbu->mbox);
	if (err) {
		dev_notice(dev, "Failed to register mailboxes %d\n", err);
		return err;
	}

	dev_info(dev, "MTK MBOX Mailbox registered\n");

	return 0;
}



struct tinysys_mbox_plat {
	u32 thread_nr;
	u32 bfs;
};

static const struct tinysys_mbox_plat tinysys_mbox_plat_v1 = {.thread_nr = 1, .bfs = 64};

static const struct of_device_id tinysys_mbox_of_ids[] = {
	{.compatible = "mediatek,tinysys_mbox", .data = (void *)&tinysys_mbox_plat_v1},
	{}
};

static struct platform_driver tinysys_mbox_drv = {
	.probe = tinysys_mbox_probe,
	.driver = {
		.name = "mtk_tinysys_mbox",
		.of_match_table = tinysys_mbox_of_ids,
	}
};
static __init int mtk_tinysys_mbox_driver(void)
{
	u32 err = 0;

	err = platform_driver_register(&tinysys_mbox_drv);
	if (err) {
		pr_notice("mtk_tinysys_mbox_driver failed:%d", err);
		return err;
	}
	return 0;
}

module_init(mtk_tinysys_mbox_driver);
MODULE_LICENSE("GPL v2");

