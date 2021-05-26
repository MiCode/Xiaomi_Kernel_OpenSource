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
#define PROC_SERIAL_NUM_FILE "serial_num"
static struct proc_dir_entry *entry;
/*BSP.Security - 2020.12.17 - modify cpuid - start*/
extern char  *saved_command_line;
/*BSP.Security - 2020.12.17 - modify cpuid - end*/
#if 0  // Need root to read emmc id node
#define EMMC_ID_PATH "sys/class/mmc_host/mmc0/mmc0:0001/block/mmcblk0/device/serial"

int read_file(char *file_path, char *buf, int size)
{

	struct file *file_p = NULL;
	mm_segment_t old_fs;
	loff_t pos;
	int ret;
	file_p = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(file_p)) {
			pr_err("%s fail to open file \n", __func__);
			return -1;
	} else{
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		ret = vfs_read(file_p, buf, size, &pos);
		filp_close(file_p, NULL);
		set_fs(old_fs);
		file_p = NULL;
	}

	return ret;
}

static void read_emmc_id(char *emmc_id_buf)
{
	char buf[16] = {0};
	read_file(EMMC_ID_PATH, buf, sizeof(buf));
	sprintf(emmc_id_buf, "%s", buf);

}
#endif

static int serial_num_proc_show(struct seq_file *file, void *data)
{
/*BSP.Security - 2020.12.17 - modify cpuid - start*/
	char *tempbuf = NULL;
	char *tempbuf2 = NULL;
	char temp[60] = {0};

	pr_err("[%s]: serial_num_proc_show failed\n", __func__);
	tempbuf = strstr(saved_command_line, "androidboot.cpuid=0x");
	if (tempbuf != 0) {
		tempbuf2 = strstr(tempbuf, " ");
		if (tempbuf2 != 0) {
			strncpy(temp, tempbuf + 20, tempbuf2 - tempbuf - 20);
			temp[tempbuf2 - tempbuf - 20] = '\0';
			seq_printf(file, "0x%s\n", temp);
		}
	} else {
		seq_printf(file, "%s\n", "123456ABCDEF");
	}
/*BSP.Security - 2020.12.17 - modify cpuid - end*/
	return 0;
}

static int serial_num_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, serial_num_proc_show, inode->i_private);
}

static const struct file_operations serial_num_proc_fops = {
	.open = serial_num_proc_open,
	.read = seq_read,
};

static int __init sn_fuse_init(void)
{
	entry = proc_create(PROC_SERIAL_NUM_FILE, 0644, NULL, &serial_num_proc_fops);
	if (entry == NULL) {
		pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}

	return 0;
}

late_initcall(sn_fuse_init);

static void __exit sn_fuse_exit(void)
{
	printk("sn_fuse_exit\n");
}

module_exit(sn_fuse_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JTag Fuse driver");

