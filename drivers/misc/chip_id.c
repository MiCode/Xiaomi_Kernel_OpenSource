#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/mmc/mmc.h>
#define PROC_CHIP_ID "chip_id"

#define HRID0 12
#define HRID1 13
#define HRID2 14
#define HRID3 15
static struct proc_dir_entry *entry;

static int chip_id_proc_show(struct seq_file *file, void *data)
{
	char temp[60] = {0};
	u32 temp0, temp1, temp2, temp3;

	temp0 = get_devinfo_with_index(HRID0);
	temp1 = get_devinfo_with_index(HRID1);
	temp2 = get_devinfo_with_index(HRID2);
	temp3 = get_devinfo_with_index(HRID3);

	pr_err("[HR_ID] temp0=0x%x, temp1=0x%x, temp2=0x%x, temp2=0x%x\n",
							temp0, temp1, temp2, temp3);

	sprintf(temp, "0x%08x%08x%08x%08x\n", temp0, temp1, temp2, temp3);
	seq_printf(file, "%s\n", temp);
	return 0;
}

static int chip_id_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, chip_id_proc_show, inode->i_private);
}

static const struct file_operations chip_id_proc_fops = {
	.open = chip_id_proc_open,
	.read = seq_read,
};

static int __init chip_id_init(void)
{
	entry = proc_create(PROC_CHIP_ID, 0644, NULL, &chip_id_proc_fops);
	if (entry == NULL) {
		pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}

	return 0;
}

late_initcall(chip_id_init);

static void __exit chip_id_exit(void)
{
	printk("chip_id_exit\n");
}

module_exit(chip_id_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JTag Fuse driver");;

