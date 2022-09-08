// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "tpd.h"
#include "met_ftrace_touch.h"

#ifdef TPD_DEBUG_CODE
int tpd_fail_count;
int tpd_trial_count;

int tpd_debug_nr;
module_param(tpd_debug_nr, int, 0644);
module_param(tpd_fail_count, int, 0644);
module_param(tpd_trial_count, int, 0644);

int tpd_debug_time;
long tpd_last_2_int_time[2] = { 0 };

long tpd_last_down_time;
int tpd_start_profiling;

int tpd_down_status;
struct timeval {
	int		tv_sec;		/* seconds */
	long	tv_usec;	/* microseconds */
};

module_param(tpd_debug_time, int, 0644);

void tpd_debug_set_time(void)
{
	struct timespec64 now;
	struct timeval t;

	ktime_get_real_ts64(&now);

	t.tv_sec = now.tv_sec;
	t.tv_usec = now.tv_nsec/1000;

	if (!tpd_debug_time && !tpd_em_log)
		return;

	tpd_last_2_int_time[0] = tpd_last_2_int_time[1];
	tpd_last_2_int_time[1] = (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;

	/*  */
	/* Start profiling while receive touch DOWN event */
	/* Stop profiling while the first frame is upadted */
	/*  */
	if (!tpd_down_status) {
		tpd_start_profiling = 1;
		tpd_last_down_time = tpd_last_2_int_time[1];
	}
}

#ifdef TPD_DEBUG_TRACK
int tpd_debug_track;
int tpd_debug_track_color;
int tpd_debug_touch_up;
module_param(tpd_debug_track, int, 0644);

void tpd_draw(int x, int y)
{
	UINT16 *buf = (UINT16 *) dal_fb_addr;

	buf = buf + (x + y * TPD_RES_X);
	tpd_debug_track_color += (tpd_debug_track_color >= 31 ? 0 : 1);
	*buf = (0xffe0 + tpd_debug_track_color);
}

void tpd_down_debug_track(int x, int y)
{
	if (tpd_debug_touch_up == 1) {
		DAL_Clean();
		tpd_debug_touch_up = 0;
	}
	LCD_LayerEnable(5, TRUE);
	tpd_draw(x - 1, y - 1);
	tpd_draw(x, y - 1);
	tpd_draw(x + 1, y - 1);
	tpd_draw(x - 1, y);
	tpd_draw(x + 1, y);
	tpd_draw(x - 1, y + 1);
	tpd_draw(x, y + 1);
	tpd_draw(x + 1, y + 1);
}

void tpd_up_debug_track(int x, int y)
{
	if (x == 0 && y == 0) {
		tpd_debug_track_color = 0;
		DAL_Clean();
	}
	tpd_debug_touch_up = 1;

}
#endif				/* TPD_DEBUG_TRACK */


#define BUFFER_SIZE 128

int tpd_em_log;
module_param(tpd_em_log, int, 0664);

void tpd_enable_em_log(int enable)
{
	if (enable)
		tpd_em_log = 1;
	else
		tpd_em_log = 0;
}
EXPORT_SYMBOL(tpd_enable_em_log);


int tpd_em_log_to_fs;
module_param(tpd_em_log_to_fs, int, 0664);

int tpd_em_log_first = 1;

struct tpd_em_log_struct {
	struct list_head list;
	char data[BUFFER_SIZE];
};
static LIST_HEAD(tpd_em_log_list);
int tpd_log_line_buffer = 128;	/* per line 128 bytes */
int tpd_log_line_cnt = 1024 * 10;
module_param(tpd_log_line_buffer, int, 0664);
module_param(tpd_log_line_cnt, int, 0664);


struct tpd_debug_log_buf {
	unsigned int head;
	unsigned int tail;
	spinlock_t buffer_lock;
	unsigned int cnt;
	unsigned char *buffer;

};
struct tpd_debug_log_buf tpd_buf;

static int tpd_debug_log_open(struct inode *inode, struct file *file)
{

	if (tpd_buf.buffer == NULL)
		tpd_buf.buffer =
			vmalloc(tpd_log_line_cnt * tpd_log_line_buffer);
	if (tpd_buf.buffer == NULL) {
		pr_info("tpd_log: nomem for tpd_buf->buffer\n");
		return -ENOMEM;
	}
	spin_lock(&tpd_buf.buffer_lock);
	tpd_buf.head = tpd_buf.tail = 0;
	tpd_buf.cnt = 0;
	spin_unlock(&tpd_buf.buffer_lock);


	file->private_data = &tpd_buf;
	pr_debug("[tpd_em_log]: open log file\n");
	return 0;
}

static int tpd_debug_log_release(struct inode *inode, struct file *file)
{

	unsigned char *tmp_buffer = NULL;

	pr_debug("[tpd_em_log]: close log file\n");
	spin_lock(&tpd_buf.buffer_lock);
	tmp_buffer = tpd_buf.buffer;
	tpd_buf.buffer = NULL;
	spin_unlock(&tpd_buf.buffer_lock);
	if (tmp_buffer)
		vfree(tmp_buffer);
	/* free(tpd_buf); */
	return 0;
}

static ssize_t tpd_debug_log_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t tpd_debug_log_read(struct file *file, char __user *buffer,
				  size_t count, loff_t *ppos)
{
	struct tpd_debug_log_buf *tpd_buf =
		(struct tpd_debug_log_buf *)file->private_data;
	unsigned int retval = 0, unit = tpd_log_line_buffer;
	unsigned char *tmp_buf = NULL;

	if (tpd_buf == NULL)
		return -ENOMEM;


	if (tpd_buf->head == tpd_buf->tail && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;


	while ((count - retval) >= unit) {
		spin_lock_irq(&tpd_buf->buffer_lock);
		if (tpd_buf->head != tpd_buf->tail) {
			tmp_buf = &tpd_buf->buffer[tpd_buf->tail++ * unit];
			tpd_buf->tail &= tpd_log_line_cnt - 1;
		} else {
			spin_unlock_irq(&tpd_buf->buffer_lock);
			break;
		}
		spin_unlock_irq(&tpd_buf->buffer_lock);
		if (copy_to_user(buffer + retval, tmp_buf, unit))
			return -EFAULT;

		retval += unit;

	}

	return retval;


}

static unsigned char *tpd_log_find_buffer(void)
{
	unsigned char *buffer = NULL;
	unsigned int unit = tpd_log_line_buffer;

	if (tpd_buf.buffer == NULL) {
		pr_info("[tpd_em_log] :tpd_buf.buffer is NULL\n");
		return NULL;
	}
	spin_lock(&tpd_buf.buffer_lock);
	buffer = &tpd_buf.buffer[tpd_buf.head++ * unit];
	tpd_buf.head &= tpd_log_line_cnt - 1;
	spin_unlock(&tpd_buf.buffer_lock);
	if (tpd_buf.head == tpd_buf.tail) {
		snprintf(buffer, unit, "[tpd_em_log] overlay !!!!!\n");
		return NULL;
	}
	memset(buffer, 0, unit);
	return buffer;
}

static const struct file_operations tpd_debug_log_fops = {
	.owner = THIS_MODULE,
	.read = tpd_debug_log_read,
	.write = tpd_debug_log_write,
	.open = tpd_debug_log_open,
	.release = tpd_debug_log_release,
};

static struct miscdevice tpd_debug_log_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpd_em_log",
	.fops = &tpd_debug_log_fops,
};
#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
noinline void MET_touch(int raw_x, int raw_y,
			int cal_x, int cal_y, int p, int down)
{
	struct timespec64 now;
	struct timeval t;

	ktime_get_real_ts64(&now);

	t.tv_sec = now.tv_sec;
	t.tv_usec = now.tv_nsec/1000;
	if ((tpd_down_status == 0 && down == 1) ||
		(tpd_down_status == 1 && down == 0)) {
		trace_MET_touch("EV_KEY",
				t.tv_sec, t.tv_usec, "BTN_TOUCH", down);
		tpd_down_status = !tpd_down_status;
	}
	if (tpd_down_status) {
		trace_MET_touch("EV_ABS", t.tv_sec, t.tv_usec, "X", raw_x);
		trace_MET_touch("EV_ABS", t.tv_sec, t.tv_usec, "Y", raw_y);
	}
}

void tpd_em_log_output(int raw_x, int raw_y,
			int cal_x, int cal_y, int p, int down)
{
	if (down == TPD_TYPE_INT_DOWN) {
		pr_debug("[tpd_em_log] int trigger down\n");
	} else if (down == TPD_TYPE_INT_UP) {
		pr_debug("[tpd_em_log] int trigger up\n");
	} else if (down == TPD_TYPE_TIMER) {
		pr_debug("[tpd_em_log] timer trigger\n");
	} else if (down == TPD_TYPE_RAW_DATA) {
		if (tpd_em_log == TPD_TYPE_RAW_DATA) {
			pr_debug("[tpd_em_log] rx=%d,ry=%d,rz1=%d"
				SPLIT "rz2=%d,p=%d,r\n",
			       raw_x, raw_y, cal_x, cal_y, p);
		}
	} else if (down == TPD_TYPE_REJECT1) {
		pr_debug("[tpd_em_log] the first or last point is rejected\n");
	} else if (down == TPD_TYPE_REJECT2) {
		pr_debug
		    ("[tpd_em_log] pressure(%d) > NICE_PRESSURE(%d)"
		    SPLIT "debounce debt0:%d ms, debt1:%d ms, spl_num:%d\n",
		     raw_x, raw_y, cal_x, cal_y, p);
	} else if (down == TPD_TYPE_FIST_LATENCY) {
		pr_debug("[tpd_em_log] The first touch latency is %d ms\n",
			raw_x / 1000);
	} else if (down && tpd_down_status == 0) {
		pr_debug("[tpd_em_log] rx=%d,ry=%d,cx=%d"
			SPLIT "cy=%d,p=%d,d(+%ld ms)\n",
		       raw_x, raw_y, cal_x, cal_y, p,
		       (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	} else if (down && tpd_down_status != 0) {
		pr_debug("[tpd_em_log] rx=%d,ry=%d,cx=%d"
			SPLIT "cy=%d,p=%d,m(+%ld ms)\n",
		       raw_x, raw_y, cal_x, cal_y, p,
		       (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	} else {
		pr_debug("[tpd_em_log] rx=%d,ry=%d,cx=%d"
			SPLIT "cy=%d,p=%d,u(+%ld ms)\n",
		       raw_x, raw_y, cal_x, cal_y, p,
		       (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	}
}

void tpd_em_log_store(int raw_x, int raw_y,
			int cal_x, int cal_y, int p, int down)
{
	/* static struct proc_dir_entry *entry = NULL; */
	/* struct tpd_em_log_struct *tpd_em_log_struct_new; */

	unsigned char *buffer = NULL;
	/* unsigned int unit = tpd_log_line_buffer; */
	struct timespec64 now;
	struct timeval t;
	buffer = tpd_log_find_buffer();
	if (buffer == NULL) {
		pr_info("not buffer\n");
		return;
	}

	ktime_get_real_ts64(&now);

	t.tv_sec = now.tv_sec;
	t.tv_usec = now.tv_nsec/1000;

	if (down == TPD_TYPE_INT_DOWN) {
		snprintf(buffer, tpd_log_line_buffer,
			"[%5lu.%06lu][tpd_em_log] int trigger down\n",
			 (t.tv_sec & 0xFFF), t.tv_usec);
	} else if (down == TPD_TYPE_INT_UP) {
		snprintf(buffer, tpd_log_line_buffer,
			"[%5lu.%06lu][tpd_em_log] int trigger up\n",
			 (t.tv_sec & 0xFFF), t.tv_usec);
	} else if (down == TPD_TYPE_TIMER) {
		snprintf(buffer, tpd_log_line_buffer,
			"[%5lu.%06lu][tpd_em_log] timer trigger\n",
			 (t.tv_sec & 0xFFF), t.tv_usec);
	} else if (down == TPD_TYPE_RAW_DATA) {
		if (tpd_em_log == TPD_TYPE_RAW_DATA) {
			snprintf(buffer, tpd_log_line_buffer,
				 "[%5lu.%06lu][tpd_em_log]"
				 SPLIT "rx=%d,ry=%d,rz1=%d,rz2=%d,p=%d,r\n",
				 (t.tv_sec & 0xFFF), t.tv_usec,
				 raw_x, raw_y, cal_x, cal_y, p);
		}
	} else if (down == TPD_TYPE_REJECT1) {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "the first or last point is rejected\n",
			 (t.tv_sec & 0xFFF), t.tv_usec);
	} else if (down == TPD_TYPE_REJECT2) {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "pressure(%d) > NICE_PRESSURE(%d), "
			 SPLIT "debounce debt0:%d, debt1:%d, spl_num:%d\n",
			 (t.tv_sec & 0xFFF), t.tv_usec,
			 raw_x, raw_y, cal_x, cal_y, p);
	} else if (down == TPD_TYPE_FIST_LATENCY) {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "The first touch latency is %d ms\n",
			 (t.tv_sec & 0xFFF), t.tv_usec, raw_x / 1000);
	} else if (down && tpd_down_status == 0) {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "rx=%d,ry=%d,cx=%d,cy=%d,p=%d,d(+%ld ms)\n",
			 (t.tv_sec & 0xFFF), t.tv_usec,
			 raw_x, raw_y, cal_x, cal_y, p,
			 (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	} else if (down && tpd_down_status != 0) {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "rx=%d,ry=%d,cx=%d,cy=%d,p=%d,d(+%ld ms)\n",
			 (t.tv_sec & 0xFFF), t.tv_usec,
			 raw_x, raw_y, cal_x, cal_y, p,
			 (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	} else {
		snprintf(buffer, tpd_log_line_buffer,
			 "[%5lu.%06lu][tpd_em_log]"
			 SPLIT "rx=%d,ry=%d,cx=%d,cy=%d,p=%d,u(+%ld ms)\n",
			 (t.tv_sec & 0xFFF), t.tv_usec,
			 raw_x, raw_y, cal_x, cal_y, p,
			 (tpd_last_2_int_time[1] -
				tpd_last_2_int_time[0]) / 1000);
	}

	/* list_add_tail(&tpd_em_log_struct_new->list, &tpd_em_log_list); */
}

void tpd_em_log_release(void)
{
	struct tpd_em_log_struct *p, *p_tmp;

	if (!tpd_em_log_first) {
		remove_proc_entry("tpd_em_log", NULL);

		list_for_each_entry_safe(p, p_tmp, &tpd_em_log_list, list) {
			list_del(&p->list);
			kfree(p);
		}

		tpd_em_log_first = 1;
		tpd_em_log_to_fs = 0;
	}
}

int tpd_log_init(void)
{
	if (misc_register(&tpd_debug_log_dev) < 0) {
		pr_info("[tpd_em_log] :register device failed\n");
		return -1;
	}
	pr_info("[tpd_em_log] :register device successfully\n");
	spin_lock_init(&tpd_buf.buffer_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(tpd_log_init);

void tpd_log_exit(void)
{
	misc_deregister(&tpd_debug_log_dev);
}
EXPORT_SYMBOL_GPL(tpd_log_exit);

int tpd_debuglog;
module_param(tpd_debuglog, int, 0664);
#endif				/* TPD_DEBUG_CODE */
