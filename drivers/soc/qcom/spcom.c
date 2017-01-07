/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

/*
 * Secure-Processor-Communication (SPCOM).
 *
 * This driver provides communication to Secure Processor (SP)
 * over G-Link transport layer.
 *
 * It provides interface to both User Space spcomlib and kernel drivers.
 *
 * User Space App shall use spcomlib for communication with SP.
 * User Space App can be either Client or Server.
 * spcomlib shall use write() file operation to send data,
 * and read() file operation to read data.
 *
 * This driver uses glink as the transport layer.
 * This driver exposes "/dev/<sp-channel-name>" file node for each glink
 * logical channel.
 * This driver exposes "/dev/spcom" file node for some debug/control command.
 * The predefined channel "/dev/sp_kernel" is used for loading SP Application
 * from HLOS.
 * This driver exposes "/dev/sp_ssr" file node to allow user space poll for SSR.
 * After the remote SP App is loaded, this driver exposes a new file node
 * "/dev/<ch-name>" for the matching HLOS App to use.
 * The access to predefined file node is restricted by using unix group
 * and SELinux.
 *
 * No message routing is use, but using the G-Link "multiplexing" feature
 * to use a dedicated logical channel for HLOS and SP Application-Pair.
 *
 * Each HLOS/SP Application can be either Client or Server or both,
 * Messaging is allays point-to-point between 2 HLOS<=>SP applications.
 *
 * User Space Request & Response are synchronous.
 * read() & write() operations are blocking until completed or terminated.
 *
 * This driver registers to G-Link callbacks to be aware on channel state.
 * A notify callback is called upon channel connect/disconnect.
 *
 */

/* Uncomment the line below to test spcom against modem rather than SP */
/* #define SPCOM_TEST_HLOS_WITH_MODEM 1 */

/* Uncomment the line below to enable debug messages */
/* #define DEBUG 1 */

#define pr_fmt(fmt)	"spcom [%s]: " fmt, __func__

#include <linux/kernel.h>	/* min() */
#include <linux/module.h>	/* MODULE_LICENSE */
#include <linux/device.h>	/* class_create() */
#include <linux/slab.h>	/* kzalloc() */
#include <linux/fs.h>		/* file_operations */
#include <linux/cdev.h>	/* cdev_add() */
#include <linux/errno.h>	/* EINVAL, ETIMEDOUT */
#include <linux/printk.h>	/* pr_err() */
#include <linux/bitops.h>	/* BIT(x) */
#include <linux/completion.h>	/* wait_for_completion_timeout() */
#include <linux/poll.h>	/* POLLOUT */
#include <linux/dma-mapping.h>	/* dma_alloc_coherent() */
#include <linux/platform_device.h>
#include <linux/of.h>		/* of_property_count_strings() */
#include <linux/workqueue.h>
#include <linux/delay.h>	/* msleep() */
#include <linux/msm_ion.h>	/* msm_ion_client_create() */

#include <soc/qcom/glink.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spcom.h>

#include <uapi/linux/spcom.h>

#include "glink_private.h" /* glink_ssr() */

/* "SPCM" string */
#define SPCOM_MAGIC_ID	((uint32_t)(0x5350434D))

/* Request/Response */
#define SPCOM_FLAG_REQ		BIT(0)
#define SPCOM_FLAG_RESP	BIT(1)
#define SPCOM_FLAG_ENCODED	BIT(2)
#define SPCOM_FLAG_NON_ENCODED	BIT(3)

/* SPCOM driver name */
#define DEVICE_NAME	"spcom"

#define SPCOM_MAX_CHANNELS	0x20

/* maximum ION buffers should be >= SPCOM_MAX_CHANNELS  */
#define SPCOM_MAX_ION_BUF_PER_CH (SPCOM_MAX_CHANNELS + 4)

/* maximum ION buffer per send request/response command */
#define SPCOM_MAX_ION_BUF_PER_CMD SPCOM_MAX_ION_BUF

/* Maximum command size */
#define SPCOM_MAX_COMMAND_SIZE	(PAGE_SIZE)

/* Maximum input size */
#define SPCOM_MAX_READ_SIZE	(PAGE_SIZE)

/* Current Process ID */
#define current_pid() ((u32)(current->pid))

/* Maximum channel name size (including null) - matching GLINK_NAME_SIZE */
#define MAX_CH_NAME_LEN	32

/* Connection negotiation timeout, if remote channel is open */
#define OPEN_CHANNEL_TIMEOUT_MSEC	100

/*
 * After both sides get CONNECTED,
 * there is a race between once side queueing rx buffer and the other side
 * trying to call glink_tx() , this race is only on the 1st tx.
 * do tx retry with some delay to allow the other side to queue rx buffer.
 */
#define TX_RETRY_DELAY_MSEC	100

/* number of tx retries */
#define TX_MAX_RETRY	3

/* SPCOM_MAX_REQUEST_SIZE-or-SPCOM_MAX_RESPONSE_SIZE + header */
#define SPCOM_RX_BUF_SIZE	300

/* The SPSS RAM size is 256 KB so SP App must fit into it */
#define SPCOM_MAX_APP_SIZE	SZ_256K

/*
 * ACK timeout from remote side for TX data.
 * Normally, it takes few msec for SPSS to responde with ACK for TX data.
 * However, due to SPSS HW issue, the SPSS might disable interrupts
 * for a very long time.
 */
#define TX_DONE_TIMEOUT_MSEC	5000

/*
 * Initial transaction id, use non-zero nonce for debug.
 * Incremented by client on request, and copied back by server on response.
 */
#define INITIAL_TXN_ID	0x12345678

/**
 * struct spcom_msg_hdr - Request/Response message header between HLOS and SP.
 *
 * This header is proceeding any request specific parameters.
 * The transaction id is used to match request with response.
 * Note: glink API provides the rx/tx data size, so user payload size is
 * calculated by reducing the header size.
 */
struct spcom_msg_hdr {
	uint32_t reserved;	/* for future use */
	uint32_t txn_id;	/* transaction id */
	char buf[0];		/* Variable buffer size, must be last field */
} __packed;

/**
 * struct spcom_client - Client handle
 */
struct spcom_client {
	struct spcom_channel *ch;
};

/**
 * struct spcom_server - Server handle
 */
struct spcom_server {
	struct spcom_channel *ch;
};

/**
 * struct spcom_channel - channel context
 */
struct spcom_channel {
	char name[MAX_CH_NAME_LEN];
	struct mutex lock;
	void *glink_handle;
	uint32_t txn_id;	/* incrementing nonce per channel */
	bool is_server;		/* for txn_id and response_timeout_msec */
	uint32_t response_timeout_msec; /* for client only */

	/* char dev */
	struct cdev *cdev;
	struct device *dev;
	struct device_attribute attr;

	/*
	 * glink state: CONNECTED / LOCAL_DISCONNECTED, REMOTE_DISCONNECTED
	 */
	unsigned glink_state;

	/* Events notification */
	struct completion connect;
	struct completion disconnect;
	struct completion tx_done;
	struct completion rx_done;

	/*
	 * Only one client or server per channel.
	 * Only one rx/tx transaction at a time (request + response).
	 */
	int ref_count;
	u32 pid;

	/* link UP/DOWN callback */
	void (*notify_link_state_cb)(bool up);

	/* abort flags */
	bool rx_abort;
	bool tx_abort;

	/* rx data info */
	int rx_buf_size;	/* allocated rx buffer size */
	bool rx_buf_ready;
	int actual_rx_size;	/* actual data size received */
	const void *glink_rx_buf;

	/* ION lock/unlock support */
	int ion_fd_table[SPCOM_MAX_ION_BUF_PER_CH];
	struct ion_handle *ion_handle_table[SPCOM_MAX_ION_BUF_PER_CH];
};

/**
 * struct spcom_device - device state structure.
 */
struct spcom_device {
	char predefined_ch_name[SPCOM_MAX_CHANNELS][MAX_CH_NAME_LEN];

	/* char device info */
	struct cdev cdev;
	dev_t device_no;
	struct class *driver_class;
	struct device *class_dev;

	/* G-Link channels */
	struct spcom_channel channels[SPCOM_MAX_CHANNELS];
	int channel_count;

	/* private */
	struct mutex lock;

	/* Link state */
	struct completion link_state_changed;
	enum glink_link_state link_state;

	/* ION support */
	struct ion_client *ion_client;
};

#ifdef SPCOM_TEST_HLOS_WITH_MODEM
	static const char *spcom_edge = "mpss";
	static const char *spcom_transport = "smem";
#else
	static const char *spcom_edge = "spss";
	static const char *spcom_transport = "mailbox";
