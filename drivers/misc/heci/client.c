/*
 * HECI client logic (for both HECI bus driver and user-mode API)
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
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include "heci_dev.h"
#include "hbm.h"
#include "client.h"
#include "utils.h"

#ifdef dev_dbg
#undef dev_dbg
#endif
static void no_dev_dbg(void *v, char *s, ...)
{
}
#define dev_dbg no_dev_dbg
/*#define dev_dbg dev_err*/

int	host_dma_enabled;
void	*host_dma_buf;
unsigned	host_dma_buf_size = (1024*1024);
uint64_t	host_dma_buf_phys;
int	dma_ready = 1;


void	heci_cl_alloc_dma_buf(void)
{
	int	order;
	unsigned	temp;

	/* Try to allocate 256 contiguous pages (1 M) for DMA and enabled host DMA */
	for (order = 0, temp = host_dma_buf_size / PAGE_SIZE + 1; temp; temp >>= 1)
		++order;
	host_dma_buf = (void *)__get_free_pages(GFP_KERNEL, order);
	if (host_dma_buf) {
		host_dma_buf_phys = __pa(host_dma_buf);
		host_dma_enabled = 1;
	}

	ISH_DBG_PRINT(KERN_ALERT "%s(): host_dma_enabled=%d host_dma_buf=%p host_dma_buf_phys=%llX host_dma_buf_size=%u order=%d\n", __func__, host_dma_enabled, host_dma_buf, host_dma_buf_phys, host_dma_buf_size, order);
}


/**
 * heci_io_list_flush - removes list entry belonging to cl.
 *
 * @list:  An instance of our list structure
 * @cl: host client
 */
void heci_io_list_flush(struct heci_cl_rb *list, struct heci_cl *cl)
{
	struct heci_cl_rb *rb;
	struct heci_cl_rb *next;

	list_for_each_entry_safe(rb, next, &list->list, list) {
		if (rb->cl && heci_cl_cmp_id(cl, rb->cl)) {
			list_del(&rb->list);
			heci_io_rb_free(rb);
		}
	}
}

/**
 * heci_io_rb_free - free heci_rb_private related memory
 *
 * @rb: heci callback struct
 */
void heci_io_rb_free(struct heci_cl_rb *rb)
{
	if (rb == NULL)
		return;

	kfree(rb->buffer.data);
	kfree(rb);
}
EXPORT_SYMBOL(heci_io_rb_free);

/**
 * heci_io_rb_init - allocate and initialize io callback
 *
 * @cl - heci client
 * @file: pointer to file structure
 *
 * returns heci_cl_rb pointer or NULL;
 */
struct heci_cl_rb *heci_io_rb_init(struct heci_cl *cl)
{
	struct heci_cl_rb *rb;

	rb = kzalloc(sizeof(struct heci_cl_rb), GFP_KERNEL);
	if (!rb)
		return NULL;

	heci_io_list_init(rb);

	rb->cl = cl;
	rb->buf_idx = 0;
	return rb;
}


/**
 * heci_io_rb_alloc_buf - allocate respose buffer
 *
 * @rb -  io callback structure
 * @size: size of the buffer
 *
 * returns 0 on success
 *         -EINVAL if rb is NULL
 *         -ENOMEM if allocation failed
 */
int heci_io_rb_alloc_buf(struct heci_cl_rb *rb, size_t length)
{
	if (!rb)
		return -EINVAL;

	if (length == 0)
		return 0;

	rb->buffer.data = kmalloc(length, GFP_KERNEL);
	if (!rb->buffer.data)
		return -ENOMEM;
	rb->buffer.size = length;
	return 0;
}


/*
 * heci_io_rb_recycle - re-append rb to its client's free list and send flow control if needed
 */
int heci_io_rb_recycle(struct heci_cl_rb *rb)
{
	struct heci_cl *cl;
	int	rets = 0;
	unsigned long	flags;

	if (!rb || !rb->cl)
		return	-EFAULT;

	cl = rb->cl;

	spin_lock_irqsave(&cl->free_list_spinlock, flags);
	list_add_tail(&rb->list, &cl->free_rb_list.list);
	spin_unlock_irqrestore(&cl->free_list_spinlock, flags);

	/* If we returned the first buffer to empty 'free' list, send flow control */
	if (!cl->out_flow_ctrl_creds) {
		rets = heci_cl_read_start(cl);
	}

	return	rets;
}
EXPORT_SYMBOL(heci_io_rb_recycle);


/**
 * heci_cl_flush_queues - flushes queue lists belonging to cl.
 *
 * @dev: the device structure
 * @cl: host client
 */
int heci_cl_flush_queues(struct heci_cl *cl)
{
	if (WARN_ON(!cl || !cl->dev))
		return -EINVAL;

	dev_dbg(&cl->dev->pdev->dev, "remove list entry belonging to cl\n");
	heci_io_list_flush(&cl->dev->read_list, cl);

	return 0;
}
EXPORT_SYMBOL(heci_cl_flush_queues);


