/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2017 TRUSTONIC LIMITED
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

#ifndef MCITIME_H
#define MCITIME_H

/** Trustonic Timer structure:
 * -seconds and nanoseconds since Jan 1, 1970, UTC
 * -monotonic counter
 */
struct time_t {
	u64 wall_clock_seconds;
	u64 wall_clock_nsec;
	u64 monotonic_seconds;
	u64 monotonic_nsec;
};

/** Trustonic TEE RICH OS Time:
 * -Trustonic Timer structure
 * -sync_timer purpose chose the last update timer
 */
struct mcp_time {
	struct time_t nwd_time[2];
	u32 sync_timer;
};

#endif /* MCITIME_H */
