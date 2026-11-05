// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>

/* CPUCP Register values */
#define CPUCP_SEND_IRQ_VAL		BIT(28)
#define CPUCP_CLEAR_IRQ_VAL		BIT(3)
#define CPUCP_STATUS_IRQ_VAL		BIT(3)
#define APSS_CPUCP_RX_MBOX_CMD_MASK	0xFFFFFFFFFFFFFFFF

/**
 * struct cpucp_ipc     ipc per channel
 * @mbox:		mailbox-controller interface
 * @chans:		The mailbox clients' channel array
 * @desc:		SoC specific mbox descriptor
 * @tx_irq_base:	Memory address for sending irq
 * @rx_irq_base:	Memory address for receiving irq
 * @dev:		Device associated with this instance
 * @irq:		CPUCP to HLOS irq
 * @rx_chans:		Bitmask of channels to monitor on rx
 */
struct qcom_cpucp_ipc {
	struct mbox_controller mbox;
	struct mbox_chan *chans;
	const struct qcom_cpucp_mbox_desc *desc;
	void __iomem *tx_irq_base;
	void __iomem *rx_irq_base;
	struct device *dev;
	int irq;
	u32 rx_chans;
};

struct qcom_cpucp_mbox_desc {
	u32 enable_reg;
	u32 map_reg;
	u32 rx_reg;
	u32 send_reg;
	u32 status_reg;
	u32 clear_reg;
	u32 chan_stride;
	bool v2_mbox;
	u32 num_chans;
};