/**
 * heci_cl_init - initializes intialize cl.
 *
 * @cl: host client to be initialized
 * @dev: heci device
 */
void heci_cl_init(struct heci_cl *cl, struct heci_device *dev)
{
	memset(cl, 0, sizeof(struct heci_cl));
	init_waitqueue_head(&cl->wait);
	init_waitqueue_head(&cl->rx_wait);
	init_waitqueue_head(&cl->wait_ctrl_res);
	spin_lock_init(&cl->free_list_spinlock);
	spin_lock_init(&cl->in_process_spinlock);
	INIT_LIST_HEAD(&cl->link);
	cl->dev = dev;

	INIT_LIST_HEAD(&cl->free_rb_list.list);
	INIT_LIST_HEAD(&cl->tx_list.list);
	INIT_LIST_HEAD(&cl->tx_free_list.list);
	INIT_LIST_HEAD(&cl->in_process_list.list);

	cl->rx_ring_size = CL_DEF_RX_RING_SIZE;
	cl->tx_ring_size = CL_DEF_TX_RING_SIZE;
}

int	heci_cl_free_rx_ring(struct heci_cl *cl)
{
	struct heci_cl_rb * rb;

	/* relese allocated mem- pass over free_rb_list */
	while (!list_empty(&cl->free_rb_list.list)) {
		rb = list_entry(cl->free_rb_list.list.next, struct heci_cl_rb, list);
		list_del(&rb->list);
		kfree(rb->buffer.data);
		kfree(rb);
	}

	/* relese allocated mem- pass over in_process_list */
	while (!list_empty(&cl->in_process_list.list)) {
		rb = list_entry(cl->in_process_list.list.next, struct heci_cl_rb, list);
		list_del(&rb->list);
		kfree(rb->buffer.data);
		kfree(rb);
	}

	return	0;
}

int	heci_cl_free_tx_ring(struct heci_cl *cl)
{
	struct heci_cl_tx_ring  *tx_buf;

	/* relese allocated mem- pass over tx_free_list */
	while (!list_empty(&cl->tx_free_list.list)) {
		tx_buf = list_entry(cl->tx_free_list.list.next, struct heci_cl_tx_ring, list);
		list_del(&tx_buf->list);
		kfree(tx_buf->send_buf.data);
		kfree(tx_buf);
	}

	/* relese allocated mem- pass over tx_list */
	while (!list_empty(&cl->tx_list.list)) {
		tx_buf = list_entry(cl->tx_list.list.next, struct heci_cl_tx_ring, list);
		list_del(&tx_buf->list);
		kfree(tx_buf->send_buf.data);
		kfree(tx_buf);
	}

	return	0;
}

int	heci_cl_alloc_rx_ring(struct heci_cl *cl)
{
	size_t	len = cl->device->fw_client->props.max_msg_length;
	int	j;
	struct heci_cl_rb *rb;
	int	ret;
	struct heci_device *dev = cl->dev;

	for (j = 0; j < cl->rx_ring_size; ++j) {
		rb = heci_io_rb_init(cl);
		if (!rb)
			goto out;
		ret = heci_io_rb_alloc_buf(rb, len);
		if (ret)
			goto out;
		list_add_tail(&rb->list, &cl->free_rb_list.list);
	}

	ISH_DBG_PRINT(KERN_ALERT "%s() allocated rb pool successfully\n", __func__);
	return	0;

out:
	dev_err(&dev->pdev->dev, "%s() error in allocating rb pool\n", __func__);
	heci_cl_free_rx_ring(cl);
	return	ret;
}


int	heci_cl_alloc_tx_ring(struct heci_cl *cl)
{
	size_t	len = cl->device->fw_client->props.max_msg_length;
	int	j;
	struct heci_device *dev = cl->dev;

	/*cl->send_fc_flag = 0;*/
	ISH_DBG_PRINT(KERN_ALERT "%s() allocated rb pool successfully\n", __func__);

	/* Allocate pool to free Tx bufs */
	for (j = 0; j < cl->tx_ring_size; ++j) {
		struct heci_cl_tx_ring	*tx_buf;

		tx_buf = kmalloc(sizeof(struct heci_cl_tx_ring), GFP_KERNEL);
		if (!tx_buf) {
			dev_err(&dev->pdev->dev, "%s(): error allocating Tx buffers\n", __func__);
			goto	out;
		}
		memset(tx_buf, 0, sizeof(struct heci_cl_tx_ring));
		tx_buf->send_buf.data = kmalloc(len, GFP_KERNEL);
		if (!tx_buf->send_buf.data) {
			dev_err(&dev->pdev->dev, "%s(): error allocating Tx buffers\n", __func__);
			kfree(tx_buf);
			goto	out;
		}
		list_add_tail(&tx_buf->list, &cl->tx_free_list.list);
	}
	ISH_DBG_PRINT(KERN_ALERT "%s() allocated Tx  pool successfully\n", __func__);

	spin_lock_init(&cl->tx_spinlock);	
	return	0;

out:
	dev_err(&dev->pdev->dev, "%s() error in allocating rb pool\n", __func__);
	heci_cl_free_rx_ring(cl);
	return	-ENOMEM;
}


