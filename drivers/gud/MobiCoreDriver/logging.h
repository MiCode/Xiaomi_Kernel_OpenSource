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
#ifndef _MC_LOGGING_H_
#define _MC_LOGGING_H_

#include "platform.h"	/* CONFIG_TRUSTONIC_TEE_NO_TRACES */

/* MobiCore internal trace log setup. */
#ifndef CONFIG_TRUSTONIC_TEE_NO_TRACES
void mc_logging_run(void);
int  mc_logging_init(void);
void mc_logging_exit(void);
int mc_logging_start(void);
void mc_logging_stop(void);
#else /* !CONFIG_TRUSTONIC_TEE_NO_TRACES */
static inline void mc_logging_run(void)
{
}

static inline long mc_logging_init(void)
{
	return 0;
}

static inline void mc_logging_exit(void)
{
}

static inline int mc_logging_start(void)
{
	return 0;
}

static inline void mc_logging_stop(void)
{
}

#endif /* CONFIG_TRUSTONIC_TEE_NO_TRACES */

#endif /* _MC_LOGGING_H_ */
