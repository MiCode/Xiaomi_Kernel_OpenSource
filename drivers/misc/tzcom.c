/* Qualcomm TrustZone communicator driver
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#define KMSG_COMPONENT "TZCOM"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/android_pmem.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/tzcom.h>
#include <linux/clk.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>
#include "tzcomi.h"

#define TZCOM_DEV "tzcom"

#define TZSCHEDULER_CMD_ID 1 /* CMD id of the trustzone scheduler */

#undef PDEBUG
#define PDEBUG(fmt, args...) pr_debug("%s(%i, %s): " fmt "\n", \
		__func__, current->pid, current->comm, ## args)

#undef PERR
#define PERR(fmt, args...) pr_err("%s(%i, %s): " fmt "\n", \
		__func__, current->pid, current->comm, ## args)

#undef PWARN
#define PWARN(fmt, args...) pr_warning("%s(%i, %s): " fmt "\n", \
		__func__, current->pid, current->comm, ## args)


static uint32_t tzcom_perf_client;
static struct class *driver_class;
static dev_t tzcom_device_no;
static struct cdev tzcom_cdev;
struct ion_client *ion_clnt;
static u8 *sb_in_virt;
static s32 sb_in_phys;
static size_t sb_in_length = 20 * SZ_1K;
static u8 *sb_out_virt;
static s32 sb_out_phys;
static size_t sb_out_length = 20 * SZ_1K;

static void *pil;

static atomic_t svc_instance_ctr = ATOMIC_INIT(0);
static DEFINE_MUTEX(sb_in_lock);
static DEFINE_MUTEX(sb_out_lock);
static DEFINE_MUTEX(send_cmd_lock);
static DEFINE_MUTEX(tzcom_bw_mutex);
static int tzcom_bw_count;
static struct clk *tzcom_bus_clk;
struct tzcom_callback_list {
	struct list_head      list;
	struct tzcom_callback callback;
};

struct tzcom_registered_svc_list {
	struct list_head                 list;
	struct tzcom_register_svc_op_req svc;
	wait_queue_head_t                next_cmd_wq;
	int                              next_cmd_flag;
};

struct tzcom_data_t {
	struct list_head  callback_list_head;
	struct mutex      callback_list_lock;
	struct list_head  registered_svc_list_head;
	spinlock_t        registered_svc_list_lock;
	wait_queue_head_t cont_cmd_wq;
	int               cont_cmd_flag;
	u32               handled_cmd_svc_instance_id;
	int               abort;
	wait_queue_head_t abort_wq;
	atomic_t          ioctl_count;
};

static int tzcom_enable_bus_scaling(void)
{
	int ret = 0;
	if (!tzcom_perf_client)
		return -EINVAL;

	if (IS_ERR_OR_NULL(tzcom_bus_clk))
		return -EINVAL;

	mutex_lock(&tzcom_bw_mutex);
	if (!tzcom_bw_count) {
		ret = msm_bus_scale_client_update_request(
				tzcom_perf_client, 1);
		if (ret) {
			pr_err("Bandwidth request failed (%d)\n", ret);
		} else {
			ret = clk_enable(tzcom_bus_clk);
			if (ret)
				pr_err("Clock enable failed\n");
		}
	}
	if (ret)
		msm_bus_scale_client_update_request(tzcom_perf_client, 0);
	else
		tzcom_bw_count++;
	mutex_unlock(&tzcom_bw_mutex);
	return ret;
}

static void tzcom_disable_bus_scaling(void)
{
	if (!tzcom_perf_client)
		return ;

	if (IS_ERR_OR_NULL(tzcom_bus_clk))
		return ;

	mutex_lock(&tzcom_bw_mutex);
	if (tzcom_bw_count > 0)
		if (tzcom_bw_count-- == 1) {
			msm_bus_scale_client_update_request(tzcom_perf_client,
								0);
			clk_disable(tzcom_bus_clk);
		}
	mutex_unlock(&tzcom_bw_mutex);
}

static int tzcom_scm_call(const void *cmd_buf, size_t cmd_len,
		void *resp_buf, size_t resp_len)
{
	return scm_call(SCM_SVC_TZSCHEDULER, TZSCHEDULER_CMD_ID,
			cmd_buf, cmd_len, resp_buf, resp_len);
}

static s32 tzcom_virt_to_phys(u8 *virt)
{
	if (virt >= sb_in_virt &&
			virt < (sb_in_virt + sb_in_length)) {
		return sb_in_phys + (virt - sb_in_virt);
	} else if (virt >= sb_out_virt &&
			virt < (sb_out_virt + sb_out_length)) {
		return sb_out_phys + (virt - sb_out_virt);
	} else {
		return virt_to_phys(virt);
	}
}

static u8 *tzcom_phys_to_virt(s32 phys)
{
	if (phys >= sb_in_phys &&
			phys < (sb_in_phys + sb_in_length)) {
		return sb_in_virt + (phys - sb_in_phys);
	} else if (phys >= sb_out_phys &&
			phys < (sb_out_phys + sb_out_length)) {
		return sb_out_virt + (phys - sb_out_phys);
	} else {
		return phys_to_virt(phys);
	}
}

