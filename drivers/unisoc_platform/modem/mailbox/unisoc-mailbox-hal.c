// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spreadtrum mailbox driver
 *
 * Copyright (c) 2020 Spreadtrum Communications Inc.
 */
#include <linux/cpu_pm.h>
#include <linux/cdev.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "unisoc-mailbox.h"

#define SPRD_MBOX_TX_FIFO_LEN		64

#if defined(CONFIG_DEBUG_FS)
struct mbox_device {
	int			major;
	int			minor;
	struct cdev		cdev;
	struct device		*sys_dev;	/* Device object in sysfs */
};

static struct class *mbox_class;
static struct mbox_device *mbox_dev;
struct sprd_mbox_priv *mbox_priv;
#endif

struct sprd_mbox_tx_data {
	u64 fifo[SPRD_MBOX_TX_FIFO_LEN];
	u32 wt_cnt;
	u32 rd_cnt;
};

struct sprd_mbox_ctrl_reg {
	struct regmap *regmap;
	u32 addr;
	u32 mask;
};

static int chan_num;

static struct sprd_mbox_tx_data *tx_data;
static u8 mbox_tx_mask;

static struct task_struct *mbox_deliver_thread;
static wait_queue_head_t deliver_thread_wait;

static spinlock_t mbox_lock;
static spinlock_t outbox_lock;

static struct sprd_mbox_ctrl_reg reg_ctrl;
const struct sprd_mbox_phy_ops *mbox_phy_ops;

#if defined(CONFIG_DEBUG_FS)
static int sprd_mbox_debug_open(struct inode *inode, struct file *file)
{
	if (mbox_priv)
		return single_open(file, mbox_priv->phy_ops->debugfs_show, inode->i_private);
	else
		return -EINVAL;
}

static const struct file_operations sprd_mbox_debug_ops = {
	.open = sprd_mbox_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_mbox_debug(void)
{
	int rval;
	dev_t dev_no;

	mbox_dev = kzalloc(sizeof(struct mbox_device), GFP_KERNEL);
	if (!mbox_dev)
		return -ENOMEM;
	rval = alloc_chrdev_region(&dev_no, 0, 1, "mbox_debug");
	if (rval) {
		pr_err("Failed to alloc chrdev region\n");
		goto free_mbox_dev;
	}
	mbox_dev->major = MAJOR(dev_no);
	mbox_dev->minor = MINOR(dev_no);
	cdev_init(&mbox_dev->cdev, &sprd_mbox_debug_ops);
	rval = cdev_add(&mbox_dev->cdev, dev_no, 1);
	if (rval) {
		pr_err("Failed to add mbox cdev\n");
		goto free_devno;
	}
	mbox_class = class_create(THIS_MODULE, "mbox");
	if (IS_ERR(mbox_class)) {
		rval = PTR_ERR(mbox_class);
		pr_err("Failed to create mbox class\n");
		goto free_cdev;
	}
	mbox_dev->sys_dev = device_create(mbox_class, NULL,
				       dev_no,
				       mbox_dev, "mbox");

	return 0;

free_cdev:
	cdev_del(&mbox_dev->cdev);

free_devno:
	unregister_chrdev_region(dev_no, 1);

free_mbox_dev:
	kfree(mbox_dev);
	mbox_dev = NULL;
	return rval;
}

static void mbox_destroy_debug(void)
{
	if (mbox_dev) {
		dev_t dev_no = MKDEV(mbox_dev->major, mbox_dev->minor);

		if (mbox_dev->sys_dev) {
			device_destroy(mbox_class, dev_no);
			mbox_dev->sys_dev = NULL;
		}

		if (!IS_ERR(mbox_class))
			class_destroy(mbox_class);

		unregister_chrdev_region(dev_no, 1);
		cdev_del(&mbox_dev->cdev);
		kfree(mbox_dev);
		mbox_dev = NULL;
	}
}
#endif /* CONFIG_DEBUG_FS */

static struct mbox_chan *sprd_mbox_index_xlate(
		struct mbox_controller *mbox, const struct of_phandle_args *sp)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(mbox);
	int ind = sp->args[0];
	int is_oob = sp->args[1];

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	if (is_oob) {
		dev_dbg(priv->dev, "obb chan num = %d\n", ind);
		priv->oob_outbox_base = priv->outbox_base +
			(priv->mbox_span * ind);
		priv->oob_id = ind;
	}

	return &mbox->chans[ind];
}

