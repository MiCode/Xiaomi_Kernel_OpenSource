/*
* Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
*
* Previously licensed under the ISC license by Qualcomm Atheros, Inc.
*
*
* Permission to use, copy, modify, and/or distribute this software for
* any purpose with or without fee is hereby granted, provided that the
* above copyright notice and this permission notice appear in all
* copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
* AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
* TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
* PERFORMANCE OF THIS SOFTWARE.
*/

/*
* This file was originally distributed by Qualcomm Atheros, Inc.
* under proprietary terms before Copyright ownership was assigned
* to the Linux Foundation.
*/

/******************************************************************************
 * wlan_logging_sock_svc.c
 *
 ******************************************************************************/
#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
#include <linux/rtc.h>
#include <vmalloc.h>
#include <wlan_nlink_srv.h>
#include <vos_status.h>
#include <vos_trace.h>
#include <wlan_nlink_common.h>
#include <wlan_logging_sock_svc.h>
#include <vos_types.h>
#include <kthread.h>
#include "vos_memory.h"
#include <linux/ratelimit.h>
#include <asm/arch_timer.h>
#include <vos_utils.h>

#define LOGGING_TRACE(level, args...) \
		VOS_TRACE(VOS_MODULE_ID_SVC, level, ## args)

/* Global variables */

#define ANI_NL_MSG_LOG_TYPE 89
#define ANI_NL_MSG_READY_IND_TYPE 90
#define ANI_NL_MSG_LOG_PKT_TYPE 91
#define ANI_NL_MSG_FW_LOG_PKT_TYPE 92
#define INVALID_PID -1

#define MAX_LOGMSG_LENGTH 4096
#define LOGGER_MGMT_DATA_PKT_POST_MASK   0x001
#define HOST_LOG_POST_MASK   0x002
#define LOGGER_FW_LOG_PKT_POST_MASK   0x003
#define LOGGER_FATAL_EVENT_POST_MASK  0x004

#define LOGGER_MAX_DATA_MGMT_PKT_Q_LEN   (8)
#define LOGGER_MAX_FW_LOG_PKT_Q_LEN   (16)

#define NL_BDCAST_RATELIMIT_INTERVAL (5*HZ)
#define NL_BDCAST_RATELIMIT_BURST    1

/* Qtimer Frequency */
#define QTIMER_FREQ      19200000

static DEFINE_RATELIMIT_STATE(errCnt,		\
		NL_BDCAST_RATELIMIT_INTERVAL,	\
		NL_BDCAST_RATELIMIT_BURST);

struct log_msg {
	struct list_head node;
	unsigned int radio;
	unsigned int index;
	/* indicates the current filled log length in logbuf */
	unsigned int filled_length;
	/*
	 * Buf to hold the log msg
	 * tAniHdr + log
	 */
	char logbuf[MAX_LOGMSG_LENGTH];
};

struct logger_log_complete {
	uint32_t is_fatal;
	uint32_t indicator;
	uint32_t reason_code;
	bool is_report_in_progress;
	bool is_flush_complete;
};

struct wlan_logging {
	/* Log Fatal and ERROR to console */
	bool log_fe_to_console;
	/* Number of buffers to be used for logging */
	int num_buf;
	/* Lock to synchronize access to shared logging resource */
	spinlock_t spin_lock;
	/* Holds the free node which can be used for filling logs */
	struct list_head free_list;
	/* Holds the filled nodes which needs to be indicated to APP */
	struct list_head filled_list;
	/* Points to head of logger pkt queue */
	vos_pkt_t *data_mgmt_pkt_queue;
	/* Holds number of pkts in vos pkt queue */
	unsigned int data_mgmt_pkt_qcnt;
	/* Lock to synchronize of queue/dequeue of pkts in logger pkt queue */
	spinlock_t data_mgmt_pkt_lock;
	/* Points to head of logger fw log pkt queue */
	vos_pkt_t *fw_log_pkt_queue;
	/* Holds number of pkts in fw log vos pkt queue */
	unsigned int fw_log_pkt_qcnt;
	/* Lock to synchronize of queue/dequeue of pkts in fw log pkt queue */
	spinlock_t fw_log_pkt_lock;
	/* Wait queue for Logger thread */
	wait_queue_head_t wait_queue;
	/* Logger thread */
	struct task_struct *thread;
	/* Logging thread sets this variable on exit */
	struct completion   shutdown_comp;
	/* Indicates to logger thread to exit */
	bool exit;
	/* Holds number of dropped logs*/
	unsigned int drop_count;
	/* Holds number of dropped vos pkts*/
	unsigned int pkt_drop_cnt;
	/* Holds number of dropped fw log vos pkts*/
	unsigned int fw_log_pkt_drop_cnt;
	/* current logbuf to which the log will be filled to */
	struct log_msg *pcur_node;
	/* Event flag used for wakeup and post indication*/
	unsigned long event_flag;
	/* Indicates logger thread is activated */
	bool is_active;
	/* data structure for log complete event*/
	struct logger_log_complete log_complete;
	spinlock_t bug_report_lock;
};

static struct wlan_logging gwlan_logging;
static struct log_msg *gplog_msg;

/* PID of the APP to log the message */
static int gapp_pid = INVALID_PID;
static char wlan_logging_ready[] = "WLAN LOGGING READY";

/*
 * Broadcast Logging service ready indication to any Logging application
 * Each netlink message will have a message of type tAniMsgHdr inside.
 */
void wlan_logging_srv_nl_ready_indication(void)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;
	tAniNlHdr *wnl = NULL;
	int payload_len;
	int    err;
	static int rate_limit;

	payload_len = sizeof(tAniHdr) + sizeof(wlan_logging_ready) +
		sizeof(wnl->radio);
	skb = dev_alloc_skb(NLMSG_SPACE(payload_len));
	if (NULL == skb) {
		if (!rate_limit) {
			LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
					"NLINK: skb alloc fail %s", __func__);
		}
		rate_limit = 1;
		return;
	}
	rate_limit = 0;

	nlh = nlmsg_put(skb, 0, 0, ANI_NL_MSG_LOG, payload_len,
			NLM_F_REQUEST);
	if (NULL == nlh) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"%s: nlmsg_put() failed for msg size[%d]",
				__func__, payload_len);
		kfree_skb(skb);
		return;
	}

	wnl = (tAniNlHdr *) nlh;
	wnl->radio = 0;
	wnl->wmsg.type = ANI_NL_MSG_READY_IND_TYPE;
	wnl->wmsg.length = sizeof(wlan_logging_ready);
	memcpy((char*)&wnl->wmsg + sizeof(tAniHdr),
			wlan_logging_ready,
			sizeof(wlan_logging_ready));

	/* sender is in group 1<<0 */
	NETLINK_CB(skb).dst_group = WLAN_NLINK_MCAST_GRP_ID;

	/*multicast the message to all listening processes*/
	err = nl_srv_bcast(skb);
	if (err) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_INFO_LOW,
			"NLINK: Ready Indication Send Fail %s, err %d",
			__func__, err);
	}
	return;
}

