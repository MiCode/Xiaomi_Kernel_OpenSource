//#include <securec.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/semaphore.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <uapi/linux/netlink.h>

#include "x801/rsmc_start_x801.h"

#ifdef CETCLOG_TAG
#undef CETCLOG_TAG
#endif
#define CETCLOG_TAG RSMC_CHIPTYPE_SELECT

MODULE_LICENSE("GPL");

#ifdef NETLINK_RSMC
#undef NETLINK_RSMC
#endif

#define DTS_NODE_CETC_RSMC "xiaomi_rsmc"
#define DTS_PROP_RSMC_ENABLE_X801 "rsmc_enable_x801"

bool rsmc_enable_detect(void)
{
	int ret;
	struct device_node *node = of_find_compatible_node(NULL, NULL, DTS_NODE_CETC_RSMC);
	if (node == NULL) {
		printk(KERN_ERR"%s: no cetc_rsmc", __func__);
		return false;
	}
	if (of_property_read_bool(node, DTS_PROP_RSMC_ENABLE_X801)) {
		printk(KERN_INFO"%s: rsmc_enable_x801", __func__);
		ret = rsmc_start_x801();
		if (ret == 0) {
			printk(KERN_INFO"%s: rsmc_enable_x801 success", __func__);
			return true;
		} else {
			return false;
		}
	}

	printk(KERN_INFO"%s: rsmc_disable", __func__);
	return false;
}

int rsmc_init_thread(void *data)
{
	printk(KERN_INFO"%s: enter", __func__);

	if (!rsmc_enable_detect()) {
		printk(KERN_INFO"%s: not support rsmc", __func__);
		return -EINVAL;
	}
	return 0;
}

int __init rsmc_init(void)
{
	struct task_struct *task = kthread_run(rsmc_init_thread, NULL, "rsmc_init_thread");
	if (IS_ERR(task)) {
		printk(KERN_ERR"%s: failed to create thread", __func__);
		task = NULL;
		return -EINVAL;
	}
	return 0;
}

void __exit rsmc_exit(void)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL, DTS_NODE_CETC_RSMC);
	printk(KERN_INFO"%s: enter", __func__);
	if (node == NULL) {
		printk(KERN_INFO"%s: not support rsmc", __func__);
		return;
	}
	if (of_property_read_bool(node, DTS_PROP_RSMC_ENABLE_X801)) {
		printk(KERN_INFO"%s: x801 exit", __func__);
		rsmc_stop_x801();
		return;
	}
}

late_initcall(rsmc_init);
module_exit(rsmc_exit);

