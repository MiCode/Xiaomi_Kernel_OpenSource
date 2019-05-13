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



#include "lct_if_ctp_upgrade.h"


#define CTP_IF_UPGRADE_PROC_FILE "ctp_if_upgrade"
static struct proc_dir_entry *g_ctp_if_upgrade_proc = NULL;

static struct work_struct ctp_if_upgrade_work;
static struct workqueue_struct * ctp_if_upgrade_workqueue = NULL;
static char ctp_if_upgrade_status[20] = {0};


 #define CTP_IF_UPGRADE_VERSION_PROC_FILE "ctp_if_upgrade_version"

static struct proc_dir_entry *g_ctp_if_upgrade_version_proc = NULL;

 #define CTP_IF_UPGRADE_OLD_VERSION_PROC_FILE "ctp_if_upgrade_old_version"

static struct proc_dir_entry *g_ctp_if_upgrade_old_version_proc = NULL;


static CTP_UPGRADE_FUNC PIFCtpupdateFunc = NULL;
static CTP_READ_VERSION PIFCtpreadverFunc = NULL;
static CTP_READ_VERSION PIFCtpreadOldverFunc = NULL;



 static int ctp_if_upgrade_start(void)
 {
 	if(PIFCtpupdateFunc == NULL)
	{
		strcpy(ctp_if_upgrade_status,"Unsupport");
	}
	else
	{
		printk("ctp_upgrade_from_engineermode call pointer func to upgrade \n");
		PIFCtpupdateFunc();
	}
 	return 0;
 }

static void ctp_if_upgrade_workqueue_func(struct work_struct *work)
{
	printk("ctp_upgrade_workqueue_func\n");
	ctp_if_upgrade_start();
}

static ssize_t ctp_if_upgrade_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
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
		if(PIFCtpupdateFunc == NULL)
		{
			strcpy(ctp_if_upgrade_status,"Unsupport");
		}
		else
		{
			strcpy(ctp_if_upgrade_status,"upgrading");
			queue_work(ctp_if_upgrade_workqueue, &ctp_if_upgrade_work);
		}
		
	}
	return size;
}

static ssize_t ctp_if_upgrade_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
    char *page = NULL;
	
	page = kzalloc(64, GFP_KERNEL);
	cnt = sprintf(page, "%s",ctp_if_upgrade_status);   
	printk("ctp_if_upgrade_proc_read cnt = %d\n",cnt);
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	kfree(page);
	return cnt;
}

static ssize_t ctp_if_upgrade_version_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char version[100] = {0};
    char *page = NULL;
	
	page = kzalloc(128, GFP_KERNEL);

	if(PIFCtpreadverFunc == NULL)
	{
		cnt = sprintf(page, "%s\n","Unknown");
	}
	else
	{
		PIFCtpreadverFunc(version);
		cnt = sprintf(page, "%s\n",version);
	}

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	kfree(page);
	return cnt;
}

static ssize_t ctp_if_upgrade_old_version_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char version[100] = {0};
    char *page = NULL;
	
	page = kzalloc(128, GFP_KERNEL);

	if(PIFCtpreadOldverFunc == NULL)
	{
		cnt = sprintf(page, "%s\n","Unknown");
	}
	else
	{
		PIFCtpreadOldverFunc(version);
		cnt = sprintf(page, "%s\n",version);
	}
	
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	kfree(page);
	return cnt;
}

void lct_if_ctp_upgrade_int(CTP_UPGRADE_FUNC PUpdatefunc,CTP_READ_VERSION PVerfunc,CTP_READ_VERSION POldVerfunc)
{
	PIFCtpupdateFunc = PUpdatefunc;
	PIFCtpreadverFunc = PVerfunc;
	PIFCtpreadOldverFunc = POldVerfunc;
}

int lct_set_if_ctp_upgrade_status(char * status)
{
	if((status != NULL) && (strlen(status) < 20))
	{
		strcpy(ctp_if_upgrade_status,status);
	}
	return 0;
}

