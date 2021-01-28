// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include "mt-plat/mtk_ccci_common.h"
#include <linux/rtc.h>
#include "ccci_util_log.h"

/******************************************************************************/
/* Ring buffer part, this type log is block read, used for temp debug purpose */
/******************************************************************************/
#define CCCI_LOG_BUF_SIZE 4096	/* must be power of 2 */
#define CCCI_LOG_MAX_WRITE 512

/*extern u64 local_clock(void); */

struct ccci_ring_buffer {
	void *buffer;
	unsigned int size;
	unsigned int read_pos;
	unsigned int write_pos;
	unsigned int ch_num;
	atomic_t reader_cnt;
	wait_queue_head_t log_wq;
	spinlock_t write_lock;
};

struct ccci_ring_buffer ccci_log_buf;

int ccci_log_write(const char *fmt, ...)
{
	va_list args;
	int write_len, first_half;
	unsigned long flags;
	char *temp_log;
	int this_cpu;
	char state = irqs_disabled() ? '-' : ' ';
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec = do_div(ts_nsec, 1000000000);

	if (unlikely(ccci_log_buf.buffer == NULL))
		return -ENODEV;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL) {
		/*pr_notice("[ccci0/util]alloc local buff fail p01\n");*/
		return -ENODEV;
	}

	preempt_disable();
	this_cpu = smp_processor_id();
	preempt_enable();
	write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
						"[%5lu.%06lu]%c(%x)[%d:%s]",
						(unsigned long)ts_nsec,
						rem_nsec / 1000,
						state,
						this_cpu,
						current->pid,
						current->comm);
	if (write_len < 0) {
		pr_notice("%s-%d:snprintf fail,write_len = %d\n",
			__func__, __LINE__, write_len);
		write_len = 0;
	} else if (write_len >= CCCI_LOG_MAX_WRITE)
		write_len = CCCI_LOG_MAX_WRITE - 1;

	va_start(args, fmt);
	write_len +=
		vsnprintf(temp_log + write_len,
			CCCI_LOG_MAX_WRITE - write_len,
			fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	if (ccci_log_buf.write_pos + write_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - ccci_log_buf.write_pos;
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, first_half);
		memcpy(ccci_log_buf.buffer, temp_log + first_half,
				write_len - first_half);
	} else {
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, write_len);
	}
	ccci_log_buf.write_pos = (ccci_log_buf.write_pos + write_len)
		& (CCCI_LOG_BUF_SIZE - 1);
	if (write_len > 0)
		ccci_log_buf.ch_num += (unsigned int)write_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	wake_up_all(&ccci_log_buf.log_wq);

	kfree(temp_log);
	temp_log = NULL;

	return write_len;
}

int ccci_log_write_raw(unsigned int set_flags, const char *fmt, ...)
{
	va_list args;
	int write_len, first_half;
	unsigned long flags;
	char *temp_log;
	int this_cpu;
	char state;
	u64 ts_nsec;
	unsigned long rem_nsec;

	if (unlikely(ccci_log_buf.buffer == NULL))
		return -ENODEV;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL) {
		/*pr_notice("[ccci0/util]alloc local buff fail p1\n");*/
		return -ENODEV;
	}

	if (set_flags & CCCI_DUMP_TIME_FLAG) {
		state = irqs_disabled() ? '-' : ' ';
		ts_nsec = local_clock();
		rem_nsec = do_div(ts_nsec, 1000000000);
		preempt_disable();
		this_cpu = smp_processor_id();
		preempt_enable();
		write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
					"[%5lu.%06lu]%c(%x)",
					(unsigned long)ts_nsec,
					rem_nsec / 1000, state,
					this_cpu);
		if (write_len < 0) {
			pr_notice("%s-%d:snprintf fail,write_len = %d\n",
				__func__, __LINE__, write_len);
			write_len = 0;
		} else if (write_len >= CCCI_LOG_MAX_WRITE)
			write_len = CCCI_LOG_MAX_WRITE - 1;
	} else
		write_len = 0;

	if (set_flags & CCCI_DUMP_CURR_FLAG) {
		write_len += snprintf(temp_log + write_len,
						CCCI_LOG_MAX_WRITE - write_len,
						"[%d:%s]",
						current->pid, current->comm);
	}

	va_start(args, fmt);
	write_len += vsnprintf(temp_log + write_len,
					CCCI_LOG_MAX_WRITE - write_len,
					fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	if (ccci_log_buf.write_pos + write_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - ccci_log_buf.write_pos;
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, first_half);
		memcpy(ccci_log_buf.buffer, temp_log + first_half,
				write_len - first_half);
	} else {
		memcpy(ccci_log_buf.buffer + ccci_log_buf.write_pos,
				temp_log, write_len);
	}
	ccci_log_buf.write_pos = (ccci_log_buf.write_pos + write_len)
		& (CCCI_LOG_BUF_SIZE - 1);
	if (write_len > 0)
		ccci_log_buf.ch_num += (unsigned int)write_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	wake_up_all(&ccci_log_buf.log_wq);

	kfree(temp_log);
	temp_log = NULL;

	return write_len;
}

