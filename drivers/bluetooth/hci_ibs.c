/*
 *  Qualcomm's Bluetooth Software In-Band Sleep UART protocol
 *
 *  HCI_IBS (HCI In-Band Sleep) is Qualcomm's power management
 *  protocol extension to H4.
 *
 *  Copyright (C) 2007 Texas Instruments, Inc.
 *  Copyright (c) 2010, 2012, 2014, The Linux Foundation. All rights reserved.
 *
 *  Acknowledgements:
 *  This file is based on hci_ll.c, which was...
 *  Written by Ohad Ben-Cohen <ohad@bencohen.org>
 *  which was in turn based on hci_h4.c, which was written
 *  by Maxim Krasnyansky and Marcel Holtmann.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/serial_core.h>

#ifdef CONFIG_SERIAL_MSM_HS
#include <linux/platform_data/msm_serial_hs.h>
#endif

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"

/* HCI_IBS protocol messages */
#define HCI_IBS_SLEEP_IND	0xFE
#define HCI_IBS_WAKE_IND	0xFD
#define HCI_IBS_WAKE_ACK	0xFC

/* TX idle time out value */
#define TX_IDLE_TO		1000

/* HCI_IBS receiver States */
#define HCI_IBS_W4_PACKET_TYPE	0
#define HCI_IBS_W4_EVENT_HDR	1
#define HCI_IBS_W4_ACL_HDR	2
#define HCI_IBS_W4_SCO_HDR	3
#define HCI_IBS_W4_DATA		4

/* HCI_IBS transmit side sleep protocol states */
enum tx_ibs_states_e {
	HCI_IBS_TX_ASLEEP,
	HCI_IBS_TX_WAKING,
	HCI_IBS_TX_AWAKE,
};

/* HCI_IBS receive side sleep protocol states */
enum rx_states_e {
	HCI_IBS_RX_ASLEEP,
	HCI_IBS_RX_AWAKE,
};

/* HCI_IBS transmit and receive side clock state vote */
enum hci_ibs_clock_state_vote_e {
	HCI_IBS_VOTE_STATS_UPDATE,
	HCI_IBS_TX_VOTE_CLOCK_ON,
	HCI_IBS_TX_VOTE_CLOCK_OFF,
	HCI_IBS_RX_VOTE_CLOCK_ON,
	HCI_IBS_RX_VOTE_CLOCK_OFF,
};

static unsigned long wake_retrans = 1;
static unsigned long tx_idle_delay = (HZ * 2);

struct hci_ibs_cmd {
	u8 cmd;
} __attribute__((packed));

struct ibs_struct {
	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
	struct sk_buff_head tx_wait_q;	/* HCI_IBS wait queue	*/
	spinlock_t hci_ibs_lock;	/* HCI_IBS state lock	*/
	unsigned long tx_ibs_state;	/* HCI_IBS transmit side power state */
	unsigned long rx_ibs_state;	/* HCI_IBS receive side power state */
	unsigned long tx_vote;		/* clock must be on for TX */
	unsigned long rx_vote;		/* clock must be on for RX */
	struct	timer_list tx_idle_timer;
	struct	timer_list wake_retrans_timer;
	struct	workqueue_struct *workqueue;
	struct	work_struct ws_awake_rx;
	struct	work_struct ws_awake_device;
	struct	work_struct ws_rx_vote_off;
	struct	work_struct ws_tx_vote_off;
	void *ibs_hu; /* keeps the hci_uart pointer for reference */

	/* debug */
	unsigned long ibs_sent_wacks;
	unsigned long ibs_sent_slps;
	unsigned long ibs_sent_wakes;
	unsigned long ibs_recv_wacks;
	unsigned long ibs_recv_slps;
	unsigned long ibs_recv_wakes;
	unsigned long vote_last_jif;
	unsigned long vote_on_ticks;
	unsigned long vote_off_ticks;
	unsigned long tx_votes_on;
	unsigned long rx_votes_on;
	unsigned long tx_votes_off;
	unsigned long rx_votes_off;
	unsigned long votes_on;
	unsigned long votes_off;
};

#ifdef CONFIG_SERIAL_MSM_HS
static void __ibs_msm_serial_clock_on(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->uart_port;

	msm_hs_request_clock_on(port);
}

