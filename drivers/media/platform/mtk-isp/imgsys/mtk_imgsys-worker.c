// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <johnson-ch.chiu@mediatek.com>
 *
 */
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include "mtk_imgsys-worker.h"
#include "mtk_imgsys-trace.h"

int imgsys_queue_init(struct imgsys_queue *que, struct device *dev, char *name)
{
	int ret = 0;

	if ((!que) || (!dev)) {
		ret = -1;
		goto EXIT;
	}

	que->name = name;
	que->dev = dev;
	INIT_LIST_HEAD(&que->queue);
	init_waitqueue_head(&que->wq);
	init_waitqueue_head(&que->dis_wq);
	spin_lock_init(&que->lock);
	atomic_set(&que->nr, 0);
	que->peak = 0;
	mutex_init(&que->task_lock);

EXIT:
	return ret;
}

static int worker_func(void *data)
{
	struct imgsys_queue *head = data;
	struct imgsys_work *node;
	struct list_head *list;
	u64 start;
	u64 end;

	while (1) {
		dev_dbg(head->dev, "%s: %s kthread sleeps\n", __func__,
								head->name);
		wait_event_interruptible(head->wq,
			atomic_read(&head->nr) || atomic_read(&head->disable));
		dev_dbg(head->dev, "%s: %s kthread wakes dis/nr(%d/%d)\n", __func__,
				head->name, atomic_read(&head->disable), atomic_read(&head->nr));

		spin_lock(&head->lock);
		if (atomic_read(&head->disable) || !atomic_read(&head->nr)) {
			spin_unlock(&head->lock);
			dev_info(head->dev, "%s: %s: nr(%d) dis(%d)\n", __func__,
					head->name, atomic_read(&head->nr),
						atomic_read(&head->disable));
			goto next;
		}

		list = head->queue.next;
		list_del(list);
		atomic_dec(&head->nr);
		spin_unlock(&head->lock);

		node = list_entry(list, struct imgsys_work, entry);

		IMGSYS_SYSTRACE_BEGIN("%s work:%p nr:%d\n", __func__, node, atomic_read(&head->nr));

		start = ktime_get_boottime_ns();
		if (node->run)
			node->run(node);
		end = ktime_get_boottime_ns();
		if ((end - start) > 2000000)
			dev_dbg(head->dev, "%s: work run time %lld > 2ms\n",
			__func__, (end - start));

		IMGSYS_SYSTRACE_END();

next:
		if (kthread_should_stop()) {
			dev_dbg(head->dev, "%s: %s kthread exits\n", __func__, head->name);
			break;
		}
	}

	dev_info(head->dev, "%s: %s exited\n", __func__, head->name);

	return 0;
}

int imgsys_queue_enable(struct imgsys_queue *que)
{
	if (!que)
		return -1;

	mutex_lock(&que->task_lock);
	que->task = kthread_create(worker_func, (void *)que, que->name);
	if (IS_ERR(que->task)) {
		mutex_unlock(&que->task_lock);
		dev_info(que->dev, "%s: kthread_run failed\n", __func__);
		return PTR_ERR(que->task);
	}
	sched_set_normal(que->task, -20);
	get_task_struct(que->task);
	atomic_set(&que->disable, 0);
	wake_up_process(que->task);
	mutex_unlock(&que->task_lock);

	return 0;
}

#define TIMEOUT (300)
int imgsys_queue_disable(struct imgsys_queue *que)
{
	int ret;

	if ((!que) || IS_ERR(que->task))
		return -1;

	ret = wait_event_interruptible_timeout(que->dis_wq, !atomic_read(&que->nr),
						msecs_to_jiffies(TIMEOUT));
	if (!ret)
		dev_info(que->dev, "%s: timeout", __func__);
	else if (ret == -ERESTARTSYS)
		dev_info(que->dev, "%s: signal interrupt", __func__);

	mutex_lock(&que->task_lock);

	atomic_set(&que->disable, 1);
	ret = kthread_stop(que->task);
	if (ret)
		dev_info(que->dev, "%s: kthread_stop failed %d\n",
						__func__, ret);

	put_task_struct(que->task);
	que->task = NULL;

	dev_info(que->dev, "%s: kthread(%s) queue peak(%d)\n",
		__func__, que->name, que->peak);

	mutex_unlock(&que->task_lock);

	mutex_destroy(&que->task_lock);

	return ret;
}

int imgsys_queue_add(struct imgsys_queue *que, struct imgsys_work *work)
{
	int size;

	if ((!que) || (!work) || (!que->task))
		return -1;

	if (!que->task) {
		dev_info(que->dev, "%s %s not enabled\n", __func__, que->name);
		return -1;
	}

	if (!work->run)
		dev_info(que->dev, "%s no work func added\n", __func__);

	spin_lock(&que->lock);
	list_add_tail(&work->entry, &que->queue);
	size = atomic_inc_return(&que->nr);
	if (size > que->peak)
		que->peak = size;
	spin_unlock(&que->lock);

	dev_dbg(que->dev, "%s try wakeup dis/nr(%d/%d)\n", __func__,
		atomic_read(&que->disable), atomic_read(&que->nr));
	wake_up(&que->wq);

	dev_dbg(que->dev, "%s: raising %s\n", __func__, que->name);

	return 0;
}

int imgsys_queue_timeout(struct imgsys_queue *que)
{
	struct imgsys_work *work, *tmp;

	spin_lock(&que->lock);

	dev_info(que->dev, "%s: stalled work+\n");
	list_for_each_entry_safe(work, tmp,
		&que->queue, entry){
		dev_info(que->dev, "%s: work %p\n", __func__, work);
	}
	dev_info(que->dev, "%s: stalled work-\n");

	spin_unlock(&que->lock);

	return 0;
}
