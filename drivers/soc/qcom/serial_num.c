#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <soc/qcom/socinfo.h>
#include <linux/seq_file.h>
static unsigned long sn;

static int sn_read(struct seq_file *m, void *v)
{
    if (sn == 0)
		sn = socinfo_get_serial_number();
	seq_printf(m, "0x%x\n", sn);
	return 0;
}

static int sn_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sn_read, NULL);
}

static const struct file_operations sn_fops = {
	.open		= sn_proc_open,
	.read		= seq_read,
};
int sn_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("serial_num", 0 /* default mode */,
			NULL /* parent dir */, &sn_fops);
	return 0;
}
void sn_exit(void)
{
	remove_proc_entry("serial_num", NULL);
}

module_init(sn_create_proc);
module_exit(sn_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("JTag Fuse driver");

