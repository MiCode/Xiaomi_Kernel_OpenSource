/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "hab_qvm.h"

static inline void habhyp_notify(void *commdev)
{
	struct qvm_channel *dev = (struct qvm_channel *)commdev;

	if (dev && dev->guest_ctrl)
		dev->guest_ctrl->notify = ~0;
}

int physical_channel_read(struct physical_channel *pchan,
		void *payload,
		size_t read_size)
{
	struct qvm_channel *dev  = (struct qvm_channel *)pchan->hyp_data;

	if (dev)
		return hab_pipe_read(dev->pipe_ep, payload, read_size);
	else
		return 0;
}

int physical_channel_send(struct physical_channel *pchan,
		struct hab_header *header,
		void *payload)
{
	int sizebytes = HAB_HEADER_GET_SIZE(*header);
	struct qvm_channel *dev  = (struct qvm_channel *)pchan->hyp_data;
	int total_size = sizeof(*header) + sizebytes;

	if (total_size > dev->pipe_ep->tx_info.sh_buf->size)
		return -EINVAL; /* too much data for ring */

	spin_lock_bh(&dev->io_lock);

	if ((dev->pipe_ep->tx_info.sh_buf->size -
		(dev->pipe_ep->tx_info.wr_count -
		dev->pipe_ep->tx_info.sh_buf->rd_count)) < total_size) {
		spin_unlock_bh(&dev->io_lock);
		return -EAGAIN; /* not enough free space */
	}

	if (hab_pipe_write(dev->pipe_ep,
		(unsigned char *)header,
		sizeof(*header)) != sizeof(*header)) {
		spin_unlock_bh(&dev->io_lock);
		return -EIO;
	}

	if (sizebytes) {
		if (hab_pipe_write(dev->pipe_ep,
			(unsigned char *)payload,
			sizebytes) != sizebytes) {
			spin_unlock_bh(&dev->io_lock);
			return -EIO;
		}
	}

	hab_pipe_write_commit(dev->pipe_ep);
	spin_unlock_bh(&dev->io_lock);
	habhyp_notify(dev);

	return 0;
}

void physical_channel_rx_dispatch(unsigned long data)
{
	struct hab_header header;
	struct physical_channel *pchan = (struct physical_channel *)data;
	struct qvm_channel *dev = (struct qvm_channel *)pchan->hyp_data;

	spin_lock_bh(&pchan->rxbuf_lock);
	while (1) {
		if (hab_pipe_read(dev->pipe_ep,
			(unsigned char *)&header,
			sizeof(header)) != sizeof(header))
			break; /* no data available */

		hab_msg_recv(pchan, &header);
	}
	spin_unlock_bh(&pchan->rxbuf_lock);
}
