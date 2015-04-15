/*
 * HECI bus layer messages handling
 *
 * Copyright (c) 2003-2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/export.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "heci_dev.h"
#include "hbm.h"
#include "client.h"
#include <linux/spinlock.h>

/*
#define	DEBUG_FW_BOOT_SEQ	1
#define	DUMP_CL_PROP	1
*/

#ifdef DEBUG_FW_BOOT_SEQ
unsigned char	static_fw_cl_props[6][32] = {

{0x85, 0x01, 0x00, 0x00, 0x3B, 0x79, 0x63, 0xD9, 0xCF, 0x61, 0x8E, 0x4F, 0x8C,
	0x02, 0xF2, 0xF7, 0xD0, 0x7F, 0x8E, 0x84, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x85, 0x02, 0x00, 0x00, 0xB9, 0x78, 0xCC, 0xC1, 0x93, 0xB6, 0x54, 0x4E, 0x91,
	0x91, 0x51, 0x69, 0xCB, 0x02, 0x7C, 0x25, 0x01, 0x01, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x85, 0x03, 0x00, 0x00, 0x26, 0x06, 0x05, 0x1F, 0x05, 0xD5, 0x94, 0x4E, 0xB1,
	0x89, 0x53, 0x5D, 0x7D, 0xE1, 0x9C, 0xF2, 0x01, 0x01, 0x00, 0x00, 0x34,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x85, 0x04, 0x00, 0x00, 0x54, 0x6C, 0x53, 0x28, 0x99, 0xCF, 0x27, 0x4F, 0xA6,
	0xF3, 0x49, 0x97, 0x41, 0xBA, 0xAD, 0xFE, 0x01, 0x01, 0x00, 0x00, 0x80,
	0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00},
{0x85, 0x05, 0x00, 0x00, 0x58, 0xCD, 0xAE, 0x33, 0x79, 0xB6, 0x54, 0x4E, 0x9B,
	0xD9, 0xA0, 0x4D, 0x34, 0xF0, 0xC2, 0x26, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x85, 0x06, 0x00, 0x00, 0x2E, 0x9A, 0x57, 0xBB, 0x54, 0xCC, 0x50, 0x44, 0xB1,
	0xD0, 0x5E, 0x75, 0x20, 0xDC, 0xAD, 0x25, 0x01, 0x01, 0x00, 0x00, 0x04,
	0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

};

#define	NUM_STATIC_CLIENTS	6

#endif /* DEBUG_FW_BOOT_SEQ */

#ifdef dev_dbg
#undef dev_dbg
#endif
static  void no_dev_dbg(void *v, char *s, ...)
{
}
#define dev_dbg no_dev_dbg
/*#define dev_dbg dev_err*/

/**
 * heci_hbm_me_cl_allocate - allocates storage for me clients
 *
 * @dev: the device structure
 *
	 * returns none.
 */
static void heci_hbm_me_cl_allocate(struct heci_device *dev)
{
	struct heci_me_client *clients;
	int b;

	/* count how many ME clients we have */
	for_each_set_bit(b, dev->me_clients_map, HECI_CLIENTS_MAX)
		dev->me_clients_num++;

	if (dev->me_clients_num <= 0)
		return;

	kfree(dev->me_clients);
	dev->me_clients = NULL;

	dev_dbg(&dev->pdev->dev, "memory allocation for ME clients size=%zd.\n",
		dev->me_clients_num * sizeof(struct heci_me_client));

	/* allocate storage for ME clients representation */
	clients = kcalloc(dev->me_clients_num, sizeof(struct heci_me_client),
		GFP_ATOMIC);
	if (!clients) {
		dev_err(&dev->pdev->dev, "memory allocation for ME clients failed.\n");
		dev->dev_state = HECI_DEV_RESETTING;
		heci_reset(dev, 1);
		return;
	}
	dev->me_clients = clients;
	return;
}

/**
 * heci_hbm_cl_hdr - construct client hbm header
 * @cl: - client
 * @hbm_cmd: host bus message command
 * @buf: buffer for cl header
 * @len: buffer length
 */
static inline void heci_hbm_cl_hdr(struct heci_cl *cl, u8 hbm_cmd, void *buf,
	size_t len)
{
	struct heci_hbm_cl_cmd *cmd = buf;

	memset(cmd, 0, len);

	cmd->hbm_cmd = hbm_cmd;
	cmd->host_addr = cl->host_client_id;
	cmd->me_addr = cl->me_client_id;
}

/**
 * same_disconn_addr - tells if they have the same address
 *
 * @file: private data of the file object.
 * @disconn: disconnection request.
 *
 * returns true if addres are same
 */
static inline bool heci_hbm_cl_addr_equal(struct heci_cl *cl, void *buf)
{
	struct heci_hbm_cl_cmd *cmd = buf;
	return cl->host_client_id == cmd->host_addr &&
		cl->me_client_id == cmd->me_addr;
}


int heci_hbm_start_wait(struct heci_device *dev)
{
	int ret;
	if (dev->hbm_state > HECI_HBM_START)
		return 0;

	dev_err(&dev->pdev->dev, "Going to wait for heci start hbm_state=%08X\n",
		dev->hbm_state);
	ret = wait_event_timeout(dev->wait_hbm_recvd_msg,
			dev->hbm_state >= HECI_HBM_STARTED,
			(HECI_INTEROP_TIMEOUT * HZ));

	dev_err(&dev->pdev->dev, "Woke up from waiting for heci start ret=%d hbm_state=%08X\n",
		ret, dev->hbm_state);

	if (ret <= 0 && (dev->hbm_state <= HECI_HBM_START)) {
		dev->hbm_state = HECI_HBM_IDLE;
		dev_err(&dev->pdev->dev, "wating for heci start failed ret=%d hbm_state=%08X\n",
			ret, dev->hbm_state);
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * heci_hbm_start_req - sends start request message.
 *
 * @dev: the device structure
 */
int heci_hbm_start_req(struct heci_device *dev)
{
	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	struct hbm_host_version_request *start_req;
	const size_t len = sizeof(struct hbm_host_version_request);

	heci_hbm_hdr(heci_hdr, len);

	/* host start message */
	start_req = (struct hbm_host_version_request *)data;
	memset(start_req, 0, len);
	start_req->hbm_cmd = HOST_START_REQ_CMD;
	start_req->host_version.major_version = HBM_MAJOR_VERSION;
	start_req->host_version.minor_version = HBM_MINOR_VERSION;

	/*
	 * (!) Response to HBM start may be so quick that this thread would get
	 * preempted BEFORE managing to set hbm_state = HECI_HBM_START.
	 * So set it at first, change back to HECI_HBM_IDLE upon failure
	 */
	dev->hbm_state = HECI_HBM_START;
	if (heci_write_message(dev, heci_hdr, data)) {
		dev_err(&dev->pdev->dev, "version message write failed\n");
		dev->dev_state = HECI_DEV_RESETTING;
		dev->hbm_state = HECI_HBM_IDLE;
		heci_reset(dev, 1);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(heci_hbm_start_req);

/*
 * heci_hbm_enum_clients_req - sends enumeration client request message.
 *
 * @dev: the device structure
 *
 * returns none.
 */
void heci_hbm_enum_clients_req(struct heci_device *dev)
{
	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	struct hbm_host_enum_request *enum_req;
	const size_t len = sizeof(struct hbm_host_enum_request);
	/* enumerate clients */
	heci_hbm_hdr(heci_hdr, len);

	enum_req = (struct hbm_host_enum_request *)data;
	memset(enum_req, 0, len);
	enum_req->hbm_cmd = HOST_ENUM_REQ_CMD;

	if (heci_write_message(dev, heci_hdr, data)) {
		dev->dev_state = HECI_DEV_RESETTING;
		dev_err(&dev->pdev->dev, "enumeration request write failed.\n");
		heci_reset(dev, 1);
	}
	dev->hbm_state = HECI_HBM_ENUM_CLIENTS;
	return;
}

/**
 * heci_hbm_prop_requsest - request property for a single client
 *
 * @dev: the device structure
 *
 * returns none.
 */

static int heci_hbm_prop_req(struct heci_device *dev)
{

	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	struct hbm_props_request *prop_req;
	const size_t len = sizeof(struct hbm_props_request);
	unsigned long next_client_index;
	u8 client_num;

	client_num = dev->me_client_presentation_num;

	next_client_index = find_next_bit(dev->me_clients_map, HECI_CLIENTS_MAX,
		dev->me_client_index);

	/* We got all client properties */
	if (next_client_index == HECI_CLIENTS_MAX) {
		dev->hbm_state = HECI_HBM_WORKING;
		dev->dev_state = HECI_DEV_ENABLED;

		for (dev->me_client_presentation_num = 1;
			dev->me_client_presentation_num < client_num + 1;
				++dev->me_client_presentation_num)
			/* Add new client device */
			heci_bus_new_client(dev);

		return 0;
	}

	dev->me_clients[client_num].client_id = next_client_index;

#ifndef DEBUG_FW_BOOT_SEQ
	heci_hbm_hdr(heci_hdr, len);
	prop_req = (struct hbm_props_request *)data;

	memset(prop_req, 0, sizeof(struct hbm_props_request));

	prop_req->hbm_cmd = HOST_CLIENT_PROPERTIES_REQ_CMD;
	prop_req->address = next_client_index;

	if (heci_write_message(dev, heci_hdr, data)) {
		dev->dev_state = HECI_DEV_RESETTING;
		dev_err(&dev->pdev->dev, "properties request write failed\n");
		heci_reset(dev, 1);
		return -EIO;
	}
#endif /*DEBUG_FW_BOOT_SEQ*/

	dev->me_client_index = next_client_index;

#ifdef DEBUG_FW_BOOT_SEQ
	heci_hbm_dispatch(dev,
		(struct heci_bus_message *)static_fw_cl_props[client_num]);
#endif /*DEBUG_FW_BOOT_SEQ*/

	return 0;
}

/**
 * heci_hbm_stop_req_prepare - perpare stop request message
 *
 * @dev - heci device
 * @heci_hdr - heci message header
 * @data - hbm message body buffer
 */
static void heci_hbm_stop_req_prepare(struct heci_device *dev,
	struct heci_msg_hdr *heci_hdr, unsigned char *data)
{
	struct hbm_host_stop_request *req =
		(struct hbm_host_stop_request *)data;
	const size_t len = sizeof(struct hbm_host_stop_request);

	heci_hbm_hdr(heci_hdr, len);

	memset(req, 0, len);
	req->hbm_cmd = HOST_STOP_REQ_CMD;
	req->reason = DRIVER_STOP_REQUEST;
}

/**
 * heci_hbm_cl_flow_control_req - sends flow control requst.
 *
 * @dev: the device structure
 * @cl: client info
 *
 * This function returns -EIO on write failure
 */
int heci_hbm_cl_flow_control_req(struct heci_device *dev, struct heci_cl *cl)
{
	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	const size_t len = sizeof(struct hbm_flow_control);
	int	rv;
	unsigned	num_frags;
	unsigned long	flags;

	spin_lock_irqsave(&cl->fc_spinlock, flags);
	heci_hbm_hdr(heci_hdr, len);
	heci_hbm_cl_hdr(cl, HECI_FLOW_CONTROL_CMD, data, len);

	dev_dbg(&dev->pdev->dev, "sending flow control host client = %d, ME client = %d\n",
		cl->host_client_id, cl->me_client_id);

	/* Sync possible race when RB recycle and packet receive paths
	   both try to send an out FC */
	if (cl->out_flow_ctrl_creds) {
		spin_unlock_irqrestore(&cl->fc_spinlock, flags);
		return	0;
	}

	num_frags = cl->recv_msg_num_frags;
	cl->recv_msg_num_frags = 0;

	rv = heci_write_message(dev, heci_hdr, data);
	if (!rv) {
		struct timeval	tv;

		++cl->out_flow_ctrl_creds;
		++cl->out_flow_ctrl_cnt;
		do_gettimeofday(&tv);
		cl->out_fc_sec = tv.tv_sec;
		cl->out_fc_usec = tv.tv_usec;
		if (cl->rx_sec && cl->rx_usec) {
			unsigned long	s, us;

			s = cl->out_fc_sec - cl->rx_sec;
			us = cl->out_fc_usec - cl->rx_usec;
			if (cl->rx_usec > cl->out_fc_usec) {
				us += 1000000UL;
				--s;
			}
			if (s > cl->max_fc_delay_sec ||
					s == cl->max_fc_delay_sec &&
					us > cl->max_fc_delay_usec) {
				cl->max_fc_delay_sec = s;
				cl->max_fc_delay_usec = us;
			}
		}

	} else {
		++cl->err_send_fc;
	}

	spin_unlock_irqrestore(&cl->fc_spinlock, flags);
	return	rv;
}
EXPORT_SYMBOL(heci_hbm_cl_flow_control_req);

/*
 * heci_hbm_cl_disconnect_req - sends disconnect message to fw.
 *
 * @dev: the device structure
 * @cl: a client to disconnect from
 *
 * This function returns -EIO on write failure
 */
int heci_hbm_cl_disconnect_req(struct heci_device *dev, struct heci_cl *cl)
{
	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	const size_t len = sizeof(struct hbm_client_connect_request);

	heci_hbm_hdr(heci_hdr, len);
	heci_hbm_cl_hdr(cl, CLIENT_DISCONNECT_REQ_CMD, data, len);

	return heci_write_message(dev, heci_hdr, data);
}

/*
 * heci_hbm_cl_disconnect_res - disconnect response from ME
 *
 * @dev: the device structure
 * @rs: disconnect response bus message
 */
static void heci_hbm_cl_disconnect_res(struct heci_device *dev,
	struct hbm_client_connect_response *rs)
{
	struct heci_cl *cl = NULL, *next = NULL;
	unsigned long	flags;

	dev_dbg(&dev->pdev->dev,
			"disconnect_response:\n"
			"ME Client = %d\n"
			"Host Client = %d\n"
			"Status = %d\n",
			rs->me_addr,
			rs->host_addr,
			rs->status);

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
		if (!rs->status && heci_hbm_cl_addr_equal(cl, rs)) {
			cl->state = HECI_CL_DISCONNECTED;
			break;
		}
	}
	if (cl)
		wake_up(&cl->wait_ctrl_res);
	spin_unlock_irqrestore(&dev->device_lock, flags);
}

/**
 * heci_hbm_cl_connect_req - send connection request to specific me client
 *
 * @dev: the device structure
 * @cl: a client to connect to
 *
 * returns -EIO on write failure
 */
int heci_hbm_cl_connect_req(struct heci_device *dev, struct heci_cl *cl)
{
	struct heci_msg_hdr hdr;
	unsigned char data[128];
	struct heci_msg_hdr *heci_hdr = &hdr;
	const size_t len = sizeof(struct hbm_client_connect_request);

	heci_hbm_hdr(heci_hdr, len);
	heci_hbm_cl_hdr(cl, CLIENT_CONNECT_REQ_CMD, data, len);

	return heci_write_message(dev, heci_hdr,  data);
}
EXPORT_SYMBOL(heci_hbm_cl_connect_req);

/**
 * heci_hbm_cl_connect_res - connect resposne from the ME
 *
 * @dev: the device structure
 * @rs: connect response bus message
 */
static void heci_hbm_cl_connect_res(struct heci_device *dev,
	struct hbm_client_connect_response *rs)
{
	struct heci_cl *cl = NULL, *next = NULL;
	unsigned long	flags;

	dev_dbg(&dev->pdev->dev,
			"connect_response:\n"
			"ME Client = %d\n"
			"Host Client = %d\n"
			"Status = %d\n",
			rs->me_addr,
			rs->host_addr,
			rs->status);

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
		if (heci_hbm_cl_addr_equal(cl, rs)) {
			if (!rs->status) {
				cl->state = HECI_CL_CONNECTED;
				cl->status = 0;
			} else {
				cl->state = HECI_CL_DISCONNECTED;
				cl->status = -ENODEV;
			}
			break;
		}
	}
	if (cl)
		wake_up(&cl->wait_ctrl_res);
	spin_unlock_irqrestore(&dev->device_lock, flags);
}


/**
 * heci_client_disconnect_request - disconnect request initiated by me
 *  host sends disoconnect response
 *
 * @dev: the device structure.
 * @disconnect_req: disconnect request bus message from the me
 */
static void heci_hbm_fw_disconnect_req(struct heci_device *dev,
	struct hbm_client_connect_request *disconnect_req)
{
	struct heci_cl *cl, *next;
	const size_t len = sizeof(struct hbm_client_connect_response);
	unsigned long	flags;
	struct heci_msg_hdr hdr;
	unsigned char data[4];	/* All HBM messages are 4 bytes */

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
		if (heci_hbm_cl_addr_equal(cl, disconnect_req)) {
			cl->state = HECI_CL_DISCONNECTED;

			/* prepare disconnect response */
			heci_hbm_hdr(&hdr, len);
			heci_hbm_cl_hdr(cl, CLIENT_DISCONNECT_RES_CMD, data,
				len);
			heci_write_message(dev, &hdr, data);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->device_lock, flags);
}


/**
 * heci_hbm_dispatch - bottom half read routine after ISR to
 * handle the read bus message cmd processing.
 *
 * @dev: the device structure
 * @hdr: header of bus message
 */
void heci_hbm_dispatch(struct heci_device *dev, struct heci_bus_message *hdr)
{
	struct heci_bus_message *heci_msg;
	struct heci_me_client *me_client;
	struct hbm_host_version_response *version_res;
	struct hbm_client_connect_response *connect_res;
	struct hbm_client_connect_response *disconnect_res;
	struct hbm_client_connect_request *disconnect_req;
	struct hbm_props_response *props_res;
	struct hbm_host_enum_response *enum_res;
	struct heci_msg_hdr heci_hdr;
	unsigned char data[4];	/* All HBM messages are 4 bytes */

	heci_msg = hdr;
	dev_dbg(&dev->pdev->dev, "bus cmd = %lu\n", heci_msg->hbm_cmd);

	switch (heci_msg->hbm_cmd) {
	case HOST_START_RES_CMD:
		version_res = (struct hbm_host_version_response *)heci_msg;
		if (!version_res->host_version_supported) {
			dev->version = version_res->me_max_version;
			dev_dbg(&dev->pdev->dev, "version mismatch.\n");

			dev->hbm_state = HECI_HBM_STOPPED;
			heci_hbm_stop_req_prepare(dev, &heci_hdr, data);
			heci_write_message(dev, &heci_hdr, data);
			return;
		}

		dev->version.major_version = HBM_MAJOR_VERSION;
		dev->version.minor_version = HBM_MINOR_VERSION;
		if (dev->dev_state == HECI_DEV_INIT_CLIENTS &&
		    dev->hbm_state == HECI_HBM_START) {
			dev->hbm_state = HECI_HBM_STARTED;
			heci_hbm_enum_clients_req(dev);
		} else {
			dev_err(&dev->pdev->dev, "reset: wrong host start response\n");
			/* BUG: why do we arrive here? */
			heci_reset(dev, 1);
			return;
		}

		wake_up(&dev->wait_hbm_recvd_msg);
		dev_dbg(&dev->pdev->dev, "host start response message received.\n");
		break;

	case CLIENT_CONNECT_RES_CMD:
		connect_res = (struct hbm_client_connect_response *)heci_msg;
		heci_hbm_cl_connect_res(dev, connect_res);
		dev_dbg(&dev->pdev->dev, "client connect response message received.\n");
		break;

	case CLIENT_DISCONNECT_RES_CMD:
		disconnect_res = (struct hbm_client_connect_response *)heci_msg;
		heci_hbm_cl_disconnect_res(dev, disconnect_res);
		dev_dbg(&dev->pdev->dev, "client disconnect response message received.\n");
		break;

	case HOST_CLIENT_PROPERTIES_RES_CMD:
		props_res = (struct hbm_props_response *)heci_msg;
		me_client = &dev->me_clients[dev->me_client_presentation_num];

#ifdef DUMP_CL_PROP
		/* DEBUG -- dump complete response */
		do {
			int	i;

			dev->print_log(dev,
				"%s(): HOST_CLIENT_PROPERTIES_RES_CMD, client# = %d props: ",
				__func__, dev->me_client_presentation_num);
			for (i = 0; i < sizeof(struct hbm_props_response); ++i)
				dev->print_log(dev, "%02X ",
					*(((unsigned char *)props_res) + i));
			dev->print_log(dev, "\n");
		} while (0);
#endif /*DUMP_CL_PROP*/

		if (props_res->status || !dev->me_clients) {
			dev_err(&dev->pdev->dev, "reset: properties response hbm wrong status.\n");
			heci_reset(dev, 1);
			return;
		}

		if (me_client->client_id != props_res->address) {
			dev_err(&dev->pdev->dev, "reset: host properties response address mismatch [%02X %02X]\n",
				me_client->client_id, props_res->address);
			heci_reset(dev, 1);
			return;
		}

		if (dev->dev_state != HECI_DEV_INIT_CLIENTS ||
		    dev->hbm_state != HECI_HBM_CLIENT_PROPERTIES) {
			dev_err(&dev->pdev->dev, "reset: unexpected properties response\n");
			heci_reset(dev, 1);

			return;
		}

		me_client->props = props_res->client_properties;
		dev->me_client_index++;
		dev->me_client_presentation_num++;

#if 0
		/* DEBUG -- dump received client's GUID */
		do {
			int	i;

			ISH_DBG_PRINT(
				KERN_ALERT "%s(): idx=%d protocol_name = ",
				__func__, dev->me_client_presentation_num - 1);
			for (i = 0; i <  16; ++i)
				ISH_DBG_PRINT(KERN_ALERT "%02X ",
					(unsigned)me_client->props.protocol_name.b[i]);
			ISH_DBG_PRINT(KERN_ALERT "\n");
		} while (0);
#endif

#if 0
		/* Add new client device */
		heci_bus_new_client(dev);
#endif

		/* request property for the next client */
		heci_hbm_prop_req(dev);

		break;

	case HOST_ENUM_RES_CMD:
		enum_res = (struct hbm_host_enum_response *) heci_msg;
		memcpy(dev->me_clients_map, enum_res->valid_addresses, 32);
		if (dev->dev_state == HECI_DEV_INIT_CLIENTS &&
		    dev->hbm_state == HECI_HBM_ENUM_CLIENTS) {
				dev->me_client_presentation_num = 0;
				dev->me_client_index = 0;
				heci_hbm_me_cl_allocate(dev);
				dev->hbm_state = HECI_HBM_CLIENT_PROPERTIES;

				/* first property request */
				heci_hbm_prop_req(dev);
		} else {
			dev_err(&dev->pdev->dev, "reset: unexpected enumeration response hbm.\n");
			heci_reset(dev, 1);
			return;
		}
		break;

	case HOST_STOP_RES_CMD:
		if (dev->hbm_state != HECI_HBM_STOPPED)
			dev_err(&dev->pdev->dev, "unexpected stop response.\n");

		dev->dev_state = HECI_DEV_DISABLED;
		dev_info(&dev->pdev->dev, "reset: FW stop response.\n");
		heci_reset(dev, 1);
		break;

	case CLIENT_DISCONNECT_REQ_CMD:
		/* search for client */
		disconnect_req = (struct hbm_client_connect_request *)heci_msg;
		heci_hbm_fw_disconnect_req(dev, disconnect_req);
		break;

	case ME_STOP_REQ_CMD:
		dev->hbm_state = HECI_HBM_STOPPED;
		break;

	case CLIENT_DMA_RES_CMD:
		/*
		 * TODO: wake up anybody who could be
		 * waiting for DMA completion
		 */
		dma_ready = 1;
		if (waitqueue_active(&dev->wait_dma_ready))
			wake_up(&dev->wait_dma_ready);
		break;

	default:
		/*BUG();*/
		dev_err(&dev->pdev->dev, "unknown HBM: %u\n",
			(unsigned)heci_msg->hbm_cmd);
		break;

	}
}
EXPORT_SYMBOL(heci_hbm_dispatch);


/*
 *	Receive and process HECI bus messages
 *
 *	(!) ISR context
 */
void	recv_hbm(struct heci_device *dev, struct heci_msg_hdr *heci_hdr)
{
	uint8_t	rd_msg_buf[HECI_RD_MSG_BUF_SIZE];
	struct heci_bus_message	*heci_msg =
		(struct heci_bus_message *)rd_msg_buf;
	unsigned long	flags, tx_flags;

	dev->ops->read(dev, rd_msg_buf, heci_hdr->length);

	/* Flow control - handle in place */
	if (heci_msg->hbm_cmd == HECI_FLOW_CONTROL_CMD) {
		struct hbm_flow_control *flow_control =
			(struct hbm_flow_control *)heci_msg;
		struct heci_cl *cl = NULL;
		struct heci_cl *next = NULL;
		unsigned long	flags;

		ISH_DBG_PRINT(KERN_ALERT
			"%s(): HECI_FLOW_CONTROL_CMD, checking to whom (host_addr=%d me_addr=%d\n",
			__func__, flow_control->host_addr,
			flow_control->me_addr);
		spin_lock_irqsave(&dev->device_lock, flags);
		list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
			if (cl->host_client_id == flow_control->host_addr &&
					cl->me_client_id ==
					flow_control->me_addr) {
				/*##########################################*/
				/*
				 * FIXME: It's valid only for counting
				 * flow-control implementation to receive a
				 * FC in the middle of sending
				 */
				if (cl->heci_flow_ctrl_creds)
					dev_err(&dev->pdev->dev,
						"recv extra FC from FW client %u (host client %u) (FC count was %u)\n",
						(unsigned)cl->me_client_id,
						(unsigned)cl->host_client_id,
						(unsigned)cl->heci_flow_ctrl_creds);
				else {
					if (cl->host_client_id == 3 &&
							cl->me_client_id == 5) {
						++dev->ipc_hid_in_fc;
						++dev->ipc_hid_in_fc_cnt;
					}
					++cl->heci_flow_ctrl_creds;
					++cl->heci_flow_ctrl_cnt;
					spin_lock_irqsave(&cl->tx_list_spinlock,
						tx_flags);
				if (!list_empty(&cl->tx_list.list)) {
					/*
					 * start sending the first msg
					 *	= the callback function
					 */
					spin_unlock_irqrestore(
							&cl->tx_list_spinlock,
							tx_flags);
					heci_cl_send_msg(dev, cl);
				} else {
						spin_unlock_irqrestore(
							&cl->tx_list_spinlock,
							tx_flags);
					}
				}
				break;
				/*##########################################*/
			}
		}
		spin_unlock_irqrestore(&dev->device_lock, flags);
		goto	eoi;
	}

	/*
	 * Some messages that are safe for ISR processing and important
	 * to be done "quickly" and in-order, go here
	 */
	if (heci_msg->hbm_cmd == CLIENT_CONNECT_RES_CMD ||
			heci_msg->hbm_cmd == CLIENT_DISCONNECT_RES_CMD ||
			heci_msg->hbm_cmd == CLIENT_DISCONNECT_REQ_CMD) {
		heci_hbm_dispatch(dev, heci_msg);
		goto	eoi;
	}

	/* TODO: revise, may be some don't need BH as well */
	/*
	 * All other HBMs go here.
	 * We schedule HBMs for processing serially,
	 * possibly there will be multiplpe HBMs scheduled at the same time.
	 * System wq itself is a serializing means
	 */
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	if ((dev->rd_msg_fifo_tail + IPC_PAYLOAD_SIZE) %
			(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE) ==
			dev->rd_msg_fifo_head) {
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
		dev_err(&dev->pdev->dev, "BH buffer overflow, dropping HBM %u\n",
			(unsigned)heci_msg->hbm_cmd);
		goto	eoi;
	}
	memcpy(dev->rd_msg_fifo + dev->rd_msg_fifo_tail, heci_msg,
		heci_hdr->length);
	dev->rd_msg_fifo_tail = (dev->rd_msg_fifo_tail + IPC_PAYLOAD_SIZE) %
		(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE);
	spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
	schedule_work(&dev->bh_hbm_work);
eoi:
	return;
}
EXPORT_SYMBOL(recv_hbm);

/* Suspend and resume notification*/

/*
 *      Receive and process HECI fixed client messages
 *
 *      (!) ISR context
 */
void recv_fixed_cl_msg(struct heci_device *dev, struct heci_msg_hdr *heci_hdr)
{
	uint8_t rd_msg_buf[HECI_RD_MSG_BUF_SIZE];

	dev->print_log(dev,
		"%s() got fixed client msg from client #%d\n",
		__func__, heci_hdr->me_addr);
	dev->ops->read(dev, rd_msg_buf, heci_hdr->length);
	if (heci_hdr->me_addr == HECI_SYSTEM_STATE_CLIENT_ADDR) {
		struct ish_system_states_header *msg_hdr =
			(struct ish_system_states_header *)rd_msg_buf;
		if (msg_hdr->cmd == SYSTEM_STATE_SUBSCRIBE)
			send_resume(dev);       /* if FW request arrived here,
						the system is not suspended */
		else
			dev_err(&dev->pdev->dev,
				"unknown fixed client msg [%02X]\n",
				msg_hdr->cmd);
	}
}
EXPORT_SYMBOL(recv_fixed_cl_msg);

static inline void fix_cl_hdr(struct heci_msg_hdr *hdr, size_t length,
	u8 cl_addr)
{
	hdr->host_addr = 0;
	hdr->me_addr = cl_addr;
	hdr->length = length;
	hdr->msg_complete = 1;
	hdr->reserved = 0;
}

/*Global var for suspend & resume*/
u32 current_state = 0;
u32 supported_states = 0 | SUSPEND_STATE_BIT;

void send_suspend(struct heci_device *dev)
{
	struct heci_msg_hdr     heci_hdr;
	struct ish_system_states_status state_status_msg;
	const size_t len = sizeof(struct ish_system_states_status);

	fix_cl_hdr(&heci_hdr, len, HECI_SYSTEM_STATE_CLIENT_ADDR);

	memset(&state_status_msg, 0, len);
	state_status_msg.hdr.cmd = SYSTEM_STATE_STATUS;
	state_status_msg.supported_states = supported_states;
	current_state |= SUSPEND_STATE_BIT;
	dev->print_log(dev, "%s() sends SUSPEND notification\n", __func__);
	state_status_msg.states_status = current_state;

	heci_write_message(dev, &heci_hdr, &state_status_msg);
}
EXPORT_SYMBOL(send_suspend);

void send_resume(struct heci_device *dev)
{
	struct heci_msg_hdr     heci_hdr;
	struct ish_system_states_status state_status_msg;
	const size_t len = sizeof(struct ish_system_states_status);

	fix_cl_hdr(&heci_hdr, len, HECI_SYSTEM_STATE_CLIENT_ADDR);

	memset(&state_status_msg, 0, len);
	state_status_msg.hdr.cmd = SYSTEM_STATE_STATUS;
	state_status_msg.supported_states = supported_states;
	current_state &= ~SUSPEND_STATE_BIT;
	dev->print_log(dev, "%s() sends RESUME notification\n", __func__);
	state_status_msg.states_status = current_state;

	heci_write_message(dev, &heci_hdr, &state_status_msg);
}
EXPORT_SYMBOL(send_resume);

void query_subscribers(struct heci_device *dev)
{
	struct heci_msg_hdr     heci_hdr;
	struct ish_system_states_query_subscribers query_subscribers_msg;
	const size_t len = sizeof(struct ish_system_states_query_subscribers);

	fix_cl_hdr(&heci_hdr, len, HECI_SYSTEM_STATE_CLIENT_ADDR);

	memset(&query_subscribers_msg, 0, len);
	query_subscribers_msg.hdr.cmd = SYSTEM_STATE_QUERY_SUBSCRIBERS;

	heci_write_message(dev, &heci_hdr, &query_subscribers_msg);
}

