/****************************************************************************************
 *
 * @File Name   : lct_tp_grip_area.c
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
#define TP_GRIP_AREA_NAME          "tp_grip_area"
#define TP_GRIP_AREA_LOG_ENABLE
#define TP_GRIP_AREA_TAG           "LCT_TP_GRIP_AREA"

#ifdef TP_GRIP_AREA_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_GRIP_AREA_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_GRIP_AREA_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

typedef int (*get_screen_angle_callback) (void);
typedef int (*set_screen_angle_callback) (int angle);

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef struct lct_tp {
	struct kobject *tp_device;
	int screen_angle;
	struct proc_dir_entry *proc_entry_tp_grip_area;
	set_screen_angle_callback lct_grip_area_set_screen_angle_callback;
	get_screen_angle_callback lct_grip_area_get_screen_angle_callback;
} lct_tp_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
static lct_tp_t *lct_tp_p;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
// --- proc ---
static int lct_proc_tp_grip_area_open(struct inode *node, struct file *file);
static ssize_t lct_proc_tp_grip_area_read(struct file *file, char __user *buf,
					  size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_grip_area_write(struct file *file,
					   const char __user *buf, size_t size,
					   loff_t *ppos);
static const struct file_operations lct_proc_tp_grip_area_fops = {
	.open = lct_proc_tp_grip_area_open,
	.read = lct_proc_tp_grip_area_read,
	.write = lct_proc_tp_grip_area_write,
};

int init_lct_tp_grip_area(set_screen_angle_callback set_fun,
			  get_screen_angle_callback get_fun)
{
	if (NULL == set_fun)
		return -EPERM;

	TP_LOGW("Initialization /proc/%s node!\n", TP_GRIP_AREA_NAME);
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)) {
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}

	lct_tp_p->lct_grip_area_set_screen_angle_callback = set_fun;
	lct_tp_p->lct_grip_area_get_screen_angle_callback = get_fun;

	lct_tp_p->proc_entry_tp_grip_area =
	    proc_create_data(TP_GRIP_AREA_NAME, 0444, NULL,
			     &lct_proc_tp_grip_area_fops, NULL);
	if (IS_ERR_OR_NULL(lct_tp_p->proc_entry_tp_grip_area)) {
		TP_LOGE("add /proc/%s error\n", TP_GRIP_AREA_NAME);
		kfree(lct_tp_p);
		return -EPERM;
	}
	TP_LOGW("/proc/%s is okay!\n", TP_GRIP_AREA_NAME);

	return 0;
}

EXPORT_SYMBOL(init_lct_tp_grip_area);

void uninit_lct_tp_grip_area(void)
{
	TP_LOGW("uninit /proc/%s ...\n", TP_GRIP_AREA_NAME);

	if (IS_ERR_OR_NULL(lct_tp_p))
		return;

	if (lct_tp_p->proc_entry_tp_grip_area != NULL) {
		remove_proc_entry(TP_GRIP_AREA_NAME, NULL);
		lct_tp_p->proc_entry_tp_grip_area = NULL;
		TP_LOGW("remove /proc/%s\n", TP_GRIP_AREA_NAME);
	}

	kfree(lct_tp_p);
}

EXPORT_SYMBOL(uninit_lct_tp_grip_area);

int set_tp_grip_area_angle(int screen_angle)
{
	lct_tp_p->screen_angle = screen_angle;
	return 0;
}

EXPORT_SYMBOL(set_tp_grip_area_angle);

int get_tp_grip_area_angle(void)
{
	return lct_tp_p->screen_angle;
}

EXPORT_SYMBOL(get_tp_grip_area_angle);

static int lct_proc_tp_grip_area_open(struct inode *node, struct file *file)
{
	return 0;
}

static ssize_t lct_proc_tp_grip_area_read(struct file *file, char __user *buf,
					  size_t size, loff_t *ppos)
{
	int ret = -EIO;
	ssize_t cnt = 0;
	char *page = NULL;

	if (*ppos)
		return 0;

	page = kzalloc(size + 1, GFP_KERNEL);
	if (IS_ERR_OR_NULL(page))
		return ret;

	if (NULL != lct_tp_p->lct_grip_area_get_screen_angle_callback) {
		ret = lct_tp_p->lct_grip_area_get_screen_angle_callback();
		if (ret < 0) {
			TP_LOGE("get screen angle failed!\n");
			goto out;
		}
		lct_tp_p->screen_angle = ret;
	}
	cnt = snprintf(page, PAGE_SIZE, "%d\n", lct_tp_p->screen_angle);
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	if (*ppos != cnt)
		*ppos = cnt;
	TP_LOGW("screen_angle = %s", page);
	ret = cnt;

out:
	kfree(page);
	return ret;
}

static ssize_t lct_proc_tp_grip_area_write(struct file *file,
					   const char __user *buf, size_t size,
					   loff_t *ppos)
{
	int ret = -EIO;
	ssize_t cnt = 0;
	char *cmd = NULL;
	int angle = 0;

	cmd = kzalloc(size + 1, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cmd))
		return -ENOMEM;

	cnt = simple_write_to_buffer(cmd, size + 1, ppos, buf, size);
	if (cnt <= 0)
		goto out;

	cmd[size] = '\0';
	if (sscanf(cmd, "%d", &angle) < 0)
		goto out;

	if (angle < 0 || angle > 360) {
		TP_LOGE("screen_angle range: 0 ~ 360 !\n");
		goto out;
	}
	TP_LOGW("Set screen angle = %u\n", angle);

	if (NULL == lct_tp_p->lct_grip_area_set_screen_angle_callback) {
		TP_LOGE("none callback!\n");
		goto out;
	}
	ret = lct_tp_p->lct_grip_area_set_screen_angle_callback(angle);
	if (ret < 0) {
		TP_LOGE("Set screen angle failed! ret = %d\n", ret);
		goto out;
	}
	lct_tp_p->screen_angle = angle;
	ret = cnt;

out:
	kfree(cmd);
	return ret;
}

MODULE_DESCRIPTION("Touchpad Grip Area Driver");
MODULE_LICENSE("GPL");
