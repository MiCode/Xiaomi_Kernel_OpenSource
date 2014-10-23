/*Qualcomm Secure Execution Environment Communicator (QSEECOM) driver
 *
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "QSEECOM: %s: " fmt, __func__

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
#include <linux/io.h>
#include <linux/msm_ion.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/qseecom.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/freezer.h>
#include <linux/scatterlist.h>
#include <mach/board.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/scm.h>
#include <mach/subsystem_restart.h>
#include <mach/socinfo.h>
#include <mach/qseecomi.h>
#include <asm/cacheflush.h>
#include "qseecom_legacy.h"
#include "qseecom_kernel.h"

#define QSEECOM_DEV			"qseecom"
#define QSEOS_VERSION_14		0x14
#define QSEEE_VERSION_00		0x400000
#define QSEE_VERSION_01			0x401000
#define QSEE_VERSION_02			0x402000
#define QSEE_VERSION_03			0x403000
#define QSEE_VERSION_04			0x404000
#define QSEE_VERSION_05			0x405000



#define QSEOS_CHECK_VERSION_CMD		0x00001803

#define QSEE_CE_CLK_100MHZ		100000000

#define QSEECOM_MAX_SG_ENTRY	512
#define QSEECOM_INVALID_KEY_ID  0xff

/* Save partition image hash for authentication check */
#define	SCM_SAVE_PARTITION_HASH_ID	0x01

/* Check if enterprise security is activate */
#define	SCM_IS_ACTIVATED_ID		0x02

#define RPMB_SERVICE			0x2000

#define QSEECOM_SEND_CMD_CRYPTO_TIMEOUT	2000
#define QSEECOM_LOAD_APP_CRYPTO_TIMEOUT	2000

enum qseecom_clk_definitions {
	CLK_DFAB = 0,
	CLK_SFPB,
};

enum qseecom_client_handle_type {
	QSEECOM_CLIENT_APP = 1,
	QSEECOM_LISTENER_SERVICE,
	QSEECOM_SECURE_SERVICE,
	QSEECOM_GENERIC,
	QSEECOM_UNAVAILABLE_CLIENT_APP,
};

enum qseecom_ce_hw_instance {
	CLK_QSEE = 0,
	CLK_CE_DRV,
};

static struct class *driver_class;
static dev_t qseecom_device_no;

static DEFINE_MUTEX(qsee_bw_mutex);
static DEFINE_MUTEX(app_access_lock);
static DEFINE_MUTEX(clk_access_lock);

struct qseecom_registered_listener_list {
	struct list_head                 list;
	struct qseecom_register_listener_req svc;
	uint32_t user_virt_sb_base;
	u8 *sb_virt;
	s32 sb_phys;
	size_t sb_length;
	struct ion_handle *ihandle; /* Retrieve phy addr */

	wait_queue_head_t          rcv_req_wq;
	int                        rcv_req_flag;
};

struct qseecom_registered_app_list {
	struct list_head                 list;
	u32  app_id;
	u32  ref_cnt;
	char app_name[MAX_APP_NAME_SIZE];
};

struct qseecom_registered_kclient_list {
	struct list_head list;
	struct qseecom_handle *handle;
};

struct ce_hw_usage_info {
	uint32_t  qsee_ce_hw_instance;
	uint32_t  hlos_ce_hw_instance;
	uint32_t  disk_encrypt_pipe;
	uint32_t  file_encrypt_pipe;
};

struct qseecom_clk {
	enum qseecom_ce_hw_instance instance;
	struct clk *ce_core_clk;
	struct clk *ce_clk;
	struct clk *ce_core_src_clk;
	struct clk *ce_bus_clk;
	uint32_t clk_access_cnt;
};

struct qseecom_control {
	struct ion_client *ion_clnt;		/* Ion client */
	struct list_head  registered_listener_list_head;
	spinlock_t        registered_listener_list_lock;

	struct list_head  registered_app_list_head;
	spinlock_t        registered_app_list_lock;

	struct list_head   registered_kclient_list_head;
	spinlock_t        registered_kclient_list_lock;

	wait_queue_head_t send_resp_wq;
	int               send_resp_flag;

	uint32_t          qseos_version;
	uint32_t          qsee_version;
	struct device *pdev;
	bool  commonlib_loaded;
	struct ion_handle *cmnlib_ion_handle;
	struct ce_hw_usage_info ce_info;

	int qsee_bw_count;
	int qsee_sfpb_bw_count;

	uint32_t qsee_perf_client;
	struct qseecom_clk qsee;
	struct qseecom_clk ce_drv;

	bool support_bus_scaling;
	bool support_fde;
	bool support_pfe;
	uint32_t  cumulative_mode;
	enum qseecom_bandwidth_request_mode  current_mode;
	struct timer_list bw_scale_down_timer;
	struct work_struct bw_inactive_req_ws;
	struct cdev cdev;
	bool timer_running;
};

struct qseecom_client_handle {
	u32  app_id;
	u8 *sb_virt;
	s32 sb_phys;
	uint32_t user_virt_sb_base;
	size_t sb_length;
	struct ion_handle *ihandle;		/* Retrieve phy addr */
	char app_name[MAX_APP_NAME_SIZE];
};

struct qseecom_listener_handle {
	u32               id;
};

static struct qseecom_control qseecom;

struct qseecom_dev_handle {
	enum qseecom_client_handle_type type;
	union {
		struct qseecom_client_handle client;
		struct qseecom_listener_handle listener;
	};
	bool released;
	int               abort;
	wait_queue_head_t abort_wq;
	atomic_t          ioctl_count;
	bool  perf_enabled;
	bool  fast_load_enabled;
	enum qseecom_bandwidth_request_mode mode;
};

enum qseecom_set_clear_key_flag {
	QSEECOM_CLEAR_CE_KEY_CMD = 0,
	QSEECOM_SET_CE_KEY_CMD,
};

struct qseecom_set_key_parameter {
	uint32_t ce_hw;
	uint32_t pipe;
	uint32_t flags;
	uint8_t key_id[QSEECOM_KEY_ID_SIZE];
	unsigned char hash32[QSEECOM_HASH_SIZE];
	enum qseecom_set_clear_key_flag set_clear_key_flag;
};

struct qseecom_sg_entry {
	uint32_t phys_addr;
	uint32_t len;
};

struct qseecom_key_id_usage_desc {
	uint8_t desc[QSEECOM_KEY_ID_SIZE];
};

static struct qseecom_key_id_usage_desc key_id_array[] = {
	{
		.desc = "Undefined Usage Index",
	},

	{
		.desc = "Full Disk Encryption",
	},

	{
		.desc = "Per File Encryption",
	},
};

/* Function proto types */
static int qsee_vote_for_clock(struct qseecom_dev_handle *, int32_t);
static void qsee_disable_clock_vote(struct qseecom_dev_handle *, int32_t);
static int __qseecom_enable_clk(enum qseecom_ce_hw_instance ce);
static void __qseecom_disable_clk(enum qseecom_ce_hw_instance ce);

static int __qseecom_is_svc_unique(struct qseecom_dev_handle *data,
		struct qseecom_register_listener_req *svc)
{
	struct qseecom_registered_listener_list *ptr;
	int unique = 1;
	unsigned long flags;

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr, &qseecom.registered_listener_list_head, list) {
		if (ptr->svc.listener_id == svc->listener_id) {
			pr_err("Service id: %u is already registered\n",
					ptr->svc.listener_id);
			unique = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);
	return unique;
}

static struct qseecom_registered_listener_list *__qseecom_find_svc(
						int32_t listener_id)
{
	struct qseecom_registered_listener_list *entry = NULL;
	unsigned long flags;

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(entry, &qseecom.registered_listener_list_head, list)
	{
		if (entry->svc.listener_id == listener_id)
			break;
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	if ((entry != NULL) && (entry->svc.listener_id != listener_id)) {
		pr_err("Service id: %u is not found\n", listener_id);
		return NULL;
	}

	return entry;
}

static int __qseecom_set_sb_memory(struct qseecom_registered_listener_list *svc,
				struct qseecom_dev_handle *handle,
				struct qseecom_register_listener_req *listener)
{
	int ret = 0;
	struct qseecom_register_listener_ireq req;
	struct qseecom_command_scm_resp resp;
	ion_phys_addr_t pa;

	/* Get the handle of the shared fd */
	svc->ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					listener->ifd_data_fd);
	if (IS_ERR_OR_NULL(svc->ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, svc->ihandle, &pa, &svc->sb_length);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}
	/* Populate the structure for sending scm call to load image */
	svc->sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt, svc->ihandle);
	svc->sb_phys = pa;

	req.qsee_cmd_id = QSEOS_REGISTER_LISTENER;
	req.listener_id = svc->svc.listener_id;
	req.sb_len = svc->sb_length;
	req.sb_ptr = (void *)svc->sb_phys;

	resp.result = QSEOS_RESULT_INCOMPLETE;

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req,
					sizeof(req), &resp, sizeof(resp));
	if (ret) {
		pr_err("qseecom_scm_call failed with err: %d\n", ret);
		return -EINVAL;
	}

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		pr_err("Error SB registration req: resp.result = %d\n",
			resp.result);
		return -EPERM;
	}
	return 0;
}

static int qseecom_register_listener(struct qseecom_dev_handle *data,
					void __user *argp)
{
	int ret = 0;
	unsigned long flags;
	struct qseecom_register_listener_req rcvd_lstnr;
	struct qseecom_registered_listener_list *new_entry;

	ret = copy_from_user(&rcvd_lstnr, argp, sizeof(rcvd_lstnr));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	if (!access_ok(VERIFY_WRITE, (void __user *)rcvd_lstnr.virt_sb_base,
			rcvd_lstnr.sb_size))
		return -EFAULT;

	data->listener.id = 0;
	if (!__qseecom_is_svc_unique(data, &rcvd_lstnr)) {
		pr_err("Service is not unique and is already registered\n");
		data->released = true;
		return -EBUSY;
	}

	new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) {
		pr_err("kmalloc failed\n");
		return -ENOMEM;
	}
	memcpy(&new_entry->svc, &rcvd_lstnr, sizeof(rcvd_lstnr));
	new_entry->rcv_req_flag = 0;

	new_entry->svc.listener_id = rcvd_lstnr.listener_id;
	new_entry->sb_length = rcvd_lstnr.sb_size;
	new_entry->user_virt_sb_base = rcvd_lstnr.virt_sb_base;
	if (__qseecom_set_sb_memory(new_entry, data, &rcvd_lstnr)) {
		pr_err("qseecom_set_sb_memoryfailed\n");
		kzfree(new_entry);
		return -ENOMEM;
	}

	data->listener.id = rcvd_lstnr.listener_id;
	init_waitqueue_head(&new_entry->rcv_req_wq);

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_add_tail(&new_entry->list, &qseecom.registered_listener_list_head);
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	return ret;
}

static int qseecom_unregister_listener(struct qseecom_dev_handle *data)
{
	int ret = 0;
	unsigned long flags;
	uint32_t unmap_mem = 0;
	struct qseecom_register_listener_ireq req;
	struct qseecom_registered_listener_list *ptr_svc = NULL;
	struct qseecom_command_scm_resp resp;
	struct ion_handle *ihandle = NULL;		/* Retrieve phy addr */

	req.qsee_cmd_id = QSEOS_DEREGISTER_LISTENER;
	req.listener_id = data->listener.id;
	resp.result = QSEOS_RESULT_INCOMPLETE;

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req,
					sizeof(req), &resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (lstnr id=%d)\n",
				ret, data->listener.id);
		return ret;
	}

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		pr_err("Failed resp.result=%d,(lstnr id=%d)\n",
				resp.result, data->listener.id);
		return -EPERM;
	}

	data->abort = 1;
	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr_svc, &qseecom.registered_listener_list_head,
			list) {
		if (ptr_svc->svc.listener_id == data->listener.id) {
			wake_up_all(&ptr_svc->rcv_req_wq);
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	while (atomic_read(&data->ioctl_count) > 1) {
		if (wait_event_freezable(data->abort_wq,
				atomic_read(&data->ioctl_count) <= 1)) {
			pr_err("Interrupted from abort\n");
			ret = -ERESTARTSYS;
			break;
		}
	}

	spin_lock_irqsave(&qseecom.registered_listener_list_lock, flags);
	list_for_each_entry(ptr_svc,
			&qseecom.registered_listener_list_head,
			list)
	{
		if (ptr_svc->svc.listener_id == data->listener.id) {
			if (ptr_svc->sb_virt) {
				unmap_mem = 1;
				ihandle = ptr_svc->ihandle;
				}
			list_del(&ptr_svc->list);
			kzfree(ptr_svc);
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_listener_list_lock, flags);

	/* Unmap the memory */
	if (unmap_mem) {
		if (!IS_ERR_OR_NULL(ihandle)) {
			ion_unmap_kernel(qseecom.ion_clnt, ihandle);
			ion_free(qseecom.ion_clnt, ihandle);
			}
	}
	data->released = true;
	return ret;
}

static int __qseecom_set_msm_bus_request(uint32_t mode)
{
	int ret = 0;
	struct qseecom_clk *qclk;

	qclk = &qseecom.qsee;
	if (qclk->ce_core_src_clk != NULL) {
		if (mode == INACTIVE) {
			__qseecom_disable_clk(CLK_QSEE);
		} else {
			ret = __qseecom_enable_clk(CLK_QSEE);
			if (ret)
				pr_err("CLK enabling failed (%d) MODE (%d)\n",
							ret, mode);
		}
	}

	if ((!ret) && (qseecom.current_mode != mode)) {
		ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, mode);
		if (ret) {
			pr_err("Bandwidth req failed(%d) MODE (%d)\n",
							ret, mode);
			if (qclk->ce_core_src_clk != NULL) {
				if (mode == INACTIVE)
					__qseecom_enable_clk(CLK_QSEE);
				else
					__qseecom_disable_clk(CLK_QSEE);
			}
		}
		qseecom.current_mode = mode;
	}
	return ret;
}

static void qseecom_bw_inactive_req_work(struct work_struct *work)
{
	mutex_lock(&app_access_lock);
	mutex_lock(&qsee_bw_mutex);
	__qseecom_set_msm_bus_request(INACTIVE);
	pr_debug("current_mode = %d, cumulative_mode = %d\n",
				qseecom.current_mode, qseecom.cumulative_mode);
	qseecom.timer_running = false;
	mutex_unlock(&qsee_bw_mutex);
	mutex_unlock(&app_access_lock);
	return;
}

static void qseecom_scale_bus_bandwidth_timer_callback(unsigned long data)
{
	schedule_work(&qseecom.bw_inactive_req_ws);
	return;
}

static int __qseecom_decrease_clk_ref_count(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;
	int ret = 0;
	mutex_lock(&clk_access_lock);
	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	if (qclk->clk_access_cnt > 2) {
		pr_err("Invalid clock ref count %d\n", qclk->clk_access_cnt);
		ret = -EINVAL;
		goto err_dec_ref_cnt;
	}
	if (qclk->clk_access_cnt == 2)
		qclk->clk_access_cnt--;

err_dec_ref_cnt:
	mutex_unlock(&clk_access_lock);
	return ret;
}


static int qseecom_scale_bus_bandwidth_timer(uint32_t mode)
{
	int32_t ret = 0;
	int32_t request_mode = INACTIVE;

	mutex_lock(&qsee_bw_mutex);
	if (mode == 0) {
		if (qseecom.cumulative_mode > MEDIUM)
			request_mode = HIGH;
		else
			request_mode = qseecom.cumulative_mode;
	} else {
		request_mode = mode;
	}

	ret = __qseecom_set_msm_bus_request(request_mode);
	if (ret) {
		pr_err("set msm bus request failed (%d),request_mode (%d)\n",
			ret, request_mode);
		goto err_scale_timer;
	}

	if (qseecom.timer_running) {
		ret = __qseecom_decrease_clk_ref_count(CLK_QSEE);
		if (ret) {
			pr_err("Failed to decrease clk ref count.\n");
			goto err_scale_timer;
		}
		del_timer_sync(&(qseecom.bw_scale_down_timer));
		qseecom.timer_running = false;
	}
err_scale_timer:
	mutex_unlock(&qsee_bw_mutex);
	return ret;
}


static int qseecom_unregister_bus_bandwidth_needs(
					struct qseecom_dev_handle *data)
{
	int32_t ret = 0;

	qseecom.cumulative_mode -= data->mode;
	data->mode = INACTIVE;

	return ret;
}

static int __qseecom_register_bus_bandwidth_needs(
			struct qseecom_dev_handle *data, uint32_t request_mode)
{
	int32_t ret = 0;

	if (data->mode == INACTIVE) {
		qseecom.cumulative_mode += request_mode;
		data->mode = request_mode;
	} else {
		if (data->mode != request_mode) {
			qseecom.cumulative_mode -= data->mode;
			qseecom.cumulative_mode += request_mode;
			data->mode = request_mode;
		}
	}
	return ret;
}

static int qseecom_perf_enable(struct qseecom_dev_handle *data)
{
	int ret = 0;
	ret = qsee_vote_for_clock(data, CLK_DFAB);
	if (ret) {
		pr_err("Failed to vote for DFAB clock with err %d\n", ret);
		goto perf_enable_exit;
	}
	ret = qsee_vote_for_clock(data, CLK_SFPB);
	if (ret) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		pr_err("Failed to vote for SFPB clock with err %d\n", ret);
		goto perf_enable_exit;
	}

perf_enable_exit:
	return ret;
}

