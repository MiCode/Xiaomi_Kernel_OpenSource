/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>

#if defined(CONFIG_MTK_IO_BOOST)

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched_clock.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

struct bst_tid_struct {
	pid_t tid;
};

#define BST_TASK_FILE_PATH  "/dev/stune/io/tasks"
#define BST_MAX_TID         (20)

#define boost_print(fmt, arg...) \
	pr_debug(BOOST_PRINT_PREFIX fmt, ##arg)
#define boost_print_no_prefix(fmt, arg...)  pr_debug(fmt, ##arg)
DEFINE_SPINLOCK(bst_lock);

static struct bst_tid_struct bst_tid[BST_MAX_TID];
static int bst_tid_cnt;
static int bst_tid_handled_cnt;
static int bst_tid_pending_cnt;
static int bst_init_done;
static wait_queue_head_t bst_wq;
static struct file *bst_task_fd;

static int mtk_iobst_open_task_file(void)
{
	int err = 0;
	int retry_cnt = 0;
	mm_segment_t oldfs;

retry:

	oldfs = get_fs();
	set_fs(get_ds());

	bst_task_fd = filp_open(BST_TASK_FILE_PATH, O_WRONLY, 0);

	set_fs(oldfs);

	if (IS_ERR(bst_task_fd)) {
		err = PTR_ERR(bst_task_fd);
		bst_task_fd = NULL;

		if (retry_cnt++ < 1) {
			msleep_interruptible(20 * 1000);
			err = 0;
			goto retry;
		}
	}

	return err;
}

static void mtk_iobst_close_task_file(void)
{
	if (bst_task_fd)
		filp_close(bst_task_fd, NULL);
}

static int mtk_iobst_get_next_tid(void)
{
	int tid = -1;

	spin_lock_irq(&bst_lock);

	if (bst_tid_pending_cnt) {
		tid = bst_tid[bst_tid_handled_cnt].tid;
		bst_tid_handled_cnt++;
		bst_tid_pending_cnt--;
	}

	spin_unlock_irq(&bst_lock);

	return tid;
}

static int mtk_iobst_add_task_internal(int tid)
{
	/* specialized itoa -- works for tid > 0 */
	char text[22];
	char *end = text + sizeof(text) - 1;
	char *ptr = end;
	loff_t pos = 0;
	ssize_t ret;
	mm_segment_t old_fs;

	*ptr = '\0';

	while (tid > 0) {
		*--ptr = '0' + (tid % 10);
		tid = tid / 10;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = vfs_write(bst_task_fd,
		(__force const char __user *)ptr, end - ptr, &pos);

	set_fs(old_fs);

	/* ignore "no such process" error since process may be deleted */
	if (ret < 0 && (int)ret != -ESRCH)
		pr_info("failed to write task, ret:%d\n", (int)ret);

	return 0;
}

static int mtk_iobst_add_task(void)
{
	int ret = 0;
	int tid;

	ret = mtk_iobst_open_task_file();

	if (ret < 0) {
		pr_info("failed to open task file, ret=%d\n", ret);
		return ret;
	}

	while ((tid = mtk_iobst_get_next_tid()) != -1) {
		ret = mtk_iobst_add_task_internal(tid);
		if (ret)
			pr_info("failed to add tid=%d, ret=%d\n", tid, ret);
	}

	mtk_iobst_close_task_file();

	return ret;
}

int mtk_iobst_register_tid(int tid)
{
	int ret = -1;

	spin_lock_irq(&bst_lock);

	if (bst_tid_cnt < BST_MAX_TID) {
		bst_tid[bst_tid_cnt].tid = tid;
		bst_tid_cnt++;
		bst_tid_pending_cnt++;
		ret = 0;
	}

	spin_unlock_irq(&bst_lock);

	if (ret)
		pr_info("failed to register tid=%d\n", tid);
	else if (bst_init_done)
		wake_up_interruptible(&bst_wq);

	return ret;
}

static int mtk_iobst_thread(void *data)
{
	spin_lock_irq(&bst_lock);

	while (1) {
		wait_event_interruptible_lock_irq(bst_wq,
			bst_tid_pending_cnt || kthread_should_stop(),
			bst_lock);

		if (kthread_should_stop())
			break;

		spin_unlock_irq(&bst_lock);

		/* add pending tids to task file */
		mtk_iobst_add_task();

		spin_lock_irq(&bst_lock);
	}

	spin_unlock_irq(&bst_lock);

	return 0;
}


static int __init mtk_iobst_init(void)
{
	init_waitqueue_head(&bst_wq);

	kthread_run(mtk_iobst_thread, NULL, "mtk_io_boost");

	bst_init_done = 1;

	return 0;
}

module_init(mtk_iobst_init);

#else

int mtk_iobst_register_tid(int tid, bool *done)
{
	return 0;
}

#endif

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek IO Booster");

