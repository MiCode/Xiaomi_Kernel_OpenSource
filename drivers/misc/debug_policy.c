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


#define PROC_DP_STATUS_FILE "dp"
static struct proc_dir_entry *entry;
extern char  *saved_command_line;

static int dp_status_proc_show(struct seq_file *file, void *data)
{
/*BSP.Security - 2020.12.17 - modify cpuid - start*/
	char *tempbuf = NULL;
	char *tempbuf2 = NULL;
	char temp[60] = {0};

	pr_err("[%s]: dp_status_proc_show failed\n", __func__);
	tempbuf = strstr(saved_command_line, "androidboot.dp=");
	if (tempbuf != 0) {
		tempbuf2 = strstr(tempbuf, " ");
		if (tempbuf2 != 0) {
			strncpy(temp, tempbuf + 15, tempbuf2 - tempbuf - 15);
			temp[tempbuf2 - tempbuf - 15] = '\0';
			seq_printf(file, "%s\n", temp);
		}
	} else {
		pr_err("[Loren] dp_status_proc_show failed\n");
	}
/*BSP.Security - 2020.12.17 - modify cpuid - end*/
	return 0;
}

static int dp_status_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, dp_status_proc_show, inode->i_private);
}

static const struct file_operations dp_status_proc_fops = {
	.open = dp_status_proc_open,
	.read = seq_read,
};

static int __init dp_init(void)
{

    entry = proc_create(PROC_DP_STATUS_FILE, 0644, NULL, &dp_status_proc_fops);
	if (entry == NULL) {
		pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}
}

late_initcall(dp_init);

static void __exit dp_exit(void)
{
	printk("dp_exit\n");
}

module_exit(dp_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("debugpolicy feature");