static int qseecom_scale_bus_bandwidth(struct qseecom_dev_handle *data,
						void __user *argp)
{
	int32_t ret = 0;
	int32_t req_mode;

	ret = copy_from_user(&req_mode, argp, sizeof(req_mode));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	if (req_mode > HIGH) {
		pr_err("Invalid bandwidth mode (%d)\n", req_mode);
		return ret;
	}

	/*
	* Register bus bandwidth needs if bus scaling feature is enabled;
	* otherwise, qseecom enable/disable clocks for the client directly.
	*/
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, req_mode);
		mutex_unlock(&qsee_bw_mutex);
	} else {
		pr_debug("Bus scaling feature is NOT enabled\n");
		pr_debug("request bandwidth mode %d for the client\n",
				req_mode);
		if (req_mode != INACTIVE) {
			ret = qseecom_perf_enable(data);
			if (ret)
				pr_err("Failed to vote for clock with err %d\n",
						ret);
		} else {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		}
	}
	return ret;
}

static void __qseecom_add_bw_scale_down_timer(uint32_t duration)
{
	mutex_lock(&qsee_bw_mutex);
	qseecom.bw_scale_down_timer.expires = jiffies +
		msecs_to_jiffies(duration);
	mod_timer(&(qseecom.bw_scale_down_timer),
		qseecom.bw_scale_down_timer.expires);
	qseecom.timer_running = true;
	mutex_unlock(&qsee_bw_mutex);
}

static void __qseecom_disable_clk_scale_down(struct qseecom_dev_handle *data)
{
	if (!qseecom.support_bus_scaling)
		qsee_disable_clock_vote(data, CLK_SFPB);
	else
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_LOAD_APP_CRYPTO_TIMEOUT);
	return;
}

static int __qseecom_enable_clk_scale_up(struct qseecom_dev_handle *data)
{
	int ret = 0;
	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(MEDIUM);
		if (ret)
			pr_err("Failed to set bw MEDIUM.\n");
	} else {
		ret = qsee_vote_for_clock(data, CLK_SFPB);
		if (ret)
			pr_err("Fail vote for clk SFPB ret %d\n", ret);
	}
	return ret;
}

static int qseecom_set_client_mem_param(struct qseecom_dev_handle *data,
						void __user *argp)
{
	ion_phys_addr_t pa;
	int32_t ret;
	struct qseecom_set_sb_mem_param_req req;
	uint32_t len;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&req, (void __user *)argp, sizeof(req)))
		return -EFAULT;

	if ((req.ifd_data_fd <= 0) || (req.virt_sb_base == 0) ||
					(req.sb_len == 0)) {
		pr_err("Inavlid input(s)ion_fd(%d), sb_len(%d), vaddr(0x%x)\n",
			req.ifd_data_fd, req.sb_len, req.virt_sb_base);
		return -EFAULT;
	}
	if (!access_ok(VERIFY_WRITE, (void __user *)req.virt_sb_base,
			req.sb_len))
		return -EFAULT;

	/* Get the handle of the shared fd */
	data->client.ihandle = ion_import_dma_buf(qseecom.ion_clnt,
						req.ifd_data_fd);
	if (IS_ERR_OR_NULL(data->client.ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}
	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, data->client.ihandle, &pa, &len);
	if (ret) {

		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}
	/* Populate the structure for sending scm call to load image */
	data->client.sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt,
							data->client.ihandle);
	data->client.sb_phys = pa;
	data->client.sb_length = req.sb_len;
	data->client.user_virt_sb_base = req.virt_sb_base;
	return 0;
}

static int __qseecom_listener_has_sent_rsp(struct qseecom_dev_handle *data)
{
	int ret;
	ret = (qseecom.send_resp_flag != 0);
	return ret || data->abort;
}

static int __qseecom_process_incomplete_cmd(struct qseecom_dev_handle *data,
					struct qseecom_command_scm_resp *resp)
{
	int ret = 0;
	int rc = 0;
	uint32_t lstnr;
	unsigned long flags;
	struct qseecom_client_listener_data_irsp send_data_rsp;
	struct qseecom_registered_listener_list *ptr_svc = NULL;
	sigset_t new_sigset;
	sigset_t old_sigset;

	while (resp->result == QSEOS_RESULT_INCOMPLETE) {
		lstnr = resp->data;
		/*
		 * Wake up blocking lsitener service with the lstnr id
		 */
		spin_lock_irqsave(&qseecom.registered_listener_list_lock,
					flags);
		list_for_each_entry(ptr_svc,
				&qseecom.registered_listener_list_head, list) {
			if (ptr_svc->svc.listener_id == lstnr) {
				ptr_svc->rcv_req_flag = 1;
				wake_up_interruptible(&ptr_svc->rcv_req_wq);
				break;
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_listener_list_lock,
				flags);

		if (ptr_svc == NULL) {
			pr_err("Listener Svc %d does not exist\n", lstnr);
			return -EINVAL;
		}
		if (ptr_svc->svc.listener_id != lstnr) {
			pr_warning("Service requested for does on exist\n");
			return -ERESTARTSYS;
		}
		pr_debug("waking up rcv_req_wq and "
				"waiting for send_resp_wq\n");

		/* initialize the new signal mask with all signals*/
		sigfillset(&new_sigset);
		/* block all signals */
		sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset);

		do {
			if (!wait_event_freezable(qseecom.send_resp_wq,
				__qseecom_listener_has_sent_rsp(data)))
				break;
		} while (1);

		/* restore signal mask */
		sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		if (data->abort) {
			pr_err("Abort clnt %d waiting on lstnr svc %d, ret %d",
				data->client.app_id, lstnr, ret);
			rc = -ENODEV;
			send_data_rsp.status  = QSEOS_RESULT_FAILURE;
		} else {
			send_data_rsp.status  = QSEOS_RESULT_SUCCESS;
		}

		qseecom.send_resp_flag = 0;
		send_data_rsp.qsee_cmd_id = QSEOS_LISTENER_DATA_RSP_COMMAND;
		send_data_rsp.listener_id  = lstnr ;
		if (ptr_svc)
			msm_ion_do_cache_op(qseecom.ion_clnt, ptr_svc->ihandle,
					ptr_svc->sb_virt, ptr_svc->sb_length,
						ION_IOC_CLEAN_INV_CACHES);

		if (lstnr == RPMB_SERVICE)
			__qseecom_enable_clk(CLK_QSEE);

		ret = scm_call(SCM_SVC_TZSCHEDULER, 1,
					(const void *)&send_data_rsp,
					sizeof(send_data_rsp), resp,
					sizeof(*resp));
		if (ret) {
			pr_err("scm_call() failed with err: %d (app_id = %d)\n",
				ret, data->client.app_id);
			if (lstnr == RPMB_SERVICE)
				__qseecom_disable_clk(CLK_QSEE);
			return ret;
		}
		if ((resp->result != QSEOS_RESULT_SUCCESS) &&
			(resp->result != QSEOS_RESULT_INCOMPLETE)) {
			pr_err("fail:resp res= %d,app_id = %d,lstr = %d\n",
				resp->result, data->client.app_id, lstnr);
			ret = -EINVAL;
		}
		if (lstnr == RPMB_SERVICE)
			__qseecom_disable_clk(CLK_QSEE);

	}
	if (rc)
		return rc;

	return ret;
}

static int __qseecom_check_app_exists(struct qseecom_check_app_ireq req)
{
	int32_t ret;
	struct qseecom_command_scm_resp resp;

	/*  SCM_CALL  to check if app_id for the mentioned app exists */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req,
				sizeof(struct qseecom_check_app_ireq),
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to check if app is already loaded failed\n");
		return -EINVAL;
	}

	if (resp.result == QSEOS_RESULT_FAILURE) {
			return 0;
	} else {
		switch (resp.resp_type) {
		/*qsee returned listener type response */
		case QSEOS_LISTENER_ID:
			pr_err("resp type is of listener type instead of app");
			return -EINVAL;
			break;
		case QSEOS_APP_ID:
			return resp.data;
		default:
			pr_err("invalid resp type (%d) from qsee",
					resp.resp_type);
			return -ENODEV;
			break;
		}
	}
}

static int qseecom_load_app(struct qseecom_dev_handle *data, void __user *argp)
{
	struct qseecom_registered_app_list *entry = NULL;
	unsigned long flags = 0;
	u32 app_id = 0;
	struct ion_handle *ihandle;	/* Ion handle */
	struct qseecom_load_img_req load_img_req;
	int32_t ret = 0;
	ion_phys_addr_t pa = 0;
	uint32_t len;
	struct qseecom_command_scm_resp resp;
	struct qseecom_check_app_ireq req;
	struct qseecom_load_app_ireq load_req;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&load_img_req,
				(void __user *)argp,
				sizeof(struct qseecom_load_img_req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret)
			return ret;
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret)
		goto enable_clk_err;

	req.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	load_img_req.img_name[MAX_APP_NAME_SIZE-1] = '\0';
	memcpy(req.app_name, load_img_req.img_name, MAX_APP_NAME_SIZE);

	ret = __qseecom_check_app_exists(req);
	if (ret < 0)
		goto loadapp_err;

	app_id = ret;
	if (app_id) {
		pr_debug("App id %d (%s) already exists\n", app_id,
			(char *)(req.app_name));
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
		&qseecom.registered_app_list_head, list){
			if (entry->app_id == app_id) {
				entry->ref_cnt++;
				break;
			}
		}
		spin_unlock_irqrestore(
		&qseecom.registered_app_list_lock, flags);
		ret = 0;
	} else {
		pr_warn("App (%s) does'nt exist, loading apps for first time\n",
			(char *)(load_img_req.img_name));
		/* Get the handle of the shared fd */
		ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					load_img_req.ifd_data_fd);
		if (IS_ERR_OR_NULL(ihandle)) {
			pr_err("Ion client could not retrieve the handle\n");
			ret = -ENOMEM;
			goto loadapp_err;
		}

		/* Get the physical address of the ION BUF */
		ret = ion_phys(qseecom.ion_clnt, ihandle, &pa, &len);
		if (ret) {
			pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
				ret);
			goto loadapp_err;
		}

		/* Populate the structure for sending scm call to load image */
		memcpy(load_req.app_name, load_img_req.img_name,
						MAX_APP_NAME_SIZE);
		load_req.qsee_cmd_id = QSEOS_APP_START_COMMAND;
		load_req.mdt_len = load_img_req.mdt_len;
		load_req.img_len = load_img_req.img_len;
		load_req.phy_addr = pa;
		msm_ion_do_cache_op(qseecom.ion_clnt, ihandle, NULL, len,
					ION_IOC_CLEAN_INV_CACHES);

		/*  SCM_CALL  to load the app and get the app_id back */
		ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &load_req,
			sizeof(struct qseecom_load_app_ireq),
			&resp, sizeof(resp));
		if (ret) {
			pr_err("scm_call to load app failed\n");
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EINVAL;
			goto loadapp_err;
		}

		if (resp.result == QSEOS_RESULT_FAILURE) {
			pr_err("scm_call rsp.result is QSEOS_RESULT_FAILURE\n");
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EFAULT;
			goto loadapp_err;
		}

		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd failed err: %d\n",
					ret);
				if (!IS_ERR_OR_NULL(ihandle))
					ion_free(qseecom.ion_clnt, ihandle);
				ret = -EFAULT;
				goto loadapp_err;
			}
		}

		if (resp.result != QSEOS_RESULT_SUCCESS) {
			pr_err("scm_call failed resp.result unknown, %d\n",
				resp.result);
			if (!IS_ERR_OR_NULL(ihandle))
				ion_free(qseecom.ion_clnt, ihandle);
			ret = -EFAULT;
			goto loadapp_err;
		}

		app_id = resp.data;

		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			pr_err("kmalloc failed\n");
			ret = -ENOMEM;
			goto loadapp_err;
		}
		entry->app_id = app_id;
		entry->ref_cnt = 1;
		memset((void *)entry->app_name, 0, MAX_APP_NAME_SIZE);
		memcpy((void *)entry->app_name,
			(void *)load_img_req.img_name, MAX_APP_NAME_SIZE);
		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);

		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_add_tail(&entry->list, &qseecom.registered_app_list_head);
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
									flags);

		pr_warn("App with id %d (%s) now loaded\n", app_id,
		(char *)(load_img_req.img_name));
	}
	data->client.app_id = app_id;
	memset((void *)data->client.app_name, 0, MAX_APP_NAME_SIZE);
	memcpy((void *)data->client.app_name,
		(void *)load_img_req.img_name, MAX_APP_NAME_SIZE);
	load_img_req.app_id = app_id;
	if (copy_to_user(argp, &load_img_req, sizeof(load_img_req))) {
		pr_err("copy_to_user failed\n");
		kzfree(entry);
		ret = -EFAULT;
	}

loadapp_err:
	__qseecom_disable_clk_scale_down(data);
enable_clk_err:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}
	return ret;
}

static int __qseecom_cleanup_app(struct qseecom_dev_handle *data)
{
	wake_up_all(&qseecom.send_resp_wq);
	while (atomic_read(&data->ioctl_count) > 1) {
		if (wait_event_freezable(data->abort_wq,
					atomic_read(&data->ioctl_count) <= 1)) {
			pr_err("Interrupted from abort\n");
			return -ERESTARTSYS;
			break;
		}
	}
	/* Set unload app */
	return 1;
}

static int qseecom_unmap_ion_allocated_memory(struct qseecom_dev_handle *data)
{
	int ret = 0;
	if (!IS_ERR_OR_NULL(data->client.ihandle)) {
		ion_unmap_kernel(qseecom.ion_clnt, data->client.ihandle);
		ion_free(qseecom.ion_clnt, data->client.ihandle);
		data->client.ihandle = NULL;
	}
	return ret;
}

static int qseecom_unload_app(struct qseecom_dev_handle *data,
				bool app_crash)
{
	unsigned long flags;
	unsigned long flags1;
	int ret = 0;
	struct qseecom_command_scm_resp resp;
	struct qseecom_registered_app_list *ptr_app = NULL;
	bool unload = false;
	bool found_app = false;
	bool found_dead_app = false;

	if (!memcmp(data->client.app_name, "keymaste", strlen("keymaste"))) {
		pr_warn("Do not unload keymaster app from tz\n");
		return 0;
	}

	if (data->client.app_id > 0) {
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
									list) {
			if (ptr_app->app_id == data->client.app_id) {
				if (!memcmp((void *)ptr_app->app_name,
					(void *)data->client.app_name,
					strlen(data->client.app_name))) {
					found_app = true;
					if (app_crash || ptr_app->ref_cnt == 1)
						unload = true;
					break;
				} else {
					found_dead_app = true;
					break;
				}
			}
		}
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
								flags);
		if (found_app == false && found_dead_app == false) {
			pr_err("Cannot find app with id = %d (%s)\n",
				data->client.app_id,
				(char *)data->client.app_name);
			return -EINVAL;
		}
	}

	if (found_dead_app) {
		pr_warn("cleanup app_id %d(%s)\n", data->client.app_id,
			(char *)data->client.app_name);
		__qseecom_cleanup_app(data);
	}

	if (unload) {
		struct qseecom_unload_app_ireq req;
		/* Populate the structure for sending scm call to load image */
		req.qsee_cmd_id = QSEOS_APP_SHUTDOWN_COMMAND;
		req.app_id = data->client.app_id;

		/* SCM_CALL to unload the app */
		ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req,
				sizeof(struct qseecom_unload_app_ireq),
				&resp, sizeof(resp));
		if (ret) {
			pr_err("scm_call to unload app (id = %d) failed\n",
								req.app_id);
			return -EFAULT;
		} else {
			pr_warn("App id %d now unloaded\n", req.app_id);
		}
		if (resp.result == QSEOS_RESULT_FAILURE) {
			pr_err("app (%d) unload_failed!!\n",
					data->client.app_id);
			return -EFAULT;
		}
		if (resp.result == QSEOS_RESULT_SUCCESS)
			pr_info("App (%d) is unloaded!!\n",
					data->client.app_id);
		__qseecom_cleanup_app(data);
		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			ret = __qseecom_process_incomplete_cmd(data, &resp);
			if (ret) {
				pr_err("process_incomplete_cmd fail err: %d\n",
									ret);
				return ret;
			}
		}
	}

	if (found_app) {
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags1);
		if (app_crash) {
			ptr_app->ref_cnt = 0;
			pr_debug("app_crash: ref_count = 0\n");
		} else {
			if (ptr_app->ref_cnt == 1) {
				ptr_app->ref_cnt = 0;
				pr_info("ref_count set to 0\n");
			} else {
				ptr_app->ref_cnt--;
				pr_info("Can't unload app(%d) inuse\n",
					ptr_app->app_id);
			}
		}
		if (unload) {
			list_del(&ptr_app->list);
			kzfree(ptr_app);
		}
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
								flags1);
	}
	qseecom_unmap_ion_allocated_memory(data);
	data->released = true;
	return ret;
}