static irqreturn_t sprd_mbox_outbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;
	irqreturn_t ret;

	spin_lock(&outbox_lock);
	ret = priv->phy_ops->outbox_isr(irq, data);
	spin_unlock(&outbox_lock);

	return ret;
}

u8 get_tx_fifo_mask(u8 deliver_bit)
{
	u8 dst_bit, mask = 0;
	u32 id;

	spin_lock(&mbox_lock);

	for (id = 0; id < chan_num; id++) {
		dst_bit = (1 << id);
		if (deliver_bit & dst_bit) {
			if (tx_data[id].wt_cnt != tx_data[id].rd_cnt)
				mask |= (dst_bit);
		}
	}

	spin_unlock(&mbox_lock);

	return mask;
}

static irqreturn_t sprd_mbox_inbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;

	return priv->phy_ops->inbox_isr(irq, data);
}

static void put_msg_in_tx_fifo(unsigned long id, void *msg)
{
	u32 pos, wt, rd;

	pr_debug("put msg in tx_fifo\n");
	wt = tx_data[id].wt_cnt;
	rd = tx_data[id].rd_cnt;
	pos = wt % SPRD_MBOX_TX_FIFO_LEN;

	if ((rd != wt) && (rd % SPRD_MBOX_TX_FIFO_LEN == pos)) {
		pr_err("tx_fifo full, drop msg, dst = %ld, rd =%d, wt = %d\n",
		       id, rd, wt);
		return;
	}

	tx_data[id].fifo[pos] = *(u64 *)msg;
	tx_data[id].wt_cnt++;
}

void mbox_start_send_tx_fifo(u8 msk)
{
	mbox_tx_mask = msk;
	pr_debug("wake up deliver thread\n");
	wake_up(&deliver_thread_wait);
}

static int sprd_mbox_deliver_thread(void *pdata)
{
	struct sprd_mbox_priv *priv = pdata;
	struct mbox_chan *chan;
	unsigned long flag;
	u32 dst, rd, pos;
	u64 msg;
	int ret;

	while (!kthread_should_stop()) {
		dev_dbg(priv->dev, "tx_fifo waiting deliver event...\n");
		ret = wait_event_interruptible(deliver_thread_wait, mbox_tx_mask);
		if (ret) {
			dev_warn(priv->dev, "mbox_deliver_thread is interrupted\n");
			continue;
		}
		/* Event triggered, process all tx_fifo. */
		spin_lock_irqsave(&mbox_lock, flag);

		for (dst = 0; dst < chan_num; dst++) {
			if (!(mbox_tx_mask & (1 << dst)))
				continue;
			chan = &priv->mbox.chans[dst];
			rd = tx_data[dst].rd_cnt;
			while (tx_data[dst].wt_cnt != rd) {
				pos = rd % SPRD_MBOX_TX_FIFO_LEN;
				msg = tx_data[dst].fifo[pos];
				if (priv->phy_ops->send(chan, &msg))
					/* Send failed, than next chan */
					break;
				tx_data[dst].rd_cnt = ++rd;
			}
		}
		mbox_tx_mask = 0;
		spin_unlock_irqrestore(&mbox_lock, flag);
	}

	return 0;
}

static int sprd_mbox_send_data(struct mbox_chan *chan, void *msg)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long flag, core_id = (unsigned long)chan->con_priv;

	spin_lock_irqsave(&mbox_lock, flag);

	/* If tx_fifo is not empty, do not send, put msg in tx_fifo */
	if (tx_data[core_id].rd_cnt != tx_data[core_id].wt_cnt) {
		put_msg_in_tx_fifo(core_id, msg);
	/* Try send, if fail, put msg in tx_fifo */
	} else if (priv->phy_ops->send(chan, msg)) {
		put_msg_in_tx_fifo(core_id, msg);
	}
	spin_unlock_irqrestore(&mbox_lock, flag);

	return 0;
}

