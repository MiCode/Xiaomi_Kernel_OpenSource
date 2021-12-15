/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_GAUGE_TIME_SERVICE_INTF_H__
#define __MTK_GAUGE_TIME_SERVICE_INTF_H__

struct gtimer {
	char *name;
	struct device *dev;
	struct timespec endtime;
	int interval;

	int (*callback)(struct gtimer *gt);
	struct list_head list;
};

extern void gtimer_init(struct gtimer *timer, struct device *dev, char *name);
extern void gtimer_start(struct gtimer *timer, int sec);
extern void gtimer_stop(struct gtimer *timer);
extern void gtimer_dump_list(void);
extern void gtimer_set_log_level(int x);


#endif /* __MTK_GAUGE_TIME_SERVICE_INTF_H__ */
