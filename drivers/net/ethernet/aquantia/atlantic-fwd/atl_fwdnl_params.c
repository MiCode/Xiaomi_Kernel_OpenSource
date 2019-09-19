/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <linux/workqueue.h>

#include "atl_common.h"

unsigned int atlfwd_nl_tx_clean_threshold_msec = 200;
unsigned int atlfwd_nl_tx_clean_threshold_frac = 8;
bool atlfwd_nl_speculative_queue_stop = true;

static void force_tx_housekeeping(void)
{
	struct net_device *ndev;
	struct atl_nic *nic;
	struct net *ns;

	rcu_read_lock();
	for_each_net_rcu (ns) {
		for_each_netdev_rcu (ns, ndev) {
			if (!is_atlfwd_device(ndev))
				continue;

			nic = netdev_priv(ndev);
			/* Schedule for ASAP execution */
			schedule_delayed_work(nic->fwdnl.tx_cleanup_wq, 0);
		}
	}
	rcu_read_unlock();
}

static int tx_clean_threshold_msec_set(const char *val,
				       const struct kernel_param *kp)
{
	unsigned int msec;
	int ret = kstrtouint(val, 10, &msec);

	if (unlikely(ret != 0))
		return -EINVAL;

	if (msec != 0 && atlfwd_nl_tx_clean_threshold_msec == 0) {
		/* Force extra TX housekeeping, if threshold is re-enabled */
		force_tx_housekeeping();
	}

	return param_set_uint(val, kp);
}

static const struct kernel_param_ops fwdnl_tx_clean_threshold_msec_ops = {
	.set = tx_clean_threshold_msec_set,
	.get = param_get_uint,
};

static int tx_clean_threshold_frac_set(const char *val,
				       const struct kernel_param *kp)
{
	unsigned int frac;
	int ret = kstrtouint(val, 10, &frac);

	if (unlikely(ret != 0))
		return -EINVAL;

	if (frac != 0 && atlfwd_nl_tx_clean_threshold_frac == 0) {
		/* Force extra TX housekeeping, if threshold is re-enabled */
		force_tx_housekeeping();
	}

	return param_set_uint(val, kp);
}

static const struct kernel_param_ops fwdnl_tx_clean_threshold_frac_ops = {
	.set = tx_clean_threshold_frac_set,
	.get = param_get_uint,
};

module_param_cb(fwdnl_tx_clean_threshold_msec,
		&fwdnl_tx_clean_threshold_msec_ops,
		&atlfwd_nl_tx_clean_threshold_msec, 0644);
module_param_cb(fwdnl_tx_clean_threshold_frac,
		&fwdnl_tx_clean_threshold_frac_ops,
		&atlfwd_nl_tx_clean_threshold_frac, 0644);
module_param_named(fwdnl_speculative_queue_stop,
		   atlfwd_nl_speculative_queue_stop, bool, 0644);
