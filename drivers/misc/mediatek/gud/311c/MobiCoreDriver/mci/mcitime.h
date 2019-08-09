/*
 * Copyright (c) 2015 TRUSTONIC LIMITED
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

#ifndef MCITIME_H_
#define MCITIME_H_

/*
 * Trustonic TEE RICH OS Time:
 * Seconds and nanoseconds since Jan 1, 1970, UTC
 */
struct mcp_time {
	u64	seconds;
	u64	nsec;
};

#endif /* MCITIME_H_ */
