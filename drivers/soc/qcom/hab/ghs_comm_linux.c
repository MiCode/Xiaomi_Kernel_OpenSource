/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "hab.h"
#include "hab_ghs.h"

inline int hab_gipc_wait_to_send(GIPC_Endpoint endpoint)
{
	(void)endpoint;

	return GIPC_Success;
}

void physical_channel_rx_dispatch(unsigned long physical_channel)
{
	struct physical_channel *pchan =
		(struct physical_channel *)physical_channel;
	struct ghs_vdev *dev = (struct ghs_vdev *)pchan->hyp_data;

	uint32_t events;
	unsigned long flags;

	spin_lock_irqsave(&pchan->rxbuf_lock, flags);
	events = kgipc_dequeue_events(dev->endpoint);
	spin_unlock_irqrestore(&pchan->rxbuf_lock, flags);

	if (events & (GIPC_EVENT_RESET))
		pr_err("hab gipc %s remote vmid %d RESET\n",
				dev->name, pchan->vmid_remote);
	if (events & (GIPC_EVENT_RESETINPROGRESS))
		pr_err("hab gipc %s remote vmid %d RESETINPROGRESS\n",
				dev->name, pchan->vmid_remote);

	if (events & (GIPC_EVENT_RECEIVEREADY))
		physical_channel_rx_dispatch_common(physical_channel);

	if (events & (GIPC_EVENT_SENDREADY))
		pr_debug("kgipc send ready\n");
}
