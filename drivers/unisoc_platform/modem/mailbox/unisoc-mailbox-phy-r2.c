// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spreadtrum mailbox driver
 *
 * Copyright (c) 2020 Spreadtrum Communications Inc.
 */
#include <linux/suspend.h>
#include "unisoc-mailbox.h"

#define SPRD_MBOX_ID		0x0
#define SPRD_MBOX_MSG_LOW	0x4
#define SPRD_MBOX_MSG_HIGH	0x8
#define SPRD_MBOX_TRIGGER	0xc
#define SPRD_MBOX_FIFO_RST	0x10
#define SPRD_INBOX_FIFO_STS1	0x14
#define SPRD_OUTBOX_FIFO_STS1	0x14
#define SPRD_MBOX_IRQ_STS	0x18
#define SPRD_MBOX_IRQ_MSK	0x1c
#define SPRD_MBOX_LOCK		0x20
#define SPRD_INBOX_FIFO_STS2	0x24
#define SPRD_OUTBOX_FIFO_DEPTH	0x24
#define SPRD_OUTBOX_FIFO_STS2	0x28
#define SPRD_MBOX_VERSION	0x8


/* Bit and mask definiation for inbox's SPRD_MBOX_FIFO_RST register */
#define SPRD_INBOX_FIFO_OVERLOW_DELIVER_RST		GENMASK(31, 0)

/* Bit and mask definiation for inbox's SPRD_MBOX_FIFO_STS1 register */
#define SPRD_INBOX_FIFO_RECV_MASK		GENMASK(31, 16)
#define SPRD_INBOX_FIFO_RECV_SHIFT		16
#define SPRD_INBOX_FIFO_DELIVER_MASK		GENMASK(15, 0)

/* Bit and mask definiation for outbox's SPRD_MBOX_FIFO_STS1 register */
#define SPRD_OUTBOX_FIFO_FULL			BIT(2)
#define SPRD_OUTBOX_FIFO_WR_SHIFT		16
#define SPRD_OUTBOX_FIFO_RD_SHIFT		24
#define SPRD_OUTBOX_FIFO_POS_MASK		GENMASK(7, 0)

/* Bit and mask definiation for SPRD_MBOX_IRQ_STS register */
#define SPRD_OUTBOX_SCR_FIFO_WR_IRQ		GENMASK(16, 0)
#define SPRD_OUTBOX_SCR_FIFO_IRQ		BIT(16)
#define SPRD_INBOX_SCR_FIFO_IRQ			BIT(0)

/* Bit and mask definiation for inbox's SPRD_MBOX_IRQ_MSK register */
#define SPRD_INBOX_FIFO_BLOCK_IRQ		BIT(0)
#define SPRD_INBOX_FIFO_OVERFLOW_IRQ		BIT(1)
#define SPRD_INBOX_FIFO_DELIVER_IRQ		BIT(2)
#define SPRD_INBOX_FIFO_IRQ_MASK		GENMASK(2, 0)

/* Bit and mask definiation for outbox's SPRD_MBOX_IRQ_MSK register */
#define SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ		BIT(0)
#define SPRD_OUTBOX_FIFO_IRQ_MASK		GENMASK(4, 0)
#define SPRD_OUTBOX_DISABLE_ALL_IRQ		GENMASK(31, 0)

/* Bit and mask definiation for inbox's SPRD_MBOX_FIFO_STS2 register */
#define SPRD_INBOX_FIFO_OVERLOW_MASK		GENMASK(31, 16)
#define SPRD_INBOX_FIFO_OVERLOW_SHIFT		16
#define SPRD_INBOX_FIFO_BLOCK_MASK		GENMASK(15, 0)

/* Bit and mask definiation for SPRD_MBOX_VERSION register */
#define SPRD_MBOX_VERSION_MASK		GENMASK(15, 0)

#define MBOX_THIS_CHAN		0
#define SPRD_MBOX_SPAN		0x1000
#define SPRD_INBOX_RECV_TIMEOUT	1000
#define SPRD_MBOX_CHAN_MAX		16
#define SPRD_MBOX_RX_FIFO_LEN	64

