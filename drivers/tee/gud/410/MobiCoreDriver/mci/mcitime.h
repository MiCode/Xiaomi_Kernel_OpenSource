/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef MCITIME_H_
#define MCITIME_H_

/*
 * Trustonic TEE RICH OS Time:
 * -seconds and nanoseconds since Jan 1, 1970, UTC
 * -monotonic counter
 */
struct mcp_time {
	u64	wall_clock_seconds;
	u64	wall_clock_nsec;
	u64	monotonic_seconds;
	u64	monotonic_nsec;
};

#endif /* MCITIME_H_ */
