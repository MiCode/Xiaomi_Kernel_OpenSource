/* drivers/tty/smux_loopback.c
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/types.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/smux.h>
#include "smux_private.h"

#define SMUX_LOOP_FIFO_SIZE	128

static void smux_loopback_rx_worker(struct work_struct *work);
static struct workqueue_struct *smux_loopback_wq;
static DECLARE_WORK(smux_loopback_work, smux_loopback_rx_worker);
static struct kfifo smux_loop_pkt_fifo;
static DEFINE_SPINLOCK(hw_fn_lock);

/**
 * Initialize loopback framework (called by n_smux.c).
 */
int smux_loopback_init(void)
{
	int ret = 0;

	spin_lock_init(&hw_fn_lock);
	smux_loopback_wq = create_singlethread_workqueue("smux_loopback_wq");
	if (IS_ERR(smux_loopback_wq)) {
		pr_err("%s: failed to create workqueue\n", __func__);
		return -ENOMEM;
	}

	ret |= kfifo_alloc(&smux_loop_pkt_fifo,
			SMUX_LOOP_FIFO_SIZE * sizeof(struct smux_pkt_t *),
			GFP_KERNEL);

	return ret;
}

/**
 * Simulate a write to the TTY hardware by duplicating
 * the TX packet and putting it into the RX queue.
 *
 * @pkt     Packet to write
 *
 * @returns 0 on success
 */
int smux_tx_loopback(struct smux_pkt_t *pkt_ptr)
{
	struct smux_pkt_t *send_pkt;
	unsigned long flags;
	int i;
	int ret;

	/* duplicate packet */
	send_pkt = smux_alloc_pkt();
	send_pkt->hdr = pkt_ptr->hdr;
	if (pkt_ptr->hdr.payload_len) {
		ret = smux_alloc_pkt_payload(send_pkt);
		if (ret) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(send_pkt->payload, pkt_ptr->payload,
				pkt_ptr->hdr.payload_len);
	}

	/* queue duplicate as pseudo-RX data */
	spin_lock_irqsave(&hw_fn_lock, flags);
	i = kfifo_avail(&smux_loop_pkt_fifo);
	if (i < sizeof(struct smux_pkt_t *)) {
		pr_err("%s: no space in fifo\n", __func__);
		ret = -ENOMEM;
		goto unlock;
	}

	i = kfifo_in(&smux_loop_pkt_fifo,
			&send_pkt,
			sizeof(struct smux_pkt_t *));
	if (i < 0) {
		pr_err("%s: fifo error\n", __func__);
		ret = -ENOMEM;
		goto unlock;
	}
	queue_work(smux_loopback_wq, &smux_loopback_work);
	ret = 0;

unlock:
	spin_unlock_irqrestore(&hw_fn_lock, flags);
out:
	return ret;
}

/**
 * Receive loopback byte processor.
 *
 * @pkt  Incoming packet
 */
static void smux_loopback_rx_byte(struct smux_pkt_t *pkt)
{
	static int simulated_retry_cnt;
	const char ack = SMUX_WAKEUP_ACK;

	switch (pkt->hdr.flags) {
	case SMUX_WAKEUP_REQ:
		/* reply with ACK after appropriate delays */
		++simulated_retry_cnt;
		if (simulated_retry_cnt >= smux_simulate_wakeup_delay) {
			pr_err("%s: completed %d of %d\n",
				__func__, simulated_retry_cnt,
				smux_simulate_wakeup_delay);
			pr_err("%s: simulated wakeup\n", __func__);
			simulated_retry_cnt = 0;
			smux_rx_state_machine(&ack, 1, 0);
		} else {
			/* force retry */
			pr_err("%s: dropping wakeup request %d of %d\n",
					__func__, simulated_retry_cnt,
					smux_simulate_wakeup_delay);
		}
		break;
	case SMUX_WAKEUP_ACK:
		/* this shouldn't happen since we don't send requests */
		pr_err("%s: wakeup ACK unexpected\n", __func__);
		break;

	default:
		/* invalid character */
		pr_err("%s: invalid character 0x%x\n",
				__func__, (unsigned)pkt->hdr.flags);
		break;
	}
}

/**
 * Simulated remote hardware used for local loopback testing.
 *
 * @work Not used
 */