static void __ibs_msm_serial_clock_request_off(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->uart_port;

	msm_hs_request_clock_off(port);
}
#else
static inline void __ibs_msm_serial_clock_on(struct tty_struct *tty) {}
static inline void __ibs_msm_serial_clock_request_off(struct tty_struct *tty) {}
#endif

/* clock_vote needs to be called with the ibs lock held */
static void ibs_msm_serial_clock_vote(unsigned long vote, struct hci_uart *hu)
{
	struct ibs_struct *ibs = hu->priv;

	unsigned long old_vote = (ibs->tx_vote | ibs->rx_vote);
	unsigned long new_vote;

	switch (vote) {
	default: /* error */
		BT_ERR("voting irregularity");
		return;
	case HCI_IBS_VOTE_STATS_UPDATE:
		if (old_vote)
			ibs->vote_off_ticks += (jiffies - ibs->vote_last_jif);
		else
			ibs->vote_on_ticks += (jiffies - ibs->vote_last_jif);
		return;
	case HCI_IBS_TX_VOTE_CLOCK_ON:
		ibs->tx_vote = 1;
		ibs->tx_votes_on++;
		new_vote = 1;
		break;
	case HCI_IBS_RX_VOTE_CLOCK_ON:
		ibs->rx_vote = 1;
		ibs->rx_votes_on++;
		new_vote = 1;
		break;
	case HCI_IBS_TX_VOTE_CLOCK_OFF:
		ibs->tx_vote = 0;
		ibs->tx_votes_off++;
		new_vote = ibs->rx_vote | ibs->tx_vote;
		break;
	case HCI_IBS_RX_VOTE_CLOCK_OFF:
		ibs->rx_vote = 0;
		ibs->rx_votes_off++;
		new_vote = ibs->rx_vote | ibs->tx_vote;
		break;
	}
	if (new_vote != old_vote) {
		if (new_vote)
			__ibs_msm_serial_clock_on(hu->tty);
		else
			__ibs_msm_serial_clock_request_off(hu->tty);

		BT_DBG("HCIUART_IBS: vote msm_serial_hs clock %lu(%lu)",
			new_vote, vote);
		/* debug */
		if (new_vote) {
			ibs->votes_on++;
			ibs->vote_off_ticks += (jiffies - ibs->vote_last_jif);
		} else {
			ibs->votes_off++;
			ibs->vote_on_ticks += (jiffies - ibs->vote_last_jif);
		}
		ibs->vote_last_jif = jiffies;
	}
}

/*
 * Builds and sends an HCI_IBS command packet.
 * These are very simple packets with only 1 cmd byte
 */
static int send_hci_ibs_cmd(u8 cmd, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;
	struct ibs_struct *ibs = hu->priv;
	struct hci_ibs_cmd *hci_ibs_packet;

	BT_DBG("hu %pK cmd 0x%x", hu, cmd);

	/* allocate packet */
	skb = bt_skb_alloc(1, GFP_ATOMIC);
	if (!skb) {
		BT_ERR("cannot allocate memory for HCI_IBS packet");
		err = -ENOMEM;
		goto out;
	}

	/* prepare packet */
	hci_ibs_packet = (struct hci_ibs_cmd *) skb_put(skb, 1);
	hci_ibs_packet->cmd = cmd;
	skb->dev = (void *) hu->hdev;

	/* send packet */
	skb_queue_tail(&ibs->txq, skb);
out:
	return err;
}

static void ibs_wq_awake_device(struct work_struct *work)
{
	struct ibs_struct *ibs = container_of(work, struct ibs_struct,
					ws_awake_device);
	struct hci_uart *hu = (struct hci_uart *)ibs->ibs_hu;
	unsigned long flags;

	BT_DBG(" %pK ", hu);

	/* Vote for serial clock */
	ibs_msm_serial_clock_vote(HCI_IBS_TX_VOTE_CLOCK_ON, hu);

	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	/* send wake indication to device */
	if (send_hci_ibs_cmd(HCI_IBS_WAKE_IND, hu) < 0)
		BT_ERR("cannot send WAKE to device");

	ibs->ibs_sent_wakes++; /* debug */

	/* start retransmit timer */
	mod_timer(&ibs->wake_retrans_timer, jiffies + msecs_to_jiffies(10));

	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);

}

