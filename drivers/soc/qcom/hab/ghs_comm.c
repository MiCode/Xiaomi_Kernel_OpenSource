/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

int physical_channel_read(struct physical_channel *pchan,
		void *payload,
		size_t read_size)
{
	struct ghs_vdev *dev  = (struct ghs_vdev *)pchan->hyp_data;

	if (!payload || !dev->read_data) {
		pr_err("invalid parameters %pK %pK offset %d read %zd\n",
			payload, dev->read_data, dev->read_offset, read_size);
		return 0;
	}

	/* size in header is only for payload excluding the header itself */
	if (dev->read_size < read_size + sizeof(struct hab_header) +
		dev->read_offset) {
		pr_warn("read %zd is less than requested %zd header %zd offset %d\n",
				dev->read_size, read_size,
				sizeof(struct hab_header), dev->read_offset);
		read_size = dev->read_size - dev->read_offset -
					sizeof(struct hab_header);
	}

	/* always skip the header */
	memcpy(payload, (unsigned char *)dev->read_data +
		sizeof(struct hab_header) + dev->read_offset, read_size);
	dev->read_offset += (int)read_size;

	return (int)read_size;
}

int physical_channel_send(struct physical_channel *pchan,
		struct hab_header *header,
		void *payload)
{
	size_t sizebytes = HAB_HEADER_GET_SIZE(*header);
	struct ghs_vdev *dev  = (struct ghs_vdev *)pchan->hyp_data;
	GIPC_Result result = GIPC_Success;
	uint8_t *msg = NULL;

	spin_lock_bh(&dev->io_lock);

	result = GIPC_PrepareMessage(dev->endpoint, sizebytes+sizeof(*header),
		(void **)&msg);
	if (result == GIPC_Full) {
		spin_unlock_bh(&dev->io_lock);
		/* need to wait for space! */
		pr_err("failed to reserve send msg for %zd bytes\n",
			sizebytes+sizeof(*header));
		return -EBUSY;
	} else if (result != GIPC_Success) {
		spin_unlock_bh(&dev->io_lock);
		pr_err("failed to send due to error %d\n", result);
		return -ENOMEM;
	}

	if (HAB_HEADER_GET_TYPE(*header) == HAB_PAYLOAD_TYPE_PROFILE) {
		struct timeval tv = {0};
		struct habmm_xing_vm_stat *pstat =
					(struct habmm_xing_vm_stat *)payload;

		do_gettimeofday(&tv);
		pstat->tx_sec = tv.tv_sec;
		pstat->tx_usec = tv.tv_usec;
	}

	memcpy(msg, header, sizeof(*header));

	if (sizebytes)
		memcpy(msg+sizeof(*header), payload, sizebytes);

	result = GIPC_IssueMessage(dev->endpoint, sizebytes+sizeof(*header),
		header->id_type_size);
	spin_unlock_bh(&dev->io_lock);
	if (result != GIPC_Success) {
		pr_err("send error %d, sz %zd, prot %x\n",
			result, sizebytes+sizeof(*header),
			   header->id_type_size);
		return -EAGAIN;
	}

	return 0;
}

void physical_channel_rx_dispatch(unsigned long physical_channel)
{
	struct hab_header header = {0};
	struct physical_channel *pchan =
		(struct physical_channel *)physical_channel;
	struct ghs_vdev *dev = (struct ghs_vdev *)pchan->hyp_data;
	GIPC_Result result = GIPC_Success;

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

	if (events & (GIPC_EVENT_RECEIVEREADY)) {
		spin_lock_bh(&pchan->rxbuf_lock);
		while (1) {
			dev->read_size = 0;
			dev->read_offset = 0;
			result = GIPC_ReceiveMessage(dev->endpoint,
					dev->read_data,
					GIPC_RECV_BUFF_SIZE_BYTES,
					&dev->read_size,
					(uint32_t *)&header.id_type_size);

			if (result == GIPC_Success || dev->read_size > 0) {
				 /* handle corrupted msg? */
				hab_msg_recv(pchan, dev->read_data);
				continue;
			} else if (result == GIPC_Empty) {
				/* no more pending msg */
				break;
			}
			pr_err("recv unhandled result %d, size %zd\n",
				result, dev->read_size);
			break;
		}
		spin_unlock_bh(&pchan->rxbuf_lock);
	}

	if (events & (GIPC_EVENT_SENDREADY))
		pr_debug("kgipc send ready\n");
}
