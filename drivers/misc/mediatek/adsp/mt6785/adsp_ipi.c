/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/mutex.h>

#include <linux/io.h>
#include <linux/ktime.h>

#include "adsp_ipi.h"
#include "adsp_platform.h"
#include "adsp_core.h"
#include "adsp_platform_driver.h"
#include "adsp_excep.h"

#include <adsp_ipi_queue.h>
#include <audio_ipi_platform.h>
#include <audio_messenger_ipi.h>

#include "adsp_reg.h"

/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][IPC] %s(), " fmt "\n", __func__

char *adsp_core_ids[ADSP_CORE_TOTAL] = {"ADSP A"};

#define PRINT_THRESHOLD 10000

struct adsp_ipi_desc adsp_ipi_desc[ADSP_NR_IPI];
struct ipi_ctrl_s adsp_ipi_ctrl;

/*
 * find an ipi handler and invoke it
 */
void adsp_ipi_handler(int irq, void *data, int cid)
{
	struct adsp_priv *pdata = (struct adsp_priv *)data;
	struct ipi_ctrl_s *ctrl;
	struct adsp_share_obj *recv_obj;

	enum adsp_ipi_id ipi_id;
	struct ipi_msg_t *ipi_msg = NULL;
	u8 share_buf[SHARE_BUF_SIZE - 16];
	u32 len;

	ktime_t start_time;
	s64 stop_time;

	start_time = ktime_get();

	if (!pdata) {
		pr_err("%s pdata[%p]\n", __func__, pdata);
		return;
	}
	if (pdata->id >= ADSP_CORE_TOTAL) {
		pr_err("%s core_id[%d] is invalid\n", __func__, pdata->id);
		return;
	}

	ctrl = pdata->ipi_ctrl;
	recv_obj = ctrl->recv_obj;
	ipi_id = recv_obj->id;
	len = recv_obj->len;

	if (ipi_id >= ADSP_NR_IPI || ipi_id < 0)
		pr_debug("[ADSP] A ipi handler id abnormal, id=%d", ipi_id);
	else if (adsp_ipi_desc[ipi_id].handler) {
		memcpy_fromio(share_buf,
			(void *)recv_obj->share_buf, len);

		if (ipi_id == ADSP_IPI_ADSP_A_READY ||
		    ipi_id == ADSP_IPI_LOGGER_INIT) {
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

	stop_time = ktime_us_delta(ktime_get(), start_time);
	if (stop_time > 1000) /* 1 ms */
		pr_notice("IPC ISR %lld us too long!!", stop_time);
}

/*
 * ipi initialize
 */
void adsp_ipi_init(void)
{
	struct adsp_priv *pdata = get_adsp_core_by_id(ADSP_A_ID);
	struct ipi_ctrl_s *ctrl = NULL;
	void __iomem *base = adsp_get_sharedmem_base(pdata, ADSP_SHAREDMEM_IPCBUF);

	pdata->ipi_ctrl = &adsp_ipi_ctrl;
	ctrl = pdata->ipi_ctrl;
	mutex_init(&ctrl->lock);
	ctrl->recv_obj = (struct adsp_share_obj *)base;
	ctrl->send_obj = ctrl->recv_obj + 1;
	pr_debug("[adsp_ipi]ctrl->recv_obj = 0x%p\n", ctrl->recv_obj);
	pr_debug("[adsp_ipi]ctrl->send_obj = 0x%p\n", ctrl->send_obj);
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
	if (id < ADSP_NR_IPI && id >= 0) {
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
	if (id < ADSP_NR_IPI && id >= 0) {
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
				       unsigned int  adsp_id)
{
	struct adsp_priv *pdata = get_adsp_core_by_id(adsp_id);
	struct ipi_ctrl_s *ctrl;
	struct adsp_share_obj *send_obj;
	struct ipi_msg_t *p_ipi_msg = NULL;
	ktime_t start_time;
	s64     time_ipc_us;
	static bool busy_log_flag;
	static u8 share_buf[SHARE_BUF_SIZE - 16];

	if (unlikely(!pdata))
		return -EACCES;
	ctrl = pdata->ipi_ctrl;
	send_obj = ctrl->send_obj;

	if (in_interrupt() && wait) {
		pr_info("adsp_ipi_send: cannot use in isr");
		return ADSP_IPI_ERROR;
	}
	if (is_adsp_ready(pdata->id) != 1) {
		pr_notice("adsp_ipi_send: %s not enabled, id=%d",
			  adsp_core_ids[pdata->id], id);
		return ADSP_IPI_ERROR;
	}

	if (len > sizeof(send_obj->share_buf) || buf == NULL) {
		pr_info("adsp_ipi_send: %s buffer error",
			adsp_core_ids[pdata->id]);
		return ADSP_IPI_ERROR;
	}

	if (mutex_trylock(&ctrl->lock) == 0) {
		pr_info("adsp_ipi_send:%s %d mutex_trylock busy,owner=%d",
			adsp_core_ids[pdata->id], id,
			ctrl->ipi_mutex_owner);
		return ADSP_IPI_BUSY;
	}

	/* get adsp ipi mutex owner */
	ctrl->ipi_mutex_owner = id;

	if (adsp_mt_check_sw_int(pdata->id) > 0) {
		if (busy_log_flag == false) {
			busy_log_flag = true;
			p_ipi_msg = (struct ipi_msg_t *)share_buf;
			if (p_ipi_msg->magic == IPI_MSG_MAGIC_NUMBER)
				DUMP_IPI_MSG("busy. ipc owner", p_ipi_msg);
			else
				pr_info("adsp_ipi_send: %s %d host to adsp busy, ipi last time = %d",
					adsp_core_ids[pdata->id], id,
					ctrl->ipi_owner);
		}
		mutex_unlock(&ctrl->lock);
		return ADSP_IPI_BUSY;
	}
	busy_log_flag = false;

	/* get adsp ipi send owner */
	ctrl->ipi_owner = id;

	memcpy(share_buf, buf, len);
	memcpy_toio((void *)send_obj->share_buf, buf, len);

	send_obj->len = len;
	send_obj->id = id;
	dsb(SY);

	/* send host to adsp ipi */
	adsp_mt_set_sw_int(pdata->id);

	if (wait) {
		start_time = ktime_get();
		while (adsp_mt_check_sw_int(pdata->id) > 0) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 1000) /* 1 ms */
				break;
		}
	}

#ifdef Liang_Check
	if (adsp_awake_unlock(pdata->id) == -1)
		pr_debug("adsp_ipi_send: awake unlock fail");
#endif
	mutex_unlock(&ctrl->lock);

	return ADSP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(adsp_ipi_send);

