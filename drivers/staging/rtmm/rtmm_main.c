
#define pr_fmt(fmt)  "rtmm : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sysfs.h>

#include "rtmm.h"

static int __init rtmm_init(void)
{
	struct kobject *rtmm_kobj;

	rtmm_kobj = kobject_create_and_add("rtmm", mm_kobj);
	if (unlikely(!rtmm_kobj)) {
		pr_err("failed to initialize the sysfs interface of rtmm\n");
		return -ENOMEM;
	}

	rtmm_reclaim_init(rtmm_kobj);

	pr_info("rtmm init OK\n");

	return 0;
}
subsys_initcall(rtmm_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Cai Liu <liucai@xiaomi.com>");
