/****************************************************************************************
 *
 * @File Name   : lct_tp_proximity_switch.c
 * @Author      : hongmo
 * @E-mail      : <hongmo@longcheer.com>
 * @Create Time : 2023-07-25 17:34:43
 * @Description : proximity switch.
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
#include <asm/uaccess.h>

/*
 * DEFINE CONFIGURATION
 ****************************************************************************************
 */
#define TP_PROXIMITY_SWITCH_NAME          "tp_proximity_switch"
#define TP_PROXIMITY_SWITCH_LOG_ENABLE
#define TP_PROXIMITY_SWITCH_TAG           "LCT_TP_PROXIMITY_SWITCH"

#ifdef TP_PROXIMITY_SWITCH_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_PROXIMITY_SWITCH_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_PROXIMITY_SWITCH_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

bool tp_proximity_probe = false;

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef int (*tp_proximity_switch_cb_t)(bool enable_tp);

typedef struct lct_tp{
	bool enable_tp_proximity_switch_flag;
	struct proc_dir_entry *proc_entry_tp;
	tp_proximity_switch_cb_t pfun;
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
static ssize_t lct_proc_tp_proximity_switch_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_proximity_switch_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos);
static const struct proc_ops lct_proc_tp_proximity_switch_fops = {
	.proc_read		= lct_proc_tp_proximity_switch_read,
	.proc_write		= lct_proc_tp_proximity_switch_write,
};


int init_lct_tp_proximity_switch(tp_proximity_switch_cb_t callback)
{
	if (NULL == callback) {
		TP_LOGE("callback is NULL!\n");
		return -EINVAL;
	}

	TP_LOGW("Initialization tp_proximity_switch node!\n");
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)){
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}
	lct_tp_p->pfun = callback;
	lct_tp_p->enable_tp_proximity_switch_flag = true;

	if (lct_creat_proc_tp_entry() < 0) {
		kfree(lct_tp_p);
		return -1;
	}
	tp_proximity_probe = true;

	return 0;
}
EXPORT_SYMBOL(init_lct_tp_proximity_switch);

void uninit_lct_tp_proximity_switch(void)
{
	TP_LOGW("uninit /proc/%s ...\n", TP_PROXIMITY_SWITCH_NAME);
	if (IS_ERR_OR_NULL(lct_tp_p))
		return;
	if (lct_tp_p->proc_entry_tp != NULL) {
		remove_proc_entry(TP_PROXIMITY_SWITCH_NAME, NULL);
		lct_tp_p->proc_entry_tp = NULL;
		TP_LOGW("remove /proc/%s\n", TP_PROXIMITY_SWITCH_NAME);
	}
	kfree(lct_tp_p);
	tp_proximity_probe = false;
	return;
}
EXPORT_SYMBOL(uninit_lct_tp_proximity_switch);

void set_lct_tp_proximity_switch_status(bool en)
{
    int ret = 0;
    if (!tp_proximity_probe) {
        TP_LOGW("tp probe Failed to load successfully ");
        return;
    }
    if (en) {
        lct_tp_p->enable_tp_proximity_switch_flag = 1;
        ret = lct_tp_p->pfun(true);
		if (ret) {
            TP_LOGW("Enable Proimity Failed! ret=%d\n", ret);
			return;
		}
    } else {
        lct_tp_p->enable_tp_proximity_switch_flag = 0;
        ret = lct_tp_p->pfun(false);
		if (ret) {
            TP_LOGW("disable Proimity Failed! ret=%d\n", ret);
			return;
		}
    }
    TP_LOGW("set_lct_tp_proximity_switch_status en=%d\n", en);

}
EXPORT_SYMBOL(set_lct_tp_proximity_switch_status);

bool get_lct_tp_proximity_switch_status(void)
{
	return lct_tp_p->enable_tp_proximity_switch_flag;
}
EXPORT_SYMBOL(get_lct_tp_proximity_switch_status);

static int lct_creat_proc_tp_entry(void)
{
	lct_tp_p->proc_entry_tp = proc_create_data(TP_PROXIMITY_SWITCH_NAME, 0444, NULL, &lct_proc_tp_proximity_switch_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp)) {
		TP_LOGE("add /proc/tp_proximity_switch error \n");
		return -1;
	}
	TP_LOGW("/proc/tp_proximity_switch is okay!\n");

	return 0;
}

static ssize_t lct_proc_tp_proximity_switch_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	ssize_t cnt=0;
	char *page = NULL;

	if (*ppos)
		return 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;

	cnt = sprintf(page, "%s", (lct_tp_p->enable_tp_proximity_switch_flag ? "1\n" : "0\n"));

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_LOGW("Touchpad Proimity status : %s", page);

	kfree(page);
	return cnt;
}

static ssize_t lct_proc_tp_proximity_switch_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int ret;
	ssize_t cnt = 0;
	char *page = NULL;
	unsigned int input = 0;

	page = kzalloc(128, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return -ENOMEM;
	cnt = simple_write_to_buffer(page, 128, ppos, buf, size);
	if (cnt <= 0)
		return -EINVAL;
	if (sscanf(page, "%u", &input) != 1)
		return -EINVAL;

	if (input > 0) {
		if (lct_tp_p->enable_tp_proximity_switch_flag)
			goto exit;
		TP_LOGW("Enbale Proimity Touchpad ...\n");
		ret = lct_tp_p->pfun(true);
		if (ret) {
			TP_LOGW("Enable Touchpad Proimity Failed! ret=%d\n", ret);
			goto exit;
		}
		lct_tp_p->enable_tp_proximity_switch_flag = true;
	} else {
		if (!lct_tp_p->enable_tp_proximity_switch_flag)
			goto exit;
		TP_LOGW("Disable Touchpad Proimity ...\n");
		ret = lct_tp_p->pfun(false);
		if (ret) {
			TP_LOGW("Disable Touchpad Proimity Failed! ret=%d\n", ret);
			goto exit;
		}
		lct_tp_p->enable_tp_proximity_switch_flag = false;
	}
	TP_LOGW("Set Touchpad Proimity successfully!\n");

exit:
	kfree(page);
	return cnt;
}

MODULE_DESCRIPTION("Touchpad Work Contoller Driver");
MODULE_LICENSE("GPL");