static ssize_t ccci_log_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned int available, read_len, first_half, read_pos;
	unsigned long flags;
	int ret;

 retry:
	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	available = ccci_log_buf.ch_num;
	if (available >= CCCI_LOG_BUF_SIZE) {
		/* This means over flow, write pos is oldest log */
		available = CCCI_LOG_BUF_SIZE;
		read_pos = ccci_log_buf.write_pos;
	} else
		read_pos = ccci_log_buf.read_pos;

	if (!available) {
		spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			ret = wait_event_interruptible(ccci_log_buf.log_wq,
				ccci_log_buf.ch_num);
			if (ret == -ERESTARTSYS)
				return -EINTR;
			goto retry;
		} else {
			return -EAGAIN;
		}
	}

	read_len = size < available ? size : available;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);

	if (read_pos + read_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE - read_pos;
		ret = copy_to_user(buf,
						ccci_log_buf.buffer + read_pos,
						first_half);
		ret += copy_to_user(buf + first_half, ccci_log_buf.buffer,
			read_len - first_half);
	} else {
		ret = copy_to_user(buf,
						ccci_log_buf.buffer + read_pos,
						read_len);
	}

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	read_len = read_len - ret;
	ccci_log_buf.read_pos = (read_pos + read_len) & (CCCI_LOG_BUF_SIZE - 1);
	ccci_log_buf.ch_num -= read_len;
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	return read_len;
}

