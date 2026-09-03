/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-smsg: " fmt

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/sipc.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_pdbg.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/wait.h>

#include "sblock.h"
#include "sipc_priv.h"
#include "sipx.h"

#include "../drivers/unisoc_platform/modem/mailbox/unisoc-mailbox.h"

#define SMSG_TXBUF_ADDR		(0)
#define SMSG_TXBUF_SIZE		(SZ_1K)
#define SMSG_RXBUF_ADDR		(SMSG_TXBUF_SIZE)
#define SMSG_RXBUF_SIZE		(SZ_1K)

#define SMSG_RINGHDR		(SMSG_TXBUF_SIZE + SMSG_RXBUF_SIZE)
#define SMSG_TXBUF_RDPTR	(SMSG_RINGHDR + 0)
#define SMSG_TXBUF_WRPTR	(SMSG_RINGHDR + 4)
#define SMSG_RXBUF_RDPTR	(SMSG_RINGHDR + 8)
#define SMSG_RXBUF_WRPTR	(SMSG_RINGHDR + 12)

#define SIPC_READL(addr)      readl((__force void __iomem *)(addr))
#define SIPC_WRITEL(b, addr)  writel(b, (__force void __iomem *)(addr))

#define HIGH_OFFSET_FLAG (0xFEFE)

#if defined(CONFIG_DEBUG_FS)
struct smsg_device {
	int			major;
	int			minor;
	struct cdev		cdev;
	struct device		*sys_dev;	/* Device object in sysfs */
};

static struct class *smsg_class;
static struct smsg_device *smsg_dev;
#endif

static u8 g_wakeup_flag;
static struct smsg_callback_t smsg_callback[SIPC_ID_NR][SMSG_CH_NR + 1];
#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
static u32 ws_info = 0xffff;
#endif

struct smsg_ipc *smsg_ipcs[SIPC_ID_NR];
EXPORT_SYMBOL_GPL(smsg_ipcs);

static u8 channel2index[SMSG_CH_NR + 1];

static int smsg_ipc_smem_init(struct smsg_ipc *ipc);

static void smsg_wakeup_print(struct smsg_ipc *ipc, struct smsg *msg)
{
	/* if the first msg come after the irq wake up by sipc,
	 * use prin_fo to output log
	 */
#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
	u32 ch = msg->channel;
#endif
	if (g_wakeup_flag) {
		g_wakeup_flag = 0;
		pr_info("irq read smsg: dst=%d, channel=%d,type=%d, flag=0x%04x, value=0x%08x\n",
			ipc->dst,
			msg->channel,
			msg->type,
			msg->flag,
			msg->value);
#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
		ws_info = (ipc->dst | (ch << 8));
#endif
	} else {
		pr_debug("irq read non wakeup smsg: dst=%d, channel=%d,type=%d, flag=0x%04x, value=0x%08x\n",
			 ipc->dst,
			 msg->channel,
			 msg->type,
			 msg->flag,
			 msg->value);
	}
}

void smsg_init_channel2index(void)
{
	u16 i, j;
	for (i = 0; i < ARRAY_SIZE(channel2index); i++) {
		for (j = 0; j < SMSG_VALID_CH_NR; j++) {
			/* find the index of channel i */
			if (sipc_cfg[j].channel == i)
				break;
		}

		/* if not find, init with INVALID_CHANEL_INDEX,
		 * else init whith j
		 */
		if (j == SMSG_VALID_CH_NR)
			channel2index[i] = INVALID_CHANEL_INDEX;
		else
			channel2index[i] = j;
	}
}

void smsg_callback_register(u8 dst, u8 channel,
			void (*callback)(const struct smsg *msg, void *data),
			void *data)
{
	smsg_callback[dst][channel].callback = callback;
	smsg_callback[dst][channel].data = data;
}
EXPORT_SYMBOL_GPL(smsg_callback_register);

void smsg_callback_unregister(u8 dst, u8 channel)
{
	smsg_callback[dst][channel].callback = NULL;
	smsg_callback[dst][channel].data = NULL;
}
EXPORT_SYMBOL_GPL(smsg_callback_unregister);