static int __tzcom_is_svc_unique(struct tzcom_data_t *data,
		struct tzcom_register_svc_op_req svc)
{
	struct tzcom_registered_svc_list *ptr;
	int unique = 1;
	unsigned long flags;

	spin_lock_irqsave(&data->registered_svc_list_lock, flags);
	list_for_each_entry(ptr, &data->registered_svc_list_head, list) {
		if (ptr->svc.svc_id == svc.svc_id) {
			PERR("Service id: %u is already registered",
					ptr->svc.svc_id);
			unique = 0;
			break;
		} else if (svc.cmd_id_low >= ptr->svc.cmd_id_low &&
				svc.cmd_id_low <= ptr->svc.cmd_id_high) {
			PERR("Cmd id low falls in the range of another"
					"registered service");
			unique = 0;
			break;
		} else if (svc.cmd_id_high >= ptr->svc.cmd_id_low &&
				svc.cmd_id_high <= ptr->svc.cmd_id_high) {
			PERR("Cmd id high falls in the range of another"
					"registered service");
			unique = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&data->registered_svc_list_lock, flags);
	return unique;
}

static int tzcom_register_service(struct tzcom_data_t *data, void __user *argp)
{
	int ret;
	unsigned long flags;
	struct tzcom_register_svc_op_req rcvd_svc;
	struct tzcom_registered_svc_list *new_entry;

	ret = copy_from_user(&rcvd_svc, argp, sizeof(rcvd_svc));

	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}

	PDEBUG("svc_id: %u, cmd_id_low: %u, cmd_id_high: %u",
			rcvd_svc.svc_id, rcvd_svc.cmd_id_low,
			rcvd_svc.cmd_id_high);
	if (!__tzcom_is_svc_unique(data, rcvd_svc)) {
		PERR("Provided service is not unique");
		return -EINVAL;
	}

	rcvd_svc.instance_id = atomic_inc_return(&svc_instance_ctr);

	ret = copy_to_user(argp, &rcvd_svc, sizeof(rcvd_svc));
	if (ret) {
		PERR("copy_to_user failed");
		return ret;
	}

	new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) {
		PERR("kmalloc failed");
		return -ENOMEM;
	}
	memcpy(&new_entry->svc, &rcvd_svc, sizeof(rcvd_svc));
	new_entry->next_cmd_flag = 0;
	init_waitqueue_head(&new_entry->next_cmd_wq);

	spin_lock_irqsave(&data->registered_svc_list_lock, flags);
	list_add_tail(&new_entry->list, &data->registered_svc_list_head);
	spin_unlock_irqrestore(&data->registered_svc_list_lock, flags);


	return ret;
}

