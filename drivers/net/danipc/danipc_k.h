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


#ifndef __DANIPC_H__
#define __DANIPC_H__

#include <linux/netdevice.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/danipc_ioctl.h>

#define RESOURCE_NUM		3
#define IPC_BUFS_RES		0
#define AGENT_TABLE_RES		1
#define KRAIT_IPC_MUX_RES	2

/* Network device private data */
struct danipc_priv {
	struct tasklet_struct	rx_task;
	struct timer_list	timer;
	struct mutex		lock;
	resource_size_t		res_start[RESOURCE_NUM];
	resource_size_t		res_len[RESOURCE_NUM];
	unsigned		irq;
};


struct delayed_skb {
	struct list_head	list;
	struct sk_buff		*skb;
};


/* Connection information. */
struct danipc_pair {
	unsigned		prio;
	danipc_addr_t		dst;
	danipc_addr_t		src;
};


#define HADDR_CB_OFFSET		40

#define COOKIE_BASE		0xE000		/* EtherType */

#define PRIO_SHIFT			4
#define PRIO_MASK			(((1 <<  PRIO_SHIFT)) - 1)

#define COOKIE_TO_AGENTID(cookie)	((cookie - COOKIE_BASE) >> PRIO_SHIFT)
#define COOKIE_TO_PRIO(cookie)		((cookie - COOKIE_BASE) & PRIO_MASK)
#define AGENTID_TO_COOKIE(agentid, pri)	(COOKIE_BASE +			\
					  ((agentid) << PRIO_SHIFT) +	\
					  (pri))

extern int		danipc_ll_init(struct danipc_priv *priv);
extern void		danipc_ll_cleanup(void);
extern void		danipc_poll(struct net_device *dev);
extern int		danipc_ioctl(struct net_device *dev, struct ifreq *ifr,
					int cmd);
extern int		danipc_hard_start_xmit(struct sk_buff *skb,
					struct net_device *dev);

extern void		danipc_init_irq(struct net_device *dev,
					struct danipc_priv *priv);
extern irqreturn_t	danipc_interrupt(int irq, void *data);
extern void		high_prio_rx(unsigned long data);

extern void		send_pkt(struct sk_buff *skb);

extern struct list_head	delayed_skbs;
extern spinlock_t	skbs_lock;
extern struct work_struct delayed_skbs_work;

extern struct net_device	*danipc_dev;
extern int danipc_change_mtu(struct net_device *dev, int new_mtu);

#endif /* __DANIPC_H__ */