static void ibs_wq_awake_rx(struct work_struct *work)
{
	struct ibs_struct *ibs = container_of(work, struct ibs_struct,
					ws_awake_rx);
	struct hci_uart *hu = (struct hci_uart *)ibs->ibs_hu;
	unsigned long flags;

	BT_DBG(" %pK ", hu);

	ibs_msm_serial_clock_vote(HCI_IBS_RX_VOTE_CLOCK_ON, hu);

	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	ibs->rx_ibs_state = HCI_IBS_RX_AWAKE;
	/* Always acknowledge device wake up,
	 * sending IBS message doesn't count as TX ON
	 */
	if (send_hci_ibs_cmd(HCI_IBS_WAKE_ACK, hu) < 0)
		BT_ERR("cannot acknowledge device wake up");

	ibs->ibs_sent_wacks++; /* debug */

	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);

	/* actually send the packets */
	hci_uart_tx_wakeup(hu);

}

static void ibs_wq_serial_rx_clock_vote_off(struct work_struct *work)
{
	struct ibs_struct *ibs = container_of(work, struct ibs_struct,
					ws_rx_vote_off);
	struct hci_uart *hu = (struct hci_uart *)ibs->ibs_hu;

	BT_DBG(" %pK ", hu);

	ibs_msm_serial_clock_vote(HCI_IBS_RX_VOTE_CLOCK_OFF, hu);

}

static void ibs_wq_serial_tx_clock_vote_off(struct work_struct *work)
{
	struct ibs_struct *ibs = container_of(work, struct ibs_struct,
					ws_tx_vote_off);
	struct hci_uart *hu = (struct hci_uart *)ibs->ibs_hu;

	BT_DBG(" %pK ", hu);

	hci_uart_tx_wakeup(hu);  /* run HCI tx handling unlocked */

	/* now that message queued to tty driver, vote for tty clocks off */
	/* It is up to the tty driver to pend the clocks off until tx done. */
	ibs_msm_serial_clock_vote(HCI_IBS_TX_VOTE_CLOCK_OFF, hu);

}

static void hci_ibs_tx_idle_timeout(unsigned long arg)
{
	struct hci_uart *hu = (struct hci_uart *) arg;
	struct ibs_struct *ibs = hu->priv;
	unsigned long flags;

	BT_DBG("hu %pK idle timeout in %lu state", hu, ibs->tx_ibs_state);

	spin_lock_irqsave_nested(&ibs->hci_ibs_lock,
					flags, SINGLE_DEPTH_NESTING);

	switch (ibs->tx_ibs_state) {
	default:
	case HCI_IBS_TX_ASLEEP:
	case HCI_IBS_TX_WAKING:
		BT_ERR("spurrious timeout in tx state %ld", ibs->tx_ibs_state);
		goto out;
	case HCI_IBS_TX_AWAKE: /* TX_IDLE, go to SLEEP */
		if (send_hci_ibs_cmd(HCI_IBS_SLEEP_IND, hu) < 0) {
			BT_ERR("cannot send SLEEP to device");
			goto out;
		}
		ibs->tx_ibs_state = HCI_IBS_TX_ASLEEP;
		ibs->ibs_sent_slps++; /* debug */
		break;
	}

	queue_work(ibs->workqueue, &ibs->ws_tx_vote_off);

out:
	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);
}

static void hci_ibs_wake_retrans_timeout(unsigned long arg)
{
	struct hci_uart *hu = (struct hci_uart *) arg;
	struct ibs_struct *ibs = hu->priv;
	unsigned long flags;
	unsigned long retransmit = 0;

	BT_DBG("hu %pK wake retransmit timeout in %lu state",
	       hu, ibs->tx_ibs_state);

	spin_lock_irqsave_nested(&ibs->hci_ibs_lock,
					flags, SINGLE_DEPTH_NESTING);

	switch (ibs->tx_ibs_state) {
	default:
	case HCI_IBS_TX_ASLEEP:
	case HCI_IBS_TX_AWAKE:
		BT_ERR("spurrious timeout tx state %ld", ibs->tx_ibs_state);
		goto out;
	case HCI_IBS_TX_WAKING: /* No WAKE_ACK, retransmit WAKE */
		retransmit = 1;
		if (send_hci_ibs_cmd(HCI_IBS_WAKE_IND, hu) < 0) {
			BT_ERR("cannot acknowledge device wake up");
			goto out;
		}
		ibs->ibs_sent_wakes++; /* debug */
		mod_timer(&ibs->wake_retrans_timer, jiffies + wake_retrans);
		break;
	}
out:
	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);
	if (retransmit)
		hci_uart_tx_wakeup(hu);
}

