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
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "ipc_api.h"
#include "danipc_k.h"
#include "danipc_lowlevel.h"

static void			handle_skbs(struct work_struct *work);

LIST_HEAD(delayed_skbs);
DEFINE_SPINLOCK(skbs_lock);
DECLARE_WORK(delayed_skbs_work, handle_skbs);


static void handle_skbs(struct work_struct *work)
{
	while (!list_empty(&delayed_skbs)) {

		struct delayed_skb *dskb = list_entry(delayed_skbs.next,
							struct delayed_skb,
							list);
		struct danipc_pair *pair = (struct danipc_pair *)
					&(dskb->skb->cb[HADDR_CB_OFFSET]);
		struct ipc_to_virt_map	*map =
			&ipc_to_virt_map[ipc_get_node(pair->dst)][pair->prio];
		unsigned long	flags;

		spin_lock_irqsave(&skbs_lock, flags);
		list_del(delayed_skbs.next);
		spin_unlock_irqrestore(&skbs_lock, flags);

		send_pkt(dskb->skb);

		/* Must decrement ref. counter after sending the packet. */
		spin_lock_irqsave(&skbs_lock, flags);
		atomic_dec(&map->pending_skbs);
		spin_unlock_irqrestore(&skbs_lock, flags);

		kfree(dskb);
	}
}
