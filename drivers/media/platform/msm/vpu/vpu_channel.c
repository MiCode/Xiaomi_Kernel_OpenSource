/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/rpm-smd.h>

#include <uapi/media/msm_vpu.h>
#include "vpu_hfi.h"
#include "vpu_hfi_intf.h"
#include "vpu_ipc.h"
#include "vpu_bus_clock.h"
#include "vpu_property.h"
#include "vpu_channel.h"
#include "vpu_translate.h"
#include "vpu_debug.h"

#define VPU_SHUTDOWN_DEFAULT_DELAY_MS	1000
#define VPU_IPC_DEFAULT_TIMEOUT_MS	1000
#define VPU_LONG_TIMEOUT_MS		10000000

u32 vpu_shutdown_delay = VPU_SHUTDOWN_DEFAULT_DELAY_MS;
u32 vpu_ipc_timeout = VPU_IPC_DEFAULT_TIMEOUT_MS;

#define MAX_CHANNELS		VPU_CHANNEL_ID_MAX
#define SYSTEM_SESSION_ID	((u32)-1)

/* session ID [0..n] -> session's channel ID {1,2} */
#define SID2CID(sid)                 (VPU_SESSION_CHANNEL_ID_BASE + (sid & 0x1))
#define IS_A_SESSION_CHANNEL(cid)    ((cid >= VPU_SESSION_CHANNEL_ID_BASE) &&  \
	       (cid < (VPU_SESSION_CHANNEL_ID_BASE + VPU_NUM_SESSION_CHANNELS)))

#define VPU_OFF			0
#define VPU_ON			1

#define VPU_IS_UP(mode) ((mode) != VPU_OFF)
#define VPU_IS_DOWN(mode) ((mode) == VPU_OFF)

struct vpu_channel {
	/* lock to protect settings/infor of this channel */
	struct mutex chlock;

	/* how many clients */
	int open_count;
};

#define VPU_SYNC_TRANSACT_NUM		8
#define TRANS_ID_MASK			0x0F
#define TRANS_SEQ_SHIFT			4
#define SEQ_TRUNCATE(seq)	(((seq) << TRANS_SEQ_SHIFT) >> TRANS_SEQ_SHIFT)

/* transact is free, not being used */
#define VPU_SYNC_STATE_FREE		0
/* transact is being used, waiting to be signaled */
#define VPU_SYNC_STATE_PENDING		1
/* transact is being used and already signaled */
#define VPU_SYNC_STATE_SIGNALED		2
/* transact is in the middle of being freed, so don't signal */
#define VPU_SYNC_STATE_VOLATILE		3
/* transact is no longer available */
#define VPU_SYNC_STATE_LOCKED		4

struct vpu_sync_transact {
	/* lock to protect everything in this structure */
	struct mutex	slock;

	/* ID of this object 1...VPU_SYNC_TRANSACT_NUM */
	u32 id;

	/* a monotonously increasing sequence number */
	int seq;

	struct completion comp;
	int state; /* VPU_SYNC_STATE_XXX */

	/* TODO: move to dynamic allocation */
	u8 buf[VPU_MAX_EXT_DATA_SIZE];
	u32 buf_size;

	/* number of valid bytes */
	u32 data_size;

	int status; /* transaction return code */

	void *priv; /* data to pass */
};

struct vpu_channel_hal {
	/* lock to protect non-power infor in this structure */
	struct mutex hal_lock;

	/* start/stop reference count */
	int upcount;

	struct vpu_channel channels[MAX_CHANNELS];
	struct vpu_platform_resources *res_orig;

	/* callback */
	channel_event_handler callback_event;
	channel_buffer_handler callback_buffer;
	void *priv;

	/* transacts used for blocking operation */
	struct vpu_sync_transact total_transact[VPU_SYNC_TRANSACT_NUM];

	/* work queue to handle boot/shutdown */
	struct workqueue_struct *power_workq;
	struct work_struct boot_work;
	struct delayed_work shutdown_work;

	/*
	 * lock to protect power infor and serialize
	 * power operations (boot/shutdown/clock etc)
	 */
	struct mutex pw_lock;
	u32 mode; /* power state: VPU_ON/OFF */
	void *clk_handle;
	struct regulator *vdd;
	bool vdd_enabled; /* if VDD is enabled */
	/* internally cached value for current fw logging level */
	int fw_log_level;
};

static struct vpu_channel_hal g_vpu_ch_hal;

/* work handlers */
static void vpu_boot_work_handler(struct work_struct *work);
static void vpu_shutdown_work_handler(struct work_struct *work);

/* init_transact can only be called first time at initialization */
static void init_transact(struct vpu_channel_hal *phal)
{
	int i;
	struct vpu_sync_transact *ptrans;

	for (i = 0; i < VPU_SYNC_TRANSACT_NUM; i++) {
		ptrans = &phal->total_transact[i];

		mutex_init(&ptrans->slock);

		/* transact ids start from 1... */
		ptrans->id = i + 1;

		/* different initial sequence value for each transact */
		ptrans->seq = 2 << i;

		ptrans->buf_size = VPU_MAX_EXT_DATA_SIZE;

		/* state is free */
		ptrans->state = VPU_SYNC_STATE_FREE;
		init_completion(&ptrans->comp);
	}
}

/* reset_transact is called each time at VPU boot */
static void reset_transact(struct vpu_channel_hal *phal)
{
	int i;
	struct vpu_sync_transact *ptrans;

	for (i = 0; i < VPU_SYNC_TRANSACT_NUM; i++) {
		ptrans = &phal->total_transact[i];
		/* state is free */
		ptrans->state = VPU_SYNC_STATE_FREE;
	}
}

static void dump_transact(struct vpu_channel_hal *phal)
{
	int i;
	struct vpu_sync_transact *ptrans;

	/* get one from the list, change state from 0 to 1; */
	for (i = 0; i < VPU_SYNC_TRANSACT_NUM; i++) {
		ptrans = &phal->total_transact[i];
		pr_debug("sync obj %d state: %d seq %d\n",
			ptrans->id, ptrans->state, ptrans->seq);
	}
}

/* get a free sync object, called by user thread */
static struct vpu_sync_transact *get_transact(struct vpu_channel_hal *phal,
				void *buf, u32 buf_size)
{
	int i;
	struct vpu_sync_transact *ptrans, *found;

	/* get one from the list, change state from 0 to 1; */
	for (i = 0; i < VPU_SYNC_TRANSACT_NUM; i++) {
		ptrans = &phal->total_transact[i];
		if (ptrans->state != VPU_SYNC_STATE_FREE)
			continue;

		mutex_lock(&ptrans->slock);
		if (ptrans->state == VPU_SYNC_STATE_FREE) {
			/* mark it as being used */
			ptrans->state = VPU_SYNC_STATE_PENDING;
			INIT_COMPLETION(ptrans->comp);

			ptrans->data_size = 0;
			ptrans->seq++;
			found = ptrans;
		}

		mutex_unlock(&ptrans->slock);

		if (found)
			return found;
	}

	/* not found */
	dump_transact(phal);
	return NULL;
}

/*
 * to free a sync object, called by user thread
 * can only be called for an object returned by get_transact
 */
static void put_transact(struct vpu_channel_hal *phal,
	struct vpu_sync_transact *ptrans)
{
	mutex_lock(&ptrans->slock);

	/*
	 * inc the seq number to signal that any pending
	 * IPC transaction using the old seq number has expired
	 */
	ptrans->seq++;

	if ((ptrans->state == VPU_SYNC_STATE_SIGNALED) ||
		(ptrans->state == VPU_SYNC_STATE_VOLATILE))
		ptrans->state = VPU_SYNC_STATE_FREE;

	mutex_unlock(&ptrans->slock);
}

/* to wait on a sync object, called by user thread
 * can only be called after get_transact, before put_transact
 */
static int wait_transact(struct vpu_sync_transact *ptrans, u32 timeout)
{
	int rc;

	if (!ptrans)
		return -EINVAL;

	if (ptrans->state == VPU_SYNC_STATE_LOCKED)
		return -EINVAL;

	if (ptrans->state == VPU_SYNC_STATE_FREE)
		return -EINVAL;

	rc = wait_for_completion_interruptible_timeout(&ptrans->comp, timeout);
	if (rc > 0) /* transaction is done */
		return rc;

	mutex_lock(&ptrans->slock);

	/* transaction not done, try to cancel it */
	if (ptrans->state == VPU_SYNC_STATE_PENDING)
		ptrans->state = VPU_SYNC_STATE_VOLATILE;
	else if (ptrans->state == VPU_SYNC_STATE_SIGNALED)
		rc = 1; /* The worker thread just signaled it */

	mutex_unlock(&ptrans->slock);

	return rc;
}

/*
 * to check if a sync object is still valid (being waited by a user thread
 * called by kernel worker thread. If successful, the lock is held
 */
static struct vpu_sync_transact *check_transact(struct vpu_channel_hal *phal,
		u32 signature)
{
	struct vpu_sync_transact *ptrans;
	u32 index;

	index = (signature & TRANS_ID_MASK) - 1;

	if (index >= VPU_SYNC_TRANSACT_NUM)
		return NULL;

	ptrans = &phal->total_transact[index];

	mutex_lock(&ptrans->slock);

	/* does seq number match? */
	if ((signature >> TRANS_SEQ_SHIFT) == SEQ_TRUNCATE(ptrans->seq)) {
		/* yes, it is a match, check state then */
		if (ptrans->state == VPU_SYNC_STATE_PENDING) {
			ptrans->state = VPU_SYNC_STATE_SIGNALED;
			/* return with the lock held */
			return ptrans;
		} else {
			/* it is already expired */
			pr_warn("transact with sig %x expired\n", signature);
		}
	}
	mutex_unlock(&ptrans->slock);

	return NULL;
}

/*
 * To signal that a sync object is done, called by kernel worker thread
 * can only be called on object returned by check_transact
 * This function releases the lock held by check_transact.
 */
