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
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/wait.h>
#include <linux/component.h>
#include <soc/qcom/tracer_pkt.h>
#include <sound/wcd-dsp-mgr.h>
#include <sound/wcd-spi.h>
#include "glink_core_if.h"
#include "glink_private.h"
#include "glink_xprt_if.h"

#define XPRT_NAME "spi"
#define FIFO_ALIGNMENT 16
#define FIFO_FULL_RESERVE 8
#define TX_BLOCKED_CMD_RESERVE 16
#define TRACER_PKT_FEATURE BIT(2)
#define DEFAULT_FIFO_SIZE 1024
#define SHORT_PKT_SIZE 16
#define XPRT_ALIGNMENT 4

#define MAX_INACTIVE_CYCLES 50
#define POLL_INTERVAL_US 500

#define ACTIVE_TX BIT(0)
#define ACTIVE_RX BIT(1)

#define ID_MASK 0xFFFFFF
/**
 * enum command_types - definition of the types of commands sent/received
 * @VERSION_CMD:		Version and feature set supported
 * @VERSION_ACK_CMD:		Response for @VERSION_CMD
 * @OPEN_CMD:			Open a channel
 * @CLOSE_CMD:			Close a channel
 * @OPEN_ACK_CMD:		Response to @OPEN_CMD
 * @CLOSE_ACK_CMD:		Response for @CLOSE_CMD
 * @RX_INTENT_CMD:		RX intent for a channel is queued
 * @RX_DONE_CMD:		Use of RX intent for a channel is complete
 * @RX_DONE_W_REUSE_CMD:	Same as @RX_DONE but also reuse the used intent
 * @RX_INTENT_REQ_CMD:		Request to have RX intent queued
 * @RX_INTENT_REQ_ACK_CMD:	Response for @RX_INTENT_REQ_CMD
 * @TX_DATA_CMD:		Start of a data transfer
 * @TX_DATA_CONT_CMD:		Continuation or end of a data transfer
 * @READ_NOTIF_CMD:		Request for a notification when this cmd is read
 * @SIGNALS_CMD:		Sideband signals
 * @TRACER_PKT_CMD:		Start of a Tracer Packet Command
 * @TRACER_PKT_CONT_CMD:	Continuation or end of a Tracer Packet Command
 * @TX_SHORT_DATA_CMD:		Transmit short packets
 */
enum command_types {
	VERSION_CMD,
	VERSION_ACK_CMD,
	OPEN_CMD,
	CLOSE_CMD,
	OPEN_ACK_CMD,
	CLOSE_ACK_CMD,
	RX_INTENT_CMD,
	RX_DONE_CMD,
	RX_DONE_W_REUSE_CMD,
	RX_INTENT_REQ_CMD,
	RX_INTENT_REQ_ACK_CMD,
	TX_DATA_CMD,
	TX_DATA_CONT_CMD,
	READ_NOTIF_CMD,
	SIGNALS_CMD,
	TRACER_PKT_CMD,
	TRACER_PKT_CONT_CMD,
	TX_SHORT_DATA_CMD,
};

/**
 * struct glink_cmpnt - Component to cache WDSP component and its operations
 * @master_dev:	Device structure corresponding to WDSP device.
 * @master_ops:	Operations supported by the WDSP device.
 */
struct glink_cmpnt {
	struct device *master_dev;
	struct wdsp_mgr_ops *master_ops;
};

/**
 * struct edge_info - local information for managing a single complete edge
 * @xprt_if:			The transport interface registered with the
 *				glink core associated with this edge.
 * @xprt_cfg:			The transport configuration for the glink core
 *				assocaited with this edge.
 * @subsys_name:		Name of the remote subsystem in the edge.
 * @spi_dev:			Pointer to the connectingSPI Device.
 * @fifo_size:			Size of the FIFO at the remote end.
 * @tx_fifo_start:		Base Address of the TX FIFO.
 * @tx_fifo_end:		End Address of the TX FIFO.
 * @rx_fifo_start:		Base Address of the RX FIFO.
 * @rx_fifo_end:		End Address of the RX FIFO.
 * @tx_fifo_read_reg_addr:	Address of the TX FIFO Read Index Register.
 * @tx_fifo_write_reg_addr:	Address of the TX FIFO Write Index Register.
 * @rx_fifo_read_reg_addr:	Address of the RX FIFO Read Index Register.
 * @rx_fifo_write_reg_addr:	Address of the RX FIFO Write Index Register.
 * @kwork:			Work to be executed when receiving data.
 * @kworker:			Handle to the entity processing @kwork.
 * @task:			Handle to the task context that runs @kworker.
 * @use_ref:			Active users of this transport grab a
 *				reference. Used for SSR synchronization.
 * @in_ssr:			Signals if this transport is in ssr.
 * @write_lock:			Lock to serialize write/tx operation.
 * @tx_blocked_queue:		Queue of entities waiting for the remote side to
 *				signal the resumption of TX.
 * @tx_resume_needed:		A tx resume signal needs to be sent to the glink
 *				core.
 * @tx_blocked_signal_sent:	Flag to indicate the flush signal has already
 *				been sent, and a response is pending from the
 *				remote side.  Protected by @write_lock.
 * @num_pw_states:		Size of @ramp_time_us.
 * @ramp_time_us:		Array of ramp times in microseconds where array
 *				index position represents a power state.
 * @activity_flag:		Flag indicating active TX and RX.
 * @activity_lock:		Lock to synchronize access to activity flag.
 * @cmpnt:			Component to interface with the remote device.
 */
struct edge_info {
	struct list_head list;
	struct glink_transport_if xprt_if;
	struct glink_core_transport_cfg xprt_cfg;
	char subsys_name[GLINK_NAME_SIZE];
	struct spi_device *spi_dev;

	uint32_t fifo_size;
	uint32_t tx_fifo_start;
	uint32_t tx_fifo_end;
	uint32_t rx_fifo_start;
	uint32_t rx_fifo_end;
	unsigned int tx_fifo_read_reg_addr;
	unsigned int tx_fifo_write_reg_addr;
	unsigned int rx_fifo_read_reg_addr;
	unsigned int rx_fifo_write_reg_addr;

	struct kthread_work kwork;
	struct kthread_worker kworker;
	struct task_struct *task;
	struct srcu_struct use_ref;
	bool in_ssr;
	struct mutex write_lock;
	wait_queue_head_t tx_blocked_queue;
	bool tx_resume_needed;
	bool tx_blocked_signal_sent;

	uint32_t num_pw_states;
	unsigned long *ramp_time_us;

	uint32_t activity_flag;
	spinlock_t activity_lock;

	struct glink_cmpnt cmpnt;
};

static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features);
static DEFINE_SPINLOCK(edge_infos_lock);
static LIST_HEAD(edge_infos);
static struct glink_core_version versions[] = {
	{1, TRACER_PKT_FEATURE, negotiate_features_v1},
};

/**
 * negotiate_features_v1() - determine what features of a version can be used
 * @if_ptr:	The transport for which features are negotiated for.
 * @version:	The version negotiated.
 * @features:	The set of requested features.
 *
 * Return: What set of the requested features can be supported.
 */
static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features)
{
	return features & version->features;
}

