/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc mailbox driver
 *
 * Copyright (c) 2020 Spreadtrum Communications Inc.
 */
 #ifndef _UNISOC_MAILBOX_H
#define _UNISOC_MAILBOX_H

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "unisoc-mailbox " fmt

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

struct sprd_mbox_phy_ops {
	int (*startup)(struct mbox_chan *chan);
	void (*shutdown)(struct mbox_chan *chan);
	int (*flush)(struct mbox_chan *chan, unsigned long timeout);
	int (*send)(struct mbox_chan *chan, void *msg);
	irqreturn_t (*inbox_isr)(int irq, void *data);
	irqreturn_t (*outbox_isr)(int irq, void *data);
	bool (*outbox_has_irq)(void);
#if defined(CONFIG_DEBUG_FS)
	int (*debugfs_show)(struct seq_file *m, void *private);
#endif
};

struct sprd_mbox_priv {
	struct mbox_controller	mbox;
	struct device	*dev;
	void __iomem	*inbox_base;
	void __iomem	*outbox_base;
	void __iomem	*common_base;
	struct clk	*clk;
	u32		outbox_fifo_depth;
	u32		mbox_span;
	u32		version;

	int		inbox_irq;
	int		outbox_irq;
	u8		started;

	/* out-of-band data */
	void __iomem	*oob_outbox_base;
	int		oob_irq;
	u8		oob_started;
	unsigned long	oob_id;

	const struct sprd_mbox_phy_ops	*phy_ops;

#if defined(CONFIG_DEBUG_FS)
	struct dentry	*debugfs_dir;
#endif
};

#define SPRD_MBOX_VER(r, p)		(u32)((r) * 0x100 + (p))
#define to_sprd_mbox_priv(_mbox)	container_of(_mbox, struct sprd_mbox_priv, mbox)

u8 get_tx_fifo_mask(u8 deliver_bit);
void mbox_start_send_tx_fifo(u8 msk);
#if defined(CONFIG_DEBUG_FS)
extern struct sprd_mbox_priv *mbox_priv;
#endif

#if defined CONFIG_UNISOC_MAILBOX_R1
int sprd_mbox_phy_r1_init(struct sprd_mbox_priv *priv);
void sprd_mbox_change_wakeup_flag_r1(bool wakeup_flag);
void sprd_mbox_wakeup_flag_callback_register_r1(void (*callback)(bool wakeup_flag));
void sprd_mbox_wakeup_flag_callback_unregister_r1(void);
#elif defined CONFIG_UNISOC_MAILBOX_R2
int sprd_mbox_phy_r2_init(struct sprd_mbox_priv *priv);
void sprd_mbox_change_wakeup_flag_r2(bool wakeup_flag);
void sprd_mbox_wakeup_flag_callback_register_r2(void (*callback)(bool wakeup_flag));
void sprd_mbox_wakeup_flag_callback_unregister_r2(void);
#endif

#endif /* _UNISOC_MAILBOX_H */