static void finish_transact(struct vpu_sync_transact *ptrans, s32 status)
{
	if ((ptrans) && mutex_is_locked(&ptrans->slock)) {
		if (ptrans->state == VPU_SYNC_STATE_SIGNALED) {
			ptrans->status = status;
			complete_all(&ptrans->comp);
			mutex_unlock(&ptrans->slock);
		} else {
			pr_err("wrong state %x!\n", ptrans->state);
		}
	}
}

/*
 * called when VPU appears dead
 * to unblock any thread waiting for responses, and prevent any future
 * blocking
 */
static void wakeup_all_transact(struct vpu_channel_hal *phal, int status)
{
	int i;
	struct vpu_sync_transact *ptrans;

	for (i = 0; i < VPU_SYNC_TRANSACT_NUM; i++) {
		ptrans = &phal->total_transact[i];

		mutex_lock(&ptrans->slock);
		ptrans->state = VPU_SYNC_STATE_LOCKED;
		ptrans->status = status;
		complete_all(&ptrans->comp);
		mutex_unlock(&ptrans->slock);
	}
}

static void on_session_cmd_done(struct vpu_channel_hal *phal,
		struct vpu_ipc_msg_header_packet *phdr,
		enum vpu_hw_event evt)
{
	if (likely(phdr->size >= sizeof(struct vpu_ipc_msg_header_packet))) {
		/* in case someone is waiting */
		struct vpu_sync_transact *ptrans;
		ptrans = check_transact(phal, phdr->trans_id);
		if (ptrans) {
			finish_transact(ptrans, phdr->status);
			pr_debug("Returning to waiting caller thread\n");
		} else {
			/* notify upper layer that a command is done */
			if (phal->callback_event)
				phal->callback_event(phdr->sid,	evt,
					phdr->status, phal->priv);
			pr_debug("cmd_done event %d\n", evt);
		}
	} else {
		pr_warn("incomplete packet!\n");
	}
}

/*
 * To handle returned buffer(s)
 * Checks the packet integrity
 */
static void on_buffer_done(struct vpu_channel_hal *phal,
		struct vpu_ipc_msg_header_packet *phdr)
{
	int i, num, info_size;
	struct vpu_ipc_buf_info *pinfo;
	struct vpu_ipc_msg_session_process_buffers_done_packet *packet;

	packet = (struct vpu_ipc_msg_session_process_buffers_done_packet *)phdr;
	pr_debug("for session %d", packet->hdr.sid);

	/* number of buffers, we don't differentiate input and output buffers */
	num = packet->num_in_buf + packet->num_out_buf;

	/* locate the first buffer infor block, following the packet */
	pinfo = (struct vpu_ipc_buf_info *)(((u32)packet) + sizeof(*packet));

	for (i = 0; i < num; i++) {
		/* the size of this buffer info block */
		info_size = pinfo->buf_addr_size + sizeof(*pinfo);

		/* make sure it is a complete buffer information block */
		if (((u32)pinfo) + info_size <= ((u32)phdr) + phdr->size) {
			/* get the vb2 buffer pointer */
			struct vpu_buffer *vb;
			vb = (struct vpu_buffer *) pinfo->tag;
			if (vb) {
				int status = 0;

				/* output buffers always first; get time stamp,
				 * if no error
				 */
				if ((i < packet->num_out_buf) &&
						(phdr->status == 0)) {

					/* store buffer flags info */
					if (pinfo->flag & BUFFER_PKT_FLAG_EOS) {
						pr_debug("out EOS buf #%d\n",
							vb->vb.v4l2_buf.index);
						vb->vb.v4l2_buf.flags |=
							V4L2_QCOM_BUF_FLAG_EOS;
					}
					if (pinfo->flag &
						BUFFER_PKT_FLAG_CDS_ENABLE) {
						vb->vb.v4l2_buf.flags |=
						V4L2_BUF_FLAG_CDS_ENABLE;
					}

					vb->vb.v4l2_buf.timestamp.tv_sec =
							pinfo->timestamp_hi;
					vb->vb.v4l2_buf.timestamp.tv_usec =
							pinfo->timestamp_lo;

					vb->vb.v4l2_buf.field =
						translate_field_to_api(
							packet->buf_pkt_flag);
				}

				/* TODO: if error, translate the status */
				status = phdr->status;

				if (likely(phal->callback_buffer))
					phal->callback_buffer(phdr->sid, vb,
							status, phal->priv);
			}

			/* move to next one */
			pinfo = (struct vpu_ipc_buf_info *)((u32)pinfo +
					info_size);
		} else {
			pr_warn("incomplete packet\n");
			break;
		}
	}
}

static void on_sys_property_info(struct vpu_channel_hal *phal,
		struct vpu_ipc_msg_header_packet *phdr)
{
	struct vpu_ipc_msg_sys_property_info_packet *packet;

	packet = (struct vpu_ipc_msg_sys_property_info_packet *)phdr;

	if (packet->data_offset + packet->data_size <= phdr->size) {
		struct vpu_sync_transact *ptrans;
		ptrans = check_transact(phal, phdr->trans_id);

		/* update property buffer in trans object */
		if (ptrans) {

			if (packet->data_size > ptrans->buf_size)
				pr_warn("packet size greater than buffer\n");

			ptrans->data_size = min(ptrans->buf_size,
					packet->data_size);
			memcpy(ptrans->buf,
				(void *) (((u32) packet) + packet->data_offset),
				ptrans->data_size);

			/* in case someone is waiting */
			finish_transact(ptrans, phdr->status);
		}
	} else {
		pr_warn("incomplete packet\n");
	}
}

static void on_session_property_info(struct vpu_channel_hal *phal,
		struct vpu_ipc_msg_header_packet *phdr)
{
	struct vpu_ipc_msg_session_property_info_packet *packet;

	packet = (struct vpu_ipc_msg_session_property_info_packet *)phdr;

	if (packet->data_offset + packet->data_size <= phdr->size) {
		struct vpu_sync_transact *ptrans;
		ptrans = check_transact(phal, phdr->trans_id);

		/* update property buffer */
		if (ptrans) {

			if (packet->data_size > ptrans->buf_size)
				pr_warn("packet size greater than buffer\n");

			ptrans->data_size = min(ptrans->buf_size,
					packet->data_size);
			memcpy(ptrans->buf,
				(void *) (((u32) packet) + packet->data_offset),
				ptrans->data_size);

			/* in case someone is waiting */
			finish_transact(ptrans, phdr->status);
		}
	} else {
		pr_warn("incomplete packet\n");
	}
}

static void on_event_notify(struct vpu_channel_hal *phal,
		u32 sid, struct vpu_ipc_msg_header_packet *phdr)
{
	u32 event = 0;
	struct vpu_ipc_msg_event_notify_packet *packet =
			(struct vpu_ipc_msg_event_notify_packet *)phdr;

	pr_debug("(event_id=%d | event_data=%d)\n",
			packet->event_id, packet->event_data);

	if (unlikely(phdr->size < sizeof(*packet)))
		return; /* incomplete packet */

	switch (packet->event_id) {
	case VPU_IPC_EVENT_ERROR_NONE:
		event = VPU_SYS_EVENT_NONE;
		break;
	case VPU_IPC_EVENT_ERROR_SYSTEM:
		event = VPU_HW_EVENT_IPC_ERROR;
		break;
	case VPU_IPC_EVENT_ERROR_SESSION:
		event = VPU_HW_EVENT_IPC_ERROR;
		break;
	default:
		pr_err("Notification (%d) not handled\n", packet->event_id);
		break;
	}

	if (event != 0 && phal->callback_event)
		phal->callback_event(sid, event,
				packet->event_data, phal->priv);
}

static void on_msg_active_region(struct vpu_channel_hal *phal,
		struct vpu_ipc_msg_header_packet *phdr)
{
	struct vpu_ipc_msg_session_active_region_packet *pkt;

	pkt = (struct vpu_ipc_msg_session_active_region_packet *) phdr;

	if (phal->callback_event)
		phal->callback_event(phdr->sid,
				VPU_HW_EVENT_ACTIVE_REGION_CHANGED,
				(u32)&pkt->active_rect, phal->priv);
}

#define TEMP_BUFFER_SIZE			128

static void on_logging_msg(struct vpu_channel_hal *phal,
			struct vpu_ipc_log_header_packet *logphdr)
{
	char	log_buf[TEMP_BUFFER_SIZE];
	size_t	length;
	u32	ptr = logphdr->msg_addr ? logphdr->msg_addr :
			((u32)logphdr + sizeof(*logphdr));

	/* VPU log is not Null-terminated, find out what the right size is */
	length = (logphdr->msg_size < TEMP_BUFFER_SIZE) ?
			logphdr->msg_size : TEMP_BUFFER_SIZE;

	strlcpy(log_buf, (const char *)ptr, length);
	pr_debug("t=%d: %s\n", logphdr->time_stamp, log_buf);
}

/*
 * to handle a packet received from HFI layer
 * the first int of the packet is the size of the packet, and HFI layer
 * guarantees that amount of data. The on_xxx handlers need to check if that
 * size matches size of the specific structure before interpreting it
 */
