/*
 * Copyright (C) 2015-2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>

#define SEQ_printf(m, x...)\
	do {\
		if (m)\
			seq_printf(m, x);\
		else\
			pr_debug(x);\
	} while (0)
#undef TAG
#define TAG "[SMART]"

#define S_LOG(fmt, args...)		pr_debug(TAG fmt, ##args)

/*--------------------------------------------*/


struct smart_context {
	struct input_dev   *idev;
	struct miscdevice   mdev;
	struct mutex        s_op_mutex;
};

struct smart_context *smart_context_obj;

int set_cpuset(int cluster)
{
	int ret;
	char event_ll[10] = "DETECT=11"; /* HPS*/
	char event_l[10] = "DETECT=10"; /* HPS*/
	char event_all[10] = "DETECT=12"; /* HPS*/
	char event_act_up[9]   = "ACTION=1"; /* up   */
	char *envp_clu0[3] = { event_ll, event_act_up, NULL };
	char *envp_clu1[3] = { event_l, event_act_up, NULL };
	char *envp_cluall[3] = { event_all, event_act_up, NULL };
	char **envp;

	switch (cluster) {
	case 0:
		/* send 0-3 */
		envp = &envp_clu0[0];
		break;

	case 1:
		/* send 4-7 */
		envp = &envp_clu1[0];
		break;

	case -1:
		/* send 0-7 */
		envp = &envp_cluall[0];
		break;

	default:
		return -EINVAL;
	}

	if (!smart_context_obj)
		return 0;

	ret = kobject_uevent_env(&smart_context_obj->mdev.this_device->kobj,
				 KOBJ_CHANGE, envp);
	if (ret) {
		pr_debug(TAG"kobject_uevent error:%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

/*--------------------INIT------------------------*/
static int init_smart_obj(void)
{
	int ret;

	/* dev init */
	struct smart_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

#if 0
	if (!obj) {
		pr_debug(TAG"kzalloc error\n");
		return -1;
	}
#endif

	mutex_init(&obj->s_op_mutex);
	smart_context_obj = obj;
	smart_context_obj->mdev.minor = MISC_DYNAMIC_MINOR;
	smart_context_obj->mdev.name = "m_smart_misc";
	ret = misc_register(&smart_context_obj->mdev);
	if (ret) {
		pr_debug(TAG"misc_register error:%d\n", ret);
		return -2;
	}

#if 0
	ret = sysfs_create_group(&smart_context_obj->mdev.this_device->kobj, &smart_attribute_group);
	if (ret < 0) {
		pr_debug(TAG"sysfs_create_group error:%d\n", ret);
		return -3;
	}
#endif
	ret = kobject_uevent(&smart_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	if (ret) {
		pr_debug(TAG"kobject_uevent error:%d\n", ret);
		return -4;
	}

	return 0;
}

static
int __init init_smart(void)
{
	pr_debug(TAG"init smart driver start\n");
	/* dev init */
	init_smart_obj();

	pr_debug(TAG"init smart driver done\n");

	return 0;
}
late_initcall(init_smart);

