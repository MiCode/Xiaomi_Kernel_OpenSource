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
#ifndef _MC_LOGGING_H_
#define _MC_LOGGING_H_

/* MobiCore internal trace log setup. */
#ifdef MC_MEM_TRACES
void mobicore_log_read(void);
int  mobicore_log_setup(void);
void mobicore_log_free(void);
#else
static inline void mobicore_log_read(void)
{ }

static inline long mobicore_log_setup(void)
{
	return 0;
}

static inline void mobicore_log_free(void)
{ }
#endif

#endif /* _MC_LOGGING_H_ */
