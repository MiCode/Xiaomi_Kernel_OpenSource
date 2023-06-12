#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <soc/qcom/socinfo.h>
#include <linux/soc/qcom/smem.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include "mi_memory_sysfs.h"
#include "mem_interface.h"
#include "mi_mem_type.h"


static int memory_type_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "UFS\n");
	return 0;
}

static int memory_type_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, memory_type_proc_show, NULL);
}

static const struct file_operations memory_type_proc_fops = {
	.open		= memory_type_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int add_proc_memtype_node(void)
{
	proc_create(MEMTYPE_NAME, 0555, NULL, &memory_type_proc_fops);
	return 0;
}

int remove_proc_memtype_node(void)
{
	remove_proc_entry(MEMTYPE_NAME, NULL);
	return 0;
}
