/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_PM_H_
#define _MC_PM_H_

#include "platform.h"	/* MC_BL_NOTIFIER */

#ifdef MC_BL_NOTIFIER
/* Initialize Power Management */
int mc_pm_start(void);
/* Free all Power Management resources*/
void mc_pm_stop(void);
#else
static inline int mc_pm_start(void)
{
	return 0;
}

static inline void mc_pm_stop(void)
{
}
#endif

#endif /* _MC_PM_H_ */
