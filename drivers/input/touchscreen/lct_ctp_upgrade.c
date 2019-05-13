#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>



#include "lct_ctp_upgrade.h"


#define CTP_UPGRADE_PROC_FILE "ctp_upgrade"
static struct proc_dir_entry *g_ctp_upgrade_proc = NULL;

static struct work_struct ctp_upgrade_work;
static struct workqueue_struct * ctp_upgrade_workqueue = NULL;
static char ft_ctp_upgrade_status[20] = {0};


 #define CTP_PROC_FILE "ctp_version"

static struct proc_dir_entry *g_ctp_proc = NULL;


static CTP_UPGRADE_FUNC PCtpupdateFunc = NULL;
static CTP_READ_VERSION PCtpreadverFunc = NULL;


 static int ctp_upgrade_from_engineermode(void)
 {
 	if(PCtpupdateFunc == NULL)
	{
		strcpy(ft_ctp_upgrade_status,"Unsupport");
	}
	else
	{
		printk("ctp_upgrade_from_engineermode call pointer func to upgrade \n");
		PCtpupdateFunc();
	}
 	return 0;
 }

static void ctp_upgrade_workqueue_func(struct work_struct *work)
{
	printk("ctp_upgrade_workqueue_func\n");
	ctp_upgrade_from_engineermode();
}

static ssize_t ctp_upgrade_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char tmp_data[64] = {0};

	if(copy_from_user(tmp_data, buf, size))
    {
        printk("copy_from_user() fail.\n");
        return -EFAULT;
    }

	if(strncmp(tmp_data,"tp_update_13817059502",strlen("tp_update_13817059502")) == 0)
	{
		printk("ctp_upgrade_proc_write start upgrade\n");
		if(PCtpupdateFunc == NULL)
		{
			strcpy(ft_ctp_upgrade_status,"Unsupport");
		}
		else
		{
			strcpy(ft_ctp_upgrade_status,"upgrading");
			queue_work(ctp_upgrade_workqueue, &ctp_upgrade_work);
		}
	}
	return size;
}

static ssize_t ctp_upgrade_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
    char *page = NULL;

	page = kzalloc(256, GFP_KERNEL);
	cnt = sprintf(page, "%s",ft_ctp_upgrade_status);   

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);

	kfree(page);
	return cnt;
}

static ssize_t ctp_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char version[100] = {0};
    char *page = NULL;

	page = kzalloc(64, GFP_KERNEL);
	if(PCtpreadverFunc == NULL)
	{
		cnt = sprintf(page, "%s\n","Unknown");
	}
	else
	{
		PCtpreadverFunc(version);
		cnt = sprintf(page, "%s\n",version);
	}
	
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);

	kfree(page);
	return cnt;
}

void lct_ctp_upgrade_int(CTP_UPGRADE_FUNC PUpdatefunc,CTP_READ_VERSION PVerfunc)
{
	PCtpupdateFunc = PUpdatefunc;
	PCtpreadverFunc = PVerfunc;
}

int lct_set_ctp_upgrade_status(char * status)
{
	if((status != NULL) && (strlen(status) < 20))
	{
		strcpy(ft_ctp_upgrade_status,status);
	}
	return 0;
}

static const struct file_operations ctp_upgrade_proc_fops = {
	.read		= ctp_upgrade_proc_read,
	.write		= ctp_upgrade_proc_write,
};

static const struct file_operations ctp_proc_fops = {
	.read		= ctp_proc_read,
};

static int __init ctp_upgrade_init(void)
{
	printk( "%s\n", __func__);
		
	strcpy(ft_ctp_upgrade_status,"Unsupport");
	ctp_upgrade_workqueue = create_singlethread_workqueue("ctp_upgrade");
	INIT_WORK(&ctp_upgrade_work, ctp_upgrade_workqueue_func);

#if 1
	g_ctp_upgrade_proc = proc_create_data(CTP_UPGRADE_PROC_FILE, 0660, NULL, &ctp_upgrade_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_upgrade_proc))
	{
		printk("[elan] create_proc_entry 222 failed\n");
	}
	else
	{
		printk("[elan] create_proc_entry 222 success\n");
	}
	
	g_ctp_proc = proc_create_data(CTP_PROC_FILE, 0444, NULL, &ctp_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_proc))
	{
		printk("create_proc_entry failed\n");
	}
	else
	{
		printk("create_proc_entry success\n");
	}
#else
	g_ctp_upgrade_proc = create_proc_entry(CTP_UPGRADE_PROC_FILE, 0660, NULL);
	if (g_ctp_upgrade_proc == NULL) {
		printk("[elan] create_proc_entry 222 failed\n");
	} else {
		g_ctp_upgrade_proc->read_proc = ctp_upgrade_proc_read;
		g_ctp_upgrade_proc->write_proc = ctp_upgrade_proc_write;

		printk("[elan] create_proc_entry 222 success\n");
	}

	g_ctp_proc = create_proc_entry(CTP_PROC_FILE, 0444, NULL);
	if (g_ctp_proc == NULL) {
		printk("create_proc_entry failed\n");
	} else {
		g_ctp_proc->read_proc = ctp_proc_read;
		g_ctp_proc->write_proc = NULL;	
		printk("create_proc_entry success\n");
	}
#endif

	return 0;
}

static void __exit ctp_upgrade_exit(void)
{
	return;
}

module_init(ctp_upgrade_init);
module_exit(ctp_upgrade_exit);

MODULE_DESCRIPTION("CTP upgrade driver");
MODULE_LICENSE("GPL");



