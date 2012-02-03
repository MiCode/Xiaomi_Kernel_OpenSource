/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MSM_RTB_H__
#define __MSM_RTB_H__

enum logk_event_type {
	LOGK_NONE = 0,
	LOGK_READL,
	LOGK_WRITEL,
	LOGK_OTHER,
};

#if defined(CONFIG_MSM_RTB)
/*
 * returns 1 if data was logged, 0 otherwise
 */
int uncached_logk_pc(enum logk_event_type log_type, void *caller,
				void *data);

/*
 * returns 1 if data was logged, 0 otherwise
 */
int uncached_logk(enum logk_event_type log_type, void *data);

#else

static inline int uncached_logk_pc(enum logk_event_type log_type,
					void *caller,
					void *data) { return 0; }

static inline int uncached_logk(enum logk_event_type log_type,
					void *data) { return 0; }
#endif
#endif