static irqreturn_t qcom_cpucp_rx_interrupt(int irq, void *p)
{
	struct qcom_cpucp_ipc *cpucp_ipc = p;
	const struct qcom_cpucp_mbox_desc *desc = cpucp_ipc->desc;
	u32 val;
	int i;
	unsigned long flags;

	for (i = 0; i < desc->num_chans; i++) {
		val = readl(cpucp_ipc->rx_irq_base + desc->status_reg + (i * desc->chan_stride));
		if (val & CPUCP_STATUS_IRQ_VAL) {
			writel(CPUCP_CLEAR_IRQ_VAL,
			       cpucp_ipc->rx_irq_base + desc->clear_reg + (i * desc->chan_stride));
			/* Make sure reg write is complete before proceeding */
			mb();
			spin_lock_irqsave(&cpucp_ipc->chans[i].lock, flags);
			if (!IS_ERR(cpucp_ipc->chans[i].con_priv))
				mbox_chan_received_data(&cpucp_ipc->chans[i], NULL);
			spin_unlock_irqrestore(&cpucp_ipc->chans[i].lock, flags);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t qcom_cpucp_v2_mbox_rx_interrupt(int irq, void *p)
{
	struct qcom_cpucp_ipc *cpucp_ipc = p;
	const struct qcom_cpucp_mbox_desc *desc = cpucp_ipc->desc;
	irqreturn_t ret = IRQ_NONE;
	u64 status, data;
	int i;
	unsigned long flags;

	status = readq(cpucp_ipc->rx_irq_base + desc->status_reg);

	for (i = 0; i < desc->num_chans; i++) {
		if (cpucp_ipc->rx_chans && !(cpucp_ipc->rx_chans & BIT(i)))
			continue;
		if (status & ((u64)1 << i)) {
			data = readq(cpucp_ipc->rx_irq_base + desc->rx_reg +
						(i * desc->chan_stride));
			writeq(status, cpucp_ipc->rx_irq_base + desc->clear_reg);
			/* Make sure reg write is complete before proceeding */
			mb();
			spin_lock_irqsave(&cpucp_ipc->chans[i].lock, flags);
			if (!IS_ERR(cpucp_ipc->chans[i].con_priv))
				mbox_chan_received_data(&cpucp_ipc->chans[i], (void *)&data);
			spin_unlock_irqrestore(&cpucp_ipc->chans[i].lock, flags);
			ret = IRQ_HANDLED;
		}
	}

	return ret;
}

static int qcom_cpucp_mbox_startup(struct mbox_chan *chan)
{
	struct qcom_cpucp_ipc *cpucp_ipc = container_of(chan->mbox, struct qcom_cpucp_ipc, mbox);
	unsigned long chan_id = (unsigned long)chan->con_priv;
	const struct qcom_cpucp_mbox_desc *desc = cpucp_ipc->desc;
	u64 val;

	if (desc->v2_mbox) {
		val = readq(cpucp_ipc->rx_irq_base + desc->enable_reg);
		val |= ((u64)1 << chan_id);
		writeq(val, cpucp_ipc->rx_irq_base + desc->enable_reg);
	}

	return 0;
}

static void qcom_cpucp_mbox_shutdown(struct mbox_chan *chan)
{
	struct qcom_cpucp_ipc *cpucp_ipc = container_of(chan->mbox, struct qcom_cpucp_ipc, mbox);
	unsigned long chan_id = (unsigned long)chan->con_priv;
	const struct qcom_cpucp_mbox_desc *desc = cpucp_ipc->desc;
	unsigned long flags;
	u64 val;

	if (desc->v2_mbox) {
		val = readq(cpucp_ipc->rx_irq_base + desc->enable_reg);
		val &= ~((u64)1 << chan_id);
		writeq(val, cpucp_ipc->rx_irq_base + desc->enable_reg);
	}

	spin_lock_irqsave(&chan->lock, flags);
	chan->con_priv = ERR_PTR(-EINVAL);
	spin_unlock_irqrestore(&chan->lock, flags);
}

static int qcom_cpucp_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct qcom_cpucp_ipc *cpucp_ipc = container_of(chan->mbox, struct qcom_cpucp_ipc, mbox);
	unsigned long chan_id = (unsigned long)chan->con_priv;
	const struct qcom_cpucp_mbox_desc *desc = cpucp_ipc->desc;
	u32 val = desc->v2_mbox ? *(u32 *)data : CPUCP_SEND_IRQ_VAL;
	u32 offset = desc->v2_mbox ? (chan_id * desc->chan_stride) : 0;

	writel(val, cpucp_ipc->tx_irq_base + desc->send_reg + offset);

	return 0;
}

static struct mbox_chan *qcom_cpucp_mbox_xlate(struct mbox_controller *mbox,
			const struct of_phandle_args *sp)
{
	unsigned long ind = sp->args[0];

	if (sp->args_count != 1)
		return ERR_PTR(-EINVAL);

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	if (!IS_ERR(mbox->chans[ind].con_priv))
		return ERR_PTR(-EBUSY);

	mbox->chans[ind].con_priv = (void *)ind;

	return &mbox->chans[ind];
}

static const struct mbox_chan_ops cpucp_mbox_chan_ops = {
	.startup = qcom_cpucp_mbox_startup,
	.send_data = qcom_cpucp_mbox_send_data,
	.shutdown = qcom_cpucp_mbox_shutdown
};

static int qcom_cpucp_ipc_setup_mbox(struct qcom_cpucp_ipc *cpucp_ipc)
{
	struct mbox_controller *mbox;
	struct device *dev = cpucp_ipc->dev;
	unsigned long i;

	/* Initialize channel identifiers */
	for (i = 0; i < cpucp_ipc->desc->num_chans; i++)
		cpucp_ipc->chans[i].con_priv = ERR_PTR(-EINVAL);

	mbox = &cpucp_ipc->mbox;
	mbox->dev = dev;
	mbox->num_chans = cpucp_ipc->desc->num_chans;
	mbox->chans = cpucp_ipc->chans;
	mbox->ops = &cpucp_mbox_chan_ops;
	mbox->of_xlate = qcom_cpucp_mbox_xlate;
	mbox->txdone_irq = false;
	mbox->txdone_poll = false;

	return mbox_controller_register(mbox);
}

static int qcom_cpucp_probe(struct platform_device *pdev)
{
	const struct qcom_cpucp_mbox_desc *desc;
	struct qcom_cpucp_ipc *cpucp_ipc;
	struct resource *res;
	unsigned long flags = IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND;
	int ret;

	desc = device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	cpucp_ipc = devm_kzalloc(&pdev->dev, sizeof(*cpucp_ipc), GFP_KERNEL);
	if (!cpucp_ipc)
		return -ENOMEM;

	cpucp_ipc->dev = &pdev->dev;
	cpucp_ipc->desc = desc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tx");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get the device base address\n");
		return -ENODEV;
	}

	cpucp_ipc->tx_irq_base = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
	if (!cpucp_ipc->tx_irq_base) {
		dev_err(&pdev->dev, "Failed to ioremap cpucp tx irq addr\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rx");
	if (!res) {
		dev_err(&pdev->dev, "Failed to get the device base address\n");
		return -ENODEV;
	}

	cpucp_ipc->rx_irq_base = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
	if (!cpucp_ipc->rx_irq_base) {
		dev_err(&pdev->dev, "Failed to ioremap cpucp rx irq addr\n");
		return -ENOMEM;
	}

	cpucp_ipc->chans = devm_kzalloc(&pdev->dev, desc->num_chans *
					sizeof(struct mbox_chan), GFP_KERNEL);
	if (!cpucp_ipc->chans)
		return -ENOMEM;

	if (desc->v2_mbox) {
		writeq(0, cpucp_ipc->rx_irq_base + desc->enable_reg);
		writeq(0, cpucp_ipc->rx_irq_base + desc->clear_reg);
		writeq(0, cpucp_ipc->rx_irq_base + desc->map_reg);
	}

	cpucp_ipc->irq = platform_get_irq(pdev, 0);
	if (cpucp_ipc->irq < 0) {
		dev_err(&pdev->dev, "Failed to get the IRQ\n");
		return cpucp_ipc->irq;
	}

	ret = of_property_read_u32(cpucp_ipc->dev->of_node, "qcom,rx-chans",
						&cpucp_ipc->rx_chans);
	if (ret < 0 || !cpucp_ipc->rx_chans)
		dev_dbg(&pdev->dev, "Missing chans mask. Skipping\n");
	else
		flags |= IRQF_SHARED;
	ret = devm_request_irq(&pdev->dev, cpucp_ipc->irq,
			desc->v2_mbox ? qcom_cpucp_v2_mbox_rx_interrupt : qcom_cpucp_rx_interrupt,
			flags, "qcom_cpucp", cpucp_ipc);

	if (ret < 0)
		return dev_err_probe(&pdev->dev, ret, "Failed to register the irq\n");

	if (desc->v2_mbox)
		writeq(APSS_CPUCP_RX_MBOX_CMD_MASK, cpucp_ipc->rx_irq_base + desc->map_reg);

	ret = qcom_cpucp_ipc_setup_mbox(cpucp_ipc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "Failed to create mailbox\n");

	platform_set_drvdata(pdev, cpucp_ipc);

	return 0;
}

static int qcom_cpucp_remove(struct platform_device *pdev)
{
	struct qcom_cpucp_ipc *cpucp_ipc = platform_get_drvdata(pdev);

	mbox_controller_unregister(&cpucp_ipc->mbox);

	return 0;
}

static const struct qcom_cpucp_mbox_desc cpucp_mbox_desc = {
	.send_reg = 0xC,
	.chan_stride = 0x1000,
	.status_reg = 0x30C,
	.clear_reg = 0x308,
	.v2_mbox = false,
	.num_chans = 2,
};

static const struct qcom_cpucp_mbox_desc cpucp_v2_mbox_desc = {
	.rx_reg = 0x100,
	.send_reg = 0x104,
	.chan_stride = 0x8,
	.map_reg = 0x4000,
	.status_reg = 0x4400,
	.clear_reg = 0x4800,
	.enable_reg = 0x4C00,
	.v2_mbox = true,
	.num_chans = 8,
};

static const struct of_device_id qcom_cpucp_of_match[] = {
	{ .compatible = "qcom,cpucp-v2", .data = &cpucp_v2_mbox_desc},
	{ .compatible = "qcom,cpucp", .data = &cpucp_mbox_desc},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_cpucp_of_match);

static struct platform_driver qcom_cpucp_driver = {
	.probe = qcom_cpucp_probe,
	.remove = qcom_cpucp_remove,
	.driver = {
		.name = "qcom_cpucp",
		.of_match_table = qcom_cpucp_of_match,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(qcom_cpucp_driver);

MODULE_DESCRIPTION("QTI CPUCP Driver");
MODULE_LICENSE("GPL");