/* Utility function to send a netlink message to an application
 * in user space
 */
static int wlan_send_sock_msg_to_app(tAniHdr *wmsg, int radio,
				int src_mod, int pid)
{
	int err = -1;
	int payload_len;
	int tot_msg_len;
	tAniNlHdr *wnl = NULL;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int wmsg_length = wmsg->length;
	static int nlmsg_seq;

	if (radio < 0 || radio > ANI_MAX_RADIOS) {
		pr_err("%s: invalid radio id [%d]",
				__func__, radio);
		return -EINVAL;
	}

	payload_len = wmsg_length + sizeof(wnl->radio);
	tot_msg_len = NLMSG_SPACE(payload_len);
	skb = dev_alloc_skb(tot_msg_len);
	if (skb == NULL) {
		pr_err("%s: dev_alloc_skb() failed for msg size[%d]",
				__func__, tot_msg_len);
		return -ENOMEM;
	}
	nlh = nlmsg_put(skb, pid, nlmsg_seq++, src_mod, payload_len,
		NLM_F_REQUEST);
	if (NULL == nlh) {
		pr_err("%s: nlmsg_put() failed for msg size[%d]",
				__func__, tot_msg_len);
		kfree_skb(skb);
		return -ENOMEM;
	}

	wnl = (tAniNlHdr *) nlh;
	wnl->radio = radio;
	vos_mem_copy(&wnl->wmsg, wmsg, wmsg_length);

	err = nl_srv_ucast(skb, pid, MSG_DONTWAIT);

	return err;
}