static int tzcom_unregister_service(struct tzcom_data_t *data,
		void __user *argp)
{
	int ret = 0;
	unsigned long flags;
	struct tzcom_unregister_svc_op_req req;
	struct tzcom_registered_svc_list *ptr, *next;
	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}

	spin_lock_irqsave(&data->registered_svc_list_lock, flags);
	list_for_each_entry_safe(ptr, next, &data->registered_svc_list_head,
			list) {
		if (req.svc_id == ptr->svc.svc_id &&
				req.instance_id == ptr->svc.instance_id) {
			wake_up_all(&ptr->next_cmd_wq);
			list_del(&ptr->list);
			kfree(ptr);
			spin_unlock_irqrestore(&data->registered_svc_list_lock,
					flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&data->registered_svc_list_lock, flags);

	return -EINVAL;
}

static int __tzcom_is_cont_cmd(struct tzcom_data_t *data)
{
	int ret;
	ret = (data->cont_cmd_flag != 0);
	return ret || data->abort;
}

/**
 *   +---------+                              +-----+       +-----------------+
 *   |  TZCOM  |                              | SCM |       | TZCOM_SCHEDULER |
 *   +----+----+                              +--+--+       +--------+--------+
 *        |                                      |                   |
 *        |        scm_call                      |                   |
 *        |------------------------------------->|                   |
 *        |  cmd_buf = struct tzcom_command {    |                   |
 *        |              cmd_type,               |------------------>|
 * +------+------------- sb_in_cmd_addr,         |                   |
 * |      |              sb_in_cmd_len           |                   |
 * |      |            }                         |                   |
 * |      |   resp_buf = struct tzcom_response { |                   |
 * |                         cmd_status,         |                   |
 * |             +---------- sb_in_rsp_addr,     |                   |
 * |             |           sb_in_rsp_len       |<------------------|
 * |             |         }
 * |             |                            struct tzcom_callback {---------+
 * |             |                                uint32_t cmd_id;            |
 * |             |                                uint32_t sb_out_cb_data_len;|
 * |             +---------------+                uint32_t sb_out_cb_data_off;|
 * |                             |            }                               |
 * |    _________________________|_______________________________             |
 * |    +-----------------------+| +----------------------+                   |
 * +--->+ copy from req.cmd_buf |+>| copy to req.resp_buf |                   |
 *      +-----------------------+  +----------------------+                   |
 *      _________________________________________________________             |
 *                               INPUT SHARED BUFFER                          |
 *   +------------------------------------------------------------------------+
 *   |  _________________________________________________________
 *   |  +---------------------------------------------+
 *   +->| cmd_id | data_len | data_off |   data...    |
 *      +---------------------------------------------+
 *                                     |<------------>|copy to next_cmd.req_buf
 *      _________________________________________________________
 *                              OUTPUT SHARED BUFFER
 */
static int __tzcom_send_cmd(struct tzcom_data_t *data,
			struct tzcom_send_cmd_op_req *req)
{
	int ret = 0;
	unsigned long flags;
	u32 reqd_len_sb_in = 0;
	u32 reqd_len_sb_out = 0;
	struct tzcom_command cmd;
	struct tzcom_response resp;
	struct tzcom_callback *next_callback;
	void *cb_data = NULL;
	struct tzcom_callback_list *new_entry;
	struct tzcom_callback *cb;
	size_t new_entry_len = 0;
	struct tzcom_registered_svc_list *ptr_svc;

	if (req->cmd_buf == NULL || req->resp_buf == NULL) {
		PERR("cmd buffer or response buffer is null");
		return -EINVAL;
	}

	if (req->cmd_len <= 0 || req->resp_len <= 0 ||
		req->cmd_len > sb_in_length || req->resp_len > sb_in_length) {
		PERR("cmd buffer length or "
				"response buffer length not valid");
		return -EINVAL;
	}
	PDEBUG("received cmd_req.req: 0x%p",
				req->cmd_buf);
	PDEBUG("received cmd_req.rsp size: %u, ptr: 0x%p",
			req->resp_len,
			req->resp_buf);

	reqd_len_sb_in = req->cmd_len + req->resp_len;
	if (reqd_len_sb_in > sb_in_length) {
		PDEBUG("Not enough memory to fit cmd_buf and "
				"resp_buf. Required: %u, Available: %u",
				reqd_len_sb_in, sb_in_length);
		return -ENOMEM;
	}

	/* Copy req->cmd_buf to SB in and set
	 * req->resp_buf to SB in + cmd_len
	 */
	mutex_lock(&sb_in_lock);
	PDEBUG("Before memcpy on sb_in");
	memcpy(sb_in_virt, req->cmd_buf, req->cmd_len);
	PDEBUG("After memcpy on sb_in");

	/* cmd_type will always be a new here */
	cmd.cmd_type = TZ_SCHED_CMD_NEW;
	cmd.sb_in_cmd_addr = (u8 *) tzcom_virt_to_phys(sb_in_virt);
	cmd.sb_in_cmd_len = req->cmd_len;

	resp.cmd_status = TZ_SCHED_STATUS_INCOMPLETE;
	resp.sb_in_rsp_addr = (u8 *) tzcom_virt_to_phys(sb_in_virt +
			req->cmd_len);
	resp.sb_in_rsp_len = req->resp_len;

	PDEBUG("before call tzcom_scm_call, cmd_id = : %u", req->cmd_id);
	PDEBUG("before call tzcom_scm_call, sizeof(cmd) = : %u", sizeof(cmd));

	ret = tzcom_scm_call((const void *) &cmd, sizeof(cmd),
			&resp, sizeof(resp));
	mutex_unlock(&sb_in_lock);

	if (ret) {
		PERR("tzcom_scm_call failed with err: %d", ret);
		return ret;
	}

	while (resp.cmd_status != TZ_SCHED_STATUS_COMPLETE) {
		/*
		 * If cmd is incomplete, get the callback cmd out from SB out
		 * and put it on the list
		 */
		PDEBUG("cmd_status is incomplete.");
		next_callback = (struct tzcom_callback *)sb_out_virt;

		mutex_lock(&sb_out_lock);
		reqd_len_sb_out = sizeof(*next_callback)
					+ next_callback->sb_out_cb_data_len;
		if (reqd_len_sb_out > sb_out_length ||
			reqd_len_sb_out < sizeof(*next_callback) ||
			next_callback->sb_out_cb_data_len > sb_out_length) {
			PERR("Incorrect callback data length"
					" Required: %u, Available: %u, Min: %u",
					reqd_len_sb_out, sb_out_length,
					sizeof(*next_callback));
			mutex_unlock(&sb_out_lock);
			return -ENOMEM;
		}

		/* Assumption is cb_data_off is sizeof(tzcom_callback) */
		new_entry_len = sizeof(*new_entry)
					+ next_callback->sb_out_cb_data_len;
		new_entry = kmalloc(new_entry_len, GFP_KERNEL);
		if (!new_entry) {
			PERR("kmalloc failed");
			mutex_unlock(&sb_out_lock);
			return -ENOMEM;
		}

		cb = &new_entry->callback;
		cb->cmd_id = next_callback->cmd_id;
		cb->sb_out_cb_data_len = next_callback->sb_out_cb_data_len;
		cb->sb_out_cb_data_off = sizeof(*cb);

		cb_data = (u8 *)next_callback
				+ next_callback->sb_out_cb_data_off;
		memcpy((u8 *)cb + cb->sb_out_cb_data_off, cb_data,
				next_callback->sb_out_cb_data_len);
		mutex_unlock(&sb_out_lock);

		mutex_lock(&data->callback_list_lock);
		list_add_tail(&new_entry->list, &data->callback_list_head);
		mutex_unlock(&data->callback_list_lock);

		/*
		 * We don't know which service can handle the command. so we
		 * wake up all blocking services and let them figure out if
		 * they can handle the given command.
		 */
		spin_lock_irqsave(&data->registered_svc_list_lock, flags);
		list_for_each_entry(ptr_svc,
				&data->registered_svc_list_head, list) {
			ptr_svc->next_cmd_flag = 1;
			wake_up_interruptible(&ptr_svc->next_cmd_wq);
		}
		spin_unlock_irqrestore(&data->registered_svc_list_lock,
				flags);

		PDEBUG("waking up next_cmd_wq and "
				"waiting for cont_cmd_wq");
		if (wait_event_interruptible(data->cont_cmd_wq,
				__tzcom_is_cont_cmd(data))) {
			PWARN("Interrupted: exiting send_cmd loop");
			return -ERESTARTSYS;
		}

		if (data->abort) {
			PERR("Aborting driver");
			return -ENODEV;
		}
		data->cont_cmd_flag = 0;
		cmd.cmd_type = TZ_SCHED_CMD_PENDING;
		mutex_lock(&sb_in_lock);
		ret = tzcom_scm_call((const void *) &cmd, sizeof(cmd), &resp,
				sizeof(resp));
		mutex_unlock(&sb_in_lock);
		if (ret) {
			PERR("tzcom_scm_call failed with err: %d", ret);
			return ret;
		}
	}

	mutex_lock(&sb_in_lock);
	resp.sb_in_rsp_addr = sb_in_virt + cmd.sb_in_cmd_len;
	resp.sb_in_rsp_len = req->resp_len;
	memcpy(req->resp_buf, resp.sb_in_rsp_addr, resp.sb_in_rsp_len);
	/* Zero out memory for security purpose */
	memset(sb_in_virt, 0, reqd_len_sb_in);
	mutex_unlock(&sb_in_lock);

	return ret;
}


static int tzcom_send_cmd(struct tzcom_data_t *data, void __user *argp)
{
	int ret = 0;
	struct tzcom_send_cmd_op_req req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}
	ret = __tzcom_send_cmd(data, &req);
	if (ret)
		return ret;

	PDEBUG("sending cmd_req->rsp "
			"size: %u, ptr: 0x%p", req.resp_len,
			req.resp_buf);
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		PDEBUG("copy_to_user failed");
		return ret;
	}
	return ret;
}

