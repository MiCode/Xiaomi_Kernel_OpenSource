#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include "lct_tp_fm_info.h"

static struct kobject *msm_tp_device;
static u16 tp_ver_show;
static char tp_ver_show_str[80] = {0x00};
static char module_name[80] = {0x00};

static ssize_t msm_tp_module_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	char tp_version[128];

	memset(tp_version, 0, sizeof(tp_version));

	if ((0 == tp_ver_show) && (0 == strlen(tp_ver_show_str)))
		strcpy(tp_version, "no tp");
	else if (0 == tp_ver_show) {
		sprintf(tp_version, "[Vendor]%s, %s\n", (strlen(module_name) ? module_name : "Unknown"),
				(strlen(tp_ver_show_str) ? tp_ver_show_str : "Unknown product"));
	    } else {
		sprintf(tp_version, "[Vendor]%s, %s\nFW: V%03d(0x%04x)\n",
			(strlen(module_name) ? module_name : "Unknown"),
			(strlen(tp_ver_show_str) ? tp_ver_show_str : "Unknown product"),
			tp_ver_show, tp_ver_show);
	}

	sprintf(buf, "%s\n", tp_version);
	rc = strlen(buf) + 1;

	return rc;
}

static DEVICE_ATTR(tp_info, 0444, msm_tp_module_id_show, NULL);
static int tp_fm_creat_sys_entry(void)
{
	int32_t rc = 0;

	msm_tp_device = kobject_create_and_add("android_tp", NULL);
	if (msm_tp_device == NULL) {
		pr_info("%s: subsystem_register failed\n", __func__);
		rc = -ENOMEM;
		return rc ;
	}
	rc = sysfs_create_file(msm_tp_device, &dev_attr_tp_info.attr);
	if (rc) {
		pr_info("%s: sysfs_create_file failed\n", __func__);
		kobject_del(msm_tp_device);
	}

	return 0 ;
}

static ssize_t tp_proc_tp_info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt = 0;
	char *page = NULL;

	page = kzalloc(128, GFP_KERNEL);

	if ((0 == strlen(module_name)) && (0 == tp_ver_show) && (0 == strlen(tp_ver_show_str)))
		cnt = sprintf(page, "no tp\n");
	else {
		cnt = sprintf(page, "[Vendor]%s, %s\nFW: V%03d\n", (strlen(module_name) ? module_name : "Unknown"),
				(strlen(tp_ver_show_str) ? tp_ver_show_str : "Unknown product"),
				tp_ver_show);
	}

	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);

	kfree(page);
	return cnt;
}

static const struct file_operations tp_proc_tp_info_fops = {
	.read		= tp_proc_tp_info_read,
};

static int tp_fm_creat_proc_entry(void)
{
	struct proc_dir_entry *proc_entry_tp;

	proc_entry_tp = proc_create_data("tp_info", 0444, NULL, &tp_proc_tp_info_fops, NULL);
	if (IS_ERR_OR_NULL(proc_entry_tp))
		pr_err("add /proc/tp_info error \n");

	return 0;
}

int init_tp_fm_info(u16 version_info_num, char *version_info_str, char *name)
{
	tp_ver_show = version_info_num;

	if (NULL != version_info_str)
		strcpy(tp_ver_show_str, version_info_str);
	if (NULL != name)
		strcpy(module_name, name);

	tp_fm_creat_sys_entry();
	tp_fm_creat_proc_entry();

	return 0;
}



MODULE_DESCRIPTION("GTP Series Driver");
MODULE_LICENSE("GPL");