static void chan_handle_msg(u32 cid, struct vpu_hfi_packet *packet, void *priv)
{
	struct vpu_channel_hal *phal = (struct vpu_channel_hal *)priv;
	struct vpu_ipc_msg_header_packet *phdr;

	if (unlikely(!priv)) {
		pr_err("Null priv data\n");
		return;
	}

	if (cid == VPU_LOGGING_CHANNEL_ID) {
		struct vpu_ipc_log_header_packet *logphdr =
				(struct vpu_ipc_log_header_packet *)packet;
		on_logging_msg(phal, logphdr);
		return;
	}

	phdr = (struct vpu_ipc_msg_header_packet *)packet;
	pr_debug("0x%08x, message id = 0x%08x\n", phdr->trans_id, phdr->msg_id);

	switch (phdr->msg_id) {
	case VPU_IPC_MSG_SESSION_SET_BUFFERS_DONE:
		/* do nothing*/
		break;

	case VPU_IPC_MSG_SESSION_PROCESS_BUFFERS_DONE:
		on_buffer_done(phal, phdr);
		break;

	case VPU_IPC_MSG_SESSION_RELEASE_BUFFERS_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_RELEASE_DONE);
		break;

	case VPU_IPC_MSG_SYS_SESSION_OPEN_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_OPEN_DONE);
		break;

	case VPU_IPC_MSG_SYS_SESSION_CLOSE_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_CLOSE_DONE);
		break;

	case VPU_IPC_MSG_SESSION_START_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_START_DONE);
		break;

	case VPU_IPC_MSG_SESSION_STOP_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_STOP_DONE);
		break;

	case VPU_IPC_MSG_SESSION_PAUSE_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_PAUSE_DONE);
		break;

	case VPU_IPC_MSG_SESSION_FLUSH_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_FLUSH_DONE);
		break;

	case VPU_IPC_MSG_SESSION_SET_PROPERTY_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SESSION_SET_PROPERTY_DONE);
		break;

	case VPU_IPC_MSG_SYS_SET_PROPERTY_DONE:
		on_session_cmd_done(phal, phdr,
			VPU_HW_EVENT_SYS_SET_PROPERTY_DONE);
		break;

	case VPU_IPC_MSG_SESSION_PROPERTY_INFO:
		on_session_property_info(phal, phdr);
		break;

	case VPU_IPC_MSG_SYS_PROPERTY_INFO:
		on_sys_property_info(phal, phdr);
		break;

	case VPU_IPC_MSG_SYS_EVENT_NOTIFY:
		on_event_notify(phal, SYSTEM_SESSION_ID, phdr);
		break;
	case VPU_IPC_MSG_SESSION_EVENT_NOTIFY:
		on_event_notify(phal, phdr->sid, phdr);
		break;

	case VPU_IPC_MSG_SESSION_ACTIVE_REGION:
		on_msg_active_region(phal, phdr);
		break;

	default:
		pr_warn("message 0x%08x not handled\n", phdr->msg_id);
		break;
	}
}

/*
 * handler for local events from HFI layer
 * local events include:
 * VPU watchdog
 */
static void chan_handle_event(u32 cid, enum vpu_hfi_event local_event,
		void *priv)
{
	struct vpu_channel_hal *phal = (struct vpu_channel_hal *)priv;

	pr_debug("local event = %d\n", local_event);

	if (unlikely(!phal))
		return;

	if (local_event == VPU_LOCAL_EVENT_WD) {
		/*
		 * watchdog bite, means VPU hangs. Need to unblock all the
		 * blocking operation and prevent any new synch operation from
		 * happening
		 */
		wakeup_all_transact(phal, -ECONNRESET);

		if (likely(phal->callback_event))
			phal->callback_event(SYSTEM_SESSION_ID,
					VPU_HW_EVENT_WATCHDOG_BITE,
					0, phal->priv);

		/* immediate (forced) runtime stop */
		pr_warn("Watchdog Event. Forcing shutdown\n");
		vpu_hw_sys_stop(1);
	}
}

static int copy_from_trans_buffer(struct vpu_sync_transact *trans,
		u8 *rx_data, u32 rx_size) {
	if (unlikely(rx_size < trans->data_size)) {
		pr_err("Not enough memory (%d < %d bytes)\n",
				rx_size, trans->data_size);
		return -ENOMEM;
	} else {
		memcpy(rx_data, (const void *) trans->buf, trans->data_size);
		return 0;
	}
}

static int copy_to_user_from_trans_buffer(struct vpu_sync_transact *trans,
		void __user *rx_data, u32 rx_size)
{
	if (unlikely(rx_size < trans->data_size)) {
		pr_err("Not enough memory (%d < %d bytes)\n",
				rx_size, trans->data_size);
		return -ENOMEM;
	}

	if (copy_to_user(rx_data, (const void *)trans->buf, trans->data_size))
		return -EFAULT;

	/* return how much data are copied into user buffer */
	return trans->data_size;
}

/**
 * ipc_cmd_sync_prepare() - prepare synchronous transaction
 * @hdr:	in/out - command packet header, will be written to make it sync
 *
 * Return: pointer to the transact struct to use
 */
static struct vpu_sync_transact *ipc_cmd_sync_prepare(
		struct vpu_ipc_cmd_header_packet *hdr,
		void *buf, u32 buf_size)
{
	struct vpu_sync_transact *ptrans;

	/* try to get a reusable transact, use async otherwise */
	ptrans = get_transact(&g_vpu_ch_hal, buf, buf_size);

	if (unlikely(!ptrans)) {
		pr_warn("no sync object left, use async operation\n");
		hdr->flags = 0;
		hdr->trans_id = 0;
	} else {
		/* configure header for a sync command */
		hdr->flags = 1;
		/* record transaction sequence number and ID in packet header */
		hdr->trans_id = ptrans->seq << TRANS_SEQ_SHIFT;
		hdr->trans_id |= ptrans->id;
	}

	return ptrans;
}

/**
 * ipc_cmd_sync_wait()
 * @ptrans:	in - the transact to wait for
 * @timeout_ms:	in - the time to wait for, in ms
 * @cid:	in - the channel id (debug purpose)
 *
 * Return:	-EIO, in case of FW error
 *		-ETIMEDOUT, in case of timeout
 *		0, otherwise
 */
static int ipc_cmd_sync_wait(struct vpu_sync_transact *ptrans, u32 timeout_ms,
		int cid)
{
	int rc = 0;
	u32 jiffies = msecs_to_jiffies(timeout_ms);

	/* wait for the ack */
	rc = wait_transact(ptrans, jiffies);

	/* return value */
	if (rc > 0) {
		if (ptrans->status == 0) {
			/* FW returns success */
			rc = 0;
		} else if (ptrans->status > 0) {
			/* FW error state */
			rc = -EIO;
			pr_err("FW failed during IPC with err %d\n",
					ptrans->status);
		} else {
			/* local error */
			rc = ptrans->status;
			pr_err("Local IPC err %d\n", rc);
		}
	} else if (rc == 0) {
		/* timeout */
		char dbg_buf[320];
		size_t dbg_buf_size = 320;

		pr_err("Timeout for transact 0x%08x\n",
			ptrans->seq << TRANS_SEQ_SHIFT | ptrans->id);

		/* service log queue on timeout */
		vpu_wakeup_fw_logging_wq();

		strlcpy(dbg_buf, "", dbg_buf_size);
		/* cid represents Tx & Rx queues index) */
		vpu_hfi_dump_queue_headers(cid, dbg_buf, dbg_buf_size);
		pr_err("Queue dump:\n%s", dbg_buf);

		rc = -ETIMEDOUT;
	} else {
		pr_err("transact 0x%08x failed! (error %d)\n",
			ptrans->seq << TRANS_SEQ_SHIFT | ptrans->id, rc);
	}

	return rc;
}

/* release the transact */
static void ipc_cmd_sync_release(struct vpu_sync_transact *ptrans)
{
	put_transact(&g_vpu_ch_hal, ptrans);
}

/**
 * ipc_cmd_simple() - Simple IPC command
 * @cid:	channel ID
 * @hdr:	packet header
 * @nonblocking: if true, not block the caller
 * @timeout_ms	timeout in milliseconds, ignored if nonblocking
 *
 * Return: 0 on success, -ve on failure
 */
static int ipc_cmd_simple(u32 cid, struct vpu_ipc_cmd_header_packet *hdr,
		bool nonblocking, u32 timeout_ms) {
	int rc;
	struct vpu_sync_transact *ptrans = NULL;

	if (!nonblocking)
		ptrans = ipc_cmd_sync_prepare(hdr, NULL, 0);

	pr_debug("IPC Tx%d: cmd_id=0x%08x | sending %d bytes\n",
			cid, hdr->cmd_id, hdr->size);

	/* send the synchronous command over system channel */
	rc = vpu_hfi_write_packet_commit(cid, (struct vpu_hfi_packet *)hdr);

	if (!nonblocking && ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, timeout_ms, cid);
		ipc_cmd_sync_release(ptrans);
	}

	return rc;
}

/* channel id (enum vpu_hfi_channel_ids) -> q ids (enum vpu_hfi_queue_ids) */
#define GET_CMD_QUEUE_ID(cid)		(VPU_SYSTEM_CMD_QUEUE_ID + (cid * 2))
#define GET_MSG_QUEUE_ID(cid)		(VPU_SYSTEM_MSG_QUEUE_ID + (cid * 2))

int vpu_hw_session_open(u32 sid, u32 flag)
{
	struct vpu_ipc_cmd_session_open_packet packet;
	int cid = SID2CID(sid);
	int rc;
	struct vpu_channel *ch;
	struct vpu_channel_hal *ch_hal;

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	ch_hal = &g_vpu_ch_hal;
	ch = &ch_hal->channels[cid];

	mutex_lock(&ch->chlock);
	/* enable channel for first open on this channel */
	if (ch->open_count == 0)
		vpu_hfi_enable(cid, ch_hal);
	ch->open_count++;
	mutex_unlock(&ch->chlock);

	/* open session command */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SYS_SESSION_OPEN;
	packet.hdr.sid = sid;
	/* FIXME request priority from upper layer */
	packet.session_priority = 0;
	packet.cmd_queue_id = GET_CMD_QUEUE_ID(cid);
	packet.msg_queue_id = GET_MSG_QUEUE_ID(cid);

	/* send the open command over system channel */
	pr_debug("IPC  CMD_SYS_SESSION_OPEN (session %d)\n", sid);
	rc = ipc_cmd_simple(VPU_SYSTEM_CHANNEL_ID, &packet.hdr, false,
			vpu_ipc_timeout);
	if (unlikely(rc)) {
		/* not successful, restore the count */
		pr_err("open session failed, err %d\n", rc);

		mutex_lock(&ch->chlock);
		ch->open_count--;
		if (ch->open_count == 0)
			vpu_hfi_disable(cid);
		mutex_unlock(&ch->chlock);

		return rc;
	}
	return 0;
}