/**
 * heci_cl_allocate - allocates cl  structure and sets it up.
 *
 * @dev: heci device
 * returns  The allocated file or NULL on failure
 */
struct heci_cl *heci_cl_allocate(struct heci_device *dev)
{
	struct heci_cl *cl;

	cl = kmalloc(sizeof(struct heci_cl), GFP_ATOMIC);
	if (!cl)
		return NULL;

	heci_cl_init(cl, dev);
	return cl;
}
EXPORT_SYMBOL(heci_cl_allocate);


void	heci_cl_free(struct heci_cl *cl)
{
	if (!cl)
		return;

	heci_cl_free_rx_ring(cl);
	heci_cl_free_tx_ring(cl);
	kfree(cl);
}


/**
 * heci_cl_find_read_rb - find this cl's callback in the read list
 *
 * @dev: device structure
 * returns rb on success, NULL on error
 */
struct heci_cl_rb *heci_cl_find_read_rb(struct heci_cl *cl)
{
	struct heci_device *dev = cl->dev;
	struct heci_cl_rb *rb = NULL;
	struct heci_cl_rb *next = NULL;
	unsigned long     dev_flags;

	spin_lock_irqsave(&dev->read_list_spinlock, dev_flags);
	list_for_each_entry_safe(rb, next, &dev->read_list.list, list)
		if (heci_cl_cmp_id(cl, rb->cl)) {
			spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
			return rb;
		}
	spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
	return NULL;
}
EXPORT_SYMBOL(heci_cl_find_read_rb);

/** heci_cl_link: allocte host id in the host map
 *
 * @cl - host client
 * @id - fixed host id or -1 for genereting one
 * returns 0 on success
 *	-EINVAL on incorrect values
 *	-ENONET if client not found
 */
int heci_cl_link(struct heci_cl *cl, int id)
{
	struct heci_device *dev;
	unsigned long	flags;

	if (WARN_ON(!cl || !cl->dev))
		return -EINVAL;

	dev = cl->dev;

	spin_lock_irqsave(&dev->device_lock, flags);

	if (dev->open_handle_count >= HECI_MAX_OPEN_HANDLE_COUNT) {
		spin_unlock_irqrestore(&dev->device_lock, flags);
		return	-EMFILE;
	}

	/* If Id is not asigned get one*/
	if (id == HECI_HOST_CLIENT_ID_ANY)
		id = find_first_zero_bit(dev->host_clients_map, HECI_CLIENTS_MAX);

	if (id >= HECI_CLIENTS_MAX) {
		spin_unlock_irqrestore(&dev->device_lock, flags);
		dev_err(&dev->pdev->dev, "id exceded %d", HECI_CLIENTS_MAX);
		return -ENOENT;
	}

	dev->open_handle_count++;
	cl->host_client_id = id;
	list_add_tail(&cl->link, &dev->cl_list);
	set_bit(id, dev->host_clients_map);
	cl->state = HECI_CL_INITIALIZING;
	spin_unlock_irqrestore(&dev->device_lock, flags);

	dev_dbg(&dev->pdev->dev, "link cl host id = %d\n", cl->host_client_id);

	return 0;
}
EXPORT_SYMBOL(heci_cl_link);

/**
 * heci_cl_unlink - remove me_cl from the list
 *
 * @dev: the device structure
 */
