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



typedef  int (*CTP_SELFTEST_FUNC)(void);

static CTP_SELFTEST_FUNC PSelftest_func;




#define CTP_FELF_TEST_RETROY_COUNT      3
#define CTP_SELF_TEST_CHECK_STATE_COUNT 30

#define CTP_SELF_TEST_PROC_FILE "tp_selftest"
static struct proc_dir_entry *g_ctp_selftest_proc;
static struct work_struct ctp_selftest_work;
static struct workqueue_struct *ctp_selftest_workqueue;

static char ft_ctp_selftest_status[15] = {0};
static int is_in_self_test;
static int retry_count = CTP_FELF_TEST_RETROY_COUNT;




static ssize_t ctp_selftest_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char tmp_data[64] = {0};

	if (copy_from_user(tmp_data, buf, size))
    {
		printk("copy_from_user() fail.\n");
		return -EFAULT;
    }

		if ((strncmp(tmp_data, "mmi", strlen("mmi")) == 0) && (is_in_self_test == 0))
		{
			printk("ctp_selftest_proc_write start \n");
			strcpy(ft_ctp_selftest_status, "Testing");
			is_in_self_test = 1;
			retry_count = CTP_FELF_TEST_RETROY_COUNT;
			queue_work(ctp_selftest_workqueue, &ctp_selftest_work);

		}
	return size;
}

static ssize_t ctp_selftest_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt = 0;
    char *page = NULL;

	page = kzalloc(128, GFP_KERNEL);
	cnt = sprintf(page, "%s", ft_ctp_selftest_status);

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	printk("%s, page=%s, cnt=%d\n", __func__, page, cnt);
	kfree(page);
	return cnt;
}

static int ctp_chip_self_test(void)
{
	if (PSelftest_func == NULL)
	{
		printk("ctp_chip_self_test this ctp is not support self test func \n");
		return 0;
	}
	else
	{
		printk("ctp_chip_self_test call pointer func to test \n");
		return PSelftest_func();
	}

}
static void ctp_selftest_workqueue_func(struct work_struct *work)
{
	int val = 0x00;
	printk("ctp_selftest_workqueue_func is_in_self_test=%d,retry_count=%d\n", is_in_self_test, retry_count);
	val = ctp_chip_self_test();
	if (val == 2)
	{
		strcpy(ft_ctp_selftest_status, "2\n");
		is_in_self_test = 0;
		printk("ctp_selftest_workqueue_func self test success \n");
	}
	else if (val == 1)
	{
		retry_count--;
		if (retry_count > 0)
		{
			printk("ctp_selftest_workqueue_func self test retry retry_count=%d\n", retry_count);
			queue_work(ctp_selftest_workqueue, &ctp_selftest_work);
		}
		else
		{
			strcpy(ft_ctp_selftest_status, "1\n");
			printk("ctp_selftest_workqueue_func self test failed\n");
			is_in_self_test = 0;
		}

	}
		else
		{
			strcpy(ft_ctp_selftest_status, "0\n");
			is_in_self_test = 0;
			printk("ctp_selftest_workqueue_func self test invalid\n");
		}

}

int lct_get_ctp_selttest_status(void)
{
	return is_in_self_test;
}
EXPORT_SYMBOL(lct_get_ctp_selttest_status);

void lct_ctp_selftest_int(CTP_SELFTEST_FUNC PTestfunc)
{
	printk("%s\n", __func__);
	PSelftest_func = PTestfunc;
}
EXPORT_SYMBOL(lct_ctp_selftest_int);

static const struct file_operations ctp_selftest_proc_fops = {
	.read		= ctp_selftest_proc_read,
	.write		= ctp_selftest_proc_write,
};

static int  ctp_selftest_init(void)
{
	printk("%s\n", __func__);
	ctp_selftest_workqueue = create_singlethread_workqueue("ctp_selftest");
	INIT_WORK(&ctp_selftest_work, ctp_selftest_workqueue_func);

#if 1
	g_ctp_selftest_proc = proc_create_data(CTP_SELF_TEST_PROC_FILE, 0666, NULL, &ctp_selftest_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_ctp_selftest_proc))
	{
		printk("create_proc_entry g_ctp_selftest_proc failed\n");
	}
	else
	{
		printk("create_proc_entry g_ctp_selftest_proc success\n");
	}
#else
	g_ctp_selftest_proc = create_proc_entry(CTP_SELF_TEST_PROC_FILE, 0660, NULL);
	if (g_ctp_selftest_proc == NULL) {
		printk("create_proc_entry g_ctp_selftest_proc failed\n");
	} else {
		g_ctp_selftest_proc->read_proc = ctp_selftest_proc_read;
		g_ctp_selftest_proc->write_proc = ctp_selftest_proc_write;
		printk("create_proc_entry g_ctp_selftest_proc success\n");
	}
#endif
	return 0;
}

static void __exit ctp_selftest_exit(void)
{
	return;
}

module_init(ctp_selftest_init);
module_exit(ctp_selftest_exit);

MODULE_DESCRIPTION("CTP selftest driver");
MODULE_LICENSE("GPL");