static int __tzcom_send_cmd_req_clean_up(
			struct tzcom_send_cmd_fd_op_req *req)
{
	char *field;
	uint32_t *update;
	int ret = 0;
	int i = 0;

	for (i = 0; i < MAX_ION_FD; i++) {
		if (req->ifd_data[i].fd != 0) {
			field = (char *)req->cmd_buf +
					req->ifd_data[i].cmd_buf_offset;
			update = (uint32_t *) field;
			*update = 0;
		}
	}
	return ret;
}

static int __tzcom_update_with_phy_addr(
			struct tzcom_send_cmd_fd_op_req *req)
{
	struct ion_handle *ihandle;
	char *field;
	uint32_t *update;
	ion_phys_addr_t pa;
	int ret = 0;
	int i = 0;
	uint32_t length;

	for (i = 0; i < MAX_ION_FD; i++) {
		if (req->ifd_data[i].fd != 0) {
			/* Get the handle of the shared fd */
			ihandle = ion_import_fd(ion_clnt, req->ifd_data[i].fd);
			if (ihandle == NULL) {
				PERR("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = (char *) req->cmd_buf +
						req->ifd_data[i].cmd_buf_offset;
			update = (uint32_t *) field;

			/* Populate the cmd data structure with the phys_addr */
			ret = ion_phys(ion_clnt, ihandle, &pa, &length);
			if (ret)
				return -ENOMEM;

			*update = (uint32_t)pa;
			ion_free(ion_clnt, ihandle);
		}
	}
	return ret;
}

static int tzcom_send_cmd_with_fd(struct tzcom_data_t *data,
					void __user *argp)
{
	int ret = 0;
	struct tzcom_send_cmd_fd_op_req req;
	struct tzcom_send_cmd_op_req send_cmd_req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}

	send_cmd_req.cmd_id = req.cmd_id;
	send_cmd_req.cmd_buf = req.cmd_buf;
	send_cmd_req.cmd_len = req.cmd_len;
	send_cmd_req.resp_buf = req.resp_buf;
	send_cmd_req.resp_len = req.resp_len;

	ret = __tzcom_update_with_phy_addr(&req);
	if (ret)
		return ret;
	ret = __tzcom_send_cmd(data, &send_cmd_req);
	__tzcom_send_cmd_req_clean_up(&req);

	if (ret)
		return ret;

	PDEBUG("sending cmd_req->rsp "
			"size: %u, ptr: 0x%p", req.resp_len,
			req.resp_buf);
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		PDEBUG("copy_to_user failed");
		return ret;
	}
	return ret;
}