#endif

/* Device Driver State */
static struct spcom_device *spcom_dev;

/* static functions declaration */
static int spcom_create_channel_chardev(const char *name);
static int spcom_open(struct spcom_channel *ch, unsigned int timeout_msec);
static int spcom_close(struct spcom_channel *ch);
static void spcom_notify_rx_abort(void *handle, const void *priv,
				  const void *pkt_priv);

/**
 * spcom_is_ready() - driver is initialized and ready.
 */
static inline bool spcom_is_ready(void)
{
	return spcom_dev != NULL;
}

/**
 * spcom_is_channel_open() - channel is open on this side.
 *
 * Channel might not be fully connected if remote side didn't open the channel
 * yet.
 */
static inline bool spcom_is_channel_open(struct spcom_channel *ch)
{
	return ch->glink_handle != NULL;
}

/**
 * spcom_is_channel_connected() - channel is fully connected by both sides.
 */
static inline bool spcom_is_channel_connected(struct spcom_channel *ch)
{
	return (ch->glink_state == GLINK_CONNECTED);
}

/**
 * spcom_create_predefined_channels_chardev() - expose predefined channels to
 * user space.
 *
 * Predefined channels list is provided by device tree.
 * Typically, it is for known servers on remote side that are not loaded by the
 * HLOS.
 */
static int spcom_create_predefined_channels_chardev(void)
{
	int i;
	int ret;

	for (i = 0; i < SPCOM_MAX_CHANNELS; i++) {
		const char *name = spcom_dev->predefined_ch_name[i];

		if (name[0] == 0)
			break;
		ret = spcom_create_channel_chardev(name);
		if (ret) {
			pr_err("failed to create chardev [%s], ret [%d].\n",
			       name, ret);
			return -EFAULT;
		}
	}

	return 0;
}

/*======================================================================*/
/*		GLINK CALLBACKS						*/
/*======================================================================*/

/**
 * spcom_link_state_notif_cb() - glink callback for link state change.
 *
 * glink notifies link layer is up, before any channel opened on remote side.
 * Calling glink_open() locally allowed only after link is up.
 * Notify link down, normally upon Remote Subsystem Reset (SSR).
 * Note: upon SSR, glink will also notify each channel about remote disconnect,
 * and abort any pending rx buffer.
 */
static void spcom_link_state_notif_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	spcom_dev->link_state = cb_info->link_state;

	pr_debug("spcom_link_state_notif_cb called. transport = %s edge = %s\n",
		 cb_info->transport, cb_info->edge);

	switch (cb_info->link_state) {
	case GLINK_LINK_STATE_UP:
		pr_info("GLINK_LINK_STATE_UP.\n");
		spcom_create_predefined_channels_chardev();
		break;
	case GLINK_LINK_STATE_DOWN:
		pr_err("GLINK_LINK_STATE_DOWN.\n");
		break;
	default:
		pr_err("unknown link_state [%d].\n", cb_info->link_state);
		break;
	}
	complete_all(&spcom_dev->link_state_changed);
}

/**
 * spcom_notify_rx() - glink callback on receiving data.
 *
 * Glink notify rx data is ready. The glink internal rx buffer was
 * allocated upon glink_queue_rx_intent().
 */
static void spcom_notify_rx(void *handle,
			    const void *priv, const void *pkt_priv,
			    const void *buf, size_t size)
{
	struct spcom_channel *ch = (struct spcom_channel *) priv;

	if (!ch) {
		pr_err("invalid ch parameter.\n");
		return;
	}

	pr_debug("ch [%s] rx size [%d].\n", ch->name, (int) size);

	ch->actual_rx_size = (int) size;
	ch->glink_rx_buf = (void *) buf;

	complete_all(&ch->rx_done);
}

/**
 * spcom_notify_tx_done() - glink callback on ACK sent data.
 *
 * after calling glink_tx() the remote side ACK receiving the data.
 */
static void spcom_notify_tx_done(void *handle,
				 const void *priv, const void *pkt_priv,
				 const void *buf)
{
	struct spcom_channel *ch = (struct spcom_channel *) priv;
	int *tx_buf = (int *) buf;

	if (!ch) {
		pr_err("invalid ch parameter.\n");
		return;
	}

	pr_debug("ch [%s] buf[0] = [0x%x].\n", ch->name, tx_buf[0]);

	complete_all(&ch->tx_done);
}

/**
 * spcom_notify_state() - glink callback on channel connect/disconnect.
 *
 * Channel is fully CONNECTED after both sides opened the channel.
 * Channel is LOCAL_DISCONNECTED after both sides closed the channel.
 * If the remote side closed the channel, it is expected that the local side
 * will also close the channel.
 * Upon connection, rx buffer is allocated to receive data,
 * the maximum transfer size is agreed by both sides.
 */
static void spcom_notify_state(void *handle, const void *priv, unsigned event)
{
	int ret;
	struct spcom_channel *ch = (struct spcom_channel *) priv;

	switch (event) {
	case GLINK_CONNECTED:
		pr_debug("GLINK_CONNECTED, ch name [%s].\n", ch->name);
		complete_all(&ch->connect);

		/*
		 * if spcom_notify_state() is called within glink_open()
		 * then ch->glink_handle is not updated yet.
		 */
		if (!ch->glink_handle) {
			pr_debug("update glink_handle, ch [%s].\n", ch->name);
			ch->glink_handle = handle;
		}

		/* prepare default rx buffer after connected */
		ret = glink_queue_rx_intent(ch->glink_handle,
					    ch, ch->rx_buf_size);
		if (ret) {
			pr_err("glink_queue_rx_intent() err [%d]\n", ret);
		} else {
			pr_debug("rx buf is ready, size [%d].\n",
				 ch->rx_buf_size);
			ch->rx_buf_ready = true;
		}
		break;
	case GLINK_LOCAL_DISCONNECTED:
		/*
		 * Channel state is GLINK_LOCAL_DISCONNECTED
		 * only after *both* sides closed the channel.
		 */
		pr_debug("GLINK_LOCAL_DISCONNECTED, ch [%s].\n", ch->name);
		complete_all(&ch->disconnect);
		break;
	case GLINK_REMOTE_DISCONNECTED:
		/*
		 * Remote side initiates glink_close().
		 * This is not expected on normal operation.
		 * This may happen upon remote SSR.
		 */
		pr_err("GLINK_REMOTE_DISCONNECTED, ch [%s].\n", ch->name);

		/*
		 * Abort any blocking read() operation.
		 * The glink notification might be after REMOTE_DISCONNECT.
		 */
		spcom_notify_rx_abort(NULL, ch, NULL);

		/*
		 * after glink_close(),
		 * expecting notify GLINK_LOCAL_DISCONNECTED
		 */
		spcom_close(ch);
		break;
	default:
		pr_err("unknown event id = %d, ch name [%s].\n",
		       (int) event, ch->name);
		return;
	}

	ch->glink_state = event;
}

/**
 * spcom_notify_rx_intent_req() - glink callback on intent request.
 *
 * glink allows the remote side to request for a local rx buffer if such
 * buffer is not ready.
 * However, for spcom simplicity on SP, and to reduce latency, we decided
 * that glink_tx() on both side is not using INTENT_REQ flag, so this
 * callback should not be called.
 * Anyhow, return "false" to reject the request.
 */
static bool spcom_notify_rx_intent_req(void *handle, const void *priv,
				       size_t req_size)
{
	struct spcom_channel *ch = (struct spcom_channel *) priv;

	pr_err("Unexpected intent request for ch [%s].\n", ch->name);

	return false;
}

/**
 * spcom_notify_rx_abort() - glink callback on aborting rx pending buffer.
 *
 * Rx abort may happen if channel is closed by remote side, while rx buffer is
 * pending in the queue.
 */
static void spcom_notify_rx_abort(void *handle, const void *priv,
				  const void *pkt_priv)
{
	struct spcom_channel *ch = (struct spcom_channel *) priv;

	pr_debug("ch [%s] pending rx aborted.\n", ch->name);

	if (spcom_is_channel_connected(ch) && (!ch->rx_abort)) {
		ch->rx_abort = true;
		complete_all(&ch->rx_done);
	}
}

/**
 * spcom_notify_tx_abort() - glink callback on aborting tx data.
 *
 * This is probably not relevant, since glink_txv() is not used.
 * Tx abort may happen if channel is closed by remote side,
 * while multiple tx buffers are in a middle of tx operation.
 */