static void set_default_logtoapp_log_level(void)
{
	vos_trace_setValue(VOS_MODULE_ID_WDI, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_PE,  VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_ALL,
			VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_PMC, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	vos_trace_setValue(VOS_MODULE_ID_SVC, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
}

static void clear_default_logtoapp_log_level(void)
{
	int module;

	for (module = 0; module < VOS_MODULE_ID_MAX; module++) {
		vos_trace_setValue(module, VOS_TRACE_LEVEL_NONE,
				VOS_FALSE);
		vos_trace_setValue(module, VOS_TRACE_LEVEL_FATAL,
				VOS_TRUE);
		vos_trace_setValue(module, VOS_TRACE_LEVEL_ERROR,
				VOS_TRUE);
	}

	vos_trace_setValue(VOS_MODULE_ID_RSV4, VOS_TRACE_LEVEL_NONE,
			VOS_FALSE);
}

/* Need to call this with spin_lock acquired */
static int wlan_queue_logmsg_for_app(void)
{
	char *ptr;
	int ret = 0;
	ptr = &gwlan_logging.pcur_node->logbuf[sizeof(tAniHdr)];
	ptr[gwlan_logging.pcur_node->filled_length] = '\0';

	*(unsigned short *)(gwlan_logging.pcur_node->logbuf) =
			ANI_NL_MSG_LOG_TYPE;
	*(unsigned short *)(gwlan_logging.pcur_node->logbuf + 2) =
			gwlan_logging.pcur_node->filled_length;
	list_add_tail(&gwlan_logging.pcur_node->node,
			&gwlan_logging.filled_list);

	if (!list_empty(&gwlan_logging.free_list)) {
		/* Get buffer from free list */
		gwlan_logging.pcur_node =
			(struct log_msg *)(gwlan_logging.free_list.next);
		list_del_init(gwlan_logging.free_list.next);
	} else if (!list_empty(&gwlan_logging.filled_list)) {
		/* Get buffer from filled list */
		/* This condition will drop the packet from being
		 * indicated to app
		 */
		gwlan_logging.pcur_node =
			(struct log_msg *)(gwlan_logging.filled_list.next);
		++gwlan_logging.drop_count;
		/* print every 64th drop count */
		if (vos_is_multicast_logging() &&
			(!(gwlan_logging.drop_count % 0x40))) {
			pr_err("%s: drop_count = %u index = %d filled_length = %d\n",
				__func__, gwlan_logging.drop_count,
				gwlan_logging.pcur_node->index,
				gwlan_logging.pcur_node->filled_length);
		}
		list_del_init(gwlan_logging.filled_list.next);
		ret = 1;
	}

	/* Reset the current node values */
	gwlan_logging.pcur_node->filled_length = 0;
	return ret;
}


int wlan_log_to_user(VOS_TRACE_LEVEL log_level, char *to_be_sent, int length)
{
	/* Add the current time stamp */
	char *ptr;
	char tbuf[100];
	int tlen;
	int total_log_len;
	unsigned int *pfilled_length;
	bool wake_up_thread = false;
	unsigned long flags;

	struct timeval tv;
	struct rtc_time tm;
	unsigned long local_time;
        u64 qtimer_ticks;

	if (!vos_is_multicast_logging()) {
		/*
		 * This is to make sure that we print the logs to kmsg console
		 * when no logger app is running. This is also needed to
		 * log the initial messages during loading of driver where even
		 * if app is running it will not be able to
		 * register with driver immediately and start logging all the
		 * messages.
		 */
		pr_err("%s\n", to_be_sent);
	} else {

	/* Format the Log time [hr:min:sec.microsec] */
	do_gettimeofday(&tv);

	/* Convert rtc to local time */
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time_to_tm(local_time, &tm);
        /* Firmware Time Stamp */
        qtimer_ticks =  arch_counter_get_cntpct();

        tlen = snprintf(tbuf, sizeof(tbuf), "[%02d:%02d:%02d.%06lu] [%016llX]"
                        " [%.5s] ", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,
                        qtimer_ticks, current->comm);
	/* 1+1 indicate '\n'+'\0' */
	total_log_len = length + tlen + 1 + 1;

	spin_lock_irqsave(&gwlan_logging.spin_lock, flags);

	// wlan logging svc resources are not yet initialized
	if (!gwlan_logging.pcur_node) {
	    spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);
	    return -EIO;
	}

	pfilled_length = &gwlan_logging.pcur_node->filled_length;

	 /* Check if we can accomodate more log into current node/buffer */
	if (MAX_LOGMSG_LENGTH < (*pfilled_length + sizeof(tAniNlHdr) +
			total_log_len)) {
		wake_up_thread = true;
		wlan_queue_logmsg_for_app();
		pfilled_length = &gwlan_logging.pcur_node->filled_length;
	}

	ptr = &gwlan_logging.pcur_node->logbuf[sizeof(tAniHdr)];

	/* Assumption here is that we receive logs which is always less than
	 * MAX_LOGMSG_LENGTH, where we can accomodate the
	 *   tAniNlHdr + [context][timestamp] + log
	 * VOS_ASSERT if we cannot accomodate the the complete log into
	 * the available buffer.
	 *
	 * Continue and copy logs to the available length and discard the rest.
	 */
	if (MAX_LOGMSG_LENGTH < (sizeof(tAniNlHdr) + total_log_len)) {
		VOS_ASSERT(0);
		total_log_len = MAX_LOGMSG_LENGTH - sizeof(tAniNlHdr) - 2;
	}

	vos_mem_copy(&ptr[*pfilled_length], tbuf, tlen);
	vos_mem_copy(&ptr[*pfilled_length + tlen], to_be_sent,
			min(length, (total_log_len - tlen)));
	*pfilled_length += tlen + min(length, total_log_len - tlen);
	ptr[*pfilled_length] = '\n';
	*pfilled_length += 1;

	spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);

	/* Wakeup logger thread */
	if ((true == wake_up_thread)) {
		/* If there is logger app registered wakeup the logging
                 * thread
                 */

			set_bit(HOST_LOG_POST_MASK, &gwlan_logging.event_flag);
			wake_up_interruptible(&gwlan_logging.wait_queue);
	}

	if (gwlan_logging.log_fe_to_console
		&& ((VOS_TRACE_LEVEL_FATAL == log_level)
		|| (VOS_TRACE_LEVEL_ERROR == log_level))) {
		pr_err("%s %s\n",tbuf, to_be_sent);
	}
	}

	return 0;
}

