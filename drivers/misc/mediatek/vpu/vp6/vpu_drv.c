/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/compat.h>
#include <linux/fs.h>
#endif

#include "vpu_cmn.h"
#include "vpu_dbg.h"
#include "vpu_load_image.h"
#include "vpubuf-core.h"
#include "vpu_utilization.h"

//#define VPU_LOAD_FW_SUPPORT

static int vpu_probe(struct platform_device *dev);

static int vpu_remove(struct platform_device *dev);

static int vpu_suspend(struct platform_device *dev, pm_message_t mesg);

static int vpu_resume(struct platform_device *dev);

/*---------------------------------------------------------------------------*/
/* VPU Driver: pm operations                                                 */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
int vpu_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return vpu_suspend(pdev, PMSG_SUSPEND);
}

int vpu_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return vpu_resume(pdev);
}

int vpu_pm_restore_noirq(struct device *device)
{
	return 0;
}
#else
#define vpu_pm_suspend NULL
#define vpu_pm_resume  NULL
#define vpu_pm_restore_noirq NULL
#endif

static const struct dev_pm_ops vpu_pm_ops = {
	.suspend = vpu_pm_suspend,
	.resume = vpu_pm_resume,
	.freeze = vpu_pm_suspend,
	.thaw = vpu_pm_resume,
	.poweroff = vpu_pm_suspend,
	.restore = vpu_pm_resume,
	.restore_noirq = vpu_pm_restore_noirq,
};


/*---------------------------------------------------------------------------*/
/* VPU Core Driver: Prototype						     */
/*---------------------------------------------------------------------------*/
static int vpu_core_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i, irq;
	struct resource *mem;
	struct vpu_core *vpu_core;

	vpu_core = devm_kzalloc(dev, sizeof(*vpu_core), GFP_KERNEL);
	if (!vpu_core)
		return -ENOMEM;

	LOG_INF("[vpu] %s %p\n", __func__, vpu_core);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		LOG_ERR("cannot get IORESOUCE_MEM\n");
		return -ENOENT;
	}

	vpu_core->vpu_base = (unsigned long)devm_ioremap_resource(dev, mem);
	if (IS_ERR((const void *)(uintptr_t)(vpu_core->vpu_base))) {
		LOG_ERR("cannot get ioremap of vpu_base\n");
		return -ENOENT;
	}

	/* interrupt resource */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		LOG_ERR("platform_get_irq fail\n");
		return irq;
	}

	vpu_core->irq = irq;
	vpu_core->irq_flags = irq_get_trigger_type(irq);

	INIT_LIST_HEAD(&vpu_core->pool_list);
	mutex_init(&vpu_core->servicepool_mutex);
	vpu_core->servicepool_list_size = 0;
	vpu_core->service_core_available = true;
	mutex_init(&vpu_core->sdsp_control_mutex);

	for (i = 0 ; i < VPU_REQ_MAX_NUM_PRIORITY ; i++)
		vpu_core->priority_list[i] = 0;

	vpu_core->dev = &pdev->dev;
	platform_set_drvdata(pdev, vpu_core);
	dev_set_drvdata(dev, vpu_core);

	return 0;
}

static int vpu_core_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id vpu_core_of_ids[] = {
	{.compatible = "mediatek,mt8168-vpu-core",},
	{.compatible = "mediatek,mt6779-vpu-core",},
	{}
};

#ifdef CONFIG_PM
static int vpu_core_runtime_suspend(struct device *dev)
{
	return 0;
}

static int vpu_core_runtime_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops vpu_core_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
#ifdef CONFIG_PM
	.runtime_suspend = vpu_core_runtime_suspend,
	.runtime_resume = vpu_core_runtime_resume,
	.runtime_idle = NULL,
#endif
};

static struct platform_driver vpu_core_driver = {
	.probe = vpu_core_probe,
	.remove = vpu_core_remove,
	.driver = {
		.name = "mediatek,vpu-core",
		.of_match_table = vpu_core_of_ids,
		.pm = &vpu_core_pm_ops,
	}
};



/*---------------------------------------------------------------------------*/
/* VPU Driver: Prototype                                                     */
/*---------------------------------------------------------------------------*/

static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,mt8168-vpu",},
	{.compatible = "mediatek,mt6779-vpu",},
	{}
};

static struct platform_driver vpu_driver = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.suspend = vpu_suspend,
	.resume  = vpu_resume,
	.driver  = {
		.name = VPU_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = vpu_of_ids,
#ifdef CONFIG_PM
		.pm = &vpu_pm_ops,
#endif
	}
};


/*---------------------------------------------------------------------------*/
/* VPU Driver: file operations                                               */
/*---------------------------------------------------------------------------*/
static int vpu_open(struct inode *inode, struct file *flip);

static int vpu_release(struct inode *inode, struct file *flip);

static int vpu_mmap(struct file *flip, struct vm_area_struct *vma);

#ifdef CONFIG_COMPAT
static long vpu_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg);
#endif

static long vpu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);

static const struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.release = vpu_release,
	.mmap = vpu_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpu_compat_ioctl,
#endif
	.unlocked_ioctl = vpu_ioctl
};


/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

int vpu_create_user(struct vpu_device *vpu_device, struct vpu_user **user)
{
	struct vpu_user *u;
	int i = 0;

	u = kzalloc(sizeof(vlist_type(struct vpu_user)), GFP_KERNEL);
	if (!u)
		return -ENOMEM;

	mutex_init(&u->data_mutex);
	mutex_lock(&vpu_device->debug_list_mutex);
	vpu_device->vpu_num_users++;
	mutex_unlock(&vpu_device->debug_list_mutex);
	u->dev = vpu_device->dev;
	u->id = NULL;
	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	INIT_LIST_HEAD(&u->enque_list);
	INIT_LIST_HEAD(&u->deque_list);
	INIT_LIST_HEAD(&u->algo_list);
	init_waitqueue_head(&u->deque_wait);
	init_waitqueue_head(&u->delete_wait);

	mutex_init(&u->dbgbuf_mutex);
	INIT_LIST_HEAD(&u->dbgbuf_list);

	for (i = 0 ; i < vpu_device->core_num ; i++)
		u->running[i] = false;

	u->deleting = false;
	u->power_mode = VPU_POWER_MODE_DYNAMIC;
	u->power_opp = VPU_POWER_OPP_UNREQUEST;
	u->algo_num = 0;

	mutex_lock(&vpu_device->user_mutex);
	list_add_tail(vlist_link(u, struct vpu_user), &vpu_device->user_list);
	mutex_unlock(&vpu_device->user_mutex);

	*user = u;
	return 0;
}

static int vpu_write_register(struct vpu_reg_values *regs)
{
	return 0;
}

int vpu_push_request_to_queue(struct vpu_user *user, struct vpu_request *req)
{
	struct vpu_device *vpu_device;

	if (!user) {
		LOG_ERR("empty user\n");
		return -EINVAL;
	}

	if (user->deleting) {
		LOG_WRN("push a request while deleting the user\n");
		return -ENONET;
	}

	vpu_device = dev_get_drvdata(user->dev);
	if (!vpu_device) {
		LOG_ERR("Failed to get vpu_device\n");
		return -EINVAL;
	}

	mutex_lock(&user->data_mutex);
	list_add_tail(vlist_link(req, struct vpu_request), &user->enque_list);
	mutex_unlock(&user->data_mutex);

	wake_up(&vpu_device->req_wait);

	return 0;
}

