/*
* Copyright (C) 2016 XiaoMi, Inc.
*/
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/delay.h>
#include <sound/apr_audio-v2.h>

static int proc_opalum_f0_show(struct seq_file *m, void *v)
{
	int f0 = 0;
	int ref_diff = 0;

	opalum_afe_get_param(0);
	msleep(9);
	opalum_get_f0_values(&f0, &ref_diff);
	seq_printf(m, "%d\n%d\n", f0, ref_diff);
	return 0;
}

static ssize_t proc_opalum_f0_write(struct file *file, const char __user *data, size_t size, loff_t *loff)
{
	opalum_afe_set_param(0);

	return size;
}

static int proc_opalum_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_opalum_f0_show, NULL);
}

static const struct file_operations proc_opalum_operations = {
	.open		= proc_opalum_open,
	.write		= proc_opalum_f0_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_opalum_init(void)
{
	proc_create("opalum-f0-calib", 0, NULL, &proc_opalum_operations);
	return 0;
}
module_init(proc_opalum_init);


