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

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sched_clock.h>
#include <linux/time.h>
#include <linux/uaccess.h>

static DEFINE_MUTEX(boost_mutex);

#define BOOST_FILE_TASKS                   "/dev/stune/io/tasks"
#define BOOST_PRINT_PREFIX                 "[io-boost]"
#define BOOST_OPEN_TRIAL_DURATION_SEC      (1)

#define boost_print(fmt, arg...) \
	pr_debug(BOOST_PRINT_PREFIX fmt, ##arg)
#define boost_print_no_prefix(fmt, arg...)  pr_debug(fmt, ##arg)

static struct file *boost_tasks_fd;
static bool boost_task_file_ready;

static int open_boost_task_file(void)
{
	int err = 0;
	mm_segment_t oldfs;
	static u64 last_trial_time;

	if (!boost_task_file_ready) {
		if ((sched_clock() - last_trial_time) < NSEC_PER_SEC) {
			err = -EAGAIN;
			goto out;
		} else
			last_trial_time = sched_clock();
	}

	oldfs = get_fs();
	set_fs(get_ds());

	boost_tasks_fd = filp_open(BOOST_FILE_TASKS, O_WRONLY, 0);

	set_fs(oldfs);

	if (IS_ERR(boost_tasks_fd)) {

		err = PTR_ERR(boost_tasks_fd);

		boost_tasks_fd = NULL;

	} else
		boost_task_file_ready = true;

out:

	return err;
}

static void close_boost_task_file(void)
{
	if (boost_tasks_fd)
		filp_close(boost_tasks_fd, NULL);
}

static int add_tid_to_task_file(int tid)
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

	ret = vfs_write(boost_tasks_fd,
		(__force const char __user *)ptr, end - ptr, &pos);

	set_fs(old_fs);

	if (ret < 0)
		boost_print("write task failed, ret:%d\n", (int)ret);

	return 0;
}

int mtk_io_boost_add_tid(int tid)
{
	int ret = 0;

	mutex_lock(&boost_mutex);

	ret = open_boost_task_file();

	if (ret < 0)
		goto out;

	add_tid_to_task_file(tid);

	close_boost_task_file();

out:

	mutex_unlock(&boost_mutex);

	if (ret != -EAGAIN)
		boost_print("add tid %d, ret=%d\n", tid, ret);

	return ret;
}

int mtk_io_boost_test_and_add_tid(int tid, bool *done)
{
	int ret;

	if (done && *done)
		return 0;

	ret = mtk_io_boost_add_tid(tid);

	if (!ret && done)
		*done = true;

	return ret;
}

#else

int mtk_io_boost_add_tid(int tid)
{
	return 0;
}

int mtk_io_boost_test_and_add_tid(int tid, bool *done)
{
	return 0;
}

#endif

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek IO Booster");

