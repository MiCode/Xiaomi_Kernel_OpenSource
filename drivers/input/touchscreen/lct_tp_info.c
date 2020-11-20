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
#define TP_INFO_NAME          "tp_info"
#define TP_LOCKDOWN_INFO_NAME "tp_lockdown_info"
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
typedef struct lct_tp {
	struct kobject *tp_device;
	char tp_info_buf[LCT_STRING_SIZE];
	char tp_lockdown_info_buf[LCT_STRING_SIZE];
	struct proc_dir_entry *proc_entry_tp_info;
	struct proc_dir_entry *proc_entry_tp_lockdown_info;
	int (*pfun_info_cb) (const char *);
} lct_tp_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static lct_tp_t *lct_tp_p;
static int (*pfun_lockdown_cb) (void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
// --- proc ---
static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf,
				     size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_lockdown_info_read(struct file *file,
					      char __user *buf, size_t size,
					      loff_t *ppos);
static const struct file_operations lct_proc_tp_info_fops = {
	.read = lct_proc_tp_info_read,
};

static const struct file_operations lct_proc_tp_lockdown_info_fops = {
	.read = lct_proc_tp_lockdown_info_read,
};

int init_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	TP_LOGW("init /proc/%s and /proc/%s ...\n", TP_INFO_NAME,
		TP_LOCKDOWN_INFO_NAME);
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)) {
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}

	if (NULL != tp_info_buf)
		strlcpy(lct_tp_p->tp_info_buf, tp_info_buf,
			sizeof(lct_tp_p->tp_info_buf));
	if (NULL != tp_lockdown_info_buf)
		strlcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf,
			sizeof(lct_tp_p->tp_lockdown_info_buf));

	lct_tp_p->proc_entry_tp_info =
	    proc_create_data(TP_INFO_NAME, 0444, NULL, &lct_proc_tp_info_fops,
			     NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp_info)) {
		TP_LOGE("add /proc/%s error\n", TP_INFO_NAME);
		goto err_tp_info;
	}
	lct_tp_p->proc_entry_tp_lockdown_info =
	    proc_create_data(TP_LOCKDOWN_INFO_NAME, 0444, NULL,
			     &lct_proc_tp_lockdown_info_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp_lockdown_info)) {
		TP_LOGE("add /proc/%s error\n", TP_LOCKDOWN_INFO_NAME);
		goto err_tp_lockdown;
	}

	TP_LOGW("done\n");

	return 0;

err_tp_lockdown:
	remove_proc_entry(TP_INFO_NAME, NULL);
err_tp_info:
	kfree(lct_tp_p);
	return -EPERM;
}

EXPORT_SYMBOL(init_lct_tp_info);

void uninit_lct_tp_info(void)
{
	TP_LOGW("uninit /proc/%s and /proc/%s ...\n", TP_INFO_NAME,
		TP_LOCKDOWN_INFO_NAME);

	if (IS_ERR_OR_NULL(lct_tp_p))
		return;

	if (lct_tp_p->proc_entry_tp_info != NULL) {
		remove_proc_entry(TP_INFO_NAME, NULL);
		lct_tp_p->proc_entry_tp_info = NULL;
		TP_LOGW("remove /proc/%s\n", TP_INFO_NAME);
	}

	if (lct_tp_p->proc_entry_tp_lockdown_info != NULL) {
		remove_proc_entry(TP_LOCKDOWN_INFO_NAME, NULL);
		lct_tp_p->proc_entry_tp_lockdown_info = NULL;
		TP_LOGW("remove /proc/%s\n", TP_LOCKDOWN_INFO_NAME);
	}

	kfree(lct_tp_p);
	TP_LOGW("done\n");
}

EXPORT_SYMBOL(uninit_lct_tp_info);

void update_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf)
{
	if (NULL != tp_info_buf) {
		memset(lct_tp_p->tp_info_buf, 0, sizeof(lct_tp_p->tp_info_buf));
		strlcpy(lct_tp_p->tp_info_buf, tp_info_buf,
			sizeof(lct_tp_p->tp_info_buf));
	}
	if (NULL != tp_lockdown_info_buf) {
		memset(lct_tp_p->tp_lockdown_info_buf, 0,
		       sizeof(lct_tp_p->tp_lockdown_info_buf));
		strlcpy(lct_tp_p->tp_lockdown_info_buf, tp_lockdown_info_buf,
			sizeof(lct_tp_p->tp_lockdown_info_buf));
	}
	return;
}

EXPORT_SYMBOL(update_lct_tp_info);

void set_lct_tp_info_callback(int (*pfun) (const char *))
{
	if (NULL != lct_tp_p)
		lct_tp_p->pfun_info_cb = pfun;
	return;
}

EXPORT_SYMBOL(set_lct_tp_info_callback);

void set_lct_tp_lockdown_info_callback(int (*pfun) (void))
{
	pfun_lockdown_cb = pfun;
	return;
}

EXPORT_SYMBOL(set_lct_tp_lockdown_info_callback);

static ssize_t lct_proc_tp_info_read(struct file *file, char __user *buf,
				     size_t size, loff_t *ppos)
{
	int cnt = 0;
	char *page = NULL;

	//TP_LOGW("size = %lu, pos = %lld\n", size, *ppos);
	if (*ppos)
		return 0;

	if (NULL != lct_tp_p->pfun_info_cb)
		lct_tp_p->pfun_info_cb(TP_CALLBACK_CMD_INFO);

	page = kzalloc(128, GFP_KERNEL);

	if (NULL == lct_tp_p->tp_info_buf)
		cnt = snprintf(page, PAGE_SIZE, "No touchpad\n");
	else
		cnt =
		    snprintf(page, PAGE_SIZE, "%s",
				(strlen(lct_tp_p->tp_info_buf) ? lct_tp_p->tp_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_LOGW("page=%s", page);

	kfree(page);
	return cnt;
}

static ssize_t lct_proc_tp_lockdown_info_read(struct file *file,
					      char __user *buf, size_t size,
					      loff_t *ppos)
{
	int cnt = 0;
	char *page = NULL;

	if (*ppos)
		return 0;

	if (NULL != lct_tp_p->pfun_info_cb)
		lct_tp_p->pfun_info_cb(TP_CALLBACK_CMD_LOCKDOWN);

	if (NULL != pfun_lockdown_cb)
		pfun_lockdown_cb();

	page = kzalloc(128, GFP_KERNEL);

	if (NULL == lct_tp_p->tp_lockdown_info_buf)
		cnt = snprintf(page, PAGE_SIZE, "No touchpad\n");
	else
		cnt =
		    snprintf(page, PAGE_SIZE, "%s",
			     (strlen(lct_tp_p->tp_lockdown_info_buf) ?
			      lct_tp_p->tp_lockdown_info_buf : "Unknown touchpad"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_LOGW("page=%s", page);

	kfree(page);
	return cnt;
}

static int __init tp_info_init(void)
{
	TP_LOGW("init");
	return 0;
}

static void __exit tp_info_exit(void)
{
	TP_LOGW("exit");
	return;
}

module_init(tp_info_init);
module_exit(tp_info_exit);

MODULE_DESCRIPTION("Touchpad Information Driver");
MODULE_LICENSE("GPL");
