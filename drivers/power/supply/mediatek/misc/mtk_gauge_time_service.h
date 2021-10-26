/*
 * Copyright (C) 2016 MediaTek Inc.
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