static void spcom_notify_tx_abort(void *handle, const void *priv,
				  const void *pkt_priv)
{
	struct spcom_channel *ch = (struct spcom_channel *) priv;

	pr_debug("ch [%s] pending tx aborted.\n", ch->name);

	if (spcom_is_channel_connected(ch) && (!ch->tx_abort)) {
		ch->tx_abort = true;
		complete_all(&ch->tx_done);
	}
}

/*======================================================================*/
/*		UTILITIES						*/
/*======================================================================*/

/**
 * spcom_init_open_config() - Fill glink_open() configuration parameters.
 *
 * @cfg: glink configuration struct pointer
 * @name: channel name
 * @priv: private caller data, provided back by callbacks, channel state.
 *
 * specify callbacks and other parameters for glink open channel.
 */
static void spcom_init_open_config(struct glink_open_config *cfg,
				   const char *name, void *priv)
{
	cfg->notify_rx		= spcom_notify_rx;
	cfg->notify_rxv		= NULL;
	cfg->notify_tx_done	= spcom_notify_tx_done;
	cfg->notify_state	= spcom_notify_state;
	cfg->notify_rx_intent_req = spcom_notify_rx_intent_req;
	cfg->notify_rx_sigs	= NULL;
	cfg->notify_rx_abort	= spcom_notify_rx_abort;
	cfg->notify_tx_abort	= spcom_notify_tx_abort;

	cfg->options	= 0; /* not using GLINK_OPT_INITIAL_XPORT */
	cfg->priv	= priv; /* provided back by callbacks */

	cfg->name	= name;

	cfg->transport	= spcom_transport;
	cfg->edge	= spcom_edge;
}

/**
 * spcom_init_channel() - initialize channel state.
 *
 * @ch: channel state struct pointer
 * @name: channel name
 */
static int spcom_init_channel(struct spcom_channel *ch, const char *name)
{
	if (!ch || !name || !name[0]) {
		pr_err("invalid parameters.\n");
		return -EINVAL;
	}

	strlcpy(ch->name, name, sizeof(ch->name));

	init_completion(&ch->connect);
	init_completion(&ch->disconnect);
	init_completion(&ch->tx_done);
	init_completion(&ch->rx_done);

	mutex_init(&ch->lock);
	ch->glink_state = GLINK_LOCAL_DISCONNECTED;
	ch->actual_rx_size = 0;
	ch->rx_buf_size = SPCOM_RX_BUF_SIZE;

	return 0;
}

/**
 * spcom_find_channel_by_name() - find a channel by name.
 *
 * @name: channel name
 *
 * Return: a channel state struct.
 */
static struct spcom_channel *spcom_find_channel_by_name(const char *name)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(spcom_dev->channels); i++) {
		struct spcom_channel *ch = &spcom_dev->channels[i];

		if (strcmp(ch->name, name) == 0) {
			return ch;
		}
	}

	return NULL;
}

/**
 * spcom_open() - Open glink channel and wait for connection ACK.
 *
 * @ch: channel state struct pointer
 *
 * Normally, a local client opens a channel after remote server has opened
 * the channel.
 * A local server may open the channel before remote client is running.
 */
static int spcom_open(struct spcom_channel *ch, unsigned int timeout_msec)
{
	struct glink_open_config cfg = {0};
	unsigned long jiffies = msecs_to_jiffies(timeout_msec);
	long timeleft;
	const char *name;
	void *handle;

	mutex_lock(&ch->lock);
	name = ch->name;

	/* only one client/server may use the channel */
	if (ch->ref_count) {
		pr_err("channel [%s] already in use.\n", name);
		goto exit_err;
	}
	ch->ref_count++;
	ch->pid = current_pid();
	ch->txn_id = INITIAL_TXN_ID;

	pr_debug("ch [%s] opened by PID [%d], count [%d]\n",
		 name, ch->pid, ch->ref_count);

	pr_debug("Open channel [%s] timeout_msec [%d].\n", name, timeout_msec);

	if (spcom_is_channel_open(ch)) {
		pr_debug("channel [%s] already open.\n", name);
		mutex_unlock(&ch->lock);
		return 0;
	}

	spcom_init_open_config(&cfg, name, ch);

	/* init completion before calling glink_open() */
	reinit_completion(&ch->connect);

	handle = glink_open(&cfg);
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("glink_open failed.\n");
		goto exit_err;
	} else {
		pr_debug("glink_open [%s] ok.\n", name);
	}
	ch->glink_handle = handle;

	pr_debug("Wait for connection on channel [%s] timeout_msec [%d].\n",
		 name, timeout_msec);

	/* Wait for remote side to connect */
	if (timeout_msec) {
		timeleft = wait_for_completion_timeout(&(ch->connect), jiffies);
		if (timeleft == 0)
			pr_debug("Channel [%s] is NOT connected.\n", name);
		else
			pr_debug("Channel [%s] fully connect.\n", name);
	} else {
		pr_debug("wait for connection ch [%s] no timeout.\n", name);
		wait_for_completion(&(ch->connect));
		pr_debug("Channel [%s] opened, no timeout.\n", name);
	}

	mutex_unlock(&ch->lock);

	return 0;
exit_err:
	mutex_unlock(&ch->lock);

	return -EFAULT;
}

/**
 * spcom_close() - Close glink channel.
 *
 * @ch: channel state struct pointer
 *
 * A calling API functions should wait for disconnecting by both sides.
 */
static int spcom_close(struct spcom_channel *ch)
{
	int ret = 0;

	mutex_lock(&ch->lock);

	if (!spcom_is_channel_open(ch)) {
		pr_err("ch already closed.\n");
		mutex_unlock(&ch->lock);
		return 0;
	}

	ret = glink_close(ch->glink_handle);
	if (ret)
		pr_err("glink_close() fail, ret [%d].\n", ret);
	else
		pr_debug("glink_close() ok.\n");

	ch->glink_handle = NULL;
	ch->ref_count = 0;
	ch->rx_abort = false;
	ch->tx_abort = false;
	ch->glink_state = GLINK_LOCAL_DISCONNECTED;
	ch->txn_id = INITIAL_TXN_ID; /* use non-zero nonce for debug */
	ch->pid = 0;

	pr_debug("Channel closed [%s].\n", ch->name);
	mutex_unlock(&ch->lock);

	return 0;
}

/**
 * spcom_tx() - Send data and wait for ACK or timeout.
 *
 * @ch: channel state struct pointer
 * @buf: buffer pointer
 * @size: buffer size
 *
 * ACK is expected within a very short time (few msec).
 */
static int spcom_tx(struct spcom_channel *ch,
		    void *buf,
		    uint32_t size,
		    uint32_t timeout_msec)
{
	int ret;
	void *pkt_priv = NULL;
	uint32_t tx_flags = 0 ; /* don't use GLINK_TX_REQ_INTENT */
	unsigned long jiffies = msecs_to_jiffies(timeout_msec);
	long timeleft;
	int retry = 0;

	mutex_lock(&ch->lock);

	/* reset completion before calling glink */
	reinit_completion(&ch->tx_done);

	for (retry = 0; retry < TX_MAX_RETRY ; retry++) {
		ret = glink_tx(ch->glink_handle, pkt_priv, buf, size, tx_flags);
		if (ret == -EAGAIN) {
			pr_err("glink_tx() fail, try again.\n");
			/*
			 * Delay to allow remote side to queue rx buffer.
			 * This may happen after the first channel connection.
			 */
			msleep(TX_RETRY_DELAY_MSEC);
		} else if (ret < 0) {
			pr_err("glink_tx() error %d.\n", ret);
			goto exit_err;
		} else {
			break; /* no retry needed */
		}
	}

	pr_debug("Wait for Tx done.\n");

	/* Wait for Tx Completion */
	timeleft = wait_for_completion_timeout(&ch->tx_done, jiffies);
	if (timeleft == 0) {
		pr_err("tx_done timeout %d msec expired.\n", timeout_msec);
		goto exit_err;
	} else if (ch->tx_abort) {
		pr_err("tx aborted.\n");
		goto exit_err;
	}

	mutex_unlock(&ch->lock);

	return ret;
exit_err:
	mutex_unlock(&ch->lock);
	return -EFAULT;
}

/**
 * spcom_rx() - Wait for received data until timeout, unless pending rx data is
 * already ready
 *
 * @ch: channel state struct pointer
 * @buf: buffer pointer
 * @size: buffer size
 *
 * ACK is expected within a very short time (few msec).
 */
