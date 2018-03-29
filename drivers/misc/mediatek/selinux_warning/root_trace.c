/*
* Copyright (C) 2015 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/file.h>

/* strcut to keep uid0 change , from none-root to root. */
struct uid0_change_struct {
	atomic_t to_root_count;
	atomic_t old_ruid;
	atomic_t old_suid;
	atomic_t old_euid;
} uid0_trace;


static struct platform_driver root_trace = {
	.driver = {
		   .name = "root_trace",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   }
};

#define MAX_PATH        (256)
/* the exclusive list for root trace, below is an example, please configure your own list*/
const char *exclusive_list[] = {
	"/system/bin/app_process32",
	"/system/bin/app_process64",
	"/system/bin/pppd"
};

/*
*  by default not to traverse exclusive list
*  0 : not to traverse exclusive list
*  1 : traverse exclusive list
*/
int traverse_exclusive_list = 0;


static ssize_t root_trace_show(struct device_driver *driver, char *buf)
{

	char *ptr = buf;

	sprintf(ptr, "%d\nold_ruid:%d\nold_euid:%d\nold_suid:%d\n",
		atomic_read(&uid0_trace.to_root_count), atomic_read(&uid0_trace.old_ruid),
		atomic_read(&uid0_trace.old_euid), atomic_read(&uid0_trace.old_suid));

	return strlen(buf);
}


DRIVER_ATTR(root_trace, 0444, root_trace_show, NULL);


static int __init root_trace_init(void)
{

	int ret = 0;

	/* register driver and create sysfs files */
	ret = driver_register(&root_trace.driver);

	if (ret) {
		pr_warn("[%s] fail to register root_trace driver\n", __func__);
		return -1;
	}

	ret = driver_create_file(&root_trace.driver, &driver_attr_root_trace);

	if (ret) {
		pr_warn("[%s] fail to create root_trace sysfs file\n", __func__);
		driver_unregister(&root_trace.driver);
		return -1;
	}

	return 0;
}


int sec_trace_root(const struct cred *old, const struct cred *new)
{

	int ret = 0;
	kuid_t root_uid = make_kuid(old->user_ns, 0);
	char *pathname = NULL;

	if ((!new) || (!old))
		goto _exit;


	if ((uid_eq(new->euid, root_uid) && uid_gt(old->euid, root_uid)) ||
	    (uid_eq(new->uid, root_uid) && uid_gt(old->uid, root_uid)) ||
	    (uid_eq(new->suid, root_uid) && uid_gt(old->suid, root_uid))) {

		if (traverse_exclusive_list) {
			/* traverse exclusive list for not tracking root events */
			int i = 0;
			char *exec_path = NULL;
			struct mm_struct *pmm;

			pmm = current->mm;

			if (!pmm)
				goto _exit;

			if (pmm->exe_file) {
				pathname = kmalloc(MAX_PATH, GFP_KERNEL);
				if (!pathname) {
					pr_warn("[%s] fail to kmalloc !\n", __func__);
					ret = -ENOMEM;
					goto _exit;
				}
				exec_path = d_path(&pmm->exe_file->f_path, pathname, MAX_PATH);

				for (i = 0; i < ARRAY_SIZE(exclusive_list); i++) {
					if (!strcmp(exclusive_list[i], exec_path)) {
						/* found match in exclusive list, return immediately */
						pr_warn
						    (" bypass root trace - old ruid:%d euid:%d suid%d\n",
						     __kuid_val(old->uid), __kuid_val(old->euid),
						     __kuid_val(old->suid));
						goto _exit;
					}
				}
			}
		}

		atomic_inc(&uid0_trace.to_root_count);
		atomic_set(&uid0_trace.old_ruid, __kuid_val(old->uid));
		atomic_set(&uid0_trace.old_euid, __kuid_val(old->euid));
		atomic_set(&uid0_trace.old_suid, __kuid_val(old->suid));
	}
_exit:
	kfree(pathname);
	return ret;
}
EXPORT_SYMBOL(sec_trace_root);
module_init(root_trace_init);
