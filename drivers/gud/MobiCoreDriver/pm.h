/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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

#include "main.h"

/* How long after resume the daemon should backoff */
#define DAEMON_BACKOFF_TIME	500

#ifdef MC_PM_RUNTIME
/* Initialize Power Management */
int mc_pm_initialize(void);
/* Free all Power Management resources*/
int mc_pm_free(void);
/* Test if sleep is possible */
bool mc_pm_sleep_ready(void);
#else
static inline int mc_pm_initialize(void)
{
	return 0;
}

static inline int mc_pm_free(void)
{
	return 0;
}

static inline bool mc_pm_sleep_ready(void)
{
	return true;
}
#endif

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT

/* Initialize secure crypto clocks */
int mc_pm_clock_initialize(void);
/* Free secure crypto clocks */
void mc_pm_clock_finalize(void);
/* Enable secure crypto clocks */
int mc_pm_clock_enable(void);
/* Disable secure crypto clocks */
void mc_pm_clock_disable(void);

#else

static inline
int mc_pm_clock_initialize(void)
{
	return 0;
}

static inline
void mc_pm_clock_finalize(void)
{ }

static inline
int mc_pm_clock_enable(void)
{
	return 0;
}

static inline
void mc_pm_clock_disable(void)
{ }

#endif

#endif /* _MC_PM_H_ */