static int spcom_rx(struct spcom_channel *ch,
		     void *buf,
		     uint32_t size,
		     uint32_t timeout_msec)
{
	int ret;
	unsigned long jiffies = msecs_to_jiffies(timeout_msec);
	long timeleft = 1;

	mutex_lock(&ch->lock);

	/* check for already pending data */
	if (ch->actual_rx_size) {
		pr_debug("already pending data size [%d].\n",
			 ch->actual_rx_size);
		goto copy_buf;
	}

	/* reset completion before calling glink */
	reinit_completion(&ch->rx_done);

	/* Wait for Rx response */
	pr_debug("Wait for Rx done.\n");
	if (timeout_msec)
		timeleft = wait_for_completion_timeout(&ch->rx_done, jiffies);
	else
		wait_for_completion(&ch->rx_done);

	if (timeleft == 0) {
		pr_err("rx_done timeout [%d] msec expired.\n", timeout_msec);
		goto exit_err;
	} else if (ch->rx_abort) {
		pr_err("rx aborted.\n");
		goto exit_err;
	} else if (ch->actual_rx_size) {
		pr_debug("actual_rx_size is [%d].\n", ch->actual_rx_size);
	} else {
		pr_err("actual_rx_size is zero.\n");
		goto exit_err;
	}

	if (!ch->glink_rx_buf) {
		pr_err("invalid glink_rx_buf.\n");
		goto exit_err;
	}

copy_buf:
	/* Copy from glink buffer to spcom buffer */
	size = min_t(int, ch->actual_rx_size, size);
	memcpy(buf, ch->glink_rx_buf, size);

	pr_debug("copy size [%d].\n", (int) size);

	/* free glink buffer after copy to spcom buffer */
	glink_rx_done(ch->glink_handle, ch->glink_rx_buf, false);
	ch->glink_rx_buf = NULL;
	ch->actual_rx_size = 0;

	/* queue rx buffer for the next time */
	ret = glink_queue_rx_intent(ch->glink_handle, ch, ch->rx_buf_size);
	if (ret) {
		pr_err("glink_queue_rx_intent() failed, ret [%d]", ret);
		goto exit_err;
	} else {
		pr_debug("queue rx_buf, size [%d].\n", ch->rx_buf_size);
	}

	mutex_unlock(&ch->lock);

	return size;
exit_err:
	mutex_unlock(&ch->lock);
	return -EFAULT;
}

/**
 * spcom_get_next_request_size() - get request size.
 * already ready
 *
 * @ch: channel state struct pointer
 *
 * Server needs the size of the next request to allocate a request buffer.
 * Initially used intent-request, however this complicated the remote side,
 * so both sides are not using glink_tx() with INTENT_REQ anymore.
 */
static int spcom_get_next_request_size(struct spcom_channel *ch)
{
	int size = -1;

	/* NOTE: Remote clients might not be connected yet.*/
	mutex_lock(&ch->lock);
	reinit_completion(&ch->rx_done);

	/* check if already got it via callback */
	if (ch->actual_rx_size) {
		pr_debug("next-req-size already ready ch [%s] size [%d].\n",
			 ch->name, ch->actual_rx_size);
		goto exit_ready;
	}

	pr_debug("Wait for Rx Done, ch [%s].\n", ch->name);
	wait_for_completion(&ch->rx_done);
	if (ch->actual_rx_size <= 0) {
		pr_err("invalid rx size [%d] ch [%s].\n",
		       ch->actual_rx_size, ch->name);
		goto exit_error;
	}

exit_ready:
	size = ch->actual_rx_size;
	if (size > sizeof(struct spcom_msg_hdr)) {
		size -= sizeof(struct spcom_msg_hdr);
	} else {
		pr_err("rx size [%d] too small.\n", size);
		goto exit_error;
	}

	mutex_unlock(&ch->lock);
	return size;

exit_error:
	mutex_unlock(&ch->lock);
	return -EFAULT;


}

/*======================================================================*/
/*		General API for kernel drivers				*/
/*======================================================================*/

/**
 * spcom_is_sp_subsystem_link_up() - check if SPSS link is up.
 *
 * return: true if link is up, false if link is down.
 */
bool spcom_is_sp_subsystem_link_up(void)
{
	 return (spcom_dev->link_state == GLINK_LINK_STATE_UP);
}
EXPORT_SYMBOL(spcom_is_sp_subsystem_link_up);

/*======================================================================*/
/*		Client API for kernel drivers				*/
/*======================================================================*/

/**
 * spcom_register_client() - register a client.
 *
 * @info: channel name and ssr callback.
 *
 * Return: client handle
 */
struct spcom_client *spcom_register_client(struct spcom_client_info *info)
{
	int ret;
	const char *name;
	struct spcom_channel *ch;
	struct spcom_client *client;

	if (!info) {
		pr_err("Invalid parameter.\n");
		return NULL;
	}
	name = info->ch_name;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		pr_err("channel %s doesn't exist, load App first.\n", name);
		return NULL;
	}

	client->ch = ch; /* backtrack */

	ret = spcom_open(ch, OPEN_CHANNEL_TIMEOUT_MSEC);
	if (ret) {
		pr_err("failed to open channel [%s].\n", name);
		kfree(client);
		client = NULL;
	} else {
		pr_info("remote side connect to channel [%s].\n", name);
	}

	return client;
}
EXPORT_SYMBOL(spcom_register_client);


/**
 * spcom_unregister_client() - unregister a client.
 *
 * @client: client handle
 */
int spcom_unregister_client(struct spcom_client *client)
{
	struct spcom_channel *ch;

	if (!client) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = client->ch;

	kfree(client);

	spcom_close(ch);

	return 0;
}
EXPORT_SYMBOL(spcom_unregister_client);


/**
 * spcom_client_send_message_sync() - send request and wait for response.
 *
 * @client: client handle
 * @req_ptr: request pointer
 * @req_size: request size
 * @resp_ptr: response pointer
 * @resp_size: response size
 * @timeout_msec: timeout waiting for response.
 *
 * The timeout depends on the specific request handling time at the remote side.
 */
int spcom_client_send_message_sync(struct spcom_client	*client,
				    void	*req_ptr,
				    uint32_t	req_size,
				    void	*resp_ptr,
				    uint32_t	resp_size,
				    uint32_t	timeout_msec)
{
	int ret;
	struct spcom_channel *ch;

	if (!client || !req_ptr || !resp_ptr) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = client->ch;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	ret = spcom_tx(ch, req_ptr, req_size, TX_DONE_TIMEOUT_MSEC);
	if (ret < 0) {
		pr_err("tx error %d.\n", ret);
		return ret;
	}

	ret = spcom_rx(ch, resp_ptr, resp_size, timeout_msec);
	if (ret < 0) {
		pr_err("rx error %d.\n", ret);
		return ret;
	}

	/* @todo verify response transaction id match the request */

	return ret;
}
EXPORT_SYMBOL(spcom_client_send_message_sync);


/**
 * spcom_client_is_server_connected() - is remote server connected.
 *
 * @client: client handle
 */
bool spcom_client_is_server_connected(struct spcom_client *client)
{
	bool connected;

	if (!client) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	connected = spcom_is_channel_connected(client->ch);

	return connected;
}
EXPORT_SYMBOL(spcom_client_is_server_connected);

/*======================================================================*/
/*		Server API for kernel drivers				*/
/*======================================================================*/

/**
 * spcom_register_service() - register a server.
 *
 * @info: channel name and ssr callback.
 *
 * Return: server handle
 */
struct spcom_server *spcom_register_service(struct spcom_service_info *info)
{
	int ret;
	const char *name;
	struct spcom_channel *ch;
	struct spcom_server *server;

	if (!info) {
		pr_err("Invalid parameter.\n");
		return NULL;
	}
	name = info->ch_name;

	server = kzalloc(sizeof(*server), GFP_KERNEL);
	if (!server)
		return NULL;

	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		pr_err("channel %s doesn't exist, load App first.\n", name);
		return NULL;
	}

	server->ch = ch; /* backtrack */

	ret = spcom_open(ch, 0);
	if (ret) {
		pr_err("failed to open channel [%s].\n", name);
		kfree(server);
		server = NULL;
	}

	return server;
}
EXPORT_SYMBOL(spcom_register_service);

/**
 * spcom_unregister_service() - unregister a server.
 *
 * @server: server handle
 */
int spcom_unregister_service(struct spcom_server *server)
{
	struct spcom_channel *ch;

	if (!server) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = server->ch;

	kfree(server);

	spcom_close(ch);

	return 0;
}
EXPORT_SYMBOL(spcom_unregister_service);

/**
 * spcom_server_get_next_request_size() - get request size.
 *
 * @server: server handle
 *
 * Return: request size in bytes.
 */