unsigned int ccci_log_poll(struct file *fp, struct poll_table_struct *poll)
{
	unsigned int mask = 0;

	poll_wait(fp, &ccci_log_buf.log_wq, poll);
	if (ccci_log_buf.ch_num)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int ccci_log_open(struct inode *inode, struct file *file)
{
	if (atomic_read(&ccci_log_buf.reader_cnt))
		return -EBUSY;
	atomic_inc(&ccci_log_buf.reader_cnt);
	return 0;
}

static int ccci_log_close(struct inode *inode, struct file *file)
{
	atomic_dec(&ccci_log_buf.reader_cnt);
	return 0;
}

static const struct file_operations ccci_log_fops = {
	.read = ccci_log_read,
	.open = ccci_log_open,
	.release = ccci_log_close,
	.poll = ccci_log_poll,
};


/******************************************************************************/
/* Dump buffer part, this type log is NON block read, used for AED dump       */
/******************************************************************************/
#define CCCI_INIT_SETTING_BUF	(4096*2)
#define CCCI_BOOT_UP_BUF		(4096*16)

#ifdef CCCI_LOG_DISABLE
#define CCCI_NORMAL_BUF			(0)
#define CCCI_REPEAT_BUF			(0)
#define CCCI_HISTORY_BUF		(0)
#else
#define CCCI_NORMAL_BUF			(4096*2)
#define CCCI_REPEAT_BUF			(4096*32)
#define CCCI_HISTORY_BUF		(4096*128)
#endif

#define CCCI_REG_DUMP_BUF		(4096*64 * 2)

#define MD3_CCCI_INIT_SETTING_BUF   (4096*2)
#define MD3_CCCI_BOOT_UP_BUF                (4096*16)
#define MD3_CCCI_NORMAL_BUF                 (4096*2)
#define MD3_CCCI_REPEAT_BUF                 (4096*32)
#define MD3_CCCI_REG_DUMP_BUF               (4096*32)
#define MD3_CCCI_HISTORY_BUF                (4096*32)

struct ccci_dump_buffer {
	void *buffer;
	unsigned int buf_size;
	unsigned int data_size;
	unsigned int write_pos;
	unsigned int max_num;
	unsigned int attr;
	spinlock_t lock;
};

struct ccci_user_ctlb {
	unsigned int read_idx[2][CCCI_DUMP_MAX];
	unsigned int sep_cnt1[2][CCCI_DUMP_MAX];
	unsigned int sep_cnt2[2]; /* 1st MD; 2nd MD */
	unsigned int busy;
};
static spinlock_t file_lock;

static struct ccci_dump_buffer init_setting_ctlb[2];
static struct ccci_dump_buffer normal_ctlb[2];
static struct ccci_dump_buffer boot_up_ctlb[2];
static struct ccci_dump_buffer repeat_ctlb[2];
static struct ccci_dump_buffer reg_dump_ctlb[2];
static struct ccci_dump_buffer history_ctlb[2];
static struct ccci_dump_buffer ke_dump_ctlb[2];
static int buff_bind_md_id[5];
static int md_id_bind_buf_id[5];
static int buff_en_bit_map;
static char sep_buf[64];
static char md_sep_buf[64];

struct buffer_node {
	struct ccci_dump_buffer *ctlb_ptr;
	unsigned int init_size;
	unsigned int init_attr;
	unsigned int index;
};

/* local attribute */
#define CCCI_DUMP_ATTR_BUSY	(1<<0)
#define CCCI_DUMP_ATTR_RING	(1<<1)

static int get_plat_capbility(int md_id)
{
	int en_flag = 0;

	/* MD1 */
	/* Fix me, may design more better solution to reduce memory usage */
	en_flag |= (1<<0);

	/* MD3 */
	en_flag |= (1<<2);

	return (en_flag & (1<<md_id));
}

static struct buffer_node node_array[2][CCCI_DUMP_MAX+1] = {
	{
		{&init_setting_ctlb[0], CCCI_INIT_SETTING_BUF,
		0, CCCI_DUMP_INIT},
		{&boot_up_ctlb[0], CCCI_BOOT_UP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_BOOTUP},
		{&normal_ctlb[0], CCCI_NORMAL_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_NORMAL},
		{&repeat_ctlb[0], CCCI_REPEAT_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REPEAT},
		{&reg_dump_ctlb[0], CCCI_REG_DUMP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_MEM_DUMP},
		{&history_ctlb[0], CCCI_HISTORY_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_HISTORY},
		{&ke_dump_ctlb[0], 32*1024,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REGISTER},
	},
	{
		{&init_setting_ctlb[1], MD3_CCCI_INIT_SETTING_BUF,
		0, CCCI_DUMP_INIT},
		{&boot_up_ctlb[1], MD3_CCCI_BOOT_UP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_BOOTUP},
		{&normal_ctlb[1], MD3_CCCI_NORMAL_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_NORMAL},
		{&repeat_ctlb[1], MD3_CCCI_REPEAT_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REPEAT},
		{&reg_dump_ctlb[1], MD3_CCCI_REG_DUMP_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_MEM_DUMP},
		{&history_ctlb[1], MD3_CCCI_HISTORY_BUF,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_HISTORY},
		{&ke_dump_ctlb[1], 1*1024,
		CCCI_DUMP_ATTR_RING, CCCI_DUMP_REGISTER},
	}
};

int ccci_dump_write(int md_id, int buf_type,
	unsigned int flag, const char *fmt, ...)
{
	va_list args;
	unsigned int write_len = 0;
	unsigned long flags;
	char *temp_log;
	int this_cpu;
	char state;
	u64 ts_nsec;
	unsigned long rem_nsec;
	int buf_id;
	int can_be_write;
	int actual_write;
	struct ccci_dump_buffer *ptr;

	/* parameter check */
	if (unlikely(md_id >= MAX_MD_NUM))
		return -1;
	if (unlikely(md_id < 0))
		md_id = 0;
	if (unlikely(buf_type >= CCCI_DUMP_MAX))
		return -2;
	buf_id = buff_bind_md_id[md_id];
	if (buf_id < 0 || buf_id > 2 || buf_type < 0)
		return -3;
	if (unlikely(node_array[buf_id][buf_type].index != buf_type))
		return -4;
	if (unlikely(node_array[buf_id][buf_type].ctlb_ptr->buffer == NULL))
		return -5;

	/* using local ptr */
	ptr = node_array[buf_id][buf_type].ctlb_ptr;

	/* if ring buffer mode, write pointer always can be updated */
	/* if one short mode,
	 * when write pointer equal to buff size,
	 * means full
	 */
	if ((ptr->attr & CCCI_DUMP_ATTR_RING) == 0) {
		if (ptr->write_pos == ptr->buf_size)
			return -6; /* buffer full */
	}

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL) {
		/*pr_notice("[ccci0/util]alloc local buff fail p2\n");*/
		return -7;
	}

	/* prepare time info */
	if (flag&CCCI_DUMP_TIME_FLAG) {
		state = irqs_disabled() ? '-' : ' ';
		ts_nsec = local_clock();
		rem_nsec = do_div(ts_nsec, 1000000000);
		preempt_disable();
		this_cpu = smp_processor_id();
		preempt_enable();
		write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
						"[%5lu.%06lu]%c(%x)[%d:%s]",
						(unsigned long)ts_nsec,
						rem_nsec / 1000, state,
						this_cpu,
						current->pid,
						current->comm);
		if (write_len < 0) {
			pr_notice("%s-%d:snprintf fail,write_len = %d\n",
				__func__, __LINE__, write_len);
			write_len = 0;
		} else if (write_len >= CCCI_LOG_MAX_WRITE)
			write_len = CCCI_LOG_MAX_WRITE - 1;
	}

	va_start(args, fmt);
	write_len += vsnprintf(temp_log + write_len,
					CCCI_LOG_MAX_WRITE - write_len,
					fmt, args);
	va_end(args);

	spin_lock_irqsave(&ptr->lock, flags);
	if (flag&CCCI_DUMP_CLR_BUF_FLAG) {
		ptr->data_size = 0;
		ptr->write_pos = 0;
	}

	if ((ptr->attr & CCCI_DUMP_ATTR_RING) == 0) { /* One short mode */
		if (ptr->write_pos + write_len > ptr->buf_size) {
			can_be_write = ptr->buf_size - ptr->write_pos;
			memcpy(ptr->buffer + ptr->write_pos,
					temp_log,
					can_be_write);
			actual_write = can_be_write;
		} else {
			memcpy(ptr->buffer + ptr->write_pos,
					temp_log,
					write_len);
			actual_write = write_len;
		}
		ptr->write_pos = ptr->write_pos + actual_write;
	} else { /* Ring buffer mode */
		if (ptr->write_pos + write_len > ptr->buf_size) {
			can_be_write = ptr->buf_size - ptr->write_pos;
			memcpy(ptr->buffer + ptr->write_pos,
					temp_log,
					can_be_write);
			memcpy(ptr->buffer,
					temp_log + can_be_write,
					write_len - can_be_write);
		} else {
			memcpy(ptr->buffer + ptr->write_pos,
					temp_log,
					write_len);
		}
		actual_write = write_len;
		ptr->data_size += actual_write;
		if (ptr->data_size > ptr->buf_size)
			ptr->data_size = ptr->buf_size + 1;

		ptr->write_pos = (ptr->write_pos + actual_write)
							&(ptr->buf_size-1);
	}
	if (ptr->write_pos > ptr->max_num)
		ptr->max_num = ptr->write_pos;
	spin_unlock_irqrestore(&ptr->lock, flags);

	/* pr_notice("[ccci0/util][ccci_log] has write %d, %d\n",
	 *		write_len, ptr->write_pos);
	 */
	kfree(temp_log);

	return write_len;
}
EXPORT_SYMBOL(ccci_dump_write);

