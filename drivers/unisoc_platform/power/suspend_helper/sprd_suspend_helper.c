// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2019-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/suspend.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define SYNC_TIMEOUT_MS_DEF  (3000)

static int sprd_sys_sync_timeout = SYNC_TIMEOUT_MS_DEF;
module_param(sprd_sys_sync_timeout, int, 0644);
MODULE_PARM_DESC(sprd_sys_sync_timeout, "sprd sys sync timeout_ms (default: 3000)");

struct sprd_sys_sync_data {
	struct workqueue_struct *suspend_sys_sync_work_queue;
	struct wakeup_source *sys_sync_check_ws;
	struct work_struct suspend_sys_sync_work;
	struct notifier_block pm_notifier_block;
	wait_queue_head_t waitq;
	struct mutex sync_mtx;
	bool sync_done;
	bool timeout;
};

static struct sprd_sys_sync_data *sprd_sys_sync_get_instance(void)
{
	static struct sprd_sys_sync_data *sync_data;

	if (sync_data)
		return sync_data;

	sync_data = kzalloc(sizeof(struct sprd_sys_sync_data), GFP_KERNEL);

	return sync_data;
}

static void sprd_sys_sync_work_handler(struct work_struct *work)
{
	struct sprd_sys_sync_data *sync_data = sprd_sys_sync_get_instance();

	ksys_sync_helper();

	mutex_lock(&sync_data->sync_mtx);
	if (!sync_data->timeout) {
		sync_data->sync_done = true;
		wake_up_interruptible(&sync_data->waitq);
	} else if (sync_data->sys_sync_check_ws->active) {
		__pm_relax(sync_data->sys_sync_check_ws);
	}
	mutex_unlock(&sync_data->sync_mtx);
}

static int sprd_do_sys_sync(void)
{
	unsigned long dirty;
	struct sprd_sys_sync_data *sync_data = sprd_sys_sync_get_instance();
	int notify_ret = NOTIFY_DONE;

	sync_data->sync_done = false;
	sync_data->timeout = false;
	queue_work(sync_data->suspend_sys_sync_work_queue, &sync_data->suspend_sys_sync_work);

	if (!wait_event_interruptible_timeout(sync_data->waitq, (sync_data->sync_done == true),
					      msecs_to_jiffies(sprd_sys_sync_timeout))) {
		mutex_lock(&sync_data->sync_mtx);
		if (!sync_data->sync_done) {
			sync_data->timeout = true;
			notify_ret = NOTIFY_BAD;
			/* timeout, aquire a wakeup source to ensure the sync done */
			__pm_stay_awake(sync_data->sys_sync_check_ws);
		}
		mutex_unlock(&sync_data->sync_mtx);
	}

	dirty = (global_node_page_state(NR_FILE_DIRTY)
		+ global_node_page_state(NR_WRITEBACK)) << (PAGE_SHIFT - 10);

	pr_info("%s: dirty: %luKb sync_done: %d timeout[%d:%d]\n",
		__func__, dirty, sync_data->sync_done, sync_data->timeout, sprd_sys_sync_timeout);

	return notify_ret;
}

static int sprd_sys_sync_pm_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct sprd_sys_sync_data *sync_data_ptr = sprd_sys_sync_get_instance();
	int notify_ret = NOTIFY_DONE;

	if (!sync_data_ptr)
		return NOTIFY_DONE;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		notify_ret = sprd_do_sys_sync();
		break;
	default:
		break;
	}
	return notify_ret;
}

static int __init sprd_sys_sync_init(void)
{
	struct sprd_sys_sync_data *sync_data_ptr = sprd_sys_sync_get_instance();
	int ret;

	if (!sync_data_ptr)
		return -ENOMEM;

	sync_data_ptr->sys_sync_check_ws = wakeup_source_register(NULL, "sys_sync_ws");
	if (!sync_data_ptr->sys_sync_check_ws) {
		pr_err("sync_queue_init: wakeup_source_register err!");
		ret = -EBUSY;
		goto err_ws;
	}

	sync_data_ptr->suspend_sys_sync_work_queue = create_singlethread_workqueue("sys_sync_wq");
	if (!sync_data_ptr->suspend_sys_sync_work_queue) {
		pr_err("sync_queue_init: create_singlethread_workqueue err!");
		ret = -EBUSY;
		goto err_wq;
	}

	INIT_WORK(&sync_data_ptr->suspend_sys_sync_work, sprd_sys_sync_work_handler);
	init_waitqueue_head(&sync_data_ptr->waitq);
	mutex_init(&sync_data_ptr->sync_mtx);
	sync_data_ptr->pm_notifier_block.notifier_call = sprd_sys_sync_pm_notifier;
	sync_data_ptr->pm_notifier_block.priority = S32_MAX;
	ret = register_pm_notifier(&sync_data_ptr->pm_notifier_block);
	if (ret) {
		pr_err("sys sync nb register err!");
		goto err_nb;
	}

	return 0;

err_nb:
	destroy_workqueue(sync_data_ptr->suspend_sys_sync_work_queue);
err_wq:
	wakeup_source_unregister(sync_data_ptr->sys_sync_check_ws);
err_ws:
	kfree(sync_data_ptr);

	return ret;
}

static void  __exit sprd_sys_sync_exit(void)
{
	struct sprd_sys_sync_data *sync_data = sprd_sys_sync_get_instance();

	if (!sync_data)
		return;

	flush_work(&sync_data->suspend_sys_sync_work);
	destroy_workqueue(sync_data->suspend_sys_sync_work_queue);
	wakeup_source_unregister(sync_data->sys_sync_check_ws);
	unregister_pm_notifier(&sync_data->pm_notifier_block);
	kfree(sync_data);
}

module_init(sprd_sys_sync_init);
module_exit(sprd_sys_sync_exit);

MODULE_DESCRIPTION("sprd suspend helper");
MODULE_LICENSE("GPL v2");