static uint32_t __qseecom_uvirt_to_kphys(struct qseecom_dev_handle *data,
						uint32_t virt)
{
	return data->client.sb_phys + (virt - data->client.user_virt_sb_base);
}

static uint32_t __qseecom_uvirt_to_kvirt(struct qseecom_dev_handle *data,
						uint32_t virt)
{
	return (uint32_t)data->client.sb_virt +
				(virt - data->client.user_virt_sb_base);
}

int __qseecom_process_rpmb_svc_cmd(struct qseecom_dev_handle *data_ptr,
		struct qseecom_send_svc_cmd_req *req_ptr,
		struct qseecom_client_send_service_ireq *send_svc_ireq_ptr)
{
	int ret = 0;
	void *req_buf = NULL;

	if ((req_ptr == NULL) || (send_svc_ireq_ptr == NULL)) {
		pr_err("Error with pointer: req_ptr = %p, send_svc_ptr = %p\n",
			req_ptr, send_svc_ireq_ptr);
		return -EINVAL;
	}

	if ((!req_ptr->cmd_req_buf) || (!req_ptr->resp_buf)) {
		pr_err("Invalid req/resp buffer, exiting\n");
		return -EINVAL;
	}

	/* Clients need to ensure req_buf is at base offset of shared buffer */
	if ((uint32_t)req_ptr->cmd_req_buf !=
			data_ptr->client.user_virt_sb_base) {
		pr_err("cmd buf not pointing to base offset of shared buffer\n");
		return -EINVAL;
	}

	if (((uint32_t)req_ptr->resp_buf < data_ptr->client.user_virt_sb_base)
			|| ((uint32_t)req_ptr->resp_buf >=
			(data_ptr->client.user_virt_sb_base +
			data_ptr->client.sb_length))){
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	req_buf = data_ptr->client.sb_virt;

	send_svc_ireq_ptr->qsee_cmd_id = req_ptr->cmd_id;
	send_svc_ireq_ptr->key_type =
		((struct qseecom_rpmb_provision_key *)req_buf)->key_type;
	send_svc_ireq_ptr->req_len = req_ptr->cmd_req_len;
	send_svc_ireq_ptr->rsp_ptr = (void *)(__qseecom_uvirt_to_kphys(data_ptr,
					(uint32_t)req_ptr->resp_buf));
	send_svc_ireq_ptr->rsp_len = req_ptr->resp_len;

	return ret;
}

static int qseecom_send_service_cmd(struct qseecom_dev_handle *data,
				void __user *argp)
{
	int ret = 0;
	struct qseecom_client_send_service_ireq send_svc_ireq;
	struct qseecom_command_scm_resp resp;
	struct qseecom_send_svc_cmd_req req;
	/*struct qseecom_command_scm_resp resp;*/

	if (copy_from_user(&req,
				(void __user *)argp,
				sizeof(req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	if (req.resp_buf == NULL) {
		pr_err("cmd buffer or response buffer is null\n");
		return -EINVAL;
	}

	if (data->client.sb_virt == NULL) {
		pr_err("sb_virt null\n");
		return -EINVAL;
	}

	if (data->client.user_virt_sb_base == 0) {
		pr_err("user_virt_sb_base is null\n");
		return -EINVAL;
	}

	if (data->client.sb_length == 0) {
		pr_err("sb_length is 0\n");
		return -EINVAL;
	}

	data->type = QSEECOM_SECURE_SERVICE;

	switch (req.cmd_id) {
	case QSEOS_RPMB_PROVISION_KEY_COMMAND:
	case QSEOS_RPMB_ERASE_COMMAND:
		if (__qseecom_process_rpmb_svc_cmd(data, &req,
				&send_svc_ireq))
			return -EINVAL;
		break;
	default:
		pr_err("Unsupported cmd_id %d\n", req.cmd_id);
		return -EINVAL;
	}

	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(HIGH);
		if (ret) {
			pr_err("Fail to set bw HIGH\n");
			return ret;
		}
	} else {
		ret = qseecom_perf_enable(data);
		if (ret) {
			pr_err("Failed to vote for clocks with err %d\n", ret);
			goto exit;
		}
	}

	msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_CLEAN_INV_CACHES);
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1, (const void *) &send_svc_ireq,
					sizeof(send_svc_ireq),
					&resp, sizeof(resp));
	msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	if (ret) {
		pr_err("qseecom_scm_call failed with err: %d\n", ret);
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		} else {
			__qseecom_add_bw_scale_down_timer(
				QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		}
		goto exit;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		pr_err("qseos_result_incomplete\n");
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd fail: err: %d\n",
				ret);
		}
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("process_incomplete_cmd failed err: %d\n", ret);
		break;
	default:
		pr_err("Response result %d not supported\n",
				resp.result);
		ret = -EINVAL;
		break;
	}
	if (!qseecom.support_bus_scaling) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		qsee_disable_clock_vote(data, CLK_SFPB);
	} else {
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
	}

exit:
	return ret;
}

static int __qseecom_send_cmd(struct qseecom_dev_handle *data,
				struct qseecom_send_cmd_req *req)
{
	int ret = 0;
	u32 reqd_len_sb_in = 0;
	struct qseecom_client_send_data_ireq send_data_req;
	struct qseecom_command_scm_resp resp;
	unsigned long flags;
	struct qseecom_registered_app_list *ptr_app;
	bool found_app = false;
	int name_len = 0;

	if (req->cmd_req_buf == NULL || req->resp_buf == NULL) {
		pr_err("cmd buffer or response buffer is null\n");
		return -EINVAL;
	}
	if (((uint32_t)req->cmd_req_buf < data->client.user_virt_sb_base) ||
		((uint32_t)req->cmd_req_buf >= (data->client.user_virt_sb_base +
					data->client.sb_length))) {
		pr_err("cmd buffer address not within shared bufffer\n");
		return -EINVAL;
	}


	if (((uint32_t)req->resp_buf < data->client.user_virt_sb_base)  ||
		((uint32_t)req->resp_buf >= (data->client.user_virt_sb_base +
					data->client.sb_length))){
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	if ((req->cmd_req_len == 0) || (req->resp_len == 0) ||
		req->cmd_req_len > data->client.sb_length ||
		req->resp_len > data->client.sb_length) {
		pr_err("cmd buffer length or "
				"response buffer length not valid\n");
		return -EINVAL;
	}

	if (req->cmd_req_len > UINT_MAX - req->resp_len) {
		pr_err("Integer overflow detected in req_len & rsp_len, exiting now\n");
		return -EINVAL;
	}

	reqd_len_sb_in = req->cmd_req_len + req->resp_len;
	if (reqd_len_sb_in > data->client.sb_length) {
		pr_debug("Not enough memory to fit cmd_buf and "
			"resp_buf. Required: %u, Available: %u\n",
				reqd_len_sb_in, data->client.sb_length);
		return -ENOMEM;
	}

	/* find app_id & img_name from list */
	spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
	list_for_each_entry(ptr_app, &qseecom.registered_app_list_head,
							list) {
		name_len = min(strlen(data->client.app_name),
				strlen(ptr_app->app_name));
		if ((ptr_app->app_id == data->client.app_id) &&
			 (!memcmp((void *)ptr_app->app_name,
				(void *)data->client.app_name, name_len))) {
			found_app = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_app_list_lock, flags);

	if (!found_app) {
		pr_err("app_id %d (%s) is not found\n", data->client.app_id,
			(char *)data->client.app_name);
		return -EINVAL;
	}

	send_data_req.qsee_cmd_id = QSEOS_CLIENT_SEND_DATA_COMMAND;
	send_data_req.app_id = data->client.app_id;
	send_data_req.req_ptr = (void *)(__qseecom_uvirt_to_kphys(data,
					(uint32_t)req->cmd_req_buf));
	send_data_req.req_len = req->cmd_req_len;
	send_data_req.rsp_ptr = (void *)(__qseecom_uvirt_to_kphys(data,
					(uint32_t)req->resp_buf));
	send_data_req.rsp_len = req->resp_len;

	msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
					data->client.sb_virt,
					reqd_len_sb_in,
					ION_IOC_CLEAN_INV_CACHES);

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1, (const void *) &send_data_req,
					sizeof(send_data_req),
					&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call() failed with err: %d (app_id = %d)\n",
					ret, data->client.app_id);
		return ret;
	}

	if (resp.result == QSEOS_RESULT_INCOMPLETE) {
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd failed err: %d\n", ret);
			return ret;
		}
	} else {
		if (resp.result != QSEOS_RESULT_SUCCESS) {
			pr_err("Response result %d not supported\n",
							resp.result);
			ret = -EINVAL;
		}
	}
	msm_ion_do_cache_op(qseecom.ion_clnt, data->client.ihandle,
				data->client.sb_virt, data->client.sb_length,
				ION_IOC_INV_CACHES);
	return ret;
}

static int qseecom_send_cmd(struct qseecom_dev_handle *data, void __user *argp)
{
	int ret = 0;
	struct qseecom_send_cmd_req req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}
	ret = __qseecom_send_cmd(data, &req);

	if (ret)
		return ret;

	return ret;
}

static int __qseecom_update_cmd_buf(void *msg, bool cleanup,
					struct qseecom_dev_handle *data,
					bool listener_svc)
{
	struct ion_handle *ihandle;
	char *field;
	int ret = 0;
	int i = 0;
	uint32_t len = 0;
	struct scatterlist *sg;
	struct qseecom_send_modfd_cmd_req *cmd_req = NULL;
	struct qseecom_send_modfd_listener_resp *lstnr_resp = NULL;
	struct qseecom_registered_listener_list *this_lstnr = NULL;

	if (msg == NULL) {
		pr_err("Invalid address\n");
		return -EINVAL;
	}
	if (listener_svc) {
		lstnr_resp = (struct qseecom_send_modfd_listener_resp *)msg;
		this_lstnr = __qseecom_find_svc(data->listener.id);
		if (IS_ERR_OR_NULL(this_lstnr)) {
			pr_err("Invalid listener ID\n");
			return -ENOMEM;
		}
	} else {
		cmd_req = (struct qseecom_send_modfd_cmd_req *)msg;
	}

	for (i = 0; i < MAX_ION_FD; i++) {
		struct sg_table *sg_ptr = NULL;
		if ((!listener_svc) && (cmd_req->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
					cmd_req->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = (char *) cmd_req->cmd_req_buf +
				cmd_req->ifd_data[i].cmd_buf_offset;
		} else if ((listener_svc) &&
				(lstnr_resp->ifd_data[i].fd > 0)) {
			ihandle = ion_import_dma_buf(qseecom.ion_clnt,
						lstnr_resp->ifd_data[i].fd);
			if (IS_ERR_OR_NULL(ihandle)) {
				pr_err("Ion client can't retrieve the handle\n");
				return -ENOMEM;
			}
			field = lstnr_resp->resp_buf_ptr +
				lstnr_resp->ifd_data[i].cmd_buf_offset;
		} else {
			return ret;
		}
		/* Populate the cmd data structure with the phys_addr */
		sg_ptr = ion_sg_table(qseecom.ion_clnt, ihandle);
		if (sg_ptr == NULL) {
			pr_err("IOn client could not retrieve sg table\n");
			goto err;
		}
		if (sg_ptr->nents == 0) {
			pr_err("Num of scattered entries is 0\n");
			goto err;
		}
		if (sg_ptr->nents > QSEECOM_MAX_SG_ENTRY) {
			pr_err("Num of scattered entries");
			pr_err(" (%d) is greater than max supported %d\n",
				sg_ptr->nents, QSEECOM_MAX_SG_ENTRY);
			goto err;
		}
			sg = sg_ptr->sgl;
		if (sg_ptr->nents == 1) {
			uint32_t *update;
			update = (uint32_t *) field;
			if (cleanup)
				*update = 0;
			else
				*update = (uint32_t)sg_dma_address(
							sg_ptr->sgl);
				len += (uint32_t)sg->length;
		} else {
			struct qseecom_sg_entry *update;
			int j = 0;
			update = (struct qseecom_sg_entry *) field;
			for (j = 0; j < sg_ptr->nents; j++) {
				if (cleanup) {
					update->phys_addr = 0;
					update->len = 0;
				} else {
					update->phys_addr = (uint32_t)
						sg_dma_address(sg);
					update->len = sg->length;
				}
					len += sg->length;
				update++;
				sg = sg_next(sg);
			}
		}
		if (cleanup)
			msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_INV_CACHES);
		else
			msm_ion_do_cache_op(qseecom.ion_clnt,
					ihandle, NULL, len,
					ION_IOC_CLEAN_INV_CACHES);
		/* Deallocate the handle */
		if (!IS_ERR_OR_NULL(ihandle))
			ion_free(qseecom.ion_clnt, ihandle);
	}
	return ret;
err:
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return -ENOMEM;
}

static int qseecom_send_modfd_cmd(struct qseecom_dev_handle *data,
					void __user *argp)
{
	int ret = 0;
	int i;
	struct qseecom_send_modfd_cmd_req req;
	struct qseecom_send_cmd_req send_cmd_req;

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (req.cmd_req_buf == NULL || req.resp_buf == NULL) {
		pr_err("cmd buffer or response buffer is null\n");
		return -EINVAL;
	}
	if (((uint32_t)req.cmd_req_buf < data->client.user_virt_sb_base) ||
		((uint32_t)req.cmd_req_buf >= (data->client.user_virt_sb_base +
					data->client.sb_length))) {
		pr_err("cmd buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	if (((uint32_t)req.resp_buf < data->client.user_virt_sb_base)  ||
		((uint32_t)req.resp_buf >= (data->client.user_virt_sb_base +
					data->client.sb_length))){
		pr_err("response buffer address not within shared bufffer\n");
		return -EINVAL;
	}

	if (req.cmd_req_len == 0 || req.cmd_req_len > data->client.sb_length ||
		req.resp_len > data->client.sb_length) {
		pr_err("cmd or response buffer length not valid\n");
		return -EINVAL;
	}

	send_cmd_req.cmd_req_buf = req.cmd_req_buf;
	send_cmd_req.cmd_req_len = req.cmd_req_len;
	send_cmd_req.resp_buf = req.resp_buf;
	send_cmd_req.resp_len = req.resp_len;

	/* validate offsets */
	for (i = 0; i < MAX_ION_FD; i++) {
		if (req.ifd_data[i].cmd_buf_offset >= req.cmd_req_len) {
			pr_err("Invalid offset %d = 0x%x\n",
				i, req.ifd_data[i].cmd_buf_offset);
			return -EINVAL;
		}
	}
	req.cmd_req_buf = (void *)__qseecom_uvirt_to_kvirt(data,
						(uint32_t)req.cmd_req_buf);
	req.resp_buf = (void *)__qseecom_uvirt_to_kvirt(data,
						(uint32_t)req.resp_buf);

	ret = __qseecom_update_cmd_buf(&req, false, data, false);
	if (ret)
		return ret;
	ret = __qseecom_send_cmd(data, &send_cmd_req);
	if (ret)
		return ret;
	ret = __qseecom_update_cmd_buf(&req, true, data, false);
	if (ret)
		return ret;

	return ret;
}

static int __qseecom_listener_has_rcvd_req(struct qseecom_dev_handle *data,
		struct qseecom_registered_listener_list *svc)
{
	int ret;
	ret = (svc->rcv_req_flag != 0);
	return ret || data->abort;
}

static int qseecom_receive_req(struct qseecom_dev_handle *data)
{
	int ret = 0;
	struct qseecom_registered_listener_list *this_lstnr;

	this_lstnr = __qseecom_find_svc(data->listener.id);
	if (!this_lstnr) {
		pr_err("Invalid listener ID\n");
		return -ENODATA;
	}

	while (1) {
		if (wait_event_freezable(this_lstnr->rcv_req_wq,
				__qseecom_listener_has_rcvd_req(data,
				this_lstnr))) {
			pr_debug("Interrupted: exiting Listener Service = %d\n",
						(uint32_t)data->listener.id);
			/* woken up for different reason */
			return -ERESTARTSYS;
		}

		if (data->abort) {
			pr_err("Aborting Listener Service = %d\n",
						(uint32_t)data->listener.id);
			return -ENODEV;
		}
		this_lstnr->rcv_req_flag = 0;
		break;
	}
	return ret;
}

static bool __qseecom_is_fw_image_valid(const struct firmware *fw_entry)
{
	struct elf32_hdr *ehdr;

	if (fw_entry->size < sizeof(*ehdr)) {
		pr_err("%s: Not big enough to be an elf header\n",
				 qseecom.pdev->init_name);
		return false;
	}
	ehdr = (struct elf32_hdr *)fw_entry->data;
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		pr_err("%s: Not an elf header\n",
				 qseecom.pdev->init_name);
		return false;
	}

	if (ehdr->e_phnum == 0) {
		pr_err("%s: No loadable segments\n",
				 qseecom.pdev->init_name);
		return false;
	}
	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw_entry->size) {
		pr_err("%s: Program headers not within mdt\n",
				 qseecom.pdev->init_name);
		return false;
	}
	return true;
}

