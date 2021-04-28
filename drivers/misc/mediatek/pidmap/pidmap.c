/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifdef CONFIG_MTK_PID_MAP

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <mt-plat/mtk_pidmap.h>

/*
 * The best way favor performance to know task name is
 * keeping pid and task name mapping in a one-way table.
 *
 * Other options will suffer performance, for example,
 * using find_task_by_vpid() w/ rcu read lock, or
 * maintaining mapping in an rb-tree.
 *
 * Besides, static memory is chosen for pid map because
 * static memory layout is required for some exception
 * handling flow, e.g., hwt or hw reboot.
 */

static char mtk_pidmap[PIDMAP_AEE_BUF_SIZE];
static int  mtk_pidmap_proc_dump_mode;
static int  mtk_pidmap_max_pid;
static char mtk_pidmap_proc_cmd_buf[PIDMAP_PROC_CMD_BUF_SIZE];
static struct proc_dir_entry *mtk_pidmap_proc_entry;

static void mtk_pidmap_init_map(void)
{
	/*
	 * now pidmap is designed for keeping
	 * maximum 32768 pids in 512 KB buffer.
	 */
	mtk_pidmap_max_pid =
		PIDMAP_AEE_BUF_SIZE / PIDMAP_ENTRY_SIZE;
}

/*
 * mtk_pidmap_update
 * insert or update new mapping between pid and task name.
 *
 * task: current task
 */
void mtk_pidmap_update(struct task_struct *task)
{
	char *name;
	int len;
	pid_t pid;

	if (unlikely(!mtk_pidmap_max_pid))
		mtk_pidmap_init_map();

	pid = task->pid;

	if (unlikely((pid < 1) || (pid > mtk_pidmap_max_pid)))
		return;

	/*
	 * part 1, get current task's name.
	 *
	 * this could be lockless because the specific offset
	 * will be updated by its task only.
	 */
	name = mtk_pidmap + ((pid - 1) * PIDMAP_ENTRY_SIZE);

	/* copy task name */
	memcpy(name, task->comm, PIDMAP_TASKNAME_SIZE);
	*(name + PIDMAP_TASKNAME_SIZE) = '\0';

	/* clear garbage tail chars to help parsers */
	len = strlen(name);
	if (len > PIDMAP_TASKNAME_SIZE)
		len = PIDMAP_TASKNAME_SIZE;

	memset(name + len, 0, PIDMAP_TASKNAME_SIZE - len);

	/* part 2, copy thread group ID as hex format */

	name += PIDMAP_TASKNAME_SIZE;
	*(name + 0) = (char)(task->tgid & 0xFF);
	*(name + 1) = (char)(task->tgid >> 8);
}

static void mtk_pidmap_seq_dump_readable(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i, active_pid;
	pid_t tgid;
	char *name;
	char name_tmp[TASK_COMM_LEN + 1] = {0};

	seq_puts(seq, "<PID Map>\n");
	seq_puts(seq, "PID\tTGID\tTask Name\n");

	/*
	 * pid map shall be protected for dump, however
	 * we ignore locking here to favor performance.
	 */
	active_pid = 0;

	for (i = 0; i < PIDMAP_ENTRY_CNT; i++) {
		name = &mtk_pidmap[i * PIDMAP_ENTRY_SIZE];
		if (name[0]) {
			/* get task name */
			memset(name_tmp, 0, PIDMAP_TASKNAME_SIZE);
			memcpy(name_tmp, name, PIDMAP_TASKNAME_SIZE);

			/* get tgid */
			name += PIDMAP_TASKNAME_SIZE;
			tgid = (pid_t)name[0] +
				((pid_t)name[1] << 8);

			seq_printf(seq, "%d\t%d\t%s\n",
				i + 1, tgid, name_tmp);

			active_pid++;
		}
	}

	seq_puts(seq, "\n<Information>\n");
	seq_printf(seq, "Total PIDs: %d\n", active_pid);
	seq_printf(seq, "Entry size: %d bytes\n",
			PIDMAP_ENTRY_SIZE);
	seq_printf(seq, " - Task name size: %d bytes\n",
			(int)PIDMAP_TASKNAME_SIZE);
	seq_printf(seq, " - TGID size: %d bytes\n",
			(int)PIDMAP_TGID_SIZE);
	seq_printf(seq, "Total Buffer Size: %d bytes\n",
			PIDMAP_AEE_BUF_SIZE + PIDMAP_PROC_CMD_BUF_SIZE);
	seq_printf(seq, "mtk_pidmap address: 0x%p\n",
			&mtk_pidmap[0]);

	seq_puts(seq, "\n<Configuration>\n");
	seq_puts(seq, "echo 0 > pidmap: Dump raw pidmap (default, for AEE DB)\n");
	seq_puts(seq, "echo 1 > pidmap: Dump readable pidmap\n");
	seq_puts(seq, "echo 2 > pidmap: Reset pidmap\n");

}