void vpu_hw_session_close(u32 sid)
{
	struct vpu_ipc_cmd_session_close_packet packet;
	struct vpu_channel *ch;
	int cid = SID2CID(sid);
	int rc;

	if (unlikely(!IS_A_SESSION_CHANNEL(cid)))
		return;

	ch = &g_vpu_ch_hal.channels[cid];

	/* close session command */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SYS_SESSION_CLOSE;
	packet.hdr.sid = sid;

	/* send the close command over system channel */
	pr_debug("IPC  CMD_SYS_SESSION_CLOSE (session %d)\n", sid);
	rc = ipc_cmd_simple(VPU_SYSTEM_CHANNEL_ID, &packet.hdr, false,
			vpu_ipc_timeout);
	if (unlikely(rc))
		pr_err("session %d: close failed, err %d\n", sid, rc);

	mutex_lock(&ch->chlock);
	ch->open_count--;
	/* close the channel for last close on this channel */
	if (ch->open_count == 0)
		vpu_hfi_disable(cid);
	mutex_unlock(&ch->chlock);
}

/*
 * to set a property
 * sid must be valid
 */
static int ipc_cmd_set_session_prop(u32 sid, u32 prop_id,
		u32 port_id, void *extra, u32 extra_size)
{
	struct vpu_ipc_cmd_session_set_property_packet packet;
	int cid = SID2CID(sid);

	/* set property cmd, and the property is "commit" */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet) + extra_size;
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_SET_PROPERTY;
	packet.hdr.flags = 0; /* no ack */
	packet.hdr.trans_id = 0; /* no sync */
	packet.hdr.sid = sid;
	packet.hdr.port_id = port_id;

	packet.prop_id = prop_id;
	packet.data_offset = sizeof(packet);
	packet.data_size = extra_size;
	packet.reserved = 0;

	/* send out */
	pr_debug("IPC Tx%d: CMD_SESSION_SET_PROPERTY, prop_id=0x%08x\n",
		cid, packet.prop_id);
	return vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&packet,
			(u8 *)extra, extra_size);
}

/**
 * ipc_cmd_sync_get_session_prop() - get a session property
 * @sid:	Session ID
 * @rxd:	buf to store the property data once response packet is received.
 *		it's the caller's responsibility to make sure @rxd not NULL
 * @rxd_size: expected size of the received property data
 *
 * blocking operation
 *
 * Return: 0 on success, -ve on failure
 */
static int ipc_cmd_sync_get_session_prop(u32 sid, u32 prop_id,
		u32 port_id, u8 *rxd, u32 rxd_size)
{
	struct vpu_ipc_cmd_session_get_property_packet packet;
	struct vpu_sync_transact *ptrans;
	int cid = SID2CID(sid);
	int rc;

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_GET_PROPERTY;
	packet.hdr.sid = sid;
	packet.hdr.port_id = port_id;

	packet.prop_id = prop_id;
	packet.data_offset = 0;
	packet.data_size = 0;
	packet.reserved = 0;

	ptrans = ipc_cmd_sync_prepare(&packet.hdr, rxd, rxd_size);

	/* send the synchronous command over system channel */
	rc = vpu_hfi_write_packet_commit(cid, (struct vpu_hfi_packet *)&packet);
	if (ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, vpu_ipc_timeout, cid);

		/* copy the content of the cache into param */
		if (!rc)
			rc = copy_from_trans_buffer(ptrans, rxd, rxd_size);

		ipc_cmd_sync_release(ptrans);
	}

	pr_debug("Exit (rc=%d)\n", rc);
	return rc;
}