static int __qseecom_get_fw_size(char *appname, uint32_t *fw_size)
{
	int ret = -1;
	int i = 0, rc = 0;
	const struct firmware *fw_entry = NULL;
	struct elf32_phdr *phdr;
	char fw_name[MAX_APP_NAME_SIZE];
	struct elf32_hdr *ehdr;
	int num_images = 0;

	snprintf(fw_name, sizeof(fw_name), "%s.mdt", appname);
	rc = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
	if (rc) {
		pr_err("error with request_firmware\n");
		ret = -EIO;
		goto err;
	}
	if (!__qseecom_is_fw_image_valid(fw_entry)) {
		ret = -EIO;
		goto err;
	}
	*fw_size = fw_entry->size;
	phdr = (struct elf32_phdr *)(fw_entry->data + sizeof(struct elf32_hdr));
	ehdr = (struct elf32_hdr *)fw_entry->data;
	num_images = ehdr->e_phnum;
	release_firmware(fw_entry);
	for (i = 0; i < num_images; i++, phdr++) {
		memset(fw_name, 0, sizeof(fw_name));
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d", appname, i);
		ret = request_firmware(&fw_entry, fw_name, qseecom.pdev);
		if (ret)
			goto err;
		*fw_size += fw_entry->size;
		release_firmware(fw_entry);
	}
	return ret;
err:
	if (fw_entry)
		release_firmware(fw_entry);
	*fw_size = 0;
	return ret;
}

static int __qseecom_get_fw_data(char *appname, u8 *img_data,
					struct qseecom_load_app_ireq *load_req)
{
	int ret = -1;
	int i = 0, rc = 0;
	const struct firmware *fw_entry = NULL;
	char fw_name[MAX_APP_NAME_SIZE];
	u8 *img_data_ptr = img_data;
	struct elf32_hdr *ehdr;
	int num_images = 0;

	snprintf(fw_name, sizeof(fw_name), "%s.mdt", appname);
	rc = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
	if (rc) {
		ret = -EIO;
		goto err;
	}
	load_req->img_len = fw_entry->size;
	memcpy(img_data_ptr, fw_entry->data, fw_entry->size);
	img_data_ptr = img_data_ptr + fw_entry->size;
	load_req->mdt_len = fw_entry->size; /*Get MDT LEN*/
	ehdr = (struct elf32_hdr *)fw_entry->data;
	num_images = ehdr->e_phnum;
	release_firmware(fw_entry);
	for (i = 0; i < num_images; i++) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d", appname, i);
		ret = request_firmware(&fw_entry, fw_name,  qseecom.pdev);
		if (ret) {
			pr_err("Failed to locate blob %s\n", fw_name);
			goto err;
		}
		memcpy(img_data_ptr, fw_entry->data, fw_entry->size);
		img_data_ptr = img_data_ptr + fw_entry->size;
		load_req->img_len += fw_entry->size;
		release_firmware(fw_entry);
	}
	load_req->phy_addr = virt_to_phys(img_data);
	return ret;
err:
	release_firmware(fw_entry);
	return ret;
}

static int __qseecom_load_fw(struct qseecom_dev_handle *data, char *appname)
{
	int ret = -1;
	uint32_t fw_size = 0;
	struct qseecom_load_app_ireq load_req = {0, 0, 0, 0};
	struct qseecom_command_scm_resp resp;
	u8 *img_data = NULL;

	if (__qseecom_get_fw_size(appname, &fw_size))
		return -EIO;

	img_data = kzalloc(fw_size, GFP_KERNEL);
	if (!img_data) {
		pr_err("Failied to allocate memory for copying image data\n");
		return -ENOMEM;
	}
	ret = __qseecom_get_fw_data(appname, img_data, &load_req);
	if (ret) {
		kzfree(img_data);
		return -EIO;
	}

	/* Populate the remaining parameters */
	load_req.qsee_cmd_id = QSEOS_APP_START_COMMAND;
	memcpy(load_req.app_name, appname, MAX_APP_NAME_SIZE);

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			kzfree(img_data);
			return ret;
		}
	}

	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto loadfw_err;
	}

	__cpuc_flush_dcache_area((void *)img_data, fw_size);
	/* SCM_CALL to load the image */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,	&load_req,
			sizeof(struct qseecom_load_app_ireq),
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n", ret);
		ret = -EIO;
		goto loadfw_err;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		ret = resp.data;
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd FAILED\n");
		else
			ret = resp.data;
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm call failed with response QSEOS_RESULT FAILURE\n");
		break;
	default:
		pr_err("scm call return unknown response %d\n", resp.result);
		ret = -EINVAL;
		break;
	}

loadfw_err:
	kzfree(img_data);
	__qseecom_disable_clk_scale_down(data);
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}
	return ret;
}

static int qseecom_load_commonlib_image(struct qseecom_dev_handle *data)
{
	int ret = 0;
	int len = 0;
	uint32_t fw_size = 0;
	struct qseecom_load_app_ireq load_req = {0, 0, 0, 0};
	struct qseecom_command_scm_resp resp;
	u8 *img_data = NULL;
	ion_phys_addr_t pa;

	if (__qseecom_get_fw_size("cmnlib", &fw_size))
		return -EIO;

	qseecom.cmnlib_ion_handle = ion_alloc(qseecom.ion_clnt, fw_size,
					SZ_4K, ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(qseecom.cmnlib_ion_handle)) {
		pr_err("ION alloc failed\n");
		return -ENOMEM;
	}

	img_data = (u8 *)ion_map_kernel(qseecom.ion_clnt,
					qseecom.cmnlib_ion_handle);
	if (IS_ERR_OR_NULL(img_data)) {
		pr_err("ION memory mapping for cmnlib failed\n");
		ret = -ENOMEM;
		goto exit_ion_free;
	}
	ret = __qseecom_get_fw_data("cmnlib", img_data, &load_req);
	if (ret) {
		ret = -EIO;
		goto exit_ion_unmap_kernel;
	}
	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, qseecom.cmnlib_ion_handle,
					&pa, &len);
	load_req.phy_addr = (s32)pa;
	if (ret) {
		pr_err("physical memory retrieval failure\n");
		ret = -EIO;
		goto exit_ion_unmap_kernel;
	}
	/* Populate the remaining parameters */
	load_req.qsee_cmd_id = QSEOS_LOAD_SERV_IMAGE_COMMAND;

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			ret = -EIO;
			goto exit_ion_unmap_kernel;
		}
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto exit_unregister_bus_bw_need;
	}

	__cpuc_flush_dcache_area((void *)img_data, fw_size);
	/* SCM_CALL to load the image */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1, &load_req,
				sizeof(struct qseecom_load_lib_image_ireq),
							&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n", ret);
		ret = -EIO;
		goto exit_disable_clk_vote;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm call failed w/response result%d\n", resp.result);
		ret = -EINVAL;
		goto exit_disable_clk_vote;
	case  QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd failed err: %d\n", ret);
			goto exit_disable_clk_vote;
		}
		break;
	default:
		pr_err("scm call return unknown response %d\n",	resp.result);
		ret = -EINVAL;
		goto exit_disable_clk_vote;
	}
	__qseecom_disable_clk_scale_down(data);
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}
	return ret;

exit_disable_clk_vote:
	__qseecom_disable_clk_scale_down(data);

exit_unregister_bus_bw_need:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}

exit_ion_unmap_kernel:
	ion_unmap_kernel(qseecom.ion_clnt, qseecom.cmnlib_ion_handle);

exit_ion_free:
	ion_free(qseecom.ion_clnt, qseecom.cmnlib_ion_handle);
	qseecom.cmnlib_ion_handle = NULL;
	return ret;
}

static int qseecom_unload_commonlib_image(void)
{
	int ret = -EINVAL;
	struct qseecom_unload_lib_image_ireq unload_req = {0};
	struct qseecom_command_scm_resp resp;

	/* Populate the remaining parameters */
	unload_req.qsee_cmd_id = QSEOS_UNLOAD_SERV_IMAGE_COMMAND;
	/* SCM_CALL to load the image */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,	&unload_req,
			sizeof(struct qseecom_unload_lib_image_ireq),
						&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to unload lib failed : ret %d\n", ret);
		ret = -EIO;
	} else {
		switch (resp.result) {
		case QSEOS_RESULT_SUCCESS:
			break;
		case QSEOS_RESULT_FAILURE:
			pr_err("scm fail resp.result QSEOS_RESULT FAILURE\n");
			break;
		default:
			pr_err("scm call return unknown response %d\n",
					resp.result);
			ret = -EINVAL;
			break;
		}
	}

	ion_unmap_kernel(qseecom.ion_clnt, qseecom.cmnlib_ion_handle);
	ion_free(qseecom.ion_clnt, qseecom.cmnlib_ion_handle);
	qseecom.cmnlib_ion_handle = NULL;

	return ret;
}

int qseecom_start_app(struct qseecom_handle **handle,
						char *app_name, uint32_t size)
{
	int32_t ret = 0;
	unsigned long flags = 0;
	struct qseecom_dev_handle *data = NULL;
	struct qseecom_check_app_ireq app_ireq;
	struct qseecom_registered_app_list *entry = NULL;
	struct qseecom_registered_kclient_list *kclient_entry = NULL;
	bool found_app = false;
	uint32_t len;
	ion_phys_addr_t pa;

	*handle = kzalloc(sizeof(struct qseecom_handle), GFP_KERNEL);
	if (!(*handle)) {
		pr_err("failed to allocate memory for kernel client handle\n");
		return -ENOMEM;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("kmalloc failed\n");
		if (ret == 0) {
			kfree(*handle);
			*handle = NULL;
		}
		return -ENOMEM;
	}
	data->abort = 0;
	data->type = QSEECOM_CLIENT_APP;
	data->released = false;
	data->client.sb_length = size;
	data->client.user_virt_sb_base = 0;
	data->client.ihandle = NULL;

	init_waitqueue_head(&data->abort_wq);
	atomic_set(&data->ioctl_count, 0);

	data->client.ihandle = ion_alloc(qseecom.ion_clnt, size, 4096,
				ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(data->client.ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		kfree(data);
		kfree(*handle);
		*handle = NULL;
		return -EINVAL;
	}
	mutex_lock(&app_access_lock);
	if (qseecom.qsee_version > QSEEE_VERSION_00) {
		if (qseecom.commonlib_loaded == false) {
			ret = qseecom_load_commonlib_image(data);
			if (ret == 0)
				qseecom.commonlib_loaded = true;
		}
	}
	if (ret) {
		pr_err("Failed to load commonlib image\n");
		ret = -EIO;
		goto err;
	}

	app_ireq.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	memcpy(app_ireq.app_name, app_name, MAX_APP_NAME_SIZE);
	ret = __qseecom_check_app_exists(app_ireq);
	if (ret < 0)
		goto err;

	data->client.app_id = ret;
	if (ret > 0) {
		pr_warn("App id %d for [%s] app exists\n", ret,
			(char *)app_ireq.app_name);
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
				&qseecom.registered_app_list_head, list){
			if (entry->app_id == ret) {
				entry->ref_cnt++;
				found_app = true;
				break;
			}
		}
		spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
		if (!found_app)
			pr_warn("App_id %d [%s] was loaded but not registered\n",
					ret, (char *)app_ireq.app_name);
	} else {
		/* load the app and get the app_id  */
		pr_debug("%s: Loading app for the first time'\n",
				qseecom.pdev->init_name);
		ret = __qseecom_load_fw(data, app_name);
		if (ret < 0)
			goto err;
		data->client.app_id = ret;
	}
	if (!found_app) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			pr_err("kmalloc for app entry failed\n");
			ret =  -ENOMEM;
			goto err;
		}
		entry->app_id = ret;
		entry->ref_cnt = 1;

		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_add_tail(&entry->list, &qseecom.registered_app_list_head);
		spin_unlock_irqrestore(&qseecom.registered_app_list_lock,
									flags);
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, data->client.ihandle, &pa, &len);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		goto err;
	}

	/* Populate the structure for sending scm call to load image */
	data->client.sb_virt = (char *) ion_map_kernel(qseecom.ion_clnt,
							data->client.ihandle);
	data->client.user_virt_sb_base = (uint32_t)data->client.sb_virt;
	data->client.sb_phys = pa;
	(*handle)->dev = (void *)data;
	(*handle)->sbuf = (unsigned char *)data->client.sb_virt;
	(*handle)->sbuf_len = data->client.sb_length;

	kclient_entry = kzalloc(sizeof(*kclient_entry), GFP_KERNEL);
	if (!kclient_entry) {
		pr_err("kmalloc failed\n");
		ret = -ENOMEM;
		goto err;
	}
	kclient_entry->handle = *handle;

	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);
	list_add_tail(&kclient_entry->list,
			&qseecom.registered_kclient_list_head);
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);

	mutex_unlock(&app_access_lock);
	return 0;

err:
	kfree(data);
	kfree(*handle);
	*handle = NULL;
	mutex_unlock(&app_access_lock);
	return ret;
}
EXPORT_SYMBOL(qseecom_start_app);

int qseecom_shutdown_app(struct qseecom_handle **handle)
{
	int ret = -EINVAL;
	struct qseecom_dev_handle *data;

	struct qseecom_registered_kclient_list *kclient = NULL;
	unsigned long flags = 0;
	bool found_handle = false;

	if ((handle == NULL)  || (*handle == NULL)) {
		pr_err("Handle is not initialized\n");
		return -EINVAL;
	}
	data =	(struct qseecom_dev_handle *) ((*handle)->dev);
	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);
	list_for_each_entry(kclient, &qseecom.registered_kclient_list_head,
				list) {
		if (kclient->handle == (*handle)) {
			list_del(&kclient->list);
			found_handle = true;
			break;
		}
	}
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);
	if (!found_handle)
		pr_err("Unable to find the handle, exiting\n");
	else
		ret = qseecom_unload_app(data, false);

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		if (data->mode != INACTIVE) {
			qseecom_unregister_bus_bandwidth_needs(data);
			if (qseecom.cumulative_mode == INACTIVE) {
				ret = __qseecom_set_msm_bus_request(INACTIVE);
				if (ret)
					pr_err("Fail to scale down bus\n");
			}
		}
		mutex_unlock(&qsee_bw_mutex);
	} else {
		if (data->fast_load_enabled == true)
			qsee_disable_clock_vote(data, CLK_SFPB);
		if (data->perf_enabled == true)
			qsee_disable_clock_vote(data, CLK_DFAB);
	}
	if (ret == 0) {
		kzfree(data);
		kzfree(*handle);
		kzfree(kclient);
		*handle = NULL;
	}
	return ret;
}
EXPORT_SYMBOL(qseecom_shutdown_app);

