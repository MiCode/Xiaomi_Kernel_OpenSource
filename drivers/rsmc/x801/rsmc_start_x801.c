//#include <securec.h>

//#include <cetc_platform/log/cetc_log.h>
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

#include "rsmc_msg_loop_x801.h"

#ifdef CETCLOG_TAG
#undef CETCLOG_TAG
#endif
#define CETCLOG_TAG RSMC_START_X801


int rsmc_start_x801(void)
{
	int ret;
	printk(KERN_INFO"%s: enter", __func__);
	ret = rsmc_init_x801();
	if (ret != 0) {
		printk(KERN_INFO"%s: enable success!", __func__);
		return -EINVAL;
	}
	return 0;
}

void rsmc_stop_x801(void)
{
	printk(KERN_INFO"%s: enter", __func__);
	rsmc_exit_x801();
}