int spcom_server_get_next_request_size(struct spcom_server *server)
{
	int size;
	struct spcom_channel *ch;

	if (!server) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = server->ch;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	size = spcom_get_next_request_size(ch);

	pr_debug("next_request_size [%d].\n", size);

	return size;
}
EXPORT_SYMBOL(spcom_server_get_next_request_size);

/**
 * spcom_server_wait_for_request() - wait for request.
 *
 * @server: server handle
 * @req_ptr: request buffer pointer
 * @req_size: max request size
 *
 * Return: request size in bytes.
 */
int spcom_server_wait_for_request(struct spcom_server	*server,
				  void			*req_ptr,
				  uint32_t		req_size)
{
	int ret;
	struct spcom_channel *ch;

	if (!server || !req_ptr) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = server->ch;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	ret = spcom_rx(ch, req_ptr, req_size, 0);

	return ret;
}
EXPORT_SYMBOL(spcom_server_wait_for_request);

/**
 * spcom_server_send_response() - Send response
 *
 * @server: server handle
 * @resp_ptr: response buffer pointer
 * @resp_size: response size
 */
int spcom_server_send_response(struct spcom_server	*server,
			       void			*resp_ptr,
			       uint32_t			resp_size)
{
	int ret;
	struct spcom_channel *ch;

	if (!server || !resp_ptr) {
		pr_err("Invalid parameter.\n");
		return -EINVAL;
	}

	ch = server->ch;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	ret = spcom_tx(ch, resp_ptr, resp_size, TX_DONE_TIMEOUT_MSEC);

	return ret;
}
EXPORT_SYMBOL(spcom_server_send_response);

/*======================================================================*/
/*	USER SPACE commands handling					*/
/*======================================================================*/

/**
 * spcom_handle_create_channel_command() - Handle Create Channel command from
 * user space.
 *
 * @cmd_buf:	command buffer.
 * @cmd_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_create_channel_command(void *cmd_buf, int cmd_size)
{
	int ret = 0;
	struct spcom_user_create_channel_command *cmd = cmd_buf;
	const char *ch_name;

	if (cmd_size != sizeof(*cmd)) {
		pr_err("cmd_size [%d] , expected [%d].\n",
		       (int) cmd_size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	ch_name = cmd->ch_name;

	pr_debug("ch_name [%s].\n", ch_name);

	ret = spcom_create_channel_chardev(ch_name);

	return ret;
}

/**
 * spcom_handle_send_command() - Handle send request/response from user space.
 *
 * @buf:	command buffer.
 * @buf_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_send_command(struct spcom_channel *ch,
					     void *cmd_buf, int size)
{
	int ret = 0;
	struct spcom_send_command *cmd = cmd_buf;
	uint32_t buf_size;
	void *buf;
	struct spcom_msg_hdr *hdr;
	void *tx_buf;
	int tx_buf_size;
	uint32_t timeout_msec;

	pr_debug("send req/resp ch [%s] size [%d] .\n", ch->name, size);

	/*
	 * check that cmd buf size is at least struct size,
	 * to allow access to struct fields.
	 */
	if (size < sizeof(*cmd)) {
		pr_err("ch [%s] invalid cmd buf.\n",
			ch->name);
		return -EINVAL;
	}

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	/* parse command buffer */
	buf = &cmd->buf;
	buf_size = cmd->buf_size;
	timeout_msec = cmd->timeout_msec;

	/* Check param validity */
	if (buf_size > SPCOM_MAX_RESPONSE_SIZE) {
		pr_err("ch [%s] invalid buf size [%d].\n",
			ch->name, buf_size);
		return -EINVAL;
	}
	if (size != sizeof(*cmd) + buf_size) {
		pr_err("ch [%s] invalid cmd size [%d].\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	tx_buf_size = sizeof(*hdr) + buf_size;
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;

	/* Prepare Tx Buf */
	hdr = tx_buf;

	/* Header */
	hdr->txn_id = ch->txn_id;
	if (!ch->is_server) {
		ch->txn_id++;   /* client sets the request txn_id */
		ch->response_timeout_msec = timeout_msec;
	}

	/* user buf */
	memcpy(hdr->buf, buf, buf_size);

	/*
	 * remote side should have rx buffer ready.
	 * tx_done is expected to be received quickly.
	 */
	ret = spcom_tx(ch, tx_buf, tx_buf_size, TX_DONE_TIMEOUT_MSEC);
	if (ret < 0)
		pr_err("tx error %d.\n", ret);

	kfree(tx_buf);

	return ret;
}

/**
 * modify_ion_addr() - replace the ION buffer virtual address with physical
 * address in a request or response buffer.
 *
 * @buf: buffer to modify
 * @buf_size: buffer size
 * @ion_info: ION buffer info such as FD and offset in buffer.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int modify_ion_addr(void *buf,
			    uint32_t buf_size,
			    struct spcom_ion_info ion_info)
{
	struct ion_handle *handle = NULL;
	ion_phys_addr_t ion_phys_addr;
	size_t len;
	int fd;
	uint32_t buf_offset;
	char *ptr = (char *)buf;
	int ret;

	fd = ion_info.fd;
	buf_offset = ion_info.buf_offset;
	ptr += buf_offset;

	if (fd < 0) {
		pr_err("invalid fd [%d].\n", fd);
		return -ENODEV;
	}

	if (buf_size < sizeof(uint64_t)) {
		pr_err("buf size too small [%d].\n", buf_size);
		return -ENODEV;
	}

	if (buf_offset > buf_size - sizeof(uint64_t)) {
		pr_err("invalid buf_offset [%d].\n", buf_offset);
		return -ENODEV;
	}

	/* Get ION handle from fd */
	handle = ion_import_dma_buf(spcom_dev->ion_client, fd);
	if (handle == NULL) {
		pr_err("fail to get ion handle.\n");
		return -EINVAL;
	}
	pr_debug("ion handle ok.\n");

	/* Get the ION buffer Physical Address */
	ret = ion_phys(spcom_dev->ion_client, handle, &ion_phys_addr, &len);
	if (ret < 0) {
		pr_err("fail to get ion phys addr.\n");
		ion_free(spcom_dev->ion_client, handle);
		return -EINVAL;
	}
	if (buf_offset % sizeof(uint64_t))
		pr_debug("offset [%d] is NOT 64-bit aligned.\n", buf_offset);
	else
		pr_debug("offset [%d] is 64-bit aligned.\n", buf_offset);

	/* Set the ION Physical Address at the buffer offset */
	pr_debug("ion phys addr = [0x%lx].\n", (long int) ion_phys_addr);
	memcpy(ptr, &ion_phys_addr, sizeof(uint64_t));

	/* Release the ION handle */
	ion_free(spcom_dev->ion_client, handle);

	return 0;
}