static int sprd_mbox_flush(struct mbox_chan *chan, unsigned long timeout)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);

	dev_info(priv->dev, "flush chan%ld\n", (unsigned long)chan->con_priv);
	return priv->phy_ops->flush(chan, timeout);
}

static int sprd_mbox_startup(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);

	dev_info(priv->dev, "startup chan-%ld\n", (unsigned long)chan->con_priv);
	priv->phy_ops->startup(chan);

	return 0;
}

static void sprd_mbox_shutdown(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);

#if defined(CONFIG_DEBUG_FS)
	mbox_destroy_debug();
#endif
	dev_info(priv->dev, "shutdown chan-%ld\n", (unsigned long)chan->con_priv);
	priv->phy_ops->shutdown(chan);
}

static const struct mbox_chan_ops sprd_mbox_ops = {
	.send_data	= sprd_mbox_send_data,
	.flush		= sprd_mbox_flush,
	.startup	= sprd_mbox_startup,
	.shutdown	= sprd_mbox_shutdown,
};

static void sprd_mbox_disable(void *data)
{
	struct sprd_mbox_priv *priv = data;

	if (reg_ctrl.regmap == NULL)
		return;
	regmap_update_bits(reg_ctrl.regmap, reg_ctrl.addr, reg_ctrl.mask, 0);
	dev_info(priv->dev, "mailbox disabled.\n");
}

static int sprd_mbox_request_irq(struct sprd_mbox_priv *priv)
{
	struct device *dev = priv->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	/* Requeset IRQ for inbox */
	priv->inbox_irq = platform_get_irq_byname(pdev, "inbox");
	if (priv->inbox_irq < 0)
		return priv->inbox_irq;

	ret = devm_request_irq(dev, priv->inbox_irq, sprd_mbox_inbox_isr,
				   IRQF_NO_SUSPEND, dev_name(dev), priv);
	if (ret) {
		dev_err(dev, "failed to request inbox IRQ: %d\n", ret);
		return ret;
	}

	/* Requeset IRQ for outbox */
	priv->outbox_irq = platform_get_irq_byname(pdev, "outbox");
	if (priv->outbox_irq < 0)
		return priv->outbox_irq;

	ret = devm_request_irq(dev, priv->outbox_irq, sprd_mbox_outbox_isr,
				   IRQF_NO_SUSPEND, dev_name(dev), priv);
	if (ret) {
		dev_err(dev, "failed to request outbox IRQ: %d\n", ret);
		return ret;
	}

	/* Requeset IRQ for out-of-band outbox for no-wakeup-sensor */
	priv->oob_irq = platform_get_irq_byname(pdev, "oob-outbox");
	if (priv->oob_irq < 0)
		dev_warn(dev, "failed to get oob_irq\n");
	else {
		ret = devm_request_irq(dev, priv->oob_irq,
			sprd_mbox_outbox_isr, 0, dev_name(dev), priv);
		if (ret)
			dev_err(dev, "failed to request oob_irq: %d\n", ret);
	}

	return 0;
}

static int mbox_pm_notifier(struct notifier_block *self,
			    unsigned long cmd, void *v)
{
	if (cmd == CPU_CLUSTER_PM_EXIT && mbox_phy_ops->outbox_has_irq()) {
#if defined CONFIG_UNISOC_MAILBOX_R1
		sprd_mbox_change_wakeup_flag_r1(true);
#elif defined CONFIG_UNISOC_MAILBOX_R2
		sprd_mbox_change_wakeup_flag_r2(true);
#endif
	}

	return NOTIFY_OK;
}

static struct notifier_block mbox_pm_notifier_block = {
	.notifier_call = mbox_pm_notifier,
};