static int send_fw_log_pkt_to_user(void)
{
	int ret = -1;
	int extra_header_len, nl_payload_len;
	struct sk_buff *skb = NULL;
	static int nlmsg_seq;
	vos_pkt_t *current_pkt;
	vos_pkt_t *next_pkt;
	VOS_STATUS status = VOS_STATUS_E_FAILURE;
	unsigned long flags;

	tAniNlHdr msg_header;

	do {
		spin_lock_irqsave(&gwlan_logging.fw_log_pkt_lock, flags);

		if (!gwlan_logging.fw_log_pkt_queue) {
			spin_unlock_irqrestore(
				&gwlan_logging.fw_log_pkt_lock, flags);
			return -EIO;
		}

		/* pick first pkt from queued chain */
		current_pkt = gwlan_logging.fw_log_pkt_queue;

		/* get the pointer to the next packet in the chain */
		status = vos_pkt_walk_packet_chain(current_pkt, &next_pkt,
							TRUE);

		/* both "success" and "empty" are acceptable results */
		if (!((status == VOS_STATUS_SUCCESS) ||
					(status == VOS_STATUS_E_EMPTY))) {
			++gwlan_logging.fw_log_pkt_drop_cnt;
			spin_unlock_irqrestore(
				&gwlan_logging.fw_log_pkt_lock, flags);
			pr_err("%s: Failure walking packet chain", __func__);
			return -EIO;
		}

		/* update queue head with next pkt ptr which could be NULL */
		gwlan_logging.fw_log_pkt_queue = next_pkt;
		--gwlan_logging.fw_log_pkt_qcnt;
		spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock, flags);

		status = vos_pkt_get_os_packet(current_pkt, (v_VOID_t **)&skb,
						TRUE);
		if (!VOS_IS_STATUS_SUCCESS(status)) {
			++gwlan_logging.fw_log_pkt_drop_cnt;
			pr_err("%s: Failure extracting skb from vos pkt",
				__func__);
			return -EIO;
		}

		/*return vos pkt since skb is already detached */
		vos_pkt_return_packet(current_pkt);

		extra_header_len = sizeof(msg_header.radio) + sizeof(tAniHdr);
		nl_payload_len = NLMSG_ALIGN(extra_header_len + skb->len);

		msg_header.nlh.nlmsg_type = ANI_NL_MSG_LOG;
		msg_header.nlh.nlmsg_len = nl_payload_len;
		msg_header.nlh.nlmsg_flags = NLM_F_REQUEST;
		msg_header.nlh.nlmsg_pid = gapp_pid;
		msg_header.nlh.nlmsg_seq = nlmsg_seq++;

		msg_header.radio = 0;

		msg_header.wmsg.type = ANI_NL_MSG_FW_LOG_PKT_TYPE;
		msg_header.wmsg.length = skb->len;

		if (unlikely(skb_headroom(skb) < sizeof(msg_header))) {
			pr_err("VPKT [%d]: Insufficient headroom, head[%p],"
				" data[%p], req[%zu]", __LINE__, skb->head,
				skb->data, sizeof(msg_header));
			return -EIO;
		}

		vos_mem_copy(skb_push(skb, sizeof(msg_header)), &msg_header,
							sizeof(msg_header));

		ret = nl_srv_bcast(skb);
		if (ret < 0) {
			pr_info("%s: Send Failed %d drop_count = %u\n",
				__func__, ret, ++gwlan_logging.fw_log_pkt_drop_cnt);
		} else {
			ret = 0;
		}

	} while (next_pkt);

	return ret;
}

static int send_data_mgmt_log_pkt_to_user(void)
{
	int ret = -1;
	int extra_header_len, nl_payload_len;
	struct sk_buff *skb = NULL;
	static int nlmsg_seq;
	vos_pkt_t *current_pkt;
	vos_pkt_t *next_pkt;
	VOS_STATUS status = VOS_STATUS_E_FAILURE;
	unsigned long flags;

	tAniNlLogHdr msg_header;

	do {
		spin_lock_irqsave(&gwlan_logging.data_mgmt_pkt_lock, flags);

		if (!gwlan_logging.data_mgmt_pkt_queue) {
			spin_unlock_irqrestore(
				&gwlan_logging.data_mgmt_pkt_lock, flags);
			return -EIO;
		}

		/* pick first pkt from queued chain */
		current_pkt = gwlan_logging.data_mgmt_pkt_queue;

		/* get the pointer to the next packet in the chain */
		status = vos_pkt_walk_packet_chain(current_pkt, &next_pkt,
							TRUE);

		/* both "success" and "empty" are acceptable results */
		if (!((status == VOS_STATUS_SUCCESS) ||
					(status == VOS_STATUS_E_EMPTY))) {
			++gwlan_logging.pkt_drop_cnt;
			spin_unlock_irqrestore(
				&gwlan_logging.data_mgmt_pkt_lock, flags);
			pr_err("%s: Failure walking packet chain", __func__);
			return -EIO;
		}

		/* update queue head with next pkt ptr which could be NULL */
		gwlan_logging.data_mgmt_pkt_queue = next_pkt;
		--gwlan_logging.data_mgmt_pkt_qcnt;
		spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock, flags);

		status = vos_pkt_get_os_packet(current_pkt, (v_VOID_t **)&skb,
						TRUE);
		if (!VOS_IS_STATUS_SUCCESS(status)) {
			++gwlan_logging.pkt_drop_cnt;
			pr_err("%s: Failure extracting skb from vos pkt",
				__func__);
			return -EIO;
		}

		/*return vos pkt since skb is already detached */
		vos_pkt_return_packet(current_pkt);

		extra_header_len = sizeof(msg_header.radio) + sizeof(tAniHdr) +
						sizeof(msg_header.frameSize);
		nl_payload_len = NLMSG_ALIGN(extra_header_len + skb->len);

		msg_header.nlh.nlmsg_type = ANI_NL_MSG_LOG;
		msg_header.nlh.nlmsg_len = nl_payload_len;
		msg_header.nlh.nlmsg_flags = NLM_F_REQUEST;
		msg_header.nlh.nlmsg_pid = 0;
		msg_header.nlh.nlmsg_seq = nlmsg_seq++;

		msg_header.radio = 0;

		msg_header.wmsg.type = ANI_NL_MSG_LOG_PKT_TYPE;
		msg_header.wmsg.length = skb->len + sizeof(uint32);

		msg_header.frameSize = WLAN_MGMT_LOGGING_FRAMESIZE_128BYTES;

		if (unlikely(skb_headroom(skb) < sizeof(msg_header))) {
			pr_err("VPKT [%d]: Insufficient headroom, head[%p],"
				" data[%p], req[%zu]", __LINE__, skb->head,
				skb->data, sizeof(msg_header));
			return -EIO;
		}

		vos_mem_copy(skb_push(skb, sizeof(msg_header)), &msg_header,
							sizeof(msg_header));

		ret =  nl_srv_bcast(skb);
		if (ret < 0) {
			pr_info("%s: Send Failed %d drop_count = %u\n",
				__func__, ret, ++gwlan_logging.pkt_drop_cnt);
		} else {
			ret = 0;
		}

	} while (next_pkt);

	return ret;
}