static struct tzcom_registered_svc_list *__tzcom_find_svc(
		struct tzcom_data_t *data,
		uint32_t instance_id)
{
	struct tzcom_registered_svc_list *entry;
	unsigned long flags;

	spin_lock_irqsave(&data->registered_svc_list_lock, flags);
	list_for_each_entry(entry,
			&data->registered_svc_list_head, list) {
		if (entry->svc.instance_id == instance_id)
			break;
	}
	spin_unlock_irqrestore(&data->registered_svc_list_lock, flags);

	return entry;
}

static int __tzcom_copy_cmd(struct tzcom_data_t *data,
		struct tzcom_next_cmd_op_req *req,
		struct tzcom_registered_svc_list *ptr_svc)
{
	int found = 0;
	int ret = -EAGAIN;
	struct tzcom_callback_list *entry, *next;
	struct tzcom_callback *cb;

	PDEBUG("In here");
	mutex_lock(&data->callback_list_lock);
	PDEBUG("Before looping through cmd and svc lists.");
	list_for_each_entry_safe(entry, next, &data->callback_list_head, list) {
		cb = &entry->callback;
		if (req->svc_id == ptr_svc->svc.svc_id &&
			req->instance_id == ptr_svc->svc.instance_id &&
			cb->cmd_id >= ptr_svc->svc.cmd_id_low &&
			cb->cmd_id <= ptr_svc->svc.cmd_id_high) {
			PDEBUG("Found matching entry");
			found = 1;
			if (cb->sb_out_cb_data_len <= req->req_len) {
				PDEBUG("copying cmd buffer %p to req "
					"buffer %p, length: %u",
					(u8 *)cb + cb->sb_out_cb_data_off,
					req->req_buf, cb->sb_out_cb_data_len);
				req->cmd_id = cb->cmd_id;
				ret = copy_to_user(req->req_buf,
					(u8 *)cb + cb->sb_out_cb_data_off,
					cb->sb_out_cb_data_len);
				if (ret) {
					PERR("copy_to_user failed");
					break;
				}
				list_del(&entry->list);
				kfree(entry);
				ret = 0;
			} else {
				PERR("callback data buffer is "
					"larger than provided buffer."
					"Required: %u, Provided: %u",
					cb->sb_out_cb_data_len,
					req->req_len);
				ret = -ENOMEM;
			}
			break;
		}
	}
	PDEBUG("After looping through cmd and svc lists.");
	mutex_unlock(&data->callback_list_lock);
	return ret;
}

static int __tzcom_is_next_cmd(struct tzcom_data_t *data,
		struct tzcom_registered_svc_list *svc)
{
	int ret;
	ret = (svc->next_cmd_flag != 0);
	return ret || data->abort;
}

static int tzcom_read_next_cmd(struct tzcom_data_t *data, void __user *argp)
{
	int ret = 0;
	struct tzcom_next_cmd_op_req req;
	struct tzcom_registered_svc_list *this_svc;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}

	if (req.instance_id > atomic_read(&svc_instance_ctr)) {
		PERR("Invalid instance_id for the request");
		return -EINVAL;
	}

	if (!req.req_buf || req.req_len == 0) {
		PERR("Invalid request buffer or buffer length");
		return -EINVAL;
	}

	PDEBUG("Before next_cmd loop");
	this_svc = __tzcom_find_svc(data, req.instance_id);

	while (1) {
		PDEBUG("Before wait_event next_cmd.");
		if (wait_event_interruptible(this_svc->next_cmd_wq,
				__tzcom_is_next_cmd(data, this_svc))) {
			PWARN("Interrupted: exiting wait_next_cmd loop");
			/* woken up for different reason */
			return -ERESTARTSYS;
		}

		if (data->abort) {
			PERR("Aborting driver");
			return -ENODEV;
		}
		PDEBUG("After wait_event next_cmd.");
		this_svc->next_cmd_flag = 0;

		ret = __tzcom_copy_cmd(data, &req, this_svc);
		if (ret == 0) {
			PDEBUG("Successfully found svc for cmd");
			data->handled_cmd_svc_instance_id = req.instance_id;
			break;
		} else if (ret == -ENOMEM) {
			PERR("Not enough memory");
			return ret;
		}
	}
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		PERR("copy_to_user failed");
		return ret;
	}
	PDEBUG("copy_to_user is done.");
	return ret;
}

static int tzcom_cont_cmd(struct tzcom_data_t *data, void __user *argp)
{
	int ret = 0;
	struct tzcom_cont_cmd_op_req req;
	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		PERR("copy_from_user failed");
		return ret;
	}

	/*
	 * Only the svc instance that handled the cmd (in read_next_cmd method)
	 * can call continue cmd
	 */
	if (data->handled_cmd_svc_instance_id != req.instance_id) {
		PWARN("Only the service instance that handled the last "
				"callback can continue cmd. "
				"Expected: %u, Received: %u",
				data->handled_cmd_svc_instance_id,
				req.instance_id);
		return -EINVAL;
	}

	if (req.resp_buf) {
		mutex_lock(&sb_out_lock);
		memcpy(sb_out_virt, req.resp_buf, req.resp_len);
		mutex_unlock(&sb_out_lock);
	}

	data->cont_cmd_flag = 1;
	wake_up_interruptible(&data->cont_cmd_wq);
	return ret;
}