int vpu_put_request_to_pool(struct vpu_user *user, struct vpu_request *req)
{
	struct vpu_device *vpu_device = NULL;
	struct vpu_core *vpu_core = NULL;
	int i = 0, request_core_index = -1;
	int j = 0, cnt = 0;
	uint64_t handle_id;
	int ion_fd;

	if (!user) {
		LOG_ERR("empty user\n");
		return -EINVAL;
	}

	if (user->deleting) {
		LOG_WRN("push a request while deleting the user\n");
		return -ENONET;
	}

	vpu_device = dev_get_drvdata(user->dev);
	if (!vpu_device) {
		LOG_ERR("Failed to get vpu_device\n");
		return -EINVAL;
	}

	for (i = 0 ; i < req->buffer_count; i++) {
		for (j = 0 ; j < req->buffers[i].plane_count; j++) {
			handle_id = 0;
			LOG_DBG("[vpu] (%d) FD.0x%lx\n", cnt,
			  (unsigned long)(uintptr_t)(req->buf_ion_infos[cnt]));
			ion_fd = req->buf_ion_infos[cnt];

			handle_id = vbuf_import_handle(vpu_device, ion_fd);
			if (IS_ERR((void *)(uintptr_t)handle_id)) {
				LOG_WRN(
				"[vpu_drv] import ion handle(0x%llx) failed!\n"
								, handle_id);
			} else {
				if (vpu_device->vpu_log_level >
						Log_STATE_MACHINE)
					LOG_INF(
					"[vpu_drv] (cnt_%d)ion_import (0x%llx)\n"
							, cnt, handle_id);
				/*
				 * import fd to handle for buffer ref count + 1
				 */
				req->buf_ion_infos[cnt] = handle_id;
			}
			cnt++;
		}
	}

	/* CHRISTODO, specific vpu */
	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (req->requested_core == (0x1 << i)) {
			request_core_index = i;
			vpu_core = vpu_device->vpu_core[i];
			if (!vpu_core->vpu_hw_support) {
				LOG_ERR(
				"[vpu_%d] not support. push to common queue\n"
							, request_core_index);
				request_core_index = -1;
			}
			break;
		}
	}

	#if 0
	LOG_DBG("[vpu] push request to euque CORE_IDNEX (0x%x/0x%x)...\n",
		req->requested_core, request_core_index);
	#endif

	if (request_core_index >= vpu_device->core_num) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
				req->requested_core,
				request_core_index,
				vpu_device->core_num);
	}

	if (request_core_index > -1 &&
		request_core_index < vpu_device->core_num) {
		/* LOG_DBG("[vpu] push self pool, index(%d/0x%x)\n",
		 * request_core_index, req->requested_core);
		 */
		mutex_lock(&vpu_core->servicepool_mutex);

		list_add_tail(vlist_link(req, struct vpu_request),
				&vpu_core->pool_list);

		vpu_core->servicepool_list_size += 1;
		/*add priority list number*/
		vpu_core->priority_list[req->priority] += 1;
		mutex_unlock(&vpu_core->servicepool_mutex);
	} else {
		/* LOG_DBG("[vpu] push common pool, index(%d,0x%x)\n",
		 * request_core_index, req->requested_core);
		 */
		mutex_lock(&vpu_device->commonpool_mutex);

		list_add_tail(vlist_link(req, struct vpu_request),
				&vpu_device->cmnpool_list);

		vpu_device->commonpool_list_size += 1;
		/* LOG_DBG("[vpu] push common pool size (%d)\n",
		 * vpu_device->commonpool_list_size);
		 */
		mutex_unlock(&vpu_device->commonpool_mutex);
	}

	wake_up(&vpu_device->req_wait);
	/*LOG_DBG("[vpu] vpu_push_request_to_queue ---\n");*/

	return 0;
}

bool vpu_user_is_running(struct vpu_user *user)
{
	bool running = false;
	int i = 0;
	struct vpu_device *vpu_device;

	if (!user) {
		LOG_ERR("empty user\n");
		return -EINVAL;
	}

	vpu_device = dev_get_drvdata(user->dev);
	if (!vpu_device) {
		LOG_ERR("Failed to get vpu_device\n");
		return -EINVAL;
	}

	mutex_lock(&user->data_mutex);
	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (user->running[i]) {
			running = true;
			break;
		}
	}
	mutex_unlock(&user->data_mutex);

	return running;
}

int vpu_flush_requests_from_queue(struct vpu_user *user)
{
#if 0
	struct list_head *head, *temp;
	struct vpu_request *req;

	mutex_lock(&user->data_mutex);

	if (!user->running && list_empty(&user->enque_list)) {
		mutex_unlock(&user->data_mutex);
		return 0;
	}

	user->flushing = true;
	mutex_unlock(&user->data_mutex);

	/* the running request will add to the deque before interrupt */
	wait_event_interruptible(user->deque_wait, !user->running);

	while (user->running)
		ndelay(1000);

	mutex_lock(&user->data_mutex);
	/* push the remaining enque to the deque */
	list_for_each_safe(head, temp, &user->enque_list) {
		req = vlist_node_of(head, struct vpu_request);
		req->status = VPU_REQ_STATUS_FLUSH;
		list_del_init(head);
		list_add_tail(head, &user->deque_list);
	}

	user->flushing = false;
	LOG_DBG("flushed queue, user:%d\n", user->id);

	mutex_unlock(&user->data_mutex);
#endif
	return 0;
}

int vpu_pop_request_from_queue(struct vpu_user *user,
	struct vpu_request **rreq)
{
	int ret;
	struct vpu_request *req;

	/* wait until condition is true */
	ret = wait_event_interruptible(
		user->deque_wait,
		!list_empty(&user->deque_list));

	/* ret == -ERESTARTSYS, if signal interrupt */
	if (ret < 0) {
		LOG_ERR("interrupt by signal, %s, ret=%d\n",
			"while pop a request", ret);
		*rreq = NULL;
		return -EINTR;
	}

	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_list)) {
		mutex_unlock(&user->data_mutex);
		LOG_ERR("pop a request from empty queue! ret=%d\n", ret);
		*rreq = NULL;
		return -ENODATA;
	};

	/* get first node from deque list */
	req = vlist_node_of(user->deque_list.next, struct vpu_request);

	list_del_init(vlist_link(req, struct vpu_request));
	mutex_unlock(&user->data_mutex);

	*rreq = req;
	return 0;
}

int vpu_get_request_from_queue(struct vpu_user *user,
	uint64_t request_id, struct vpu_request **rreq)
{
	struct vpu_device *vpu_device = dev_get_drvdata(user->dev);
	int ret;
	struct list_head *head = NULL;
	struct vpu_request *req;
	bool get = false;
	int retry = 0;

	do {
		/* wait until condition is true */
		ret = wait_event_interruptible(
			user->deque_wait,
			!list_empty(&user->deque_list));

		/* ret == -ERESTARTSYS, if signal interrupt */
		/* ERESTARTSYS may caused by Freezing user space processes */

		if (ret < 0) {
			LOG_ERR("interrupt by signal, %s, ret=%d\n",
				"while pop a request", ret);
			if (retry < 30) {
				LOG_ERR("retry=%d and sleep 10ms\n", retry);
				msleep(20);
				retry += 1;
				get = false;
				continue;
			} else {
				LOG_ERR("retry %d times fail, return FAIL\n",
						retry);
				*rreq = NULL;
				return ret;
			}
		}

		mutex_lock(&user->data_mutex);
		/* This part should not be happened */
		if (list_empty(&user->deque_list)) {
			mutex_unlock(&user->data_mutex);
			LOG_ERR("pop a request from empty queue! ret=%d\n",
					ret);
			*rreq = NULL;
			return -ENODATA;
		};

		/* get corresponding node from deque list */
		list_for_each(head, &user->deque_list)
		{
			req = vlist_node_of(head, struct vpu_request);
			LOG_DBG("[vpu] req->request_id = 0x%lx, 0x%lx\n",
					(unsigned long)req->request_id,
					(unsigned long)request_id);
			if ((unsigned long)req->request_id ==
					(unsigned long)request_id) {
				get = true;
				LOG_DBG("[vpu] get = true\n");
				break;
			}
		}

		if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
			LOG_INF("[vpu] %s (%d)\n", __func__, get);
		if (get)
			list_del_init(vlist_link(req, struct vpu_request));

		mutex_unlock(&user->data_mutex);
	} while (!get);

	*rreq = req;
	return 0;
}

int vpu_get_core_status(struct vpu_device *vpu_device,
			struct vpu_status *status)
{
	struct vpu_core *vpu_core;
	int index = status->vpu_core_index; /* - 1;*/

	if (index > -1 && index < vpu_device->core_num) {
		vpu_core = vpu_device->vpu_core[index];
		LOG_DBG("vpu_%d, support(%d/0x%x)\n",
			index, vpu_core->vpu_hw_support,
			vpu_device->efuse_data);
		if (vpu_core->vpu_hw_support) {
			mutex_lock(&vpu_core->servicepool_mutex);

			status->vpu_core_available =
				vpu_core->service_core_available;

			status->pool_list_size =
				vpu_core->servicepool_list_size;
			mutex_unlock(&vpu_core->servicepool_mutex);
		} else {
			LOG_ERR("core_%d not support (0x%x).\n",
					index, vpu_device->efuse_data);
			return -EINVAL;
		}
	} else {
		mutex_lock(&vpu_device->commonpool_mutex);
		status->vpu_core_available = true;
		status->pool_list_size = vpu_device->commonpool_list_size;
		mutex_unlock(&vpu_device->commonpool_mutex);
	}