static void format_separate_str(char str[], int type)
{
	int i, j;
	char *sep_str;

	switch (type) {
	case CCCI_DUMP_INIT:
		sep_str = "[0]INIT LOG REGION";
		break;
	case CCCI_DUMP_NORMAL:
		sep_str = "[0]NORMAL LOG REGION";
		break;
	case CCCI_DUMP_BOOTUP:
		sep_str = "[0]BOOT LOG REGION";
		break;
	case CCCI_DUMP_REPEAT:
		sep_str = "[0]REPEAT LOG REGION";
		break;
	case CCCI_DUMP_MEM_DUMP:
		sep_str = "[0]MEM DUMP LOG REGION";
		break;
	case CCCI_DUMP_HISTORY:
		sep_str = "[0]HISTORY LOG REGION";
		break;
	case CCCI_DUMP_REGISTER:
		sep_str = "[0]REGISTER LOG REGION";
		break;
	default:
		sep_str = "[0]Unsupport REGION";
		break;
	}

	j = 0;
	for (i = 8; i < (strlen(sep_str) + 8); i++)
		str[i] = sep_str[j++];

	for (; i < (64-1); i++) {
		if (str[i] != '_')
			str[i] = '_';
		else
			break;
	}
}

static ssize_t ccci_dump_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	unsigned int available, read_len;
	int ret;
	int i;
	int has_read = 0;
	int left = size;
	int read_pos;
	int has_closed;
	int index;
	int curr;
	unsigned long flags;
	struct ccci_dump_buffer *ptr;
	struct ccci_user_ctlb *user_info;
	struct buffer_node *node_ptr;

	/* This make sure avoid read when close */
	spin_lock_irqsave(&file_lock, flags);
	user_info = (struct ccci_user_ctlb *)file->private_data;
	if (user_info == NULL) {
		has_closed = 1;
	} else {
		has_closed = 0;
		user_info->busy = 1;
	}
	spin_unlock_irqrestore(&file_lock, flags);

	if (has_closed)
		return 0;

	for (i = 0; i < 2; i++) {
		if (!(buff_en_bit_map & (1<<i)))
			continue;

		md_sep_buf[13] = '0' + i;
		/* dump data begin */
		node_ptr = &node_array[i][0];

		/* insert separator "===" to buf */
		curr = user_info->sep_cnt2[i];
		if (curr < 64) {
			available = 64 - curr;
			read_len = left < available ? left : available;
			if (read_len == 0)
				goto _out;
			ret = copy_to_user(&buf[has_read],
					&md_sep_buf[curr], read_len);
			if (ret == 0) {
				has_read += read_len;
				left -= read_len;
				user_info->sep_cnt2[i] += read_len;
			} else
				pr_notice("[ccci0/util]dump copy to ser fail%d[-1]\n",
					ret);
		}

		while (node_ptr->ctlb_ptr != NULL) {
			ptr = node_ptr->ctlb_ptr;
			index = node_ptr->index;
			node_ptr++;

			format_separate_str(sep_buf, index);
			/*set log title md id */
			sep_buf[9] = '0' + md_id_bind_buf_id[i];
			/* insert region separator "___" to buf */
			curr = user_info->sep_cnt1[i][index];
			if (curr < 64) {
				available = 64 - curr;
				read_len = left < available ? left : available;
				if (read_len == 0)
					goto _out;
				ret = copy_to_user(
						&buf[has_read],
						&sep_buf[curr],
						read_len);
				if (ret == 0) {
					has_read += read_len;
					left -= read_len;
					user_info->sep_cnt1[i][index]
						+= read_len;
				} else
					pr_notice(
					"[ccci0/util]dump copy to ser fail%d[-2]\n",
						ret);
			}

			/* insert region data */
			/* One short read */
			if ((ptr->attr & CCCI_DUMP_ATTR_RING) == 0) {
				read_pos = user_info->read_idx[i][index];
				available = ptr->write_pos - read_pos;
				if (available == 0)
					continue;
				read_len = left < available ? left : available;
				if (read_len == 0)
					goto _out;
				ret = copy_to_user(
						&buf[has_read],
						ptr->buffer + read_pos,
						read_len);
				if (ret == 0) {
					has_read += read_len;
					left -= read_len;
					user_info->read_idx[i][index]
						+= read_len;
				} else
					pr_notice(
					"[ccci0/util]dump copy to ser fail%d\n",
						ret);
			} else { /* ring buffer read */
				if (ptr->data_size > ptr->buf_size) {
					read_pos = ptr->write_pos
						+ user_info->read_idx[i][index];
					read_pos &= ptr->buf_size - 1;
				} else {
					read_pos =
						user_info->read_idx[i][index];
				}
				available = ptr->data_size
					- user_info->read_idx[i][index];
				if (available == 0)
					continue;
				read_len = left < available ? left : available;
				if (read_len == 0)
					goto _out;
				available = read_len;
				if (read_pos + available > ptr->buf_size) {
					read_len = ptr->buf_size - read_pos;
					ret = copy_to_user(&buf[has_read],
							ptr->buffer + read_pos,
							read_len);
					if (ret == 0) {
						has_read += read_len;
						left -= read_len;
						user_info->read_idx[i][index]
							+= read_len;
					} else
						pr_notice("[ccci0/util]dump copy to ser fail%d[1]\n",
									ret);
					ret = copy_to_user(&buf[has_read],
						ptr->buffer,
						available - read_len);
					if (ret == 0) {
						has_read +=
							available - read_len;
						left -= available - read_len;
						user_info->read_idx[i][index]
							+= available - read_len;
					} else
						pr_notice("[ccci0/util]dump copy to ser fail%d[2]\n",
									ret);
				} else {
					ret = copy_to_user(&buf[has_read],
							ptr->buffer + read_pos,
							available);
					if (ret == 0) {
						has_read += available;
						left -= available;
						user_info->read_idx[i][index]
							+= available;
					} else
						pr_notice("[ccci0/util]dump copy to ser fail%d[3]\n",
									ret);
				}
			}
		}
	}