void smsg_msg_process(struct smsg_ipc *ipc, struct smsg *msg, bool wake_lock)
{
	struct smsg_channel *ch = NULL;
	struct smsg_callback_t *callback_handler;
	u32 wr;
	u8 ch_index;

	ch_index = channel2index[msg->channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", msg->channel);
		return;
	}

	if (!ipc)
		return;
	smsg_wakeup_print(ipc, msg);
	callback_handler = &smsg_callback[ipc->dst][msg->channel];
	atomic_inc(&ipc->busy[ch_index]);

	pr_debug("smsg:get dst=%d msg channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
		ipc->dst, msg->channel,
		msg->type, msg->flag,
		msg->value);

	if (msg->type >= SMSG_TYPE_NR) {
		/* invalid msg */
		pr_info("invalid smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);
		goto exit_msg_proc;
	}

	if (msg->type == SMSG_TYPE_HIGH_OFFSET &&
		msg->flag == HIGH_OFFSET_FLAG) {
		/* high offset msg */
		ipc->high_offset = msg->value;
		pr_info("smsg:  high_offset = 0x%x\n",
			msg->value);
		goto exit_msg_proc;
	}

	ch = ipc->channels[ch_index];
	if (!ch) {
		if (ipc->states[ch_index] == CHAN_STATE_UNUSED &&
			msg->type == SMSG_TYPE_OPEN &&
			msg->flag == SMSG_OPEN_MAGIC)
			ipc->states[ch_index] = CHAN_STATE_CLIENT_OPENED;
		else
			/* drop this bad msg since channel
			 * is not opened
			 */
			pr_info("smsg channel %d not opened! drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type,
				msg->flag, msg->value);

		goto exit_msg_proc;
	}

	if ((int)(SIPC_READL(ch->wrptr) - SIPC_READL(ch->rdptr)) >=
		SMSG_CACHE_NR) {
		/* msg cache is full, drop this msg */
		pr_info("smsg channel %d recv cache is full! drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);
	} else {
		/* write smsg to cache */
		wr = SIPC_READL(ch->wrptr) & (SMSG_CACHE_NR - 1);
		memcpy(&ch->caches[wr], msg, sizeof(struct smsg));
		SIPC_WRITEL(SIPC_READL(ch->wrptr) + 1, ch->wrptr);
	}

	if (callback_handler->callback)
		callback_handler->callback(msg, callback_handler->data);

	wake_up_interruptible_all(&ch->rxwait);

	if (wake_lock)
		sprd_pms_request_wakelock_period(ch->rx_pms, 500);

exit_msg_proc:
	atomic_dec(&ipc->busy[ch_index]);
}
EXPORT_SYMBOL(smsg_msg_process);

static int smsg_send_high_offset_thread(void *data)
{
	struct smsg msg;
	struct smsg_ipc *ipc = (struct smsg_ipc *)data;

	smsg_set(&msg,
		 SMSG_CH_CTRL,
		 SMSG_TYPE_HIGH_OFFSET,
		 HIGH_OFFSET_FLAG,
		 ipc->dst_high_offset);
	smsg_send(ipc->dst, &msg, 0);

	pr_debug("smsg: send high offset(%d) to client!\n",
		ipc->dst_high_offset);
	ipc->thread = NULL;

	return 0;
}

static void smsg_send_high_offset(struct smsg_ipc *ipc)
{
	u8 dst = ipc->dst;

	/* host sipc, if offset > 0, send high offset to dst */
	if (!ipc->client && ipc->dst_high_offset && !ipc->thread) {
		ipc->thread = kthread_create(smsg_send_high_offset_thread,
					     ipc, "sipc-send-%d", dst);
		if (!IS_ERR(ipc->thread))
			wake_up_process(ipc->thread);
	}
}

static int smsg_ipc_smem_init(struct smsg_ipc *ipc)
{
	void __iomem *base, *p;
	int i;
	struct smem_item *smem_ptr;

	if (ipc->smem_inited)
		return 0;

	ipc->smem_inited = 1;
	pr_debug("%s!\n", ipc->name);

	for (i = 0; i < ipc->smem_cnt; i++) {
		smem_ptr = ipc->smem_ptr;
		smem_init(smem_ptr[i].smem_base, smem_ptr[i].smem_size,
			  ipc->dst, i, ipc->smem_type);
	}

	if (ipc->ring_base) {
		base = (void __iomem *)shmem_ram_vmap_nocache(ipc->dst,
					ipc->ring_base + ipc->high_offset,
					ipc->ring_size);
		if (!base)
			return -ENOMEM;

		/* assume client is boot later than host */
		if (!ipc->client) {
			/**
			 * memset(base, 0, ipc->ring_size);
			 * the instruction dc avz
			 * will abort for nocache memory
			 */
			for (p = base; p < base + ipc->ring_size;) {
#ifdef CONFIG_64BIT
				*(uint64_t *)p = 0x0;
				p += sizeof(uint64_t);
#else
				*(u32 *)p = 0x0;
				p += sizeof(u32);
#endif
			}
		}

		if (ipc->client) {
			/* clent mode, tx is host rx , rx is host tx*/
			ipc->smem_vbase = (void *)base;
			ipc->txbuf_size = SMSG_RXBUF_SIZE /
				sizeof(struct smsg);
			ipc->txbuf_addr = (uintptr_t)base +
				SMSG_RXBUF_ADDR;
			ipc->txbuf_rdptr = (uintptr_t)base +
				SMSG_RXBUF_RDPTR;
			ipc->txbuf_wrptr = (uintptr_t)base +
				SMSG_RXBUF_WRPTR;
			ipc->rxbuf_size = SMSG_TXBUF_SIZE /
				sizeof(struct smsg);
			ipc->rxbuf_addr = (uintptr_t)base +
				SMSG_TXBUF_ADDR;
			ipc->rxbuf_rdptr = (uintptr_t)base +
				SMSG_TXBUF_RDPTR;
			ipc->rxbuf_wrptr = (uintptr_t)base +
				SMSG_TXBUF_WRPTR;
		} else {
			ipc->smem_vbase = (void *)base;
			ipc->txbuf_size = SMSG_TXBUF_SIZE /
				sizeof(struct smsg);
			ipc->txbuf_addr = (uintptr_t)base +
				SMSG_TXBUF_ADDR;
			ipc->txbuf_rdptr = (uintptr_t)base +
				SMSG_TXBUF_RDPTR;
			ipc->txbuf_wrptr = (uintptr_t)base +
				SMSG_TXBUF_WRPTR;
			ipc->rxbuf_size = SMSG_RXBUF_SIZE /
				sizeof(struct smsg);
			ipc->rxbuf_addr = (uintptr_t)base +
				SMSG_RXBUF_ADDR;
			ipc->rxbuf_rdptr = (uintptr_t)base +
				SMSG_RXBUF_RDPTR;
			ipc->rxbuf_wrptr = (uintptr_t)base +
				SMSG_RXBUF_WRPTR;
		}
	}

	smsg_send_high_offset(ipc);

	return 0;
}

static void smsg_ipc_mpm_init(struct smsg_ipc *ipc)
{
	/* create modem power manger instance for this sipc */
	sprd_mpm_create(ipc->dst, ipc->name, ipc->latency);

	/* init a power manager source */
	ipc->sipc_pms = sprd_pms_create(ipc->dst, ipc->name, true);
	if (!ipc->sipc_pms)
		pr_warn("create pms %s failed!\n", ipc->name);
}

void smsg_ipc_create(struct smsg_ipc *ipc)
{
	pr_info("%s\n", ipc->name);

	smsg_ipcs[ipc->dst] = ipc;
	smsg_ipc_mpm_init(ipc);
	smsg_ipc_smem_init(ipc);
}

void smsg_ipc_destroy(struct smsg_ipc *ipc)
{
	shmem_ram_unmap(ipc->dst, ipc->smem_vbase);
	smem_free(ipc->dst, ipc->ring_base, SZ_4K);
	sprd_mpm_destroy(ipc->dst);

	if (!IS_ERR_OR_NULL(ipc->thread))
		kthread_stop(ipc->thread);

	smsg_ipcs[ipc->dst] = NULL;
}

int sipc_get_wakeup_flag(void)
{
	return (int)g_wakeup_flag;
}
EXPORT_SYMBOL_GPL(sipc_get_wakeup_flag);

void sipc_set_wakeup_flag(void)
{
	g_wakeup_flag = 1;
}
EXPORT_SYMBOL_GPL(sipc_set_wakeup_flag);

void sipc_clear_wakeup_flag(void)
{
	g_wakeup_flag = 0;
}
EXPORT_SYMBOL_GPL(sipc_clear_wakeup_flag);

static void smsg_wakeup_flag_notify(bool wakeup_flag)
{
	if (wakeup_flag)
		sipc_set_wakeup_flag();
	else if (sipc_get_wakeup_flag())
		sipc_clear_wakeup_flag();
}

#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
static int pdbg_nb_cb(struct notifier_block *self, unsigned long cmd, void *priv)
{
	switch (cmd) {
	case PDBG_NB_WS_UPDATE:
		*(u32 *)priv = ws_info;
		ws_info = 0xffff;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block pdbg_nb = {
	.notifier_call = pdbg_nb_cb,
};
#endif

void smsg_init_wakeup(void)
{
#if defined CONFIG_UNISOC_MAILBOX_R1
	sprd_mbox_wakeup_flag_callback_register_r1(smsg_wakeup_flag_notify);
#elif defined CONFIG_UNISOC_MAILBOX_R2
	sprd_mbox_wakeup_flag_callback_register_r2(smsg_wakeup_flag_notify);
#endif
#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
	if (sprd_pdbg_notify_register(&pdbg_nb))
		pr_err("sprd_pdbg_notify_register failed!!!\n");
#endif
}
EXPORT_SYMBOL_GPL(smsg_init_wakeup);

void smsg_remove_wakeup(void)
{
#if defined CONFIG_UNISOC_MAILBOX_R1
	sprd_mbox_wakeup_flag_callback_unregister_r1();
#elif defined CONFIG_UNISOC_MAILBOX_R2
	sprd_mbox_wakeup_flag_callback_unregister_r2();
#endif
}
EXPORT_SYMBOL_GPL(smsg_remove_wakeup);

int smsg_ch_wake_unlock(u8 dst, u8 channel)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	u8 ch_index;

	ch_index = channel2index[channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	ch = ipc->channels[ch_index];
	if (!ch)
		return -ENODEV;

	sprd_pms_release_wakelock(ch->rx_pms);
	return 0;
}
EXPORT_SYMBOL_GPL(smsg_ch_wake_unlock);

int smsg_ch_open(u8 dst, u8 channel, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mopen;
	struct smsg mrecv;
	int rval = 0;
	u8 ch_index;

	ch_index = channel2index[channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	ch = kzalloc(sizeof(*ch), GFP_KERNEL);
	if (!ch)
		return -ENOMEM;

	snprintf(ch->tx_name, sizeof(ch->tx_name), "smsg-%d-%d-tx", dst, channel);

	ch->tx_pms = sprd_pms_create(dst, ch->tx_name, true);
	if (!ch->tx_pms)
		pr_warn("create pms %s failed!\n", ch->tx_name);

	snprintf(ch->rx_name, sizeof(ch->rx_name), "smsg-%d-%d-rx", dst, channel);
	ch->rx_pms = sprd_pms_create(dst, ch->rx_name, true);
	if (!ch->rx_pms)
		pr_warn("create pms %s failed!\n", ch->rx_name);

	atomic_set(&ipc->busy[ch_index], 1);
	init_waitqueue_head(&ch->rxwait);
	mutex_init(&ch->rxlock);
	ipc->channels[ch_index] = ch;

	pr_info("channel %d-%d send open msg!\n",
		dst, channel);

	smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
	rval = smsg_send(dst, &mopen, timeout);
	if (rval != 0) {
		pr_err("channel %d-%d send open msg error = %d!\n",
		       dst, channel, rval);
		ipc->states[ch_index] = CHAN_STATE_UNUSED;
		ipc->channels[ch_index] = NULL;
		atomic_dec(&ipc->busy[ch_index]);
		/* guarantee that channel resource isn't used in irq handler  */
		while (atomic_read(&ipc->busy[ch_index]))
			;

		kfree(ch);

		return rval;
	}

	/* open msg might be got before */
	if (ipc->states[ch_index] == CHAN_STATE_CLIENT_OPENED)
		goto open_done;

	ipc->states[ch_index] = CHAN_STATE_HOST_OPENED;

	do {
		smsg_set(&mrecv, channel, 0, 0, 0);
		rval = smsg_recv(dst, &mrecv, timeout);
		if (rval != 0) {
			pr_err("channel %d-%d smsg receive error = %d!\n",
			       dst, channel, rval);
			ipc->states[ch_index] = CHAN_STATE_UNUSED;
			ipc->channels[ch_index] = NULL;

			/* guarantee that channel resource isn't used
			 * in irq handler
			 */
			while (atomic_read(&ipc->busy[ch_index]) != 1)
				;

			kfree(ch);
			atomic_dec(&ipc->busy[ch_index]);
			return rval;
		}
	} while (mrecv.type != SMSG_TYPE_OPEN || mrecv.flag != SMSG_OPEN_MAGIC);

	pr_info("channel %d-%d receive open msg!\n",
		dst, channel);

open_done:
	pr_info("channel %d-%d success\n", dst, channel);
	ipc->states[ch_index] = CHAN_STATE_OPENED;
	atomic_dec(&ipc->busy[ch_index]);

	return 0;
}
EXPORT_SYMBOL_GPL(smsg_ch_open);

int smsg_ch_close(u8 dst, u8 channel,  int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mclose;
	u8 ch_index;

	ch_index = channel2index[channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	ch = ipc->channels[ch_index];
	if (!ch)
		return 0;

	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst, &mclose, timeout);

	ipc->states[ch_index] = CHAN_STATE_FREE;
	wake_up_interruptible_all(&ch->rxwait);

	/* wait for the channel being unused */
	while (atomic_read(&ipc->busy[ch_index]))
		;

	/* maybe channel has been free for smsg_ch_open failed */
	if (ipc->channels[ch_index]) {
		ipc->channels[ch_index] = NULL;
		/* guarantee that channel resource isn't used in irq handler */
		while (atomic_read(&ipc->busy[ch_index]))
			;

		kfree(ch);
	}

	/* finally, update the channel state*/
	ipc->states[ch_index] = CHAN_STATE_UNUSED;

	return 0;
}
EXPORT_SYMBOL_GPL(smsg_ch_close);

int smsg_senddie(u8 dst)
{
	struct smsg msg;
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	uintptr_t txpos;
	int rval = 0;
	int rc;

	if (!ipc)
		return -ENODEV;

	msg.channel = SMSG_CH_CTRL;
	msg.type = SMSG_TYPE_DIE;
	msg.flag = 0;
	msg.value = 0;

	rc = mbox_send_message(ipc->chan, &msg);
	if (rc < 0) {
		pr_err("mailbox chan[%d] send error\n", ipc->dst);
		rval = -EBUSY;
		goto send_failed;
	} else
		mbox_chan_txdone(ipc->chan, 0);

	pr_info("%s mailbox send die smsg\n", __func__);

	if (ipc->ring_base) {
		/* must wait resource before read or write share memory */
		rval = sprd_pms_request_resource(ipc->sipc_pms, 0);
		if (rval < 0)
			return rval;
		if ((int)(SIPC_READL(ipc->txbuf_wrptr) -
			SIPC_READL(ipc->txbuf_rdptr)) >= ipc->txbuf_size) {
			pr_err("smsg_send: smsg txbuf is full!\n");
			rval = -EBUSY;
			goto send_failed;
		}

		/* calc txpos and write smsg */
		txpos = (SIPC_READL(ipc->txbuf_wrptr) & (ipc->txbuf_size - 1)) *
			sizeof(struct smsg) + ipc->txbuf_addr;
		memcpy((void *)txpos, &msg, sizeof(struct smsg));

		pr_debug("write smsg: wrptr=%u, rdptr=%u, txpos=0x%lx\n",
			 SIPC_READL(ipc->txbuf_wrptr),
			 SIPC_READL(ipc->txbuf_rdptr),
			 txpos);

		/* update wrptr */
		SIPC_WRITEL(SIPC_READL(ipc->txbuf_wrptr) + 1, ipc->txbuf_wrptr);

	}

send_failed:
	sprd_pms_release_resource(ipc->sipc_pms);
	return rval;
}
EXPORT_SYMBOL_GPL(smsg_senddie);

int smsg_send(u8 dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	uintptr_t txpos;
	int rval = 0;
	unsigned long flags;
	u8 ch_index;
	int rc;

	ch_index = channel2index[msg->channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", msg->channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	if (!ipc->channels[ch_index]) {
		pr_err("channel %d not inited!\n", msg->channel);
		return -ENODEV;
	}

	if (ipc->states[ch_index] != CHAN_STATE_OPENED &&
	    msg->type != SMSG_TYPE_OPEN &&
	    msg->type != SMSG_TYPE_CLOSE) {
		pr_err("channel %d not opened!\n", msg->channel);
		return -EINVAL;
	}

	pr_debug("dst=%d, channel=%d, timeout=%d, suspend=%d\n",
		 dst, msg->channel, timeout, ipc->suspend);

	if (ipc->suspend) {
		rval = wait_event_interruptible(
				ipc->suspend_wait,
				!ipc->suspend);
		if (rval) {
			pr_err("rval = %d!\n", rval);
			return -EINVAL;
		}
	}
	pr_debug("send smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
		 msg->channel, msg->type, msg->flag, msg->value);

	ch = ipc->channels[ch_index];
	rval = sprd_pms_request_resource(ch->tx_pms, timeout);
	if (rval < 0)
		return rval;

	spin_lock_irqsave(&ipc->txpinlock, flags);
	if (ipc->ring_base) {
		if (((int)(SIPC_READL(ipc->txbuf_wrptr) -
				SIPC_READL(ipc->txbuf_rdptr)) >=
				ipc->txbuf_size)) {
			pr_err("smsg txbuf is full!\n");
			rval = -EBUSY;
			goto send_failed;
		}

		/* calc txpos and write smsg */
		txpos = (SIPC_READL(ipc->txbuf_wrptr) & (ipc->txbuf_size - 1)) *
			sizeof(struct smsg) + ipc->txbuf_addr;
		memcpy((void *)txpos, msg, sizeof(struct smsg));

		pr_debug("write smsg: wrptr=0x%x, rdptr=0x%x, txpos=0x%lx\n",
			 SIPC_READL(ipc->txbuf_wrptr),
			 SIPC_READL(ipc->txbuf_rdptr),
			 txpos);

		/* update wrptr */
		SIPC_WRITEL(SIPC_READL(ipc->txbuf_wrptr) + 1, ipc->txbuf_wrptr);
	}

	rc = mbox_send_message(ipc->chan, msg);
	if (rc < 0) {
		pr_err("mailbox chan send error\n");
		rval = -EBUSY;
		goto send_failed;
	} else
		mbox_chan_txdone(ipc->chan, 0);
	pr_debug("%s mailbox send smsg  %llx\n", __func__, *(u64 *)msg);

send_failed:
	spin_unlock_irqrestore(&ipc->txpinlock, flags);
	sprd_pms_release_resource(ch->tx_pms);
	return rval;
}
EXPORT_SYMBOL_GPL(smsg_send);

int smsg_recv(u8 dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	u32 rd;
	int rval = 0;
	u8 ch_index;

	ch_index = channel2index[msg->channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", msg->channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	atomic_inc(&ipc->busy[ch_index]);

	ch = ipc->channels[ch_index];

	if (!ch) {
		pr_err("channel %d not opened!\n", msg->channel);
		atomic_dec(&ipc->busy[ch_index]);
		return -ENODEV;
	}

	pr_debug("dst=%d, channel=%d, timeout=%d, ch_index = %d\n",
		 dst, msg->channel, timeout, ch_index);

	if (timeout == 0) {
		if (!mutex_trylock(&ch->rxlock)) {
			pr_err("dst=%d, channel=%d recv smsg busy!\n",
			       dst, msg->channel);
			atomic_dec(&ipc->busy[ch_index]);

			return -EBUSY;
		}

		/* no wait */
		if (SIPC_READL(ch->wrptr) == SIPC_READL(ch->rdptr)) {
			pr_debug("dst=%d, channel=%d smsg rx cache is empty!\n",
				dst, msg->channel);

			rval = -ENODATA;

			goto recv_failed;
		}
	} else if (timeout < 0) {
		mutex_lock(&ch->rxlock);
		/* wait forever */
		rval = wait_event_interruptible(
				ch->rxwait,
				(SIPC_READL(ch->wrptr) !=
				 SIPC_READL(ch->rdptr)) ||
				(ipc->states[ch_index] == CHAN_STATE_FREE));
		if (rval < 0) {
			pr_debug("dst=%d, channel=%d wait interrupted!\n",
				 dst, msg->channel);

			goto recv_failed;
		}

		if (ipc->states[ch_index] == CHAN_STATE_FREE) {
			pr_debug("dst=%d, channel=%d channel is free!\n",
				dst, msg->channel);

			rval = -EIO;

			goto recv_failed;
		}
	} else {
		mutex_lock(&ch->rxlock);
		/* wait timeout */
		rval = wait_event_interruptible_timeout(
			ch->rxwait,
			(SIPC_READL(ch->wrptr) != SIPC_READL(ch->rdptr)) ||
			(ipc->states[ch_index] == CHAN_STATE_FREE),
			timeout);
		if (rval < 0) {
			pr_debug("dst=%d, channel=%d wait interrupted!\n",
				 dst, msg->channel);

			goto recv_failed;
		} else if (rval == 0) {
			pr_debug("dst=%d, channel=%d wait timeout!\n",
				 dst, msg->channel);

			rval = -ETIME;

			goto recv_failed;
		}

		if (ipc->states[ch_index] == CHAN_STATE_FREE) {
			pr_debug("dst=%d, channel=%d channel is free!\n",
				dst, msg->channel);

			rval = -EIO;

			goto recv_failed;
		}
	}

	/* read smsg from cache */
	rd = SIPC_READL(ch->rdptr) & (SMSG_CACHE_NR - 1);
	memcpy(msg, &ch->caches[rd], sizeof(struct smsg));
	SIPC_WRITEL(SIPC_READL(ch->rdptr) + 1, ch->rdptr);

	if (ipc->ring_base)
		pr_debug("read smsg: dst=%d, channel=%d, wrptr=%d, rdptr=%d, rd=%d\n",
			 dst,
			 msg->channel,
			 SIPC_READL(ch->wrptr),
			 SIPC_READL(ch->rdptr),
			 rd);

	pr_debug("recv smsg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x, rval = %d\n",
		 dst, msg->channel, msg->type, msg->flag, msg->value, rval);

recv_failed:
	mutex_unlock(&ch->rxlock);
	atomic_dec(&ipc->busy[ch_index]);
	return rval;
}
EXPORT_SYMBOL_GPL(smsg_recv);

u8 sipc_channel2index(u8 channel)
{
	return channel2index[channel];
}
EXPORT_SYMBOL_GPL(sipc_channel2index);

#if defined(CONFIG_DEBUG_FS)
static int smsg_debug_show(struct seq_file *m, void *private)
{
	struct smsg_ipc *ipc = NULL;
	struct smsg_channel *ch;

	int i, j, cnt, ch_index;

	for (i = 0; i < SIPC_ID_NR; i++) {
		ipc = smsg_ipcs[i];
		if (!ipc)
			continue;

		sipc_debug_putline(m, '*', 120);
		seq_printf(m, "sipc: %s:\n", ipc->name);
		seq_printf(m, "dst: 0x%0x, irq: 0x%0x\n",
			   ipc->dst, ipc->irq);
		if (ipc->ring_base) {
			if (sipc_smem_request_resource(ipc->sipc_pms, ipc->dst, 1000) < 0)
				continue;

			seq_printf(m, "txbufAddr: 0x%p, txbufsize: 0x%x, txbufrdptr: [0x%p]=%d, txbufwrptr: [0x%p]=%d\n",
				   (void *)ipc->txbuf_addr,
				   ipc->txbuf_size,
				   (void *)ipc->txbuf_rdptr,
				   SIPC_READL(ipc->txbuf_rdptr),
				   (void *)ipc->txbuf_wrptr,
				   SIPC_READL(ipc->txbuf_wrptr));
			seq_printf(m, "rxbufAddr: 0x%p, rxbufsize: 0x%x, rxbufrdptr: [0x%p]=%d, rxbufwrptr: [0x%p]=%d\n",
				   (void *)ipc->rxbuf_addr,
				   ipc->rxbuf_size,
				   (void *)ipc->rxbuf_rdptr,
				   SIPC_READL(ipc->rxbuf_rdptr),
				   (void *)ipc->rxbuf_wrptr,
				   SIPC_READL(ipc->rxbuf_wrptr));

			/* release resource */
			sipc_smem_release_resource(ipc->sipc_pms, ipc->dst);
		}
		sipc_debug_putline(m, '-', 80);
		seq_puts(m, "1. all channel state list:\n");

		for (j = 0; j < SMSG_VALID_CH_NR; j++)
			seq_printf(m,
				   "%2d. channel[%3d] states: %d, name: %s\n",
				   j,
				   sipc_cfg[j].channel,
				   ipc->states[j],
				   sipc_cfg[j].name);

		sipc_debug_putline(m, '-', 80);
		seq_puts(m, "2. channel rdpt < wrpt list:\n");

		cnt = 1;
		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			ch_index = channel2index[i];
			ch = ipc->channels[ch_index];
			if (!ch)
				continue;

			if (SIPC_READL(ch->rdptr) < SIPC_READL(ch->wrptr))
				seq_printf(m, "%2d. channel[%3d] rd: %d, wt: %d, name: %s\n",
					   cnt++,
					   sipc_cfg[j].channel,
					   SIPC_READL(ch->rdptr),
					   SIPC_READL(ch->wrptr),
					   sipc_cfg[j].name);
		}
	}
	return 0;
}

static int smsg_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, smsg_debug_show, inode->i_private);
}

static const struct file_operations smsg_debug_fops = {
	.open = smsg_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int smsg_init_debug(void)
{
	int rval;
	dev_t dev_no;

	smsg_dev = kzalloc(sizeof(struct smsg_device), GFP_KERNEL);
	if (!smsg_dev)
		return -ENOMEM;
	rval = alloc_chrdev_region(&dev_no, 0, 1, "smsg_debug");
	if (rval) {
		pr_err("Failed to alloc chrdev region\n");
		goto free_smsg_dev;
	}
	smsg_dev->major = MAJOR(dev_no);
	smsg_dev->minor = MINOR(dev_no);
	cdev_init(&smsg_dev->cdev, &smsg_debug_fops);
	rval = cdev_add(&smsg_dev->cdev, dev_no, 1);
	if (rval) {
		pr_err("Failed to add smsg cdev\n");
		goto free_devno;
	}
	smsg_class = class_create(THIS_MODULE, "smsg");
	if (IS_ERR(smsg_class)) {
		rval = PTR_ERR(smsg_class);
		pr_err("Failed to create smsg class\n");
		goto free_cdev;
	}
	smsg_dev->sys_dev = device_create(smsg_class, NULL,
				       dev_no,
				       smsg_dev, "sipc_smsg");

	return 0;

free_cdev:
	cdev_del(&smsg_dev->cdev);

free_devno:
	unregister_chrdev_region(dev_no, 1);

free_smsg_dev:
	kfree(smsg_dev);
	smsg_dev = NULL;
	return rval;
}
EXPORT_SYMBOL_GPL(smsg_init_debug);

void smsg_destroy_debug(void)
{
	if (smsg_dev) {
		dev_t dev_no = MKDEV(smsg_dev->major, smsg_dev->minor);

		if (smsg_dev->sys_dev) {
			device_destroy(smsg_class, dev_no);
			smsg_dev->sys_dev = NULL;
		}

		if (!IS_ERR(smsg_class))
			class_destroy(smsg_class);

		unregister_chrdev_region(dev_no, 1);
		cdev_del(&smsg_dev->cdev);
		kfree(smsg_dev);
		smsg_dev = NULL;
	}
}
EXPORT_SYMBOL_GPL(smsg_destroy_debug);
#endif /* CONFIG_DEBUG_FS */


MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMSG driver");
MODULE_LICENSE("GPL v2");