int heci_cl_unlink(struct heci_cl *cl)
{
	struct heci_device *dev;
	struct heci_cl *pos, *next;
	unsigned long	flags;

	/* don't shout on error exit path */
	if (!cl || !cl->dev)
		return 0;

	dev = cl->dev;

	spin_lock_irqsave(&dev->device_lock, flags);
		
	if (dev->open_handle_count > 0) {
		clear_bit(cl->host_client_id, dev->host_clients_map);
		dev->open_handle_count--;
	}

	/* This checks that 'cl' is actually linked into device's structure, before attempting 'list_del' */
	list_for_each_entry_safe(pos, next, &dev->cl_list, link) {
		if (cl->host_client_id == pos->host_client_id) {
			list_del_init(&pos->link);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->device_lock, flags);

	return 0;
}
EXPORT_SYMBOL(heci_cl_unlink);


/**
 * heci_cl_disconnect - disconnect host clinet form the me one
 *
 * @cl: host client
 *
 * Locking: called under "dev->device_lock" lock
 *
 * returns 0 on success, <0 on failure.
 */
int heci_cl_disconnect(struct heci_cl *cl)
{
	struct heci_device *dev;
	int rets, err;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (cl->state != HECI_CL_DISCONNECTING)
		return 0;

	rets = pm_runtime_get_sync(&dev->pdev->dev);
	dev_dbg(&dev->pdev->dev, "rpm: get sync %d\n", rets);
	if (IS_ERR_VALUE(rets)) {
		dev_err(&dev->pdev->dev, "rpm: get sync failed %d\n", rets);
		return rets;
	}

	if (heci_hbm_cl_disconnect_req(dev, cl)) {
		rets = -ENODEV;
		dev_err(&dev->pdev->dev, "failed to disconnect.\n");
		goto free;
	}

	err = wait_event_timeout(cl->wait_ctrl_res,
			(dev->dev_state == HECI_DEV_ENABLED && HECI_CL_DISCONNECTED == cl->state),
			heci_secs_to_jiffies(HECI_CL_CONNECT_TIMEOUT));

	/* If FW reset arrived, this will happen. Don't check cl->, as 'cl' may be freed already */
	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto	free;
	}

	if (HECI_CL_DISCONNECTED == cl->state) {
		rets = 0;
		dev_dbg(&dev->pdev->dev, "successfully disconnected from FW client.\n");
	} else {
		rets = -ENODEV;
		if (HECI_CL_DISCONNECTED != cl->state)
			dev_dbg(&dev->pdev->dev, "wrong status client disconnect.\n");

		if (err)
			dev_dbg(&dev->pdev->dev,
				"wait failed disconnect err=%08x\n", err);

		dev_dbg(&dev->pdev->dev, "failed to disconnect from FW client.\n");
	}

free:
	dev_dbg(&dev->pdev->dev, "rpm: autosuspend\n");
	pm_runtime_mark_last_busy(&dev->pdev->dev);
	pm_runtime_put_autosuspend(&dev->pdev->dev);

	return rets;
}
EXPORT_SYMBOL(heci_cl_disconnect);


/**
 * heci_cl_is_other_connecting - checks if other
 *    client with the same me client id is connecting
 *
 * @cl: private data of the file object
 *
 * returns ture if other client is connected, 0 - otherwise.
 */
bool heci_cl_is_other_connecting(struct heci_cl *cl)
{
	struct heci_device *dev;
	struct heci_cl *pos;
	struct heci_cl *next;
	unsigned long	flags;

	if (WARN_ON(!cl || !cl->dev))
		return false;

	dev = cl->dev;

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(pos, next, &dev->cl_list, link) {
		if ((pos->state == HECI_CL_CONNECTING) && (pos != cl) && cl->me_client_id == pos->me_client_id) {
			spin_unlock_irqrestore(&dev->device_lock, flags);
			return true;
		}

	}
	spin_unlock_irqrestore(&dev->device_lock, flags);

	return false;
}

/**
 * heci_cl_connect - connect host clinet to the me one
 *
 * @cl: host client
 *
 * Locking: called under "dev->device_lock" lock
 *
 * returns 0 on success, <0 on failure.
 */
int heci_cl_connect(struct heci_cl *cl)
{
	struct heci_device *dev;
	long timeout = heci_secs_to_jiffies(HECI_CL_CONNECT_TIMEOUT);
	int rets;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	if (heci_cl_is_other_connecting(cl))
		return	-EBUSY;

	dev = cl->dev;

	rets = pm_runtime_get_sync(&dev->pdev->dev);
	dev_dbg(&dev->pdev->dev, "rpm: get sync %d\n", rets);
	if (IS_ERR_VALUE(rets)) {
		dev_err(&dev->pdev->dev, "rpm: get sync failed %d\n", rets);
		return rets;
	}

	if (heci_hbm_cl_connect_req(dev, cl)) {
		rets = -ENODEV;
		goto out;
	}

	rets = wait_event_timeout(cl->wait_ctrl_res,
				(dev->dev_state == HECI_DEV_ENABLED &&
				 (cl->state == HECI_CL_CONNECTED ||
				  cl->state == HECI_CL_DISCONNECTED)),
				 timeout * HZ);

	/* If FW reset arrived, this will happen. Don't check cl->, as 'cl' may be freed already */
	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -EFAULT;
		goto	out;
	}

	if (cl->state != HECI_CL_CONNECTED) {
		rets = -EFAULT;
		goto out;
	}

	rets = cl->status;
	if (rets)
		goto	out;

	rets = heci_cl_device_bind(cl);
	if (rets) {
		heci_cl_disconnect(cl);
		goto    out;
	}

	rets = heci_cl_alloc_rx_ring(cl);
	if (rets) {
		/* if failed allocation, disconnect */
		heci_cl_disconnect(cl);
		goto	out;
	}

	rets = heci_cl_alloc_tx_ring(cl);
	if (rets) {
		/* if failed allocation, disconnect */
		heci_cl_free_rx_ring(cl);
		heci_cl_disconnect(cl);
		goto	out;
	}

	/* Upon successful connection and allocation, emit flow-control */