_out:
	spin_lock_irqsave(&file_lock, flags);
	user_info->busy = 0;
	spin_unlock_irqrestore(&file_lock, flags);

	return (ssize_t)has_read;
}

unsigned int ccci_dump_poll(struct file *fp, struct poll_table_struct *poll)
{
	return POLLIN | POLLRDNORM;
}

static int ccci_dump_open(struct inode *inode, struct file *file)
{
	struct ccci_user_ctlb *user_info;

	user_info = kzalloc(sizeof(struct ccci_user_ctlb), GFP_KERNEL);
	if (user_info == NULL) {
		/*pr_notice("[ccci0/util]fail to alloc memory for ctlb\n"); */
		return -1;
	}

	file->private_data = user_info;
	user_info->busy = 0;
	nonseekable_open(inode, file);
	return 0;
}

static int ccci_dump_close(struct inode *inode, struct file *file)
{
	struct ccci_user_ctlb *user_info;
	unsigned long flags;
	int need_wait = 0;

	user_info = (struct ccci_user_ctlb *)file->private_data;
	if (user_info == NULL)
		return -1;

	do {
		spin_lock_irqsave(&file_lock, flags);
		if (user_info->busy) {
			need_wait = 1;
		} else {
			need_wait = 0;
			file->private_data = NULL;
		}
		spin_unlock_irqrestore(&file_lock, flags);
		if (need_wait)
			msleep(20);
	} while (need_wait);

	if (user_info != NULL)
		kfree(user_info);

	return 0;
}