static void smux_loopback_rx_worker(struct work_struct *work)
{
	struct smux_pkt_t *pkt;
	struct smux_pkt_t reply_pkt;
	char *data;
	int len;
	int lcid;
	int i;
	unsigned long flags;

	data = kzalloc(SMUX_MAX_PKT_SIZE, GFP_ATOMIC);

	spin_lock_irqsave(&hw_fn_lock, flags);
	while (kfifo_len(&smux_loop_pkt_fifo) >= sizeof(struct smux_pkt_t *)) {
		i = kfifo_out(&smux_loop_pkt_fifo, &pkt,
					sizeof(struct smux_pkt_t *));
		spin_unlock_irqrestore(&hw_fn_lock, flags);

		if (pkt->hdr.magic != SMUX_MAGIC) {
			pr_err("%s: invalid magic %x\n", __func__,
					pkt->hdr.magic);
			return;
		}

		lcid = pkt->hdr.lcid;

		switch (pkt->hdr.cmd) {
		case SMUX_CMD_OPEN_LCH:
			if (smux_assert_lch_id(lcid)) {
				pr_err("%s: invalid channel id %d\n",
						__func__, lcid);
				break;
			}

			if (pkt->hdr.flags & SMUX_CMD_OPEN_ACK)
				break;

			/* Reply with Open ACK */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_OPEN_LCH;
			reply_pkt.hdr.flags = SMUX_CMD_OPEN_ACK
				| SMUX_CMD_OPEN_POWER_COLLAPSE;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.hdr.pad_len = 0;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);

			/* Send Remote Open */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_OPEN_LCH;
			reply_pkt.hdr.flags = SMUX_CMD_OPEN_POWER_COLLAPSE;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.hdr.pad_len = 0;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);
			break;

		case SMUX_CMD_CLOSE_LCH:
			if (smux_assert_lch_id(lcid)) {
				pr_err("%s: invalid channel id %d\n",
						__func__, lcid);
				break;
			}

			if (pkt->hdr.flags == SMUX_CMD_CLOSE_ACK)
				break;

			/* Reply with Close ACK */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_CLOSE_LCH;
			reply_pkt.hdr.flags = SMUX_CMD_CLOSE_ACK;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.hdr.pad_len = 0;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);

			/* Send Remote Close */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_CLOSE_LCH;
			reply_pkt.hdr.flags = 0;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.hdr.pad_len = 0;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);
			break;

		case SMUX_CMD_DATA:
			if (smux_assert_lch_id(lcid)) {
				pr_err("%s: invalid channel id %d\n",
						__func__, lcid);
				break;
			}

			/* Echo back received data */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_DATA;
			reply_pkt.hdr.flags = 0;
			reply_pkt.hdr.payload_len = pkt->hdr.payload_len;
			reply_pkt.payload = pkt->payload;
			reply_pkt.hdr.pad_len = pkt->hdr.pad_len;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);
			break;

		case SMUX_CMD_STATUS:
			if (smux_assert_lch_id(lcid)) {
				pr_err("%s: invalid channel id %d\n",
						__func__, lcid);
				break;
			}

			/* Echo back received status */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = lcid;
			reply_pkt.hdr.cmd = SMUX_CMD_STATUS;
			reply_pkt.hdr.flags = pkt->hdr.flags;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.payload = NULL;
			reply_pkt.hdr.pad_len = pkt->hdr.pad_len;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);
			break;

		case SMUX_CMD_PWR_CTL:
			/* reply with ack */
			smux_init_pkt(&reply_pkt);
			reply_pkt.hdr.lcid = SMUX_BROADCAST_LCID;
			reply_pkt.hdr.cmd = SMUX_CMD_PWR_CTL;
			reply_pkt.hdr.flags = SMUX_CMD_PWR_CTL_ACK;
			reply_pkt.hdr.payload_len = 0;
			reply_pkt.payload = NULL;
			reply_pkt.hdr.pad_len = pkt->hdr.pad_len;
			smux_serialize(&reply_pkt, data, &len);
			smux_rx_state_machine(data, len, 0);
			break;

		case SMUX_CMD_BYTE:
			smux_loopback_rx_byte(pkt);
			break;

		default:
			pr_err("%s: unknown command %d\n",
					__func__, pkt->hdr.cmd);
			break;
		};

		smux_free_pkt(pkt);
		spin_lock_irqsave(&hw_fn_lock, flags);
	}
	spin_unlock_irqrestore(&hw_fn_lock, flags);
	kfree(data);
}