/**
 * spcom_handle_send_modified_command() - send a request/response with ION
 * buffer address. Modify the request/response by replacing the ION buffer
 * virtual address with the physical address.
 *
 * @ch: channel pointer
 * @cmd_buf: User space command buffer
 * @size: size of user command buffer
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_send_modified_command(struct spcom_channel *ch,
					       void *cmd_buf, int size)
{
	int ret = 0;
	struct spcom_user_send_modified_command *cmd = cmd_buf;
	uint32_t buf_size;
	void *buf;
	struct spcom_msg_hdr *hdr;
	void *tx_buf;
	int tx_buf_size;
	uint32_t timeout_msec;
	struct spcom_ion_info ion_info[SPCOM_MAX_ION_BUF_PER_CMD];
	int i;

	pr_debug("send req/resp ch [%s] size [%d] .\n", ch->name, size);

	/*
	 * check that cmd buf size is at least struct size,
	 * to allow access to struct fields.
	 */
	if (size < sizeof(*cmd)) {
		pr_err("ch [%s] invalid cmd buf.\n",
			ch->name);
		return -EINVAL;
	}

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	/* parse command buffer */
	buf = &cmd->buf;
	buf_size = cmd->buf_size;
	timeout_msec = cmd->timeout_msec;
	memcpy(ion_info, cmd->ion_info, sizeof(ion_info));

	/* Check param validity */
	if (buf_size > SPCOM_MAX_RESPONSE_SIZE) {
		pr_err("ch [%s] invalid buf size [%d].\n",
			ch->name, buf_size);
		return -EINVAL;
	}
	if (size != sizeof(*cmd) + buf_size) {
		pr_err("ch [%s] invalid cmd size [%d].\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	tx_buf_size = sizeof(*hdr) + buf_size;
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;

	/* Prepare Tx Buf */
	hdr = tx_buf;

	/* Header */
	hdr->txn_id = ch->txn_id;
	if (!ch->is_server) {
		ch->txn_id++;   /* client sets the request txn_id */
		ch->response_timeout_msec = timeout_msec;
	}

	/* user buf */
	memcpy(hdr->buf, buf, buf_size);

	for (i = 0 ; i < ARRAY_SIZE(ion_info) ; i++) {
		if (ion_info[i].fd >= 0) {
			ret = modify_ion_addr(hdr->buf, buf_size, ion_info[i]);
			if (ret < 0) {
				pr_err("modify_ion_addr() error [%d].\n", ret);
				kfree(tx_buf);
				return -EFAULT;
			}
		}
	}

	/*
	 * remote side should have rx buffer ready.
	 * tx_done is expected to be received quickly.
	 */
	ret = spcom_tx(ch, tx_buf, tx_buf_size, TX_DONE_TIMEOUT_MSEC);
	if (ret < 0)
		pr_err("tx error %d.\n", ret);

	kfree(tx_buf);

	return ret;
}


/**
 * spcom_handle_lock_ion_buf_command() - Lock an ION buffer.
 *
 * Lock an ION buffer, prevent it from being free if the user space App crash,
 * while it is used by the remote subsystem.
 */
static int spcom_handle_lock_ion_buf_command(struct spcom_channel *ch,
					      void *cmd_buf, int size)
{
	struct spcom_user_command *cmd = cmd_buf;
	int fd = cmd->arg;
	struct ion_handle *ion_handle;
	int i;

	if (size != sizeof(*cmd)) {
		pr_err("cmd size [%d] , expected [%d].\n",
		       (int) size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	/* Check ION client */
	if (spcom_dev->ion_client == NULL) {
		pr_err("invalid ion client.\n");
		return -ENODEV;
	}

	/* Get ION handle from fd - this increments the ref count */
	ion_handle = ion_import_dma_buf(spcom_dev->ion_client, fd);
	if (ion_handle == NULL) {
		pr_err("fail to get ion handle.\n");
		return -EINVAL;
	}
	pr_debug("ion handle ok.\n");

	/* Check if this ION buffer is already locked */
	for (i = 0 ; i < ARRAY_SIZE(ch->ion_handle_table) ; i++) {
		if (ch->ion_handle_table[i] == ion_handle) {
			pr_debug("fd [%d] ion buf is already locked.\n", fd);
			/* decrement back the ref count */
			ion_free(spcom_dev->ion_client, ion_handle);
			return -EINVAL;
		}
	}

       /* Store the ION handle */
	for (i = 0 ; i < ARRAY_SIZE(ch->ion_handle_table) ; i++) {
		if (ch->ion_handle_table[i] == NULL) {
			ch->ion_handle_table[i] = ion_handle;
			ch->ion_fd_table[i] = fd;
			pr_debug("locked ion buf#[%d], fd [%d].\n", i, fd);
			return 0;
		}
	}

	return -EFAULT;
}

/**
 * spcom_handle_unlock_ion_buf_command() - Unlock an ION buffer.
 *
 * Unlock an ION buffer, let it be free, when it is no longer being used by
 * the remote subsystem.
 */
static int spcom_handle_unlock_ion_buf_command(struct spcom_channel *ch,
					      void *cmd_buf, int size)
{
	struct spcom_user_command *cmd = cmd_buf;
	int fd = cmd->arg;
	struct ion_client *ion_client = spcom_dev->ion_client;
	int i;

	if (size != sizeof(*cmd)) {
		pr_err("cmd size [%d] , expected [%d].\n",
		       (int) size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	/* Check ION client */
	if (ion_client == NULL) {
		pr_err("fail to create ion client.\n");
		return -ENODEV;
	}

	if (fd == (int) SPCOM_ION_FD_UNLOCK_ALL) {
		/* unlock all ION buf */
		for (i = 0 ; i < ARRAY_SIZE(ch->ion_handle_table) ; i++) {
			if (ch->ion_handle_table[i] != NULL) {
				ion_free(ion_client, ch->ion_handle_table[i]);
				ch->ion_handle_table[i] = NULL;
				ch->ion_fd_table[i] = -1;
				pr_debug("unlocked ion buf#[%d].\n", i);
			}
		}
	} else {
		/* unlock specific ION buf */
		for (i = 0 ; i < ARRAY_SIZE(ch->ion_handle_table) ; i++) {
			if (ch->ion_fd_table[i] == fd) {
				ion_free(ion_client, ch->ion_handle_table[i]);
				ch->ion_handle_table[i] = NULL;
				ch->ion_fd_table[i] = -1;
				pr_debug("unlocked ion buf#[%d].\n", i);
				break;
			}
		}
	}

	return 0;
}

/**
 * spcom_handle_fake_ssr_command() - Handle fake ssr command from user space.
 */
static int spcom_handle_fake_ssr_command(struct spcom_channel *ch, int arg)
{
	pr_debug("Start Fake glink SSR subsystem [%s].\n", spcom_edge);
	glink_ssr(spcom_edge);
	pr_debug("Fake glink SSR subsystem [%s] done.\n", spcom_edge);

	return 0;
}

/**
 * spcom_handle_write() - Handle user space write commands.
 *
 * @buf:	command buffer.
 * @buf_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_write(struct spcom_channel *ch,
			       void *buf,
			       int buf_size)
{
	int ret = 0;
	struct spcom_user_command *cmd = NULL;
	int cmd_id = 0;
	int swap_id;
	char cmd_name[5] = {0}; /* debug only */

	/* opcode field is the minimum length of cmd */
	if (buf_size < sizeof(cmd->cmd_id)) {
		pr_err("Invalid argument user buffer size %d.\n", buf_size);
		return -EINVAL;
	}

	cmd = (struct spcom_user_command *)buf;
	cmd_id = (int) cmd->cmd_id;
	swap_id = htonl(cmd->cmd_id);
	memcpy(cmd_name, &swap_id, sizeof(int));

	pr_debug("cmd_id [0x%x] cmd_name [%s].\n", cmd_id, cmd_name);

	switch (cmd_id) {
	case SPCOM_CMD_SEND:
		ret = spcom_handle_send_command(ch, buf, buf_size);
		break;
	case SPCOM_CMD_SEND_MODIFIED:
	       ret = spcom_handle_send_modified_command(ch, buf, buf_size);
	       break;
	case SPCOM_CMD_LOCK_ION_BUF:
	      ret = spcom_handle_lock_ion_buf_command(ch, buf, buf_size);
	      break;
	case SPCOM_CMD_UNLOCK_ION_BUF:
		ret = spcom_handle_unlock_ion_buf_command(ch, buf, buf_size);
	     break;
	case SPCOM_CMD_FSSR:
		ret = spcom_handle_fake_ssr_command(ch, cmd->arg);
		break;
	case SPCOM_CMD_CREATE_CHANNEL:
		ret = spcom_handle_create_channel_command(buf, buf_size);
		break;
	default:
		pr_err("Invalid Command Id [0x%x].\n", (int) cmd->cmd_id);
		return -EINVAL;
	}

	return ret;
}

/**
 * spcom_handle_get_req_size() - Handle user space get request size command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * Return: size in bytes.
 */
static int spcom_handle_get_req_size(struct spcom_channel *ch,
				      void *buf,
				      uint32_t size)
{
	uint32_t next_req_size = 0;

	if (size < sizeof(next_req_size)) {
		pr_err("buf size [%d] too small.\n", (int) size);
		return -EINVAL;
	}

	next_req_size = spcom_get_next_request_size(ch);

	memcpy(buf, &next_req_size, sizeof(next_req_size));
	pr_debug("next_req_size [%d].\n", next_req_size);

	return sizeof(next_req_size); /* can't exceed user buffer size */
}

/**
 * spcom_handle_read_req_resp() - Handle user space get request/response command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * Return: size in bytes.
 */
static int spcom_handle_read_req_resp(struct spcom_channel *ch,
				       void *buf,
				       uint32_t size)
{
	int ret;
	struct spcom_msg_hdr *hdr;
	void *rx_buf;
	int rx_buf_size;
	uint32_t timeout_msec = 0; /* client only */

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		pr_err("ch [%s] remote side not connect.\n", ch->name);
		return -ENOTCONN;
	}

	/* Check param validity */
	if (size > SPCOM_MAX_RESPONSE_SIZE) {
		pr_err("ch [%s] inavlid size [%d].\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	rx_buf_size = sizeof(*hdr) + size;
	rx_buf = kzalloc(rx_buf_size, GFP_KERNEL);
	if (!rx_buf)
		return -ENOMEM;

	/*
	 * client response timeout depends on the request
	 * handling time on the remote side .
	 */
	if (!ch->is_server) {
		timeout_msec = ch->response_timeout_msec;
		pr_debug("response_timeout_msec = %d.\n", (int) timeout_msec);
	}

	ret = spcom_rx(ch, rx_buf, rx_buf_size, timeout_msec);
	if (ret < 0) {
		pr_err("rx error %d.\n", ret);
		goto exit_err;
	} else {
		size = ret; /* actual_rx_size */
	}

	hdr = rx_buf;

	if (ch->is_server) {
		ch->txn_id = hdr->txn_id;
		pr_debug("request txn_id [0x%x].\n", ch->txn_id);
	}

	/* copy data to user without the header */
	if (size > sizeof(*hdr)) {
		size -= sizeof(*hdr);
		memcpy(buf, hdr->buf, size);
	} else {
		pr_err("rx size [%d] too small.\n", size);
		goto exit_err;
	}

	kfree(rx_buf);
	return size;
exit_err:
	kfree(rx_buf);
	return -EFAULT;

}

/**
 * spcom_handle_read() - Handle user space read request/response or
 * request-size command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * A special size SPCOM_GET_NEXT_REQUEST_SIZE, which is bigger than the max
 * response/request tells the kernel that user space only need the size.
 *
 * Return: size in bytes.
 */
static int spcom_handle_read(struct spcom_channel *ch,
			      void *buf,
			      uint32_t size)
{
	if (size == SPCOM_GET_NEXT_REQUEST_SIZE) {
		pr_debug("get next request size, ch [%s].\n", ch->name);
		size = spcom_handle_get_req_size(ch, buf, size);
		ch->is_server = true;
	} else {
		pr_debug("get request/response, ch [%s].\n", ch->name);
		size = spcom_handle_read_req_resp(ch, buf, size);
	}

	pr_debug("ch [%s] , size = %d.\n", ch->name, size);

	return size;
}

/*======================================================================*/
/*		CHAR DEVICE USER SPACE INTERFACE			*/
/*======================================================================*/

/**
 * file_to_filename() - get the filename from file pointer.
 *
 * @filp: file pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
static char *file_to_filename(struct file *filp)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (!filp || !filp->f_path.dentry)
		return "unknown";

	dentry = filp->f_path.dentry;
	filename = dentry->d_iname;

	return filename;
}

/**
 * spcom_device_open() - handle channel file open() from user space.
 *
 * @filp: file pointer
 *
 * The file name (without path) is the channel name.
 * Open the relevant glink channel.
 * Store the channel context in the file private
 * date pointer for future read/write/close
 * operations.
 */
static int spcom_device_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);

	pr_debug("Open file [%s].\n", name);

	if (strcmp(name, DEVICE_NAME) == 0) {
		pr_debug("root dir skipped.\n");
		return 0;
	}

	if (strcmp(name, "sp_ssr") == 0) {
		pr_debug("sp_ssr dev node skipped.\n");
		return 0;
	}

	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		pr_err("channel %s doesn't exist, load App first.\n", name);
		return -ENODEV;
	}

	filp->private_data = ch;

	ret = spcom_open(ch, OPEN_CHANNEL_TIMEOUT_MSEC);
	if (ret == -ETIMEDOUT) {
		pr_err("Connection timeout channel [%s].\n", name);
	} else if (ret) {
		pr_err("failed to open channel [%s] , err=%d.\n", name, ret);
		return ret;
	}

	pr_debug("finished.\n");

	return 0;
}

/**
 * spcom_device_release() - handle channel file close() from user space.
 *
 * @filp: file pointer
 *
 * The file name (without path) is the channel name.
 * Open the relevant glink channel.
 * Store the channel context in the file private
 * date pointer for future read/write/close
 * operations.
 */
static int spcom_device_release(struct inode *inode, struct file *filp)
{
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);
	bool connected = false;

	pr_debug("Close file [%s].\n", name);

	if (strcmp(name, DEVICE_NAME) == 0) {
		pr_debug("root dir skipped.\n");
		return 0;
	}

	if (strcmp(name, "sp_ssr") == 0) {
		pr_debug("sp_ssr dev node skipped.\n");
		return 0;
	}

	ch = filp->private_data;

	if (!ch) {
		pr_debug("ch is NULL, file name %s.\n", file_to_filename(filp));
		return -ENODEV;
	}

	/* channel might be already closed or disconnected */
	if (spcom_is_channel_open(ch) && spcom_is_channel_connected(ch))
		connected = true;

	reinit_completion(&ch->disconnect);

	spcom_close(ch);

	if (connected) {
		pr_debug("Wait for event GLINK_LOCAL_DISCONNECTED, ch [%s].\n",
			 name);
		wait_for_completion(&ch->disconnect);
		pr_debug("GLINK_LOCAL_DISCONNECTED signaled, ch [%s].\n", name);
	}

	return 0;
}

/**
 * spcom_device_write() - handle channel file write() from user space.
 *
 * @filp: file pointer
 *
 * Return: On Success - same size as number of bytes to write.
 * On Failure - negative value.
 */
static ssize_t spcom_device_write(struct file *filp,
				   const char __user *user_buff,
				   size_t size, loff_t *f_pos)
{
	int ret;
	char *buf;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);

	pr_debug("Write file [%s] size [%d] pos [%d].\n",
		 name, (int) size, (int) *f_pos);

	if (!user_buff || !f_pos || !filp) {
		pr_err("invalid null parameters.\n");
		return -EINVAL;
	}

	ch = filp->private_data;
	if (!ch) {
		pr_debug("invalid ch pointer.\n");
		/* Allow some special commands via /dev/spcom and /dev/sp_ssr */
	} else {
		/* Check if remote side connect */
		if (!spcom_is_channel_connected(ch)) {
			pr_err("ch [%s] remote side not connect.\n", ch->name);
			return -ENOTCONN;
		}
	}

	if (size > SPCOM_MAX_COMMAND_SIZE) {
		pr_err("size [%d] > max size [%d].\n",
			   (int) size , (int) SPCOM_MAX_COMMAND_SIZE);
		return -EINVAL;
	}

	if (*f_pos != 0) {
		pr_err("offset should be zero, no sparse buffer.\n");
		return -EINVAL;
	}

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	ret = copy_from_user(buf, user_buff, size);
	if (ret) {
		pr_err("Unable to copy from user (err %d).\n", ret);
		kfree(buf);
		return -EFAULT;
	}

	ret = spcom_handle_write(ch, buf, size);
	if (ret) {
		pr_err("handle command error [%d].\n", ret);
		kfree(buf);
		return -EFAULT;
	}

	kfree(buf);

	return size;
}