/* Initialize protocol */
static int ibs_open(struct hci_uart *hu)
{
	struct ibs_struct *ibs;

	BT_DBG("hu %pK", hu);

	ibs = kzalloc(sizeof(*ibs), GFP_ATOMIC);
	if (!ibs)
		return -ENOMEM;

	skb_queue_head_init(&ibs->txq);
	skb_queue_head_init(&ibs->tx_wait_q);
	spin_lock_init(&ibs->hci_ibs_lock);
	ibs->workqueue = create_singlethread_workqueue("ibs_wq");
	if (!ibs->workqueue) {
		BT_ERR("IBS Workqueue not initialized properly");
		kfree(ibs);
		return -ENOMEM;
	}

	INIT_WORK(&ibs->ws_awake_rx, ibs_wq_awake_rx);
	INIT_WORK(&ibs->ws_awake_device, ibs_wq_awake_device);
	INIT_WORK(&ibs->ws_rx_vote_off, ibs_wq_serial_rx_clock_vote_off);
	INIT_WORK(&ibs->ws_tx_vote_off, ibs_wq_serial_tx_clock_vote_off);

	ibs->ibs_hu = (void *)hu;

	/* Assume we start with both sides asleep -- extra wakes OK */
	ibs->tx_ibs_state = HCI_IBS_TX_ASLEEP;
	ibs->rx_ibs_state = HCI_IBS_RX_ASLEEP;
	/* clocks actually on, but we start votes off */
	ibs->tx_vote = 0;
	ibs->rx_vote = 0;

	/* debug */
	ibs->ibs_sent_wacks = 0;
	ibs->ibs_sent_slps = 0;
	ibs->ibs_sent_wakes = 0;
	ibs->ibs_recv_wacks = 0;
	ibs->ibs_recv_slps = 0;
	ibs->ibs_recv_wakes = 0;
	ibs->vote_last_jif = jiffies;
	ibs->vote_on_ticks = 0;
	ibs->vote_off_ticks = 0;
	ibs->votes_on = 0;
	ibs->votes_off = 0;
	ibs->tx_votes_on = 0;
	ibs->tx_votes_off = 0;
	ibs->rx_votes_on = 0;
	ibs->rx_votes_off = 0;

	hu->priv = ibs;

	init_timer(&ibs->wake_retrans_timer);
	ibs->wake_retrans_timer.function = hci_ibs_wake_retrans_timeout;
	ibs->wake_retrans_timer.data     = (u_long) hu;

	init_timer(&ibs->tx_idle_timer);
	ibs->tx_idle_timer.function = hci_ibs_tx_idle_timeout;
	ibs->tx_idle_timer.data     = (u_long) hu;

	BT_INFO("HCI_IBS open, tx_idle_delay=%lu, wake_retrans=%lu",
		tx_idle_delay, wake_retrans);

	return 0;
}

void ibs_log_local_stats(struct ibs_struct *ibs)
{
	BT_INFO("HCI_IBS stats: tx_idle_delay=%lu, wake_retrans=%lu",
		tx_idle_delay, wake_retrans);

	BT_INFO("HCI_IBS stats: tx_ibs_state=%lu, rx_ibs_state=%lu",
		ibs->tx_ibs_state, ibs->rx_ibs_state);
	BT_INFO("HCI_IBS stats: sent: sleep=%lu, wake=%lu, wake_ack=%lu",
		ibs->ibs_sent_slps, ibs->ibs_sent_wakes, ibs->ibs_sent_wacks);
	BT_INFO("HCI_IBS stats: recv: sleep=%lu, wake=%lu, wake_ack=%lu",
		ibs->ibs_recv_slps, ibs->ibs_recv_wakes, ibs->ibs_recv_wacks);

	BT_INFO("HCI_IBS stats: queues: txq=%s, txwaitq=%s",
		skb_queue_empty(&(ibs->txq)) ? "empty" : "full",
		skb_queue_empty(&(ibs->tx_wait_q)) ? "empty" : "full");

	BT_INFO("HCI_IBS stats: vote state: tx=%lu, rx=%lu",
		ibs->tx_vote, ibs->rx_vote);
	BT_INFO("HCI_IBS stats: tx votes cast: on=%lu, off=%lu",
		ibs->tx_votes_on, ibs->tx_votes_off);
	BT_INFO("HCI_IBS stats: rx votes cast: on=%lu, off=%lu",
		ibs->rx_votes_on, ibs->rx_votes_off);
	BT_INFO("HCI_IBS stats: msm_clock votes cast: on=%lu, off=%lu",
		ibs->votes_on, ibs->votes_off);
	BT_INFO("HCI_IBS stats: vote ticks: on=%lu, off=%lu",
		ibs->vote_on_ticks, ibs->vote_off_ticks);
}