/* synchronous session commit helper function */
static int ipc_cmd_config_session_commit(u32 sid, u32 type)
{
	struct vpu_ipc_cmd_session_set_property_packet packet;
	struct vpu_data_value commit_prop;
	struct vpu_sync_transact *ptrans;
	int cid = SID2CID(sid);
	int rc;

	/* set property cmd, and the property is "commit" */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet) + sizeof(commit_prop);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_SET_PROPERTY;
	packet.hdr.sid = sid;

	packet.prop_id = VPU_PROP_SESSION_COMMIT;
	packet.data_offset = sizeof(packet);
	packet.data_size = sizeof(commit_prop);
	packet.reserved = 0;

	commit_prop.flags = 0;
	commit_prop.value = type;

	/* send out, synchronously */
	pr_debug("IPC Tx%d: CMD_SESSION_SET_PROPERTY (SESSION_COMMIT)\n", cid);

	ptrans = ipc_cmd_sync_prepare(&packet.hdr, NULL, 0);

	/* send the synchronous command over */
	rc = vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&packet,
			(u8 *)&commit_prop, sizeof(commit_prop));

	if (ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, vpu_ipc_timeout, cid);
		ipc_cmd_sync_release(ptrans);
	}

	pr_debug("Exit (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_session_start(u32 sid)
{
	int rc = 0;
	struct vpu_ipc_cmd_session_start_packet packet;
	struct vpu_channel *ch;
	int cid = SID2CID(sid);
	struct vpu_channel_hal *phal =  &g_vpu_ch_hal;

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}
	ch = &phal->channels[cid];

	/* session start cmd */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_START;
	packet.hdr.sid = sid;

	pr_debug("IPC Tx%d: CMD_SESSION_START\n", cid);
	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

int vpu_hw_session_stop(u32 sid)
{
	int rc;
	struct vpu_ipc_cmd_session_stop_packet packet;
	struct vpu_channel *ch;
	int cid = SID2CID(sid);
	struct vpu_channel_hal *phal =  &g_vpu_ch_hal;

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}
	ch = &phal->channels[cid];

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_STOP;
	packet.hdr.sid = sid;

	pr_debug("IPC Tx%d: CMD_SESSION_STOP\n", cid);
	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

int vpu_hw_session_pause(u32 sid)
{
	int rc;
	struct vpu_ipc_cmd_session_pause_packet packet;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_PAUSE;
	packet.hdr.sid = sid;

	pr_debug("IPC Tx%d: CMD_SESSION_PAUSE\n", cid);
	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

int vpu_hw_session_resume(u32 sid)
{
	int rc;
	struct vpu_ipc_cmd_session_start_packet packet;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_START;
	packet.hdr.sid = sid;

	pr_debug("IPC Tx%d: CMD_SESSION_START\n", cid);
	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

int vpu_hw_session_flush(u32 sid, u32 port_id, enum flush_buf_type type)
{
	int rc;
	struct vpu_ipc_cmd_session_flush_packet packet;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_FLUSH;
	packet.hdr.sid = sid;
	packet.hdr.port_id = port_id;

	packet.flush_type = type;

	pr_debug("IPC  CMD_SESSION_FLUSH\n");
	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

int vpu_hw_session_release_buffers(u32 sid, u32 port_id,
		enum release_buf_type release_type)
{
	int rc;
	struct vpu_ipc_cmd_session_release_buffers_packet packet;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	/* fill packet */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_RELEASE_BUFFERS;
	packet.hdr.sid = sid;
	packet.hdr.port_id = port_id;

	switch (release_type) {
	case CH_RELEASE_IN_BUF:
		packet.buf_type = VPU_IPC_BUFFER_INPUT;
		break;

	case CH_RELEASE_OUT_BUF:
		packet.buf_type = VPU_IPC_BUFFER_OUTPUT;
		break;

	case CH_RELEASE_NR_BUF:
		packet.buf_type = VPU_IPC_BUFFER_NR;
		break;
	default:
		return -EINVAL;
	}

	rc = ipc_cmd_simple(cid, &packet.hdr, false, vpu_ipc_timeout);

	return rc;
}

#define MAX_BUFFER_NUM	8
struct _ipc_buffer_info_ext {
	struct vpu_ipc_buf_info buf_info;
	/* max number of address is 6 */
	u32 addr[3*2];
};

/*
 * helper function: vpu_buf_to_ipc_buf_info
 * translate information from vpu_buffer to an IPC buffer information block
 * assume the caller makes sure the number of plane is no more than 3, and VPU
 * address is always present
 */
static void vpu_buf_to_ipc_buf_info(struct vpu_buffer *vb, bool input,
			struct _ipc_buffer_info_ext *bie, u32 *pktflag)
{
	u32 flag = 0;
	u32 addr_count = 0;
	int i;

	if (input) {
		/*
		 * an input buffer, assign timestamp, VPU address,
		 * VCAP address if present
		 */

		bie->buf_info.timestamp_hi = vb->vb.v4l2_buf.timestamp.tv_sec;
		bie->buf_info.timestamp_lo = vb->vb.v4l2_buf.timestamp.tv_usec;

		if (vb->valid_addresses_mask & ADDR_VALID_VCAP) {
			/* put each plane's VCAP address as src address */
			for (i = 0; i < vb->vb.num_planes; i++)
				bie->addr[addr_count + i] =
				  vb->planes[i].mapped_address[ADDR_INDEX_VCAP];

			/* mark that input sink address present */
			flag |= BUFFER_PKT_FLAG_IN_SRC_SINK;

			/* increase the address count */
			addr_count += vb->vb.num_planes;
		}

		/* number of planes */
		flag |= (vb->vb.num_planes - 1) <<
				BUFFER_PKT_FLAG_IN_PLANE_NUM_SHIFT;

		/* field information */
		flag |= translate_field_to_hfi(vb->vb.v4l2_buf.field);

		/* store EOS info if present */
		if (vb->vb.v4l2_buf.flags & V4L2_QCOM_BUF_FLAG_EOS) {
			pr_debug("in EOS buf #%d\n", vb->vb.v4l2_buf.index);
			flag |= BUFFER_PKT_FLAG_EOS;
		}

		/* set chroma downsample bit if present */
		if (vb->vb.v4l2_buf.flags & V4L2_BUF_FLAG_CDS_ENABLE) {
			pr_debug("in CDS enable buf #%d\n",
					vb->vb.v4l2_buf.index);
			flag |= BUFFER_PKT_FLAG_CDS_ENABLE;
		}
	}

	/* VPU address must always be present, callers of this func to check */
	if (vb->valid_addresses_mask & ADDR_VALID_VPU) {
		/* put each plane's VPU address as dest address */
		for (i = 0; i < vb->vb.num_planes; i++) {
			bie->addr[addr_count + i] =
				vb->planes[i].mapped_address[ADDR_INDEX_VPU];
			pr_debug("%s, buffer %d plane %d address = (0x%08x)\n",
				input ? "Input" : "Output",
				vb->vb.v4l2_buf.index, i,
				vb->planes[i].mapped_address[ADDR_INDEX_VPU]);
		}

		/* increase the address count */
		addr_count += vb->vb.num_planes;
	}

	if (!input) {
		/* output buffer */
		if (vb->valid_addresses_mask & ADDR_VALID_MDP) {
			/* put each plane's MDP address as dest address */
			for (i = 0; i < vb->vb.num_planes; i++)
				bie->addr[addr_count + i] =
				   vb->planes[i].mapped_address[ADDR_INDEX_MDP];

			/* mark that output sink address present */
			flag |= BUFFER_PKT_FLAG_OUT_SRC_SINK;

			/* increase the address count */
			addr_count += vb->vb.num_planes;
		}

		/* number of planes */
		flag |= (vb->vb.num_planes - 1) <<
				BUFFER_PKT_FLAG_OUT_PLANE_NUM_SHIFT;
	}

	/* save address size, in bytes */
	bie->buf_info.buf_addr_size = addr_count * 4;

	/* save the vb pointer into tag */
	bie->buf_info.tag = (u32) vb;

	/* unused */
	bie->buf_info.flag = 0;

	/* return the flag */
	if (pktflag)
		*pktflag = flag;
}

int vpu_hw_session_register_buffers(u32 sid, u32 port_id,
		struct vpu_buffer *vb, u32 num)
{
	int rc;
	int i;
	u32 extra_size;
	struct vpu_ipc_cmd_session_buffers_packet buffer_packet;
	u8 extra_info[MAX_BUFFER_NUM * sizeof(struct _ipc_buffer_info_ext)];
	struct _ipc_buffer_info_ext *pbuf_info_ext;
	int cid = SID2CID(sid);
	bool input = (port_id == VPU_IPC_PORT_INPUT) ? true : false;

	if (unlikely(!vb)) {
		pr_err("Null pointer vb\n");
		return -EINVAL;
	}

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	if (unlikely((num > MAX_BUFFER_NUM) || (num == 0))) {
		pr_err("unexpected number of buffers %d\n", num);
		return -EINVAL;
	}

	if (unlikely((vb[0].vb.num_planes == 0) ||
			(vb[0].vb.num_planes > VPU_MAX_PLANES))) {
		pr_err("unsupported # of planes (%d)\n", vb[0].vb.num_planes);
		return -EINVAL;
	}

	if (unlikely(!(vb[0].valid_addresses_mask & ADDR_VALID_VPU))) {
		pr_err("VPU address not present!\n");
		return -EINVAL;
	}

	/* fill packet */
	memset(&buffer_packet, 0, sizeof(buffer_packet));
	buffer_packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_SET_BUFFERS;
	buffer_packet.hdr.flags = 0; /* no ack */
	buffer_packet.hdr.trans_id = 0; /* no sync */
	buffer_packet.hdr.sid = sid;
	buffer_packet.hdr.port_id = port_id;

	if (input) {
		buffer_packet.num_in_buf = num;
		buffer_packet.num_out_buf = 0;
	} else {
		buffer_packet.num_in_buf = 0;
		buffer_packet.num_out_buf = num;
	}
	buffer_packet.reserved = 0; /* unused */

	/* fill first buffer_info, pkt flag expected same for all */
	pbuf_info_ext = (struct _ipc_buffer_info_ext *) extra_info;
	vpu_buf_to_ipc_buf_info(&vb[0], input, pbuf_info_ext,
			&buffer_packet.buf_pkt_flag);

	/* actual size of the buffer infor (excluding non used addr) */
	extra_size = sizeof(struct vpu_ipc_buf_info) +
			pbuf_info_ext->buf_info.buf_addr_size;

	/* fill rest buffer_info */
	for (i = 1; i < num; i++) {
		pbuf_info_ext = (struct _ipc_buffer_info_ext *)(extra_info +
							i * extra_size);
		vpu_buf_to_ipc_buf_info(&vb[i], input, pbuf_info_ext, NULL);
	}

	/* total packet size */
	buffer_packet.hdr.size =
			sizeof(struct vpu_ipc_cmd_session_buffers_packet) +
				extra_size * num;
	/* send out asynchronously */
	pr_debug("IPC Tx%d: CMD_SESSION_SET_BUFFERS (%dB @0x%08x)\n",
			cid, buffer_packet.hdr.size, (u32)pbuf_info_ext->addr);
	rc = vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&buffer_packet,
			(u8 *)&extra_info, extra_size * num);
	return rc;
}

/*
 * vpu_hw_session_fill_buffer
 * to queue an empty output buffer
 */
int vpu_hw_session_fill_buffer(u32 sid, u32 port_id, struct vpu_buffer *vb)
{
	int rc;
	struct vpu_ipc_cmd_session_buffers_packet buffer_packet;
	struct _ipc_buffer_info_ext buf_info_ext;
	u32 extra_size;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	if (unlikely((vb->vb.num_planes == 0) ||
			(vb->vb.num_planes > VPU_MAX_PLANES))) {
		pr_err("unsupported # of planes (%d)\n", vb->vb.num_planes);
		return -EINVAL;
	}

	if (!(vb->valid_addresses_mask & ADDR_VALID_VPU)) {
		pr_err("VPU address not present!\n");
		return -EINVAL;
	}

	/* fill packet */
	memset(&buffer_packet, 0, sizeof(buffer_packet));
	buffer_packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_PROCESS_BUFFERS;
	buffer_packet.hdr.flags = 0; /* no ack */
	buffer_packet.hdr.trans_id = 0; /* no sync */
	buffer_packet.hdr.sid = sid;
	buffer_packet.hdr.port_id = port_id;

	buffer_packet.num_out_buf = 1;
	buffer_packet.num_in_buf = 0;
	buffer_packet.reserved = 0; /* unused */

	/* fill buffer_info, and the flag */
	vpu_buf_to_ipc_buf_info(vb, false, &buf_info_ext,
			&buffer_packet.buf_pkt_flag);
	extra_size = sizeof(struct vpu_ipc_buf_info) +
				buf_info_ext.buf_info.buf_addr_size;
	/* total size */
	buffer_packet.hdr.size =
		sizeof(struct vpu_ipc_cmd_session_buffers_packet) + extra_size;

	/* send out asynchronously */
	pr_debug("IPC Tx%d: CMD_SESSION_PROCESS_BUFFERS (fill %dB @0x%08x)\n",
			cid, buffer_packet.hdr.size, (u32)buf_info_ext.addr);
	rc = vpu_hfi_write_packet_extra_commit(cid,
		(struct vpu_hfi_packet *)&buffer_packet,
		(u8 *)&buf_info_ext, extra_size);
	return rc;
}

int vpu_hw_session_empty_buffer(u32 sid, u32 port_id, struct vpu_buffer *vb)
{
	int rc;
	struct vpu_ipc_cmd_session_buffers_packet buffer_packet;
	struct _ipc_buffer_info_ext buf_info_ext;
	u32 extra_size;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad sid\n");
		return -EINVAL;
	}

	if (unlikely((vb->vb.num_planes == 0) ||
			(vb->vb.num_planes > VPU_MAX_PLANES))) {
		pr_err("unsupported # of planes (%d)\n", vb->vb.num_planes);
		return -EINVAL;
	}

	if (!(vb->valid_addresses_mask & ADDR_VALID_VPU)) {
		pr_err("VPU address not present!\n");
		return -EINVAL;
	}

	/* fill packet */
	memset(&buffer_packet, 0, sizeof(buffer_packet));
	buffer_packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_PROCESS_BUFFERS;
	buffer_packet.hdr.flags = 0; /* no ack */
	buffer_packet.hdr.trans_id = 0; /* no sync */
	buffer_packet.hdr.sid = sid;
	buffer_packet.hdr.port_id = port_id;

	buffer_packet.num_out_buf = 0;
	buffer_packet.num_in_buf = 1;
	buffer_packet.reserved = 0; /* unused */

	/* fill buffer_info, and the flag */
	vpu_buf_to_ipc_buf_info(vb, true, &buf_info_ext,
				&buffer_packet.buf_pkt_flag);

	extra_size = sizeof(struct vpu_ipc_buf_info) +
				buf_info_ext.buf_info.buf_addr_size;
	/* total size */
	buffer_packet.hdr.size =
		sizeof(struct vpu_ipc_cmd_session_buffers_packet) + extra_size;

	/* send out asynchronously */
	pr_debug("IPC Tx%d: CMD_SESSION_PROCESS_BUFFERS (Empty %dB @0x%08x)\n",
			cid, buffer_packet.hdr.size, (u32)buf_info_ext.addr);
	rc = vpu_hfi_write_packet_extra_commit(cid,
		(struct vpu_hfi_packet *)&buffer_packet,
		(u8 *)&buf_info_ext, extra_size);
	return rc;
}

int vpu_hw_session_commit(u32 sid, enum commit_type ct,
			  u32 load_kbps, u32 pwr_mode)
{
	int rc;
	u32 ipc_ct;
	int cid = SID2CID(sid);
	struct vpu_channel_hal *hal = &g_vpu_ch_hal;
	pr_debug("(sid=%d, type=%d)", sid, ct);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	if (ct == CH_COMMIT_IN_ORDER) {
		ipc_ct = VPU_COMMIT_APPLY_IN_ORDER;
	} else if (ct == CH_COMMIT_AT_ONCE) {
		ipc_ct = VPU_COMMIT_APPLY_AT_ONCE;
	} else {
		pr_err("bad commit type\n");
		return -EINVAL;
	}

	mutex_lock(&hal->pw_lock);
	if (vpu_bus_scale(load_kbps))
		pr_err("bus scale failed\n");
	if (vpu_clock_scale(hal->clk_handle, pwr_mode))
		pr_err("clock scale failed\n");
	mutex_unlock(&hal->pw_lock);

	/* send the configuration commit through IPC */
	rc = ipc_cmd_config_session_commit(sid, ipc_ct);

	pr_debug("return (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_session_s_input_params(u32 sid, u32 port_id,
		const struct vpu_prop_session_input *inparam)
{
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	return ipc_cmd_set_session_prop(sid, VPU_PROP_SESSION_INPUT, port_id,
				(void *)inparam, sizeof(*inparam));
}

int vpu_hw_session_s_output_params(u32 sid, u32 port_id,
		const struct vpu_prop_session_output *outparam)
{
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	return ipc_cmd_set_session_prop(sid, VPU_PROP_SESSION_OUTPUT, port_id,
				(void *)outparam, sizeof(*outparam));
}

int vpu_hw_session_g_input_params(u32 sid, u32 port_id,
		struct vpu_prop_session_input *inp)
{
	int rc;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid) || !inp)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	/* send the synchronous command (blocking call) */
	rc = ipc_cmd_sync_get_session_prop(sid, VPU_PROP_SESSION_INPUT,
			port_id, (u8 *)inp, sizeof(*inp));
	if (unlikely(rc))
		pr_err("Error while getting input param property\n");

	pr_debug("Return (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_session_g_output_params(u32 sid, u32 port_id,
		struct vpu_prop_session_output *outp)
{
	int rc;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid) || !outp)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	/* send the synchronous command (blocking call) */
	rc = ipc_cmd_sync_get_session_prop(sid, VPU_PROP_SESSION_OUTPUT,
			port_id, (u8 *)outp, sizeof(*outp));
	if (unlikely(rc))
		pr_err("Error while getting output param property\n");

	pr_debug("Return (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_session_nr_buffer_config(u32 sid, u32 in_addr, u32 out_addr)
{
	struct vpu_prop_session_noise_reduction_config nr_conf_pkt;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid) || !in_addr || !out_addr)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	memset(&nr_conf_pkt, 0, sizeof(nr_conf_pkt));
	nr_conf_pkt.in_buf_addr = in_addr;
	nr_conf_pkt.out_buf_addr = out_addr;
	nr_conf_pkt.release_flag = false;

	return ipc_cmd_set_session_prop(sid,
		VPU_PROP_SESSION_NOISE_REDUCTION_CONFIG, VPU_IPC_PORT_UNUSED,
		(void *)&nr_conf_pkt, sizeof(nr_conf_pkt));
}

