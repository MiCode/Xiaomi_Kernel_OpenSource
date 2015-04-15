/*
 * Most HECI provider device and HECI logic declarations
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
 */

#ifndef _HECI_DEV_H_
#define _HECI_DEV_H_

#include "platform-config.h"
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/poll.h>
#include "bus.h"
#include "hbm.h"
#include <linux/spinlock.h>

#define	IPC_PAYLOAD_SIZE	128
#define HECI_RD_MSG_BUF_SIZE	IPC_PAYLOAD_SIZE
/* Number of messages to be held in ISR->BH FIFO */
#define	RD_INT_FIFO_SIZE	64
#define	IPC_FULL_MSG_SIZE	132
/*
 * Number of IPC messages to be held in Tx FIFO, to be sent by ISR -
 * Tx complete interrupt or RX_COMPLETE handler
 */
#define	IPC_TX_FIFO_SIZE	512

/*
 * Number of Maximum HECI Clients
 */
#define HECI_CLIENTS_MAX 256

/*
 * Number of File descriptors/handles
 * that can be opened to the driver.
 *
 * Limit to 255: 256 Total Clients
 * minus internal client for HECI Bus Messags
 */
#define  HECI_MAX_OPEN_HANDLE_COUNT (HECI_CLIENTS_MAX - 1)

/*
 * Internal Clients Number
 */
#define HECI_HOST_CLIENT_ID_ANY        (-1)
#define HECI_HBM_HOST_CLIENT_ID         0 /* not used, just for documentation */

/* HECI device states */
enum heci_dev_state {
	HECI_DEV_INITIALIZING = 0,
	HECI_DEV_INIT_CLIENTS,
	HECI_DEV_ENABLED,
	HECI_DEV_RESETTING,
	HECI_DEV_DISABLED,
	HECI_DEV_POWER_DOWN,
	HECI_DEV_POWER_UP
};

const char *heci_dev_state_str(int state);

/**
 * struct heci_me_client - representation of me (fw) client
 *
 * @props  - client properties
 * @client_id - me client id
 */
struct heci_me_client {
	struct heci_client_properties props;
	u8 client_id;
};


struct heci_cl;

/*
 * Intel HECI message data struct
 */
struct heci_msg_data {
	u32 size;
	int	dma_flag;	/* non-0 if this is DMA msg buf */
	unsigned char *data;
};

/**
 * struct heci_cl_rb - request block (was: callback) structure
 *
 * @cl - client who is running this operation
 * @type - request type
 */
struct heci_cl_rb {
	struct list_head list;
	struct heci_cl *cl;
	struct heci_msg_data buffer;
	unsigned long buf_idx;
	unsigned long read_time;
};


struct wr_msg_ctl_info {
	void	(*ipc_send_compl)(void *);	/* Will be called with
					'ipc_send_compl_prm' as parameter */
	void	*ipc_send_compl_prm;
	size_t length;
	struct list_head	link;
	unsigned char	inline_data[IPC_FULL_MSG_SIZE];
};

/** struct heci_hw_ops
 *
 * @host_is_ready    - query for host readiness
 * @hw_is_ready      - query if hw is ready
 * @hw_reset         - reset hw
 * @hw_start         - start hw after reset
 * @hw_config        - configure hw
 * @write            - write a message to FW
 */
struct heci_hw_ops {
	bool (*host_is_ready)(struct heci_device *dev);
	bool (*hw_is_ready)(struct heci_device *dev);
	int (*hw_reset)(struct heci_device *dev, bool enable);
	int (*hw_start)(struct heci_device *dev);
	void (*hw_config)(struct heci_device *dev);
	int (*write)(struct heci_device *dev, struct heci_msg_hdr *hdr,
		unsigned char *buf);
	int (*write_ex)(struct heci_device *dev, struct heci_msg_hdr *hdr,
		void *msg, void(*ipc_send_compl)(void *),
		void *ipc_send_compl_prm);
	int (*read)(struct heci_device *dev, unsigned char *buffer,
		unsigned long buffer_length);
	u32 (*get_fw_status)(struct heci_device *dev);
};

#define PRINT_BUFFER_SIZE 204800

/**
 * struct heci_device -  HECI private device struct
 *
 * @hbm_state - state of host bus message protocol
 * @mem_addr - mem mapped base register address
 */
struct heci_device {
	struct pci_dev *pdev;	/* pointer to pci device struct */
	/*
	 * lists of queues
	 */

	/* array of pointers to aio lists */
	struct heci_cl_rb read_list;		/* driver read queue */
	spinlock_t      read_list_spinlock;

	/*
	 * list of heci_cl's (formerly: files)
	 */
	struct list_head cl_list;
	long open_handle_count;			/* Why's this?.. */

	/*
	 * lock for the device
	 * for everything that doesn't have a dedicated spinlock
	 */
	spinlock_t	device_lock;

	bool recvd_hw_ready;
	/*
	 * waiting queue for receive message from FW
	 */
	wait_queue_head_t wait_hw_ready;
	wait_queue_head_t wait_hbm_recvd_msg;
	wait_queue_head_t wait_dma_ready;

	/*
	 * heci device  states
	 */
	enum heci_dev_state dev_state;
	enum heci_hbm_state hbm_state;