static int send_filled_buffers_to_user(void)
{
	int ret = -1;
	struct log_msg *plog_msg;
	int payload_len;
	int tot_msg_len;
	tAniNlHdr *wnl;
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;
	static int nlmsg_seq;
	unsigned long flags;
	static int rate_limit;

	while (!list_empty(&gwlan_logging.filled_list)
		&& !gwlan_logging.exit) {

		skb = dev_alloc_skb(MAX_LOGMSG_LENGTH);
		if (skb == NULL) {
			if (!rate_limit) {
				pr_err("%s: dev_alloc_skb() failed for msg size[%d] drop count = %u\n",
					__func__, MAX_LOGMSG_LENGTH,
					gwlan_logging.drop_count);
			}
			rate_limit = 1;
			ret = -ENOMEM;
			break;
		}
		rate_limit = 0;

		spin_lock_irqsave(&gwlan_logging.spin_lock, flags);

		plog_msg = (struct log_msg *)
			(gwlan_logging.filled_list.next);
		list_del_init(gwlan_logging.filled_list.next);
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);
		/* 4 extra bytes for the radio idx */
		payload_len = plog_msg->filled_length +
			sizeof(wnl->radio) + sizeof(tAniHdr);

		tot_msg_len = NLMSG_SPACE(payload_len);
		nlh = nlmsg_put(skb, 0, nlmsg_seq++,
				ANI_NL_MSG_LOG, payload_len,
				NLM_F_REQUEST);
		if (NULL == nlh) {
			spin_lock_irqsave(&gwlan_logging.spin_lock, flags);
			list_add_tail(&plog_msg->node,
				&gwlan_logging.free_list);
			spin_unlock_irqrestore(&gwlan_logging.spin_lock,
							flags);
			pr_err("%s: drop_count = %u\n", __func__,
				++gwlan_logging.drop_count);
			pr_err("%s: nlmsg_put() failed for msg size[%d]\n",
				__func__, tot_msg_len);
			dev_kfree_skb(skb);
			skb = NULL;
			ret = -EINVAL;
			continue;
		}

		wnl = (tAniNlHdr *) nlh;
		wnl->radio = plog_msg->radio;
		vos_mem_copy(&wnl->wmsg, plog_msg->logbuf,
				plog_msg->filled_length +
				sizeof(tAniHdr));

		spin_lock_irqsave(&gwlan_logging.spin_lock, flags);
		list_add_tail(&plog_msg->node,
				&gwlan_logging.free_list);
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);

		ret = nl_srv_bcast(skb);
		if (ret < 0) {
			if (__ratelimit(&errCnt))
			{
			    pr_info("%s: Send Failed %d drop_count = %u\n",
				  __func__, ret, gwlan_logging.drop_count);
			}
			gwlan_logging.drop_count++;
			skb = NULL;
			break;
		} else {
			skb = NULL;
			ret = 0;
		}
	}

	return ret;
}

/**
 * wlan_logging_thread() - The WLAN Logger thread
 * @Arg - pointer to the HDD context
 *
 * This thread logs log message to App registered for the logs.
 */
static int wlan_logging_thread(void *Arg)
{
	int ret_wait_status = 0;
	int ret = 0;

	set_user_nice(current, -2);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	daemonize("wlan_logging_thread");
#endif
	while (!gwlan_logging.exit) {
		ret_wait_status = wait_event_interruptible(
		  gwlan_logging.wait_queue,
		  (test_bit(HOST_LOG_POST_MASK, &gwlan_logging.event_flag) ||
		   gwlan_logging.exit ||
		   test_bit(LOGGER_MGMT_DATA_PKT_POST_MASK,
						&gwlan_logging.event_flag) ||
		   test_bit(LOGGER_FW_LOG_PKT_POST_MASK,
						&gwlan_logging.event_flag) ||
		   test_bit(LOGGER_FATAL_EVENT_POST_MASK,
						&gwlan_logging.event_flag)));

		if (ret_wait_status == -ERESTARTSYS) {
			pr_err("%s: wait_event return -ERESTARTSYS", __func__);
			break;
		}

		if (gwlan_logging.exit) {
		    break;
		}

		if (test_and_clear_bit(HOST_LOG_POST_MASK,
			&gwlan_logging.event_flag)) {
			ret = send_filled_buffers_to_user();
			if (-ENOMEM == ret) {
				msleep(200);
			}
		}

		if (test_and_clear_bit(LOGGER_FW_LOG_PKT_POST_MASK,
			&gwlan_logging.event_flag)) {
			send_fw_log_pkt_to_user();
		}

		if (test_and_clear_bit(LOGGER_MGMT_DATA_PKT_POST_MASK,
			&gwlan_logging.event_flag)) {
			send_data_mgmt_log_pkt_to_user();
		}

		if (test_and_clear_bit(LOGGER_FATAL_EVENT_POST_MASK,
			&gwlan_logging.event_flag)) {
			if (gwlan_logging.log_complete.is_flush_complete == true) {
				gwlan_logging.log_complete.is_flush_complete = false;
				vos_send_fatal_event_done();
			}
			else {
				gwlan_logging.log_complete.is_flush_complete = true;
				set_bit(HOST_LOG_POST_MASK,&gwlan_logging.event_flag);
				set_bit(LOGGER_FW_LOG_PKT_POST_MASK,&gwlan_logging.event_flag);
				set_bit(LOGGER_FATAL_EVENT_POST_MASK,&gwlan_logging.event_flag);
				wake_up_interruptible(&gwlan_logging.wait_queue);
			}
		}
	}

	complete_and_exit(&gwlan_logging.shutdown_comp, 0);

	return 0;
}