int vpu_hw_session_nr_buffer_release(u32 sid)
{
	struct vpu_prop_session_noise_reduction_config nr_conf_pkt;
	int cid = SID2CID(sid);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid))) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	memset(&nr_conf_pkt, 0, sizeof(nr_conf_pkt));
	nr_conf_pkt.in_buf_addr = 0L;
	nr_conf_pkt.out_buf_addr = 0L;
	nr_conf_pkt.release_flag = true;

	return ipc_cmd_set_session_prop(sid,
		VPU_PROP_SESSION_NOISE_REDUCTION_CONFIG, VPU_IPC_PORT_UNUSED,
		(void *)&nr_conf_pkt, sizeof(nr_conf_pkt));
}

int vpu_hw_session_s_property(u32 sid, u32 prop_id, void *data, u32 data_size)
{
	int cid = SID2CID(sid);
	pr_debug("(prop_id = 0x%08x)", prop_id);

	if (!IS_A_SESSION_CHANNEL(cid) || !data || (data_size == 0)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	/* Send the command for the current property, asynchronously */
	return ipc_cmd_set_session_prop(sid, prop_id, VPU_IPC_PORT_UNUSED,
			data, data_size);
}

int vpu_hw_session_g_property(u32 sid, u32 prop_id, void *data, u32 data_size)
{
	int cid = SID2CID(sid);
	pr_debug("(prop_id = 0x%08x)", prop_id);

	if (unlikely(!IS_A_SESSION_CHANNEL(cid) || !data)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	return ipc_cmd_sync_get_session_prop(sid, prop_id, VPU_IPC_PORT_UNUSED,
			data, data_size);
}

int vpu_hw_session_s_property_ext(u32 sid,
		void __user *data, u32 data_size)
{
	int rc;
	u8 temp_buf[VPU_MAX_EXT_DATA_SIZE];

	if (data_size > VPU_MAX_EXT_DATA_SIZE)
		return -EINVAL;

	rc = copy_from_user((void *)temp_buf, data, data_size);
	if (!rc)
		return vpu_hw_session_s_property(sid,
				VPU_PROP_SESSION_GENERIC,
				temp_buf, data_size);
	else
		return -EFAULT;
}

int vpu_hw_session_g_property_ext(u32 sid,
		void __user *data, u32 data_size,
		void __user *buf, u32 buf_size)
{
	struct vpu_ipc_cmd_session_get_property_packet packet;
	struct vpu_sync_transact *ptrans;
	int cid = SID2CID(sid);
	int rc;
	u8 temp_buf[VPU_MAX_EXT_DATA_SIZE];

	if (data_size > VPU_MAX_EXT_DATA_SIZE)
		return -EINVAL;

	rc = copy_from_user((void *)temp_buf, data, data_size);
	if (rc)
		return -EFAULT;

	/* prepare the hdr */
	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet) + data_size;
	packet.hdr.cmd_id = VPU_IPC_CMD_SESSION_GET_PROPERTY;
	packet.hdr.sid = sid;

	/* generic prop */
	packet.prop_id = VPU_PROP_SESSION_GENERIC;
	packet.data_offset = sizeof(packet);
	packet.data_size = data_size;
	packet.reserved = 0;

	ptrans = ipc_cmd_sync_prepare(&packet.hdr, buf, buf_size);

	/* send the synchronous command */
	rc = vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&packet, temp_buf, data_size);
	if (ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, vpu_ipc_timeout, cid);

		/* copy the content of the cache into user buffer */
		if (!rc)
			rc = copy_to_user_from_trans_buffer(ptrans,
					buf, buf_size);

		ipc_cmd_sync_release(ptrans);
	}

	pr_debug("Return (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_session_cmd_ext(u32 sid, u32 cmd,
		void *data, u32 data_size)
{
	int rc = 0;
	int cid = SID2CID(sid);
	struct vpu_ipc_cmd_header_packet hdr;

	/* send the generic command */
	memset(&hdr, 0, sizeof(hdr));
	hdr.sid = sid;
	hdr.size = sizeof(hdr) + data_size;
	hdr.cmd_id = VPU_IPC_CMD_SESSION_GENERIC;

	rc = vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&hdr,
			(u8 *)data, data_size);

	return rc;
}

static inline void raw_init_channel(struct vpu_channel *ch, u32 cid)
{
	mutex_init(&ch->chlock);

	if (IS_A_SESSION_CHANNEL(cid))
		ch->open_count = 0;
}

/* static initialization, called during system bootup */
int vpu_hw_sys_init(struct vpu_platform_resources *res)
{
	u32 i;
	void *clkh;
	int rc;

	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	/* init vpu channel hal */
	ch_hal->res_orig = res;
	mutex_init(&ch_hal->hal_lock);
	mutex_init(&ch_hal->pw_lock);
	ch_hal->upcount = 0;

	/* powered off initially */
	ch_hal->mode = VPU_OFF;

	/* fw logging off initially */
	ch_hal->fw_log_level = VPU_LOGGING_ERROR;

	/* init each channel (system, sessions, logging) */
	for (i = 0; i < MAX_CHANNELS; i++)
		raw_init_channel(&ch_hal->channels[i], i);

	/* init all the sync object */
	init_transact(ch_hal);

	/* init gdsc */
	ch_hal->vdd_enabled = false;
	ch_hal->vdd = devm_regulator_get(&res->pdev->dev, "vdd");
	if (IS_ERR(ch_hal->vdd)) {
		pr_err("failed to init gdsc\n");
		rc = -EINVAL;
		goto err_deinit_vpu_channel;
	}

	/* init vpu clock */
	clkh = vpu_clock_init(res);
	if (likely(clkh)) {
		ch_hal->clk_handle = clkh;
	} else {
		pr_err("failed to init clock\n");
		rc = -EINVAL;
		goto err_deinit_vpu_channel;
	}

	/* init vpu bus */
	rc = vpu_bus_init(res);
	if (unlikely(rc)) {
		pr_err("failed to init vpu bus\n");
		goto err_deinit_clock;
	}

	/* HFI init and fw loading */
	rc = vpu_hfi_init(res);
	if (unlikely(rc)) {
		pr_err("failed to init HFI\n");
		goto err_deinit_bus;
	}

	ch_hal->power_workq = alloc_ordered_workqueue("vpu_power_workq", 0);
	if (unlikely(!ch_hal->power_workq)) {
		pr_err("create power workq failed\n");
		rc = -ENOMEM;
		goto workq_fail;
	}
	INIT_WORK(&ch_hal->boot_work, vpu_boot_work_handler);
	INIT_DELAYED_WORK(&ch_hal->shutdown_work,
			vpu_shutdown_work_handler);

	return 0;

workq_fail:
	vpu_hfi_deinit();
err_deinit_bus:
	vpu_bus_deinit();
err_deinit_clock:
	vpu_clock_deinit(ch_hal->clk_handle);
	ch_hal->clk_handle = NULL;
err_deinit_vpu_channel:
	ch_hal->vdd = NULL;

	pr_debug("Return error (rc=%d)\n", rc);
	return rc;
}

#define RPM_MISC_REQ_TYPE	0x6373696d
#define RPM_MISC_REQUEST_VPU	0x757076

static void inform_rpm_vpu_state(u32 on)
{
	int rc, value = on;

	struct msm_rpm_kvp kvp = {
		.key = RPM_MISC_REQUEST_VPU,
		.data = (void *)&value,
		.length = sizeof(value),
	};

	rc = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
			RPM_MISC_REQ_TYPE, 0, &kvp, 1);
	if (rc < 0)
		pr_err("failed to inform RPM! (err = %d)\n", rc);
}

/*
 * power up VPU HW.
 * caller need to hold the hal->pw_lock when call this function
 */
