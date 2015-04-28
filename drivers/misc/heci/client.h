/*
 * HECI client logic
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

#ifndef _HECI_CLIENT_H_
#define _HECI_CLIENT_H_

#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/poll.h>
#include "heci_dev.h"

/* Client state */
enum cl_state {
	HECI_CL_INITIALIZING = 0,
	HECI_CL_CONNECTING,
	HECI_CL_CONNECTED,
	HECI_CL_DISCONNECTING,
	HECI_CL_DISCONNECTED
};

#define	CL_DEF_RX_RING_SIZE	2
#define	CL_DEF_TX_RING_SIZE	2
#define	CL_MAX_RX_RING_SIZE	32
#define	CL_MAX_TX_RING_SIZE	32

/* Client Tx  buffer list entry */
struct heci_cl_tx_ring {
	struct list_head list;
	struct heci_msg_data	send_buf;
};

/* HECI client instance carried as file->pirvate_data*/
struct heci_cl {
	struct list_head link;
	struct heci_device *dev;
	enum cl_state state;
	wait_queue_head_t rx_wait;
	wait_queue_head_t wait;
	int status;
	/* ID of client connected */
	u8 host_client_id;
	u8 me_client_id;
	u8 heci_flow_ctrl_creds;
	u8 out_flow_ctrl_creds;
	struct heci_cl_rb *read_rb;

	/* Link to HECI bus device */
	struct heci_cl_device *device;

	/* Rx ring buffer pool */
	unsigned	rx_ring_size;
	struct heci_cl_rb	free_rb_list;
	/*int     send_fc_flag;*/
	spinlock_t      free_list_spinlock;
	/* Rx in-process list */
	struct heci_cl_rb       in_process_list;
	spinlock_t      in_process_spinlock;

	/* Client Tx buffers list */
	unsigned	tx_ring_size;
	struct heci_cl_tx_ring	tx_list, tx_free_list;
	spinlock_t      tx_list_spinlock;
	spinlock_t      tx_free_list_spinlock;
	size_t	tx_offs;			/* Offset in buffer at head of 'tx_list' */
	/*#############################*/
	/* if we get a FC, and the list is not empty, we must know whether we
	 * are at the middle of sending.
	 * if so - need to increase FC counter, otherwise, need to start sending
	 * the first msg in list
	 * (!) This is for counting-FC implementation only. Within single-FC the
	 * other party may NOT send FC until it receives complete message
	 */
	int sending;
	/*#############################*/

	/* Send FC spinlock */
	spinlock_t      fc_spinlock;

	/* wait queue for connect and disconnect response from FW */
	wait_queue_head_t wait_ctrl_res;

	/* Error stats */
	unsigned	err_send_msg;
	unsigned	err_send_fc;

	/* Send/recv stats */
	unsigned	send_msg_cnt;
	unsigned	recv_msg_cnt;
	unsigned	recv_msg_num_frags;
	unsigned	heci_flow_ctrl_cnt;
	unsigned	out_flow_ctrl_cnt;

	/* Rx msg ... out FC timing */
	unsigned long	rx_sec, rx_usec;
	unsigned long	out_fc_sec, out_fc_usec;
	unsigned long	max_fc_delay_sec, max_fc_delay_usec;
};

extern int	dma_ready;
extern int	host_dma_enabled;

int heci_can_client_connect(struct heci_device *heci_dev, uuid_le *uuid);
int heci_me_cl_by_uuid(struct heci_device *dev, const uuid_le *cuuid);
int heci_me_cl_by_id(struct heci_device *dev, u8 client_id);

/*
 * HECI IO Functions
 */
struct heci_cl_rb *heci_io_rb_init(struct heci_cl *cl);
void heci_io_rb_free(struct heci_cl_rb *priv_rb);
int heci_io_rb_alloc_buf(struct heci_cl_rb *rb, size_t length);
int heci_io_rb_recycle(struct heci_cl_rb *rb);


/**
 * heci_io_list_init - Sets up a queue list.
 *
 * @list: An instance cl callback structure
 */
static inline void heci_io_list_init(struct heci_cl_rb *list)
{
	INIT_LIST_HEAD(&list->list);
}
void heci_read_list_flush(struct heci_cl *cl);

/*
 * HECI Host Client Functions
 */

struct heci_cl *heci_cl_allocate(struct heci_device *dev);
void heci_cl_init(struct heci_cl *cl, struct heci_device *dev);
void	heci_cl_free(struct heci_cl *cl);

int	heci_cl_alloc_rx_ring(struct heci_cl *cl);
int	heci_cl_alloc_tx_ring(struct heci_cl *cl);
int	heci_cl_free_rx_ring(struct heci_cl *cl);
int	heci_cl_free_tx_ring(struct heci_cl *cl);

int heci_cl_link(struct heci_cl *cl, int id);
int heci_cl_unlink(struct heci_cl *cl);

int heci_cl_flush_queues(struct heci_cl *cl);
struct heci_cl_rb *heci_cl_find_read_rb(struct heci_cl *cl);

/**
 * heci_cl_cmp_id - tells if file private data have same id
 *
 * @fe1: private data of 1. file object
 * @fe2: private data of 2. file object
 *
 * returns true  - if ids are the same and not NULL
 */
static inline bool heci_cl_cmp_id(const struct heci_cl *cl1,
				const struct heci_cl *cl2)
{
	return cl1 && cl2 &&
		(cl1->host_client_id == cl2->host_client_id) &&
		(cl1->me_client_id == cl2->me_client_id);
}


int heci_cl_flow_ctrl_creds(struct heci_cl *cl);

/*
 *  HECI input output function prototype
 */
bool heci_cl_is_other_connecting(struct heci_cl *cl);
int heci_cl_disconnect(struct heci_cl *cl);
int heci_cl_connect(struct heci_cl *cl);
int heci_cl_read_start(struct heci_cl *cl);
int heci_cl_send(struct heci_cl *cl, u8 *buf, size_t length);
void heci_cl_read_complete(struct heci_cl_rb *rb);
void heci_cl_all_disconnect(struct heci_device *dev);
void heci_cl_all_read_wakeup(struct heci_device *dev);
void heci_cl_send_msg(struct heci_device *dev, struct heci_cl *cl);
void	heci_cl_alloc_dma_buf(void);
void	recv_heci_cl_msg(struct heci_device *dev, struct heci_msg_hdr *heci_hdr);

#endif /* _HECI_CLIENT_H_ */

