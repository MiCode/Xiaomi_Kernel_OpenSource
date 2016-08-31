/*
 *   GPS Char Driver for Texas Instrument's Connectivity Chip.
 *   Copyright (C) 2009 Texas Instruments
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

#include <linux/uaccess.h>
#include <linux/tty.h>
#include <linux/sched.h>

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>

#include <linux/ti_wilink_st.h>

#undef VERBOSE
#undef DEBUG

/* Debug macros*/
#if defined(DEBUG)		/* limited debug messages */
#define GPSDRV_DBG(fmt, arg...)  \
	printk(KERN_INFO "[GPS] (gpsdrv):"fmt"\n" , ## arg)
#define GPSDRV_VER(fmt, arg...)
#elif defined(VERBOSE)		/* very verbose */
#define GPSDRV_DBG(fmt, arg...)  \
	printk(KERN_INFO "[GPS] (gpsdrv):"fmt"\n" , ## arg)
#define GPSDRV_VER(fmt, arg...)  \
	printk(KERN_INFO "[GPS] (gpsdrv):"fmt"\n" , ## arg)
#define GPSDRV_ERR(fmt, arg...)  \
	printk(KERN_ERR "[GPS] (gpsdrv):"fmt"\n" , ## arg)
#else /* Error msgs only */
#define GPSDRV_ERR(fmt, arg...)  \
	printk(KERN_ERR "[GPS] (gpsdrv):"fmt"\n" , ## arg)
#define GPSDRV_VER(fmt, arg...)
#define GPSDRV_DBG(fmt, arg...)
#endif

static void gpsdrv_tsklet_write(unsigned long data);

/* List of error codes returned by the gps driver*/
enum {
	GPS_ERR_FAILURE = -1,	/* check struct */
	GPS_SUCCESS,
	GPS_ERR_CLASS = -15,
	GPS_ERR_CPY_TO_USR,
	GPS_ERR_CPY_FRM_USR,
	GPS_ERR_UNKNOWN,
};

/* Channel-9 details for GPS */
#define GPS_CH9_PKT_HDR_SIZE		4
#define GPS_CH9_PKT_NUMBER		0x9
#define GPS_CH9_OP_WRITE		0x1
#define GPS_CH9_OP_READ			0x2
#define GPS_CH9_OP_COMPLETED_EVT	0x3

/* Macros for Syncronising GPS registration and other R/W/ICTL operations */
#define GPS_ST_REGISTERED	0
#define GPS_ST_RUNNING		1

/* Read time out defined to 10 seconds */
#define GPSDRV_READ_TIMEOUT	10000
/* Reg time out defined to 6 seconds */
#define GPSDRV_REG_TIMEOUT	6000


struct gpsdrv_event_hdr {
	uint8_t opcode;
	uint16_t plen;
} __packed;

/*
 * struct gpsdrv_data - gps internal driver data
 * @gpsdrv_reg_completed - completion to wait for registration
 * @streg_cbdata - registration feedback
 * @state - driver state
 * @tx_count - TX throttling/unthrottling
 * @st_write - write ptr from ST
 * @rx_list - Rx data SKB queue
 * @tx_list - Tx data SKB queue
 * @gpsdrv_data_q - dataq checked up on poll/receive
 * @lock - spin lock
 * @gpsdrv_tx_tsklet - gps write task
 */

struct gpsdrv_data {
	struct completion gpsdrv_reg_completed;
	char streg_cbdata;
	unsigned long state;
	unsigned char tx_count;
	long (*st_write) (struct sk_buff *skb);
	struct sk_buff_head rx_list;
	struct sk_buff_head tx_list;
	wait_queue_head_t gpsdrv_data_q;
	spinlock_t lock;
	struct tasklet_struct gpsdrv_tx_tsklet;
};

#define DEVICE_NAME     "tigps"

/***********Functions called from ST driver**********************************/

/*  gpsdrv_st_recv Function
 *  This is Called in from -- ST Core when a data is received
 *  This is a registered callback with ST core when the gps driver registers
 *  with ST.
 *
 *  Parameters:
 *  @skb    : SKB buffer pointer which contains the incoming Ch-9 GPS data.
 *  Returns:
 *          GPS_SUCCESS - On Success
 *          else suitable error code
 */
long gpsdrv_st_recv(void *arg, struct sk_buff *skb)
{
	struct gpsdrv_event_hdr gpsdrv_hdr = { 0x00, 0x0000 };
	struct gpsdrv_data *hgps = (struct gpsdrv_data *)arg;

	/* SKB is NULL */
	if (NULL == skb) {
		GPSDRV_ERR("Input SKB is NULL");
		return GPS_ERR_FAILURE;
	}

	/* Sanity Check - To Check if the Rx Pkt is Channel -9 or not */
	if (0x09 != skb->cb[0]) {
		GPSDRV_ERR("Input SKB is not a Channel-9 packet");
		return GPS_ERR_FAILURE;
	}
	/* Copy Ch-9 info to local structure */
	memcpy(&gpsdrv_hdr, skb->data, GPS_CH9_PKT_HDR_SIZE - 1);
	skb_pull(skb, GPS_CH9_PKT_HDR_SIZE - 1);

	/* check if skb->len and gpsdrv_hdr.plen are equal */
	if (skb->len != gpsdrv_hdr.plen) {
		GPSDRV_ERR("Received corrupted packet - Length Mismatch");
		return -EINVAL;
	}
#ifdef VERBOSE
	printk(KERN_INFO"data start >>\n");
	print_hex_dump(KERN_INFO, ">in>", DUMP_PREFIX_NONE,
			16, 1, skb->data, skb->len, 0);
	printk(KERN_INFO"\n<< end\n");
#endif
	/* Check the Opcode */
	if ((gpsdrv_hdr.opcode != GPS_CH9_OP_READ) && (gpsdrv_hdr.opcode != \
				GPS_CH9_OP_COMPLETED_EVT)) {
		GPSDRV_ERR("Rec corrupt pkt opcode %x", gpsdrv_hdr.opcode);
		return -EINVAL;
	}

	/* Strip Channel 9 packet information from SKB only
	 * if the opcode is GPS_CH9_OP_READ and get AI2 packet
	 */
	if (GPS_CH9_OP_READ == gpsdrv_hdr.opcode) {
		skb_queue_tail(&hgps->rx_list, skb);
		wake_up_interruptible(&hgps->gpsdrv_data_q);
	} else {
		spin_lock(&hgps->lock);
		/* The no. of Completed Packets is always 1
		 * in case of Channel 9 as per spec.
		 * Forcing it to 1 for precaution.
		 */
		hgps->tx_count = 1;
		/* Check if Tx queue and Tx count not empty */
		if (!skb_queue_empty(&hgps->tx_list)) {
			/* Schedule the Tx-task let */
			spin_unlock(&hgps->lock);
			GPSDRV_VER(" Scheduling tasklet to write");
			tasklet_schedule(&hgps->gpsdrv_tx_tsklet);
		} else {
			spin_unlock(&hgps->lock);
		}

		GPSDRV_VER(" Tx count = %x", hgps->tx_count);
		/* Free the received command complete SKB */
		kfree_skb(skb);
	}

	return GPS_SUCCESS;

}

/*  gpsdrv_st_cb Function
 *  This is Called in from -- ST Core when the state is pending during
 *  st_register. This is a registered callback with ST core when the gps
 *  driver registers with ST.
 *
 *  Parameters:
 *  @data   Status update of GPS registration
 *  Returns: NULL
 */
void gpsdrv_st_cb(void *arg, char data)
{
	struct gpsdrv_data *hgps = (struct gpsdrv_data *)arg;

	GPSDRV_DBG(" Inside %s", __func__);
	hgps->streg_cbdata = data;	/* ST registration callback  status */
	complete_all(&hgps->gpsdrv_reg_completed);
	return;
}

static struct st_proto_s gpsdrv_proto = {
	.chnl_id = 0x09,
	.max_frame_size = 1024,
	.hdr_len = 3,
	.offset_len_in_hdr = 1,
	.len_size = 2,
	.reserve = 1,
	.recv = gpsdrv_st_recv,
	.reg_complete_cb = gpsdrv_st_cb,
};

/** gpsdrv_tsklet_write Function
 *  This tasklet function will be scheduled when there is a data in Tx queue
 *  and GPS chip sent an command completion packet with non zero value.
 *
 *  Parameters :
 *  @data  : data passed to tasklet function
 *  Returns : NULL
 */
void gpsdrv_tsklet_write(unsigned long data)
{
	struct sk_buff *skb = NULL;
	struct gpsdrv_data *hgps = (struct gpsdrv_data *)data;

	GPSDRV_DBG(" Inside %s", __func__);

	spin_lock(&hgps->lock);

	/* Perform sanity check of verifying the status
	   to perform an st_write */
	if (((!hgps->st_write) || (0 == hgps->tx_count))
			|| ((skb_queue_empty(&hgps->tx_list)))) {
		spin_unlock(&hgps->lock);
		GPSDRV_ERR("Sanity check Failed exiting %s", __func__);
		return;
	}
	/* hgps->tx_list not empty skb already present
	 * dequeue the tx-data and perform a st_write
	 */
	hgps->tx_count--;
	spin_unlock(&hgps->lock);
	GPSDRV_VER(" Tx count in gpsdrv_tsklet_write = %x", hgps->tx_count);
	skb = skb_dequeue(&hgps->tx_list);
	hgps->st_write(skb);

	return;
}

/*********Functions Called from GPS host***************************************/

/** gpsdrv_open Function
 *  This function will perform an register on ST driver.
 *
 *  Parameters :
 *  @file  : File pointer for GPS char driver
 *  @inod  :
 *  Returns  GPS_SUCCESS -  on success
 *           else suitable error code
 */
int gpsdrv_open(struct inode *inod, struct file *file)
{
	int ret = 0;
	unsigned long timeout = GPSDRV_REG_TIMEOUT;
	struct gpsdrv_data *hgps;

	GPSDRV_DBG(" Inside %s", __func__);

	/* Allocate local resource memory */
	hgps = kzalloc(sizeof(struct gpsdrv_data), GFP_KERNEL);
	if (!(hgps)) {
		GPSDRV_ERR("Can't allocate GPS data structure");
		return -ENOMEM;
	}

	/* Initialize wait queue, skb queue head and
	 * registration complete strucuture
	 */
	skb_queue_head_init(&hgps->rx_list);
	skb_queue_head_init(&hgps->tx_list);
	init_completion(&hgps->gpsdrv_reg_completed);
	init_waitqueue_head(&hgps->gpsdrv_data_q);
	spin_lock_init(&hgps->lock);

	/* Check if GPS is already registered with ST */
	if (test_and_set_bit(GPS_ST_REGISTERED, &hgps->state)) {
		GPSDRV_ERR("GPS Registered/Registration in progress with ST"
				" ,open called again?");
		kfree(hgps);
		return -EAGAIN;
	}

	/* Initialize  gpsdrv_reg_completed so as to wait for completion
	 * on the same
	 * if st_register returns with a PENDING status
	 */
	INIT_COMPLETION(hgps->gpsdrv_reg_completed);

	gpsdrv_proto.priv_data = hgps;
	/* Resgister GPS with ST */
	ret = st_register(&gpsdrv_proto);
	GPSDRV_VER(" st_register returned %d", ret);

	/* If GPS Registration returned with error, then clear GPS_ST_REGISTERED
	 * for future open calls and return the appropriate error code
	 */
	if (ret < 0 && ret != -EINPROGRESS) {
		GPSDRV_ERR(" st_register failed");
		clear_bit(GPS_ST_REGISTERED, &hgps->state);
		if (ret == -EINPROGRESS)
			return -EAGAIN;
		return GPS_ERR_FAILURE;
	}

	/* if returned status is pending, wait for the completion */
	if (ret == -EINPROGRESS) {
		GPSDRV_VER(" GPS Register waiting for completion ");
		timeout = wait_for_completion_timeout \
		(&hgps->gpsdrv_reg_completed, msecs_to_jiffies(timeout));
		/* Check for timed out condition */
		if (0 == timeout) {
			GPSDRV_ERR("GPS Device registration timed out");
			clear_bit(GPS_ST_REGISTERED, &hgps->state);
			return -ETIMEDOUT;
		} else if (0 > hgps->streg_cbdata) {
			GPSDRV_ERR("GPS Device Registration Failed-ST\n");
			GPSDRV_ERR("RegCB called with ");
			GPSDRV_ERR("Invalid value %d\n", hgps->streg_cbdata);
			clear_bit(GPS_ST_REGISTERED, &hgps->state);
			return -EAGAIN;
		}
	}
	GPSDRV_DBG(" gps registration complete ");

	/* Assign the write callback pointer */
	hgps->st_write = gpsdrv_proto.write;
	hgps->tx_count = 1;
	file->private_data = hgps;	/* set drv data */
	tasklet_init(&hgps->gpsdrv_tx_tsklet, (void *)gpsdrv_tsklet_write,
			(unsigned long)hgps);
	set_bit(GPS_ST_RUNNING, &hgps->state);

	return GPS_SUCCESS;
}

/** gpsdrv_release Function
 *  This function will un-registers from the ST driver.
 *
 *  Parameters :
 *  @file  : File pointer for GPS char driver
 *  @inod  :
 *  Returns  GPS_SUCCESS -  on success
 *           else suitable error code
 */
int gpsdrv_release(struct inode *inod, struct file *file)
{
	struct gpsdrv_data *hgps = file->private_data;

	GPSDRV_DBG(" Inside %s", __func__);

	/* Disabling task-let 1st & then un-reg to avoid
	 * tasklet getting scheduled
	 */
	tasklet_disable(&hgps->gpsdrv_tx_tsklet);
	tasklet_kill(&hgps->gpsdrv_tx_tsklet);
	/* Cleat registered bit if already registered */
	if (test_and_clear_bit(GPS_ST_REGISTERED, &hgps->state)) {
		if (st_unregister(&gpsdrv_proto) < 0) {
			GPSDRV_ERR(" st_unregister failed");
			/* Re-Enable the task-let if un-register fails */
			tasklet_enable(&hgps->gpsdrv_tx_tsklet);
			return GPS_ERR_FAILURE;
		}
	}

	/* Reset Tx count value and st_write function pointer */
	hgps->tx_count = 0;
	hgps->st_write = NULL;
	clear_bit(GPS_ST_RUNNING, &hgps->state);
	GPSDRV_VER(" st_unregister success");

	skb_queue_purge(&hgps->rx_list);
	skb_queue_purge(&hgps->tx_list);
	kfree(hgps);
	file->private_data = NULL;

	return GPS_SUCCESS;
}

/** gpsdrv_read Function
 *  This function will wait till the data received from the ST driver
 *  and then strips the GPS-Channel-9 header from the
 *  incoming AI2 packet and then send it to GPS host application.
 *
 *  Parameters :
 *  @file  : File pointer for GPS char driver
 *  @data  : Data which needs to be passed to APP
 *  @size  : Length of the data passesd
 *  offset :
 *  Returns  Size of AI2 packet received -  on success
 *           else suitable error code
 */
ssize_t gpsdrv_read(struct file *file, char __user *data, size_t size,
		loff_t *offset)
{
	int len = 0, error = 0;
	struct sk_buff *skb = NULL;
	unsigned long timeout = GPSDRV_READ_TIMEOUT;
	struct gpsdrv_data *hgps;

	GPSDRV_DBG(" Inside %s", __func__);

	/* Validate input parameters */
	if ((NULL == file) || (((NULL == data) || (0 == size)))) {
		return -EINVAL;
	}

	hgps = file->private_data;
	/* Check if GPS is registered to perform read operation */
	if (!test_bit(GPS_ST_RUNNING, &hgps->state)) {
		GPSDRV_ERR("GPS Device is not running");
		return -EINVAL;
	}

	/* cannot come here if poll-ed before reading
	 * if not poll-ed wait on the same wait_q
	 */
	timeout = wait_event_interruptible_timeout(hgps->gpsdrv_data_q,
		!skb_queue_empty(&hgps->rx_list), msecs_to_jiffies(timeout));
	/* Check for timed out condition */
	if (0 == timeout) {
		GPSDRV_ERR("GPS Device Read timed out");
		return -ETIMEDOUT;
	}


	/* hgps->rx_list not empty skb already present */
	skb = skb_dequeue(&hgps->rx_list);

	if (!skb) {
		GPSDRV_ERR("Dequed SKB is NULL?");
		return GPS_ERR_UNKNOWN;
	} else if (skb->len > size) {
		GPSDRV_DBG("SKB length is Greater than requested size ");
		GPSDRV_DBG("Returning the available length of SKB\n");

		error = copy_to_user(data, skb->data, size);
		skb_pull(skb, size);

		if (skb->len != 0)
			skb_queue_head(&hgps->rx_list, skb);

		/* printk(KERN_DEBUG  "gpsdrv: total size read= %d", size);*/
		return size;
	}

#ifdef VERBOSE
	print_hex_dump(KERN_INFO, ">in>", DUMP_PREFIX_NONE,
			16, 1, skb->data, skb->len, 0);
#endif

	/* Forward the data to the user */
	if (skb->len <= size) {
		if (copy_to_user(data, skb->data, skb->len)) {
			GPSDRV_ERR(" Unable to copy to user space");
			/* Queue the skb back to head */
			skb_queue_head(&hgps->rx_list, skb);
			return GPS_ERR_CPY_TO_USR;
		}
	}

	len = skb->len;
	kfree_skb(skb);
	/* printk(KERN_DEBUG  "gpsdrv: total size read= %d", len); */
	return len;
}
/* gpsdrv_write Function
 *  This function will pre-pend the GPS-Channel-9 header to the
 *  incoming AI2 packet sent from the GPS host application.
 *
 *  Parameters :
 *  @file   : File pointer for GPS char driver
 *  @data   : AI2 packet data from GPS application
 *  @size   : Size of the AI2 packet data
 *  @offset :
 *  Returns  Size of AI2 packet on success
 *           else suitable error code
 */
ssize_t gpsdrv_write(struct file *file, const char __user *data,
		size_t size, loff_t *offset)
{
	unsigned char channel = GPS_CH9_PKT_NUMBER; /* GPS Channel number */
	/* Initialize gpsdrv_event_hdr with write opcode */
	struct gpsdrv_event_hdr gpsdrv_hdr = { GPS_CH9_OP_WRITE, 0x0000 };
	struct sk_buff *skb = NULL;
	struct gpsdrv_data *hgps;

	GPSDRV_DBG(" Inside %s", __func__);
	/* Validate input parameters */
	if ((NULL == file) || (((NULL == data) || (0 == size)))) {
		GPSDRV_ERR("Invalid input params passed to %s", __func__);
		return -EINVAL;
	}

	hgps = file->private_data;

	/* Check if GPS is registered to perform write operation */
	if (!test_bit(GPS_ST_RUNNING, &hgps->state)) {
		GPSDRV_ERR("GPS Device is not running");
		return -EINVAL;
	}

	if (!hgps->st_write) {
		GPSDRV_ERR(" Can't write to ST, hgps->st_write null ?");
		return -EINVAL;
	}


	skb = alloc_skb(size + GPS_CH9_PKT_HDR_SIZE, GFP_ATOMIC);
	/* Validate Created SKB */
	if (NULL == skb) {
		GPSDRV_ERR("Error aaloacting SKB");
		return -ENOMEM;
	}

	/* Update chnl-9 information with plen=AI2 pckt size which is "size"*/
	gpsdrv_hdr.plen = size;

	/* PrePend Channel-9 header to AI2 packet and write to SKB */
	memcpy(skb_put(skb, 1), &channel, 1);
	memcpy(skb_put(skb, GPS_CH9_PKT_HDR_SIZE - 1), &gpsdrv_hdr,
			GPS_CH9_PKT_HDR_SIZE - 1);

	/* Forward the data from the user space to ST core */
	if (copy_from_user(skb_put(skb, size), data, size)) {
		GPSDRV_ERR(" Unable to copy from user space");
		kfree_skb(skb);
		return GPS_ERR_CPY_FRM_USR;
	}

#ifdef VERBOSE
	GPSDRV_VER("start data..");
	print_hex_dump(KERN_INFO, "<out<", DUMP_PREFIX_NONE,
			16, 1, skb->data, size, 0);
	GPSDRV_VER("\n..end data");
#endif

	/* Check if data can be sent to GPS chip
	 * If not, add it to queue and that can be sent later
	 */
	spin_lock(&hgps->lock);
	if (0 != hgps->tx_count) {
		/* If TX Q is empty send current SKB;
		 *  else, queue current SKB at end of tx_list queue and
		 *  send first SKB in tx_list queue.
		 */
		hgps->tx_count--;
		spin_unlock(&hgps->lock);

		GPSDRV_VER(" Tx count in gpsdrv_write = %x", hgps->tx_count);

		if (skb_queue_empty(&hgps->tx_list)) {
			hgps->st_write(skb);
		} else {
			skb_queue_tail(&hgps->tx_list, skb);
			hgps->st_write(skb_dequeue(&hgps->tx_list));
		}
	} else {
		/* Add it to TX queue */
		spin_unlock(&hgps->lock);
		GPSDRV_VER(" SKB added to Tx queue ");
		GPSDRV_VER("Tx count = %x ", hgps->tx_count);
		skb_queue_tail(&hgps->tx_list, skb);
		/* This is added for precaution in the case that there is a
		 * context switch during the execution of the above lines.
		 * Redundant otherwise.
		 */
		if ((0 != hgps->tx_count) && \
				(!skb_queue_empty(&hgps->tx_list))) {
			/* Schedule the Tx-task let */
			GPSDRV_VER("Scheduling tasklet to write\n");
			tasklet_schedule(&hgps->gpsdrv_tx_tsklet);
		}
	}

	return size;
}

/** gpsdrv_ioctl Function
 *  This will peform the functions as directed by the command and command
 *  argument.
 *
 *  Parameters :
 *  @file  : File pointer for GPS char driver
 *  @cmd   : IOCTL Command
 *  @arg   : Command argument for IOCTL command
 *  Returns  GPS_SUCCESS on success
 *           else suitable error code
 */
static long gpsdrv_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct sk_buff *skb = NULL;
	int		retCode = GPS_SUCCESS;
	struct gpsdrv_data *hgps;

	GPSDRV_DBG(" Inside %s", __func__);

	/* Validate input parameters */
	if ((NULL == file) || (0 == cmd)) {
		GPSDRV_ERR("Invalid input parameters passed to %s", __func__);
		return -EINVAL;
	}

	hgps = file->private_data;

	/* Check if GPS is registered to perform IOCTL operation */
	if (!test_bit(GPS_ST_RUNNING, &hgps->state)) {
		GPSDRV_ERR("GPS Device is not running");
		return -EINVAL;
	}

	switch (cmd) {
	case TCFLSH:
		GPSDRV_VER(" IOCTL TCFLSH invoked with %ld argument", arg);
		switch (arg) {
			/* purge Rx/Tx SKB list queues depending on arg value */
		case TCIFLUSH:
			skb_queue_purge(&hgps->rx_list);
			break;
		case TCOFLUSH:
			skb_queue_purge(&hgps->tx_list);
			break;
		case TCIOFLUSH:
			skb_queue_purge(&hgps->rx_list);
			skb_queue_purge(&hgps->tx_list);
			break;
		default:
			GPSDRV_ERR("Invalid Command passed for tcflush");
			retCode = 0;
			break;
		}
		break;
	case FIONREAD:
		/* Deque the SKB from the head if rx_list is not empty
		 * And update the argument with skb->len to provide
		 * the amount of data available in the available SKB
		 */
		skb = skb_dequeue(&hgps->rx_list);
		if (skb != NULL) {
			*(unsigned int *)arg = skb->len;
			/* Re-Store the SKB for furtur Read operations */
			skb_queue_head(&hgps->rx_list, skb);
		} else {
			*(unsigned int *)arg = 0;
		}
		GPSDRV_DBG("returning %d\n", *(unsigned int *)arg);

		break;
	default:
		GPSDRV_DBG("Un-Identified IOCTL %d", cmd);
		retCode = 0;
		break;
	}

	return retCode;
}

/** gpsdrv_poll Function
 *  This function will wait till some data is received to the gps driver from ST
 *
 *  Parameters :
 *  @file  : File pointer for GPS char driver
 *  @wait  : POLL wait information
 *  Returns  status of POLL on success
 *           else suitable error code
 */
static unsigned int gpsdrv_poll(struct file *file, poll_table *wait)
{
	unsigned long mask = 0;
	struct gpsdrv_data *hgps = file->private_data;

	/* Check if GPS is registered to perform read operation */
	if (!test_bit(GPS_ST_RUNNING, &hgps->state)) {
		GPSDRV_ERR("GPS Device is not running");
		return -EINVAL;
	}

	/* Wait till data is signalled from gpsdrv_st_recv function
	 *  with AI2 packet
	 */
	poll_wait(file, &hgps->gpsdrv_data_q, wait);

	if (!skb_queue_empty(&hgps->rx_list))
		mask |= POLLIN;	/* TODO: check app for mask */

	return mask;
}

/* GPS Char driver function pointers
 * These functions are called from USER space by pefroming File Operations
 * on /dev/gps node exposed by this driver during init
 */
const struct file_operations gpsdrv_chrdev_ops = {
	.owner = THIS_MODULE,
	.open = gpsdrv_open,
	.read = gpsdrv_read,
	.write = gpsdrv_write,
	.unlocked_ioctl = gpsdrv_ioctl,
	.poll = gpsdrv_poll,
	.release = gpsdrv_release,
};

/*********Functions called during insmod and delmod****************************/

static int gpsdrv_major;		/* GPS major number */
static struct class *gpsdrv_class;	/* GPS class during class_create */
static struct device *gpsdrv_dev;	/* GPS dev during device_create */
/** gpsdrv_init Function
 *  This function Initializes the gps driver parametes and exposes
 *  /dev/gps node to user space
 *
 *  Parameters : NULL
 *  Returns  GPS_SUCCESS on success
 *           else suitable error code
 */
static int __init gpsdrv_init(void)
{

	GPSDRV_DBG(" Inside %s", __func__);

	/* Expose the device DEVICE_NAME to user space
	 * And obtain the major number for the device
	 */
	gpsdrv_major = register_chrdev(0, DEVICE_NAME, \
			&gpsdrv_chrdev_ops);
	if (0 > gpsdrv_major) {
		GPSDRV_ERR("Error when registering to char dev");
		return GPS_ERR_FAILURE;
	}
	GPSDRV_VER("allocated %d, %d", gpsdrv_major, 0);

	/*  udev */
	gpsdrv_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(gpsdrv_class)) {
		GPSDRV_ERR(" Something went wrong in class_create");
		unregister_chrdev(gpsdrv_major, DEVICE_NAME);
		return GPS_ERR_CLASS;
	}

	gpsdrv_dev =
		device_create(gpsdrv_class, NULL, MKDEV(gpsdrv_major, 0),
				NULL, DEVICE_NAME);
	if (IS_ERR(gpsdrv_dev)) {
		GPSDRV_ERR(" Error in class_create");
		unregister_chrdev(gpsdrv_major, DEVICE_NAME);
		class_destroy(gpsdrv_class);
		return GPS_ERR_CLASS;
	}

	return GPS_SUCCESS;
}

/** gpsdrv_exit Function
 *  This function Destroys the gps driver parametes and /dev/gps node
 *
 *  Parameters : NULL
 *  Returns   NULL
 */
static void __exit gpsdrv_exit(void)
{
	GPSDRV_DBG(" Inside %s", __func__);
	GPSDRV_VER(" Bye.. freeing up %d", gpsdrv_major);

	device_destroy(gpsdrv_class, MKDEV(gpsdrv_major, 0));
	class_destroy(gpsdrv_class);
	unregister_chrdev(gpsdrv_major, DEVICE_NAME);
}

module_init(gpsdrv_init);
module_exit(gpsdrv_exit);
MODULE_LICENSE("GPL");
