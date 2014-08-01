/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "ipc_api.h"

#include "danipc_k.h"
#include "danipc_lowlevel.h"

void send_pkt(struct sk_buff *skb)
{
	struct danipc_pair	*pair = (struct danipc_pair *)
						&(skb->cb[HADDR_CB_OFFSET]);
	char			*msg;

	netdev_dbg(skb->dev, "%s: pair={dst=0x%x src=0x%x}\n", __func__,
		pair->dst, pair->src);

	msg = ipc_msg_alloc(pair->src,
			pair->dst,
			skb->data,
			skb->len,
			0x12,
			pair->prio
		);

	if (likely(msg)) {
		ipc_msg_send(msg, pair->prio);
		skb->dev->stats.tx_packets++;
		skb->dev->stats.tx_bytes += skb->len;
	} else {
		pr_err("%s: ipc_msg_alloc failed!", __func__);
		skb->dev->stats.tx_dropped++;
	}

	dev_kfree_skb(skb);
}


static int delay_skb(struct sk_buff *skb, struct ipc_to_virt_map *map)
{
	int			rc;
	struct delayed_skb	*dskb = kmalloc(sizeof(*dskb), GFP_ATOMIC);

	if (dskb) {
		unsigned long	flags;
		dskb->skb = skb;
		INIT_LIST_HEAD(&dskb->list);

		spin_lock_irqsave(&skbs_lock, flags);
		list_add_tail(&delayed_skbs, &dskb->list);
		atomic_inc(&map->pending_skbs);
		spin_unlock_irqrestore(&skbs_lock, flags);

		schedule_work(&delayed_skbs_work);
		rc = NETDEV_TX_OK;
	} else {
		netdev_err(skb->dev, "cannot allocate struct delayed_skb\n");
		rc = NETDEV_TX_BUSY;	/* Try again sometime */
		skb->dev->stats.tx_dropped++;
	}
	return rc;
}

int danipc_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct danipc_pair	*pair = (struct danipc_pair *)
						&(skb->cb[HADDR_CB_OFFSET]);
	struct ipc_to_virt_map	*map = &ipc_to_virt_map[ipc_get_node(pair->dst)]
								[pair->prio];
	int			rc = NETDEV_TX_OK;

	/* DANICP is a network device, however it does not support regular IP
	 * packets. All packets not identified by DANIPC protcol (marked with
	 * COOKIE_BASE bits) are discarded.
	 */
	if ((skb->protocol & __constant_htons(0xf000)) ==
		__constant_htons(COOKIE_BASE)) {
		if (map->paddr && atomic_read(&map->pending_skbs) == 0)
			send_pkt(skb);
		else
			rc = delay_skb(skb, map);
	} else {
		dev_kfree_skb(skb);
		netdev_dbg(dev, "%s() discard packet with protocol=0x%x\n",
			__func__, ntohs(skb->protocol));
	}
	return rc;
}

static void
read_ipc_message(char *const packet, char *buf,
		struct ipc_msg_hdr *const first_hdr, const unsigned len,
		u8 cpuid, enum ipc_trns_prio prio)
{
	unsigned		data_len = IPC_FIRST_BUF_DATA_SIZE_MAX;
	unsigned		total_len = 0;
	unsigned		rest_len = len;
	uint8_t			*data_ptr = (uint8_t *)(first_hdr) +
						sizeof(struct ipc_msg_hdr);
	struct ipc_buf_hdr	*next_ptr = NULL;

	if (first_hdr->next)
		first_hdr->next = ipc_to_virt(cpuid, prio,
						(u32)first_hdr->next);
	next_ptr = first_hdr->next;

	do {
		if (total_len != 0) {
			data_len = IPC_NEXT_BUF_DATA_SIZE_MAX;
			data_ptr = (uint8_t *)(next_ptr) +
						sizeof(struct ipc_buf_hdr);
			if (next_ptr->next)
				next_ptr->next = ipc_to_virt(cpuid, prio,
						(u32)next_ptr->next);
			next_ptr = next_ptr->next;
		}

		/* Clean 2 last bits (service information) */
		next_ptr = (struct ipc_buf_hdr *)(((uint32_t)next_ptr) &
							(~IPC_BUF_TYPE_BITS));
		data_len = min(rest_len, data_len);
		rest_len -= data_len;
		memcpy(buf + total_len, data_ptr, data_len);
		total_len += data_len;
	} while ((next_ptr != NULL) && (rest_len != 0));

	ipc_buf_free(packet, prio);
}

void
handle_incoming_packet(char *const packet, u8 cpuid, enum ipc_trns_prio prio)
{
	struct ipc_msg_hdr *const first_hdr = (struct ipc_msg_hdr *)packet;
	const unsigned			msg_len = first_hdr->msg_len;

	struct sk_buff *skb = netdev_alloc_skb(danipc_dev, msg_len);

	if (skb) {
		struct danipc_pair	*pair = (struct danipc_pair *)
						&(skb->cb[HADDR_CB_OFFSET]);

		pair->dst = first_hdr->dest_aid;
		pair->src = first_hdr->src_aid;

		read_ipc_message(packet, skb->data, first_hdr, msg_len, cpuid,
					prio);

		netdev_dbg(danipc_dev, "%s() pair={dst=0x%x src=0x%x}\n",
			__func__, pair->dst, pair->src);

		skb_put(skb, msg_len);
		skb_reset_mac_header(skb);

		skb->protocol = cpu_to_be16(AGENTID_TO_COOKIE(pair->dst, prio));

		netif_rx(skb);
		danipc_dev->stats.rx_packets++;
		danipc_dev->stats.rx_bytes += skb->len;
	} else
		danipc_dev->stats.rx_dropped++;
}

int danipc_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > IPC_BUF_SIZE_MAX))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}