/**
 * wdsp_suspend() - Vote for the WDSP device suspend
 * @cmpnt:	Component to identify the WDSP device.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int wdsp_suspend(struct glink_cmpnt *cmpnt)
{
	int rc = 0;

	if (cmpnt && cmpnt->master_dev &&
	    cmpnt->master_ops && cmpnt->master_ops->suspend)
		rc = cmpnt->master_ops->suspend(cmpnt->master_dev);
	return rc;
}

/**
 * wdsp_resume() - Vote for the WDSP device resume
 * @cmpnt:	Component to identify the WDSP device.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int wdsp_resume(struct glink_cmpnt *cmpnt)
{
	int rc = 0;

	if (cmpnt && cmpnt->master_dev &&
	    cmpnt->master_ops && cmpnt->master_ops->resume)
		rc = cmpnt->master_ops->resume(cmpnt->master_dev);
	return rc;
}

/**
 * glink_spi_xprt_set_poll_mode() - Set the transport to polling mode
 * @einfo:	Edge information corresponding to the transport.
 *
 * This helper function indicates the start of RX polling. This will
 * prevent the system from suspending and keeps polling for RX for a
 * pre-defined duration.
 */
static void glink_spi_xprt_set_poll_mode(struct edge_info *einfo)
{
	unsigned long flags;

	spin_lock_irqsave(&einfo->activity_lock, flags);
	einfo->activity_flag |= ACTIVE_RX;
	spin_unlock_irqrestore(&einfo->activity_lock, flags);
	if (!strcmp(einfo->xprt_cfg.edge, "wdsp"))
		wdsp_resume(&einfo->cmpnt);
}

/**
 * glink_spi_xprt_set_irq_mode() - Set the transport to IRQ mode
 * @einfo:	Edge information corresponding to the transport.
 *
 * This helper indicates the end of RX polling. This will allow the
 * system to suspend and new RX data can be handled only through an IRQ.
 */
static void glink_spi_xprt_set_irq_mode(struct edge_info *einfo)
{
	unsigned long flags;

	spin_lock_irqsave(&einfo->activity_lock, flags);
	einfo->activity_flag &= ~ACTIVE_RX;
	spin_unlock_irqrestore(&einfo->activity_lock, flags);
}

/**
 * glink_spi_xprt_rx_data() - Receive data over SPI bus
 * @einfo:	Edge from which the data has to be received.
 * @src:	Source Address of the RX data.
 * @dst:	Address of the destination RX buffer.
 * @size:	Size of the RX data.
 *
 * This function is used to receive data or command as a byte stream from
 * the remote subsystem over the SPI bus.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_xprt_rx_data(struct edge_info *einfo, void *src,
				  void *dst, uint32_t size)
{
	struct wcd_spi_msg spi_msg;

	memset(&spi_msg, 0, sizeof(spi_msg));
	spi_msg.data = dst;
	spi_msg.remote_addr = (uint32_t)(size_t)src;
	spi_msg.len = (size_t)size;
	return wcd_spi_data_read(einfo->spi_dev, &spi_msg);
}

/**
 * glink_spi_xprt_tx_data() - Transmit data over SPI bus
 * @einfo:	Edge from which the data has to be received.
 * @src:	Address of the TX buffer.
 * @dst:	Destination Address of the TX Date.
 * @size:	Size of the TX data.
 *
 * This function is used to transmit data or command as a byte stream to
 * the remote subsystem over the SPI bus.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_xprt_tx_data(struct edge_info *einfo, void *src,
				  void *dst, uint32_t size)
{
	struct wcd_spi_msg spi_msg;

	memset(&spi_msg, 0, sizeof(spi_msg));
	spi_msg.data = src;
	spi_msg.remote_addr = (uint32_t)(size_t)dst;
	spi_msg.len = (size_t)size;
	return wcd_spi_data_write(einfo->spi_dev, &spi_msg);
}

/**
 * glink_spi_xprt_reg_read() - Read the TX/RX FIFO Read/Write Index registers
 * @einfo:	Edge from which the registers have to be read.
 * @reg_addr:	Address of the register to be read.
 * @data:	Buffer into which the register data has to be read.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_xprt_reg_read(struct edge_info *einfo, u32 reg_addr,
				   uint32_t *data)
{
	int rc;

	rc = glink_spi_xprt_rx_data(einfo, (void *)(unsigned long)reg_addr,
				    data, sizeof(*data));
	if (!rc)
		*data = *data & ID_MASK;
	return rc;
}

/**
 * glink_spi_xprt_reg_write() - Write the TX/RX FIFO Read/Write Index registers
 * @einfo:	Edge to which the registers have to be written.
 * @reg_addr:	Address of the registers to be written.
 * @data:	Data to be written to the registers.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_xprt_reg_write(struct edge_info *einfo, u32 reg_addr,
					uint32_t data)
{
	return glink_spi_xprt_tx_data(einfo, &data,
				(void *)(unsigned long)reg_addr, sizeof(data));
}

/**
 * glink_spi_xprt_write_avail() - Available Write Space in the remote side
 * @einfo:	Edge information corresponding to the remote side.
 *
 * This function reads the TX FIFO Read & Write Index registers from the
 * remote subsystem and calculate the available write space.
 *
 * Return: 0 on error, available write space on success.
 */
static int glink_spi_xprt_write_avail(struct edge_info *einfo)
{
	uint32_t read_id;
	uint32_t write_id;
	int write_avail;
	int ret;

	ret = glink_spi_xprt_reg_read(einfo, einfo->tx_fifo_read_reg_addr,
				   &read_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s tx_fifo_read_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->tx_fifo_read_reg_addr);
		return 0;
	}

	ret = glink_spi_xprt_reg_read(einfo, einfo->tx_fifo_write_reg_addr,
				&write_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s tx_fifo_write_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->tx_fifo_write_reg_addr);
		return 0;
	}

	if (!read_id || !write_id)
		return 0;

	if (unlikely(!einfo->tx_fifo_start))
		einfo->tx_fifo_start = write_id;

	if (read_id > write_id)
		write_avail = read_id - write_id;
	else
		write_avail = einfo->fifo_size - (write_id - read_id);

	if (write_avail < FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE)
		write_avail = 0;
	else
		write_avail -= FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE;

	return write_avail;
}

/**
 * glink_spi_xprt_read_avail() - Available Read Data from the remote side
 * @einfo:	Edge information corresponding to the remote side.
 *
 * This function reads the RX FIFO Read & Write Index registers from the
 * remote subsystem and calculate the available read data size.
 *
 * Return: 0 on error, available read data on success.
 */
static int glink_spi_xprt_read_avail(struct edge_info *einfo)
{
	uint32_t read_id;
	uint32_t write_id;
	int read_avail;
	int ret;

	ret = glink_spi_xprt_reg_read(einfo, einfo->rx_fifo_read_reg_addr,
				   &read_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s rx_fifo_read_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->rx_fifo_read_reg_addr);
		return 0;
	}

	ret = glink_spi_xprt_reg_read(einfo, einfo->rx_fifo_write_reg_addr,
				&write_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s rx_fifo_write_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->rx_fifo_write_reg_addr);
		return 0;
	}

	if (!read_id || !write_id)
		return 0;

	if (unlikely(!einfo->rx_fifo_start))
		einfo->rx_fifo_start = read_id;

	if (read_id <= write_id)
		read_avail = write_id - read_id;
	else
		read_avail = einfo->fifo_size - (read_id - write_id);
	return read_avail;
}

/**
 * glink_spi_xprt_rx_cmd() - Receive G-Link commands
 * @einfo:	Edge information corresponding to the remote side.
 * @dst:	Destination buffer where the commands have to be read into.
 * @size:	Size of the data to be read.
 *
 * This function is used to receive the commands from the RX FIFO. This
 * function updates the RX FIFO Read Index after reading the data.
 *
 * Return: 0 on success, standard Linux error codes on error.
 */