/**
 * spcom_device_read() - handle channel file write() from user space.
 *
 * @filp: file pointer
 *
 * Return: number of bytes to read on success, negative value on
 * failure.
 */
static ssize_t spcom_device_read(struct file *filp, char __user *user_buff,
				 size_t size, loff_t *f_pos)
{
	int ret = 0;
	int actual_size = 0;
	char *buf;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);

	pr_debug("Read file [%s], size = %d bytes.\n", name, (int) size);

	if (!filp || !user_buff || !f_pos ||
	    (size == 0) || (size > SPCOM_MAX_READ_SIZE)) {
		pr_err("invalid parameters.\n");
		return -EINVAL;
	}

	ch = filp->private_data;

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	actual_size = spcom_handle_read(ch, buf, size);
	if ((actual_size <= 0) || (actual_size > size)) {
		pr_err("invalid actual_size [%d].\n", actual_size);
		kfree(buf);
		return -EFAULT;
	}

	ret = copy_to_user(user_buff, buf, actual_size);

	if (ret) {
		pr_err("Unable to copy to user, err = %d.\n", ret);
		kfree(buf);
		return -EFAULT;
	}

	kfree(buf);

	pr_debug("ch [%s] ret [%d].\n", name, (int) actual_size);

	return actual_size;
}

/**
 * spcom_device_poll() - handle channel file poll() from user space.
 *
 * @filp: file pointer
 *
 * This allows user space to wait/check for channel connection,
 * or wait for SSR event.
 *
 * Return: event bitmask on success, set POLLERR on failure.
 */