static int tzcom_abort(struct tzcom_data_t *data)
{
	int ret = 0;
	unsigned long flags;
	struct tzcom_registered_svc_list *lsvc, *nsvc;
	if (data->abort) {
		PERR("Already aborting");
		return -EINVAL;
	}

	data->abort = 1;

	PDEBUG("Waking up cont_cmd_wq");
	wake_up_all(&data->cont_cmd_wq);

	spin_lock_irqsave(&data->registered_svc_list_lock, flags);
	PDEBUG("Before waking up service wait queues");
	list_for_each_entry_safe(lsvc, nsvc,
			&data->registered_svc_list_head, list) {
		wake_up_all(&lsvc->next_cmd_wq);
	}
	spin_unlock_irqrestore(&data->registered_svc_list_lock, flags);

	PDEBUG("ioctl_count before loop: %d", atomic_read(&data->ioctl_count));
	while (atomic_read(&data->ioctl_count) > 0) {
		if (wait_event_interruptible(data->abort_wq,
				atomic_read(&data->ioctl_count) <= 0)) {
			PERR("Interrupted from abort");
			ret = -ERESTARTSYS;
			break;
		}
	}
	return ret;
}

static long tzcom_ioctl(struct file *file, unsigned cmd,
		unsigned long arg)
{
	int ret = 0;
	struct tzcom_data_t *tzcom_data = file->private_data;
	void __user *argp = (void __user *) arg;
	PDEBUG("enter tzcom_ioctl()");
	if (tzcom_data->abort) {
		PERR("Aborting tzcom driver");
		return -ENODEV;
	}

	switch (cmd) {
	case TZCOM_IOCTL_REGISTER_SERVICE_REQ: {
		PDEBUG("ioctl register_service_req()");
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_register_service(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		if (ret)
			PERR("failed tzcom_register_service: %d", ret);
		break;
	}
	case TZCOM_IOCTL_UNREGISTER_SERVICE_REQ: {
		PDEBUG("ioctl unregister_service_req()");
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_unregister_service(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		if (ret)
			PERR("failed tzcom_unregister_service: %d", ret);
		break;
	}
	case TZCOM_IOCTL_SEND_CMD_REQ: {
		PDEBUG("ioctl send_cmd_req()");
		/* Only one client allowed here at a time */
		mutex_lock(&send_cmd_lock);
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_send_cmd(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		mutex_unlock(&send_cmd_lock);
		if (ret)
			PERR("failed tzcom_send_cmd: %d", ret);
		break;
	}
	case TZCOM_IOCTL_SEND_CMD_FD_REQ: {
		PDEBUG("ioctl send_cmd_req()");
		/* Only one client allowed here at a time */
		mutex_lock(&send_cmd_lock);
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_send_cmd_with_fd(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		mutex_unlock(&send_cmd_lock);
		if (ret)
			PERR("failed tzcom_send_cmd: %d", ret);
		break;
	}
	case TZCOM_IOCTL_READ_NEXT_CMD_REQ: {
		PDEBUG("ioctl read_next_cmd_req()");
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_read_next_cmd(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		if (ret)
			PERR("failed tzcom_read_next: %d", ret);
		break;
	}
	case TZCOM_IOCTL_CONTINUE_CMD_REQ: {
		PDEBUG("ioctl continue_cmd_req()");
		atomic_inc(&tzcom_data->ioctl_count);
		ret = tzcom_cont_cmd(tzcom_data, argp);
		atomic_dec(&tzcom_data->ioctl_count);
		wake_up_interruptible(&tzcom_data->abort_wq);
		if (ret)
			PERR("failed tzcom_cont_cmd: %d", ret);
		break;
	}
	case TZCOM_IOCTL_ABORT_REQ: {
		PDEBUG("ioctl abort_req()");
		ret = tzcom_abort(tzcom_data);
		if (ret)
			PERR("failed tzcom_abort: %d", ret);
		break;
	}
	default:
		return -EINVAL;
	}
	return ret;
}

static int tzcom_open(struct inode *inode, struct file *file)
{
	int ret;
	long pil_error;
	struct tz_pr_init_sb_req_s sb_out_init_req;
	struct tz_pr_init_sb_rsp_s sb_out_init_rsp;
	void *rsp_addr_virt;
	struct tzcom_command cmd;
	struct tzcom_response resp;
	struct tzcom_data_t *tzcom_data;

	PDEBUG("In here");

	ret = tzcom_enable_bus_scaling();

	if (pil == NULL) {
		pil = pil_get("tzapps");
		if (IS_ERR(pil)) {
			PERR("Playready PIL image load failed");
			pil_error = PTR_ERR(pil);
			pil = NULL;
			return pil_error;
		}
		PDEBUG("tzapps image loaded successfully");
	}

	sb_out_init_req.pr_cmd = TZ_SCHED_CMD_ID_INIT_SB_OUT;
	sb_out_init_req.sb_len = sb_out_length;
	sb_out_init_req.sb_ptr = tzcom_virt_to_phys(sb_out_virt);
	PDEBUG("sb_out_init_req { pr_cmd: %d, sb_len: %u, "
			"sb_ptr (phys): 0x%x }",
			sb_out_init_req.pr_cmd,
			sb_out_init_req.sb_len,
			sb_out_init_req.sb_ptr);

	mutex_lock(&sb_in_lock);
	PDEBUG("Before memcpy on sb_in");
	memcpy(sb_in_virt, &sb_out_init_req, sizeof(sb_out_init_req));
	PDEBUG("After memcpy on sb_in");

	/* It will always be a new cmd from this method */
	cmd.cmd_type = TZ_SCHED_CMD_NEW;
	cmd.sb_in_cmd_addr = (u8 *) tzcom_virt_to_phys(sb_in_virt);
	cmd.sb_in_cmd_len = sizeof(sb_out_init_req);
	PDEBUG("tzcom_command { cmd_type: %u, sb_in_cmd_addr: %p, "
			"sb_in_cmd_len: %u }",
			cmd.cmd_type, cmd.sb_in_cmd_addr, cmd.sb_in_cmd_len);

	resp.cmd_status = TZ_SCHED_STATUS_INCOMPLETE;

	PDEBUG("Before scm_call for sb_init");
	ret = tzcom_scm_call(&cmd, sizeof(cmd), &resp, sizeof(resp));
	if (ret) {
		PERR("tzcom_scm_call failed with err: %d", ret);
		return ret;
	}
	PDEBUG("After scm_call for sb_init");

	PDEBUG("tzcom_response after scm cmd_status: %u", resp.cmd_status);
	if (resp.cmd_status == TZ_SCHED_STATUS_COMPLETE) {
		resp.sb_in_rsp_addr = (u8 *)cmd.sb_in_cmd_addr +
				cmd.sb_in_cmd_len;
		resp.sb_in_rsp_len = sizeof(sb_out_init_rsp);
		PDEBUG("tzcom_response sb_in_rsp_addr: %p, sb_in_rsp_len: %u",
				resp.sb_in_rsp_addr, resp.sb_in_rsp_len);
		rsp_addr_virt = tzcom_phys_to_virt((unsigned long)
				resp.sb_in_rsp_addr);
		PDEBUG("Received response phys: %p, virt: %p",
				resp.sb_in_rsp_addr, rsp_addr_virt);
		memcpy(&sb_out_init_rsp, rsp_addr_virt, resp.sb_in_rsp_len);
	} else {
		PERR("Error with SB initialization");
		mutex_unlock(&sb_in_lock);
		return -EPERM;
	}
	mutex_unlock(&sb_in_lock);

	PDEBUG("sb_out_init_rsp { pr_cmd: %d, ret: %d }",
			sb_out_init_rsp.pr_cmd, sb_out_init_rsp.ret);

	if (sb_out_init_rsp.ret) {
		PERR("sb_out_init_req failed: %d", sb_out_init_rsp.ret);
		return -EPERM;
	}

	tzcom_data = kmalloc(sizeof(*tzcom_data), GFP_KERNEL);
	if (!tzcom_data) {
		PERR("kmalloc failed");
		return -ENOMEM;
	}
	file->private_data = tzcom_data;

	INIT_LIST_HEAD(&tzcom_data->callback_list_head);
	mutex_init(&tzcom_data->callback_list_lock);

	INIT_LIST_HEAD(&tzcom_data->registered_svc_list_head);
	spin_lock_init(&tzcom_data->registered_svc_list_lock);

	init_waitqueue_head(&tzcom_data->cont_cmd_wq);
	tzcom_data->cont_cmd_flag = 0;
	tzcom_data->handled_cmd_svc_instance_id = 0;
	tzcom_data->abort = 0;
	init_waitqueue_head(&tzcom_data->abort_wq);
	atomic_set(&tzcom_data->ioctl_count, 0);
	return 0;
}

static int tzcom_release(struct inode *inode, struct file *file)
{
	struct tzcom_data_t *tzcom_data = file->private_data;
	struct tzcom_callback_list *lcb, *ncb;
	struct tzcom_registered_svc_list *lsvc, *nsvc;
	unsigned long flags;
	PDEBUG("In here");

	if (!tzcom_data->abort) {
		PDEBUG("Calling abort");
		tzcom_abort(tzcom_data);
	}

	PDEBUG("Before removing callback list");
	mutex_lock(&tzcom_data->callback_list_lock);
	list_for_each_entry_safe(lcb, ncb,
			&tzcom_data->callback_list_head, list) {
		list_del(&lcb->list);
		kfree(lcb);
	}
	mutex_unlock(&tzcom_data->callback_list_lock);
	PDEBUG("After removing callback list");

	PDEBUG("Before removing svc list");
	spin_lock_irqsave(&tzcom_data->registered_svc_list_lock, flags);
	list_for_each_entry_safe(lsvc, nsvc,
			&tzcom_data->registered_svc_list_head, list) {
		list_del(&lsvc->list);
		kfree(lsvc);
	}
	spin_unlock_irqrestore(&tzcom_data->registered_svc_list_lock, flags);
	PDEBUG("After removing svc list");
	if (pil != NULL) {
		pil_put(pil);
		pil = NULL;
	}
	PDEBUG("Freeing tzcom data");
	kfree(tzcom_data);
	tzcom_disable_bus_scaling();
	return 0;
}

static struct msm_bus_paths tzcom_bw_table[] = {
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
			},
		},
		.num_paths = 1,
	},
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
				.ib = (492 * 8) * 1000000UL,
				.ab = (492 * 8) *  100000UL,
			},
		},
		.num_paths = 1,
	},

};