static int glink_spi_xprt_rx_cmd(struct edge_info *einfo, void *dst,
				 uint32_t size)
{
	uint32_t read_id;
	uint32_t size_to_read = size;
	uint32_t offset = 0;
	int ret;

	ret = glink_spi_xprt_reg_read(einfo, einfo->rx_fifo_read_reg_addr,
				   &read_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s rx_fifo_read_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->rx_fifo_read_reg_addr);
		return ret;
	}

	do {
		if ((read_id + size_to_read) >=
		    (einfo->rx_fifo_start + einfo->fifo_size))
			size_to_read = einfo->rx_fifo_start + einfo->fifo_size
					- read_id;
		ret = glink_spi_xprt_rx_data(einfo, (void *)(size_t)read_id,
					     dst + offset, size_to_read);
		if (ret < 0) {
			pr_err("%s: Error %d reading data\n", __func__, ret);
			return ret;
		}
		read_id += size_to_read;
		offset += size_to_read;
		if (read_id >= (einfo->rx_fifo_start + einfo->fifo_size))
			read_id = einfo->rx_fifo_start;
		size_to_read = size - offset;
	} while (size_to_read);

	ret = glink_spi_xprt_reg_write(einfo, einfo->rx_fifo_read_reg_addr,
				read_id);
	if (ret < 0)
		pr_err("%s: Error %d writing %s rx_fifo_read_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->rx_fifo_read_reg_addr);
	return ret;
}

/**
 * glink_spi_xprt_tx_cmd_safe() - Transmit G-Link commands
 * @einfo:	Edge information corresponding to the remote subsystem.
 * @src:	Source buffer containing the G-Link command.
 * @size:	Size of the command to transmit.
 *
 * This function is used to transmit the G-Link commands. This function
 * must be called with einfo->write_lock locked.
 *
 * Return: 0 on success, standard Linux error codes on error.
 */
