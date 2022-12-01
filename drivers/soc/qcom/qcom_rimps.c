// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>
#include <linux/msm_rtb.h>

/* RIMPS Register offsets */
#define RIMPS_IPC_CHAN_SUPPORTED	2
#define RIMPS_SEND_IRQ_OFFSET		0xC
#define RIMPS_SEND_IRQ_VAL		BIT(28)
#define RIMPS_CLEAR_IRQ_OFFSET		0x308
#define RIMPS_STATUS_IRQ_OFFSET		0x30C
#define RIMPS_CLEAR_IRQ_VAL		BIT(3)
#define RIMPS_STATUS_IRQ_VAL		BIT(3)
#define RIMPS_CLOCK_DOMAIN_OFFSET	0x1000


/**
 * struct rimps_ipc     ipc per channel
 * @mbox:		mailbox-controller interface
 * @chans:		The mailbox clients' channel array
 * @dev:		Device associated with this instance
 * @irq:		Rimps to HLOS irq
 */
struct qcom_rimps_ipc {
	struct mbox_controller mbox;
	struct mbox_chan chans[RIMPS_IPC_CHAN_SUPPORTED];
	void __iomem *tx_irq_base;
	void __iomem *rx_irq_base;
	struct device *dev;
	int irq;
	int num_chan;
};

static irqreturn_t qcom_rimps_rx_interrupt(int irq, void *p)
{
	struct qcom_rimps_ipc *rimps_ipc;
	u32 val;
	int i;
	unsigned long flags;

	rimps_ipc = p;

	for (i = 0; i < rimps_ipc->num_chan; i++) {

		val = readl_no_log(rimps_ipc->rx_irq_base +
		RIMPS_STATUS_IRQ_OFFSET + (i * RIMPS_CLOCK_DOMAIN_OFFSET));
		if (val & RIMPS_STATUS_IRQ_VAL) {

			val = RIMPS_CLEAR_IRQ_VAL;
			writel_no_log(val, rimps_ipc->rx_irq_base +
			RIMPS_CLEAR_IRQ_OFFSET +
				(i * RIMPS_CLOCK_DOMAIN_OFFSET));
			/* Make sure register write is complete before proceeding */
			mb();
			spin_lock_irqsave(&rimps_ipc->chans[i].lock, flags);
			if (rimps_ipc->chans[i].con_priv)
				mbox_chan_received_data(&rimps_ipc->chans[i]
							, NULL);
			spin_unlock_irqrestore(&rimps_ipc->chans[i].lock, flags);
		}
	}

	return IRQ_HANDLED;
}

static void qcom_rimps_mbox_shutdown(struct mbox_chan *chan)
{
	unsigned long flags;

	spin_lock_irqsave(&chan->lock, flags);
	chan->con_priv = NULL;
	spin_unlock_irqrestore(&chan->lock, flags);
}

static int qcom_rimps_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct qcom_rimps_ipc *rimps_ipc = container_of(chan->mbox,
						  struct qcom_rimps_ipc, mbox);

	writel_no_log(RIMPS_SEND_IRQ_VAL,
			rimps_ipc->tx_irq_base + RIMPS_SEND_IRQ_OFFSET);
	return 0;
}

static struct mbox_chan *qcom_rimps_mbox_xlate(struct mbox_controller *mbox,
			const struct of_phandle_args *sp)
{
	struct qcom_rimps_ipc *rimps_ipc = container_of(mbox,
						  struct qcom_rimps_ipc, mbox);
	unsigned long ind = sp->args[0];

	if (sp->args_count != 1)
		return ERR_PTR(-EINVAL);

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	if (mbox->chans[ind].con_priv)
		return ERR_PTR(-EBUSY);

	mbox->chans[ind].con_priv = rimps_ipc;
	return &mbox->chans[ind];
}

static const struct mbox_chan_ops rimps_mbox_chan_ops = {
	.send_data = qcom_rimps_mbox_send_data,
	.shutdown = qcom_rimps_mbox_shutdown
};

static int qcom_rimps_ipc_setup_mbox(struct qcom_rimps_ipc *rimps_ipc)
{
	struct mbox_controller *mbox;
	struct device *dev = rimps_ipc->dev;
	unsigned long i;

	/* Initialize channel identifiers */
	for (i = 0; i < ARRAY_SIZE(rimps_ipc->chans); i++)
		rimps_ipc->chans[i].con_priv = NULL;

	mbox = &rimps_ipc->mbox;
	mbox->dev = dev;
	mbox->num_chans = rimps_ipc->num_chan;
	mbox->chans = rimps_ipc->chans;
	mbox->ops = &rimps_mbox_chan_ops;
	mbox->of_xlate = qcom_rimps_mbox_xlate;
	mbox->txdone_irq = false;
	mbox->txdone_poll = false;

	return mbox_controller_register(mbox);
}

static int qcom_rimps_probe(struct platform_device *pdev)
{
	struct qcom_rimps_ipc *rimps_ipc;
	struct resource *res;
	int ret;

	rimps_ipc = devm_kzalloc(&pdev->dev, sizeof(*rimps_ipc), GFP_KERNEL);
	if (!rimps_ipc)
		return -ENOMEM;

	rimps_ipc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get the device base address\n");
		return -ENODEV;
	}

	rimps_ipc->tx_irq_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!rimps_ipc->tx_irq_base) {
		dev_err(&pdev->dev, "Failed to ioremap the rimps tx irq addr\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get the device base address\n");
		return -ENODEV;
	}

	rimps_ipc->rx_irq_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!rimps_ipc->rx_irq_base) {
		dev_err(&pdev->dev, "Failed to ioremap the rimps rx irq addr\n");
		return -ENOMEM;
	}

	rimps_ipc->irq = platform_get_irq(pdev, 0);
	if (rimps_ipc->irq < 0) {
		dev_err(&pdev->dev, "Failed to get the IRQ\n");
		return rimps_ipc->irq;
	}

	rimps_ipc->num_chan = RIMPS_IPC_CHAN_SUPPORTED;
	ret = qcom_rimps_ipc_setup_mbox(rimps_ipc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create mailbox\n");
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, rimps_ipc->irq,
	qcom_rimps_rx_interrupt, IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, "qcom_rimps", rimps_ipc);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register the irq: %d\n", ret);
		goto err_mbox;
	}
	platform_set_drvdata(pdev, rimps_ipc);

	return 0;

err_mbox:
	mbox_controller_unregister(&rimps_ipc->mbox);
	return ret;
}

static int qcom_rimps_remove(struct platform_device *pdev)
{
	struct qcom_rimps_ipc *rimps_ipc = platform_get_drvdata(pdev);

	mbox_controller_unregister(&rimps_ipc->mbox);

	return 0;
}

static const struct of_device_id qcom_rimps_of_match[] = {
	{ .compatible = "qcom,rimps"},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_rimps_of_match);

static struct platform_driver qcom_rimps_driver = {
	.probe = qcom_rimps_probe,
	.remove = qcom_rimps_remove,
	.driver = {
		.name = "qcom_rimps",
		.of_match_table = qcom_rimps_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_rimps_init(void)
{
	int ret;

	ret = platform_driver_register(&qcom_rimps_driver);
	if (ret)
		pr_err("%s: qcom_rimps register failed %d\n", __func__, ret);
	return ret;
}
module_init(qcom_rimps_init);

static __exit void qcom_rimps_exit(void)
{
	platform_driver_unregister(&qcom_rimps_driver);
}
module_exit(qcom_rimps_exit);

MODULE_DESCRIPTION("QTI RIMPS Driver");
MODULE_LICENSE("GPL v2");
