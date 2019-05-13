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

#define TP_SELF_TEST_RETROY_COUNT        3
#define TP_SELF_TEST_CHECK_STATE_COUNT   30

#define TP_SELF_TEST_PROC_FILE           "tp_selftest"

#define TP_SELF_TEST_RESULT_UNKNOW       "0\n"
#define TP_SELF_TEST_RESULT_FAIL         "1\n"
#define TP_SELF_TEST_RESULT_PASS         "2\n"

#define TP_SELF_TEST_LONGCHEER_MMI_CMD   "mmi"
#define TP_SELF_TEST_XIAOMI_I2C_CMD      "i2c"
#define TP_SELF_TEST_XIAOMI_OPEN_CMD     "open"
#define TP_SELF_TEST_XIAOMI_SHORT_CMD    "short"

enum lct_tp_selftest_cmd {
	TP_SELFTEST_CMD_LONGCHEER_MMI = 0x00,
	TP_SELFTEST_CMD_XIAOMI_I2C = 0x01,
	TP_SELFTEST_CMD_XIAOMI_OPEN = 0x02,
	TP_SELFTEST_CMD_XIAOMI_SHORT = 0x03,
};


#define TP_INFO_TAG           "LCT_TP_SELFTEST"
#define TP_INFO_LOG_ENABLE

#ifdef TP_INFO_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_INFO_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_INFO_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */


typedef int (*tp_selftest_callback_t)(unsigned char cmd);
static tp_selftest_callback_t tp_selftest_callback_func = NULL;

static char ft_tp_selftest_status[15] = {0};
static bool is_in_self_test = false;
static int lct_tp_selftest_cmd;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

static int tp_chip_self_test(void);
static void tp_selftest_work_func(void);

static ssize_t tp_selftest_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char tmp_data[64] = {0};

	if(copy_from_user(tmp_data, buf, size)) {
		TP_LOGE("copy_from_user() fail.\n");
		return -EFAULT;
	}

	if( (strncmp(tmp_data, TP_SELF_TEST_LONGCHEER_MMI_CMD, strlen(TP_SELF_TEST_LONGCHEER_MMI_CMD)) == 0)
			&& (!is_in_self_test)) {

		TP_LOGW("Longcheer MMI TP self-test ...\n");
		lct_tp_selftest_cmd = TP_SELFTEST_CMD_LONGCHEER_MMI;

	} else if ((strncmp(tmp_data, TP_SELF_TEST_XIAOMI_I2C_CMD, strlen(TP_SELF_TEST_XIAOMI_I2C_CMD)) == 0)
			&& (!is_in_self_test)) {

		TP_LOGW("Xiaomi TP i2c self-test ...\n");
		lct_tp_selftest_cmd = TP_SELFTEST_CMD_XIAOMI_I2C;


	} else if ((strncmp(tmp_data, TP_SELF_TEST_XIAOMI_OPEN_CMD, strlen(TP_SELF_TEST_XIAOMI_OPEN_CMD)) == 0)
			&& (!is_in_self_test)) {

		TP_LOGW("Xiaomi TP open self-test ...\n");
		lct_tp_selftest_cmd = TP_SELFTEST_CMD_XIAOMI_OPEN;

	} else if ((strncmp(tmp_data, TP_SELF_TEST_XIAOMI_SHORT_CMD, strlen(TP_SELF_TEST_XIAOMI_SHORT_CMD)) == 0)
			&& (!is_in_self_test)) {

		TP_LOGW("Xiaomi TP short self-test ...\n");
		lct_tp_selftest_cmd = TP_SELFTEST_CMD_XIAOMI_SHORT;

	} else {

		TP_LOGW("Unknow command\n");
		is_in_self_test = false;
		return size;

	}

	is_in_self_test = true;

	return size;
}

static ssize_t tp_selftest_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char *page = NULL;

	if (is_in_self_test)
		tp_selftest_work_func();

	page = kzalloc(128, GFP_KERNEL);
	cnt = sprintf(page, "%s", ft_tp_selftest_status);
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	TP_LOGW("page=%s, cnt=%d\n", page, cnt);
	kfree(page);

	return cnt;
}

static int tp_chip_self_test(void)
{
	unsigned char cmd = lct_tp_selftest_cmd;
	if(tp_selftest_callback_func == NULL) {
		TP_LOGW("The TP is not support self test func\n");
		return 0;
	} else {
		TP_LOGW("Testing ...\n");
		return tp_selftest_callback_func(cmd);
	}

}

static void tp_selftest_work_func(void)
{
	int i = 0;
	int val = 0;

	for(i = 0; i < TP_SELF_TEST_RETROY_COUNT; i++) {
		TP_LOGW("tp self test count = %d\n", i);
		val = tp_chip_self_test();
		if(val == 2) {
			strcpy(ft_tp_selftest_status, TP_SELF_TEST_RESULT_PASS);
			TP_LOGW("self test success\n");
			break;
		} else if(val == 1) {
			strcpy(ft_tp_selftest_status, TP_SELF_TEST_RESULT_FAIL);
			TP_LOGW("self test failed\n");
		} else {
			strcpy(ft_tp_selftest_status, TP_SELF_TEST_RESULT_UNKNOW);
			TP_LOGW("self test result unknow\n");
			break;
		}
	}
	is_in_self_test = false;
}

void lct_tp_selftest_init(tp_selftest_callback_t callback)
{
	tp_selftest_callback_func = callback;
}
EXPORT_SYMBOL(lct_tp_selftest_init);

static const struct file_operations tp_selftest_proc_fops = {
	.read		= tp_selftest_proc_read,
	.write		= tp_selftest_proc_write,
};

static int __init tp_selftest_init(void)
{
	struct proc_dir_entry *tp_selftest_proc = NULL;

	tp_selftest_proc = proc_create_data(TP_SELF_TEST_PROC_FILE, 0666, NULL, &tp_selftest_proc_fops, NULL);
	if (IS_ERR_OR_NULL(tp_selftest_proc)) {
		TP_LOGW("create /proc/%s failed\n", TP_SELF_TEST_PROC_FILE);
	} else {
		TP_LOGW("create /proc/%s success\n", TP_SELF_TEST_PROC_FILE);
	}
	return 0;
}

static void __exit tp_selftest_exit(void)
{
	return;
}

module_init(tp_selftest_init);
module_exit(tp_selftest_exit);

MODULE_DESCRIPTION("TP selftest driver");
MODULE_LICENSE("GPL");