static const struct file_operations ccci_dump_fops = {
	.read = ccci_dump_read,
	.open = ccci_dump_open,
	.release = ccci_dump_close,
	.poll = ccci_dump_poll,
};

static void ccci_dump_buffer_init(void)
{
	int i = 0;
	int j = 0;
	struct proc_dir_entry *ccci_dump_proc;
	struct buffer_node *node_ptr;
	struct ccci_dump_buffer *ptr;

	ccci_dump_proc = proc_create("ccci_dump", 0440, NULL, &ccci_dump_fops);
	if (ccci_dump_proc == NULL) {
		pr_notice("[ccci0/util]fail to create proc entry for dump\n");
		return;
	}

	spin_lock_init(&file_lock);

	for (i = 1; i < (64-1); i++) {
		sep_buf[i] = '_';
		md_sep_buf[i] = '=';
	}
	sep_buf[i] = '\n';
	md_sep_buf[i] = '\n';
	sep_buf[0] = '\n';
	md_sep_buf[0] = '\n';
	md_sep_buf[8] = ' ';
	md_sep_buf[9] = 'B';
	md_sep_buf[10] = 'U';
	md_sep_buf[11] = 'F';
	md_sep_buf[12] = 'F';
	md_sep_buf[14] = ' ';

	for (i = 0; i < 5; i++) {
		buff_bind_md_id[i] = -1;
		if (j >= 2)
			continue;
		if (get_plat_capbility(MD_SYS1 + i)) {
			buff_bind_md_id[MD_SYS1 + i] = j;
			buff_en_bit_map |= 1<<j;
			md_id_bind_buf_id[j] = MD_SYS1 + i + 1;
			j++;
		}
	}

	for (i = 0; i < 2; i++) {
		node_ptr = &node_array[i][0];
		while (node_ptr->ctlb_ptr != NULL) {
			ptr = node_ptr->ctlb_ptr;
			spin_lock_init(&ptr->lock);
			if (buff_en_bit_map & (1<<i) && node_ptr->init_size) {
				/* allocate buffer */
				ptr->buffer = kmalloc(node_ptr->init_size,
						GFP_KERNEL);
				if (ptr->buffer != NULL) {
					ptr->buf_size = node_ptr->init_size;
					ptr->attr = node_ptr->init_attr;
				} else
					pr_notice("[ccci0/util]fail to allocate buff index %d\n",
						node_ptr->index);
			}
			node_ptr++;
		}
	}
}

/* functions will be called by external */
int get_dump_buf_usage(char buf[], int size)
{
	int ret = 0;
	int i;

	for (i = 0; i < 2; i++) {
		ret += snprintf(&buf[ret], size - ret,
					"For dump buf [%d]\n", i);
		ret += snprintf(&buf[ret], size - ret, "  init:%d\n",
					init_setting_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret, "  bootup:%d\n",
					boot_up_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret, "  normal:%d\n",
					normal_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret, "  repeat:%d\n",
					repeat_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret,
					"  reg_dump:%d\n",
					reg_dump_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret, "  history:%d\n",
					history_ctlb[i].max_num);
		ret += snprintf(&buf[ret], size - ret, "  register:%d\n",
					ke_dump_ctlb[i].max_num);
	}

	return ret;
}