/*
 * Process all the Netlink messages from Logger Socket app in user space
 */
static int wlan_logging_proc_sock_rx_msg(struct sk_buff *skb)
{
	tAniNlHdr *wnl;
	int radio;
	int type;
	int ret;

        if (TRUE == vos_isUnloadInProgress())
        {
                pr_info("%s: unload in progress\n",__func__);
                return -ENODEV;
        }

	wnl = (tAniNlHdr *) skb->data;
	radio = wnl->radio;
	type = wnl->nlh.nlmsg_type;

	if (radio < 0 || radio > ANI_MAX_RADIOS) {
		pr_err("%s: invalid radio id [%d]\n",
				__func__, radio);
		return -EINVAL;
	}

	if (gapp_pid != INVALID_PID) {
		if (wnl->nlh.nlmsg_pid > gapp_pid) {
			gapp_pid = wnl->nlh.nlmsg_pid;
		}

		spin_lock_bh(&gwlan_logging.spin_lock);
		if (gwlan_logging.pcur_node->filled_length) {
			wlan_queue_logmsg_for_app();
		}
		spin_unlock_bh(&gwlan_logging.spin_lock);
		set_bit(HOST_LOG_POST_MASK, &gwlan_logging.event_flag);
		wake_up_interruptible(&gwlan_logging.wait_queue);
	} else {
		/* This is to set the default levels (WLAN logging
		 * default values not the VOS trace default) when
		 * logger app is registered for the first time.
		 */
		gapp_pid = wnl->nlh.nlmsg_pid;
	}

	ret = wlan_send_sock_msg_to_app(&wnl->wmsg, 0,
			ANI_NL_MSG_LOG, wnl->nlh.nlmsg_pid);
	if (ret < 0) {
		pr_err("wlan_send_sock_msg_to_app: failed");
	}

	return ret;
}

void wlan_init_log_completion(void)
{
	gwlan_logging.log_complete.indicator = WLAN_LOG_TYPE_NON_FATAL;
	gwlan_logging.log_complete.is_fatal = WLAN_LOG_INDICATOR_UNUSED;
	gwlan_logging.log_complete.is_report_in_progress = false;
	gwlan_logging.log_complete.reason_code = WLAN_LOG_REASON_CODE_UNUSED;

	spin_lock_init(&gwlan_logging.bug_report_lock);
}

int wlan_set_log_completion(uint32 is_fatal,
                            uint32 indicator,
                            uint32 reason_code)
{
	unsigned long flags;

	spin_lock_irqsave(&gwlan_logging.bug_report_lock, flags);
	gwlan_logging.log_complete.indicator = indicator;
	gwlan_logging.log_complete.is_fatal = is_fatal;
	gwlan_logging.log_complete.is_report_in_progress = true;
	gwlan_logging.log_complete.reason_code = reason_code;
	spin_unlock_irqrestore(&gwlan_logging.bug_report_lock, flags);

	return 0;
}
void wlan_get_log_completion(uint32 *is_fatal,
                             uint32 *indicator,
                             uint32 *reason_code)
{
	unsigned long flags;

	spin_lock_irqsave(&gwlan_logging.bug_report_lock, flags);
	*indicator = gwlan_logging.log_complete.indicator;
	*is_fatal = gwlan_logging.log_complete.is_fatal;
	*reason_code = gwlan_logging.log_complete.reason_code;
	gwlan_logging.log_complete.is_report_in_progress = false;

	spin_unlock_irqrestore(&gwlan_logging.bug_report_lock, flags);

}
bool wlan_is_log_report_in_progress(void)
{
	return gwlan_logging.log_complete.is_report_in_progress;
}

void wlan_reset_log_report_in_progress(void)
{
	unsigned long flags;

	spin_lock_irqsave(&gwlan_logging.bug_report_lock, flags);
	gwlan_logging.log_complete.is_report_in_progress = false;
	spin_unlock_irqrestore(&gwlan_logging.bug_report_lock, flags);
}


void wlan_deinit_log_completion(void)
{
	return;
}


int wlan_logging_sock_activate_svc(int log_fe_to_console, int num_buf)
{
	int i = 0;
	unsigned long irq_flag;

	gapp_pid = INVALID_PID;

	gplog_msg = (struct log_msg *) vmalloc(
			num_buf * sizeof(struct log_msg));
	if (!gplog_msg) {
		pr_err("%s: Could not allocate memory\n", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(gplog_msg, (num_buf * sizeof(struct log_msg)));

	gwlan_logging.log_fe_to_console = !!log_fe_to_console;
	gwlan_logging.num_buf = num_buf;

	spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
	INIT_LIST_HEAD(&gwlan_logging.free_list);
	INIT_LIST_HEAD(&gwlan_logging.filled_list);

	for (i = 0; i < num_buf; i++) {
		list_add(&gplog_msg[i].node, &gwlan_logging.free_list);
		gplog_msg[i].index = i;
	}
	gwlan_logging.pcur_node = (struct log_msg *)
		(gwlan_logging.free_list.next);
	list_del_init(gwlan_logging.free_list.next);
	spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);

	init_waitqueue_head(&gwlan_logging.wait_queue);
	gwlan_logging.exit = false;
	clear_bit(HOST_LOG_POST_MASK, &gwlan_logging.event_flag);
	clear_bit(LOGGER_MGMT_DATA_PKT_POST_MASK, &gwlan_logging.event_flag);
	clear_bit(LOGGER_FW_LOG_PKT_POST_MASK, &gwlan_logging.event_flag);
	clear_bit(LOGGER_FATAL_EVENT_POST_MASK, &gwlan_logging.event_flag);
	init_completion(&gwlan_logging.shutdown_comp);
	gwlan_logging.thread = kthread_create(wlan_logging_thread, NULL,
					"wlan_logging_thread");
	if (IS_ERR(gwlan_logging.thread)) {
		pr_err("%s: Could not Create LogMsg Thread Controller",
		       __func__);
		spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
		vfree(gplog_msg);
		gplog_msg = NULL;
		gwlan_logging.pcur_node = NULL;
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);
		return -ENOMEM;
	}
	wake_up_process(gwlan_logging.thread);
	gwlan_logging.is_active = true;

	nl_srv_register(ANI_NL_MSG_LOG, wlan_logging_proc_sock_rx_msg);

	//Broadcast SVC ready message to logging app/s running
	wlan_logging_srv_nl_ready_indication();
	return 0;
}