static unsigned int spcom_device_poll(struct file *filp,
				       struct poll_table_struct *poll_table)
{
	/*
	 * when user call with timeout -1 for blocking mode,
	 * any bit must be set in response
	 */
	unsigned int ret = SPCOM_POLL_READY_FLAG;
	unsigned long mask;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);
	bool wait = false;
	bool done = false;
	/* Event types always implicitly polled for */
	unsigned long reserved = POLLERR | POLLHUP | POLLNVAL;
	int ready = 0;

	ch = filp->private_data;

	mask = poll_requested_events(poll_table);

	pr_debug("== ch [%s] mask [0x%x] ==.\n", name, (int) mask);

	/* user space API has poll use "short" and not "long" */
	mask &= 0x0000FFFF;

	wait = mask & SPCOM_POLL_WAIT_FLAG;
	if (wait)
		pr_debug("ch [%s] wait for event flag is ON.\n", name);
	mask &= ~SPCOM_POLL_WAIT_FLAG; /* clear the wait flag */
	mask &= ~SPCOM_POLL_READY_FLAG; /* clear the ready flag */
	mask &= ~reserved; /* clear the implicitly set reserved bits */

	switch (mask) {
	case SPCOM_POLL_LINK_STATE:
		pr_debug("ch [%s] SPCOM_POLL_LINK_STATE.\n", name);
		if (wait) {
			reinit_completion(&spcom_dev->link_state_changed);
			ready = wait_for_completion_interruptible(
				&spcom_dev->link_state_changed);
			pr_debug("ch [%s] poll LINK_STATE signaled.\n", name);
		}
		done = (spcom_dev->link_state == GLINK_LINK_STATE_UP);
		break;
	case SPCOM_POLL_CH_CONNECT:
		pr_debug("ch [%s] SPCOM_POLL_CH_CONNECT.\n", name);
		if (wait) {
			reinit_completion(&ch->connect);
			ready = wait_for_completion_interruptible(&ch->connect);
			pr_debug("ch [%s] poll CH_CONNECT signaled.\n", name);
		}
		done = completion_done(&ch->connect);
		break;
	default:
		pr_err("ch [%s] poll, invalid mask [0x%x].\n",
			 name, (int) mask);
		ret = POLLERR;
		break;
	}

	if (ready < 0) { /* wait was interrupted */
		pr_debug("ch [%s] poll interrupted, ret [%d].\n", name, ready);
		ret = POLLERR | SPCOM_POLL_READY_FLAG | mask;
	}
	if (done)
		ret |= mask;

	pr_debug("ch [%s] poll, mask = 0x%x, ret=0x%x.\n",
		 name, (int) mask, ret);

	return ret;
}

/* file operation supported from user space */
static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = spcom_device_read,
	.poll = spcom_device_poll,
	.write = spcom_device_write,
	.open = spcom_device_open,
	.release = spcom_device_release,
};

/**
 * spcom_create_channel_chardev() - Create a channel char-dev node file
 * for user space interface
 */
static int spcom_create_channel_chardev(const char *name)
{
	int ret;
	struct device *dev;
	struct spcom_channel *ch;
	dev_t devt;
	struct class *cls = spcom_dev->driver_class;
	struct device *parent = spcom_dev->class_dev;
	void *priv;
	struct cdev *cdev;

	pr_debug("Add channel [%s].\n", name);

	ch = spcom_find_channel_by_name(name);
	if (ch) {
		pr_err("channel [%s] already exist.\n", name);
		return -EINVAL;
	}

	ch = spcom_find_channel_by_name(""); /* find reserved channel */
	if (!ch) {
		pr_err("no free channel.\n");
		return -ENODEV;
	}

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;

	spcom_dev->channel_count++;
	devt = spcom_dev->device_no + spcom_dev->channel_count;
	priv = ch;
	dev = device_create(cls, parent, devt, priv, name);
	if (!dev) {
		pr_err("device_create failed.\n");
		kfree(cdev);
		return -ENODEV;
	}

	cdev_init(cdev, &fops);
	cdev->owner = THIS_MODULE;

	ret = cdev_add(cdev, devt, 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto exit_destroy_device;
	}

	spcom_init_channel(ch, name);

	ch->cdev = cdev;
	ch->dev = dev;

	return 0;

exit_destroy_device:
	device_destroy(spcom_dev->driver_class, devt);
	kfree(cdev);
	return -EFAULT;
}

static int __init spcom_register_chardev(void)
{
	int ret;
	unsigned baseminor = 0;
	unsigned count = 1;
	void *priv = spcom_dev;

	ret = alloc_chrdev_region(&spcom_dev->device_no, baseminor, count,
				 DEVICE_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	spcom_dev->driver_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(spcom_dev->driver_class)) {
		ret = -ENOMEM;
		pr_err("class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	spcom_dev->class_dev = device_create(spcom_dev->driver_class, NULL,
				  spcom_dev->device_no, priv,
				  DEVICE_NAME);

	if (!spcom_dev->class_dev) {
		pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&spcom_dev->cdev, &fops);
	spcom_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&spcom_dev->cdev,
		       MKDEV(MAJOR(spcom_dev->device_no), 0),
		       SPCOM_MAX_CHANNELS);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto exit_destroy_device;
	}

	pr_debug("char device created.\n");

	return 0;

exit_destroy_device:
	device_destroy(spcom_dev->driver_class, spcom_dev->device_no);
exit_destroy_class:
	class_destroy(spcom_dev->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(spcom_dev->device_no, 1);
	return ret;
}

static void spcom_unregister_chrdev(void)
{
	cdev_del(&spcom_dev->cdev);
	device_destroy(spcom_dev->driver_class, spcom_dev->device_no);
	class_destroy(spcom_dev->driver_class);
	unregister_chrdev_region(spcom_dev->device_no, 1);

}

/*======================================================================*/
/*		Device Tree						*/
/*======================================================================*/

static int spcom_parse_dt(struct device_node *np)
{
	int ret;
	const char *propname = "qcom,spcom-ch-names";
	int num_ch = of_property_count_strings(np, propname);
	int i;
	const char *name;

	pr_debug("num of predefined channels [%d].\n", num_ch);

	for (i = 0; i < num_ch; i++) {
		ret = of_property_read_string_index(np, propname, i, &name);
		if (ret) {
			pr_err("failed to read DT channel [%d] name .\n", i);
			return -EFAULT;
		}
		strlcpy(spcom_dev->predefined_ch_name[i],
			name,
			sizeof(spcom_dev->predefined_ch_name[i]));

		pr_debug("found ch [%s].\n", name);
	}

	return num_ch;
}

static int spcom_probe(struct platform_device *pdev)
{
	int ret;
	struct spcom_device *dev = NULL;
	struct glink_link_info link_info;
	struct device_node *np;
	struct link_state_notifier_info *notif_handle;

	if (!pdev) {
		pr_err("invalid pdev.\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("invalid DT node.\n");
		return -EINVAL;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return -ENOMEM;

	spcom_dev = dev;
	mutex_init(&dev->lock);
	init_completion(&dev->link_state_changed);
	spcom_dev->link_state = GLINK_LINK_STATE_DOWN;

	ret = spcom_register_chardev();
	if (ret) {
		pr_err("create character device failed.\n");
		goto fail_reg_chardev;
	}

	link_info.glink_link_state_notif_cb = spcom_link_state_notif_cb;
	link_info.transport = spcom_transport;
	link_info.edge = spcom_edge;

	ret = spcom_parse_dt(np);
	if (ret < 0)
		goto fail_reg_chardev;

	/*
	 * Register for glink link up/down notification.
	 * glink channels can't be opened before link is up.
	 */
	pr_debug("register_link_state_cb(), transport [%s] edge [%s]\n",
		link_info.transport, link_info.edge);
	notif_handle = glink_register_link_state_cb(&link_info, spcom_dev);
	if (!notif_handle) {
		pr_err("glink_register_link_state_cb(), err [%d]\n", ret);
		goto fail_reg_chardev;
	}

	spcom_dev->ion_client = msm_ion_client_create(DEVICE_NAME);
	if (spcom_dev->ion_client == NULL) {
		pr_err("fail to create ion client.\n");
		goto fail_reg_chardev;
	}

	pr_info("Driver Initialization ok.\n");

	return 0;

fail_reg_chardev:
	pr_err("Failed to init driver.\n");
	spcom_unregister_chrdev();
	kfree(dev);
	spcom_dev = NULL;

	return -ENODEV;
}

static const struct of_device_id spcom_match_table[] = {
	{ .compatible = "qcom,spcom", },
	{ },
};

static struct platform_driver spcom_driver = {
	.probe = spcom_probe,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(spcom_match_table),
	},
};

/*======================================================================*/
/*		Driver Init/Exit					*/
/*======================================================================*/

static int __init spcom_init(void)
{
	int ret;

	pr_info("spcom driver Ver 1.0 23-Nov-2015.\n");

	ret = platform_driver_register(&spcom_driver);
	if (ret)
		pr_err("spcom_driver register failed %d\n", ret);

	return 0;
}
module_init(spcom_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure Processor Communication");