	LOG_DBG("[vpu]%s idx(%d), available(%d), size(%d)\n", __func__,
			status->vpu_core_index,
			status->vpu_core_available,
			status->pool_list_size);

	return 0;
}

bool vpu_is_available(struct vpu_device *vpu_device)
{
	int i = 0;
	int pool_wait_size = 0;

	mutex_lock(&vpu_device->commonpool_mutex);
	pool_wait_size = vpu_device->commonpool_list_size;
	mutex_unlock(&vpu_device->commonpool_mutex);


	if (pool_wait_size != 0) {
		LOG_INF("common pool size = %d, no empty vpu \r\n",
			pool_wait_size);

		return false;
	}

	for (i = 0; i < vpu_device->core_num; i++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[i];

		mutex_lock(&vpu_core->servicepool_mutex);

		if (vpu_core->service_core_available)
			pool_wait_size = vpu_core->servicepool_list_size;

		mutex_unlock(&vpu_core->servicepool_mutex);

		LOG_INF("vpu_%d, pool size = %d\r\n", i, pool_wait_size);
		if ((pool_wait_size == 0) &&
				vpu_is_idle(vpu_device->vpu_core[i])) {
			LOG_INF("vpu_%d, is available !!\r\n", i);
			return true;
		}
	}
	LOG_INF("GG, no vpu available !!\r\n");

	return false;
}

int vpu_delete_user(struct vpu_user *user)
{
	struct vpu_device *vpu_device;
	struct list_head *head, *temp;
	struct vpu_request *req;
	struct vpu_algo *algo;
	int ret = 0;
	int retry = 0;

	if (!user) {
		LOG_ERR("delete empty user!\n");
		return -EINVAL;
	}

	vpu_device = dev_get_drvdata(user->dev);

	vpu_check_dbg_buf(user);

	mutex_lock(&user->data_mutex);
	user->deleting = true;
	mutex_unlock(&user->data_mutex);

	/*vpu_flush_requests_from_queue(user);*/

	ret = wait_event_interruptible(
			user->delete_wait,
			!vpu_user_is_running(user));

	/* ret == -ERESTARTSYS, if signal interrupt */
	/* ERESTARTSYS may caused by Freezing user space processes */
	if (ret < 0) {
		LOG_WRN("[vpu]%s, ret=%d, wait delete user again\n",
			"interrupt by signal", ret);

		LOG_WRN("ERESTARTSYS = %d\n", ERESTARTSYS);

		while (ret < 0 && retry < 30) {
			msleep(20);
			LOG_ERR("ret=%d retry=%d and sleep 10ms\n", ret, retry);

			ret = wait_event_interruptible(user->delete_wait,
			!vpu_user_is_running(user));
			retry += 1;
		}
	}


	/* clear the list of deque */
	mutex_lock(&user->data_mutex);
	list_for_each_safe(head, temp, &user->deque_list) {
		req = vlist_node_of(head, struct vpu_request);
		list_del(head);
		vpu_free_request(req);
	}
	mutex_unlock(&user->data_mutex);

	list_for_each_safe(head, temp, &user->algo_list) {
		algo = vlist_node_of(head, struct vpu_algo);
		list_del(head);
		vpu_free_algo(algo);
	}

	/* confirm the lock has released */
	if (user->locked)
		vpu_hw_unlock(user);

	mutex_lock(&vpu_device->user_mutex);
	LOG_INF("deleted user[0x%lx]\n", (unsigned long)(user->id));
	list_del(vlist_link(user, struct vpu_user));
	mutex_unlock(&vpu_device->user_mutex);

	kfree(user);

	return 0;
}

int vpu_dump_user_algo(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device = NULL;
	struct vpu_user *user;
	struct list_head *head_user;
	struct list_head *head_algo;
	struct vpu_algo *algo;
	int header = 4;
	unsigned long magic_num1 = 0;
	unsigned long magic_num2 = 1;
	const char *line_bar =
"  +------+-----+--------------------------------+--------+-----------+----------+\n"
	;

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "%s", line_bar);
	vpu_print_seq(s, "  |%-6s|%-5s|%-32s|%-8s|%-11s|%-10s|\n",
			  "Header", "Id", "Name", "MagicNum", "MVA", "Length");
	vpu_print_seq(s, "%s", line_bar);


	mutex_lock(&vpu_device->user_mutex);
	list_for_each(head_user, &vpu_device->user_list)
	{
		user = vlist_node_of(head_user, struct vpu_user);
		list_for_each(head_algo, &user->algo_list)
		{
			algo = vlist_node_of(head_algo, struct vpu_algo);
			vpu_print_seq(s,
				"  |%-6d|%-5d|%-32s|0x%-6lx|0x%-9lx|0x%-8x|\n",
					  header,
					  algo->id[0] ? algo->id[0]:algo->id[1],
					  algo->name,
					  (algo->id[0] ? magic_num1:magic_num2),
					  (unsigned long)algo->bin_ptr,
					  algo->bin_length);
		}
	}
	mutex_unlock(&vpu_device->user_mutex);
	vpu_print_seq(s, "%s", line_bar);

	return 0;
}

int vpu_dump_user(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device;
	struct vpu_user *user;
	struct list_head *head_user;
	struct list_head *head_req;
	uint32_t cnt_deq;
	const char *line_bar =
		"  +------------------+------+------+-------+-------+\n";

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "%s", line_bar);
	vpu_print_seq(s, "  |%-18s|%-6s|%-6s|%-7s|%-7s|\n",
			"Id", "Pid", "Tid", "Deque", "Locked");
	vpu_print_seq(s, "%s", line_bar);

	mutex_lock(&vpu_device->user_mutex);
	list_for_each(head_user, &vpu_device->user_list)
	{
		user = vlist_node_of(head_user, struct vpu_user);
		cnt_deq = 0;

		list_for_each(head_req, &user->deque_list)
		{
			cnt_deq++;
		}

		vpu_print_seq(s, "  |0x%-16lx|%-6d|%-6d|%-7d|%7d|\n",
			      (unsigned long)(user->id),
			      user->open_pid,
			      user->open_tgid,
			      cnt_deq,
			      user->locked);
		vpu_print_seq(s, "%s", line_bar);
	}
	mutex_unlock(&vpu_device->user_mutex);
	vpu_print_seq(s, "\n");

	return 0;
}

int vpu_alloc_debug_info(struct vpu_dev_debug_info **rdbginfo)
{
	struct vpu_dev_debug_info *dbginfo;

	dbginfo = kzalloc(sizeof(vlist_type(struct vpu_dev_debug_info)),
				GFP_KERNEL);
	if (dbginfo == NULL) {
		LOG_ERR("%s, node=0x%p\n", __func__, dbginfo);
		return -ENOMEM;
	}

	*rdbginfo = dbginfo;

	return 0;
}

int vpu_free_debug_info(struct vpu_dev_debug_info *dbginfo)
{
	if (dbginfo != NULL)
		kfree(dbginfo);
	return 0;
}

int vpu_dump_device_dbg(struct seq_file *s, struct vpu_device *gvpu_device)
{
	struct vpu_device *vpu_device = NULL;
	struct list_head *head = NULL;
	struct vpu_dev_debug_info *dbg_info;
	const char *line_bar =
		"  +-------+-------+-------+------------------------------+\n";

	if (gvpu_device) {
		vpu_device = gvpu_device;
	} else {
		if (s == NULL || s->private == NULL) {
			LOG_ERR("[vpu seq_file] seq_file error");
			return -EINVAL;
		}

		vpu_device = (struct vpu_device *)s->private;
	}

	vpu_print_seq(s, "========== vpu device debug info dump ==========\n");
	vpu_print_seq(s, "%s", line_bar);
	vpu_print_seq(s, "  |%-7s|%-7s|%-7s|%-30s|\n",
				  "PID", "TGID", "OPENFD", "USER");
	vpu_print_seq(s, "%s", line_bar);

	mutex_lock(&vpu_device->debug_list_mutex);
	list_for_each(head, &vpu_device->device_debug_list)
	{
		dbg_info = vlist_node_of(head, struct vpu_dev_debug_info);
		vpu_print_seq(s, "  |%-7d|%-7d|%-7d|%-30s|\n",
				  dbg_info->open_pid,
				  dbg_info->open_tgid,
				  dbg_info->dev_fd,
				  dbg_info->callername);
	}
	mutex_unlock(&vpu_device->debug_list_mutex);
	vpu_print_seq(s, "%s", line_bar);

	return 0;
}


