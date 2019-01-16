#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/xlog.h>
#include <linux/suspend.h>

#include <mach/mtk_hibernate_dpm.h>
#ifdef CONFIG_MTK_SYSENV
#include <mach/env.h> /* for set_env() by MTK */
#endif

#define HIB_SWSUSP_DEBUG 0
#if (HIB_SWSUSP_DEBUG)
#undef hib_log
#define hib_log(fmt, args...)	pr_debug("[HIB/SwSuspHlper] " fmt, ##args);
#else
#define hib_log(fmt, args...) ((void)0)
#endif
#undef hib_warn
#define hib_warn(fmt, args...)  pr_warn("[HIB/SwSuspHlper] " fmt, ##args);
#undef hib_err
#define hib_err(fmt, args...)   pr_err("[HIB/SwSuspHlper] " fmt, ##args);

#define SWSUSP_HELPER_NAME "swsusp-helper"

/* callback APIs helper function table */
static swsusp_cb_func_info restore_noirq_func_table[MAX_CB_FUNCS];
static int initialized = 0;

int register_swsusp_restore_noirq_func(unsigned int id, swsusp_cb_func_t func,
				       struct device *device)
{
	int ret = 0;
	swsusp_cb_func_info *info_ptr;

	BUG_ON(!initialized);

	if ((id >= MAX_CB_FUNCS) || (func == NULL)) {
		hib_err("register func fail: func_id: %d!\n", id);
		return E_PARAM;
	}
	info_ptr = &(restore_noirq_func_table[id]);
	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
		info_ptr->device = device;
		hib_warn("reg. func %d:0x%p with device %s%p\n",
				 restore_noirq_func_table[id].id,
				 restore_noirq_func_table[id].func,
				 (device == NULL) ? " " : "0x",
				 restore_noirq_func_table[id].device);
	} else
		hib_err("register func fail: func(%d) already registered!\n", id);

	return ret;
}
EXPORT_SYMBOL(register_swsusp_restore_noirq_func);

int unregister_swsusp_restore_noirq_func(unsigned int id)
{
	int ret = 0;
	swsusp_cb_func_info *info_ptr;

	BUG_ON(!initialized);

	if (id >= MAX_CB_FUNCS || id != restore_noirq_func_table[id].id) {
		hib_err("register func fail: func_id: %d!\n", id);
		return E_PARAM;
	}
	info_ptr = &(restore_noirq_func_table[id]);
	if (info_ptr->func != NULL) {
		info_ptr->id = -1;
		info_ptr->func = NULL;
		info_ptr->device = NULL;
	} else
		hib_warn("register func fail: func(%d) already unregistered!\n", id);

	return ret;
}
EXPORT_SYMBOL(unregister_swsusp_restore_noirq_func);

int exec_swsusp_restore_noirq_func(unsigned int id)
{
	swsusp_cb_func_t func;
	struct device *device = NULL;
	int ret = 0;

	BUG_ON(!initialized);

	if (id >= MAX_CB_FUNCS || id != restore_noirq_func_table[id].id) {
		hib_err("exec func fail: invalid func id(%d)!\n", id);
		return E_PARAM;
	}

	func = restore_noirq_func_table[id].func;
	device = restore_noirq_func_table[id].device;
	if (func != NULL) {
		ret = func(device);
	} else {
		ret = E_NO_EXIST;
		hib_warn("exec func fail: func id(%d) not register!\n", id);
	}

	return ret;
}
EXPORT_SYMBOL(exec_swsusp_restore_noirq_func);

static int swsusp_helper_probe(struct platform_device *dev)
{
	hib_log("[%s] enter...\n", __func__);
	return 0;
}

static int swsusp_helper_remove(struct platform_device *dev)
{
	hib_log("[%s] enter...\n", __func__);
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int swsusp_helper_pm_suspend(struct device *device)
{
	hib_log("[%s] enter...\n", __func__);
	return 0;
}

int swsusp_helper_pm_resume(struct device *device)
{
	hib_log("[%s] enter...\n", __func__);
	return 0;
}

int swsusp_helper_pm_restore_noirq(struct device *device)
{
	int id, ret = 0, retall = 0;

	hib_log("[%s] enter...\n", __func__);

	BUG_ON(!initialized);

	for (id = ID_M_BEGIN; id < ID_M_END; id++) {
		if (restore_noirq_func_table[id].func != NULL) {
			hib_warn("exec func %d:0x%p !\n", restore_noirq_func_table[id].id, restore_noirq_func_table[id].func);
			if (id != restore_noirq_func_table[id].id) {
				hib_err("exec func fail: func id miss-matched (%d/%d) !\n", id, restore_noirq_func_table[id].id);
				continue;
			}
			ret =
			    restore_noirq_func_table[id].func(restore_noirq_func_table[id].device);
			if (ret != 0) {
				hib_warn("exec func fail: func id(%d), err code %d !\n",
					 restore_noirq_func_table[id].id, ret);
				retall = ret;
			}
		}
	}

	return retall;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define swsusp_helper_pm_suspend NULL
#define swsusp_helper_pm_resume  NULL
#define swsusp_helper_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
struct dev_pm_ops swsusp_helper_pm_ops = {
	.suspend = swsusp_helper_pm_suspend,
	.resume = swsusp_helper_pm_resume,
	.freeze = swsusp_helper_pm_suspend,
	.thaw = swsusp_helper_pm_resume,
	.poweroff = swsusp_helper_pm_suspend,
	.restore = swsusp_helper_pm_resume,
	.restore_noirq = swsusp_helper_pm_restore_noirq,
};

struct platform_device swsusp_helper_device = {
	.name = SWSUSP_HELPER_NAME,
	.id = -1,
	.dev = {},
};

static struct platform_driver swsusp_helper_driver = {
	.driver = {
		   .name = SWSUSP_HELPER_NAME,
#ifdef CONFIG_PM
		   .pm = &swsusp_helper_pm_ops,
#endif
		   .owner = THIS_MODULE,
	},
	.probe = swsusp_helper_probe,
	.remove = swsusp_helper_remove,
};

static int swsusp_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch(pm_event) {
	case PM_HIBERNATION_PREPARE: /* Going to hibernate */
#ifdef CONFIG_MTK_SYSENV
		/* for lk */
		set_env("hibboot", "1");
#endif
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION: /* Hibernation finished */
#ifdef CONFIG_MTK_SYSENV
		/* from lk */
		hib_log("hibboot = %s\n", get_env("hibboot"));
		set_env("hibboot", "0");
#endif
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block swsusp_pm_notifier_block = {
	.notifier_call = swsusp_pm_event,
	.priority = 0,
};

static int __init swsusp_helper_init(void)
{
	int ret;

	hib_log("[%s] enter...\n", __func__);

	/* init restore_noirq callback function table */
	memset((void *)restore_noirq_func_table, 0, sizeof(restore_noirq_func_table));

	ret = platform_device_register(&swsusp_helper_device);
	if (ret) {
		hib_err("swsusp_helper_device register fail(%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&swsusp_helper_driver);
	if (ret) {
		hib_err("swsusp_helper_driver register fail(%d)\n", ret);
		return ret;
	}

	ret = register_pm_notifier(&swsusp_pm_notifier_block);
	if (ret)
		hib_err("failed to register PM notifier %d\n", ret);

	initialized = 1;
	return 0;
}
arch_initcall(swsusp_helper_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("The swsusp helper function");
