/*
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef AVS_H
#define AVS_H

#ifdef CONFIG_MSM_AVS_HW
u32 avs_reset_delays(u32 avsdscr);
u32 avs_get_avscsr(void);
u32 avs_get_avsdscr(void);
u32 avs_get_tscsr(void);
void avs_set_tscsr(u32 to_tscsr);
u32 avs_disable(void);
void avs_enable(u32 avscsr);
#else
static inline u32 avs_reset_delays(u32 avsdscr)
{ return 0; }
static inline u32 avs_get_avscsr(void)
{ return 0; }
static inline u32 avs_get_avsdscr(void)
{ return 0; }
static inline u32 avs_get_tscsr(void)
{ return 0; }
static inline void avs_set_tscsr(u32 to_tscsr) {}
static inline u32 avs_disable(void)
{return 0; }
static inline void avs_enable(u32 avscsr) {}
#endif

#define AVS_DISABLE(cpu) do {			\
		if (get_cpu() == (cpu))		\
			avs_disable();		\
		put_cpu();			\
	} while (0);

/* AVSCSR(0x61) to enable CPU, V and L2 AVS module */

#define AVS_ENABLE(cpu, x) do {			\
		if (get_cpu() == (cpu)) {       \
			avs_reset_delays((x));	\
			avs_enable(0x61);	\
		}				\
		put_cpu();			\
	} while (0);

#endif /* AVS_H */
