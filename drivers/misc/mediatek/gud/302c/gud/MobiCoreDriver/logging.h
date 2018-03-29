/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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

/* MobiCore internal trace buffer structure. */
struct mc_trace_buf {
	uint32_t version; /* version of trace buffer */
	uint32_t length; /* length of allocated buffer(includes header) */
	uint32_t write_pos; /* last write position */
	char  buff[1]; /* start of the log buffer */
};

/* MobiCore internal trace log setup. */
void mobicore_log_read(void);
long mobicore_log_setup(void);
void mobicore_log_free(void);

#endif /* _MC_LOGGING_H_ */