static int vpu_hw_power_on(struct vpu_channel_hal *hal)
{
	int rc;

	/* inform RPM VPU state is ON */
	inform_rpm_vpu_state(1);

	/* enable the power */
	if (!hal->vdd_enabled) {
		rc = regulator_enable(hal->vdd);
		if (rc) {
			pr_err("failed to enable gdsc\n");
			goto err_power;
		}
		hal->vdd_enabled = true;
	}

	/* bus request */
	rc = vpu_bus_vote();
	if (rc) {
		pr_err("failed to request bus bandwidth\n");
		goto err_bus;
	}

	/* enable the VPU clocks */
	rc = vpu_clock_enable(hal->clk_handle, CLOCK_BOOT);
	if (unlikely(rc)) {
		pr_err("failed to enable clock\n");
		goto err_clock;
	}
	return 0;

err_clock:
	vpu_bus_unvote();
err_bus:
	regulator_disable(hal->vdd);
	hal->vdd_enabled = false;
err_power:
	inform_rpm_vpu_state(0);
	return rc;
}

/*
 * power off VPU HW.
 * caller need to hold the hal->pw_lock when call this function
 */
static void vpu_hw_power_off(struct vpu_channel_hal *hal)
{
	vpu_clock_disable(hal->clk_handle, CLOCK_ALL_GROUPS);
	vpu_bus_unvote();
	if (hal->vdd_enabled) {
		regulator_disable(hal->vdd);
		hal->vdd_enabled = false;
	}

	inform_rpm_vpu_state(0);
}

int vpu_hw_sys_suspend(void)
{
	int rc = 0;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	mutex_lock(&ch_hal->pw_lock);

	if (!ch_hal->vdd_enabled)
		goto suspend_exit;

	/* TODO: Send suspend IPC command once it is defined	*/

	/* make sure clock are on */
	rc = vpu_clock_enable(ch_hal->clk_handle, CLOCK_BOOT);
	if (rc) {
		pr_err("clock off when trying to suspend\n");
		goto suspend_exit;
	}

	/* TODO: SCM call to suspend VPU */

	/* shut off clock and power */
	vpu_hw_power_off(ch_hal);

suspend_exit:
	mutex_unlock(&ch_hal->pw_lock);
	return rc;
}

int vpu_hw_sys_resume(void)
{
	int rc = 0;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	mutex_lock(&ch_hal->pw_lock);

	/* enable power and clock */
	rc = vpu_hw_power_on(ch_hal);
	if (rc) {
		pr_err("resume fail to enable power or clock\n");
		goto resume_exit;
	}

	/* TODO: SCM call to resume VPU */

resume_exit:
	mutex_unlock(&ch_hal->pw_lock);
	return rc;
}

static void vpu_boot_work_handler(struct work_struct *work)
{
	struct vpu_channel_hal *ch_hal;
	int rc = 0;

	ch_hal = container_of(work, struct vpu_channel_hal, boot_work);

	/* in case VPU is still up */
	if (VPU_IS_UP(ch_hal->mode))
		return;

	mutex_lock(&ch_hal->pw_lock);

	/* power up */
	rc = vpu_hw_power_on(ch_hal);
	if (rc) {
		pr_err("failed to power or clock\n");
		goto powerup_fail;
	}

	rc = attach_vpu_iommus(ch_hal->res_orig);
	if (rc) {
		pr_err("could not attach VPU IOMMUs\n");
		goto err_iommu_attach;
	}

	/* boot up VPU and set callback */
	rc = vpu_hfi_start(chan_handle_msg, chan_handle_event);
	if (unlikely(rc)) {
		pr_err("failed to start HFI\n");
		goto err_hfi_start;
	}

	ch_hal->mode = VPU_ON;

	/* configure firmware logging level on boot up
	 * in order to avoid missing any logs while starting up.
	 */
	vpu_hw_sys_set_log_level(ch_hal->fw_log_level);

	mutex_unlock(&ch_hal->pw_lock);
	return;

err_hfi_start:
	detach_vpu_iommus(ch_hal->res_orig);
err_iommu_attach:
	vpu_hw_power_off(ch_hal);
powerup_fail:
	mutex_unlock(&ch_hal->pw_lock);

	if (likely(ch_hal->callback_event))
		ch_hal->callback_event(SYSTEM_SESSION_ID,
				VPU_HW_EVENT_BOOT_FAIL, rc, ch_hal->priv);
}

/*
 * vpu_hw_sys_start
 * to boot up VPU dynamically
 * grabs the lock governing hal settings, power
 */
int vpu_hw_sys_start(channel_event_handler event_cb,
		channel_buffer_handler buffer_cb, void *priv)
{
	int rc = 0;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	/* make sure we have a valid callback handle system events */
	if (unlikely(!event_cb)) {
		pr_err("NULL param\n");
		return -EINVAL;
	}

	mutex_lock(&ch_hal->hal_lock);
	if (ch_hal->upcount++ == 0) {
		/* cancel any pending shutdown work and wait if needed */
		cancel_delayed_work_sync(&ch_hal->shutdown_work);

		/* register upper layer's callback */
		ch_hal->callback_event = event_cb;
		ch_hal->callback_buffer = buffer_cb;
		ch_hal->priv = priv;

		if (!VPU_IS_UP(ch_hal->mode)) {
			/* reset and enable HFI system and logging channels */
			vpu_hfi_enable(VPU_SYSTEM_CHANNEL_ID, ch_hal);
			vpu_hfi_enable(VPU_LOGGING_CHANNEL_ID, ch_hal);

			/* reset the sync object state */
			reset_transact(ch_hal);

			/* power up VPU */
			queue_work(ch_hal->power_workq, &ch_hal->boot_work);
			pr_debug("boot_work queued\n");
		}
	}
	mutex_unlock(&ch_hal->hal_lock);

	return rc;
}

static void vpu_shutdown_work_handler(struct work_struct *work)
{
	struct vpu_channel_hal *ch_hal;
	struct vpu_ipc_cmd_header_packet hdr;

	ch_hal = container_of((struct delayed_work *)work,
			struct vpu_channel_hal, shutdown_work);

	/* in case VPU is down already */
	if (VPU_IS_DOWN(ch_hal->mode))
		return;

	/* send the shutdown command */
	memset(&hdr, 0, sizeof(hdr));
	hdr.cmd_id = VPU_IPC_CMD_SYS_SHUTDOWN;
	hdr.flags = 0;
	hdr.size = sizeof(hdr);
	hdr.trans_id = 0;
	vpu_hfi_write_packet_commit(VPU_SYSTEM_CHANNEL_ID,
		(struct vpu_hfi_packet *)&hdr);

	mutex_lock(&ch_hal->pw_lock);

	vpu_hfi_stop();
	detach_vpu_iommus(ch_hal->res_orig);
	vpu_hw_power_off(ch_hal);

	/* disable HFI system and logging channels */
	vpu_hfi_disable(VPU_SYSTEM_CHANNEL_ID);
	vpu_hfi_disable(VPU_LOGGING_CHANNEL_ID);

	/* vpu is OFF */
	ch_hal->mode = VPU_OFF;

	/* cancel all blocking operations */
	wakeup_all_transact(ch_hal, -EHOSTDOWN);

	mutex_unlock(&ch_hal->pw_lock);
}

/*
 * vpu_sys_stop
 * to shutdown VPU dynamically
 * grabs the lock governing hal settings, power
 */
void vpu_hw_sys_stop(int forced)
{
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;
	u32 shutdown_delay = 0;

	if (!forced)
		shutdown_delay = vpu_shutdown_delay;

	mutex_lock(&ch_hal->hal_lock);

	if (ch_hal->upcount > 0) {
		/* if boot is in progress, let it finish */
		flush_workqueue(ch_hal->power_workq);

		if (forced)
			ch_hal->upcount = 0; /* force regardless of upcount */
		else
			--ch_hal->upcount;

		if (ch_hal->upcount == 0) {
			/* shutdown VPU */
			queue_delayed_work(ch_hal->power_workq,
					&ch_hal->shutdown_work,
					msecs_to_jiffies(shutdown_delay));

			/* unset system callback */
			ch_hal->callback_event = NULL;
			ch_hal->callback_buffer = NULL;
			ch_hal->priv = NULL;
		}
	} else if (forced) {
		/* shutdown work might be pending, let it run immediately */
		if (cancel_delayed_work(&ch_hal->shutdown_work))
			queue_delayed_work(ch_hal->power_workq,
					&ch_hal->shutdown_work,
					msecs_to_jiffies(shutdown_delay));
	}

	mutex_unlock(&ch_hal->hal_lock);
}

/* static cleanup, called during system shutdown */
void vpu_hw_sys_cleanup(void)
{
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	destroy_workqueue(ch_hal->power_workq);

	vpu_hfi_deinit();

	vpu_bus_deinit();

	vpu_clock_deinit(ch_hal->clk_handle);
	ch_hal->clk_handle = NULL;
}

int vpu_hw_sys_cmd_ext(enum vpu_sys_cmd_ext cmd,
		void *data, u32 data_size)
{
	int rc = 0;
	struct vpu_ipc_cmd_header_packet hdr;

	/* prepare the generic command header */
	memset(&hdr, 0, sizeof(hdr));
	hdr.size = sizeof(hdr);
	hdr.cmd_id = VPU_IPC_CMD_SYS_GENERIC;

	/* send the generic command */
	if (cmd == VPU_SYS_CMD_DEBUG_CRASH) {
		struct vpu_data_pkt gen_data;

		gen_data.size = sizeof(gen_data);
		hdr.size += gen_data.size;
		gen_data.payload[0] = VPU_PROP_SYS_WATCHDOG_TEST;

		rc = vpu_hfi_write_packet_extra_commit(VPU_SYSTEM_CHANNEL_ID,
				(struct vpu_hfi_packet *)&hdr,
				(u8 *)&gen_data, sizeof(gen_data));
	} else {
		hdr.size += data_size;
		rc = vpu_hfi_write_packet_extra_commit(VPU_SYSTEM_CHANNEL_ID,
				(struct vpu_hfi_packet *)&hdr,
				(u8 *)data, data_size);
	}

	return rc;
}