/* Flush protocol data */
static int ibs_flush(struct hci_uart *hu)
{
	struct ibs_struct *ibs = hu->priv;

	BT_DBG("hu %pK", hu);

	skb_queue_purge(&ibs->tx_wait_q);
	skb_queue_purge(&ibs->txq);

	return 0;
}

/* Close protocol */
static int ibs_close(struct hci_uart *hu)
{
	struct ibs_struct *ibs = hu->priv;

	BT_DBG("hu %pK", hu);

	ibs_msm_serial_clock_vote(HCI_IBS_VOTE_STATS_UPDATE, hu);
	ibs_log_local_stats(ibs);

	skb_queue_purge(&ibs->tx_wait_q);
	skb_queue_purge(&ibs->txq);
	del_timer(&ibs->tx_idle_timer);
	del_timer(&ibs->wake_retrans_timer);
	destroy_workqueue(ibs->workqueue);
	ibs->ibs_hu = NULL;

	kfree_skb(ibs->rx_skb);

	hu->priv = NULL;

	kfree(ibs);

	return 0;
}

/*
 * Called upon a wake-up-indication from the device
 */
static void ibs_device_want_to_wakeup(struct hci_uart *hu)
{
	unsigned long flags;
	struct ibs_struct *ibs = hu->priv;

	BT_DBG("hu %pK", hu);

	/* lock hci_ibs state */
	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	/* debug */
	ibs->ibs_recv_wakes++;

	switch (ibs->rx_ibs_state) {
	case HCI_IBS_RX_ASLEEP:
		/* Make sure clock is on - we may have turned clock off since
		 * receiving the wake up indicator
		 */
		/* awake rx clock */
		queue_work(ibs->workqueue, &ibs->ws_awake_rx);
		spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);
		return;
	case HCI_IBS_RX_AWAKE:
		/* Always acknowledge device wake up,
		 * sending IBS message doesn't count as TX ON.
		 */
		if (send_hci_ibs_cmd(HCI_IBS_WAKE_ACK, hu) < 0) {
			BT_ERR("cannot acknowledge device wake up");
			goto out;
		}
		ibs->ibs_sent_wacks++; /* debug */
		break;
	default:
		/* any other state is illegal */
		BT_ERR("received HCI_IBS_WAKE_IND in rx state %ld",
			ibs->rx_ibs_state);
		break;
	}

out:
	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);

	/* actually send the packets */
	hci_uart_tx_wakeup(hu);
}

/*
 * Called upon a sleep-indication from the device
 */
static void ibs_device_want_to_sleep(struct hci_uart *hu)
{
	unsigned long flags;
	struct ibs_struct *ibs = hu->priv;

	BT_DBG("hu %pK", hu);

	/* lock hci_ibs state */
	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	/* debug */
	ibs->ibs_recv_slps++;

	switch (ibs->rx_ibs_state) {
	case HCI_IBS_RX_AWAKE:
		/* update state */
		ibs->rx_ibs_state = HCI_IBS_RX_ASLEEP;
		/* vote off rx clock under workqueue */
		queue_work(ibs->workqueue, &ibs->ws_rx_vote_off);
		break;
	case HCI_IBS_RX_ASLEEP:
		/* deliberate fall-through */
	default:
		/* any other state is illegal */
		BT_ERR("received HCI_IBS_SLEEP_IND in rx state %ld",
			ibs->rx_ibs_state);
		break;
	}

	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);
}

/*
 * Called upon wake-up-acknowledgement from the device
 */