int qseecom_send_command(struct qseecom_handle *handle, void *send_buf,
			uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len)
{
	int ret = 0;
	struct qseecom_send_cmd_req req = {0, 0, 0, 0};
	struct qseecom_dev_handle *data;
	bool perf_enabled = false;

	if (handle == NULL) {
		pr_err("Handle is not initialized\n");
		return -EINVAL;
	}
	data = handle->dev;

	req.cmd_req_len = sbuf_len;
	req.resp_len = rbuf_len;
	req.cmd_req_buf = send_buf;
	req.resp_buf = resp_buf;

	mutex_lock(&app_access_lock);
	atomic_inc(&data->ioctl_count);
	if (qseecom.support_bus_scaling) {
		ret = qseecom_scale_bus_bandwidth_timer(INACTIVE);
		if (ret) {
			pr_err("Failed to set bw.\n");
			atomic_dec(&data->ioctl_count);
			mutex_unlock(&app_access_lock);
			return ret;
		}
	}
	/*
	* On targets where crypto clock is handled by HLOS,
	* if clk_access_cnt is zero and perf_enabled is false,
	* then the crypto clock was not enabled before sending cmd
	* to tz, qseecom will enable the clock to avoid service failure.
	*/
	if (!qseecom.qsee.clk_access_cnt && !data->perf_enabled) {
		pr_debug("ce clock is not enabled!\n");
		ret = qseecom_perf_enable(data);
		if (ret) {
			pr_err("Failed to vote for clock with err %d\n",
						ret);
			atomic_dec(&data->ioctl_count);
			mutex_unlock(&app_access_lock);
			return -EINVAL;
		}
		perf_enabled = true;
	}

	ret = __qseecom_send_cmd(data, &req);
	if (qseecom.support_bus_scaling)
		__qseecom_add_bw_scale_down_timer(
			QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);

	if (perf_enabled) {
		qsee_disable_clock_vote(data, CLK_DFAB);
		qsee_disable_clock_vote(data, CLK_SFPB);
	}

	atomic_dec(&data->ioctl_count);
	mutex_unlock(&app_access_lock);

	if (ret)
		return ret;

	pr_debug("sending cmd_req->rsp size: %u, ptr: 0x%p\n",
			req.resp_len, req.resp_buf);
	return ret;
}
EXPORT_SYMBOL(qseecom_send_command);

int qseecom_set_bandwidth(struct qseecom_handle *handle, bool high)
{
	int ret = 0;
	if ((handle == NULL) || (handle->dev == NULL)) {
		pr_err("No valid kernel client\n");
		return -EINVAL;
	}
	if (high) {
		if (qseecom.support_bus_scaling) {
			mutex_lock(&qsee_bw_mutex);
			__qseecom_register_bus_bandwidth_needs(handle->dev,
									HIGH);
			mutex_unlock(&qsee_bw_mutex);
			if (ret)
				pr_err("Failed to scale bus (med) %d\n", ret);
		} else {
			ret = qseecom_perf_enable(handle->dev);
			if (ret)
				pr_err("Failed to vote for clock with err %d\n",
						ret);
		}
	} else {
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(handle->dev, CLK_DFAB);
			qsee_disable_clock_vote(handle->dev, CLK_SFPB);
		} else {
			mutex_lock(&qsee_bw_mutex);
			qseecom_unregister_bus_bandwidth_needs(handle->dev);
			mutex_unlock(&qsee_bw_mutex);
		}
	}
	return ret;
}
EXPORT_SYMBOL(qseecom_set_bandwidth);

static int qseecom_send_resp(void)
{
	qseecom.send_resp_flag = 1;
	wake_up_interruptible(&qseecom.send_resp_wq);
	return 0;
}


static int qseecom_send_modfd_resp(struct qseecom_dev_handle *data,
						void __user *argp)
{
	struct qseecom_send_modfd_listener_resp resp;
	int i;
	struct qseecom_registered_listener_list *this_lstnr = NULL;

	if (copy_from_user(&resp, argp, sizeof(resp))) {
		pr_err("copy_from_user failed");
		return -EINVAL;
	}
	this_lstnr = __qseecom_find_svc(data->listener.id);
	if (this_lstnr == NULL)
		return -EINVAL;

	if (resp.resp_buf_ptr == NULL) {
		pr_err("Invalid resp_buf_ptr\n");
		return -EINVAL;
	}
	/* validate offsets */
	for (i = 0; i < MAX_ION_FD; i++) {
		if (resp.ifd_data[i].cmd_buf_offset >= resp.resp_len) {
			pr_err("Invalid offset %d = 0x%x\n",
				i, resp.ifd_data[i].cmd_buf_offset);
			return -EINVAL;
		}
	}

	if (((uint32_t)resp.resp_buf_ptr <
			this_lstnr->user_virt_sb_base)
			|| ((uint32_t)resp.resp_buf_ptr >=
			(this_lstnr->user_virt_sb_base +
			this_lstnr->sb_length))) {
		pr_err("resp_buf_ptr address not within shared buffer\n");
		return -EINVAL;
	}
	resp.resp_buf_ptr = (uint32_t)this_lstnr->sb_virt +
		(resp.resp_buf_ptr - this_lstnr->user_virt_sb_base);
	__qseecom_update_cmd_buf(&resp, false, data, true);
	qseecom.send_resp_flag = 1;
	wake_up_interruptible(&qseecom.send_resp_wq);
	return 0;
}


static int qseecom_get_qseos_version(struct qseecom_dev_handle *data,
						void __user *argp)
{
	struct qseecom_qseos_version_req req;

	if (copy_from_user(&req, argp, sizeof(req))) {
		pr_err("copy_from_user failed");
		return -EINVAL;
	}
	req.qseos_version = qseecom.qseos_version;
	if (copy_to_user(argp, &req, sizeof(req))) {
		pr_err("copy_to_user failed");
		return -EINVAL;
	}
	return 0;
}

static int __qseecom_enable_clk(enum qseecom_ce_hw_instance ce)
{
	int rc = 0;
	struct qseecom_clk *qclk;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	mutex_lock(&clk_access_lock);

	if (qclk->clk_access_cnt == ULONG_MAX)
		goto err;

	if (qclk->clk_access_cnt > 0) {
		qclk->clk_access_cnt++;
		mutex_unlock(&clk_access_lock);
		return rc;
	}

	/* Enable CE core clk */
	rc = clk_prepare_enable(qclk->ce_core_clk);
	if (rc) {
		pr_err("Unable to enable/prepare CE core clk\n");
		goto err;
	}
	/* Enable CE clk */
	rc = clk_prepare_enable(qclk->ce_clk);
	if (rc) {
		pr_err("Unable to enable/prepare CE iface clk\n");
		goto ce_clk_err;
	}
	/* Enable AXI clk */
	rc = clk_prepare_enable(qclk->ce_bus_clk);
	if (rc) {
		pr_err("Unable to enable/prepare CE bus clk\n");
		goto ce_bus_clk_err;
	}
	qclk->clk_access_cnt++;
	mutex_unlock(&clk_access_lock);
	return 0;

ce_bus_clk_err:
	clk_disable_unprepare(qclk->ce_clk);
ce_clk_err:
	clk_disable_unprepare(qclk->ce_core_clk);
err:
	mutex_unlock(&clk_access_lock);
	return -EIO;
}

static void __qseecom_disable_clk(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	mutex_lock(&clk_access_lock);

	if (qclk->clk_access_cnt == 0) {
		mutex_unlock(&clk_access_lock);
		return;
	}

	if (qclk->clk_access_cnt == 1) {
		if (qclk->ce_clk != NULL)
			clk_disable_unprepare(qclk->ce_clk);
		if (qclk->ce_core_clk != NULL)
			clk_disable_unprepare(qclk->ce_core_clk);
		if (qclk->ce_bus_clk != NULL)
			clk_disable_unprepare(qclk->ce_bus_clk);
	}
	qclk->clk_access_cnt--;
	mutex_unlock(&clk_access_lock);
}

static int qsee_vote_for_clock(struct qseecom_dev_handle *data,
						int32_t clk_type)
{
	int ret = 0;
	struct qseecom_clk *qclk;

	qclk = &qseecom.qsee;
	if (!qseecom.qsee_perf_client)
		return ret;

	switch (clk_type) {
	case CLK_DFAB:
		mutex_lock(&qsee_bw_mutex);
		if (!qseecom.qsee_bw_count) {
			if (qseecom.qsee_sfpb_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 3);
			else {
				if (qclk->ce_core_src_clk != NULL)
					ret = __qseecom_enable_clk(CLK_QSEE);
				if (!ret) {
					ret =
					msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 1);
					if ((ret) &&
						(qclk->ce_core_src_clk != NULL))
						__qseecom_disable_clk(CLK_QSEE);
				}
			}
			if (ret)
				pr_err("DFAB Bandwidth req failed (%d)\n",
								ret);
			else {
				qseecom.qsee_bw_count++;
				data->perf_enabled = true;
			}
		} else {
			qseecom.qsee_bw_count++;
			data->perf_enabled = true;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	case CLK_SFPB:
		mutex_lock(&qsee_bw_mutex);
		if (!qseecom.qsee_sfpb_bw_count) {
			if (qseecom.qsee_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 3);
			else {
				if (qclk->ce_core_src_clk != NULL)
					ret = __qseecom_enable_clk(CLK_QSEE);
				if (!ret) {
					ret =
					msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 2);
					if ((ret) &&
						(qclk->ce_core_src_clk != NULL))
						__qseecom_disable_clk(CLK_QSEE);
				}
			}

			if (ret)
				pr_err("SFPB Bandwidth req failed (%d)\n",
								ret);
			else {
				qseecom.qsee_sfpb_bw_count++;
				data->fast_load_enabled = true;
			}
		} else {
			qseecom.qsee_sfpb_bw_count++;
			data->fast_load_enabled = true;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	default:
		pr_err("Clock type not defined\n");
		break;
	}
	return ret;
}

static void qsee_disable_clock_vote(struct qseecom_dev_handle *data,
						int32_t clk_type)
{
	int32_t ret = 0;
	struct qseecom_clk *qclk;

	qclk = &qseecom.qsee;
	if (!qseecom.qsee_perf_client)
		return;

	switch (clk_type) {
	case CLK_DFAB:
		mutex_lock(&qsee_bw_mutex);
		if (qseecom.qsee_bw_count == 0) {
			pr_err("Client error.Extra call to disable DFAB clk\n");
			mutex_unlock(&qsee_bw_mutex);
			return;
		}

		if (qseecom.qsee_bw_count == 1) {
			if (qseecom.qsee_sfpb_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
					qseecom.qsee_perf_client, 2);
			else {
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 0);
				if ((!ret) && (qclk->ce_core_src_clk != NULL))
					__qseecom_disable_clk(CLK_QSEE);
			}
			if (ret)
				pr_err("SFPB Bandwidth req fail (%d)\n",
								ret);
			else {
				qseecom.qsee_bw_count--;
				data->perf_enabled = false;
			}
		} else {
			qseecom.qsee_bw_count--;
			data->perf_enabled = false;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	case CLK_SFPB:
		mutex_lock(&qsee_bw_mutex);
		if (qseecom.qsee_sfpb_bw_count == 0) {
			pr_err("Client error.Extra call to disable SFPB clk\n");
			mutex_unlock(&qsee_bw_mutex);
			return;
		}
		if (qseecom.qsee_sfpb_bw_count == 1) {
			if (qseecom.qsee_bw_count > 0)
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 1);
			else {
				ret = msm_bus_scale_client_update_request(
						qseecom.qsee_perf_client, 0);
				if ((!ret) && (qclk->ce_core_src_clk != NULL))
					__qseecom_disable_clk(CLK_QSEE);
			}
			if (ret)
				pr_err("SFPB Bandwidth req fail (%d)\n",
								ret);
			else {
				qseecom.qsee_sfpb_bw_count--;
				data->fast_load_enabled = false;
			}
		} else {
			qseecom.qsee_sfpb_bw_count--;
			data->fast_load_enabled = false;
		}
		mutex_unlock(&qsee_bw_mutex);
		break;
	default:
		pr_err("Clock type not defined\n");
		break;
	}

}

static int qseecom_load_external_elf(struct qseecom_dev_handle *data,
				void __user *argp)
{
	struct ion_handle *ihandle;	/* Ion handle */
	struct qseecom_load_img_req load_img_req;
	int ret;
	int set_cpu_ret = 0;
	ion_phys_addr_t pa = 0;
	uint32_t len;
	struct cpumask mask;
	struct qseecom_load_app_ireq load_req;
	struct qseecom_command_scm_resp resp;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&load_img_req,
				(void __user *)argp,
				sizeof(struct qseecom_load_img_req))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	/* Get the handle of the shared fd */
	ihandle = ion_import_dma_buf(qseecom.ion_clnt,
				load_img_req.ifd_data_fd);
	if (IS_ERR_OR_NULL(ihandle)) {
		pr_err("Ion client could not retrieve the handle\n");
		return -ENOMEM;
	}

	/* Get the physical address of the ION BUF */
	ret = ion_phys(qseecom.ion_clnt, ihandle, &pa, &len);
	if (ret) {
		pr_err("Cannot get phys_addr for the Ion Client, ret = %d\n",
			ret);
		return ret;
	}
	/* Populate the structure for sending scm call to load image */
	load_req.qsee_cmd_id = QSEOS_LOAD_EXTERNAL_ELF_COMMAND;
	load_req.mdt_len = load_img_req.mdt_len;
	load_req.img_len = load_img_req.img_len;
	load_req.phy_addr = pa;

	/* SCM_CALL tied to Core0 */
	mask = CPU_MASK_CPU0;
	set_cpu_ret = set_cpus_allowed_ptr(current, &mask);
	if (set_cpu_ret) {
		pr_err("set_cpus_allowed_ptr failed : ret %d\n",
				set_cpu_ret);
		ret = -EFAULT;
		goto exit_ion_free;
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = __qseecom_register_bus_bandwidth_needs(data, MEDIUM);
		mutex_unlock(&qsee_bw_mutex);
		if (ret) {
			ret = -EIO;
			goto exit_cpu_restore;
		}
	}

	/* Vote for the SFPB clock */
	ret = __qseecom_enable_clk_scale_up(data);
	if (ret) {
		ret = -EIO;
		goto exit_register_bus_bandwidth_needs;
	}
	msm_ion_do_cache_op(qseecom.ion_clnt, ihandle, NULL, len,
				ION_IOC_CLEAN_INV_CACHES);
	/*  SCM_CALL to load the external elf */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &load_req,
			sizeof(struct qseecom_load_app_ireq),
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to load failed : ret %d\n",
				ret);
		ret = -EFAULT;
		goto exit_disable_clock;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		pr_err("%s: qseos result incomplete\n", __func__);
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd failed: err: %d\n", ret);
		break;
	case QSEOS_RESULT_FAILURE:
		pr_err("scm_call rsp.result is QSEOS_RESULT_FAILURE\n");
		ret = -EFAULT;
		break;
	default:
		pr_err("scm_call response result %d not supported\n",
							resp.result);
		ret = -EFAULT;
		break;
	}

exit_disable_clock:
	__qseecom_disable_clk_scale_down(data);

exit_register_bus_bandwidth_needs:
	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		ret = qseecom_unregister_bus_bandwidth_needs(data);
		mutex_unlock(&qsee_bw_mutex);
	}

exit_cpu_restore:
	/* Restore the CPU mask */
	mask = CPU_MASK_ALL;
	set_cpu_ret = set_cpus_allowed_ptr(current, &mask);
	if (set_cpu_ret) {
		pr_err("set_cpus_allowed_ptr failed to restore mask: ret %d\n",
				set_cpu_ret);
		ret = -EFAULT;
	}
exit_ion_free:
	/* Deallocate the handle */
	if (!IS_ERR_OR_NULL(ihandle))
		ion_free(qseecom.ion_clnt, ihandle);
	return ret;
}