/*---------------------------------------------------------------------------*/
/* IOCTL: implementation                                                     */
/*---------------------------------------------------------------------------*/

static int vpu_open(struct inode *inode, struct file *flip)
{
	int ret = 0, i = 0;
	bool not_support_vpu = true;
	struct vpu_user *user;
	struct vpu_device *vpu_device;

	vpu_device =
	    container_of(inode->i_cdev, struct vpu_device, vpu_chardev);

	for (i = 0 ; i < vpu_device->core_num ; i++) {
		if (vpu_device->vpu_core[i]->vpu_hw_support) {
			not_support_vpu = false;
			break;
		}
	}
	if (not_support_vpu) {
		LOG_ERR("not support vpu...(%d/0x%x)\n",
				not_support_vpu, vpu_device->efuse_data);
		return -ENODEV;
	}

	LOG_INF("vpu_support core : 0x%x\n", vpu_device->efuse_data);

	vpu_load_image(vpu_device);

	vpu_create_user(vpu_device, &user);
	if (IS_ERR_OR_NULL(user)) {
		LOG_ERR("fail to create user\n");
		return -ENOMEM;
	}

	user->id = (unsigned long *)user;
	LOG_INF("%s cnt(%d) user->id : 0x%lx, tids(%d/%d)\n", __func__,
		vpu_device->vpu_num_users,
		(unsigned long)(user->id),
		user->open_pid, user->open_tgid);
	flip->private_data = user;

	return ret;
}

#ifdef CONFIG_COMPAT
static long vpu_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	return flip->f_op->unlocked_ioctl(flip, cmd,
					(unsigned long)compat_ptr(arg));
}
#endif

