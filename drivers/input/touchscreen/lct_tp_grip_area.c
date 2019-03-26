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
#define TP_GRIP_AREA_LOG_ENABLE
#define TP_GRIP_AREA_TAG           "LCT_TP_GRIP_AREA"

#ifdef TP_GRIP_AREA_LOG_ENABLE
#define TP_LOGW(log, ...) printk(KERN_WARNING "[%s] %s (line %d): " log, TP_GRIP_AREA_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define TP_LOGE(log, ...) printk(KERN_ERR "[%s] %s ERROR (line %d): " log, TP_GRIP_AREA_TAG, __func__, __LINE__, ##__VA_ARGS__)
#else
#define TP_LOGW(log, ...) {}
#define TP_LOGE(log, ...) {}
#endif

typedef int (*get_screen_angle_callback)(void);
typedef int (*set_screen_angle_callback)(unsigned int angle);

/*
 * DATA STRUCTURES
 ****************************************************************************************
 */
typedef struct lct_tp{
	struct kobject *tp_device;
	unsigned int screen_angle;
	int (*lct_grip_area_get_screen_angle_callback)(void);
	int (*lct_grip_area_set_screen_angle_callback)(unsigned int angle);
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
static int lct_proc_tp_grip_area_open (struct inode *node, struct file *file);
static ssize_t lct_proc_tp_grip_area_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
static ssize_t lct_proc_tp_grip_area_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos);
static const struct file_operations lct_proc_tp_grip_area_fops = {
	.open		= lct_proc_tp_grip_area_open,
	.read		= lct_proc_tp_grip_area_read,
	.write		= lct_proc_tp_grip_area_write,
};

int init_lct_tp_grip_area(set_screen_angle_callback set_fun, get_screen_angle_callback get_fun)
{
	struct proc_dir_entry *proc_entry_tp;

	if (NULL == set_fun)
		return -1;

	TP_LOGW("Initialization /proc/tp_grip_area node!\n");
	lct_tp_p = kzalloc(sizeof(lct_tp_t), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lct_tp_p)){
		TP_LOGE("kzalloc() request memory failed!\n");
		return -ENOMEM;
	}

	lct_tp_p->lct_grip_area_set_screen_angle_callback = set_fun;
	lct_tp_p->lct_grip_area_get_screen_angle_callback = get_fun;

	proc_entry_tp = proc_create_data("tp_grip_area", 0666, NULL, &lct_proc_tp_grip_area_fops, NULL);
	if (IS_ERR_OR_NULL(proc_entry_tp)) {
		TP_LOGE("add /proc/tp_grip_area error \n");
		kfree(lct_tp_p);
		return -1;
	}
	TP_LOGW("/proc/tp_grip_area is okay!\n");

	return 0;
}
EXPORT_SYMBOL(init_lct_tp_grip_area);

int get_tp_grip_area_angle(void)
{
	return lct_tp_p->screen_angle;
}
EXPORT_SYMBOL(get_tp_grip_area_angle);

static int lct_proc_tp_grip_area_open (struct inode *node, struct file *file)
{
	return 0;
}

static ssize_t lct_proc_tp_grip_area_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int ret=-EIO;
	ssize_t cnt = 0;
	char *page = NULL;

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
	cnt = sprintf(page, "%u\n", lct_tp_p->screen_angle);
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	TP_LOGW("screen_angle = %s", page);
	ret = cnt;

out:
	kfree(page);
	return ret;
}

static ssize_t lct_proc_tp_grip_area_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int ret = -EIO;
	ssize_t cnt = 0;
	char *cmd = NULL;
	unsigned int angle = 0;

	cmd = kzalloc(size + 1, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cmd))
		return -ENOMEM;

	cnt = simple_write_to_buffer(cmd, size + 1, ppos, buf, size);
	if (cnt <= 0)
		goto out;

	cmd[size] = '\0';
	if (sscanf(cmd, "%u", &angle) < 0)
		goto out;

	if (angle > 360) {
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
