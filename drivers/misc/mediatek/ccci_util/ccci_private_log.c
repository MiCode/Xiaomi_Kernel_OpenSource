#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <mach/mt_ccci_common.h>

#include "ccci_util_log.h"

#define CCCI_LOG_BUF_SIZE 4096  // must be power of 2
#define CCCI_LOG_MAX_WRITE 512

//extern u64 local_clock(void);

struct ccci_ring_buffer {
	void *buffer;
	unsigned int size;
	unsigned int read_pos;
	unsigned int write_pos;
	atomic_t last_ops; // 0 for write; 1 for read
	atomic_t reader_cnt;
	wait_queue_head_t log_wq;
	spinlock_t write_lock;
};

struct ccci_ring_buffer ccci_log_buf;

int ccci_log_write(const char *fmt, ...)
{
	va_list args;
	int write_len,first_half;
	unsigned long flags;
	char temp_log[CCCI_LOG_MAX_WRITE];
	int this_cpu;
	char state = irqs_disabled()?'-':' ';
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec = do_div(ts_nsec, 1000000000);
	
	if(unlikely(ccci_log_buf.buffer == NULL))
		return -ENODEV;

	preempt_disable();
	this_cpu = smp_processor_id();
	preempt_enable();
	write_len = snprintf(temp_log, sizeof(temp_log), "[%5lu.%06lu]%c(%x)[%d:%s]",
				  (unsigned long)ts_nsec, rem_nsec/1000, state, this_cpu,
				  current->pid, current->comm); 

	va_start(args, fmt);
	write_len += vsnprintf(temp_log+write_len, sizeof(temp_log)-write_len, fmt, args);
	va_end(args);

	//printk("[cclog]write %d\n", write_len);

	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	if(ccci_log_buf.write_pos+write_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE-ccci_log_buf.write_pos;
		memcpy(ccci_log_buf.buffer+ccci_log_buf.write_pos, temp_log, first_half);
		memcpy(ccci_log_buf.buffer, temp_log+first_half, write_len-first_half);
	} else {
		memcpy(ccci_log_buf.buffer+ccci_log_buf.write_pos, temp_log, write_len);
	}
	ccci_log_buf.write_pos = (ccci_log_buf.write_pos+write_len) & (CCCI_LOG_BUF_SIZE-1);
	atomic_set(&ccci_log_buf.last_ops, 0);
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	wake_up_all(&ccci_log_buf.log_wq);
	
	return write_len;
}
EXPORT_SYMBOL(ccci_log_write);

static ssize_t ccci_log_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int available,read_len,first_half;
	unsigned long flags;
	int ret;

retry:
	spin_lock_irqsave(&ccci_log_buf.write_lock, flags);
	available = (ccci_log_buf.write_pos-ccci_log_buf.read_pos) & (CCCI_LOG_BUF_SIZE-1);
	if(available==0 && !atomic_read(&ccci_log_buf.last_ops))
		available = CCCI_LOG_BUF_SIZE;
	
	if(!available) {	
		spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
		if(!(file->f_flags & O_NONBLOCK)) {
			ret = wait_event_interruptible(ccci_log_buf.log_wq, !atomic_read(&ccci_log_buf.last_ops));
			if(ret == -ERESTARTSYS)
				return -EINTR;
			else
				goto retry;
		} else {
			return -EAGAIN;
		}
	}

	read_len = size<available?size:available;
	//printk("[cclog]read %d\n", read_len);
	if(ccci_log_buf.read_pos+read_len > CCCI_LOG_BUF_SIZE) {
		first_half = CCCI_LOG_BUF_SIZE-ccci_log_buf.read_pos;
		ret = copy_to_user(buf, ccci_log_buf.buffer+ccci_log_buf.read_pos, first_half);
		ret = copy_to_user(buf+first_half, ccci_log_buf.buffer, read_len-first_half);
	} else {
		ret = copy_to_user(buf, ccci_log_buf.buffer+ccci_log_buf.read_pos, read_len);
	}
	ccci_log_buf.read_pos = (ccci_log_buf.read_pos+read_len) & (CCCI_LOG_BUF_SIZE-1);
	atomic_set(&ccci_log_buf.last_ops, 1);
	spin_unlock_irqrestore(&ccci_log_buf.write_lock, flags);
	return read_len;
}

unsigned int ccci_log_poll(struct file *fp, struct poll_table_struct *poll)
{
	unsigned int mask = 0;
	
	poll_wait(fp, &ccci_log_buf.log_wq, poll);
	if(!atomic_read(&ccci_log_buf.last_ops))
		mask |= POLLIN|POLLRDNORM;

	return mask;
}

static int ccci_log_open(struct inode *inode, struct file *file)
{
	if(atomic_read(&ccci_log_buf.reader_cnt))
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

void ccci_log_init(void)
{
	struct proc_dir_entry *ccci_log_proc;
	ccci_log_proc = proc_create("ccci_log", 0664, NULL, &ccci_log_fops);
	if(ccci_log_proc == NULL) {
		CCCI_UTIL_INF_MSG("fail to create proc entry for log\n");
		return;
	}
	ccci_log_buf.buffer = kmalloc(CCCI_LOG_BUF_SIZE, GFP_KERNEL);
	spin_lock_init(&ccci_log_buf.write_lock);
	init_waitqueue_head(&ccci_log_buf.log_wq);
	atomic_set(&ccci_log_buf.last_ops, 1);
	atomic_set(&ccci_log_buf.reader_cnt, 0);
}