int wlan_logging_flush_pkt_queue(void)
{
	vos_pkt_t *pkt_queue_head;
	unsigned long flags;

	spin_lock_irqsave(&gwlan_logging.data_mgmt_pkt_lock, flags);
	if (NULL != gwlan_logging.data_mgmt_pkt_queue) {
		pkt_queue_head = gwlan_logging.data_mgmt_pkt_queue;
		gwlan_logging.data_mgmt_pkt_queue = NULL;
		gwlan_logging.pkt_drop_cnt = 0;
		gwlan_logging.data_mgmt_pkt_qcnt = 0;
		spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock,
					flags);
		vos_pkt_return_packet(pkt_queue_head);
	} else {
		spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock,
					flags);
	}

	spin_lock_irqsave(&gwlan_logging.fw_log_pkt_lock, flags);
	if (NULL != gwlan_logging.fw_log_pkt_queue) {
		pkt_queue_head = gwlan_logging.fw_log_pkt_queue;
		gwlan_logging.fw_log_pkt_queue = NULL;
		gwlan_logging.fw_log_pkt_drop_cnt = 0;
		gwlan_logging.fw_log_pkt_qcnt = 0;
		spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock,
					flags);
		vos_pkt_return_packet(pkt_queue_head);
	} else {
		spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock,
					flags);
	}

	return 0;
}

int wlan_logging_sock_deactivate_svc(void)
{
	unsigned long irq_flag;

	if (!gplog_msg)
		return 0;

	nl_srv_unregister(ANI_NL_MSG_LOG, wlan_logging_proc_sock_rx_msg);
	clear_default_logtoapp_log_level();
	gapp_pid = INVALID_PID;

	INIT_COMPLETION(gwlan_logging.shutdown_comp);
	gwlan_logging.exit = true;
	gwlan_logging.is_active = false;
	vos_set_multicast_logging(0);
	wake_up_interruptible(&gwlan_logging.wait_queue);
	wait_for_completion(&gwlan_logging.shutdown_comp);

	spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
	vfree(gplog_msg);
	gplog_msg = NULL;
	gwlan_logging.pcur_node = NULL;
	spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);

	wlan_logging_flush_pkt_queue();

	return 0;
}

int wlan_logging_sock_init_svc(void)
{
	spin_lock_init(&gwlan_logging.spin_lock);
	spin_lock_init(&gwlan_logging.data_mgmt_pkt_lock);
	spin_lock_init(&gwlan_logging.fw_log_pkt_lock);
	gapp_pid = INVALID_PID;
	gwlan_logging.pcur_node = NULL;

	wlan_init_log_completion();

	return 0;
}

int wlan_logging_sock_deinit_svc(void)
{
	gwlan_logging.pcur_node = NULL;
	gapp_pid = INVALID_PID;

	wlan_deinit_log_completion();
	return 0;
}

int wlan_queue_data_mgmt_pkt_for_app(vos_pkt_t *pPacket)
{
	unsigned long flags;
	vos_pkt_t *next_pkt;
	vos_pkt_t *free_pkt;
	VOS_STATUS status = VOS_STATUS_E_FAILURE;

	spin_lock_irqsave(&gwlan_logging.data_mgmt_pkt_lock, flags);
	if (gwlan_logging.data_mgmt_pkt_qcnt >= LOGGER_MAX_DATA_MGMT_PKT_Q_LEN) {
		status = vos_pkt_walk_packet_chain(
			gwlan_logging.data_mgmt_pkt_queue, &next_pkt, TRUE);
		/*both "success" and "empty" are acceptable results*/
		if (!((status == VOS_STATUS_SUCCESS) ||
				(status == VOS_STATUS_E_EMPTY))) {
			++gwlan_logging.pkt_drop_cnt;
			spin_unlock_irqrestore(
				&gwlan_logging.data_mgmt_pkt_lock, flags);
			pr_err("%s: Failure walking packet chain", __func__);
			/*keep returning pkts to avoid low resource cond*/
			vos_pkt_return_packet(pPacket);
			return VOS_STATUS_E_FAILURE;
		}

		free_pkt = gwlan_logging.data_mgmt_pkt_queue;
		gwlan_logging.data_mgmt_pkt_queue = next_pkt;
		/*returning head of pkt queue. latest pkts are important*/
		--gwlan_logging.data_mgmt_pkt_qcnt;
		spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock,
					flags);
		vos_pkt_return_packet(free_pkt);
	} else {
		spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock,
					flags);
	}

	spin_lock_irqsave(&gwlan_logging.data_mgmt_pkt_lock, flags);

	if (gwlan_logging.data_mgmt_pkt_queue) {
		vos_pkt_chain_packet(gwlan_logging.data_mgmt_pkt_queue,
					pPacket, TRUE);
	} else {
		gwlan_logging.data_mgmt_pkt_queue = pPacket;
	}
	++gwlan_logging.data_mgmt_pkt_qcnt;

	spin_unlock_irqrestore(&gwlan_logging.data_mgmt_pkt_lock, flags);

	set_bit(LOGGER_MGMT_DATA_PKT_POST_MASK, &gwlan_logging.event_flag);
	wake_up_interruptible(&gwlan_logging.wait_queue);

	return VOS_STATUS_SUCCESS;
}