static struct msm_bus_scale_pdata tzcom_bus_pdata = {
	.usecase = tzcom_bw_table,
	.num_usecases = ARRAY_SIZE(tzcom_bw_table),
	.name = "tzcom",
};
static const struct file_operations tzcom_fops = {
		.owner = THIS_MODULE,
		.unlocked_ioctl = tzcom_ioctl,
		.open = tzcom_open,
		.release = tzcom_release
};

static int __init tzcom_init(void)
{
	int rc;
	struct device *class_dev;

	PDEBUG("Hello tzcom");

	rc = alloc_chrdev_region(&tzcom_device_no, 0, 1, TZCOM_DEV);
	if (rc < 0) {
		PERR("alloc_chrdev_region failed %d", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, TZCOM_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		PERR("class_create failed %d", rc);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, tzcom_device_no, NULL,
			TZCOM_DEV);
	if (!class_dev) {
		PERR("class_device_create failed %d", rc);
		rc = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&tzcom_cdev, &tzcom_fops);
	tzcom_cdev.owner = THIS_MODULE;

	rc = cdev_add(&tzcom_cdev, MKDEV(MAJOR(tzcom_device_no), 0), 1);
	if (rc < 0) {
		PERR("cdev_add failed %d", rc);
		goto class_device_destroy;
	}

	sb_in_phys = pmem_kalloc(sb_in_length, PMEM_MEMTYPE_EBI1 |
			PMEM_ALIGNMENT_4K);
	if (IS_ERR((void *)sb_in_phys)) {
		PERR("could not allocte in kernel pmem buffers for sb_in");
		sb_in_phys = 0;
		rc = -ENOMEM;
		goto class_device_destroy;
	}
	PDEBUG("physical_addr for sb_in: 0x%x", sb_in_phys);

	sb_in_virt = (u8 *) ioremap((unsigned long)sb_in_phys,
			sb_in_length);
	if (!sb_in_virt) {
		PERR("Shared buffer IN allocation failed.");
		rc = -ENOMEM;
		goto class_device_destroy;
	}
	PDEBUG("sb_in virt address: %p, phys address: 0x%x",
			sb_in_virt, tzcom_virt_to_phys(sb_in_virt));

	sb_out_phys = pmem_kalloc(sb_out_length, PMEM_MEMTYPE_EBI1 |
			PMEM_ALIGNMENT_4K);
	if (IS_ERR((void *)sb_out_phys)) {
		PERR("could not allocte in kernel pmem buffers for sb_out");
		sb_out_phys = 0;
		rc = -ENOMEM;
		goto class_device_destroy;
	}
	PDEBUG("physical_addr for sb_out: 0x%x", sb_out_phys);

	sb_out_virt = (u8 *) ioremap((unsigned long)sb_out_phys,
			sb_out_length);
	if (!sb_out_virt) {
		PERR("Shared buffer OUT allocation failed.");
		rc = -ENOMEM;
		goto class_device_destroy;
	}
	PDEBUG("sb_out virt address: %p, phys address: 0x%x",
			sb_out_virt, tzcom_virt_to_phys(sb_out_virt));
	ion_clnt = msm_ion_client_create(0x03, "tzcom");
	/* Initialized in tzcom_open */
	pil = NULL;

	tzcom_perf_client = msm_bus_scale_register_client(
					&tzcom_bus_pdata);
	if (!tzcom_perf_client)
		pr_err("Unable to register bus client");

	tzcom_bus_clk = clk_get(class_dev, "bus_clk");
	if (IS_ERR(tzcom_bus_clk)) {
		tzcom_bus_clk = NULL;
	} else  if (tzcom_bus_clk != NULL) {
		pr_debug("Enabled DFAB clock\n");
		clk_set_rate(tzcom_bus_clk, 64000000);
	}
	return 0;

class_device_destroy:
	if (sb_in_virt)
		iounmap(sb_in_virt);
	if (sb_in_phys)
		pmem_kfree(sb_in_phys);
	if (sb_out_virt)
		iounmap(sb_out_virt);
	if (sb_out_phys)
		pmem_kfree(sb_out_phys);
	device_destroy(driver_class, tzcom_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(tzcom_device_no, 1);
	return rc;
}

static void __exit tzcom_exit(void)
{
	PDEBUG("Goodbye tzcom");
	if (sb_in_virt)
		iounmap(sb_in_virt);
	if (sb_in_phys)
		pmem_kfree(sb_in_phys);
	if (sb_out_virt)
		iounmap(sb_out_virt);
	if (sb_out_phys)
		pmem_kfree(sb_out_phys);
	if (pil != NULL) {
		pil_put(pil);
		pil = NULL;
	}
	clk_put(tzcom_bus_clk);
	device_destroy(driver_class, tzcom_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(tzcom_device_no, 1);
	ion_client_destroy(ion_clnt);
}


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sachin Shah <sachins@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm TrustZone Communicator");
MODULE_VERSION("1.00");

module_init(tzcom_init);
module_exit(tzcom_exit);