static int sprd_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sprd_mbox_priv *priv;
	struct regmap *regmap = NULL;
	struct device_node *np = pdev->dev.of_node;
	int ret;
	unsigned long id;
	u32 out_args[2];

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	regmap = syscon_regmap_lookup_by_phandle_args(np, "sprd,mailbox_clk", 2, out_args);
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to get mailbox clock\n");
		return -EINVAL;
	}
	regmap_update_bits(regmap, out_args[0], out_args[1], out_args[1]);
	reg_ctrl.regmap = regmap;
	reg_ctrl.addr = out_args[0];
	reg_ctrl.mask = out_args[1];

#if defined CONFIG_UNISOC_MAILBOX_R1
	ret = sprd_mbox_phy_r1_init(priv);
#elif defined CONFIG_UNISOC_MAILBOX_R2
	ret = sprd_mbox_phy_r2_init(priv);
#else
	dev_err(dev, "There is no phy config!\n");
	return -ENODEV;
#endif
	if (ret)
		return ret;

	mbox_phy_ops = priv->phy_ops;
	chan_num = priv->mbox.num_chans;
	priv->mbox.dev = dev;
	priv->mbox.ops = &sprd_mbox_ops;
	priv->mbox.txdone_irq = true;
	priv->mbox.of_xlate = sprd_mbox_index_xlate;
	priv->mbox.chans = devm_kzalloc(dev, sizeof(struct mbox_chan) * chan_num, GFP_KERNEL);
	if (!priv->mbox.chans)
		return -ENOMEM;

	for (id = 0; id < chan_num; id++)
		priv->mbox.chans[id].con_priv = (void *)id;

	/* To avoid triggering outbox and obb-outbox IRQ at the same time */
	spin_lock_init(&outbox_lock);
	spin_lock_init(&mbox_lock);
	init_waitqueue_head(&deliver_thread_wait);

	/* Request mailbox IRQ */
	ret = sprd_mbox_request_irq(priv);
	if (ret) {
		dev_err(dev, "failed to request IRQ\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev, sprd_mbox_disable, priv);
	if (ret) {
		dev_err(dev, "failed to add mailbox disable action\n");
		return ret;
	}

	ret = devm_mbox_controller_register(dev, &priv->mbox);
	if (ret) {
		dev_err(dev, "failed to register mailbox: %d\n", ret);
		return ret;
	}

	tx_data = devm_kzalloc(dev, sizeof(struct sprd_mbox_tx_data) * chan_num, GFP_KERNEL);
	if (!tx_data)
		return -ENOMEM;
	ret = cpu_pm_register_notifier(&mbox_pm_notifier_block);
	if (ret) {
		dev_err(dev, "failed to register cpu pm notifier\n");
		return ret;
	}
	/* Create a deliver thread for waiting if tx_fifo can be delivered */
	mbox_deliver_thread = kthread_create(sprd_mbox_deliver_thread,
		priv, "mbox-deliver-thread");
	wake_up_process(mbox_deliver_thread);

#if defined(CONFIG_DEBUG_FS)
	mbox_priv = priv;
	ret = sprd_mbox_debug();
	if (ret)
		dev_err(dev, "failed to create mbox debug! ret = %d\n", ret);
#endif
	dev_info(priv->dev, "probe done, version=0x%x\n", priv->version);

	return 0;
}

static const struct of_device_id sprd_mbox_of_match[] = {
	{ .compatible = "unisoc,mailbox", },
	{ },
};
MODULE_DEVICE_TABLE(of, sprd_mbox_of_match);

static struct platform_driver sprd_mbox_driver = {
	.driver = {
		.name = "unisoc-mailbox",
		.of_match_table = sprd_mbox_of_match,
	},
	.probe	= sprd_mbox_probe,
};

static int __init sprd_mbox_init(void)
{
	return platform_driver_register(&sprd_mbox_driver);
}
subsys_initcall(sprd_mbox_init);

MODULE_AUTHOR("Magnum.Shan <magnum.shan@unisoc.com>");
MODULE_DESCRIPTION("Unisoc mailbox driver");
MODULE_LICENSE("GPL v2");