struct  sprd_mbox_data {
	unsigned long core_id;
	u64 msg;
};

static struct  sprd_mbox_data mbox_rx_fifo[SPRD_MBOX_RX_FIFO_LEN];
static u32 mbox_rx_fifo_cnt;
static void __iomem *sprd_outbox_base;
static void (*mbox_wakeup_flag_callback_r2)(bool wakeup_flag);

static void __iomem *g_mbox_nowakeup_base;
static u32 g_irq_msk_backup;

static u32 g_started_chan_msk;

#if defined(CONFIG_DEBUG_FS)
static void sprd_mbox_debug_putline(struct seq_file *m, char c, int n)
{
	int i;

	for (i = 0; i < n; i++)
		seq_putc(m, c);

	seq_putc(m, '\n');
}

static int sprd_outbox_reg_read(struct sprd_mbox_priv *priv, int index, int offset)
{
	return readl(priv->outbox_base +
				priv->mbox_span * index + offset);
}

static int sprd_inbox_reg_read(struct sprd_mbox_priv *priv, int index, int offset)
{
	return readl(priv->inbox_base + priv->mbox_span * index + offset);
}

static int sprd_mbox_debug_show(struct seq_file *m, void *private)
{
	struct sprd_mbox_priv *priv = mbox_priv;
	int i;

	if (mbox_priv == NULL)
		return -EINVAL;

	sprd_mbox_debug_putline(m, '*', 110);
	seq_printf(m, "CHAN_NUM : %d\n", priv->mbox.num_chans);
	seq_printf(m, "INBOX_IRQ : %d\n", priv->inbox_irq);
	seq_printf(m, "OUTBOX_IRQ : %d\n", priv->outbox_irq);
	seq_printf(m, "OOB_IRQ : %d\n", priv->oob_irq);
	seq_printf(m, "OOB_ID : %ld\n", priv->oob_id);
	seq_printf(m, "VERSION : 0x%x\n", priv->version);
	seq_printf(m, "mbox_rx_fifo_cnt : %d\n", mbox_rx_fifo_cnt);
	seq_printf(m, "started_chan_msk : 0x%04x\n", g_started_chan_msk);

	for (i = 0; i < SPRD_MBOX_CHAN_MAX; i++) {

		sprd_mbox_debug_putline(m, '*', 110);
		seq_printf(m, "[INBOX %d]\n"
				"\t id: 0x%08x | msg_low: 0x%08x | msg_high:  0x%08x\n"
				"\t fifo_reset: 0x%08x | fifo_status1: 0x%08x | irq_status: 0x%08x | irq_mask: 0x%08x\n"
				"\t fifo_status2: 0x%08x\n",
			i,
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_ID),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_MSG_LOW),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_MSG_HIGH),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_FIFO_RST),
			sprd_inbox_reg_read(priv, i, SPRD_INBOX_FIFO_STS1),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_IRQ_STS),
			sprd_inbox_reg_read(priv, i, SPRD_MBOX_IRQ_MSK),
			sprd_inbox_reg_read(priv, i, SPRD_INBOX_FIFO_STS2));

		sprd_mbox_debug_putline(m, '-', 100);

		seq_printf(m, "[OUTBOX %d]\n"
				"\t id: 0x%08x | msg_low: 0x%08x | msg_high:  0x%08x\n"
				"\t fifo_reset: 0x%08x | fifo_status1: 0x%08x | irq_status: 0x%08x | irq_mask: 0x%08x\n"
				"\t depth: 0x%08x | fifo_status2: 0x%08x\n",
			i,
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_ID),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_MSG_LOW),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_MSG_HIGH),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_FIFO_RST),
			sprd_outbox_reg_read(priv, i, SPRD_OUTBOX_FIFO_STS1),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_IRQ_STS),
			sprd_outbox_reg_read(priv, i, SPRD_MBOX_IRQ_MSK),
			sprd_outbox_reg_read(priv, i, SPRD_OUTBOX_FIFO_DEPTH),
			sprd_outbox_reg_read(priv, i, SPRD_OUTBOX_FIFO_STS2));
	}

	sprd_mbox_debug_putline(m, '*', 110);

	return 0;
}
#endif /* CONFIG_DEBUG_FS */

