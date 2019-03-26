/****************************************************************************************
 *
 * @File Name   : lct_tp_info.c
 * @Author      : wanghan
 * @E-mail      : <wanghan@longcheer.com>
 * @Create Time : 2018-08-17 17:34:43
 * @Description : Display touchpad information.
 *
 ****************************************************************************************/

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>

/*
 * DEFINE CONFIGURATION
 ****************************************************************************************
 */
#define TP_INFO_LOG_ENABLE
#define TP_INFO_TAG           "LCT_TP_INFO"
#define LCT_STRING_SIZE       128
#define TP_CALLBACK_CMD_INFO      "CMD_INFO"
#define TP_CALLBACK_CMD_LOCKDOWN  "CMD_LOCKDOWN"

#ifdef TP_INFO_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_INFO_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_INFO_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef struct lct_tp{
	struct kobject *tp_device;
	char tp_info_buf[LCT_STRING_SIZE];
	char tp_lockdown_info_buf[LCT_STRING_SIZE];
	int (*pfun)(const char *);
}lct_tp_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static lct_tp_t *lct_tp_p = NULL;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
// --- proc ---
static int lct_creat_proc_tp_entry(void);
static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_lockdown_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static const struct file_operations lct_proc_tp_info_fops = {
	.read		= lct_proc_tp_info_read,
};
static const struct file_operations lct_proc_tp_lockdown_info_fops = {
	.read		= lct_proc_tp_lockdown_info_read,
};
// --- sysfs ---
static int lct_creat_sys_tp_entry(void);
static ssize_t lct_sys_tp_info_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lct_sys_tp_lockdown_info_show(struct device *dev, struct device_attribute *attr, char *buf);
static DEVICE_ATTR(tp_info, 0444, lct_sys_tp_info_show, NULL);
static DEVICE_ATTR(tp_lockdown_info, 0444, lct_sys_tp_lockdown_info_show, NULL);



int init_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	TP_LOGW("Initialization tp_info & tp_lockdown_info node!\n");
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)){
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}

	if (NULL != tp_info_buf)
		strcpy(lct_tp_p->tp_info_buf, tp_info_buf);
	if (NULL != tp_lockdown_info_buf)
		strcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf);

	lct_creat_proc_tp_entry();
	lct_creat_sys_tp_entry();

	return 0;
}
EXPORT_SYMBOL(init_lct_tp_info);

void update_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	if (NULL != tp_info_buf) {
		memset(lct_tp_p->tp_info_buf, 0, sizeof(lct_tp_p->tp_info_buf));
		strcpy(lct_tp_p->tp_info_buf, tp_info_buf);
	}
	if (NULL != tp_lockdown_info_buf) {
		memset(lct_tp_p->tp_lockdown_info_buf, 0, sizeof(lct_tp_p->tp_lockdown_info_buf));
		strcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf);
	}
	return;
}
EXPORT_SYMBOL(update_lct_tp_info);

void set_lct_tp_info_callback(int (*pfun)(const char *))
{
	if (NULL != pfun)
		lct_tp_p->pfun = pfun;
	return;
}
EXPORT_SYMBOL(set_lct_tp_info_callback);

static int lct_creat_proc_tp_entry(void)
{
	struct proc_dir_entry *proc_entry_tp;

	proc_entry_tp = proc_create_data("tp_info", 0444, NULL, &lct_proc_tp_info_fops, NULL);
	if (IS_ERR_OR_NULL(proc_entry_tp))
		TP_LOGE("add /proc/tp_info error \n");
	proc_entry_tp = proc_create_data("tp_lockdown_info", 0444, NULL, &lct_proc_tp_lockdown_info_fops, NULL);
	if (IS_ERR_OR_NULL(proc_entry_tp))
		TP_LOGE("add /proc/tp_lockdown_info error \n");
	TP_LOGW("/proc/tp_info & /proc/tp_lockdown_info is okay!\n");

	return 0;
}

static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt=0;
	char *page = NULL;

	if (NULL != lct_tp_p->pfun)
		lct_tp_p->pfun(TP_CALLBACK_CMD_INFO);

	page = kzalloc(128, GFP_KERNEL);

	if(NULL == lct_tp_p->tp_info_buf)
		cnt = sprintf(page, "No touchpad\n");
	else
		cnt = sprintf(page, "%s", (strlen(lct_tp_p->tp_info_buf) ? lct_tp_p->tp_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	TP_LOGW("page=%s", page);

	kfree(page);
	return cnt;
}

static ssize_t lct_proc_tp_lockdown_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt=0;
	char *page = NULL;

	if (NULL != lct_tp_p->pfun)
		lct_tp_p->pfun(TP_CALLBACK_CMD_LOCKDOWN);

	page = kzalloc(128, GFP_KERNEL);

	if(NULL == lct_tp_p->tp_lockdown_info_buf)
		cnt = sprintf(page, "No touchpad\n");
	else
		cnt = sprintf(page, "%s", (strlen(lct_tp_p->tp_lockdown_info_buf) ? lct_tp_p->tp_lockdown_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	TP_LOGW("page=%s", page);

	kfree(page);
	return cnt;
}

static int lct_creat_sys_tp_entry(void)
{
	int32_t rc = 0;

	lct_tp_p->tp_device = kobject_create_and_add("android_tp", NULL);
	if (lct_tp_p->tp_device == NULL) {
		TP_LOGE("subsystem_register failed!\n");
		return -ENOMEM;
	}
	rc = sysfs_create_file(lct_tp_p->tp_device, &dev_attr_tp_info.attr);
	if (rc) {
		TP_LOGE("sysfs_create_file tp_info failed!\n");
		kobject_del(lct_tp_p->tp_device);
		return rc;
	}
	rc = sysfs_create_file(lct_tp_p->tp_device, &dev_attr_tp_lockdown_info.attr);
	if (rc) {
		TP_LOGE("sysfs_create_file tp_lockdown_info failed!\n");
		kobject_del(lct_tp_p->tp_device);
		return rc;
	}
	TP_LOGW("/sys/android_tp/tp_info & tp_lockdown_info is okay!\n");

	return 0 ;
}

static ssize_t lct_sys_tp_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	if (NULL != lct_tp_p->pfun)
		lct_tp_p->pfun(TP_CALLBACK_CMD_INFO);

	if(NULL == lct_tp_p->tp_info_buf)
		rc = sprintf(buf,"No touchpad\n");
	else
		rc = sprintf(buf, "%s", (strlen(lct_tp_p->tp_info_buf) ? lct_tp_p->tp_info_buf : "Unknown touchpad"));

	return rc;
}

static ssize_t lct_sys_tp_lockdown_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	if (NULL != lct_tp_p->pfun)
		lct_tp_p->pfun(TP_CALLBACK_CMD_LOCKDOWN);

	if(NULL == lct_tp_p->tp_lockdown_info_buf)
		rc = sprintf(buf,"No touchpad\n");
	else
		rc = sprintf(buf, "%s", (strlen(lct_tp_p->tp_lockdown_info_buf) ? lct_tp_p->tp_lockdown_info_buf : "Unknown touchpad"));

	return rc;
}

MODULE_DESCRIPTION("Touchpad Information Driver");
MODULE_LICENSE("GPL");
