#include <linux/types.h>
/* #include <linux/errno.h> */
/* #include <linux/time.h> */
/* #include <linux/kernel.h> */
/* #include <linux/module.h> */
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mlog_internal.h"


extern wait_queue_head_t mlog_wait;
extern void mlog_doopen(void);
extern int mlog_unread(void);
extern int mlog_doread(char __user *buf, size_t len);
extern int mlog_show_info(struct seq_file *m, void *v);
extern int mlog_print_fmt(struct seq_file *m);


static int mlog_open(struct inode *inode, struct file *file)
{
	MLOG_PRINTK("[mlog] open %d\n", mlog_unread());
	mlog_doopen();
	return 0;
}

static int mlog_release(struct inode *inode, struct file *file)
{
	MLOG_PRINTK("[mlog] release\n");
	return 0;
}

static ssize_t mlog_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	if (file->f_flags & O_NONBLOCK) {
		if (!mlog_unread())
			return -EAGAIN;
		/* MLOG_PRINTK("[mlog] read (NonBlock) %d\n", count); */
	}
	return mlog_doread(buf, count);
}

static unsigned int mlog_poll(struct file *file, poll_table *wait)
{
	/* MLOG_PRINTK("[mlog] poll\n"); */
	poll_wait(file, &mlog_wait, wait);
	if (mlog_unread())
		return POLLIN | POLLRDNORM;
	return 0;
}

static const struct file_operations proc_mlog_operations = {
	.read = mlog_read,
	.poll = mlog_poll,
	.open = mlog_open,
	.release = mlog_release,
	.llseek = generic_file_llseek,
};

static int mlog_fmt_proc_show(struct seq_file *m, void *v)
{
	return mlog_print_fmt(m);
}

static int mlog_fmt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mlog_fmt_proc_show, NULL);
}

static const struct file_operations mlog_fmt_proc_fops = {
	.open = mlog_fmt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void mlog_init_procfs(void)
{
	proc_create("mlog_fmt", 0, NULL, &mlog_fmt_proc_fops);
	proc_create("mlog", 0, NULL, &proc_mlog_operations);
}