void sprd_mbox_wakeup_flag_callback_register_r2(void (*callback)(bool wakeup_flag))
{
	mbox_wakeup_flag_callback_r2 = callback;
}
EXPORT_SYMBOL_GPL(sprd_mbox_wakeup_flag_callback_register_r2);

void sprd_mbox_wakeup_flag_callback_unregister_r2(void)
{
	mbox_wakeup_flag_callback_r2 = NULL;
}
EXPORT_SYMBOL_GPL(sprd_mbox_wakeup_flag_callback_unregister_r2);

void sprd_mbox_change_wakeup_flag_r2(bool wakeup_flag)
{
	if (mbox_wakeup_flag_callback_r2)
		mbox_wakeup_flag_callback_r2(wakeup_flag);
	else
		pr_err("mbox wakeup callback is null\n");
}
EXPORT_SYMBOL_GPL(sprd_mbox_change_wakeup_flag_r2);

static u32 sprd_mbox_get_fifo_len(struct sprd_mbox_priv *priv, u32 fifo_sts)
{
	u32 wr_pos = (fifo_sts >> SPRD_OUTBOX_FIFO_WR_SHIFT) &
		SPRD_OUTBOX_FIFO_POS_MASK;
	u32 rd_pos = (fifo_sts >> SPRD_OUTBOX_FIFO_RD_SHIFT) &
		SPRD_OUTBOX_FIFO_POS_MASK;
	u32 fifo_len;

	/*
	 * If the read pointer is equal with write pointer, which means the fifo
	 * is full or empty.
	 */
	if (wr_pos == rd_pos) {
		if (fifo_sts & SPRD_OUTBOX_FIFO_FULL)
			fifo_len = priv->outbox_fifo_depth;
		else
			fifo_len = 0;
	} else if (wr_pos > rd_pos) {
		fifo_len = wr_pos - rd_pos;
	} else {
		fifo_len = priv->outbox_fifo_depth - rd_pos + wr_pos;
	}

	return fifo_len;
}

static irqreturn_t sprd_mbox_phy_outbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;
	struct mbox_chan *chan;
	u32 fifo_sts, irq_sts, fifo_len, msg[2];
	void __iomem *base;
	int i, id;

	if (irq == priv->oob_irq)
		base = priv->oob_outbox_base;
	else
		/* in-bounds isr */
		base = priv->outbox_base;

	fifo_sts = readl(base + SPRD_OUTBOX_FIFO_STS1);

	fifo_len = sprd_mbox_get_fifo_len(priv, fifo_sts);
	if (!fifo_len)
		dev_warn_ratelimited(priv->dev, "spurious outbox interrupt\n");

	irq_sts = readl(base + SPRD_MBOX_IRQ_STS);
	irq_sts &= SPRD_OUTBOX_SCR_FIFO_WR_IRQ;

	for (i = 0; i < fifo_len; i++) {
		msg[0] = readl(base + SPRD_MBOX_MSG_LOW);
		msg[1] = readl(base + SPRD_MBOX_MSG_HIGH);
		if (irq == priv->oob_irq)
			id = priv->oob_id;
		else
			id = readl(base + SPRD_MBOX_ID);

		chan = &priv->mbox.chans[id];
		if (chan->cl)
			mbox_chan_received_data(chan, (void *)msg);
		else if (mbox_rx_fifo_cnt >= SPRD_MBOX_RX_FIFO_LEN)
			dev_err(priv->dev, "mbox_mbox_rx_fifo full, core_id = %d\n", id);
		else {
			/* Put message in a mbox_rx_fifo if client is not ready */
			dev_dbg(priv->dev, "chan%d client is not ready, put msg in rx_fifo\n", id);
			mbox_rx_fifo[mbox_rx_fifo_cnt].core_id = id;
			mbox_rx_fifo[mbox_rx_fifo_cnt].msg = *(u64 *)msg;
			mbox_rx_fifo_cnt++;
		}

		/* Trigger to update outbox FIFO pointer */
		writel(0x1, base + SPRD_MBOX_TRIGGER);
	}

	sprd_mbox_change_wakeup_flag_r2(false);

	/* Clear irq status after reading all message. */
	writel(irq_sts, base + SPRD_MBOX_IRQ_STS);

	return IRQ_HANDLED;
}

