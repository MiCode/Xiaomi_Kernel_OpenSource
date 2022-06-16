// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/poll.h>
#include <linux/proc_fs.h>

#include "dpmaif_debug.h"
#include "net_pool.h"
#include "ccci_port.h"
#include "ccci_hif.h"

#define TAG "dbg"






#define DEBUG_BUFFER_LEN   (163840)
#define DEBUG_MIN_READ_LEN (4000)


struct dpmaif_debug_buffer {
	atomic_t          dbg_user_cnt;
	wait_queue_head_t dbg_wq;
	spinlock_t        dbg_lock;

	char         *data;
	unsigned int  rd;
	unsigned int  wr;

	int           pre_call_wq;
};


static struct dpmaif_debug_buffer g_debug_buf;
static unsigned int               g_debug_buf_len;

unsigned int                      g_debug_flags;


static inline u32 get_ringbuf_free_cnt(u32 len, u32 rdx, u32 wdx)
{
	if (wdx >= rdx)
		return len - wdx + rdx - 1;

	return (rdx - wdx) - 1;
}

static inline u32 get_ringbuf_used_cnt(u32 len, u32 rdx, u32 wdx)
{
	if (wdx >= rdx)
		return (wdx - rdx);

	return (len - rdx + wdx);
}

static inline u32 get_ringbuf_next_idx(u32 len, u32 idx, u32 cnt)
{
	idx += cnt;

	if (idx >= len)
		idx -= len;

	return idx;
}

void dpmaif_debug_add(void *data, int len)
{
	unsigned long flags;
	unsigned int free_cnt;

	spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);

	if (g_debug_buf.data == NULL)
		goto _func_exit_;

	free_cnt = get_ringbuf_free_cnt(g_debug_buf_len, g_debug_buf.rd, g_debug_buf.wr);
	if (len <= free_cnt) {
		if ((g_debug_buf.wr + len) > g_debug_buf_len) {
			memcpy(g_debug_buf.data + g_debug_buf.wr, data,
						g_debug_buf_len - g_debug_buf.wr);
			memcpy(g_debug_buf.data, data,
						len - (g_debug_buf_len - g_debug_buf.wr));
		} else
			memcpy(g_debug_buf.data + g_debug_buf.wr, data, len);

		/* for cpu exec. */
		smp_wmb();

		g_debug_buf.wr = get_ringbuf_next_idx(g_debug_buf_len, g_debug_buf.wr, len);

		len += (g_debug_buf_len - free_cnt);
		if (len > g_debug_buf.pre_call_wq) {
			if ((len - g_debug_buf.pre_call_wq) > DEBUG_MIN_READ_LEN) {
				g_debug_buf.pre_call_wq += DEBUG_MIN_READ_LEN;
				wake_up_all(&g_debug_buf.dbg_wq);
			}
		} else
			g_debug_buf.pre_call_wq = len - 1;
	}

_func_exit_:
	spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);
}

static ssize_t dpmaif_debug_read(struct file *file, char __user *buf,
		size_t size, loff_t *ppos)
{
	unsigned int read_len;
	int ret, len;

	read_len = get_ringbuf_used_cnt(g_debug_buf_len, g_debug_buf.rd, g_debug_buf.wr);
	if (read_len == 0)
		return 0;

	if (read_len > size)
		read_len = size;

	if ((g_debug_buf.rd + read_len) > g_debug_buf_len) {
		len = g_debug_buf_len - g_debug_buf.rd;

		ret = copy_to_user(buf, g_debug_buf.data + g_debug_buf.rd, len);
		if (ret) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: copy_to_user() fail; len: %d(%d)\n",
				__func__, len, ret);
			return 0;
		}

		ret = copy_to_user(buf + len, g_debug_buf.data, read_len - len);

	} else
		ret = copy_to_user(buf, g_debug_buf.data + g_debug_buf.rd, read_len);

	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_to_user() fail; read_len: %d(%d)\n",
			__func__, read_len, ret);
		return 0;
	}

	g_debug_buf.rd = get_ringbuf_next_idx(g_debug_buf_len, g_debug_buf.rd, read_len);

	return read_len;
}

