// SPDX-License-Identifier: GPL-2.0
//
// adsp_ipi.h --  Mediatek ADSP IPI interface
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/mutex.h>
#include <mt-plat/sync_write.h>

#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include <audio_ipi_platform.h>

#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"

#include <adsp_ipi_queue.h>
#include <audio_messenger_ipi.h>

/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][IPC] %s(), " fmt "\n", __func__

#define PRINT_THRESHOLD 10000
enum adsp_ipi_id adsp_ipi_mutex_owner[ADSP_CORE_TOTAL];
enum adsp_ipi_id adsp_ipi_owner[ADSP_CORE_TOTAL];

struct adsp_ipi_desc adsp_ipi_desc[ADSP_NR_IPI];
struct adsp_share_obj *adsp_send_obj[ADSP_CORE_TOTAL];
struct adsp_share_obj *adsp_rcv_obj[ADSP_CORE_TOTAL];
struct mutex adsp_ipi_mutex[ADSP_CORE_TOTAL];

/*
 * find an ipi handler and invoke it
 */
void adsp_A_ipi_handler(void)
{
	enum adsp_ipi_id ipi_id;
	struct ipi_msg_t *ipi_msg = NULL;
	u8 share_buf[SHARE_BUF_SIZE - 16];
	u32 len;

	ktime_t start_time;
	s64 stop_time;

	start_time = ktime_get();

	ipi_id = adsp_rcv_obj[ADSP_A_ID]->id;
	len = adsp_rcv_obj[ADSP_A_ID]->len;

	if (ipi_id >= ADSP_NR_IPI)
		pr_debug("[ADSP] A ipi handler id abnormal, id=%d", ipi_id);
	else if (adsp_ipi_desc[ipi_id].handler) {
		memcpy_fromio(share_buf,
			(void *)adsp_rcv_obj[ADSP_A_ID]->share_buf, len);

		if (ipi_id == ADSP_IPI_ADSP_A_READY ||
		    ipi_id == ADSP_IPI_LOGGER_INIT_A) {
			/*
			 * adsp_ready & logger init ipi bypass send to ipi
			 * queue and do callback directly. (which will in isr)
			 * Must ensure the callback can do in isr
			 */
			adsp_ipi_desc[ipi_id].handler(ipi_id, share_buf, len);
		} else if (is_scp_ipi_queue_init(AUDIO_OPENDSP_USE_HIFI3_A)) {
			scp_dispatch_ipi_hanlder_to_queue(
				AUDIO_OPENDSP_USE_HIFI3_A,
				ipi_id, share_buf, len,
				adsp_ipi_desc[ipi_id].handler);
		} else {
			ipi_msg = (struct ipi_msg_t *)share_buf;
			if (ipi_msg->magic == IPI_MSG_MAGIC_NUMBER)
				DUMP_IPI_MSG("ipi queue not ready!", ipi_msg);
			else
				pr_info("ipi queue not ready!! opendsp_id: %u, ipi_id: %u, buf: %p, len: %u, ipi_handler: %p",
					AUDIO_OPENDSP_USE_HIFI3_A,
					ipi_id, share_buf, len,
					adsp_ipi_desc[ipi_id].handler);
			WARN_ON(1);
		}
	} else {
		pr_debug("[ADSP] A ipi handler is null or abnormal, id=%d",
			 ipi_id);
	}

	/* ADSP side write 1 to assert SPM wakeup src,
	 * while AP side write 0 to clear wakeup src.
	 */
	writel(0x0, ADSP_TO_SPM_REG);

	stop_time = ktime_us_delta(ktime_get(), start_time);
	if (stop_time > 1000) /* 1 ms */
		pr_notice("IPC ISR %lld us too long!!", stop_time);
}

/*
 * ipi initialize
 */
void adsp_A_ipi_init(void)
{
	mutex_init(&adsp_ipi_mutex[ADSP_A_ID]);
	adsp_rcv_obj[ADSP_A_ID] = ADSP_A_IPC_BUFFER;
	adsp_send_obj[ADSP_A_ID] = adsp_rcv_obj[ADSP_A_ID] + 1;
	pr_debug("adsp_rcv_obj[ADSP_A_ID] = 0x%p", adsp_rcv_obj[ADSP_A_ID]);
	pr_debug("adsp_send_obj[ADSP_A_ID] = 0x%p", adsp_send_obj[ADSP_A_ID]);
}

/*
 * API let apps can register an ipi handler to receive IPI
 * @param id:      IPI ID
 * @param handler:  IPI handler
 * @param name:  IPI name
 */