static int ipc_cmd_set_sys_prop(u32 prop_id, void *extra, u32 extra_size)
{
	struct vpu_ipc_cmd_sys_set_property_packet packet;
	int sid = SYSTEM_SESSION_ID;
	int cid = VPU_SYSTEM_CHANNEL_ID;

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet) + extra_size;
	packet.hdr.cmd_id = VPU_IPC_CMD_SYS_SET_PROPERTY;
	packet.hdr.flags = 0; /*no ack */
	packet.hdr.trans_id = 0; /* no sync */
	packet.hdr.sid = sid;

	packet.prop_id = prop_id;
	packet.data_offset = sizeof(packet);
	packet.data_size = extra_size;
	packet.reserved = 0;

	/* send out */
	pr_debug("IPC CMD_SYS_SET_PROPERTY, prop_id=0x%08x\n", packet.prop_id);
	return vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&packet,
			(u8 *)extra, extra_size);
}

int vpu_hw_sys_s_property(u32 prop_id, void *data, u32 data_size)
{
	pr_debug("(prop_id = 0x%08x)", prop_id);

	if (unlikely(!data || !data_size)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	/* send the command for the current system property, asynchronously */
	return ipc_cmd_set_sys_prop(prop_id, data, data_size);
}

static int ipc_cmd_sync_get_sys_prop(u32 prop_id, u8 *rxd, u32 rxd_size)
{
	struct vpu_ipc_cmd_sys_get_property_packet packet;
	struct vpu_sync_transact *ptrans;
	int rc;
	int sid = SYSTEM_SESSION_ID;
	int cid = VPU_SYSTEM_CHANNEL_ID;

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet);
	packet.hdr.cmd_id = VPU_IPC_CMD_SYS_GET_PROPERTY;
	packet.hdr.sid = sid;

	packet.prop_id = prop_id;
	packet.data_offset = 0;
	packet.data_size = 0;
	packet.reserved = 0;

	ptrans = ipc_cmd_sync_prepare(&packet.hdr, rxd, rxd_size);

	/* send synchronous command over system channel */
	rc = vpu_hfi_write_packet_commit(cid, (struct vpu_hfi_packet *)&packet);
	if (ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, vpu_ipc_timeout, cid);

		if (!rc)
			rc = copy_from_trans_buffer(ptrans, rxd, rxd_size);

		ipc_cmd_sync_release(ptrans);
	}

	pr_debug("Return (rc=%d)\n", rc);
	return rc;
}

int vpu_hw_sys_g_property(u32 prop_id, void *buf, u32 buf_size)
{
	pr_debug("(prop_id = 0x%08x)", prop_id);
	if (unlikely(!buf || !buf_size)) {
		pr_err("bad params\n");
		return -EINVAL;
	}

	/* send the command for the current system property, asynchronously */
	return ipc_cmd_sync_get_sys_prop(prop_id, buf, buf_size);
}

int vpu_hw_sys_s_property_ext(void __user *data, u32 data_size)
{
	int rc;
	u8 temp_buf[VPU_MAX_EXT_DATA_SIZE];

	if (data_size > VPU_MAX_EXT_DATA_SIZE)
		return -EINVAL;

	rc = copy_from_user((void *)temp_buf, data, data_size);
	if (!rc)
		return vpu_hw_sys_s_property(VPU_PROP_SYS_GENERIC,
				temp_buf, data_size);
	else
		return -EFAULT;
}

int vpu_hw_sys_g_property_ext(void __user *data, u32 data_size,
		void __user *buf, u32 buf_size)
{
	struct vpu_ipc_cmd_sys_get_property_packet packet;
	struct vpu_sync_transact *ptrans;
	int sid = SYSTEM_SESSION_ID;
	int cid = VPU_SYSTEM_CHANNEL_ID;
	int rc;
	u8 temp_buf[VPU_MAX_EXT_DATA_SIZE];

	if (data_size > VPU_MAX_EXT_DATA_SIZE)
		return -EINVAL;
	rc = copy_from_user((void *)temp_buf, data, data_size);
	if (rc)
		return -EFAULT;

	memset(&packet, 0, sizeof(packet));
	packet.hdr.size = sizeof(packet) + data_size;
	packet.hdr.cmd_id = VPU_IPC_CMD_SYS_GET_PROPERTY;
	packet.hdr.sid = sid;
	/* generic prop */
	packet.prop_id = VPU_PROP_SYS_GENERIC;
	packet.data_offset = sizeof(packet);
	packet.data_size = data_size;
	packet.reserved = 0;

	ptrans = ipc_cmd_sync_prepare(&packet.hdr, buf, buf_size);

	/* send the synchronous command */
	rc = vpu_hfi_write_packet_extra_commit(cid,
			(struct vpu_hfi_packet *)&packet, temp_buf, data_size);
	if (ptrans) {
		if (!rc)
			rc = ipc_cmd_sync_wait(ptrans, vpu_ipc_timeout, cid);

		if (!rc)
			rc = copy_to_user_from_trans_buffer(ptrans,
					buf, buf_size);

		ipc_cmd_sync_release(ptrans);
	}

	pr_debug("Return (rc=%d)\n", rc);
	return rc;
}

#ifdef CONFIG_DEBUG_FS

void vpu_hw_debug_on(void)
{
	/* make the timeout very long */
	vpu_ipc_timeout = VPU_LONG_TIMEOUT_MS;
	vpu_hfi_set_pil_timeout(VPU_LONG_TIMEOUT_MS);
	vpu_hfi_set_watchdog(0);
}

void vpu_hw_debug_off(void)
{
	/* enable timeouts */
	vpu_ipc_timeout = VPU_IPC_DEFAULT_TIMEOUT_MS;
	vpu_hfi_set_pil_timeout(VPU_PIL_DEFAULT_TIMEOUT_MS);
	vpu_hfi_set_watchdog(1);
}

size_t vpu_hw_print_queues(char *buf, size_t buf_size)
{
	return vpu_hfi_print_queues(buf, buf_size);
}

int vpu_hw_write_csr_reg(u32 off, u32 val)
{
	int rc;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	if (VPU_IS_UP(ch_hal->mode))
		rc = vpu_hfi_write_csr_reg(off, val);
	else
		rc = -EIO; /* firmware down */

	return rc;
}

int vpu_hw_dump_csr_regs(char *buf, size_t buf_size)
{
	int rc = 0;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	mutex_lock(&ch_hal->pw_lock);

	if (VPU_IS_UP(ch_hal->mode))
		rc = vpu_hfi_dump_csr_regs(buf, buf_size);

	mutex_unlock(&ch_hal->pw_lock);

	return rc;
}

int vpu_hw_dump_csr_regs_no_lock(char *buf, size_t buf_size)
{
	int rc = 0;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	if (VPU_IS_UP(ch_hal->mode))
		rc = vpu_hfi_dump_csr_regs(buf, buf_size);

	return rc;
}

int vpu_hw_dump_smem_line(char *buf, size_t size, u32 offset)
{
	return vpu_hfi_dump_smem_line(buf, size, offset);
}

int vpu_hw_sys_print_log(char __user *user_buf, char *fmt_buf,
		int buf_size)
{
	int read_data = 0;
	int total_size = 0;
	int bytes_unread;
	struct vpu_ipc_log_header_packet *log_hdr;
	int msg_size;
	int offset = 0;

	/* get the full (unread) logging buffer */
	read_data = vpu_hfi_read_log_data(VPU_LOGGING_CHANNEL_ID, fmt_buf,
			buf_size);

	if (read_data <= 0) {
		pr_debug("no data read from logging queue (%d).\n", read_data);
		return read_data;
	}

	log_hdr = (struct vpu_ipc_log_header_packet *)fmt_buf;
	/* read the local buffer and interpret the data inside */
	do {
		char *log_msg;

		/* point to the next log header packet*/
		if (!log_hdr)
			/* we reach the end of the logging */
			break;

		/* locate the logging message and its size */
		log_msg = (char *)(log_hdr->msg_addr ? log_hdr->msg_addr :
			(u32)(log_hdr + 1));
		msg_size = log_hdr->msg_size;

		/* if too big, shorten log_msg to fit user buffer */
		if ((offset + msg_size) > buf_size) {
			msg_size = buf_size - offset;
			pr_warn("Losing data");
		}

		/* finally concatenate this log msg to the user buf passed in */
		bytes_unread = copy_to_user((char *)(user_buf + offset),
				log_msg, msg_size);
		if (bytes_unread > 0) {
			pr_err("copying to user failed, bytes unread %d\n",
					bytes_unread);
			return total_size;
		}
		offset += msg_size;
		total_size += msg_size;

		/* read the next message */
		log_hdr = (struct vpu_ipc_log_header_packet *)((u32)log_hdr +
				log_hdr->size);

	} while ((u32)log_hdr < ((u32)fmt_buf + read_data - sizeof(*log_hdr))
			&& total_size < buf_size);

	return total_size;
}

int vpu_hw_sys_set_log_level(int log_level)
{
	int ret = 0;
	struct vpu_prop_sys_log_ctrl log_ctrl;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	if (VPU_IS_UP(ch_hal->mode)) {
		log_ctrl.component = LOG_COMPONENT_FW;
		log_ctrl.log_level = log_level;
		ret = vpu_hw_sys_s_property(VPU_PROP_SYS_LOG_CTRL, &log_ctrl,
				sizeof(log_ctrl));
		if (ret) {
			pr_err("Error setting fw log level (err=%d)\n", ret);
			return ret;
		}
	}
	/* If firmware not up yet,
	 * cached value for log level will be sent on boot up.
	 */
	ch_hal->fw_log_level = log_level;
	return ret;
}

int vpu_hw_sys_get_log_level(void)
{
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	return ch_hal->fw_log_level;
}

void vpu_hw_sys_set_power_mode(u32 mode)
{
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	mutex_lock(&ch_hal->pw_lock);
	if (VPU_IS_UP(ch_hal->mode))
		vpu_clock_mode_set(ch_hal->clk_handle, mode);
	mutex_unlock(&ch_hal->pw_lock);
}

u32 vpu_hw_sys_get_power_mode(void)
{
	u32 mode = VPU_POWER_DYNAMIC;
	struct vpu_channel_hal *ch_hal = &g_vpu_ch_hal;

	mutex_lock(&ch_hal->pw_lock);
	if (VPU_IS_UP(ch_hal->mode))
		mode = vpu_clock_mode_get(ch_hal->clk_handle);
	mutex_unlock(&ch_hal->pw_lock);

	return mode;
}

#endif /* CONFIG_DEBUG_FS */