static const struct file_operations g_ctp_if_upgrade_proc_fops = {
	.read		= ctp_if_upgrade_proc_read,
	.write		= ctp_if_upgrade_proc_write,
};

static const struct file_operations g_ctp_if_upgrade_version_proc_fops = {
	.read		= ctp_if_upgrade_version_proc_read,
};

static const struct file_operations g_ctp_if_upgrade_old_version_proc_fops = {
	.read		= ctp_if_upgrade_old_version_proc_read,
};

static int __init ctp_if_upgrade_init(void)
{
	printk( "%s\n", __func__);
		
	strcpy(ctp_if_upgrade_status,"Unsupport");
	ctp_if_upgrade_workqueue = create_singlethread_workqueue("ctp_if_upgrade");
	INIT_WORK(&ctp_if_upgrade_work, ctp_if_upgrade_workqueue_func);
	
#if 1
	g_ctp_if_upgrade_proc = proc_create_data(CTP_IF_UPGRADE_PROC_FILE, 0660, NULL, &g_ctp_if_upgrade_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_if_upgrade_proc))
	{
		printk(" ctp_if_upgrade_init create_proc_entry 222 failed\n");
	}
	else
	{
		printk("ctp_if_upgrade_init create_proc_entry 222 success\n");
	}
	
	g_ctp_if_upgrade_version_proc = proc_create_data(CTP_IF_UPGRADE_VERSION_PROC_FILE, 0660, NULL, &g_ctp_if_upgrade_version_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_if_upgrade_version_proc))
	{
		printk("create_proc_entry failed\n");
	}
	else
	{
		printk("create_proc_entry success\n");
	}
	
	g_ctp_if_upgrade_old_version_proc = proc_create_data(CTP_IF_UPGRADE_OLD_VERSION_PROC_FILE, 0444, NULL, &g_ctp_if_upgrade_old_version_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_if_upgrade_old_version_proc))
	{
		printk("create_proc_entry failed\n");
	}
	else
	{
		printk("create_proc_entry success\n");
	}
#else
	g_ctp_if_upgrade_proc = create_proc_entry(CTP_IF_UPGRADE_PROC_FILE, 0660, NULL);
	if (g_ctp_if_upgrade_proc == NULL) {
		printk(" ctp_if_upgrade_init create_proc_entry 222 failed\n");
	} else {
		g_ctp_if_upgrade_proc->read_proc = ctp_if_upgrade_proc_read;
		g_ctp_if_upgrade_proc->write_proc = ctp_if_upgrade_proc_write;

		printk("ctp_if_upgrade_init create_proc_entry 222 success\n");
	}


	g_ctp_if_upgrade_version_proc = create_proc_entry(CTP_IF_UPGRADE_VERSION_PROC_FILE, 0444, NULL);
	if (g_ctp_if_upgrade_version_proc == NULL) {
		printk("create_proc_entry failed\n");
	} else {
		g_ctp_if_upgrade_version_proc->read_proc = ctp_if_upgrade_version_proc_read;
		g_ctp_if_upgrade_version_proc->write_proc = NULL;	
		printk("create_proc_entry success\n");
	}


	g_ctp_if_upgrade_old_version_proc = create_proc_entry(CTP_IF_UPGRADE_OLD_VERSION_PROC_FILE, 0444, NULL);
	if (g_ctp_if_upgrade_old_version_proc == NULL) {
		printk("create_proc_entry failed\n");
	} else {
		g_ctp_if_upgrade_old_version_proc->read_proc = ctp_if_upgrade_old_version_proc_read;
		g_ctp_if_upgrade_old_version_proc->write_proc = NULL;	
		printk("create_proc_entry success\n");
	}
#endif
	return 0;
}

static void __exit ctp_if_upgrade_exit(void)
{
	return;
}

module_init(ctp_if_upgrade_init);
module_exit(ctp_if_upgrade_exit);

MODULE_DESCRIPTION("CTP if upgrade driver");
MODULE_LICENSE("GPL");