/**
 * wlan_logging_set_log_level() - Set the logging level
 *
 * This function is used to set the logging level of host debug messages
 *
 * Return: None
 */
void wlan_logging_set_log_level(void)
{
	set_default_logtoapp_log_level();
}

int wlan_queue_fw_log_pkt_for_app(vos_pkt_t *pPacket)
{
	unsigned long flags;
	vos_pkt_t *next_pkt;
	vos_pkt_t *free_pkt;
	VOS_STATUS status = VOS_STATUS_E_FAILURE;

	spin_lock_irqsave(&gwlan_logging.fw_log_pkt_lock, flags);
	if (gwlan_logging.fw_log_pkt_qcnt >= LOGGER_MAX_FW_LOG_PKT_Q_LEN) {
		status = vos_pkt_walk_packet_chain(
			gwlan_logging.fw_log_pkt_queue, &next_pkt, TRUE);
		/*both "success" and "empty" are acceptable results*/
		if (!((status == VOS_STATUS_SUCCESS) ||
				(status == VOS_STATUS_E_EMPTY))) {
			++gwlan_logging.fw_log_pkt_drop_cnt;
			spin_unlock_irqrestore(
				&gwlan_logging.fw_log_pkt_lock, flags);
			pr_err("%s: Failure walking packet chain", __func__);
			/*keep returning pkts to avoid low resource cond*/
			vos_pkt_return_packet(pPacket);
			return VOS_STATUS_E_FAILURE;
		}

		free_pkt = gwlan_logging.fw_log_pkt_queue;
		gwlan_logging.fw_log_pkt_queue = next_pkt;
		/*returning head of pkt queue. latest pkts are important*/
		--gwlan_logging.fw_log_pkt_qcnt;
		spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock,
					flags);
		vos_pkt_return_packet(free_pkt);
	} else {
		spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock,
					flags);
	}

	spin_lock_irqsave(&gwlan_logging.fw_log_pkt_lock, flags);

	if (gwlan_logging.fw_log_pkt_queue) {
		vos_pkt_chain_packet(gwlan_logging.fw_log_pkt_queue,
					pPacket, TRUE);
	} else {
		gwlan_logging.fw_log_pkt_queue = pPacket;
	}
	++gwlan_logging.fw_log_pkt_qcnt;

	spin_unlock_irqrestore(&gwlan_logging.fw_log_pkt_lock, flags);

	set_bit(LOGGER_FW_LOG_PKT_POST_MASK, &gwlan_logging.event_flag);
	wake_up_interruptible(&gwlan_logging.wait_queue);

	return VOS_STATUS_SUCCESS;
}

int wlan_queue_logpkt_for_app(vos_pkt_t *pPacket, uint32 pkt_type)
{
	VOS_STATUS status = VOS_STATUS_E_FAILURE;

	if (pPacket == NULL) {
		pr_err("%s: Null param", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	if (gwlan_logging.is_active == false) {
		/*return all packets queued*/
		wlan_logging_flush_pkt_queue();

		/*return currently received pkt*/
		vos_pkt_return_packet(pPacket);
		return VOS_STATUS_E_FAILURE;
	}

	switch (pkt_type) {
	case LOG_PKT_TYPE_DATA_MGMT:
		status = wlan_queue_data_mgmt_pkt_for_app(pPacket);
		break;

	case LOG_PKT_TYPE_FW_LOG:
		status = wlan_queue_fw_log_pkt_for_app(pPacket);
		break;

	default:
		pr_info("%s: Unknown pkt received %d", __func__, pkt_type);
		status = VOS_STATUS_E_INVAL;
		break;
	};

	return status;
}

void wlan_process_done_indication(uint8 type, uint32 reason_code)
{
    if ((type == WLAN_QXDM_LOGGING) && (wlan_is_log_report_in_progress() == TRUE))
    {
        pr_info("%s: Setting LOGGER_FATAL_EVENT\n", __func__);
        set_bit(LOGGER_FATAL_EVENT_POST_MASK, &gwlan_logging.event_flag);
        wake_up_interruptible(&gwlan_logging.wait_queue);
    }
}

/**
 * wlan_is_logger_thread()- Check if threadid is
 * of logger thread
 *
 * @threadId: passed threadid
 *
 * This function is called to check if threadid is
 * of logger thread.
 *
 * Return: true if threadid is of logger thread.
 */
bool wlan_is_logger_thread(int threadId)
{
	return ((gwlan_logging.thread) &&
		(threadId == gwlan_logging.thread->pid));
}
#endif /* WLAN_LOGGING_SOCK_SVC_ENABLE */