dev->print_log(dev, "%s(): call heci_cl_read_start, out_flow_ctrl_creds is 0\n", __func__);
	rets = heci_cl_read_start(cl);
out:
	dev_dbg(&dev->pdev->dev, "rpm: autosuspend\n");
	pm_runtime_mark_last_busy(&dev->pdev->dev);
	pm_runtime_put_autosuspend(&dev->pdev->dev);

	return rets;
}
EXPORT_SYMBOL(heci_cl_connect);

/**
 * heci_cl_flow_ctrl_creds - checks flow_control credits for cl.
 *
 * @dev: the device structure
 * @cl: private data of the file object
 *
 * returns 1 if heci_flow_ctrl_creds >0, 0 - otherwise.
 *	-ENOENT if heci_cl is not present
 *	-EINVAL if single_recv_buf == 0
 */
int heci_cl_flow_ctrl_creds(struct heci_cl *cl)
{
	struct heci_device *dev;
	struct heci_me_client  *me_cl = cl->device->fw_client;

	if (WARN_ON(!cl || !cl->dev))
		return -EINVAL;

	dev = cl->dev;

	if (!dev->me_clients_num)
		return 0;

	if (cl->heci_flow_ctrl_creds > 0)
		return 1;

	if (me_cl->heci_flow_ctrl_creds) {
		if (WARN_ON(me_cl->props.single_recv_buf == 0)) /* fixed client: single_recv_buf != 0*/
			return -EINVAL;
		return 1;
	} else {
		return 0;
	}
	return -ENOENT;
}

/**
 * heci_cl_flow_ctrl_reduce - reduces flow_control.
 *
 * @dev: the device structure
 * @cl: private data of the file object
 * @returns
 *	0 on success
 *	-ENOENT when me client is not found
 *	-EINVAL when ctrl credits are <= 0
 */
int heci_cl_flow_ctrl_reduce(struct heci_cl *cl)
{
	struct heci_device *dev;
	struct heci_me_client  *me_cl = cl->device->fw_client;

	if (WARN_ON(!cl || !cl->dev))
		return -EINVAL;

	dev = cl->dev;

	if (!dev->me_clients_num)
		return -ENOENT;

	if (me_cl->props.single_recv_buf != 0) { /* fixed client: single_recv_buf != 0*/
		if (WARN_ON(me_cl->heci_flow_ctrl_creds <= 0))
			return -EINVAL;
		me_cl->heci_flow_ctrl_creds--;
	} else {
		if (WARN_ON(cl->heci_flow_ctrl_creds <= 0))
			return -EINVAL;
		cl->heci_flow_ctrl_creds--;
	}
	return 0;
}

/**
 * heci_cl_read_start - the start read client message function.
 *
 * @cl: host client
 *
 * returns 0 on success, <0 on failure.
 */
int heci_cl_read_start(struct heci_cl *cl)
{
	struct heci_device *dev;
	struct heci_cl_rb *rb;
	int rets;
	int i;
	unsigned long	flags;
	unsigned long	dev_flags;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (cl->state != HECI_CL_CONNECTED)
		return -ENODEV;

	if (dev->dev_state != HECI_DEV_ENABLED)
		return -ENODEV;

/*
	if (cl->read_rb) {
		dev_dbg(&dev->pdev->dev, "read is pending.\n");
		return -EBUSY;
	}
*/
	i = heci_me_cl_by_id(dev, cl->me_client_id);
	if (i < 0) {
		dev_err(&dev->pdev->dev, "no such me client %d\n",
			cl->me_client_id);
		return  -ENODEV;
	}

	rets = pm_runtime_get_sync(&dev->pdev->dev);
	dev_dbg(&dev->pdev->dev, "rpm: get sync %d\n", rets);
	if (IS_ERR_VALUE(rets)) {
		dev_err(&dev->pdev->dev, "rpm: get sync failed %d\n", rets);
		return rets;
	}

	/* The current rb is the head of the free rb list */
	spin_lock_irqsave(&cl->free_list_spinlock, flags);
	if (list_empty(&cl->free_rb_list.list)) {
		printk(KERN_ERR "[heci-ish] error: rb pool is empty\n");
		rets = -1;
		rb = NULL;
		spin_unlock_irqrestore(&cl->free_list_spinlock, flags);
		goto out;
	}
	rb = list_entry(cl->free_rb_list.list.next, struct heci_cl_rb, list);
	list_del_init(&rb->list);
	spin_unlock_irqrestore(&cl->free_list_spinlock, flags);
	/***************/
	rb->cl = cl;
	rb->buf_idx = 0;
	/***************/

	heci_io_list_init(rb);
	rets = 0;

	/*cl->read_rb = rb;*/

	/* This must be BEFORE sending flow control - response in ISR may come too fast... */
	spin_lock_irqsave(&dev->read_list_spinlock, dev_flags);
	list_add_tail(&rb->list, &dev->read_list.list);
	spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
	if (heci_hbm_cl_flow_control_req(dev, cl)) {
		rets = -ENODEV;
		goto out;
	}

out:
	dev_dbg(&dev->pdev->dev, "rpm: autosuspend\n");
	pm_runtime_mark_last_busy(&dev->pdev->dev);
	pm_runtime_put_autosuspend(&dev->pdev->dev);

	if (rets)
		heci_io_rb_free(rb);

	return rets;
}
EXPORT_SYMBOL(heci_cl_read_start);