static long vpu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int i = 0;
	struct vpu_user *user = flip->private_data;
	struct vpu_device *vpu_device = dev_get_drvdata(user->dev);

	switch (cmd) {
	case VPU_IOCTL_SET_POWER:
	{
		struct vpu_power power;

		ret = copy_from_user(&power, (void *) arg,
					sizeof(struct vpu_power));
		if (ret) {
			LOG_ERR("[SET_POWER] %s, ret=%d\n",
					"copy 'struct power' failed", ret);
			goto out;
		}

		ret = vpu_set_power(user, &power);
		if (ret) {
			LOG_ERR("[SET_POWER] set power failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_IOCTL_EARA_LOCK_POWER:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = copy_from_user(&vpu_lock_power,
			(void *) arg, sizeof(struct vpu_lock_power));
		if (ret) {
			LOG_ERR("[EARA_LOCK] %s, ret=%d\n",
					"copy 'struct power' failed", ret);
			goto out;
		}
		if ((vpu_lock_power.lock != true)
			|| vpu_lock_power.priority != EARA_QOS) {
			LOG_ERR("[EARA_LOCK] get arg fail\n");
			goto out;
		}
		LOG_INF("[vpu] EARA_LOCK + core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);
		ret = vpu_lock_set_power(vpu_device, &vpu_lock_power);
		if (ret) {
			LOG_ERR("[EARA_LOCK] lock power failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_IOCTL_EARA_UNLOCK_POWER:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = copy_from_user(&vpu_lock_power,
			(void *) arg, sizeof(struct vpu_lock_power));
		if (ret) {
			LOG_ERR("[EARA_UNLOCK] %s, ret=%d\n",
					"copy 'struct power' failed", ret);
			goto out;
		}
		if ((vpu_lock_power.lock != false)
			|| vpu_lock_power.priority != EARA_QOS) {
			LOG_ERR("[EARA_UNLOCK] get arg fail\n");
			goto out;
		}
		LOG_INF("[vpu] EARA_UNLOCK + core:%d\n",
			vpu_lock_power.core);
		vpu_unlock_set_power(vpu_device, &vpu_lock_power);
		if (ret) {
			LOG_ERR("[EARA_UNLOCK]unlock failed, ret=%d\n", ret);
			goto out;
		}

		break;
	}
	case VPU_IOCTL_POWER_HAL_LOCK_POWER:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = copy_from_user(&vpu_lock_power, (void *) arg,
			sizeof(struct vpu_lock_power));
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK] %s, ret=%d\n",
					"copy 'struct power' failed", ret);
			goto out;
		}
		if ((vpu_lock_power.lock != true)
			|| vpu_lock_power.priority != POWER_HAL) {
			LOG_ERR("[POWER_HAL] get arg fail\n");
			goto out;
		}
		LOG_INF("[vpu]POWER_HAL_LOCK+core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);
		ret = vpu_lock_set_power(vpu_device, &vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}


		break;
	}
	case VPU_IOCTL_POWER_HAL_UNLOCK_POWER:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = copy_from_user(&vpu_lock_power, (void *) arg,
			sizeof(struct vpu_lock_power));
		if (ret) {
			LOG_ERR("[POWER_HAL_UNLOCK] %s, ret=%d\n",
					"copy 'struct power' failed", ret);
			goto out;
		}
		if ((vpu_lock_power.lock != false)
			|| vpu_lock_power.priority != POWER_HAL) {
			LOG_ERR("[POWER_HAL_UNLOCK] get arg fail\n");
			goto out;
		}
		LOG_INF("[vpu]POWER_HAL_UNLOCK+ core:%d\n",
			vpu_lock_power.core);
		ret = vpu_unlock_set_power(vpu_device, &vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_UNLOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_IOCTL_ENQUE_REQUEST:
	{
		struct vpu_request *req;
		struct vpu_request *u_req;
		int plane_count;

		u_req = (struct vpu_request *) arg;

		if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
			LOG_INF("[vpu] VPU_IOCTL_ENQUE_REQUEST +\n");

		ret = vpu_alloc_request(&req);
		if (ret) {
			LOG_ERR("[ENQUE alloc request failed, ret=%d\n", ret);
			goto out;
		}

		ret = get_user(req->requested_core, &u_req->requested_core);


		if (req->requested_core == VPU_TRYLOCK_CORENUM &&
			(ret == 0)) {
			if (false == vpu_is_available(vpu_device)) {
				LOG_INF("[vpu] vpu try lock fail!!\n");
				vpu_free_request(req);
				ret = -EFAULT;
				goto out;
			}
			LOG_INF("[vpu] ret = %d req->requested_core = 0x%x +\n",
			ret, req->requested_core);
		}  else if (req->requested_core != 0xFFFF &&
			req->requested_core > VPU_MAX_NUM_CORES) {
			LOG_ERR("req->requested_core=%d error\n",
				req->requested_core);
			vpu_free_request(req);
			ret = -EFAULT;
			goto out;
		}

		ret |= get_user(req->user_id, &u_req->user_id);
		ret |= get_user(req->request_id, &u_req->request_id);
		//ret |= get_user(req->requested_core, &u_req->requested_core);
		ret |= copy_from_user(req->algo_id, u_req->algo_id,
				VPU_MAX_NUM_CORES * sizeof(vpu_id_t));
		ret |= get_user(req->frame_magic, &u_req->frame_magic);
		ret |= get_user(req->status, &u_req->status);
		ret |= get_user(req->buffer_count, &u_req->buffer_count);
		ret |= get_user(req->sett_ptr, &u_req->sett_ptr);
		ret |= get_user(req->sett_length, &u_req->sett_length);
		ret |= get_user(req->priv, &u_req->priv);
		ret |= get_user(req->power_param.bw,
					&u_req->power_param.bw);

		ret |= get_user(req->power_param.freq_step,
					&u_req->power_param.freq_step);

		ret |= get_user(req->power_param.opp_step,
					&u_req->power_param.opp_step);

		ret |= get_user(req->power_param.core,
					&u_req->power_param.core);

		ret |= get_user(req->power_param.boost_value,
					&u_req->power_param.boost_value);
		ret |= get_user(req->priority,
					&u_req->priority);

		if (req->priority >= VPU_REQ_MAX_NUM_PRIORITY) {
			LOG_ERR("%s: ENQUE: invalid priority (%d)\n",
				__func__, req->priority);
			req->priority = 0;
		}

		/*opp_step counted by vpu driver*/
		if (req->power_param.boost_value != 0xff) {
			if (req->power_param.boost_value >= 0 &&
				req->power_param.boost_value <= 100) {
				req->power_param.opp_step =
					vpu_boost_value_to_opp(vpu_device,
						req->power_param.boost_value);
				req->power_param.freq_step =
					vpu_boost_value_to_opp(vpu_device,
						req->power_param.boost_value);
			} else {
				req->power_param.opp_step = 0xFF;
				req->power_param.freq_step = 0xFF;
			}
		}
		req->user_id = (unsigned long *)user;

		if (req->request_id == 0x0) {
			LOG_ERR("wrong request_id (0x%lx)\n",
					(unsigned long)req->request_id);
			vpu_free_request(req);
			ret = -EFAULT;
			goto out;
		}

		if (ret)
			LOG_ERR("[ENQUE] get params failed, ret=%d\n", ret);
		else if (req->buffer_count > VPU_MAX_NUM_PORTS) {
			LOG_ERR("[ENQUE] %s, count=%d\n",
				"wrong buffer count", req->buffer_count);
			vpu_free_request(req);
			ret = -EINVAL;
			goto out;
		} else if (copy_from_user(req->buffers, u_req->buffers,
			    req->buffer_count * sizeof(struct vpu_buffer))) {
			LOG_ERR("[ENQUE] %s, ret=%d\n",
				"copy 'struct buffer' failed", ret);
			vpu_free_request(req);
			ret = -EINVAL;
			goto out;
		}

		/* Check if user plane_count is valid */
		for (i = 0 ; i < req->buffer_count; i++) {
			plane_count = req->buffers[i].plane_count;
			if ((plane_count > VPU_MAX_NUM_PLANE) ||
			    (plane_count == 0)) {
				vpu_free_request(req);
				ret = -EINVAL;
				LOG_ERR("[ENQUE] Buf#%d plane_cnt:%d fail\n",
					i, plane_count);
				goto out;
			}
		}

		if (copy_from_user(req->buf_ion_infos, u_req->buf_ion_infos,
				   req->buffer_count * VPU_MAX_NUM_PLANE *
				   sizeof(uint64_t))) {
			LOG_ERR("[ENQUE] %s, ret=%d\n",
				"copy 'buf_share_fds' failed", ret);
		} else if (vpu_put_request_to_pool(user, req)) {
			LOG_ERR("[ENQUE] %s, ret=%d\n",
				"push to user's queue failed", ret);
		} else
			break;

		/* free the request, error happened here*/
		vpu_free_request(req);
		if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
			LOG_INF("[vpu] .VPU_IOCTL_ENQUE_REQUEST - ");
		ret = -EFAULT;
		break;
	}
	case VPU_IOCTL_DEQUE_REQUEST:
	{
		struct vpu_request *req;
		uint64_t kernel_request_id;
		struct vpu_request *u_req;

		if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
			LOG_INF("[vpu] VPU_IOCTL_DEQUE_REQUEST + ");

		u_req = (struct vpu_request *) arg;
		#if 1
		ret = get_user(kernel_request_id, &u_req->request_id);
		if (ret) {
			LOG_ERR("[REG] get 'req id' failed,%d\n", ret);
			goto out;
		}

		LOG_DBG("[vpu] deque test: user_id_0x%lx, request_id_0x%lx",
			(unsigned long)user,
			(unsigned long)(kernel_request_id));

		ret = vpu_get_request_from_queue(user, kernel_request_id, &req);
		#else
		LOG_DBG("[vpu] dequee test: user_id_0x%lx, request_id_0x%lx",
				(unsigned long)user,
				(unsigned long)(u_req->request_id));

		ret = vpu_get_request_from_queue(user, u_req->request_id, &req);
		#endif
		if (ret) {
			LOG_ERR("[DEQUE] pop request failed, ret=%d\n", ret);
			goto out;
		}

		ret = put_user(req->status, &u_req->status);
		ret |= put_user(req->occupied_core, &u_req->occupied_core);
		ret |= put_user(req->frame_magic, &u_req->frame_magic);
		ret |= put_user(req->busy_time, &u_req->busy_time);
		ret |= put_user(req->bandwidth, &u_req->bandwidth);
		if (ret)
			LOG_ERR("[DEQUE] update status failed, ret=%d\n", ret);

		ret = vpu_free_request(req);
		if (ret) {
			LOG_ERR("[DEQUE] free request, ret=%d\n", ret);
			goto out;
		}
		if (vpu_device->vpu_log_level > VpuLogThre_PERFORMANCE)
			LOG_INF("[vpu] VPU_IOCTL_DEQUE_REQUEST - ");
		break;
	}
	case VPU_IOCTL_FLUSH_REQUEST:
	{
		ret = vpu_flush_requests_from_queue(user);
		if (ret) {
			LOG_ERR("[FLUSH] flush request failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_IOCTL_GET_ALGO_INFO:
	{
		LOG_WRN("DO NOT SUPPORT VPU_IOCTL_GET_ALGO_INFO");
		break;
	}
	case VPU_IOCTL_REG_WRITE:
	{
		struct vpu_reg_values regs;

		ret = copy_from_user(&regs, (void *) arg,
					sizeof(struct vpu_reg_values));
		if (ret) {
			LOG_ERR("[REG] copy 'struct reg' failed,%d\n", ret);
			goto out;
		}

		ret = vpu_write_register(&regs);
		if (ret) {
			LOG_ERR("[REG] write reg failed,%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_IOCTL_LOCK:
	{
		LOG_WRN("DO NOT SUPPORT VPU_IOCTL_LOCK\n");
		break;
	}
	case VPU_IOCTL_UNLOCK:
	{
		LOG_WRN("DO NOT SUPPORT VPU_IOCTL_LOCK\n");
		break;
	}
	case VPU_IOCTL_LOAD_ALG_TO_POOL:
	{
		char name[VPU_MAX_NAME_SIZE];
		vpu_id_t algo_id[MTK_VPU_CORE];
		int temp_algo_id;
		struct vpu_algo *u_algo;

		u_algo = (struct vpu_algo *) arg;
		ret = copy_from_user(name, u_algo->name, VPU_MAX_NAME_SIZE);
		if (ret) {
			LOG_ERR("[GET_ALGO] copy 'name' failed, ret=%d\n", ret);
			goto out;
		}
		name[VPU_MAX_NAME_SIZE - 1] = '\0';


		for (i = 0 ; i < vpu_device->core_num; i++) {
			temp_algo_id = vpu_get_algo_id_by_name(
					vpu_device->vpu_core[i], name, user);
			if (temp_algo_id < 0) {
				LOG_ERR("[GET_ALGO] %s, name=%s, id:%d\n",
						"can not find algo",
						name, temp_algo_id);
				ret = -ESPIPE;
				goto out;
			} else {
				LOG_DBG("[GET_ALGO] core(%d) name=%s, id=%d\n",
						i, name, temp_algo_id);
			}
			algo_id[i] = (vpu_id_t)temp_algo_id;
		}

		ret = copy_to_user(u_algo->id, algo_id,
				vpu_device->core_num * sizeof(vpu_id_t));
		if (ret) {
			LOG_ERR("[GET_ALGO] update id failed, ret=%d\n", ret);
			goto out;
		}

		break;
	}
	case VPU_IOCTL_CREATE_ALGO:
	{
#ifdef VPU_LOAD_FW_SUPPORT
		struct vpu_create_algo *u_create_algo;
		struct vpu_create_algo create_algo = {0};

		u_create_algo = (struct vpu_create_algo *) arg;
		ret = get_user(create_algo.core, &u_create_algo->core);

		if (ret) {
			LOG_ERR("[CREATE_ALGO] 'core' failed, ret=%d\n",
				ret);
			goto out;
		}

		if (create_algo.core >= vpu_device->core_num) {
			ret = -EINVAL;
			goto out;
		}
		ret = copy_from_user(create_algo.name,
			u_create_algo->name, (sizeof(char)*32));
		if (ret) {
			LOG_ERR("[CREATE_ALGO] 'name' failed, ret=%d\n",
				ret);
			goto out;
		}

		create_algo.name[(sizeof(char)*32) - 1] = '\0';
		ret = get_user(create_algo.algo_length,
			&u_create_algo->algo_length);
		if (ret) {
			LOG_ERR("[CREATE_ALGO] 'algo_length' failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = get_user(create_algo.algo_ptr, &u_create_algo->algo_ptr);
		if (ret) {
			LOG_ERR("[CREATE_ALGO] 'algo_ptr' failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = vpu_add_algo_to_user(user, &create_algo);
		if (ret)
			goto out;
#else
		ret = -EINVAL;
		LOG_WRN("[CREATE_ALGO] was not support!\n");
#endif
		break;
	}
	case VPU_IOCTL_FREE_ALGO:
	{
#ifdef VPU_LOAD_FW_SUPPORT
		struct vpu_create_algo *u_create_algo;
		struct vpu_create_algo create_algo = {0};

		u_create_algo = (struct vpu_create_algo *) arg;
		ret = get_user(create_algo.core, &u_create_algo->core);
		if (ret) {
			LOG_ERR("[FREE_ALGO]'core' failed, ret=%d\n",
				ret);
			goto out;
		}

		if (create_algo.core >= vpu_device->core_num) {
			ret = -EINVAL;
			goto out;
		}
		ret = copy_from_user(create_algo.name,
			u_create_algo->name, (sizeof(char)*32));
		if (ret) {
			LOG_ERR("[FREE_ALGO] 'name' failed, ret=%d\n",
				ret);
			goto out;
		}
		create_algo.name[(sizeof(char)*32) - 1] = '\0';

		vpu_free_algo_from_user(user, &create_algo);
#else
		ret = -EINVAL;
		LOG_WRN("[FREE_ALGO] was not support!\n");
#endif
		break;
	}

	case VPU_IOCTL_GET_CORE_STATUS:
	{
		struct vpu_status *u_status = (struct vpu_status *) arg;
		struct vpu_status status;

		ret = copy_from_user(&status, (void *) arg,
				sizeof(struct vpu_status));
		if (ret) {
			LOG_ERR("[%s]copy 'struct vpu_status' failed ret=%d\n",
					"GET_CORE_STATUS", ret);
			goto out;
		}

		ret = vpu_get_core_status(vpu_device, &status);
		if (ret) {
			LOG_ERR("[%s] vpu_get_core_status failed, ret=%d\n",
					"GET_CORE_STATUS", ret);
			goto out;
		}

		ret = put_user(status.vpu_core_available,
				(bool *)&(u_status->vpu_core_available));
		ret |= put_user(status.pool_list_size,
					(int *)&(u_status->pool_list_size));
		if (ret) {
			LOG_ERR("[%s] put to user failed, ret=%d\n",
					"GET_CORE_STATUS", ret);
			goto out;
		}

		break;
	}
	case VPU_IOCTL_OPEN_DEV_NOTICE:
	{
		struct vpu_dev_debug_info *dev_debug_info;
		struct vpu_dev_debug_info *u_dev_debug_info;

		ret = vpu_alloc_debug_info(&dev_debug_info);
		if (ret) {
			LOG_ERR("[%s] alloc debug_info failed, ret=%d\n",
					"OPEN_DEV_NOTICE", ret);
			goto out;
		}

		u_dev_debug_info = (struct vpu_dev_debug_info *) arg;
		ret = get_user(dev_debug_info->dev_fd,
					&u_dev_debug_info->dev_fd);
		if (ret) {
			LOG_ERR("[%s] copy 'dev_fd' failed, ret=%d\n",
				"VPU_IOCTL_OPEN_DEV_NOTICE", ret);
		}

		ret |= copy_from_user(dev_debug_info->callername,
			u_dev_debug_info->callername, (sizeof(char)*32));
		dev_debug_info->callername[(sizeof(char)*32) - 1] = '\0';
		if (ret) {
			LOG_ERR("[%s] copy 'callname' failed, ret=%d\n",
					"VPU_IOCTL_OPEN_DEV_NOTICE", ret);
		}

		dev_debug_info->open_pid = user->open_pid;
		dev_debug_info->open_tgid = user->open_tgid;

		if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO) {
			LOG_INF("[%s] user:%s/%d. pid(%d/%d)\n",
				"VPU_IOCTL_OPEN_DEV_NOTICE",
				dev_debug_info->callername,
				dev_debug_info->dev_fd,
				dev_debug_info->open_pid,
				dev_debug_info->open_tgid);
		}

		if (ret) {
			/* error handle, free memory */
			vpu_free_debug_info(dev_debug_info);
		} else {
			mutex_lock(&vpu_device->debug_list_mutex);
			list_add_tail(vlist_link(dev_debug_info,
					struct vpu_dev_debug_info),
					&vpu_device->device_debug_list);
			mutex_unlock(&vpu_device->debug_list_mutex);
		}

		break;
	}
	case VPU_IOCTL_CLOSE_DEV_NOTICE:
	{
		int dev_fd;
		struct list_head *head = NULL;
		struct vpu_dev_debug_info *dbg_info;
		bool get = false;

		ret = copy_from_user(&dev_fd, (void *) arg, sizeof(int));
		if (ret) {
			LOG_ERR("[%s] copy 'dev_fd' failed, ret=%d\n",
					"CLOSE_DEV_NOTICE", ret);
			goto out;
		}

		mutex_lock(&vpu_device->debug_list_mutex);
		list_for_each(head, &vpu_device->device_debug_list)
		{
			dbg_info = vlist_node_of(head,
						struct vpu_dev_debug_info);
			if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO) {
				LOG_INF("[%s] req_user-> = %s/%d, %d/%d\n",
					"VPU_IOCTL_CLOSE_DEV_NOTICE",
					dbg_info->callername,
					dbg_info->dev_fd, dbg_info->open_pid,
					dbg_info->open_tgid);
			}

			if (dbg_info->dev_fd == dev_fd) {
				LOG_DBG("[%s] get fd(%d) to close\n",
					"VPU_IOCTL_CLOSE_DEV_NOTICE", dev_fd);
				get = true;
				break;
			}
		}

		if (vpu_device->vpu_log_level > Log_ALGO_OPP_INFO) {
			LOG_INF("[%s] user:%d. pid(%d/%d), get(%d)\n",
					"VPU_IOCTL_CLOSE_DEV_NOTICE",
					dev_fd, user->open_pid,
					user->open_tgid, get);
		}

		if (get) {
			list_del_init(vlist_link(dbg_info,
						struct vpu_dev_debug_info));
			vpu_free_debug_info(dbg_info);
			mutex_unlock(&vpu_device->debug_list_mutex);
		} else {
			mutex_unlock(&vpu_device->debug_list_mutex);
			LOG_ERR("[%s] want to close wrong fd(%d)\n",
				"VPU_IOCTL_CLOSE_DEV_NOTICE", dev_fd);
			ret = -ESPIPE;
			goto out;
		}

		break;
	}
	case VPU_IOCTL_SDSP_SEC_LOCK:
	{
		struct vpu_core *vpu_core = NULL;

		LOG_WRN("SDSP_SEC_LOCK mutex in\n");

		if (vpu_device->in_sec_world)
			break;

		for (i = 0 ; i < vpu_device->core_num ; i++) {
			vpu_core = vpu_device->vpu_core[i];
			mutex_lock(&vpu_core->sdsp_control_mutex);
		}

		LOG_WRN("SDSP_SEC_LOCK mutex-m lock\n");
		ret = vpu_sdsp_get_power(vpu_device);
		LOG_WRN("SDSP_POWER_ON %s\n", ret == 0?"done":"fail");
		if (!vpu_is_available(vpu_device))
			LOG_WRN("vpu_queue is not empty!!\n");

		/* Disable IRQ */
		vpu_device->in_sec_world = true;

		break;
	}
	case VPU_IOCTL_SDSP_SEC_UNLOCK:
	{
		struct vpu_core *vpu_core = NULL;

		if (!vpu_device->in_sec_world)
			break;

		ret = vpu_sdsp_put_power(vpu_device);
		LOG_WRN("DSP_SEC_UNLOCK %s\n", ret == 0?"done":"fail");
		/* Enable IRQ */
		vpu_device->in_sec_world = false;

		for (i = 0 ; i < vpu_device->core_num ; i++) {
			vpu_core = vpu_device->vpu_core[i];
			mutex_unlock(&vpu_core->sdsp_control_mutex);
		}

		LOG_WRN("DSP_SEC_UNLOCK mutex-m unlock\n");

		break;
	}
	case VPU_IOCTL_VBUF_INFO:
	{
		struct vbuf_info info_buf;

		ret =
		    copy_from_user((void *)&info_buf, (void *)arg,
				   sizeof(struct vbuf_info));
		if (ret) {
			LOG_ERR("[VBUF_INFO] get params failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = vbuf_std_info(vpu_device, &info_buf);
		if (ret) {
			LOG_ERR("[VBUF_INFO] info buf failed, ret=%d\n",
				ret);
			goto out;
		}

		ret =
		    copy_to_user((void *)arg, (void *)&info_buf,
				 sizeof(struct vbuf_info));
		if (ret) {
			LOG_ERR("[VBUF_INFO] update params failed, ret=%d\n",
				ret);
			goto out;
		}

		break;
	}
	case VPU_IOCTL_VBUF_ALLOC:
	{
		struct vbuf_alloc alloc_buf;

		ret =
		    copy_from_user((void *)&alloc_buf, (void *)arg,
				   sizeof(struct vbuf_alloc));
		if (ret) {
			LOG_ERR("[VBUF_ALLOC] get params failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = vbuf_std_alloc(vpu_device, &alloc_buf);
		if (ret) {
			LOG_ERR("[VBUF_ALLOC] alloc buf failed, ret=%d\n",
				ret);
			goto out;
		}

		vpu_add_dbg_buf(user, alloc_buf.handle);

		ret =
		    copy_to_user((void *)arg, (void *)&alloc_buf,
				 sizeof(struct vbuf_alloc));
		if (ret) {
			LOG_ERR("[VBUF_ALLOC] update params failed, ret=%d\n",
				ret);
			goto out;
		}

		break;
	}
	case VPU_IOCTL_VBUF_FREE:
	{
		struct vbuf_free free_buf;

		ret =
		    copy_from_user((void *)&free_buf, (void *)arg,
				   sizeof(struct vbuf_free));
		if (ret) {
			LOG_ERR("[VBUF_FREE] get params failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = vbuf_std_free(vpu_device, &free_buf);
		if (ret) {
			LOG_ERR("[VBUF_FREE] free buf failed, ret=%d\n",
				ret);
			goto out;
		}

		vpu_delete_dbg_buf(user, free_buf.handle);

		break;
	}
	case VPU_IOCTL_VBUF_SYNC:
	{
		struct vbuf_sync sync_buf;

		ret =
		    copy_from_user((void *)&sync_buf, (void *)arg,
				   sizeof(struct vbuf_sync));
		if (ret) {
			LOG_ERR("[VBUF_SYNC] get params failed, ret=%d\n",
				ret);
			goto out;
		}

		ret = vbuf_std_sync(vpu_device, &sync_buf);
		if (ret) {
			LOG_ERR("[VBUF_SYNC] sync buf failed, ret=%d\n",
				ret);
			goto out;
		}

		break;
	}
	default:
		LOG_WRN("ioctl: no such command!\n");
		ret = -EINVAL;
		break;
	}

out:
	if (ret) {
		LOG_ERR("fail, cmd(%d), pid(%d), %s=%s, %s=%d, %s=%d\n",
				cmd, user->open_pid,
				"process", current->comm,
				"pid", current->pid,
				"tgid", current->tgid);
	}

	return ret;
}

static int vpu_release(struct inode *inode, struct file *flip)
{
	struct vpu_device *vpu_device = NULL;
	struct vpu_user *user = flip->private_data;

	vpu_device = dev_get_drvdata(user->dev);
	LOG_INF("%s cnt(%d) user->id : 0x%lx, tids(%d/%d)\n", __func__,
		vpu_device->vpu_num_users,
		(unsigned long)(user->id),
		user->open_pid, user->open_tgid);

	vpu_delete_user(user);
	mutex_lock(&vpu_device->debug_list_mutex);
	vpu_device->vpu_num_users--;
	mutex_unlock(&vpu_device->debug_list_mutex);

	if (vpu_device->vpu_num_users > 10)
		vpu_dump_device_dbg(NULL, vpu_device);

	return 0;
}

static int vpu_mmap(struct file *flip, struct vm_area_struct *vma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = (vma->vm_end - vma->vm_start);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	LOG_INF("%s:%s=0x%lx,%s=0x%x,%s=0x%lx,%s=0x%lx,%s=0x%lx,%s=0x%lx\n",
			__func__,
			"vm_pgoff", vma->vm_pgoff,
			"pfn", pfn,
			"phy", vma->vm_pgoff << PAGE_SHIFT,
			"vm_start", vma->vm_start,
			"vm_end", vma->vm_end,
			"length", length);

	switch (pfn) {

	default:
		LOG_ERR("illegal hw addr for mmap!\n");
		return -EAGAIN;
	}
}

static inline void vpu_unreg_chardev(struct vpu_device *vpu_device)
{
	/* Release char driver */
	cdev_del(&vpu_device->vpu_chardev);
	unregister_chrdev_region(vpu_device->vpu_devt, 1);
}

static inline int vpu_reg_chardev(struct vpu_device *vpu_device)
{
	int ret = 0;

	ret = alloc_chrdev_region(&vpu_device->vpu_devt, 0, 1, VPU_DEV_NAME);
	if ((ret) < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}

	/* Attatch file operation. */
	cdev_init(&vpu_device->vpu_chardev, &vpu_fops);

	vpu_device->vpu_chardev.owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(&vpu_device->vpu_chardev, vpu_device->vpu_devt, 1);
	if ((ret) < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto out;
	}

out:
	if (ret < 0)
		vpu_unreg_chardev(vpu_device);

	return ret;
}

/******************************************************************************
 * platform_driver
 *****************************************************************************/
static int vpu_initialize(struct platform_device *pdev,
				  struct vpu_device *vpu_device)
{
#ifdef MTK_VPU_FPGA_PORTING
	/* get register address */
#ifdef NEVER
	if (strncmp(node->name, "ipu_conn",
					strlen("ipu_conn"))) {
		vpu_device->vpu_syscfg_base =
				(unsigned long) of_iomap(node, 0);
	} else if (strncmp(node->name, "ipu_adl",
					strlen("ipu_adl"))) {
		vpu_device->vpu_adlctrl_base =
				(unsigned long) of_iomap(node, 0);
	} else if (strncmp(node->name, "ipu_vcore",
					strlen("ipu_vcore"))) {
		vpu_device->vpu_vcorecfg_base =
				(unsigned long) of_iomap(node, 0);
	}
#endif /* NEVER */
	struct device_node *tmp;

	tmp = of_find_compatible_node(NULL, NULL, "mediatek,ipu_conn");
	vpu_device->vpu_syscfg_base = (unsigned long) of_iomap(tmp, 0);

	tmp = of_find_compatible_node(NULL, NULL, "mediatek,ipu_vcore");
	vpu_device->vpu_vcorecfg_base =	(unsigned long) of_iomap(tmp, 0);

	LOG_DBG("probe 2, %s=0x%lx, %s=0x%lx, %s=0x%lx\n",
		 "vpu_syscfg_base", vpu_device->vpu_syscfg_base,
		 "vpu_adlctrl_base", vpu_device->vpu_adlctrl_base,
		 "vpu_vcorecfg_base", vpu_device->vpu_vcorecfg_base);
#endif

#ifdef MTK_VPU_EMULATOR
	/* emulator will fill vpu_base and bin_base */
	vpu_init_emulator(vpu_device);
#else
	struct device_node *smi_node = NULL;
	struct device_node *node;
	struct resource *mem = NULL;
	struct device *dev;
	uint32_t phy_addr;
	uint32_t phy_size;
	int i, ret = 0;

	node = pdev->dev.of_node;
	/* get physical address of binary data loaded by LK */
	if (of_property_read_u32(node, "bin-phy-addr", &phy_addr) ||
		of_property_read_u32(node, "bin-size", &phy_size)) {
		LOG_INF("fail to get phy address of vpu binary!\n");
		vpu_device->vpu_load_image_state = VPU_LOAD_IMAGE_UNLOAD;
	} else {
		/* bin_base for cpu read/write */
		vpu_device->bin_base =
			(unsigned long)ioremap_wc(phy_addr, phy_size);
		vpu_device->bin_pa = phy_addr;
		vpu_device->bin_size = phy_size;

		vpu_device->image_header =
			(struct vpu_image_header *)
			((uintptr_t)vpu_device->bin_base +
			(VPU_OFFSET_IMAGE_HEADERS));

		LOG_INF("probe, %s=0x%lx %s=0x%x, %s=0x%x\n",
			"bin_base", (unsigned long)vpu_device->bin_base,
			"phy_addr", phy_addr,
			"phy_size", phy_size);
	}

	/* get smi common register */
#ifdef MTK_VPU_SMI_DEBUG_ON
	smi_node = of_find_compatible_node(NULL, NULL,
					"mediatek,smi_common");
	vpu_device->smi_cmn_base = (unsigned long) of_iomap(smi_node, 0);
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		LOG_ERR("cannot get IORESOUCE_MEM\n");
		return -ENOENT;
	}

	vpu_device->vpu_vcorecfg_base =
		(unsigned long)devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR((void *)vpu_device->vpu_vcorecfg_base)) {
		LOG_ERR("cannot get ioremap 1\n");
		return -ENOENT;
	}

	LOG_INF(
		"probe, smi_cmn_base: 0x%lx, vpu_vcorecfg_base:0x%lx\n",
		vpu_device->smi_cmn_base,
		vpu_device->vpu_vcorecfg_base);
#endif

	vpu_init_algo(vpu_device);
	LOG_DBG("[probe] init_algo done\n");

	ret = vpu_init_device(vpu_device);
	if (ret) {
		LOG_ERR("vpu_init_device failed");
		goto return_err;
	}

	for (i = 0; i < vpu_device->core_num; i++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[i];

		vpu_core->bin_base = vpu_device->bin_base;
		ret = vpu_init_hw(vpu_core);
		if (ret) {
			LOG_ERR("vpu_init_hw failed");
			goto deinit_core;
		}
		LOG_DBG("[probe] [%d] init_hw done\n", i);

		ret = vpu_init_util(vpu_core);
		if (ret) {
			LOG_ERR("vpu_init_util failed");
			goto deinit_core;
		}
	}

#ifdef MET_POLLING_MODE
	vpu_init_profile(vpu_device);
	LOG_DBG("[probe] [%d] vpu_init_profile done\n", i);
#endif

	/* Only register char driver in the 1st time */
	ret = vpu_init_debug(vpu_device);
	if (ret) {
		LOG_ERR("vpu_init_debug failed");
		goto deinit_core;
	}

	/* Register char driver */
	ret = vpu_reg_chardev(vpu_device);
	if (ret) {
		LOG_ERR("register char failed");
		goto deinit_debug;
	}

	/* Create class register */
	vpu_device->vpu_class = class_create(THIS_MODULE, "vpudrv");
	if (IS_ERR(vpu_device->vpu_class)) {
		ret = PTR_ERR(vpu_device->vpu_class);
		LOG_ERR("Unable to create class, err = %d\n", ret);
		goto unreg_chadev;
	}

	dev = device_create(vpu_device->vpu_class, NULL, vpu_device->vpu_devt,
				NULL, VPU_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		LOG_ERR("Failed to create device: /dev/%s, err = %d",
			VPU_DEV_NAME, ret);
		goto class_destroy;
	}

	return 0;

class_destroy:
	class_destroy(vpu_device->vpu_class);

unreg_chadev:
	vpu_unreg_chardev(vpu_device);

deinit_debug:
	vpu_deinit_debug(vpu_device);

deinit_core:
	for (i = 0; i < vpu_device->core_num; i++) {
		struct vpu_core *vpu_core = vpu_device->vpu_core[i];

		if (vpu_core->vpu_util != NULL)
			vpu_deinit_util(vpu_core);

		if (vpu_core->vpu_hw_init_done == 1)
			vpu_uninit_hw(vpu_core);
	}

	vpu_uninit_device(vpu_device);
return_err:

	return ret;
}

static void vpu_deinitialize(struct vpu_device *vpu_device)
{
	int i;

	device_destroy(vpu_device->vpu_class, vpu_device->vpu_devt);
	class_destroy(vpu_device->vpu_class);
	vpu_device->vpu_class = NULL;

	vpu_unreg_chardev(vpu_device);

	vpu_deinit_debug(vpu_device);

#ifdef MET_POLLING_MODE
	vpu_uninit_profile(vpu_device);
#endif

	for (i = 0; i < vpu_device->core_num; i++) {
		vpu_deinit_util(vpu_device->vpu_core[i]);
		vpu_uninit_hw(vpu_device->vpu_core[i]);
	}

	vpu_uninit_device(vpu_device);
}

static int vpu_core_attach(struct platform_device *pdev,
			   struct vpu_device *vpu_device)
{
	struct device *dev = &pdev->dev;
	struct device_node *core_node;
	struct platform_device *core_pdev;
	int i = 0;

	do {
		struct vpu_core *vpu_core = NULL;

		core_node = of_parse_phandle(dev->of_node,
					     "mediatek,vpucore", i++);
		if (!core_node)
			break;

		core_pdev = of_find_device_by_node(core_node);
		if (core_pdev)
			vpu_core = platform_get_drvdata(core_pdev);

		if (!vpu_core) {
			LOG_ERR("Waiting for vpu core %s\n",
				 core_node->full_name);
			of_node_put(core_node);
			return -EPROBE_DEFER;
		}
		of_node_put(core_node);

		if (i > MTK_VPU_CORE)
			return -EINVAL;

		vpu_core->core = i - 1;
		sprintf(vpu_core->name, "vpu%d", vpu_core->core);
		vpu_device->vpu_core[i - 1] = vpu_core;
		vpu_core->vpu_device = vpu_device;
	} while (core_node);

	vpu_device->core_num = i - 1;

	if (vpu_device->core_num == 0) {
		LOG_ERR("no vpu core is available\n");
		return -EINVAL;
	}

	if (vpu_device->core_num != MTK_VPU_CORE) {
		LOG_ERR("core_num(%d) != MTK_VPU_CORE(%d)\n",
			vpu_device->core_num, MTK_VPU_CORE);
		return -EINVAL;
	}

	return 0;
}

static int vpu_core_detach(struct platform_device *pdev,
			   struct vpu_device *vpu_device)
{
	return 0;
}

static int vpu_probe(struct platform_device *pdev)
{
	struct vpu_device *vpu_device;
	struct device *dev = &pdev->dev;
	int ret = 0;

	vpu_device = devm_kzalloc(dev, sizeof(struct vpu_device), GFP_KERNEL);
	if (!vpu_device)
		return -ENOMEM;

	vpu_device->vpu_init_done = 0;
	vpu_device->dev = dev;
	ret = vpu_core_attach(pdev, vpu_device);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&vpu_device->user_list);
	mutex_init(&vpu_device->user_mutex);

	INIT_LIST_HEAD(&vpu_device->cmnpool_list);
	mutex_init(&vpu_device->commonpool_mutex);
	vpu_device->commonpool_list_size = 0;
	init_waitqueue_head(&vpu_device->req_wait);
	INIT_LIST_HEAD(&vpu_device->device_debug_list);
	mutex_init(&vpu_device->debug_list_mutex);
	idr_init(&vpu_device->addr_idr);

	ret = vpu_initialize(pdev, vpu_device);
	if (ret)
		goto dev_out;

	platform_set_drvdata(pdev, vpu_device);

	vpu_device->vpu_init_done = 1;
	LOG_INF("[probe] vpu probe total initialization done\n");

	return 0;

dev_out:

	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	int ret;
	struct vpu_device *vpu_device = platform_get_drvdata(pdev);

	if (!vpu_device) {
		LOG_ERR("Failed to get vpu_device\n");
		return -EINVAL;
	}

	vpu_deinitialize(vpu_device);

	idr_destroy(&vpu_device->addr_idr);

	ret = vpu_core_detach(pdev, vpu_device);
	if (ret)
		return ret;

	vpu_device->vpu_init_done = 0;

	return 0;
}

static int vpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int i = 0;
	struct vpu_device *vpu_device = platform_get_drvdata(pdev);

	if (!vpu_device) {
		LOG_ERR("Failed to get vpu_device\n");
		return -EINVAL;
	}

	for (i = 0 ; i < vpu_device->core_num ; i++)
		vpu_quick_suspend(vpu_device->vpu_core[i]);

	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	return 0;
}

static int __init VPU_INIT(void)
{
	LOG_DBG("platform_driver_register start\n");
	if (platform_driver_register(&vpu_core_driver)) {
		LOG_ERR("failed to register VPU core driver");
		return -ENODEV;
	}

	if (platform_driver_register(&vpu_driver)) {
		LOG_ERR("failed to register VPU driver");
		return -ENODEV;
	}
	LOG_DBG("platform_driver_register finsish\n");

	return 0;
}

static void __exit VPU_EXIT(void)
{
	platform_driver_unregister(&vpu_driver);
	platform_driver_unregister(&vpu_core_driver);
}


module_init(VPU_INIT);
module_exit(VPU_EXIT);
MODULE_DESCRIPTION("MTK VPU Driver");
MODULE_AUTHOR("SW6");
MODULE_LICENSE("GPL");