static irqreturn_t sprd_mbox_phy_inbox_isr(int irq, void *data)
{
	struct sprd_mbox_priv *priv = data;
	u32 fifo_sts1, fifo_sts2, block, deliver, irq_msk;
	u8 tx_mask;

	fifo_sts1 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS1);
	fifo_sts2 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS2);
	dev_dbg(priv->dev, "fifo_sts1=%x, fifo_sts2=%x\n", fifo_sts1, fifo_sts2);


	/* Clear FIFO delivery and overflow status */
	writel(SPRD_INBOX_FIFO_OVERLOW_DELIVER_RST,
	       priv->inbox_base + SPRD_MBOX_FIFO_RST);

	deliver = fifo_sts1 & SPRD_INBOX_FIFO_DELIVER_MASK;
	block = fifo_sts2 & SPRD_INBOX_FIFO_BLOCK_MASK;
	irq_msk = readl(priv->inbox_base + SPRD_MBOX_IRQ_MSK) &
			SPRD_INBOX_FIFO_IRQ_MASK;

	/* When deliver success, and corresponding tx_fifo is not empty, start send tx_fifo */
	tx_mask = get_tx_fifo_mask(deliver & (~block));
	dev_dbg(priv->dev, "tx_mask = 0x%x\n", tx_mask);
	if (tx_mask)
		mbox_start_send_tx_fifo(tx_mask);

	/* If block, enable inbox deliver irq, if not, enable inbox block & overflow irq */
	dev_dbg(priv->dev, "irq_msk = 0x%x\n", irq_msk);
	if (block)
		irq_msk = ~(u32)SPRD_INBOX_FIFO_DELIVER_IRQ;
	else
		irq_msk = ~(u32)(SPRD_INBOX_FIFO_BLOCK_IRQ | SPRD_INBOX_FIFO_OVERFLOW_IRQ);
	irq_msk &= SPRD_INBOX_FIFO_IRQ_MASK;
	writel(irq_msk, priv->inbox_base + SPRD_MBOX_IRQ_MSK);

	/* Clear irq status */
	writel(SPRD_INBOX_SCR_FIFO_IRQ, priv->inbox_base + SPRD_MBOX_IRQ_STS);

	return IRQ_HANDLED;
}

static int check_mbox_chan_state(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 fifo_sts1, fifo_sts2, recv_flag, cnt = 0;

	fifo_sts1 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS1);
	fifo_sts2 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS2);
	if (fifo_sts2 & (1 << id) & SPRD_INBOX_FIFO_BLOCK_MASK)
		return -ENOBUFS;

	/* Wait outbox recv flag, until flag is 0 */
	recv_flag = 1 << (id + SPRD_INBOX_FIFO_RECV_SHIFT);
	while (fifo_sts1 & recv_flag) {
		cnt++;
		fifo_sts1 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS1);
		fifo_sts2 = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS2);
		/* Recv flag will always be 1 when block */
		if (fifo_sts2 & (1 << id) & SPRD_INBOX_FIFO_BLOCK_MASK)
			return -ENOBUFS;
		if (cnt >= SPRD_INBOX_RECV_TIMEOUT)
			return -ETIMEDOUT;
	}
	return 0;
}

static bool mbox_phy_outbox_has_irq(void)
{
	u32 irq_sts;

	irq_sts = readl(sprd_outbox_base + SPRD_MBOX_IRQ_STS);

	return (irq_sts & SPRD_OUTBOX_SCR_FIFO_IRQ);
}

