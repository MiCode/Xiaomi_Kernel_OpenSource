/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef _MC_CLOCK_H_
#define _MC_CLOCK_H_

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

#endif /* _MC_CLOCK_H_ */
