/* Copyright (c) 2011-2013, 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __LINUX_USB_BRIDGE_H__
#define __LINUX_USB_BRIDGE_H__

#include <linux/netdevice.h>
#include <linux/usb.h>

#define MAX_INST_NAME_LEN 40

enum bridge_id {
	USB_BRIDGE_QDSS,
	USB_BRIDGE_DPL,
	USB_BRIDGE_EDL,
	MAX_BRIDGE_DEVICES,
};

static int bridge_name_to_id(const char *name)
{
	if (!name)
		goto fail;

	if (!strncasecmp(name, "qdss", MAX_INST_NAME_LEN))
		return USB_BRIDGE_QDSS;
	if (!strncasecmp(name, "dpl", MAX_INST_NAME_LEN))
		return USB_BRIDGE_DPL;
	if (!strncasecmp(name, "edl", MAX_INST_NAME_LEN))
		return USB_BRIDGE_EDL;

fail:
	return -EINVAL;
}

struct bridge_ops {
	int (*send_pkt)(void *, void *, size_t actual);

	/* flow control */
	void (*unthrottle_tx)(void *);
};

#define TX_THROTTLED BIT(0)
#define RX_THROTTLED BIT(1)

struct bridge {
	/* context of the gadget port using bridge driver */
	void *ctx;

	/*to maps bridge driver instance*/
	unsigned int ch_id;

	/*to match against bridge xport name to get bridge driver instance*/
	char *name;

	/* flow control bits */
	unsigned long flags;

	/* data/ctrl bridge callbacks */
	struct bridge_ops ops;
};

/**
 * timestamp_info: stores timestamp info for skb life cycle during data
 * transfer for tethered rmnet/DUN.
 * @created: stores timestamp at the time of creation of SKB.
 * @rx_queued: stores timestamp when SKB queued to HW to receive
 * data.
 * @rx_done: stores timestamp when skb queued to h/w is completed.
 * @rx_done_sent: stores timestamp when SKB is sent from gadget rmnet/DUN
 * driver to bridge rmnet/DUN driver or vice versa.
 * @tx_queued: stores timestamp when SKB is queued to send data.
 *
 * note that size of this struct shouldn't exceed 48bytes that's the max skb->cb
 * holds.
 */
struct timestamp_info {
	struct data_bridge	*dev;

	unsigned int		created;
	unsigned int		rx_queued;
	unsigned int		rx_done;
	unsigned int		rx_done_sent;
	unsigned int		tx_queued;
};

/* Maximum timestamp message length */
#define DBG_DATA_MSG	128UL

/* Maximum timestamp messages */
#define DBG_DATA_MAX	32UL

/* timestamp buffer descriptor */
struct timestamp_buf {
	char		(buf[DBG_DATA_MAX])[DBG_DATA_MSG];   /* buffer */
	unsigned int	idx;   /* index */
	rwlock_t	lck;   /* lock */
};

#if defined(CONFIG_USB_QTI_MDM_DATA_BRIDGE) ||	\
	defined(CONFIG_USB_QTI_MDM_DATA_BRIDGE_MODULE)

/* Bridge APIs called by gadget driver */
int data_bridge_open(struct bridge *brdg);
void data_bridge_close(unsigned int id);
int data_bridge_write(unsigned int id, struct sk_buff *skb);
int data_bridge_unthrottle_rx(unsigned int id);

#else

static inline int __maybe_unused data_bridge_open(struct bridge *brdg)
{
	return -ENODEV;
}

static inline void __maybe_unused data_bridge_close(unsigned int id) { }

static inline int __maybe_unused data_bridge_write(unsigned int id,
					    struct sk_buff *skb)
{
	return -ENODEV;
}

static inline int __maybe_unused data_bridge_unthrottle_rx(unsigned int id)
{
	return -ENODEV;
}

#endif

#endif