void ccci_util_mem_dump(int md_id, int buf_type, void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _16_fix_num = len / 16;
	int tail_num = len % 16;
	char buf[16];
	int i, j;

	if (curr_p == NULL) {
		ccci_dump_write(md_id, buf_type, 0, "start_addr <NULL>\n");
		return;
	}
	if (len == 0) {
		ccci_dump_write(md_id, buf_type, 0, "len [0]\n");
		return;
	}

	ccci_dump_write(md_id, buf_type, 0, "Base: %p\n", start_addr);
	/* Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		ccci_dump_write(md_id, buf_type, 0,
				"%03X: %08X %08X %08X %08X\n",
				i * 16, *curr_p, *(curr_p + 1),
				*(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		ccci_dump_write(md_id, buf_type, 0,
				"%03X: %08X %08X %08X %08X\n",
				i * 16, *curr_p, *(curr_p + 1),
				*(curr_p + 2), *(curr_p + 3));
	}
}
EXPORT_SYMBOL(ccci_util_mem_dump);

void ccci_util_cmpt_mem_dump(int md_id, int buf_type,
	void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _64_fix_num = len / 64;
	int tail_num = len % 64;
	char buf[64];
	int i, j;

	if (curr_p == NULL) {
		ccci_dump_write(md_id, buf_type, 0, "start_addr <NULL>\n");
		return;
	}
	if (len == 0) {
		ccci_dump_write(md_id, buf_type, 0, "len [0]\n");
		return;
	}

	/* Fix section */
	for (i = 0; i < _64_fix_num; i++) {
		ccci_dump_write(md_id, buf_type, 0,
			"%03X: %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
			i * 64,
			*curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3),
			*(curr_p + 4), *(curr_p + 5),
			*(curr_p + 6), *(curr_p + 7),
			*(curr_p + 8), *(curr_p + 9),
			*(curr_p + 10), *(curr_p + 11),
			*(curr_p + 12), *(curr_p + 13),
			*(curr_p + 14), *(curr_p + 15));
			curr_p += 64/4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 64; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		ccci_dump_write(md_id, buf_type, 0,
			"%03X: %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
			i * 64,
			*curr_p, *(curr_p + 1),
			*(curr_p + 2), *(curr_p + 3),
			*(curr_p + 4), *(curr_p + 5),
			*(curr_p + 6), *(curr_p + 7),
			*(curr_p + 8), *(curr_p + 9),
			*(curr_p + 10), *(curr_p + 11),
			*(curr_p + 12), *(curr_p + 13),
			*(curr_p + 14), *(curr_p + 15));
	}
}

/******************************************************************************/
/* Ring buffer part, this type log is block read, used for temp debug purpose */
/******************************************************************************/
#define CCCI_EVENT_BUF_SIZE		(4096)

struct ccci_event_buf_t {
	void *buffer;
	unsigned int buf_size;
	unsigned int data_size;
	unsigned int write_pos;
	spinlock_t lock;
};

static struct ccci_event_buf_t ccci_event_buffer;

static void ccci_event_buffer_init(void)
{
	spin_lock_init(&ccci_event_buffer.lock);
	ccci_event_buffer.buffer = vmalloc(CCCI_EVENT_BUF_SIZE);
	ccci_event_buffer.buf_size = CCCI_EVENT_BUF_SIZE;
	ccci_event_buffer.data_size = 0;
	ccci_event_buffer.write_pos = 0;
}

