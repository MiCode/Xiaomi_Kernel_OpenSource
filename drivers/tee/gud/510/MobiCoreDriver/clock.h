/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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

#ifndef MC_CLOCK_H
#define MC_CLOCK_H

#include "platform.h"	/* MC_CRYPTO_CLOCK_MANAGEMENT */

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT

/* Initialize secure crypto clocks */
int mc_clock_init(void);
/* Free secure crypto clocks */
void mc_clock_exit(void);
/* Enable secure crypto clocks */
int mc_clock_enable(void);
/* Disable secure crypto clocks */
void mc_clock_disable(void);

#else /* MC_CRYPTO_CLOCK_MANAGEMENT */

static inline int mc_clock_init(void)
{
	return 0;
}

static inline void mc_clock_exit(void)
{
}

static inline int mc_clock_enable(void)
{
	return 0;
}

static inline void mc_clock_disable(void)
{
}

#endif /* !MC_CRYPTO_CLOCK_MANAGEMENT */

#endif /* MC_CLOCK_H */