enum adsp_ipi_status adsp_ipi_registration(
	enum adsp_ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name)
{
	if (id < ADSP_NR_IPI) {
		adsp_ipi_desc[id].name = name;

		if (ipi_handler == NULL)
			return ADSP_IPI_ERROR;

		adsp_ipi_desc[id].handler = ipi_handler;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:      IPI ID
 */
enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id)
{
	if (id < ADSP_NR_IPI) {
		adsp_ipi_desc[id].name = "";
		adsp_ipi_desc[id].handler = NULL;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_unregistration);

/*
 * API for apps to send an IPI to adsp
 * @param id:   IPI ID
 * @param buf:  the pointer of data
 * @param len:  data length
 * @param wait: If true, wait (atomically) until data have been gotten by Host
 * @param len:  data length
 */
enum adsp_ipi_status adsp_ipi_send(enum adsp_ipi_id id, void *buf,
				   unsigned int  len, unsigned int wait,
				   enum adsp_core_id adsp_id)
{
	int retval = 0;
	uint32_t wait_ms = (wait) ? ADSP_IPI_QUEUE_DEFAULT_WAIT_MS : 0;

	/* wait until IPC done */
	retval = scp_send_msg_to_queue(
			 AUDIO_OPENDSP_USE_HIFI3_A, id, buf, len, wait_ms);

	return (retval == 0) ? ADSP_IPI_DONE : ADSP_IPI_ERROR;
}

enum adsp_ipi_status adsp_ipi_send_ipc(enum adsp_ipi_id id, void *buf,
				       unsigned int  len, unsigned int wait,
				       enum adsp_core_id adsp_id)
{
	struct ipi_msg_t *p_ipi_msg = NULL;
	ktime_t start_time;
	s64     time_ipc_us;
	static bool busy_log_flag;
	static u8 share_buf[SHARE_BUF_SIZE - 16];

	if (in_interrupt() && wait) {
		pr_info("adsp_ipi_send: cannot use in isr");
		return ADSP_IPI_ERROR;
	}
	if (is_adsp_ready(adsp_id) != 1) {
		pr_notice("adsp_ipi_send: %s not enabled, id=%d",
			  adsp_core_ids[adsp_id], id);
		return ADSP_IPI_ERROR;
	}

	if (len > sizeof(adsp_send_obj[adsp_id]->share_buf) || buf == NULL) {
		pr_info("adsp_ipi_send: %s buffer error",
			adsp_core_ids[adsp_id]);
		return ADSP_IPI_ERROR;
	}

	if (mutex_trylock(&adsp_ipi_mutex[adsp_id]) == 0) {
		pr_info("adsp_ipi_send:%s %d mutex_trylock busy,owner=%d",
			adsp_core_ids[adsp_id], id,
			adsp_ipi_mutex_owner[adsp_id]);
		return ADSP_IPI_BUSY;
	}

	/* get adsp ipi mutex owner */
	adsp_ipi_mutex_owner[adsp_id] = id;

	if ((readl(ADSP_SWINT_REG) & (1 << adsp_id)) > 0) {
		if (busy_log_flag == false) {
			busy_log_flag = true;
			p_ipi_msg = (struct ipi_msg_t *)share_buf;
			if (p_ipi_msg->magic == IPI_MSG_MAGIC_NUMBER)
				DUMP_IPI_MSG("busy. ipc owner", p_ipi_msg);
			else
				pr_info("adsp_ipi_send: %s %d host to adsp busy, ipi last time = %d",
					adsp_core_ids[adsp_id], id,
					adsp_ipi_owner[adsp_id]);
		}
		mutex_unlock(&adsp_ipi_mutex[adsp_id]);
		return ADSP_IPI_BUSY;
	}
	busy_log_flag = false;

	/* get adsp ipi send owner */
	adsp_ipi_owner[adsp_id] = id;

	memcpy(share_buf, buf, len);
	memcpy_toio((void *)adsp_send_obj[adsp_id]->share_buf, buf, len);

	adsp_send_obj[adsp_id]->len = len;
	adsp_send_obj[adsp_id]->id = id;
	dsb(SY);

	/* send host to adsp ipi */
	writel((1 << adsp_id), ADSP_SWINT_REG);

	if (wait) {
		start_time = ktime_get();
		while ((readl(ADSP_SWINT_REG) & (1 << adsp_id)) > 0) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 1000) /* 1 ms */
				break;
		}
	}

#ifdef Liang_Check
	if (adsp_awake_unlock(adsp_id) == -1)
		pr_debug("adsp_ipi_send: awake unlock fail");
#endif
	mutex_unlock(&adsp_ipi_mutex[adsp_id]);

	return ADSP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(adsp_ipi_send);