static int qseecom_unload_external_elf(struct qseecom_dev_handle *data)
{
	int ret = 0;
	int set_cpu_ret = 0;
	struct qseecom_command_scm_resp resp;
	struct qseecom_unload_app_ireq req;
	struct cpumask mask;

	/* unavailable client app */
	data->type = QSEECOM_UNAVAILABLE_CLIENT_APP;

	/* Populate the structure for sending scm call to unload image */
	req.qsee_cmd_id = QSEOS_UNLOAD_EXTERNAL_ELF_COMMAND;

	/* SCM_CALL tied to Core0 */
	mask = CPU_MASK_CPU0;
	ret = set_cpus_allowed_ptr(current, &mask);
	if (ret) {
		pr_err("set_cpus_allowed_ptr failed : ret %d\n",
				ret);
		return -EFAULT;
	}

	/* SCM_CALL to unload the external elf */
	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req,
			sizeof(struct qseecom_unload_app_ireq),
			&resp, sizeof(resp));
	if (ret) {
		pr_err("scm_call to unload failed : ret %d\n",
				ret);
		ret = -EFAULT;
		goto qseecom_unload_external_elf_scm_err;
	}
	if (resp.result == QSEOS_RESULT_INCOMPLETE) {
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd fail err: %d\n",
					ret);
	} else {
		if (resp.result != QSEOS_RESULT_SUCCESS) {
			pr_err("scm_call to unload image failed resp.result =%d\n",
						resp.result);
			ret = -EFAULT;
		}
	}

qseecom_unload_external_elf_scm_err:
	/* Restore the CPU mask */
	mask = CPU_MASK_ALL;
	set_cpu_ret = set_cpus_allowed_ptr(current, &mask);
	if (set_cpu_ret) {
		pr_err("set_cpus_allowed_ptr failed to restore mask: ret %d\n",
				set_cpu_ret);
		ret = -EFAULT;
	}

	return ret;
}

static int qseecom_query_app_loaded(struct qseecom_dev_handle *data,
					void __user *argp)
{

	int32_t ret;
	struct qseecom_qseos_app_load_query query_req;
	struct qseecom_check_app_ireq req;
	struct qseecom_registered_app_list *entry = NULL;
	unsigned long flags = 0;

	/* Copy the relevant information needed for loading the image */
	if (copy_from_user(&query_req,
				(void __user *)argp,
				sizeof(struct qseecom_qseos_app_load_query))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	req.qsee_cmd_id = QSEOS_APP_LOOKUP_COMMAND;
	query_req.app_name[MAX_APP_NAME_SIZE-1] = '\0';
	memcpy(req.app_name, query_req.app_name, MAX_APP_NAME_SIZE);

	ret = __qseecom_check_app_exists(req);

	if ((ret == -EINVAL) || (ret == -ENODEV)) {
		pr_err(" scm call to check if app is loaded failed");
		return ret;	/* scm call failed */
	} else if (ret > 0) {
		pr_debug("App id %d (%s) already exists\n", ret,
			(char *)(req.app_name));
		spin_lock_irqsave(&qseecom.registered_app_list_lock, flags);
		list_for_each_entry(entry,
				&qseecom.registered_app_list_head, list){
			if (entry->app_id == ret) {
				entry->ref_cnt++;
				break;
			}
		}
		spin_unlock_irqrestore(
				&qseecom.registered_app_list_lock, flags);
		data->client.app_id = ret;
		query_req.app_id = ret;
		memset((void *)data->client.app_name, 0, MAX_APP_NAME_SIZE);
		memcpy((void *)data->client.app_name,
				(void *)query_req.app_name, MAX_APP_NAME_SIZE);
		if (copy_to_user(argp, &query_req, sizeof(query_req))) {
			pr_err("copy_to_user failed\n");
			return -EFAULT;
		}
		return -EEXIST;	/* app already loaded */
	} else {
		return 0;	/* app not loaded */
	}
}

static int __qseecom_get_ce_pipe_info(
			enum qseecom_key_management_usage_type usage,
			uint32_t *pipe, uint32_t *ce_hw)
{
	int ret;
	switch (usage) {
	case QSEOS_KM_USAGE_DISK_ENCRYPTION:
		if (qseecom.support_fde) {
			*pipe = qseecom.ce_info.disk_encrypt_pipe;
			*ce_hw = qseecom.ce_info.hlos_ce_hw_instance;
			ret = 0;

		} else {
			pr_err("info unavailable: disk encr pipe %d ce_hw %d\n",
				qseecom.ce_info.disk_encrypt_pipe,
				qseecom.ce_info.hlos_ce_hw_instance);
			ret = -EINVAL;
		}
		break;
	case QSEOS_KM_USAGE_FILE_ENCRYPTION:
		if (qseecom.support_pfe) {
			*pipe = qseecom.ce_info.file_encrypt_pipe;
			*ce_hw = qseecom.ce_info.hlos_ce_hw_instance;
			ret = 0;
		} else {
			pr_err("info unavailable: file encr pipe %d ce_hw %d\n",
				qseecom.ce_info.file_encrypt_pipe,
				qseecom.ce_info.hlos_ce_hw_instance);
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int __qseecom_generate_and_save_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_generate_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", usage);
		return -EFAULT;
	}
	__qseecom_enable_clk(CLK_QSEE);

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_generate_ireq),
				&resp, sizeof(resp));
	if (ret) {
		pr_err("scm call to generate key failed : %d\n", ret);
		__qseecom_disable_clk(CLK_QSEE);
		return -EFAULT;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_FAIL_KEY_ID_EXISTS:
		pr_debug("Key ID exists.\n");
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			if (resp.result == QSEOS_RESULT_FAIL_KEY_ID_EXISTS) {
				pr_debug("Key ID exists.\n");
				ret = 0;
			} else {
				pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			}
		}
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("gen key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}
	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static int __qseecom_delete_saved_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_delete_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
			pr_err("Error:: unsupported usage %d\n", usage);
			return -EFAULT;
	}

	__qseecom_enable_clk(CLK_QSEE);

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_delete_ireq),
				&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		pr_err("scm call to delete key failed : %d\n", ret);
		__qseecom_disable_clk(CLK_QSEE);
		return -EFAULT;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			if (resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
				pr_debug("Max attempts to input password reached.\n");
				ret = -ERANGE;
			}
		}
		break;
	case QSEOS_RESULT_FAIL_MAX_ATTEMPT:
		pr_debug("Max attempts to input password reached.\n");
		ret = -ERANGE;
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Delete key scm call failed resp.result %d\n",
							resp.result);
		ret = -EINVAL;
		break;
	}
	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static int __qseecom_set_clear_ce_key(struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_select_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
			pr_err("Error:: unsupported usage %d\n", usage);
			return -EFAULT;
	}

	__qseecom_enable_clk(CLK_QSEE);
	if (qseecom.qsee.instance != qseecom.ce_drv.instance)
		__qseecom_enable_clk(CLK_CE_DRV);

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,
				ireq, sizeof(struct qseecom_key_select_ireq),
				&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		pr_err("scm call to set QSEOS_PIPE_ENC key failed : %d\n", ret);
		__qseecom_disable_clk(CLK_QSEE);
		if (qseecom.qsee.instance != qseecom.ce_drv.instance)
			__qseecom_disable_clk(CLK_CE_DRV);
		return -EFAULT;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret) {
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
			if (resp.result == QSEOS_RESULT_FAIL_MAX_ATTEMPT) {
				pr_debug("Max attempts to input password reached.\n");
				ret = -ERANGE;
			}
		}
		break;
	case QSEOS_RESULT_FAIL_MAX_ATTEMPT:
		pr_debug("Max attempts to input password reached.\n");
		ret = -ERANGE;
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Set key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}

	__qseecom_disable_clk(CLK_QSEE);
	if (qseecom.qsee.instance != qseecom.ce_drv.instance)
		__qseecom_disable_clk(CLK_CE_DRV);

	return ret;
}

static int __qseecom_update_current_key_user_info(
			struct qseecom_dev_handle *data,
			enum qseecom_key_management_usage_type usage,
			struct qseecom_key_userinfo_update_ireq *ireq)
{
	struct qseecom_command_scm_resp resp;
	int ret;

	if (usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		usage >= QSEOS_KM_USAGE_MAX) {
			pr_err("Error:: unsupported usage %d\n", usage);
			return -EFAULT;
	}

	__qseecom_enable_clk(CLK_QSEE);

	ret = scm_call(SCM_SVC_TZSCHEDULER, 1,
		ireq, sizeof(struct qseecom_key_userinfo_update_ireq),
		&resp, sizeof(struct qseecom_command_scm_resp));
	if (ret) {
		pr_err("scm call to update key userinfo failed : %d\n", ret);
		__qseecom_disable_clk(CLK_QSEE);
		return -EFAULT;
	}

	switch (resp.result) {
	case QSEOS_RESULT_SUCCESS:
		break;
	case QSEOS_RESULT_INCOMPLETE:
		ret = __qseecom_process_incomplete_cmd(data, &resp);
		if (ret)
			pr_err("process_incomplete_cmd FAILED, resp.result %d\n",
					resp.result);
		break;
	case QSEOS_RESULT_FAILURE:
	default:
		pr_err("Set key scm call failed resp.result %d\n", resp.result);
		ret = -EINVAL;
		break;
	}

	__qseecom_disable_clk(CLK_QSEE);
	return ret;
}