static int glink_spi_xprt_tx_cmd_safe(struct edge_info *einfo, void *src,
				      uint32_t size)
{
	uint32_t write_id;
	uint32_t size_to_write = size;
	uint32_t offset = 0;
	int ret;

	ret = glink_spi_xprt_reg_read(einfo, einfo->tx_fifo_write_reg_addr,
				&write_id);
	if (ret < 0) {
		pr_err("%s: Error %d reading %s tx_fifo_write_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->tx_fifo_write_reg_addr);
		return ret;
	}

	do {
		if ((write_id + size_to_write) >=
		    (einfo->tx_fifo_start + einfo->fifo_size))
			size_to_write = einfo->tx_fifo_start + einfo->fifo_size
					- write_id;
		ret = glink_spi_xprt_tx_data(einfo, src + offset,
				(void *)(size_t)write_id, size_to_write);
		if (ret < 0) {
			pr_err("%s: Error %d writing data\n", __func__, ret);
			return ret;
		}
		write_id += size_to_write;
		offset += size_to_write;
		if (write_id >= (einfo->tx_fifo_start + einfo->fifo_size))
			write_id = einfo->tx_fifo_start;
		size_to_write = size - offset;
	} while (size_to_write);

	ret = glink_spi_xprt_reg_write(einfo, einfo->tx_fifo_write_reg_addr,
				write_id);
	if (ret < 0)
		pr_err("%s: Error %d writing %s tx_fifo_write_reg_addr %d\n",
			__func__, ret, einfo->xprt_cfg.edge,
			einfo->tx_fifo_write_reg_addr);
	return ret;
}

/**
 * send_tx_blocked_signal() - Send flow control request message
 * @einfo:	Edge information corresponding to the remote subsystem.
 *
 * This function is used to send a message to the remote subsystem indicating
 * that the local subsystem is waiting for the write space. The remote
 * subsystem on receiving this message will send a resume tx message.
 */
static void send_tx_blocked_signal(struct edge_info *einfo)
{
	struct read_notif_request {
		uint16_t cmd;
		uint16_t reserved;
		uint32_t reserved2;
		uint64_t reserved3;
	};
	struct read_notif_request read_notif_req = {0};

	read_notif_req.cmd = READ_NOTIF_CMD;

	if (!einfo->tx_blocked_signal_sent) {
		einfo->tx_blocked_signal_sent = true;
		glink_spi_xprt_tx_cmd_safe(einfo, &read_notif_req,
					    sizeof(read_notif_req));
	}
}

/**
 * glink_spi_xprt_tx_cmd() - Transmit G-Link commands
 * @einfo:	Edge information corresponding to the remote subsystem.
 * @src:	Source buffer containing the G-Link command.
 * @size:	Size of the command to transmit.
 *
 * This function is used to transmit the G-Link commands. This function
 * might sleep if the space is not available to transmit the command.
 *
 * Return: 0 on success, standard Linux error codes on error.
 */
static int glink_spi_xprt_tx_cmd(struct edge_info *einfo, void *src,
				 uint32_t size)
{
	int ret;
	DEFINE_WAIT(wait);

	mutex_lock(&einfo->write_lock);
	while (glink_spi_xprt_write_avail(einfo) < size) {
		send_tx_blocked_signal(einfo);
		prepare_to_wait(&einfo->tx_blocked_queue, &wait,
				TASK_UNINTERRUPTIBLE);
		if (glink_spi_xprt_write_avail(einfo) < size &&
		    !einfo->in_ssr) {
			mutex_unlock(&einfo->write_lock);
			schedule();
			mutex_lock(&einfo->write_lock);
		}
		finish_wait(&einfo->tx_blocked_queue, &wait);
		if (einfo->in_ssr) {
			mutex_unlock(&einfo->write_lock);
			return -EFAULT;
		}
	}
	ret = glink_spi_xprt_tx_cmd_safe(einfo, src, size);
	mutex_unlock(&einfo->write_lock);
	return ret;
}

/**
 * process_rx_data() - process received data from an edge
 * @einfo:		The edge the data is received on.
 * @cmd_id:		ID to specify the type of data.
 * @rcid:		The remote channel id associated with the data.
 * @intend_id:		The intent the data should be put in.
 * @src:		Address of the source buffer from which the data
 *			is read.
 * @frag_size:		Size of the data fragment to read.
 * @size_remaining:	Size of data left to be read in this packet.
 */
static void process_rx_data(struct edge_info *einfo, uint16_t cmd_id,
			    uint32_t rcid, uint32_t intent_id, void *src,
			    uint32_t frag_size, uint32_t size_remaining)
{
	struct glink_core_rx_intent *intent;
	int rc = 0;

	intent = einfo->xprt_if.glink_core_if_ptr->rx_get_pkt_ctx(
				&einfo->xprt_if, rcid, intent_id);
	if (intent == NULL) {
		GLINK_ERR("%s: no intent for ch %d liid %d\n", __func__, rcid,
			  intent_id);
		return;
	} else if (intent->data == NULL) {
		GLINK_ERR("%s: intent for ch %d liid %d has no data buff\n",
			  __func__, rcid, intent_id);
		return;
	} else if (intent->intent_size - intent->write_offset < frag_size ||
		 intent->write_offset + size_remaining > intent->intent_size) {
		GLINK_ERR("%s: rx data size:%d and remaining:%d %s %d %s:%d\n",
			  __func__, frag_size, size_remaining,
			  "will overflow ch", rcid, "intent", intent_id);
		return;
	}

	if (cmd_id == TX_SHORT_DATA_CMD)
		memcpy(intent->data + intent->write_offset, src, frag_size);
	else
		rc = glink_spi_xprt_rx_data(einfo, src,
				intent->data + intent->write_offset, frag_size);
	if (rc < 0) {
		GLINK_ERR("%s: Error %d receiving data %d:%d:%d:%d\n",
			  __func__, rc, rcid, intent_id, frag_size,
			  size_remaining);
		size_remaining += frag_size;
	} else {
		intent->write_offset += frag_size;
		intent->pkt_size += frag_size;

		if (unlikely((cmd_id == TRACER_PKT_CMD ||
			cmd_id == TRACER_PKT_CONT_CMD) && !size_remaining)) {
			tracer_pkt_log_event(intent->data, GLINK_XPRT_RX);
			intent->tracer_pkt = true;
		}
	}
	einfo->xprt_if.glink_core_if_ptr->rx_put_pkt_ctx(&einfo->xprt_if,
				rcid, intent, size_remaining ? false : true);
}

/**
 * process_rx_cmd() - Process incoming G-Link commands
 * @einfo:	Edge information corresponding to the remote subsystem.
 * @rx_data:	Buffer which contains the G-Link commands to be processed.
 * @rx_size:	Size of the buffer containing the series of G-Link commands.
 *
 * This function is used to parse and process a series of G-Link commands
 * received in a buffer.
 */
static void process_rx_cmd(struct edge_info *einfo,
			   void *rx_data, int rx_size)
{
	struct command {
		uint16_t id;
		uint16_t param1;
		uint32_t param2;
		uint32_t param3;
		uint32_t param4;
	};
	struct intent_desc {
		uint32_t size;
		uint32_t id;
		uint64_t addr;
	};
	struct rx_desc {
		uint32_t size;
		uint32_t size_left;
		uint64_t addr;
	};
	struct rx_short_data_desc {
		unsigned char data[SHORT_PKT_SIZE];
	};
	struct command *cmd;
	struct intent_desc *intents;
	struct rx_desc *rx_descp;
	struct rx_short_data_desc *rx_sd_descp;
	int offset = 0;
	int rcu_id;
	uint16_t rcid;
	uint16_t name_len;
	uint16_t prio;
	char *name;
	bool granted;
	int i;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	while (offset < rx_size) {
		cmd = (struct command *)(rx_data + offset);
		offset += sizeof(*cmd);
		switch (cmd->id) {
		case VERSION_CMD:
			if (cmd->param3)
				einfo->fifo_size = cmd->param3;
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_version(
				&einfo->xprt_if, cmd->param1, cmd->param2);
			break;

		case VERSION_ACK_CMD:
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_version_ack(
				&einfo->xprt_if, cmd->param1, cmd->param2);
			break;

		case OPEN_CMD:
			rcid = cmd->param1;
			name_len = (uint16_t)(cmd->param2 & 0xFFFF);
			prio = (uint16_t)((cmd->param2 & 0xFFFF0000) >> 16);
			name = (char *)(rx_data + offset);
			offset += ALIGN(name_len, FIFO_ALIGNMENT);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_open(
				&einfo->xprt_if, rcid, name, prio);
			break;

		case CLOSE_CMD:
			einfo->xprt_if.glink_core_if_ptr->
					rx_cmd_ch_remote_close(
						&einfo->xprt_if, cmd->param1);
			break;

		case OPEN_ACK_CMD:
			prio = (uint16_t)(cmd->param2 & 0xFFFF);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_open_ack(
				&einfo->xprt_if, cmd->param1, prio);
			break;

		case CLOSE_ACK_CMD:
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(
					&einfo->xprt_if, cmd->param1);
			break;

		case RX_INTENT_CMD:
			for (i = 0; i < cmd->param2; i++) {
				intents = (struct intent_desc *)
						(rx_data + offset);
				offset += sizeof(*intents);
				einfo->xprt_if.glink_core_if_ptr->
					rx_cmd_remote_rx_intent_put_cookie(
					&einfo->xprt_if, cmd->param1,
					intents->id, intents->size,
					(void *)(uintptr_t)(intents->addr));
			}
			break;

		case RX_DONE_CMD:
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_tx_done(
				&einfo->xprt_if, cmd->param1, cmd->param2,
				false);
			break;

		case RX_INTENT_REQ_CMD:
			einfo->xprt_if.glink_core_if_ptr->
				rx_cmd_remote_rx_intent_req(
					&einfo->xprt_if, cmd->param1,
					cmd->param2);
			break;

		case RX_INTENT_REQ_ACK_CMD:
			granted = cmd->param2 == 1 ? true : false;
			einfo->xprt_if.glink_core_if_ptr->
				rx_cmd_rx_intent_req_ack(&einfo->xprt_if,
						cmd->param1, granted);
			break;

		case TX_DATA_CMD:
		case TX_DATA_CONT_CMD:
		case TRACER_PKT_CMD:
		case TRACER_PKT_CONT_CMD:
			rx_descp = (struct rx_desc *)(rx_data + offset);
			offset += sizeof(*rx_descp);
			process_rx_data(einfo, cmd->id, cmd->param1,
					cmd->param2,
					(void *)(uintptr_t)(rx_descp->addr),
					rx_descp->size, rx_descp->size_left);
			break;

		case TX_SHORT_DATA_CMD:
			rx_sd_descp = (struct rx_short_data_desc *)
							(rx_data + offset);
			offset += sizeof(*rx_sd_descp);
			process_rx_data(einfo, cmd->id, cmd->param1,
					cmd->param2, (void *)rx_sd_descp->data,
					cmd->param3, cmd->param4);
			break;

		case READ_NOTIF_CMD:
			break;

		case SIGNALS_CMD:
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_remote_sigs(
				&einfo->xprt_if, cmd->param1, cmd->param2);
			break;

		case RX_DONE_W_REUSE_CMD:
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_tx_done(
				&einfo->xprt_if, cmd->param1,
				cmd->param2, true);
			break;

		default:
			pr_err("Unrecognized command: %d\n", cmd->id);
			break;
		}
	}
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * __rx_worker() - Receive commands on a specific edge
 * @einfo:      Edge to process commands on.
 *
 * This function checks the size of data to be received, allocates the
 * buffer for that data and reads the data from the remote subsytem
 * into that buffer. This function then calls the process_rx_cmd() to
 * parse the received G-Link command sequence. This function will also
 * poll for the data for a predefined duration for performance reasons.
 */
static void __rx_worker(struct edge_info *einfo)
{
	uint32_t inactive_cycles = 0;
	int rx_avail, rc;
	void *rx_data;
	int rcu_id;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	if (unlikely(!einfo->rx_fifo_start)) {
		rx_avail = glink_spi_xprt_read_avail(einfo);
		if (!rx_avail) {
			srcu_read_unlock(&einfo->use_ref, rcu_id);
			return;
		}
		einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
	}

	glink_spi_xprt_set_poll_mode(einfo);
	while (inactive_cycles < MAX_INACTIVE_CYCLES) {
		if (einfo->tx_resume_needed &&
		    glink_spi_xprt_write_avail(einfo)) {
			einfo->tx_resume_needed = false;
			einfo->xprt_if.glink_core_if_ptr->tx_resume(
							&einfo->xprt_if);
		}
		mutex_lock(&einfo->write_lock);
		if (einfo->tx_blocked_signal_sent) {
			wake_up_all(&einfo->tx_blocked_queue);
			einfo->tx_blocked_signal_sent = false;
		}
		mutex_unlock(&einfo->write_lock);

		rx_avail = glink_spi_xprt_read_avail(einfo);
		if (!rx_avail) {
			usleep_range(POLL_INTERVAL_US, POLL_INTERVAL_US + 50);
			inactive_cycles++;
			continue;
		}
		inactive_cycles = 0;

		rx_data = kzalloc(rx_avail, GFP_KERNEL);
		if (!rx_data)
			break;

		rc = glink_spi_xprt_rx_cmd(einfo, rx_data, rx_avail);
		if (rc < 0) {
			GLINK_ERR("%s: Error %d receiving data\n",
				  __func__, rc);
			kfree(rx_data);
			break;
		}
		process_rx_cmd(einfo, rx_data, rx_avail);
		kfree(rx_data);
	}
	glink_spi_xprt_set_irq_mode(einfo);
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * rx_worker() - Worker function to process received commands
 * @work:       kwork associated with the edge to process commands on.
 */
static void rx_worker(struct kthread_work *work)
{
	struct edge_info *einfo;

	einfo = container_of(work, struct edge_info, kwork);
	__rx_worker(einfo);
};

/**
 * tx_cmd_version() - Convert a version cmd to wire format and transmit
 * @if_ptr:     The transport to transmit on.
 * @version:    The version number to encode.
 * @features:   The features information to encode.
 */
static void tx_cmd_version(struct glink_transport_if *if_ptr, uint32_t version,
			   uint32_t features)
{
	struct command {
		uint16_t id;
		uint16_t version;
		uint32_t features;
		uint32_t fifo_size;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = VERSION_CMD;
	cmd.version = version;
	cmd.features = features;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_version_ack() - Convert a version ack cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @version:	The version number to encode.
 * @features:	The features information to encode.
 */
static void tx_cmd_version_ack(struct glink_transport_if *if_ptr,
			       uint32_t version,
			       uint32_t features)
{
	struct command {
		uint16_t id;
		uint16_t version;
		uint32_t features;
		uint32_t fifo_size;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = VERSION_ACK_CMD;
	cmd.version = version;
	cmd.features = features;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * set_version() - Activate a negotiated version and feature set
 * @if_ptr:	The transport to configure.
 * @version:	The version to use.
 * @features:	The features to use.
 *
 * Return: The supported capabilities of the transport.
 */
static uint32_t set_version(struct glink_transport_if *if_ptr, uint32_t version,
			uint32_t features)
{
	struct edge_info *einfo;
	uint32_t ret;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return 0;
	}

	ret = GCAP_SIGNALS;
	if (features & TRACER_PKT_FEATURE)
		ret |= GCAP_TRACER_PKT;

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return ret;
}

/**
 * tx_cmd_ch_open() - Convert a channel open cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @name:	The channel name to encode.
 * @req_xprt:	The transport the core would like to migrate this channel to.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_open(struct glink_transport_if *if_ptr, uint32_t lcid,
			  const char *name, uint16_t req_xprt)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint16_t length;
		uint16_t req_xprt;
		uint64_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	uint32_t buf_size;
	void *buf;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = OPEN_CMD;
	cmd.lcid = lcid;
	cmd.length = (uint16_t)(strlen(name) + 1);
	cmd.req_xprt = req_xprt;

	buf_size = ALIGN(sizeof(cmd) + cmd.length, FIFO_ALIGNMENT);

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -ENOMEM;
	}

	memcpy(buf, &cmd, sizeof(cmd));
	memcpy(buf + sizeof(cmd), name, cmd.length);

	glink_spi_xprt_tx_cmd(einfo, buf, buf_size);

	kfree(buf);
	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_close() - Convert a channel close cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_close(struct glink_transport_if *if_ptr, uint32_t lcid)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t reserved1;
		uint64_t reserved2;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = CLOSE_CMD;
	cmd.lcid = lcid;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_remote_open_ack() - Convert a channel open ack cmd to wire format
 *				 and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 * @xprt_resp:	The response to a transport migration request.
 */
static void tx_cmd_ch_remote_open_ack(struct glink_transport_if *if_ptr,
				     uint32_t rcid, uint16_t xprt_resp)
{
	struct command {
		uint16_t id;
		uint16_t rcid;
		uint16_t reserved1;
		uint16_t xprt_resp;
		uint64_t reserved2;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = OPEN_ACK_CMD;
	cmd.rcid = rcid;
	cmd.xprt_resp = xprt_resp;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_ch_remote_close_ack() - Convert a channel close ack cmd to wire format
 *				  and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 */
static void tx_cmd_ch_remote_close_ack(struct glink_transport_if *if_ptr,
				       uint32_t rcid)
{
	struct command {
		uint16_t id;
		uint16_t rcid;
		uint32_t reserved1;
		uint64_t reserved2;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = CLOSE_ACK_CMD;
	cmd.rcid = rcid;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * ssr() - Process a subsystem restart notification of a transport
 * @if_ptr:	The transport to restart
 *
 * Return: 0 on success or standard Linux error code.
 */
static int ssr(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	einfo->in_ssr = true;
	wake_up_all(&einfo->tx_blocked_queue);

	synchronize_srcu(&einfo->use_ref);
	einfo->tx_resume_needed = false;
	einfo->tx_blocked_signal_sent = false;
	einfo->tx_fifo_start = 0;
	einfo->rx_fifo_start = 0;
	einfo->fifo_size = DEFAULT_FIFO_SIZE;
	einfo->xprt_if.glink_core_if_ptr->link_down(&einfo->xprt_if);

	return 0;
}

/**
 * allocate_rx_intent() - Allocate/reserve space for RX Intent
 * @if_ptr:	The transport the intent is associated with.
 * @size:	size of intent.
 * @intent:	Pointer to the intent structure.
 *
 * Assign "data" with the buffer created, since the transport creates
 * a linear buffer and "iovec" with the "intent" itself, so that
 * the data can be passed to a client that receives only vector buffer.
 * Note that returning NULL for the pointer is valid (it means that space has
 * been reserved, but the actual pointer will be provided later).
 *
 * Return: 0 on success or standard Linux error code.
 */
static int allocate_rx_intent(struct glink_transport_if *if_ptr, size_t size,
			      struct glink_core_rx_intent *intent)
{
	void *t;

	t = kzalloc(size, GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	intent->data = t;
	intent->iovec = (void *)intent;
	intent->vprovider = rx_linear_vbuf_provider;
	intent->pprovider = NULL;
	return 0;
}

/**
 * deallocate_rx_intent() - Deallocate space created for RX Intent
 * @if_ptr:	The transport the intent is associated with.
 * @intent:	Pointer to the intent structure.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int deallocate_rx_intent(struct glink_transport_if *if_ptr,
				struct glink_core_rx_intent *intent)
{
	if (!intent || !intent->data)
		return -EINVAL;

	kfree(intent->data);
	intent->data = NULL;
	intent->iovec = NULL;
	intent->vprovider = NULL;
	return 0;
}

/**
 * tx_cmd_local_rx_intent() - Convert an rx intent cmd to wire format and
 *			      transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The intent size to encode.
 * @liid:	The local intent id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_local_rx_intent(struct glink_transport_if *if_ptr,
				  uint32_t lcid, size_t size, uint32_t liid)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t count;
		uint64_t reserved;
		uint32_t size;
		uint32_t liid;
		uint64_t addr;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	if (size > UINT_MAX) {
		pr_err("%s: size %zu is too large to encode\n", __func__, size);
		return -EMSGSIZE;
	}

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_CMD;
	cmd.lcid = lcid;
	cmd.count = 1;
	cmd.size = size;
	cmd.liid = liid;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_local_rx_done() - Convert an rx done cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @liid:	The local intent id to encode.
 * @reuse:	Reuse the consumed intent.
 */
static void tx_cmd_local_rx_done(struct glink_transport_if *if_ptr,
				 uint32_t lcid, uint32_t liid, bool reuse)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t liid;
		uint64_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = reuse ? RX_DONE_W_REUSE_CMD : RX_DONE_CMD;
	cmd.lcid = lcid;
	cmd.liid = liid;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_rx_intent_req() - Convert an rx intent request cmd to wire format and
 *			    transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The requested intent size to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_rx_intent_req(struct glink_transport_if *if_ptr,
				uint32_t lcid, size_t size)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t size;
		uint64_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	if (size > UINT_MAX) {
		pr_err("%s: size %zu is too large to encode\n", __func__, size);
		return -EMSGSIZE;
	}

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_REQ_CMD,
	cmd.lcid = lcid;
	cmd.size = size;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_rx_intent_req_ack() - Convert an rx intent request ack cmd to wire
 *				format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @granted:	The request response to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_remote_rx_intent_req_ack(struct glink_transport_if *if_ptr,
					   uint32_t lcid, bool granted)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t response;
		uint64_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_REQ_ACK_CMD,
	cmd.lcid = lcid;
	if (granted)
		cmd.response = 1;
	else
		cmd.response = 0;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_set_sigs() - Convert a signals ack cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @sigs:	The signals to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_set_sigs(struct glink_transport_if *if_ptr, uint32_t lcid,
			   uint32_t sigs)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t sigs;
		uint64_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = SIGNALS_CMD,
	cmd.lcid = lcid;
	cmd.sigs = sigs;

	glink_spi_xprt_tx_cmd(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_data() - convert a data/tracer_pkt to wire format and transmit
 * @if_ptr:     The transport to transmit on.
 * @cmd_id:     The command ID to transmit.
 * @lcid:       The local channel id to encode.
 * @pctx:       The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx_data(struct glink_transport_if *if_ptr, uint16_t cmd_id,
		   uint32_t lcid, struct glink_core_tx_pkt *pctx)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t riid;
		uint64_t reserved;
		uint32_t size;
		uint32_t size_left;
		uint64_t addr;
	};
	struct command cmd;
	struct edge_info *einfo;
	uint32_t size;
	void *data_start, *dst = NULL;
	size_t tx_size = 0;
	int rcu_id;

	if (pctx->size < pctx->size_remaining) {
		GLINK_ERR("%s: size remaining exceeds size.  Resetting.\n",
			  __func__);
		pctx->size_remaining = pctx->size;
	}
	if (!pctx->size_remaining)
		return 0;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	if (cmd_id == TX_DATA_CMD) {
		if (pctx->size_remaining == pctx->size)
			cmd.id = TX_DATA_CMD;
		else
			cmd.id = TX_DATA_CONT_CMD;
	} else {
		if (pctx->size_remaining == pctx->size)
			cmd.id = TRACER_PKT_CMD;
		else
			cmd.id = TRACER_PKT_CONT_CMD;
	}
	cmd.lcid = lcid;
	cmd.riid = pctx->riid;
	data_start = get_tx_vaddr(pctx, pctx->size - pctx->size_remaining,
				  &tx_size);
	if (unlikely(!data_start)) {
		GLINK_ERR("%s: invalid data_start\n", __func__);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EINVAL;
	}
	if (tx_size & (XPRT_ALIGNMENT - 1))
		tx_size = ALIGN(tx_size - SHORT_PKT_SIZE, XPRT_ALIGNMENT);
	if (likely(pctx->cookie))
		dst = pctx->cookie + (pctx->size - pctx->size_remaining);

	mutex_lock(&einfo->write_lock);
	size = glink_spi_xprt_write_avail(einfo);
	/* Need enough space to write the command */
	if (size <= sizeof(cmd)) {
		einfo->tx_resume_needed = true;
		mutex_unlock(&einfo->write_lock);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EAGAIN;
	}
	cmd.addr = 0;
	cmd.size = tx_size;
	pctx->size_remaining -= tx_size;
	cmd.size_left = pctx->size_remaining;
	if (cmd.id == TRACER_PKT_CMD)
		tracer_pkt_log_event((void *)(pctx->data), GLINK_XPRT_TX);

	if (!strcmp(einfo->xprt_cfg.edge, "wdsp"))
		wdsp_resume(&einfo->cmpnt);
	glink_spi_xprt_tx_data(einfo, data_start, dst, tx_size);
	glink_spi_xprt_tx_cmd_safe(einfo, &cmd, sizeof(cmd));
	GLINK_DBG("%s %s: lcid[%u] riid[%u] cmd[%d], size[%d], size_left[%d]\n",
		  "<SPI>", __func__, cmd.lcid, cmd.riid, cmd.id, cmd.size,
		  cmd.size_left);
	mutex_unlock(&einfo->write_lock);
	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return cmd.size;
}

/**
 * tx_short_data() - Tansmit a short packet in band along with command
 * @if_ptr:     The transport to transmit on.
 * @cmd_id:     The command ID to transmit.
 * @lcid:       The local channel id to encode.
 * @pctx:       The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx_short_data(struct glink_transport_if *if_ptr,
			 uint32_t lcid, struct glink_core_tx_pkt *pctx)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t riid;
		uint32_t size;
		uint32_t size_left;
		unsigned char data[SHORT_PKT_SIZE];
	};
	struct command cmd;
	struct edge_info *einfo;
	uint32_t size;
	void *data_start;
	size_t tx_size = 0;
	int rcu_id;

	if (pctx->size < pctx->size_remaining) {
		GLINK_ERR("%s: size remaining exceeds size.  Resetting.\n",
			  __func__);
		pctx->size_remaining = pctx->size;
	}
	if (!pctx->size_remaining)
		return 0;

	memset(&cmd, 0, sizeof(cmd));
	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = TX_SHORT_DATA_CMD;
	cmd.lcid = lcid;
	cmd.riid = pctx->riid;
	data_start = get_tx_vaddr(pctx, pctx->size - pctx->size_remaining,
				  &tx_size);
	if (unlikely(!data_start || tx_size > SHORT_PKT_SIZE)) {
		GLINK_ERR("%s: invalid data_start %p or tx_size %zu\n",
			  __func__, data_start, tx_size);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EINVAL;
	}

	mutex_lock(&einfo->write_lock);
	size = glink_spi_xprt_write_avail(einfo);
	/* Need enough space to write the command */
	if (size <= sizeof(cmd)) {
		einfo->tx_resume_needed = true;
		mutex_unlock(&einfo->write_lock);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EAGAIN;
	}
	cmd.size = tx_size;
	pctx->size_remaining -= tx_size;
	cmd.size_left = pctx->size_remaining;
	memcpy(cmd.data, data_start, tx_size);
	if (!strcmp(einfo->xprt_cfg.edge, "wdsp"))
		wdsp_resume(&einfo->cmpnt);
	glink_spi_xprt_tx_cmd_safe(einfo, &cmd, sizeof(cmd));
	GLINK_DBG("%s %s: lcid[%u] riid[%u] cmd[%d], size[%d], size_left[%d]\n",
		  "<SPI>", __func__, cmd.lcid, cmd.riid, cmd.id, cmd.size,
		  cmd.size_left);
	mutex_unlock(&einfo->write_lock);
	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return cmd.size;
}

/**
 * tx() - convert a data transmit cmd to wire format and transmit
 * @if_ptr:     The transport to transmit on.
 * @lcid:       The local channel id to encode.
 * @pctx:       The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx(struct glink_transport_if *if_ptr, uint32_t lcid,
	      struct glink_core_tx_pkt *pctx)
{
	if (pctx->size_remaining <= SHORT_PKT_SIZE)
		return tx_short_data(if_ptr, lcid, pctx);
	return tx_data(if_ptr, TX_DATA_CMD, lcid, pctx);
}

/**
 * tx_cmd_tracer_pkt() - convert a tracer packet cmd to wire format and transmit
 * @if_ptr:     The transport to transmit on.
 * @lcid:       The local channel id to encode.
 * @pctx:       The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx_cmd_tracer_pkt(struct glink_transport_if *if_ptr, uint32_t lcid,
			     struct glink_core_tx_pkt *pctx)
{
	return tx_data(if_ptr, TRACER_PKT_CMD, lcid, pctx);
}

/**
 * int wait_link_down() - Check status of read/write indices
 * @if_ptr:     The transport to check
 *
 * Return: 1 if indices are all zero, 0 otherwise
 */
static int wait_link_down(struct glink_transport_if *if_ptr)
{
	return 0;
}

/**
 * get_power_vote_ramp_time() - Get the ramp time required for the power
 *                              votes to be applied
 * @if_ptr:     The transport interface on which power voting is requested.
 * @state:      The power state for which ramp time is required.
 *
 * Return: The ramp time specific to the power state, standard error otherwise.
 */
static unsigned long get_power_vote_ramp_time(
		struct glink_transport_if *if_ptr, uint32_t state)
{
	return 0;
}

/**
 * power_vote() - Update the power votes to meet qos requirement
 * @if_ptr:     The transport interface on which power voting is requested.
 * @state:      The power state for which the voting should be done.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int power_vote(struct glink_transport_if *if_ptr, uint32_t state)
{
	unsigned long flags;
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->activity_lock, flags);
	einfo->activity_flag |= ACTIVE_TX;
	spin_unlock_irqrestore(&einfo->activity_lock, flags);
	return 0;
}

/**
 * power_unvote() - Remove the all the power votes
 * @if_ptr:     The transport interface on which power voting is requested.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int power_unvote(struct glink_transport_if *if_ptr)
{
	unsigned long flags;
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->activity_lock, flags);
	einfo->activity_flag &= ~ACTIVE_TX;
	spin_unlock_irqrestore(&einfo->activity_lock, flags);
	return 0;
}

static int glink_wdsp_cmpnt_init(struct device *dev, void *priv_data)
{
	return 0;
}

static int glink_wdsp_cmpnt_deinit(struct device *dev, void *priv_data)
{
	return 0;
}

static int glink_wdsp_cmpnt_event_handler(struct device *dev,
		void *priv_data, enum wdsp_event_type event, void *data)
{
	struct edge_info *einfo = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &einfo->cmpnt;
	struct device *sdev;
	struct spi_device *spi_dev;

	switch (event) {
	case WDSP_EVENT_PRE_BOOTUP:
		if (cmpnt && cmpnt->master_dev &&
		    cmpnt->master_ops &&
		    cmpnt->master_ops->get_dev_for_cmpnt)
			sdev = cmpnt->master_ops->get_dev_for_cmpnt(
				cmpnt->master_dev, WDSP_CMPNT_TRANSPORT);
		else
			sdev = NULL;

		if (!sdev) {
			dev_err(dev, "%s: Failed to get transport device\n",
				__func__);
			break;
		}

		spi_dev = to_spi_device(sdev);
		einfo->spi_dev = spi_dev;
		break;
	case WDSP_EVENT_POST_BOOTUP:
		einfo->in_ssr = false;
		synchronize_srcu(&einfo->use_ref);
		/* No break here to trigger fake rx_worker */
	case WDSP_EVENT_IPC1_INTR:
		queue_kthread_work(&einfo->kworker, &einfo->kwork);
		break;
	case WDSP_EVENT_PRE_SHUTDOWN:
		ssr(&einfo->xprt_if);
		break;
	default:
		pr_debug("%s: unhandled event %d", __func__, event);
		break;
	}

	return 0;
}

/* glink_wdsp_cmpnt_ops - Callback operations registered wtih WDSP framework */
static struct wdsp_cmpnt_ops glink_wdsp_cmpnt_ops = {
	.init = glink_wdsp_cmpnt_init,
	.deinit = glink_wdsp_cmpnt_deinit,
	.event_handler = glink_wdsp_cmpnt_event_handler,
};

static int glink_component_bind(struct device *dev, struct device *master,
				void *data)
{
	struct edge_info *einfo = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &einfo->cmpnt;
	int ret = 0;

	cmpnt->master_dev = master;
	cmpnt->master_ops = data;

	if (cmpnt->master_ops && cmpnt->master_ops->register_cmpnt_ops)
		ret = cmpnt->master_ops->register_cmpnt_ops(master, dev, einfo,
							&glink_wdsp_cmpnt_ops);
	else
		ret = -EINVAL;

	if (ret)
		dev_err(dev, "%s: register_cmpnt_ops failed, err = %d\n",
			__func__, ret);
	return ret;
}

static void glink_component_unbind(struct device *dev, struct device *master,
				   void *data)
{
	struct edge_info *einfo = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &einfo->cmpnt;

	cmpnt->master_dev = NULL;
	cmpnt->master_ops = NULL;
}

static const struct component_ops glink_component_ops = {
	.bind = glink_component_bind,
	.unbind = glink_component_unbind,
};

/**
 * init_xprt_if() - Initialize the xprt_if for an edge
 * @einfo:	The edge to initialize.
 */
static void init_xprt_if(struct edge_info *einfo)
{
	einfo->xprt_if.tx_cmd_version = tx_cmd_version;
	einfo->xprt_if.tx_cmd_version_ack = tx_cmd_version_ack;
	einfo->xprt_if.set_version = set_version;
	einfo->xprt_if.tx_cmd_ch_open = tx_cmd_ch_open;
	einfo->xprt_if.tx_cmd_ch_close = tx_cmd_ch_close;
	einfo->xprt_if.tx_cmd_ch_remote_open_ack = tx_cmd_ch_remote_open_ack;
	einfo->xprt_if.tx_cmd_ch_remote_close_ack = tx_cmd_ch_remote_close_ack;
	einfo->xprt_if.ssr = ssr;
	einfo->xprt_if.allocate_rx_intent = allocate_rx_intent;
	einfo->xprt_if.deallocate_rx_intent = deallocate_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_intent = tx_cmd_local_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_done = tx_cmd_local_rx_done;
	einfo->xprt_if.tx = tx;
	einfo->xprt_if.tx_cmd_rx_intent_req = tx_cmd_rx_intent_req;
	einfo->xprt_if.tx_cmd_remote_rx_intent_req_ack =
						tx_cmd_remote_rx_intent_req_ack;
	einfo->xprt_if.tx_cmd_set_sigs = tx_cmd_set_sigs;
	einfo->xprt_if.wait_link_down = wait_link_down;
	einfo->xprt_if.tx_cmd_tracer_pkt = tx_cmd_tracer_pkt;
	einfo->xprt_if.get_power_vote_ramp_time = get_power_vote_ramp_time;
	einfo->xprt_if.power_vote = power_vote;
	einfo->xprt_if.power_unvote = power_unvote;
}

/**
 * init_xprt_cfg() - Initialize the xprt_cfg for an edge
 * @einfo:	The edge to initialize.
 * @name:	The name of the remote side this edge communicates to.
 */
static void init_xprt_cfg(struct edge_info *einfo, const char *name)
{
	einfo->xprt_cfg.name = XPRT_NAME;
	einfo->xprt_cfg.edge = name;
	einfo->xprt_cfg.versions = versions;
	einfo->xprt_cfg.versions_entries = ARRAY_SIZE(versions);
	einfo->xprt_cfg.max_cid = SZ_64K;
	einfo->xprt_cfg.max_iid = SZ_2G;
}

/**
 * parse_qos_dt_params() - Parse the power states from DT
 * @dev:	Reference to the platform device for a specific edge.
 * @einfo:	Edge information for the edge probe function is called.
 *
 * Return: 0 on success, standard error code otherwise.
 */
static int parse_qos_dt_params(struct device_node *node,
				struct edge_info *einfo)
{
	int rc;
	int i;
	char *key;
	uint32_t *arr32;
	uint32_t num_states;

	key = "qcom,ramp-time";
	if (!of_find_property(node, key, &num_states))
		return -ENODEV;

	num_states /= sizeof(uint32_t);

	einfo->num_pw_states = num_states;

	arr32 = kmalloc_array(num_states, sizeof(uint32_t), GFP_KERNEL);
	if (!arr32)
		return -ENOMEM;

	einfo->ramp_time_us = kmalloc_array(num_states, sizeof(unsigned long),
					GFP_KERNEL);
	if (!einfo->ramp_time_us) {
		rc = -ENOMEM;
		goto mem_alloc_fail;
	}

	rc = of_property_read_u32_array(node, key, arr32, num_states);
	if (rc) {
		rc = -ENODEV;
		goto invalid_key;
	}
	for (i = 0; i < num_states; i++)
		einfo->ramp_time_us[i] = arr32[i];

	kfree(arr32);
	return 0;

invalid_key:
	kfree(einfo->ramp_time_us);
mem_alloc_fail:
	kfree(arr32);
	return rc;
}

/**
 * parse_qos_dt_params() - Parse any remote FIFO configuration
 * @node:	Reference to the platform device for a specific edge.
 * @einfo:	Edge information for the edge probe function is called.
 *
 * Return: 0 on success, standard error code otherwise.
 */
static int parse_remote_fifo_cfg(struct device_node *node,
				 struct edge_info *einfo)
{
	int rc;
	char *key;

	key = "qcom,out-read-idx-reg";
	rc = of_property_read_u32(node, key, &einfo->tx_fifo_read_reg_addr);
	if (rc)
		goto key_error;

	key = "qcom,out-write-idx-reg";
	rc = of_property_read_u32(node, key, &einfo->tx_fifo_write_reg_addr);
	if (rc)
		goto key_error;

	key = "qcom,in-read-idx-reg";
	rc = of_property_read_u32(node, key, &einfo->rx_fifo_read_reg_addr);
	if (rc)
		goto key_error;

	key = "qcom,in-write-idx-reg";
	rc = of_property_read_u32(node, key, &einfo->rx_fifo_write_reg_addr);
	if (rc)
		goto key_error;
	return 0;

key_error:
	pr_err("%s: Error %d parsing key %s\n", __func__, rc, key);
	return rc;
}

static int glink_spi_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *phandle_node;
	struct edge_info *einfo;
	int rc;
	char *key;
	const char *subsys_name;
	unsigned long flags;

	node = pdev->dev.of_node;

	einfo = kzalloc(sizeof(*einfo), GFP_KERNEL);
	if (!einfo) {
		rc = -ENOMEM;
		goto edge_info_alloc_fail;
	}

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}
	strlcpy(einfo->subsys_name, subsys_name, sizeof(einfo->subsys_name));

	init_xprt_cfg(einfo, subsys_name);
	init_xprt_if(einfo);

	einfo->fifo_size = DEFAULT_FIFO_SIZE;
	init_kthread_work(&einfo->kwork, rx_worker);
	init_kthread_worker(&einfo->kworker);
	init_srcu_struct(&einfo->use_ref);
	mutex_init(&einfo->write_lock);
	init_waitqueue_head(&einfo->tx_blocked_queue);
	spin_lock_init(&einfo->activity_lock);

	spin_lock_irqsave(&edge_infos_lock, flags);
	list_add_tail(&einfo->list, &edge_infos);
	spin_unlock_irqrestore(&edge_infos_lock, flags);

	einfo->task = kthread_run(kthread_worker_fn, &einfo->kworker,
				  "spi_%s", subsys_name);
	if (IS_ERR(einfo->task)) {
		rc = PTR_ERR(einfo->task);
		pr_err("%s: kthread run failed %d\n", __func__, rc);
		goto kthread_fail;
	}

	key = "qcom,remote-fifo-config";
	phandle_node = of_parse_phandle(node, key, 0);
	if (phandle_node)
		parse_remote_fifo_cfg(phandle_node, einfo);

	key = "qcom,qos-config";
	phandle_node = of_parse_phandle(node, key, 0);
	if (phandle_node && !(of_get_glink_core_qos_cfg(phandle_node,
							&einfo->xprt_cfg)))
		parse_qos_dt_params(node, einfo);

	rc = glink_core_register_transport(&einfo->xprt_if, &einfo->xprt_cfg);
	if (rc == -EPROBE_DEFER)
		goto reg_xprt_fail;
	if (rc) {
		pr_err("%s: glink core register transport failed: %d\n",
			__func__, rc);
		goto reg_xprt_fail;
	}

	dev_set_drvdata(&pdev->dev, einfo);
	if (!strcmp(einfo->xprt_cfg.edge, "wdsp")) {
		rc = component_add(&pdev->dev, &glink_component_ops);
		if (rc) {
			pr_err("%s: component_add failed, err = %d\n",
				__func__, rc);
			rc = -ENODEV;
			goto reg_cmpnt_fail;
		}
	}
	return 0;

reg_cmpnt_fail:
	dev_set_drvdata(&pdev->dev, NULL);
	glink_core_unregister_transport(&einfo->xprt_if);
reg_xprt_fail:
	flush_kthread_worker(&einfo->kworker);
	kthread_stop(einfo->task);
	einfo->task = NULL;
kthread_fail:
	spin_lock_irqsave(&edge_infos_lock, flags);
	list_del(&einfo->list);
	spin_unlock_irqrestore(&edge_infos_lock, flags);
missing_key:
	kfree(einfo);
edge_info_alloc_fail:
	return rc;
}

static int glink_spi_remove(struct platform_device *pdev)
{
	struct edge_info *einfo;
	unsigned long flags;

	einfo = (struct edge_info *)dev_get_drvdata(&pdev->dev);
	glink_core_unregister_transport(&einfo->xprt_if);
	flush_kthread_worker(&einfo->kworker);
	kthread_stop(einfo->task);
	einfo->task = NULL;
	spin_lock_irqsave(&edge_infos_lock, flags);
	list_del(&einfo->list);
	spin_unlock_irqrestore(&edge_infos_lock, flags);
	kfree(einfo);
	return 0;
}

static int glink_spi_resume(struct platform_device *pdev)
{
	return 0;
}

static int glink_spi_suspend(struct platform_device *pdev,
				   pm_message_t state)
{
	unsigned long flags;
	struct edge_info *einfo;
	bool suspend;
	int rc = -EBUSY;

	einfo = (struct edge_info *)dev_get_drvdata(&pdev->dev);
	if (strcmp(einfo->xprt_cfg.edge, "wdsp"))
		return 0;

	spin_lock_irqsave(&einfo->activity_lock, flags);
	suspend = !(einfo->activity_flag);
	spin_unlock_irqrestore(&einfo->activity_lock, flags);
	if (suspend)
		rc = wdsp_suspend(&einfo->cmpnt);
	if (rc < 0)
		pr_err("%s: Could not suspend activity_flag %d, rc %d\n",
			__func__, einfo->activity_flag, rc);
	return rc;
}

static const struct of_device_id spi_match_table[] = {
	{ .compatible = "qcom,glink-spi-xprt" },
	{},
};

static struct platform_driver glink_spi_driver = {
	.probe = glink_spi_probe,
	.remove = glink_spi_remove,
	.resume = glink_spi_resume,
	.suspend = glink_spi_suspend,
	.driver = {
		.name = "msm_glink_spi_xprt",
		.owner = THIS_MODULE,
		.of_match_table = spi_match_table,
	},
};

static int __init glink_spi_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&glink_spi_driver);
	if (rc)
		pr_err("%s: glink_spi register failed %d\n", __func__, rc);

	return rc;
}
module_init(glink_spi_xprt_init);

static void __exit glink_spi_xprt_exit(void)
{
	platform_driver_unregister(&glink_spi_driver);
}
module_exit(glink_spi_xprt_exit);

MODULE_DESCRIPTION("MSM G-Link SPI Transport");
MODULE_LICENSE("GPL v2");