static void mtk_pidmap_seq_dump_raw(struct seq_file *seq)
{
	int i;

	/*
	 * pid map shall be protected for dump, however
	 * we ignore locking here to favor performance.
	 *
	 * Notice: be aware that you shall NOT read this seq_file
	 * in Windows environment because Windows will automatically
	 * add Carriage Return 0Dh ('\r') while you output 0Ah ('\n')
	 * unexpectedly.
	 *
	 * 0Ah may be existed for TGID, and you'll get an incorrect
	 * PID map outputs.
	 */
	for (i = 0; i < PIDMAP_AEE_BUF_SIZE; i++)
		seq_putc(seq, mtk_pidmap[i]);
}

static int mtk_pidmap_seq_show(struct seq_file *seq, void *v)
{
	if (mtk_pidmap_proc_dump_mode == PIDMAP_PROC_DUMP_READABLE)
		mtk_pidmap_seq_dump_readable(NULL, NULL, seq);
	else if (mtk_pidmap_proc_dump_mode == PIDMAP_PROC_DUMP_RAW)
		mtk_pidmap_seq_dump_raw(seq);

	return 0;
}

void get_pidmap_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	/* retrun start location */
	if (vaddr)
		*vaddr = (unsigned long)mtk_pidmap;

	/* return valid buffer size */
	if (size)
		*size = PIDMAP_AEE_BUF_SIZE;
}
EXPORT_SYMBOL(get_pidmap_aee_buffer);

static ssize_t mtk_pidmap_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	int ret;

	if (count == 0)
		goto err;
	else if (count > PIDMAP_PROC_CMD_BUF_SIZE)
		count = PIDMAP_PROC_CMD_BUF_SIZE;

	ret = copy_from_user(mtk_pidmap_proc_cmd_buf, buf, count);

	if (ret < 0)
		goto err;

	if (mtk_pidmap_proc_cmd_buf[0] == '0') {
		mtk_pidmap_proc_dump_mode = PIDMAP_PROC_DUMP_RAW;
		pr_info("[pidmap] dump mode: raw\n");
	} else if (mtk_pidmap_proc_cmd_buf[0] == '1') {
		mtk_pidmap_proc_dump_mode = PIDMAP_PROC_DUMP_READABLE;
		pr_info("[pidmap] dump mode: readable\n");
	} else if (mtk_pidmap_proc_cmd_buf[0] == '2') {
		memset(mtk_pidmap, 0, sizeof(mtk_pidmap));
		pr_info("[pidmap] reset pidmap\n");
	} else
		goto err;

	goto out;

err:
	pr_info("[pidmap] invalid arg: 0x%x\n", mtk_pidmap_proc_cmd_buf[0]);
	return -1;
out:
	return count;
}

static int mtk_pidmap_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_pidmap_seq_show, inode->i_private);
}

static const struct file_operations mtk_pidmap_proc_fops = {
	.open = mtk_pidmap_proc_open,
	.write = mtk_pidmap_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mtk_pidmap_proc_init(void)
{
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	mtk_pidmap_proc_entry = proc_create("pidmap",
		PIDMAP_PROC_PERM, NULL,
		&mtk_pidmap_proc_fops);

	if (mtk_pidmap_proc_entry)
		proc_set_user(mtk_pidmap_proc_entry, uid, gid);
	else
		pr_info("[pidmap] failed to create /proc/pidmap\n");

	return 0;
}

static int __init mtk_pidmap_init(void)
{
	mtk_pidmap_init_map();
	mtk_pidmap_proc_init();

	return 0;
}

static void __exit mtk_pidmap_exit(void)
{
	proc_remove(mtk_pidmap_proc_entry);
}

module_init(mtk_pidmap_init);
module_exit(mtk_pidmap_exit);

#else /* CONFIG_MTK_PID_MAP */

void get_pidmap_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	/* return valid buffer size */
	if (size)
		*size = 0;
}
EXPORT_SYMBOL(get_pidmap_aee_buffer);

#endif /* CONFIG_MTK_PID_MAP */

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PID Map");

