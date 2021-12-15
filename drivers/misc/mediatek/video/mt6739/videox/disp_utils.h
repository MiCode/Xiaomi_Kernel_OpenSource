/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __H_DISP_UTILS__
#define __H_DISP_UTILS__

#include <linux/mutex.h>

#define __my_wait_event_interruptible_timeout(wq, ret)                         \
	do {                                                                   \
		DEFINE_WAIT(__wait);                                           \
		prepare_to_wait(&wq, &__wait, TASK_INTERRUPTIBLE);             \
		if (!signal_pending(current)) {                                \
			ret = schedule_timeout(ret);                           \
			if (!ret)                                              \
				break;                                         \
		}                                                              \
		ret = -ERESTARTSYS;                                            \
		break;                                                         \
		finish_wait(&wq, &__wait);                                     \
	} while (0)

int disp_sw_mutex_lock(struct mutex *m);
int disp_mutex_trylock(struct mutex *m);
int disp_sw_mutex_unlock(struct mutex *m);
int disp_msleep(unsigned int ms);
long int disp_get_time_us(void);

#endif
