/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DYNAMIC_BOOST_H__
#define __DYNAMIC_BOOST_H__

enum mode {
	PRIO_TWO_LITTLES,
	PRIO_TWO_LITTLES_MAX_FREQ,
	PRIO_ONE_BIG,
	PRIO_ONE_BIG_MAX_FREQ,
	PRIO_ONE_BIG_ONE_LITTLE,
	PRIO_ONE_BIG_ONE_LITTLE_MAX_FREQ,
	PRIO_TWO_BIGS,
	PRIO_TWO_BIGS_MAX_FREQ,
	PRIO_FOUR_LITTLES,
	PRIO_FOUR_LITTLES_MAX_FREQ,
	PRIO_TWO_BIGS_TWO_LITTLES,
	PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ,
	PRIO_FOUR_BIGS,
	PRIO_FOUR_BIGS_MAX_FREQ,
	PRIO_MAX_CORES,
	PRIO_MAX_CORES_MAX_FREQ,
	PRIO_RESET,
	/* Define the max priority for priority limit */
	PRIO_DEFAULT
};

enum control {
	OFF = -2,
	ON = -1
};

int set_dynamic_boost(int duration, int prio_mode);

#endif	/* __DYNAMIC_BOOST_H__ */