static int sprd_mbox_phy_send(struct mbox_chan *chan, void *msg)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 *data = msg;
	int ret;

	ret = check_mbox_chan_state(chan);
	if (ret) {
		dev_dbg(priv->dev, "chan %ld is blocked\n", id);
		return ret;
	}

	/* Write data into inbox FIFO, and only support 8 bytes every time */
	writel(data[0], priv->inbox_base + SPRD_MBOX_MSG_LOW);
	writel(data[1], priv->inbox_base + SPRD_MBOX_MSG_HIGH);
	/* Set target core id */
	writel(id, priv->inbox_base + SPRD_MBOX_ID);
	/* Trigger remote request */
	writel(0x1, priv->inbox_base + SPRD_MBOX_TRIGGER);

	return 0;
}

static int mbox_pm_event(struct notifier_block *notifier,
				unsigned long pm_event, void *unused)
{
	void __iomem *base = g_mbox_nowakeup_base;

	if (!base)
		return NOTIFY_DONE;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_debug("PM_SUSPEND_PREPARE\n");
		g_irq_msk_backup = readl(base + SPRD_MBOX_IRQ_MSK);
		/* Disable nowakeup irq to prevent the suspend process from being interrupted */
		writel_relaxed(SPRD_OUTBOX_DISABLE_ALL_IRQ,
					  base + SPRD_MBOX_IRQ_MSK);
		break;
	case PM_POST_SUSPEND:
		pr_debug("PM_POST_SUSPEND\n");
		/* Restore nowakeup irq when suspend finish */
		writel_relaxed(g_irq_msk_backup,
					  base + SPRD_MBOX_IRQ_MSK);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mbox_pm_notifier_block = {
	.notifier_call = mbox_pm_event,
};

static int sprd_mbox_phy_flush(struct mbox_chan *chan, unsigned long timeout)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 busy;

	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		busy = readl(priv->inbox_base + SPRD_INBOX_FIFO_STS2) &
			SPRD_INBOX_FIFO_BLOCK_MASK;
		if (!(busy & BIT(id))) {
			mbox_chan_txdone(chan, 0);
			return 0;
		}

		udelay(1);
	}

	return -ETIME;
}

static void sprd_mbox_process_rx_fifo(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id, target_id = (unsigned long)chan->con_priv;
	int i, cnt = 0;

	for (i = 0; i < mbox_rx_fifo_cnt; i++) {
		id = mbox_rx_fifo[i].core_id;
		/* has been procced */
		if (id == SPRD_MBOX_CHAN_MAX) {
			cnt++;
			continue;
		}
		if (chan->cl && (id == target_id)) {
			dev_dbg(priv->dev, "chan%ld, receive data[%d] from rx_fifo\n", id, i);
			mbox_chan_received_data(chan, &mbox_rx_fifo[i].msg);
			mbox_rx_fifo[i].core_id = SPRD_MBOX_CHAN_MAX;
			cnt++;
		}
	}
	/* reset mbox_mbox_rx_fifo_cnt*/
	if (cnt == mbox_rx_fifo_cnt)
		mbox_rx_fifo_cnt = 0;
}