static void ibs_device_woke_up(struct hci_uart *hu)
{
	unsigned long flags;
	struct ibs_struct *ibs = hu->priv;
	struct sk_buff *skb = NULL;

	BT_DBG("hu %pK", hu);

	/* lock hci_ibs state */
	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	/* debug */
	ibs->ibs_recv_wacks++;

	switch (ibs->tx_ibs_state) {
	case HCI_IBS_TX_ASLEEP:
		/* This could be spurrious rx wake on the BT chip.
		 * Send it another SLEEP othwise it will stay awake. */
	default:
		BT_ERR("received HCI_IBS_WAKE_ACK in tx state %ld",
			ibs->tx_ibs_state);
		break;
	case HCI_IBS_TX_AWAKE:
		/* expect one if we send 2 WAKEs */
		BT_DBG("received HCI_IBS_WAKE_ACK in tx state %ld",
			ibs->tx_ibs_state);
		break;
	case HCI_IBS_TX_WAKING:
		/* send pending packets */
		while ((skb = skb_dequeue(&ibs->tx_wait_q)))
			skb_queue_tail(&ibs->txq, skb);
		/* switch timers and change state to HCI_IBS_TX_AWAKE */
		del_timer(&ibs->wake_retrans_timer);
		mod_timer(&ibs->tx_idle_timer, jiffies +
			msecs_to_jiffies(TX_IDLE_TO));
		ibs->tx_ibs_state = HCI_IBS_TX_AWAKE;
	}

	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);

	/* actually send the packets */
	hci_uart_tx_wakeup(hu);
}

/* Enqueue frame for transmittion (padding, crc, etc) */
/* may be called from two simultaneous tasklets */
static int ibs_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	unsigned long flags = 0;
	struct ibs_struct *ibs = hu->priv;

	BT_DBG("hu %pK skb %pK", hu, skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);

	/* lock hci_ibs state */
	spin_lock_irqsave(&ibs->hci_ibs_lock, flags);

	/* act according to current state */
	switch (ibs->tx_ibs_state) {
	case HCI_IBS_TX_AWAKE:
		BT_DBG("device awake, sending normally");
		skb_queue_tail(&ibs->txq, skb);
		mod_timer(&ibs->tx_idle_timer, jiffies +
			msecs_to_jiffies(TX_IDLE_TO));
		break;

	case HCI_IBS_TX_ASLEEP:
		BT_DBG("device asleep, waking up and queueing packet");
		/* save packet for later */
		skb_queue_tail(&ibs->tx_wait_q, skb);

		ibs->tx_ibs_state = HCI_IBS_TX_WAKING;
		/* schedule a work queue to wake up device */
		queue_work(ibs->workqueue, &ibs->ws_awake_device);
		break;

	case HCI_IBS_TX_WAKING:
		BT_DBG("device waking up, queueing packet");
		/* transient state; just keep packet for later */
		skb_queue_tail(&ibs->tx_wait_q, skb);
		break;

	default:
		BT_ERR("illegal tx state: %ld (losing packet)",
			ibs->tx_ibs_state);
		kfree_skb(skb);
		break;
	}

	spin_unlock_irqrestore(&ibs->hci_ibs_lock, flags);

	return 0;
}

static inline int ibs_check_data_len(struct ibs_struct *ibs, int len)
{
	register int room = skb_tailroom(ibs->rx_skb);

	BT_DBG("len %d room %d", len, room);

	if (!len) {
		hci_recv_frame(ibs->rx_skb);
	} else if (len > room) {
		BT_ERR("Data length is too large");
		kfree_skb(ibs->rx_skb);
	} else {
		ibs->rx_state = HCI_IBS_W4_DATA;
		ibs->rx_count = len;
		return len;
	}

	ibs->rx_state = HCI_IBS_W4_PACKET_TYPE;
	ibs->rx_skb   = NULL;
	ibs->rx_count = 0;

	return 0;
}