int ccci_event_log(const char *fmt, ...)
{
	va_list args;
	unsigned int write_len = 0;
	char *temp_log;
	int this_cpu;
	char state;
	u64 ts_nsec;
	unsigned long rem_nsec;
	unsigned long flags;
	unsigned int wr_pose;
	int can_be_write;
	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct timeval tv_android = { 0 };
	struct rtc_time tm_android;

	if (ccci_event_buffer.buffer == NULL)
		return 0;

	temp_log = kmalloc(CCCI_LOG_MAX_WRITE, GFP_ATOMIC);
	if (temp_log == NULL)
		return 0;

	/* prepare kernel time info */
	state = irqs_disabled() ? '-' : ' ';
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	preempt_disable();
	this_cpu = smp_processor_id();
	preempt_enable();

	/* prepare andorid time info */
	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);

	write_len = snprintf(temp_log, CCCI_LOG_MAX_WRITE,
			"%d%02d%02d-%02d:%02d:%02d.%03d [%5lu.%06lu]%c(%x)[%d:%s]",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm_android.tm_hour,
			tm_android.tm_min,
			tm_android.tm_sec,
			(unsigned int)tv_android.tv_usec,
			(unsigned long)ts_nsec,
			rem_nsec / 1000,
			state,
			this_cpu,
			current->pid,
			current->comm);
	if (write_len < 0) {
		pr_notice("%s-%d:snprintf fail,write_len = %d\n",
			__func__, __LINE__, write_len);
		write_len = 0;
	} else if (write_len >= CCCI_LOG_MAX_WRITE)
		write_len = CCCI_LOG_MAX_WRITE - 1;

	va_start(args, fmt);
	write_len += vsnprintf(temp_log
					+ write_len,
					CCCI_LOG_MAX_WRITE - write_len,
					fmt, args);
	va_end(args);

	spin_lock_irqsave(&ccci_event_buffer.lock, flags);

	wr_pose = ccci_event_buffer.write_pos;
	if (wr_pose + write_len > ccci_event_buffer.buf_size) {
		can_be_write = ccci_event_buffer.buf_size - wr_pose;
		memcpy(ccci_event_buffer.buffer + wr_pose,
				temp_log, can_be_write);
		memcpy(ccci_event_buffer.buffer,
				temp_log + can_be_write,
				write_len - can_be_write);
	} else {
		memcpy(ccci_event_buffer.buffer + wr_pose, temp_log, write_len);
	}
	ccci_event_buffer.data_size += write_len;
	if (ccci_event_buffer.data_size > ccci_event_buffer.buf_size)
		ccci_event_buffer.data_size = ccci_event_buffer.buf_size + 1;

	ccci_event_buffer.write_pos =
		(wr_pose + write_len) & (ccci_event_buffer.buf_size-1);

	spin_unlock_irqrestore(&ccci_event_buffer.lock, flags);

	kfree(temp_log);

	return write_len;
}
EXPORT_SYMBOL(ccci_event_log);

int ccci_event_log_cpy(char buf[], int size)
{
	unsigned long flags;
	unsigned int rd_pose;
	int cpy_size;
	int first_half;

	spin_lock_irqsave(&ccci_event_buffer.lock, flags);

	if (ccci_event_buffer.data_size > ccci_event_buffer.buf_size) {
		/* here using BUFFER size */
		cpy_size =
		ccci_event_buffer.buf_size > size
		?
		size
		:
		ccci_event_buffer.buf_size;
		rd_pose = ccci_event_buffer.write_pos;
		first_half = ccci_event_buffer.buf_size - rd_pose;
		memcpy(buf, ccci_event_buffer.buffer + rd_pose, first_half);
		memcpy(&buf[first_half], ccci_event_buffer.buffer,
			ccci_event_buffer.buf_size - first_half);
	} else {
		/* here using DATA size */
		cpy_size =
		ccci_event_buffer.data_size > size
		?
		size
		:
		ccci_event_buffer.data_size;
		memcpy(buf, ccci_event_buffer.buffer, cpy_size);
	}

	spin_unlock_irqrestore(&ccci_event_buffer.lock, flags);

	return cpy_size;
}


void ccci_log_init(void)
{
	struct proc_dir_entry *ccci_log_proc;

	ccci_log_proc = proc_create("ccci_log", 0440, NULL, &ccci_log_fops);
	if (ccci_log_proc == NULL) {
		pr_notice("[ccci0/util]fail to create proc entry for log\n");
		return;
	}
	ccci_log_buf.buffer = kmalloc(CCCI_LOG_BUF_SIZE, GFP_KERNEL);
	spin_lock_init(&ccci_log_buf.write_lock);
	init_waitqueue_head(&ccci_log_buf.log_wq);
	ccci_log_buf.ch_num = 0;
	atomic_set(&ccci_log_buf.reader_cnt, 0);
	ccci_dump_buffer_init();
	ccci_event_buffer_init();
}

void get_ccci_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long data_size = ke_dump_ctlb[0].data_size;

	if (data_size > ke_dump_ctlb[0].buf_size)
		data_size = ke_dump_ctlb[0].buf_size;

	*vaddr = (unsigned long)ke_dump_ctlb[0].buffer;
	*size = data_size;

}
EXPORT_SYMBOL(get_ccci_aee_buffer);

void get_md_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	unsigned long data_size = reg_dump_ctlb[0].data_size;

	if (data_size > reg_dump_ctlb[0].buf_size)
		data_size = reg_dump_ctlb[0].buf_size;

	*vaddr = (unsigned long)reg_dump_ctlb[0].buffer;
	*size = data_size;

}
EXPORT_SYMBOL(get_md_aee_buffer);
