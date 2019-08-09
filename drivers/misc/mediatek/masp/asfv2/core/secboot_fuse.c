#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include "sec_boot_lib.h"

#define CTRL_SBC_EN (0x1U << 1)
#define CTRL_DAA_EN (0x1U << 2)
#define CTRL_SLA_EN (0x1U << 3)

static int secboot_proc_show(struct seq_file *m, void *v)
{
	u32 efuse_value;
	char sbc_enable,daa_enable,sla_enable;
	u32 value=0x0;

	efuse_value = get_devinfo_with_index(27);
	sbc_enable =(efuse_value & CTRL_SBC_EN)? 1: 0;
	daa_enable =(efuse_value & CTRL_DAA_EN)? 1: 0;
	sla_enable =(efuse_value & CTRL_SLA_EN)? 1: 0;
	if(sbc_enable)
		value =0x300000;
	if(daa_enable)
		value |=0x3000;
	if(sla_enable)
		value |=0x30;

	seq_printf(m, "0x%x\n",value);
	return 0;
}

static int secboot_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, secboot_proc_show, NULL);
}

static const struct file_operations secboot_proc_fops = {
	.open		= secboot_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_secboot_init(void)
{
	proc_create("secboot_fuse_reg", 0, NULL, &secboot_proc_fops);
	return 0;
}
fs_initcall(proc_secboot_init);