/* Recv data */
static int ibs_recv(struct hci_uart *hu, void *data, int count)
{
	struct ibs_struct *ibs = hu->priv;
	register char *ptr;
	struct hci_event_hdr *eh;
	struct hci_acl_hdr   *ah;
	struct hci_sco_hdr   *sh;
	register int len, type, dlen;

	BT_DBG("hu %pK count %d rx_state %ld rx_count %ld",
	       hu, count, ibs->rx_state, ibs->rx_count);

	ptr = data;
	while (count) {
		if (ibs->rx_count) {
			len = min_t(unsigned int, ibs->rx_count, count);
			memcpy(skb_put(ibs->rx_skb, len), ptr, len);
			ibs->rx_count -= len; count -= len; ptr += len;

			if (ibs->rx_count)
				continue;

			switch (ibs->rx_state) {
			case HCI_IBS_W4_DATA:
				BT_DBG("Complete data");
				hci_recv_frame(ibs->rx_skb);

				ibs->rx_state = HCI_IBS_W4_PACKET_TYPE;
				ibs->rx_skb = NULL;
				continue;

			case HCI_IBS_W4_EVENT_HDR:
				eh = (struct hci_event_hdr *) ibs->rx_skb->data;

				BT_DBG("Event header: evt 0x%2.2x plen %d",
					eh->evt, eh->plen);

				ibs_check_data_len(ibs, eh->plen);
				continue;

			case HCI_IBS_W4_ACL_HDR:
				ah = (struct hci_acl_hdr *) ibs->rx_skb->data;
				dlen = __le16_to_cpu(ah->dlen);

				BT_DBG("ACL header: dlen %d", dlen);

				ibs_check_data_len(ibs, dlen);
				continue;

			case HCI_IBS_W4_SCO_HDR:
				sh = (struct hci_sco_hdr *) ibs->rx_skb->data;

				BT_DBG("SCO header: dlen %d", sh->dlen);

				ibs_check_data_len(ibs, sh->dlen);
				continue;
			}
		}

		/* HCI_IBS_W4_PACKET_TYPE */
		switch ((unsigned char) *ptr) {
		case HCI_EVENT_PKT:
			BT_DBG("Event packet");
			ibs->rx_state = HCI_IBS_W4_EVENT_HDR;
			ibs->rx_count = HCI_EVENT_HDR_SIZE;
			type = HCI_EVENT_PKT;
			break;

		case HCI_ACLDATA_PKT:
			BT_DBG("ACL packet");
			ibs->rx_state = HCI_IBS_W4_ACL_HDR;
			ibs->rx_count = HCI_ACL_HDR_SIZE;
			type = HCI_ACLDATA_PKT;
			break;

		case HCI_SCODATA_PKT:
			BT_DBG("SCO packet");
			ibs->rx_state = HCI_IBS_W4_SCO_HDR;
			ibs->rx_count = HCI_SCO_HDR_SIZE;
			type = HCI_SCODATA_PKT;
			break;

		/* HCI_IBS signals */
		case HCI_IBS_SLEEP_IND:
			BT_DBG("HCI_IBS_SLEEP_IND packet");
			ibs_device_want_to_sleep(hu);
			ptr++; count--;
			continue;

		case HCI_IBS_WAKE_IND:
			BT_DBG("HCI_IBS_WAKE_IND packet");
			ibs_device_want_to_wakeup(hu);
			ptr++; count--;
			continue;

		case HCI_IBS_WAKE_ACK:
			BT_DBG("HCI_IBS_WAKE_ACK packet");
			ibs_device_woke_up(hu);
			ptr++; count--;
			continue;

		default:
			BT_ERR("Unknown HCI packet type %2.2x", (__u8)*ptr);
			hu->hdev->stat.err_rx++;
			ptr++; count--;
			continue;
		};

		ptr++; count--;

		/* Allocate packet */
		ibs->rx_skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
		if (!ibs->rx_skb) {
			BT_ERR("Can't allocate mem for new packet");
			ibs->rx_state = HCI_IBS_W4_PACKET_TYPE;
			ibs->rx_count = 0;
			return 0;
		}

		ibs->rx_skb->dev = (void *) hu->hdev;
		bt_cb(ibs->rx_skb)->pkt_type = type;
	}

	return count;
}

static struct sk_buff *ibs_dequeue(struct hci_uart *hu)
{
	struct ibs_struct *ibs = hu->priv;
	return skb_dequeue(&ibs->txq);
}

static struct hci_uart_proto ibs_p = {
	.id		= HCI_UART_IBS,
	.open		= ibs_open,
	.close		= ibs_close,
	.recv		= ibs_recv,
	.enqueue	= ibs_enqueue,
	.dequeue	= ibs_dequeue,
	.flush		= ibs_flush,
};

int ibs_init(void)
{
	int err = hci_uart_register_proto(&ibs_p);

	if (!err)
		BT_INFO("HCI_IBS protocol initialized");
	else
		BT_ERR("HCI_IBS protocol registration failed");

	return err;
}

int ibs_deinit(void)
{
	return hci_uart_unregister_proto(&ibs_p);
}

module_param(wake_retrans, ulong, 0644);
MODULE_PARM_DESC(wake_retrans, "Delay (1/HZ) to retransmit WAKE_IND");

module_param(tx_idle_delay, ulong, 0644);
MODULE_PARM_DESC(tx_idle_delay, "Delay (1/HZ) since last tx for SLEEP_IND");