int heci_cl_send(struct heci_cl *cl, u8 *buf, size_t length)
{
	struct heci_device *dev;
	int id;
	struct heci_cl_tx_ring  *cl_msg;
	int	have_msg_to_send = 0;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	dev->print_log(dev, KERN_ALERT "%s(): cl && cl->dev OK\n", __func__);

	if (cl->state != HECI_CL_CONNECTED) {
		++cl->err_send_msg;
		return -EPIPE;
	}
	dev->print_log(dev, KERN_ALERT "%s(): cl->state is HECI_CL_CONNECTED\n", __func__);

	if (dev->dev_state != HECI_DEV_ENABLED) {
		++cl->err_send_msg;
		return -ENODEV;
	}
	dev->print_log(dev, KERN_ALERT "%s(): dev->dev_state is HECI_DEV_ENABLED\n", __func__);

	/* Check if we have an ME client device */
	id = heci_me_cl_by_id(dev, cl->me_client_id);
	if (id < 0) {
		++cl->err_send_msg;
		return -ENOENT;
	}
	dev->print_log(dev, KERN_ALERT "%s(): have ME client device, id=%d\n", __func__, id);

	if (length > dev->me_clients[id].props.max_msg_length) {
		/* If the client supports DMA, try to use it */
		if (host_dma_enabled && dev->me_clients[id].props.dma_hdr_len & HECI_CLIENT_DMA_ENABLED) {
			struct heci_msg_hdr	hdr;
			struct hbm_client_dma_request	heci_dma_request_msg;
			unsigned	len = sizeof(struct hbm_client_dma_request);
			int	preview_len = dev->me_clients[id].props.dma_hdr_len & 0x7F;

			/* DMA max msg size is 1M */
			if (length > host_dma_buf_size) {
				++cl->err_send_msg;
				return	-EMSGSIZE;
			}

			/* Client for some reason specified props.dma_hdr_len > 12, mistake? */
			if (preview_len > 12) {
				++cl->err_send_msg;
				return	-EINVAL;
			}

			/* If previous DMA transfer is in progress, go to sleep */
			wait_event(dev->wait_dma_ready, dma_ready);
			dma_ready = 0;
			memcpy(host_dma_buf, buf + preview_len, length - preview_len);	/* First 'preview_len' bytes of buffer are preview bytes, omitted from DMA message */
			heci_hbm_hdr(&hdr, len);
			heci_dma_request_msg.hbm_cmd = CLIENT_DMA_REQ_CMD;
			heci_dma_request_msg.me_addr = cl->me_client_id;
			heci_dma_request_msg.host_addr = cl->host_client_id;
			heci_dma_request_msg.reserved = 0;
			heci_dma_request_msg.msg_addr = host_dma_buf_phys;
			heci_dma_request_msg.msg_len = length - preview_len;
			heci_dma_request_msg.reserved2 = 0;
			memcpy(heci_dma_request_msg.msg_preview, buf, preview_len);
			heci_write_message(dev, &hdr, (uint8_t *)&heci_dma_request_msg);
		} else {
			++cl->err_send_msg;
			return -EINVAL;		/* -EMSGSIZE? */
		}
	}

	/* No free bufs */
	if (list_empty(&cl->tx_free_list.list)) {
		++cl->err_send_msg;
		return	-ENOMEM;
	}
	dev->print_log(dev, KERN_ALERT "%s(): have client TX free bufs\n", __func__);

	cl_msg = list_first_entry(&cl->tx_free_list.list, struct heci_cl_tx_ring, list);
	if (!cl_msg->send_buf.data) {
		return	-EIO;		/* Should not happen, as free list is pre-allocated */
	}
	dev->print_log(dev, KERN_ALERT "%s(): have send_buf.data OK\n", __func__);

	/* This is safe, as 'length' is already checked for not exceeding max. HECI message size per client */
	list_del_init(&cl_msg->list);
	memcpy(cl_msg->send_buf.data, buf, length);
	cl_msg->send_buf.size = length;
	have_msg_to_send = !list_empty(&cl->tx_list.list);
	list_add_tail(&cl_msg->list, &cl->tx_list.list);
	dev->print_log(dev, KERN_ALERT "%s(): have send_buf.data OK. have_msg_to_send=%d FC creds=%d\n", __func__, have_msg_to_send, (int)heci_cl_flow_ctrl_creds(cl));

	if (!have_msg_to_send &&  heci_cl_flow_ctrl_creds(cl) > 0)
		heci_cl_send_msg(dev, cl);	

	return	0;
}
EXPORT_SYMBOL(heci_cl_send);