static void dpmaif_sysfs_parse(char *buf, int size)
{
	char *psub = NULL, *pname = NULL, *pvalue = NULL, *pdata = NULL;
	unsigned int debug_buf_len = 0, wake_up_flag = 0;
	unsigned long flags;

	if (!buf || size <= 0)
		return;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] size: %d; buf: %s\n", __func__, size, buf);

	pname = buf;
	while (1) {
		psub = strchr(pname, '|');

		if (psub) {
			*psub = '\0';
			psub += 1;
		}

		pvalue = strchr(pname, '=');
		if (pvalue) {
			*pvalue = '\0';
			pvalue += 1;
		}

		if (strstr(pname, "debug_flags")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 16, &g_debug_flags))
					return;
		} else if (strstr(pname, "debug_buf_len")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 10, &debug_buf_len))
					return;
		} else if (strstr(pname, "run_wq")) {
			if (pvalue && *pvalue)
				if (kstrtouint(pvalue, 10, &wake_up_flag))
					return;
			if (wake_up_flag)
				wake_up_all(&g_debug_buf.dbg_wq);
		}

		if (!psub)
			break;

		pname = psub;
	}

	if ((debug_buf_len > 0) && (g_debug_buf_len != debug_buf_len)) {
		spin_lock_irqsave(&g_debug_buf.dbg_lock, flags);

		pdata = g_debug_buf.data;
		g_debug_buf.data = NULL;
		g_debug_buf.rd  = 0;
		g_debug_buf.wr  = 0;
		g_debug_buf.pre_call_wq = 0;
		g_debug_buf_len = debug_buf_len;

		spin_unlock_irqrestore(&g_debug_buf.dbg_lock, flags);

		if (pdata)
			vfree(pdata);

		if (g_debug_buf_len > 0) {
			g_debug_buf.data = vmalloc(g_debug_buf_len);
			CCCI_NORMAL_LOG(-1, TAG, "[%s] vmalloc(%u): %p\n",
				__func__, g_debug_buf_len, g_debug_buf.data);
		}
	}

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] debug_buf_len: %u; debug_flags: 0x%08X, wake_up_flag: %u\n",
		__func__, debug_buf_len, g_debug_flags, wake_up_flag);
}

#define MAX_WRITE_LEN 300
static ssize_t dpmaif_debug_write(struct file *fp, const char __user *buf,
	size_t size, loff_t *ppos)
{
	char str[MAX_WRITE_LEN] = {0};
	int ret;

	if (size > MAX_WRITE_LEN)
		return 0;

	ret = copy_from_user(str, buf, size);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_from_user() fail; size: %lu(%d)\n",
			__func__, size, ret);
		return 0;
	}
	str[MAX_WRITE_LEN-1] = '\0';
	dpmaif_sysfs_parse(str, size);

	return size;
}

static unsigned int dpmaif_debug_poll(struct file *fp, struct poll_table_struct *poll)
{
	poll_wait(fp, &g_debug_buf.dbg_wq, poll);

	if (get_ringbuf_used_cnt(g_debug_buf_len, g_debug_buf.rd, g_debug_buf.wr))
		return (POLLIN | POLLRDNORM);

	return 0;
}

static int dpmaif_debug_open(struct inode *inode, struct file *file)
{
	if (atomic_inc_return(&g_debug_buf.dbg_user_cnt) > 1) {
		atomic_set(&g_debug_buf.dbg_user_cnt, 1);
		return -EBUSY;
	}

	CCCI_ERROR_LOG(-1, TAG, "[%s] name: %s\n", __func__, current->comm);
	return 0;
}

static int dpmaif_debug_close(struct inode *inode, struct file *file)
{
	if (atomic_dec_return(&g_debug_buf.dbg_user_cnt) < 0)
		atomic_set(&g_debug_buf.dbg_user_cnt, 0);

	g_debug_flags = 0;
	CCCI_ERROR_LOG(-1, TAG, "[%s] name: %s\n", __func__, current->comm);
	return 0;
}

static void dpmaif_md_ee_cb(void)
{
	wake_up_all(&g_debug_buf.dbg_wq);
}

static const struct proc_ops g_dpmaif_debug_fops = {
	.proc_read    = dpmaif_debug_read,
	.proc_write   = dpmaif_debug_write,
	.proc_poll    = dpmaif_debug_poll,
	.proc_open    = dpmaif_debug_open,
	.proc_release = dpmaif_debug_close,

};

void dpmaif_debug_init(void)
{
	struct proc_dir_entry *dpmaif_debug_proc;

	g_debug_buf_len = 0;
	g_debug_flags   = 0;

	atomic_set(&g_debug_buf.dbg_user_cnt, 0);
	g_debug_buf.data = NULL;
	g_debug_buf.rd   = 0;
	g_debug_buf.wr   = 0;
	g_debug_buf.pre_call_wq = 0;

	dpmaif_debug_proc = proc_create("dpmaif_debug", 0664, NULL, &g_dpmaif_debug_fops);
	if (dpmaif_debug_proc == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] error: proc_create fail.\n", __func__);
		return;
	}

	spin_lock_init(&g_debug_buf.dbg_lock);
	init_waitqueue_head(&g_debug_buf.dbg_wq);

	ccci_set_dpmaif_debug_cb(&dpmaif_md_ee_cb);
}