	/* FIFO for input messages for BH processing */
	unsigned char	rd_msg_fifo[RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE];
	unsigned	rd_msg_fifo_head, rd_msg_fifo_tail;
	spinlock_t	rd_msg_spinlock;
	struct work_struct	bh_hbm_work;

#if 0
	/*
	 * FIFO for output IPC messages. Includes also HECI/IPC header
	 * to be supplied in DRBL (first dword)
	 */
	unsigned char	wr_msg_fifo[IPC_TX_FIFO_SIZE * IPC_FULL_MSG_SIZE];
#endif
	/*
	 * Control info for IPC messages HECI/IPC sending FIFO -
	 * list with inline data buffer
	 * This structure will be filled with parameters submitted
	 * by the caller glue layer
	 * 'buf' may be pointing to the external buffer or to 'inline_data'
	 * 'offset' will be initialized to 0 by submitting
	 *
	 * 'ipc_send_compl' is intended for use by clients that send fragmented
	 * messages. When a fragment is sent down to IPC msg regs,
	 * it will be called.
	 * If it has more fragments to send, it will do it. With last fragment
	 * it will send appropriate HECI "message-complete" flag.
	 * It will remove the outstanding message
	 * (mark outstanding buffer as available).
	 * If counting flow control is in work and there are more flow control
	 * credits, it can put the next client message queued in cl.
	 * structure for IPC processing.
	 *
	 * (!) We can work on FIFO list or cyclic FIFO in an array
	 */

	struct wr_msg_ctl_info wr_processing_list_head, wr_free_list_head;
	spinlock_t wr_processing_spinlock;	/* For both processing
						   and free lists */
	spinlock_t	out_ipc_spinlock;
/*
	unsigned	wr_msg_fifo_head, wr_msg_fifo_tail;
	spinlock_t	wr_msg_spinlock;
*/
	struct hbm_version version;
	struct heci_me_client *me_clients; /*Note: memory has to be allocated*/
	DECLARE_BITMAP(me_clients_map, HECI_CLIENTS_MAX);
	DECLARE_BITMAP(host_clients_map, HECI_CLIENTS_MAX);
	u8 me_clients_num;
	u8 me_client_presentation_num;
	u8 me_client_index;

	/* List of bus devices */
	struct list_head device_list;

	/* buffer to save prints from driver */
	unsigned char log_buffer[PRINT_BUFFER_SIZE];
	size_t log_head;
	size_t log_tail;
	void (*print_log)(struct heci_device *dev, char *format, ...);
	spinlock_t      log_spinlock;   /* spinlock to protect prints buffer */
	unsigned long	max_log_sec, max_log_usec;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif /* CONFIG_DEBUG_FS */

	/* Debug stats */
	unsigned	ipc_hid_out_fc;
	int	ipc_hid_in_msg;
	unsigned	ipc_hid_in_fc;
	int	ipc_hid_out_msg;
	unsigned	ipc_hid_out_fc_cnt;
	unsigned	ipc_hid_in_fc_cnt;

	unsigned	ipc_rx_cnt;
	unsigned long long	ipc_rx_bytes_cnt;
	unsigned	ipc_tx_cnt;
	unsigned long long	ipc_tx_bytes_cnt;

	const struct heci_hw_ops *ops;

	size_t	mtu;
	char hw[0] __aligned(sizeof(void *));
};

/*
 * heci init function prototypes
 */
void heci_device_init(struct heci_device *dev);
void heci_reset(struct heci_device *dev, int interrupts);
int heci_start(struct heci_device *dev);
void heci_stop(struct heci_device *dev);

static inline unsigned long heci_secs_to_jiffies(unsigned long sec)
{
	return sec * HZ;	/*msecs_to_jiffies(sec * MSEC_PER_SEC);*/
}

/*
 * Register Access Function
 */
static inline void heci_hw_config(struct heci_device *dev)
{
	dev->ops->hw_config(dev);
}
static inline int heci_hw_reset(struct heci_device *dev, bool enable)
{
	return dev->ops->hw_reset(dev, enable);
}

static inline int heci_hw_start(struct heci_device *dev)
{
	return dev->ops->hw_start(dev);
}

static inline bool heci_host_is_ready(struct heci_device *dev)
{
	return dev->ops->host_is_ready(dev);
}
static inline bool heci_hw_is_ready(struct heci_device *dev)
{
	return dev->ops->hw_is_ready(dev);
}

static inline int heci_write_message(struct heci_device *dev,
	struct heci_msg_hdr *hdr, unsigned char *buf)
{
	return dev->ops->write_ex(dev, hdr, buf, NULL, NULL);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int heci_dbgfs_register(struct heci_device *dev, const char *name);
void heci_dbgfs_deregister(struct heci_device *dev);
#else
static inline int heci_dbgfs_register(struct heci_device *dev, const char *name)
{
	return 0;
}
static inline void heci_dbgfs_deregister(struct heci_device *dev) {}
#endif /* CONFIG_DEBUG_FS */


int heci_register(struct heci_device *dev);
void heci_deregister(struct heci_device *dev);

void    heci_bus_remove_all_clients(struct heci_device *heci_dev);

#define HECI_HDR_FMT "hdr:host=%02d me=%02d len=%d comp=%1d"
#define HECI_HDR_PRM(hdr)		\
	((hdr)->host_addr, (hdr)->me_addr,	\
	(hdr)->length, (hdr)->msg_complete)

#endif /*_HECI_DEV_H_*/