static int qseecom_create_key(struct qseecom_dev_handle *data,
			void __user *argp)
{
	uint32_t ce_hw = 0;
	uint32_t pipe = 0;
	int ret = 0;
	uint32_t flags = 0;
	struct qseecom_create_key_req create_key_req;
	struct qseecom_key_generate_ireq generate_key_ireq;
	struct qseecom_key_select_ireq set_key_ireq;

	ret = copy_from_user(&create_key_req, argp, sizeof(create_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (create_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		create_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", create_key_req.usage);
		return -EFAULT;
	}

	ret = __qseecom_get_ce_pipe_info(create_key_req.usage, &pipe, &ce_hw);
	if (ret) {
		pr_err("Failed to retrieve pipe/ce_hw info: %d\n", ret);
		return -EINVAL;
	}

	generate_key_ireq.flags = flags;
	generate_key_ireq.qsee_command_id = QSEOS_GENERATE_KEY;
	memset((void *)generate_key_ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
	memset((void *)generate_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);
	memcpy((void *)generate_key_ireq.key_id,
			(void *)key_id_array[create_key_req.usage].desc,
			QSEECOM_KEY_ID_SIZE);
	memcpy((void *)generate_key_ireq.hash32,
			(void *)create_key_req.hash32, QSEECOM_HASH_SIZE);

	ret = __qseecom_generate_and_save_key(data, create_key_req.usage,
					&generate_key_ireq);
	if (ret) {
		pr_err("Failed to generate key on storage: %d\n", ret);
		return ret;
	}

	set_key_ireq.qsee_command_id = QSEOS_SET_KEY;
	set_key_ireq.ce = ce_hw;
	set_key_ireq.pipe = pipe;
	set_key_ireq.flags = flags;

	/* set both PIPE_ENC and PIPE_ENC_XTS*/
	set_key_ireq.pipe_type = QSEOS_PIPE_ENC|QSEOS_PIPE_ENC_XTS;
	memset((void *)set_key_ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
	memset((void *)set_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);
	memcpy((void *)set_key_ireq.key_id,
		(void *)key_id_array[create_key_req.usage].desc,
		QSEECOM_KEY_ID_SIZE);
	memcpy((void *)set_key_ireq.hash32, (void *)create_key_req.hash32,
				QSEECOM_HASH_SIZE);

	ret = __qseecom_set_clear_ce_key(data, create_key_req.usage,
								&set_key_ireq);
	if (ret) {
		pr_err("Failed to create key: pipe %d, ce %d: %d\n",
			pipe, ce_hw, ret);
		return ret;
	}

	return ret;
}

static int qseecom_wipe_key(struct qseecom_dev_handle *data,
				void __user *argp)
{
	uint32_t ce_hw = 0;
	uint32_t pipe = 0;
	int ret = 0;
	uint32_t flags = 0;
	int i;
	struct qseecom_wipe_key_req wipe_key_req;
	struct qseecom_key_delete_ireq delete_key_ireq;
	struct qseecom_key_select_ireq clear_key_ireq;

	ret = copy_from_user(&wipe_key_req, argp, sizeof(wipe_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (wipe_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		wipe_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", wipe_key_req.usage);
		return -EFAULT;
	}

	ret = __qseecom_get_ce_pipe_info(wipe_key_req.usage, &pipe, &ce_hw);
	if (ret) {
		pr_err("Failed to retrieve pipe/ce_hw info: %d\n", ret);
		return -EINVAL;
	}

	if (wipe_key_req.wipe_key_flag) {
		delete_key_ireq.flags = flags;
		delete_key_ireq.qsee_command_id = QSEOS_DELETE_KEY;
		memset((void *)delete_key_ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
		memcpy((void *)delete_key_ireq.key_id,
			(void *)key_id_array[wipe_key_req.usage].desc,
			QSEECOM_KEY_ID_SIZE);
		memset((void *)delete_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);

		ret = __qseecom_delete_saved_key(data, wipe_key_req.usage,
					&delete_key_ireq);
		if (ret) {
			pr_err("Failed to delete key from ssd storage: %d\n",
				ret);
			return -EFAULT;
		}
	}

	clear_key_ireq.qsee_command_id = QSEOS_SET_KEY;
	clear_key_ireq.ce = ce_hw;
	clear_key_ireq.pipe = pipe;
	clear_key_ireq.flags = flags;
	clear_key_ireq.pipe_type = QSEOS_PIPE_ENC|QSEOS_PIPE_ENC_XTS;
	for (i = 0; i < QSEECOM_KEY_ID_SIZE; i++)
		clear_key_ireq.key_id[i] = QSEECOM_INVALID_KEY_ID;
	memset((void *)clear_key_ireq.hash32, 0, QSEECOM_HASH_SIZE);

	ret = __qseecom_set_clear_ce_key(data, wipe_key_req.usage,
							&clear_key_ireq);
	if (ret) {
		pr_err("Failed to wipe key: pipe %d, ce %d: %d\n",
			pipe, ce_hw, ret);
		return -EFAULT;
	}

	return ret;
}

static int qseecom_update_key_user_info(struct qseecom_dev_handle *data,
			void __user *argp)
{
	int ret = 0;
	uint32_t flags = 0;
	struct qseecom_update_key_userinfo_req update_key_req;
	struct qseecom_key_userinfo_update_ireq ireq;

	ret = copy_from_user(&update_key_req, argp, sizeof(update_key_req));
	if (ret) {
		pr_err("copy_from_user failed\n");
		return ret;
	}

	if (update_key_req.usage < QSEOS_KM_USAGE_DISK_ENCRYPTION ||
		update_key_req.usage >= QSEOS_KM_USAGE_MAX) {
		pr_err("Error:: unsupported usage %d\n", update_key_req.usage);
		return -EFAULT;
	}

	ireq.qsee_command_id = QSEOS_UPDATE_KEY_USERINFO;
	ireq.flags = flags;
	memset(ireq.key_id, 0, QSEECOM_KEY_ID_SIZE);
	memset((void *)ireq.current_hash32, 0, QSEECOM_HASH_SIZE);
	memset((void *)ireq.new_hash32, 0, QSEECOM_HASH_SIZE);
	memcpy((void *)ireq.key_id,
		(void *)key_id_array[update_key_req.usage].desc,
		QSEECOM_KEY_ID_SIZE);
	memcpy((void *)ireq.current_hash32,
		(void *)update_key_req.current_hash32, QSEECOM_HASH_SIZE);
	memcpy((void *)ireq.new_hash32,
		(void *)update_key_req.new_hash32, QSEECOM_HASH_SIZE);

	ret = __qseecom_update_current_key_user_info(data, update_key_req.usage,
						&ireq);
	if (ret) {
		pr_err("Failed to update key info: %d\n", ret);
		return ret;
	}
	return ret;

}

static int qseecom_is_es_activated(void __user *argp)
{
	struct qseecom_is_es_activated_req req;
	int ret;
	int resp_buf;

	if (qseecom.qsee_version < QSEE_VERSION_04) {
		pr_err("invalid qsee version");
		return -ENODEV;
	}

	if (argp == NULL) {
		pr_err("arg is null");
		return -EINVAL;
	}

	ret = scm_call(SCM_SVC_ES, SCM_IS_ACTIVATED_ID, NULL, 0,
		       (void *) &resp_buf, sizeof(resp_buf));
	if (ret) {
		pr_err("scm_call failed");
		return ret;
	}

	req.is_activated = resp_buf;
	ret = copy_to_user(argp, &req, sizeof(req));
	if (ret) {
		pr_err("copy_to_user failed");
		return ret;
	}

	return 0;
}

static int qseecom_save_partition_hash(void __user *argp)
{
	struct qseecom_save_partition_hash_req req;
	int ret;

	if (qseecom.qsee_version < QSEE_VERSION_04) {
		pr_err("invalid qsee version ");
		return -ENODEV;
	}

	if (argp == NULL) {
		pr_err("arg is null");
		return -EINVAL;
	}

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("copy_from_user failed");
		return ret;
	}

	ret = scm_call(SCM_SVC_ES, SCM_SAVE_PARTITION_HASH_ID,
		       (void *) &req, sizeof(req), NULL, 0);
	if (ret) {
		pr_err("qseecom_scm_call failed");
		return ret;
	}

	return 0;
}

static long qseecom_ioctl(struct file *file, unsigned cmd,
		unsigned long arg)
{
	int ret = 0;
	struct qseecom_dev_handle *data = file->private_data;
	void __user *argp = (void __user *) arg;
	bool perf_enabled = false;

	if (!data) {
		pr_err("Invalid/uninitialized device handle\n");
		return -EINVAL;
	}

	if (data->abort) {
		pr_err("Aborting qseecom driver\n");
		return -ENODEV;
	}

	switch (cmd) {
	case QSEECOM_IOCTL_REGISTER_LISTENER_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("reg lstnr req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		pr_debug("ioctl register_listener_req()\n");
		atomic_inc(&data->ioctl_count);
		data->type = QSEECOM_LISTENER_SERVICE;
		ret = qseecom_register_listener(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_register_listener: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("unreg lstnr req: invalid handle (%d) lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		pr_debug("ioctl unregister_listener_req()\n");
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unregister_listener(data);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_unregister_listener: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_CMD_REQ: {
		pr_debug("qseecom.current_mode %d\n", qseecom.current_mode);
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("send cmd req: invalid handle (%d) app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		if (qseecom.support_bus_scaling) {
			/* register bus bw in case the client doesn't do it */
			if (!data->mode) {
				mutex_lock(&qsee_bw_mutex);
				__qseecom_register_bus_bandwidth_needs(
								data, HIGH);
				mutex_unlock(&qsee_bw_mutex);
			}
			ret = qseecom_scale_bus_bandwidth_timer(INACTIVE);
			if (ret) {
				pr_err("Failed to set bw.\n");
				ret = -EINVAL;
				mutex_unlock(&app_access_lock);
				break;
			}
		}
		/*
		* On targets where crypto clock is handled by HLOS,
		* if clk_access_cnt is zero and perf_enabled is false,
		* then the crypto clock was not enabled before sending cmd
		* to tz, qseecom will enable the clock to avoid service failure.
		*/
		if (!qseecom.qsee.clk_access_cnt && !data->perf_enabled) {
			pr_debug("ce clock is not enabled!\n");
			ret = qseecom_perf_enable(data);
			if (ret) {
				pr_err("Failed to vote for clock with err %d\n",
						ret);
				mutex_unlock(&app_access_lock);
				ret = -EINVAL;
				break;
			}
			perf_enabled = true;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_cmd(data, argp);
		if (qseecom.support_bus_scaling)
			__qseecom_add_bw_scale_down_timer(
				QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		if (perf_enabled) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		}
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_send_cmd: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_MODFD_CMD_REQ: {
		pr_debug("qseecom.current_mode %d\n", qseecom.current_mode);
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("send mdfd cmd: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		/* Only one client allowed here at a time */
		mutex_lock(&app_access_lock);
		if (qseecom.support_bus_scaling) {
			if (!data->mode) {
				mutex_lock(&qsee_bw_mutex);
				__qseecom_register_bus_bandwidth_needs(
								data, HIGH);
				mutex_unlock(&qsee_bw_mutex);
			}
			ret = qseecom_scale_bus_bandwidth_timer(INACTIVE);
			if (ret) {
				pr_err("Failed to set bw.\n");
				mutex_unlock(&app_access_lock);
				ret = -EINVAL;
				break;
			}
		}
		/*
		* On targets where crypto clock is handled by HLOS,
		* if clk_access_cnt is zero and perf_enabled is false,
		* then the crypto clock was not enabled before sending cmd
		* to tz, qseecom will enable the clock to avoid service failure.
		*/
		if (!qseecom.qsee.clk_access_cnt && !data->perf_enabled) {
			pr_debug("ce clock is not enabled!\n");
			ret = qseecom_perf_enable(data);
			if (ret) {
				pr_err("Failed to vote for clock with err %d\n",
						ret);
				mutex_unlock(&app_access_lock);
				ret = -EINVAL;
				break;
			}
			perf_enabled = true;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_modfd_cmd(data, argp);
		if (qseecom.support_bus_scaling)
			__qseecom_add_bw_scale_down_timer(
				QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		if (perf_enabled) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		}
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed qseecom_send_cmd: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_RECEIVE_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("receive req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_receive_req(data);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret && (ret != -ERESTARTSYS))
			pr_err("failed qseecom_receive_req: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SEND_RESP_REQ: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("send resp req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_resp();
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_send_resp: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_SET_MEM_PARAM_REQ: {
		if ((data->type != QSEECOM_CLIENT_APP) &&
			(data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_SECURE_SERVICE)) {
			pr_err("set mem param req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		pr_debug("SET_MEM_PARAM: qseecom addr = 0x%x\n", (u32)data);
		ret = qseecom_set_client_mem_param(data, argp);
		if (ret)
			pr_err("failed Qqseecom_set_mem_param request: %d\n",
								ret);
		break;
	}
	case QSEECOM_IOCTL_LOAD_APP_REQ: {
		if ((data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("load app req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_CLIENT_APP;
		pr_debug("LOAD_APP_REQ: qseecom_addr = 0x%x\n", (u32)data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		if (qseecom.qsee_version > QSEEE_VERSION_00) {
			if (qseecom.commonlib_loaded == false) {
				ret = qseecom_load_commonlib_image(data);
				if (ret == 0)
					qseecom.commonlib_loaded = true;
			}
		}
		if (ret == 0)
			ret = qseecom_load_app(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed load_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNLOAD_APP_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("unload app req:invalid handle(%d) app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		pr_debug("UNLOAD_APP: qseecom_addr = 0x%x\n", (u32)data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unload_app(data, false);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed unload_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ: {
		atomic_inc(&data->ioctl_count);
		ret = qseecom_get_qseos_version(data, argp);
		if (ret)
			pr_err("qseecom_get_qseos_version: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_PERF_ENABLE_REQ:{
		if ((data->type != QSEECOM_GENERIC) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("perf enable req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if ((data->type == QSEECOM_CLIENT_APP) &&
			(data->client.app_id == 0)) {
			pr_err("perf enable req:invalid handle(%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (qseecom.support_bus_scaling) {
			mutex_lock(&qsee_bw_mutex);
			__qseecom_register_bus_bandwidth_needs(data, HIGH);
			mutex_unlock(&qsee_bw_mutex);
		} else {
			ret = qseecom_perf_enable(data);
			if (ret)
				pr_err("Fail to vote for clocks %d\n", ret);
		}
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_PERF_DISABLE_REQ:{
		if ((data->type != QSEECOM_SECURE_SERVICE) &&
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("perf disable req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if ((data->type == QSEECOM_CLIENT_APP) &&
			(data->client.app_id == 0)) {
			pr_err("perf disable: invalid handle (%d)app_id(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		if (!qseecom.support_bus_scaling) {
			qsee_disable_clock_vote(data, CLK_DFAB);
			qsee_disable_clock_vote(data, CLK_SFPB);
		} else {
			mutex_lock(&qsee_bw_mutex);
			qseecom_unregister_bus_bandwidth_needs(data);
			mutex_unlock(&qsee_bw_mutex);
		}
		atomic_dec(&data->ioctl_count);
		break;
	}

	case QSEECOM_IOCTL_SET_BUS_SCALING_REQ: {
		if ((data->client.app_id == 0) ||
			(data->type != QSEECOM_CLIENT_APP)) {
			pr_err("set bus scale: invalid handle (%d) appid(%d)\n",
					data->type, data->client.app_id);
			ret = -EINVAL;
			break;
		}
		atomic_inc(&data->ioctl_count);
		ret = qseecom_scale_bus_bandwidth(data, argp);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("load ext elf req: invalid client handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_UNAVAILABLE_CLIENT_APP;
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_load_external_elf(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed load_external_elf request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ: {
		if (data->type != QSEECOM_UNAVAILABLE_CLIENT_APP) {
			pr_err("unload ext elf req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_unload_external_elf(data);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		if (ret)
			pr_err("failed unload_app request: %d\n", ret);
		break;
	}
	case QSEECOM_IOCTL_APP_LOADED_QUERY_REQ: {
		data->type = QSEECOM_CLIENT_APP;
		pr_debug("APP_LOAD_QUERY: qseecom_addr = 0x%x\n", (u32)data);
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		pr_debug("APP_LOAD_QUERY: qseecom_addr = 0x%x\n", (u32)data);
		ret = qseecom_query_app_loaded(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("send cmd svc req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->type = QSEECOM_SECURE_SERVICE;
		if (qseecom.qsee_version < QSEE_VERSION_03) {
			pr_err("SEND_CMD_SERVICE_REQ: Invalid qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_service_cmd(data, argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_CREATE_KEY_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("create key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Create Key feature unsupported: qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		atomic_inc(&data->ioctl_count);
		ret = qseecom_create_key(data, argp);
		if (ret)
			pr_err("failed to create encryption key: %d\n", ret);

		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_WIPE_KEY_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("wipe key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Wipe Key feature unsupported in qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		atomic_inc(&data->ioctl_count);
		ret = qseecom_wipe_key(data, argp);
		if (ret)
			pr_err("failed to wipe encryption key: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ: {
		if (!(qseecom.support_pfe || qseecom.support_fde))
			pr_err("Features requiring key init not supported\n");
		if (data->type != QSEECOM_GENERIC) {
			pr_err("update key req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		if (qseecom.qsee_version < QSEE_VERSION_05) {
			pr_err("Update Key feature unsupported in qsee ver %u\n",
				qseecom.qsee_version);
			return -EINVAL;
		}
		data->released = true;
		atomic_inc(&data->ioctl_count);
		ret = qseecom_update_key_user_info(data, argp);
		if (ret)
			pr_err("failed to update key user info: %d\n", ret);
		atomic_dec(&data->ioctl_count);
		break;
	}
	case QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("save part hash req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_save_partition_hash(argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ: {
		if (data->type != QSEECOM_GENERIC) {
			pr_err("ES activated req: invalid handle (%d)\n",
								data->type);
			ret = -EINVAL;
			break;
		}
		data->released = true;
		mutex_lock(&app_access_lock);
		atomic_inc(&data->ioctl_count);
		ret = qseecom_is_es_activated(argp);
		atomic_dec(&data->ioctl_count);
		mutex_unlock(&app_access_lock);
		break;
	}
	case QSEECOM_IOCTL_SEND_MODFD_RESP: {
		if ((data->listener.id == 0) ||
			(data->type != QSEECOM_LISTENER_SERVICE)) {
			pr_err("receive req: invalid handle (%d), lid(%d)\n",
						data->type, data->listener.id);
			ret = -EINVAL;
			break;
		}
		/* Only one client allowed here at a time */
		atomic_inc(&data->ioctl_count);
		ret = qseecom_send_modfd_resp(data, argp);
		atomic_dec(&data->ioctl_count);
		wake_up_all(&data->abort_wq);
		if (ret)
			pr_err("failed qseecom_send_mod_resp: %d\n", ret);
		break;
	}
	default:
		pr_err("Invalid IOCTL: %d\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int qseecom_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct qseecom_dev_handle *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("kmalloc failed\n");
		return -ENOMEM;
	}
	file->private_data = data;
	data->abort = 0;
	data->type = QSEECOM_GENERIC;
	data->released = false;
	memset((void *)data->client.app_name, 0, MAX_APP_NAME_SIZE);
	data->mode = INACTIVE;
	init_waitqueue_head(&data->abort_wq);
	atomic_set(&data->ioctl_count, 0);

	return ret;
}

static int qseecom_release(struct inode *inode, struct file *file)
{
	struct qseecom_dev_handle *data = file->private_data;
	int ret = 0;

	if (data->released == false) {
		pr_warn("data: released = false, type = %d, data = 0x%x\n",
			data->type, (u32)data);
		switch (data->type) {
		case QSEECOM_LISTENER_SERVICE:
			ret = qseecom_unregister_listener(data);
			break;
		case QSEECOM_CLIENT_APP:
			ret = qseecom_unload_app(data, true);
			break;
		case QSEECOM_SECURE_SERVICE:
		case QSEECOM_GENERIC:
			ret = qseecom_unmap_ion_allocated_memory(data);
			if (ret)
				pr_err("Ion Unmap failed\n");
			break;
		case QSEECOM_UNAVAILABLE_CLIENT_APP:
			break;
		default:
			pr_err("Unsupported clnt_handle_type %d",
				data->type);
			break;
		}
	}

	if (qseecom.support_bus_scaling) {
		mutex_lock(&qsee_bw_mutex);
		if (data->mode != INACTIVE) {
			qseecom_unregister_bus_bandwidth_needs(data);
			if (qseecom.cumulative_mode == INACTIVE) {
				ret = __qseecom_set_msm_bus_request(INACTIVE);
				if (ret)
					pr_err("Fail to scale down bus\n");
			}
		}
		mutex_unlock(&qsee_bw_mutex);
	} else {
		if (data->fast_load_enabled == true)
			qsee_disable_clock_vote(data, CLK_SFPB);
		if (data->perf_enabled == true)
			qsee_disable_clock_vote(data, CLK_DFAB);
	}
	kfree(data);

	return ret;
}

static const struct file_operations qseecom_fops = {
		.owner = THIS_MODULE,
		.unlocked_ioctl = qseecom_ioctl,
		.open = qseecom_open,
		.release = qseecom_release
};

static int __qseecom_init_clk(enum qseecom_ce_hw_instance ce)
{
	int rc = 0;
	struct device *pdev;
	struct qseecom_clk *qclk;
	char *core_clk_src = NULL;
	char *core_clk = NULL;
	char *iface_clk = NULL;
	char *bus_clk = NULL;

	switch (ce) {
	case CLK_QSEE: {
		core_clk_src = "core_clk_src";
		core_clk = "core_clk";
		iface_clk = "iface_clk";
		bus_clk = "bus_clk";
		qclk = &qseecom.qsee;
		qclk->instance = CLK_QSEE;
		break;
	};
	case CLK_CE_DRV: {
		core_clk_src = "ce_drv_core_clk_src";
		core_clk = "ce_drv_core_clk";
		iface_clk = "ce_drv_iface_clk";
		bus_clk = "ce_drv_bus_clk";
		qclk = &qseecom.ce_drv;
		qclk->instance = CLK_CE_DRV;
		break;
	};
	default:
		pr_err("Invalid ce hw instance: %d!\n", ce);
		return -EIO;
	}
	pdev = qseecom.pdev;

	/* Get CE3 src core clk. */
	qclk->ce_core_src_clk = clk_get(pdev, core_clk_src);
	if (!IS_ERR(qclk->ce_core_src_clk)) {
		/* Set the core src clk @100Mhz */
		rc = clk_set_rate(qclk->ce_core_src_clk, QSEE_CE_CLK_100MHZ);
		if (rc) {
			clk_put(qclk->ce_core_src_clk);
			pr_err("Unable to set the core src clk @100Mhz.\n");
			return -EIO;
		}
	} else {
		pr_warn("Unable to get CE core src clk, set to NULL\n");
		qclk->ce_core_src_clk = NULL;
	}

	/* Get CE core clk */
	qclk->ce_core_clk = clk_get(pdev, core_clk);
	if (IS_ERR(qclk->ce_core_clk)) {
		rc = PTR_ERR(qclk->ce_core_clk);
		pr_err("Unable to get CE core clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		return -EIO;
	}

	/* Get CE Interface clk */
	qclk->ce_clk = clk_get(pdev, iface_clk);
	if (IS_ERR(qclk->ce_clk)) {
		rc = PTR_ERR(qclk->ce_clk);
		pr_err("Unable to get CE interface clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		clk_put(qclk->ce_core_clk);
		return -EIO;
	}

	/* Get CE AXI clk */
	qclk->ce_bus_clk = clk_get(pdev, bus_clk);
	if (IS_ERR(qclk->ce_bus_clk)) {
		rc = PTR_ERR(qclk->ce_bus_clk);
		pr_err("Unable to get CE BUS interface clk\n");
		if (qclk->ce_core_src_clk != NULL)
			clk_put(qclk->ce_core_src_clk);
		clk_put(qclk->ce_core_clk);
		clk_put(qclk->ce_clk);
		return -EIO;
	}
	return rc;
}

static void __qseecom_deinit_clk(enum qseecom_ce_hw_instance ce)
{
	struct qseecom_clk *qclk;

	if (ce == CLK_QSEE)
		qclk = &qseecom.qsee;
	else
		qclk = &qseecom.ce_drv;

	if (qclk->ce_clk != NULL) {
		clk_put(qclk->ce_clk);
		qclk->ce_clk = NULL;
	}
	if (qclk->ce_core_clk != NULL) {
		clk_put(qclk->ce_core_clk);
		qclk->ce_clk = NULL;
	}
	if (qclk->ce_bus_clk != NULL) {
		clk_put(qclk->ce_bus_clk);
		qclk->ce_clk = NULL;
	}
	if (qclk->ce_core_src_clk != NULL) {
		clk_put(qclk->ce_core_src_clk);
		qclk->ce_core_src_clk = NULL;
	}
}

static int __devinit qseecom_probe(struct platform_device *pdev)
{
	int rc;
	int ret = 0;
	struct device *class_dev;
	char qsee_not_legacy = 0;
	struct msm_bus_scale_pdata *qseecom_platform_support = NULL;
	uint32_t system_call_id = QSEOS_CHECK_VERSION_CMD;

	qseecom.qsee_bw_count = 0;
	qseecom.qsee_perf_client = 0;
	qseecom.qsee_sfpb_bw_count = 0;

	qseecom.qsee.ce_core_clk = NULL;
	qseecom.qsee.ce_clk = NULL;
	qseecom.qsee.ce_core_src_clk = NULL;
	qseecom.qsee.ce_bus_clk = NULL;

	qseecom.cumulative_mode = 0;
	qseecom.current_mode = INACTIVE;
	qseecom.support_bus_scaling = false;
	qseecom.support_fde = false;
	qseecom.support_pfe = false;

	qseecom.ce_drv.ce_core_clk = NULL;
	qseecom.ce_drv.ce_clk = NULL;
	qseecom.ce_drv.ce_core_src_clk = NULL;
	qseecom.ce_drv.ce_bus_clk = NULL;

	rc = alloc_chrdev_region(&qseecom_device_no, 0, 1, QSEECOM_DEV);
	if (rc < 0) {
		pr_err("alloc_chrdev_region failed %d\n", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, QSEECOM_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d\n", rc);
		goto exit_unreg_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, qseecom_device_no, NULL,
			QSEECOM_DEV);
	if (!class_dev) {
		pr_err("class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&qseecom.cdev, &qseecom_fops);
	qseecom.cdev.owner = THIS_MODULE;

	rc = cdev_add(&qseecom.cdev, MKDEV(MAJOR(qseecom_device_no), 0), 1);
	if (rc < 0) {
		pr_err("cdev_add failed %d\n", rc);
		goto exit_destroy_device;
	}

	INIT_LIST_HEAD(&qseecom.registered_listener_list_head);
	spin_lock_init(&qseecom.registered_listener_list_lock);
	INIT_LIST_HEAD(&qseecom.registered_app_list_head);
	spin_lock_init(&qseecom.registered_app_list_lock);
	INIT_LIST_HEAD(&qseecom.registered_kclient_list_head);
	spin_lock_init(&qseecom.registered_kclient_list_lock);
	init_waitqueue_head(&qseecom.send_resp_wq);
	qseecom.send_resp_flag = 0;

	rc = scm_call(6, 1, &system_call_id, sizeof(system_call_id),
				&qsee_not_legacy, sizeof(qsee_not_legacy));
	if (rc) {
		pr_err("Failed to retrieve QSEOS version information %d\n", rc);
		goto exit_del_cdev;
	}
	if (qsee_not_legacy) {
		uint32_t feature = 10;

		qseecom.qsee_version = QSEEE_VERSION_00;
		rc = scm_call(6, 3, &feature, sizeof(feature),
			&qseecom.qsee_version, sizeof(qseecom.qsee_version));
		if (rc) {
			pr_err("Failed to get QSEE version info %d\n", rc);
			goto exit_del_cdev;
		}
		qseecom.qseos_version = QSEOS_VERSION_14;
	} else {
		pr_err("QSEE legacy version is not supported:");
		pr_err("Support for TZ1.3 and earlier is deprecated\n");
		rc = -EINVAL;
		goto exit_del_cdev;
	}
	qseecom.commonlib_loaded = false;
	qseecom.pdev = class_dev;
	/* Create ION msm client */
	qseecom.ion_clnt = msm_ion_client_create(-1, "qseecom-kernel");
	if (qseecom.ion_clnt == NULL) {
		pr_err("Ion client cannot be created\n");
		rc = -ENOMEM;
		goto exit_del_cdev;
	}

	/* register client for bus scaling */
	if (pdev->dev.of_node) {
		qseecom.pdev->of_node = pdev->dev.of_node;
		qseecom.support_bus_scaling =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-bus-scaling");
		pr_warn("support_bus_scaling=0x%x",
				qseecom.support_bus_scaling);
		qseecom.support_fde =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-fde");
		if (qseecom.support_fde) {
			if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,disk-encrypt-pipe-pair",
				&qseecom.ce_info.disk_encrypt_pipe)) {
				pr_err("Fail to get FDE pipe information.\n");
				rc = -EINVAL;
				goto exit_destroy_ion_client;
			} else {
				pr_warn("disk-encrypt-pipe-pair=0x%x",
				qseecom.ce_info.disk_encrypt_pipe);
			}
		} else {
			pr_warn("Device does not support FDE");
			qseecom.ce_info.disk_encrypt_pipe = 0xff;
		}
		qseecom.support_pfe =
				of_property_read_bool((&pdev->dev)->of_node,
						"qcom,support-pfe");
		if (qseecom.support_pfe) {
			if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,file-encrypt-pipe-pair",
				&qseecom.ce_info.disk_encrypt_pipe)) {
				pr_err("Fail to get PFE pipe information.\n");
				rc = -EINVAL;
				goto exit_destroy_ion_client;
			} else {
				pr_warn("file-encrypt-pipe-pair=0x%x",
				qseecom.ce_info.file_encrypt_pipe);
			}
		} else {
			pr_warn("Device does not support PFE");
			qseecom.ce_info.file_encrypt_pipe = 0xff;
		}
		if (qseecom.support_pfe || qseecom.support_fde) {
			if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,hlos-ce-hw-instance",
				&qseecom.ce_info.hlos_ce_hw_instance)) {
				pr_err("Fail: get hlos ce hw instanc info\n");
				rc = -EINVAL;
				goto exit_destroy_ion_client;
			} else {
				pr_warn("hlos-ce-hw-instance=0x%x",
				qseecom.ce_info.hlos_ce_hw_instance);
			}
		} else {
			pr_warn("Device does not support PFE/FDE");
			qseecom.ce_info.hlos_ce_hw_instance = 0xff;
		}

		if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,qsee-ce-hw-instance",
				&qseecom.ce_info.qsee_ce_hw_instance)) {
			pr_err("Fail to get qsee ce hw instance information.\n");
			rc = -EINVAL;
			goto exit_destroy_ion_client;
		} else {
			pr_warn("qsee-ce-hw-instance=0x%x",
			qseecom.ce_info.qsee_ce_hw_instance);
		}

		qseecom.qsee.instance = qseecom.ce_info.qsee_ce_hw_instance;
		qseecom.ce_drv.instance = qseecom.ce_info.hlos_ce_hw_instance;

		ret = __qseecom_init_clk(CLK_QSEE);
		if (ret)
			goto exit_destroy_ion_client;

		if ((qseecom.qsee.instance != qseecom.ce_drv.instance) &&
				(qseecom.support_pfe || qseecom.support_fde)) {
			ret = __qseecom_init_clk(CLK_CE_DRV);
			if (ret) {
				__qseecom_deinit_clk(CLK_QSEE);
				goto exit_destroy_ion_client;
			}
		} else {
			struct qseecom_clk *qclk;

			qclk = &qseecom.qsee;
			qseecom.ce_drv.ce_core_clk = qclk->ce_core_clk;
			qseecom.ce_drv.ce_clk = qclk->ce_clk;
			qseecom.ce_drv.ce_core_src_clk = qclk->ce_core_src_clk;
			qseecom.ce_drv.ce_bus_clk = qclk->ce_bus_clk;
		}

		qseecom_platform_support = (struct msm_bus_scale_pdata *)
						msm_bus_cl_get_pdata(pdev);
		if (qseecom.qsee_version >= (QSEE_VERSION_02)) {
			struct resource *resource = NULL;
			struct qsee_apps_region_info_ireq req;
			struct qseecom_command_scm_resp resp;

			resource = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "secapp-region");
			if (resource) {
				req.qsee_cmd_id = QSEOS_APP_REGION_NOTIFICATION;
				req.addr = resource->start;
				req.size = resource_size(resource);
				pr_warn("secure app region addr=0x%x size=0x%x",
							req.addr, req.size);
			} else {
				pr_err("Fail to get secure app region info\n");
				rc = -EINVAL;
				goto exit_destroy_ion_client;
			}
			rc = scm_call(SCM_SVC_TZSCHEDULER, 1, &req, sizeof(req),
							&resp, sizeof(resp));
			if (rc || (resp.result != QSEOS_RESULT_SUCCESS)) {
				pr_err("send secapp reg fail %d resp.res %d\n",
							rc, resp.result);
				rc = -EINVAL;
				goto exit_destroy_ion_client;
			}
		}
	} else {
		qseecom_platform_support = (struct msm_bus_scale_pdata *)
						pdev->dev.platform_data;
	}
	if (qseecom.support_bus_scaling) {
		init_timer(&(qseecom.bw_scale_down_timer));
		INIT_WORK(&qseecom.bw_inactive_req_ws,
					qseecom_bw_inactive_req_work);
		qseecom.bw_scale_down_timer.function =
				qseecom_scale_bus_bandwidth_timer_callback;
	}
	qseecom.timer_running = false;
	qseecom.qsee_perf_client = msm_bus_scale_register_client(
					qseecom_platform_support);

	if (!qseecom.qsee_perf_client)
		pr_err("Unable to register bus client\n");
	return 0;

exit_destroy_ion_client:
	ion_client_destroy(qseecom.ion_clnt);
exit_del_cdev:
	cdev_del(&qseecom.cdev);
exit_destroy_device:
	device_destroy(driver_class, qseecom_device_no);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(qseecom_device_no, 1);
	return rc;
}

static int __devinit qseecom_remove(struct platform_device *pdev)
{
	struct qseecom_registered_kclient_list *kclient = NULL;
	unsigned long flags = 0;
	int ret = 0;

	spin_lock_irqsave(&qseecom.registered_kclient_list_lock, flags);

	list_for_each_entry(kclient, &qseecom.registered_kclient_list_head,
								list) {
		if (!kclient)
			goto exit_irqrestore;

		/* Break the loop if client handle is NULL */
		if (!kclient->handle)
			goto exit_free_kclient;

		if (list_empty(&kclient->list))
			goto exit_free_kc_handle;

		list_del(&kclient->list);
		ret = qseecom_unload_app(kclient->handle->dev, false);
		if (!ret) {
			kzfree(kclient->handle->dev);
			kzfree(kclient->handle);
			kzfree(kclient);
		}
	}

exit_free_kc_handle:
	kzfree(kclient->handle);
exit_free_kclient:
	kzfree(kclient);
exit_irqrestore:
	spin_unlock_irqrestore(&qseecom.registered_kclient_list_lock, flags);

	if (qseecom.qseos_version > QSEEE_VERSION_00)
		qseecom_unload_commonlib_image();

	if (qseecom.qsee_perf_client)
		msm_bus_scale_client_update_request(qseecom.qsee_perf_client,
									0);
	if (pdev->dev.platform_data != NULL)
		msm_bus_scale_unregister_client(qseecom.qsee_perf_client);

	if (qseecom.support_bus_scaling) {
		cancel_work_sync(&qseecom.bw_inactive_req_ws);
		del_timer_sync(&qseecom.bw_scale_down_timer);
	}

	/* register client for bus scaling */
	if (pdev->dev.of_node) {
		__qseecom_deinit_clk(CLK_QSEE);
		if ((qseecom.qsee.instance != qseecom.ce_drv.instance) &&
				(qseecom.support_pfe || qseecom.support_fde))
			__qseecom_deinit_clk(CLK_CE_DRV);
	}

	ion_client_destroy(qseecom.ion_clnt);

	cdev_del(&qseecom.cdev);

	device_destroy(driver_class, qseecom_device_no);

	class_destroy(driver_class);

	unregister_chrdev_region(qseecom_device_no, 1);

	return ret;
}

static int qseecom_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct qseecom_clk *qclk;
	qclk = &qseecom.qsee;

	mutex_lock(&qsee_bw_mutex);
	mutex_lock(&clk_access_lock);

	if (qseecom.cumulative_mode != INACTIVE &&
		qseecom.current_mode != INACTIVE) {
		ret = msm_bus_scale_client_update_request(
			qseecom.qsee_perf_client, INACTIVE);
		if (ret)
			pr_err("Fail to scale down bus\n");
		else
			qseecom.current_mode = INACTIVE;
	}

	if (qclk->clk_access_cnt) {
		if (qclk->ce_clk != NULL)
			clk_disable_unprepare(qclk->ce_clk);
		if (qclk->ce_core_clk != NULL)
			clk_disable_unprepare(qclk->ce_core_clk);
		if (qclk->ce_bus_clk != NULL)
			clk_disable_unprepare(qclk->ce_bus_clk);
	}

	del_timer_sync(&(qseecom.bw_scale_down_timer));
	qseecom.timer_running = false;

	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);

	return 0;
}

static int qseecom_resume(struct platform_device *pdev)
{
	int mode = 0;
	int ret = 0;
	struct qseecom_clk *qclk;
	qclk = &qseecom.qsee;

	mutex_lock(&qsee_bw_mutex);
	mutex_lock(&clk_access_lock);
	if (qseecom.cumulative_mode >= HIGH)
		mode = HIGH;
	else
		mode = qseecom.cumulative_mode;

	if (qseecom.cumulative_mode != INACTIVE) {
		ret = msm_bus_scale_client_update_request(
			qseecom.qsee_perf_client, mode);
		if (ret)
			pr_err("Fail to scale up bus to %d\n", mode);
		else
			qseecom.current_mode = mode;
	}

	if (qclk->clk_access_cnt) {

		ret = clk_prepare_enable(qclk->ce_core_clk);
		if (ret) {
			pr_err("Unable to enable/prepare CE core clk\n");
			qclk->clk_access_cnt = 0;
			goto err;
		}

		ret = clk_prepare_enable(qclk->ce_clk);
		if (ret) {
			pr_err("Unable to enable/prepare CE iface clk\n");
			qclk->clk_access_cnt = 0;
			goto ce_clk_err;
		}

		ret = clk_prepare_enable(qclk->ce_bus_clk);
		if (ret) {
			pr_err("Unable to enable/prepare CE bus clk\n");
			qclk->clk_access_cnt = 0;
			goto ce_bus_clk_err;
		}
	}

	if (qclk->clk_access_cnt || qseecom.cumulative_mode) {
		qseecom.bw_scale_down_timer.expires = jiffies +
			msecs_to_jiffies(QSEECOM_SEND_CMD_CRYPTO_TIMEOUT);
		mod_timer(&(qseecom.bw_scale_down_timer),
				qseecom.bw_scale_down_timer.expires);
		qseecom.timer_running = true;
	}

	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);


	return 0;

ce_bus_clk_err:
	clk_disable_unprepare(qclk->ce_clk);
ce_clk_err:
	clk_disable_unprepare(qclk->ce_core_clk);
err:
	mutex_unlock(&clk_access_lock);
	mutex_unlock(&qsee_bw_mutex);
	return -EIO;
}
static struct of_device_id qseecom_match[] = {
	{
		.compatible = "qcom,qseecom",
	},
	{}
};

static struct platform_driver qseecom_plat_driver = {
	.probe = qseecom_probe,
	.remove = qseecom_remove,
	.suspend = qseecom_suspend,
	.resume = qseecom_resume,
	.driver = {
		.name = "qseecom",
		.owner = THIS_MODULE,
		.of_match_table = qseecom_match,
	},
};

static int __devinit qseecom_init(void)
{
	return platform_driver_register(&qseecom_plat_driver);
}

static void __devexit qseecom_exit(void)
{
	platform_driver_unregister(&qseecom_plat_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Secure Execution Environment Communicator");

module_init(qseecom_init);
module_exit(qseecom_exit);
