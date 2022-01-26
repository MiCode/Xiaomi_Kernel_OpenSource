/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/memory.h>



/* define /sys/board_id/board_id_name */
static int board_id_name_show(struct seq_file *m, void *v)
//static ssize_t board_id_name_show(struct kobject *kobj,struct kobj_attribute *attr, char *buf)
{
	char board_id[15];
	char *br_ptr;
	char *br_ptr_e;

	memset(board_id, 0x0, 15);
	br_ptr = strstr(saved_command_line,"board_id=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");

		if (br_ptr_e != 0) {
			strncpy(board_id, br_ptr + 9, br_ptr_e - br_ptr - 9);
			board_id[br_ptr_e - br_ptr - 9] = '\0';
		}

	//	return snprintf(m, sizeof(board_id), "%s\n", board_id);
		seq_printf(m, "%s\n", board_id);
		return 0;
	} else
		return 0;

}



static int board_id_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, board_id_name_show, NULL);
}

static const struct file_operations board_id_proc_fops = {
	.owner      = THIS_MODULE,
	.open		= board_id_proc_open,
	.read		= seq_read,
};

static int __init proc_board_id_init(void)
{
	proc_create("board_id", 0664, NULL, &board_id_proc_fops);
	return 0;
}
fs_initcall(proc_board_id_init);