/**
 * heci_cl_read_complete - processes completed operation for a client
 *
 * @cl: private data of the file object.
 * @rb: callback block.
 */
void heci_cl_read_complete(struct heci_cl_rb *rb)
{
	unsigned long	flags;
	int	schedule_work_flag = 0;
	struct heci_cl	*cl = rb->cl;

	if (waitqueue_active(&cl->rx_wait)) {
		cl->read_rb = rb;
		wake_up_interruptible(&cl->rx_wait);
	} else {
		spin_lock_irqsave(&cl->in_process_spinlock, flags);
		schedule_work_flag = list_empty(&cl->in_process_list.list);/*if in-process list is empty, then need to schedule the processing thread*/
		list_add_tail(&rb->list, &cl->in_process_list.list);
		spin_unlock_irqrestore(&cl->in_process_spinlock, flags);

		if (schedule_work_flag)
			heci_cl_bus_rx_event(cl->device);
	}
}
EXPORT_SYMBOL(heci_cl_read_complete);


/**
 * heci_cl_all_disconnect - disconnect forcefully all connected clients
 *
 * @dev - heci device
 */
void heci_cl_all_disconnect(struct heci_device *dev)
{
	struct heci_cl *cl, *next;
	unsigned long	flags;

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
		cl->state = HECI_CL_DISCONNECTED;
		cl->heci_flow_ctrl_creds = 0;
		cl->read_rb = NULL;
	}
	spin_unlock_irqrestore(&dev->device_lock, flags);
}


/**
 * heci_cl_all_read_wakeup  - wake up all readings so they can be interrupted
 *
 * @dev  - heci device
 */
void heci_cl_all_read_wakeup(struct heci_device *dev)
{
	struct heci_cl *cl, *next;
	unsigned long	flags;

	spin_lock_irqsave(&dev->device_lock, flags);
	list_for_each_entry_safe(cl, next, &dev->cl_list, link) {
		if (waitqueue_active(&cl->rx_wait)) {
			dev_dbg(&dev->pdev->dev, "Waking up client!\n");
			wake_up_interruptible(&cl->rx_wait);
		}
	}
	spin_unlock_irqrestore(&dev->device_lock, flags);
}

/*##################################*/

static void	ipc_tx_callback(void *prm)
{
	struct heci_cl	*cl = prm;
	struct heci_cl_tx_ring	*cl_msg;
	size_t	rem;
	struct heci_device	*dev = (cl ? cl->dev : NULL);
	struct heci_msg_hdr	heci_hdr;

	dev->print_log(dev, KERN_ALERT "%s() +++\n", __func__);
	if (!dev)
		return;
	dev->print_log(dev, KERN_ALERT "%s() dev OK\n", __func__);

	/* FIXME: there may be other conditions if some critical error has ocurred before this callback is called */
	if (list_empty(&cl->tx_list.list))
		return;
	dev->print_log(dev, KERN_ALERT "%s() TX list is not empty, sending\n", __func__);

	cl_msg = list_entry(cl->tx_list.list.next, struct heci_cl_tx_ring, list);
	rem = cl_msg->send_buf.size - cl->tx_offs;

	heci_hdr.host_addr = cl->host_client_id;
	heci_hdr.me_addr = cl->me_client_id;
	heci_hdr.reserved = 0;

	if (rem <= dev->mtu) {
		dev->print_log(dev, KERN_ALERT "%s() message complete\n", __func__);
		heci_hdr.length = rem;
		heci_hdr.msg_complete = 1;
		heci_write_message(dev, &heci_hdr, cl_msg->send_buf.data);	/* Submit to IPC queue with no callback */
		--cl->heci_flow_ctrl_creds;
		list_del_init(&cl_msg->list);
		list_add_tail(&cl_msg->list, &cl->tx_free_list.list);
	} else {
		/* Send IPC fragment */
		dev->print_log(dev, KERN_ALERT "%s() message fragmented, sending fragment\n", __func__);
		cl->tx_offs += dev->mtu;
		heci_hdr.length = dev->mtu;
		heci_hdr.msg_complete = 0;
		dev->ops->write_ex(dev, &heci_hdr, cl_msg->send_buf.data, ipc_tx_callback, cl);
	}
}


