/*
* Copyright (C) 2016 XiaoMi, Inc.
*/
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/delay.h>
#include <sound/apr_audio-v2.h>

static int proc_opalum_temp_show(struct seq_file *m, void *v)
{
	int acc = 0;
	int counter = 0;

	opalum_afe_get_param(1);
	msleep(9);
	opalum_get_temp_values(&acc, &counter);
	seq_printf(m, "%d\n%d\n", acc, counter);
	return 0;
}

static ssize_t proc_opalum_temp_write(struct file *file, const char __user *data, size_t size, loff_t *loff)
{
	opalum_afe_set_param(0);
	return size;
}

static int proc_opalum_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_opalum_temp_show, NULL);
}

static const struct file_operations proc_opalum_operations = {
	.open		= proc_opalum_open,
	.write		= proc_opalum_temp_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_opalum_init(void)
{
	proc_create("opalum-temp-calib", 0, NULL, &proc_opalum_operations);
	return 0;
}
module_init(proc_opalum_init);


