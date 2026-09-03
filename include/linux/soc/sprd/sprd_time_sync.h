/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SPRD_TIME_SYNC_H
#define _SPRD_TIME_SYNC_H

#include <linux/notifier.h>

struct sprd_time_sync_init_data {
	char *name;
	u64 t_monotime_ns;
	u64 t_boottime_ns;
	u64 t_realtime_s;
	u64 t_ts_cnt;
	u64 t_sysfrt_cnt;
	int tz_minuteswest;
	int tz_dsttime;
};

struct sprd_time_sync_device {
	struct sprd_time_sync_init_data *init;
	int major;
	int minor;
	struct cdev cdev;
	struct device *dev;
};
#if IS_ENABLED(CONFIG_SPRD_TIME_SYNC)
extern int sprd_send_ap_time(void);
#else
static inline int sprd_send_ap_time(void) { return 0; }
#endif

#endif /* _SPRD_TIME_SYNC_H */