static int sprd_mbox_phy_startup(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;
	u32 val;

	if (!(priv->started++)) {
		/* Select outbox FIFO mode and reset the outbox FIFO status */
		writel(0x0, priv->outbox_base + SPRD_MBOX_FIFO_RST);

		/* Enable inbox FIFO block and overflow, disable deliver irq */
		val = readl(priv->inbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~(SPRD_INBOX_FIFO_BLOCK_IRQ | SPRD_INBOX_FIFO_OVERFLOW_IRQ);
		val |= SPRD_INBOX_FIFO_DELIVER_IRQ;
		writel(val, priv->inbox_base + SPRD_MBOX_IRQ_MSK);

		/* Enable outbox FIFO not empty interrupt */
		val = readl(priv->outbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ;
		writel(val, priv->outbox_base + SPRD_MBOX_IRQ_MSK);

		g_started_chan_msk |= (1 << MBOX_THIS_CHAN);
	}

	if (priv->oob_outbox_base && !priv->oob_started) {
		/* Select outbox FIFO mode and reset the outbox FIFO status */
		writel(0x0, priv->oob_outbox_base + SPRD_MBOX_FIFO_RST);

		/* Enable outbox FIFO not empty interrupt */
		val = readl(priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);
		val &= ~SPRD_OUTBOX_FIFO_NOT_EMPTY_IRQ;
		writel(val, priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);

		g_mbox_nowakeup_base = priv->oob_outbox_base;

		priv->oob_started = 1;
	}

	/* Process messages received before startup */
	sprd_mbox_process_rx_fifo(chan);

	g_started_chan_msk |= (1 << id);

	return 0;
}

static void sprd_mbox_phy_shutdown(struct mbox_chan *chan)
{
	struct sprd_mbox_priv *priv = to_sprd_mbox_priv(chan->mbox);
	unsigned long id = (unsigned long)chan->con_priv;

	if (!(--priv->started)) {
		/* Disable inbox & outbox interrupt */
		writel(SPRD_INBOX_FIFO_IRQ_MASK, priv->inbox_base + SPRD_MBOX_IRQ_MSK);
		writel(SPRD_OUTBOX_FIFO_IRQ_MASK, priv->outbox_base + SPRD_MBOX_IRQ_MSK);

		g_started_chan_msk &= ~(1 << MBOX_THIS_CHAN);
	}

	if ((priv->oob_id == (unsigned long)chan->con_priv) && priv->oob_started) {
		writel(SPRD_OUTBOX_FIFO_IRQ_MASK,
			priv->oob_outbox_base + SPRD_MBOX_IRQ_MSK);
		priv->oob_started = 0;
	}

	g_started_chan_msk &= ~(1 << id);
}

static const struct sprd_mbox_phy_ops sprd_mbox_phy_ops_r2 = {
	.startup = sprd_mbox_phy_startup,
	.shutdown = sprd_mbox_phy_shutdown,
	.flush = sprd_mbox_phy_flush,
	.send = sprd_mbox_phy_send,
	.inbox_isr = sprd_mbox_phy_inbox_isr,
	.outbox_isr = sprd_mbox_phy_outbox_isr,
	.outbox_has_irq = mbox_phy_outbox_has_irq,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_show = sprd_mbox_debug_show,
#endif
};

int sprd_mbox_phy_r2_init(struct sprd_mbox_priv *priv)
{
	struct platform_device *pdev = to_platform_device(priv->dev);

	/* Get inbox_base & outbox_base from dts */
	priv->inbox_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->inbox_base))
		return PTR_ERR(priv->inbox_base);

	priv->outbox_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(priv->outbox_base))
		return PTR_ERR(priv->outbox_base);
	sprd_outbox_base = priv->outbox_base;

	priv->common_base = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(priv->common_base))
		return PTR_ERR(priv->common_base);

	/* Get the default outbox FIFO depth */
	priv->outbox_fifo_depth =
		readl(priv->outbox_base + SPRD_OUTBOX_FIFO_DEPTH) + 1;
	priv->mbox.num_chans = SPRD_MBOX_CHAN_MAX;
	priv->mbox_span = SPRD_MBOX_SPAN;
	priv->version = readl(priv->common_base + SPRD_MBOX_VERSION) &
		SPRD_MBOX_VERSION_MASK;
	priv->phy_ops = &sprd_mbox_phy_ops_r2;

	if (register_pm_notifier(&mbox_pm_notifier_block))
		dev_err(priv->dev, "failed to register pm notifier\n");

	return 0;
}

MODULE_AUTHOR("Magnum Shan <magnum.shan@unisoc.com>");
MODULE_DESCRIPTION("Unisoc mailbox driver");
MODULE_LICENSE("GPL v2");