void heci_cl_send_msg(struct heci_device *dev, struct heci_cl *cl)
{
	cl->tx_offs = 0;
	ipc_tx_callback(cl);	
}
EXPORT_SYMBOL(heci_cl_send_msg);
/*##################################*/


/*
 *	Receive and dispatch HECI client messages
 *
 *	(!) ISR context
 */
void	recv_heci_cl_msg(struct heci_device *dev, struct heci_msg_hdr *heci_hdr)
{
	struct heci_cl *cl;
	struct heci_cl_rb *rb, *next;
	unsigned char *buffer = NULL;
	struct heci_cl_rb *complete_rb = NULL;
	unsigned long	dev_flags;
	unsigned long	flags;

dev->print_log(dev, "%s(): +++\n", __func__);

	if (heci_hdr->reserved) {
		dev_err(&dev->pdev->dev, "corrupted message header.\n");
		goto	eoi;
	}
dev->print_log(dev, "%s(): msg header OK\n", __func__);

	if (heci_hdr->length > IPC_PAYLOAD_SIZE) {
		dev_err(&dev->pdev->dev, "HECI message length in hdr is too big for IPC MTU. Broken message\n");
		goto	eoi;
	}
dev->print_log(dev, "%s(): msg length OK\n", __func__);

	spin_lock_irqsave(&dev->read_list_spinlock, dev_flags);
	list_for_each_entry_safe(rb, next, &dev->read_list.list, list) {
		cl = rb->cl;
		if (!cl || !(cl->host_client_id == heci_hdr->host_addr && cl->me_client_id == heci_hdr->me_addr) ||
				!(cl->state == HECI_CL_CONNECTED))
			continue;

		/* FIXME: in both if() closes rb must return to free pool and/or disband and/or disconnect client */
		if (rb->buffer.size == 0 || rb->buffer.data == NULL) {
			spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
			dev_err(&dev->pdev->dev, "response buffer is not allocated.\n");
			list_del(&rb->list);
			goto	eoi;
		}

		if (rb->buffer.size < heci_hdr->length + rb->buf_idx) {
			spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
			dev_err(&dev->pdev->dev, "message overflow. size %d len %d idx %ld\n", rb->buffer.size, heci_hdr->length, rb->buf_idx);
			list_del(&rb->list);
			goto	eoi;
		}

		buffer = rb->buffer.data + rb->buf_idx;
		dev->ops->read(dev, buffer, heci_hdr->length);

		rb->buf_idx += heci_hdr->length;
		if (heci_hdr->msg_complete) {
			cl->status = 0;
			list_del(&rb->list);
			complete_rb = rb;

			--cl->out_flow_ctrl_creds;
dev->print_log(dev,"%s(): rb[%p] content %02X %02X %02X %02X\n", __func__, rb, rb->buffer.data[0], rb->buffer.data[1], rb->buffer.data[2], rb->buffer.data[3]);
			/* the whole msg arrived, send a new FC, and add a new rb buffer for the next coming msg */
			spin_lock_irqsave(&cl->free_list_spinlock, flags);

			if (!list_empty(&cl->free_rb_list.list)) {
				rb = list_entry(cl->free_rb_list.list.next, struct heci_cl_rb, list);
				list_del_init(&rb->list);
				spin_unlock_irqrestore(&cl->free_list_spinlock, flags);
				rb->cl = cl;
				rb->buf_idx = 0;
				INIT_LIST_HEAD(&rb->list);
dev->print_log(dev, "%s(): add cd = %p to read_list\n", __func__, rb);
				list_add_tail(&rb->list, &dev->read_list.list);
dev->print_log(dev, "%s(): call heci_hbm_cl_flow_control_req\n", __func__);

				/* will work properly just after the transmit path fixes (which sends hbm command without sleeping) */
				heci_hbm_cl_flow_control_req(dev, cl); 
			} else {
				/*cl->send_fc_flag = 1;*/
				spin_unlock_irqrestore(&cl->free_list_spinlock, flags);
			}
		}

		/* We can safely break here (and in BH too), a single input message can go only to a single request! */
		break;
	}
dev->print_log(dev, "%s(): buffer=%p complete_rb=%p\n", __func__, buffer, complete_rb);

	spin_unlock_irqrestore(&dev->read_list_spinlock, dev_flags);
	/* If it's nobody's message, just read and discard it */
	if (!buffer) {
		uint8_t	rd_msg_buf[HECI_RD_MSG_BUF_SIZE];

		dev->ops->read(dev, rd_msg_buf, heci_hdr->length);
		goto	eoi;
	}

	/* Looks like this is interrupt-safe */
	if (complete_rb)
		heci_cl_read_complete(complete_rb);

eoi:
dev->print_log(dev, "%s(): ---\n", __func__);
	return;
}
EXPORT_SYMBOL(recv_heci_cl_msg);

