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

#define VOLTAGE_MIN  1000 /* mV */
#define VOLTAGE_MAX  1250
#define VOLTAGE_STEP 25

int __init avs_init(int (*set_vdd)(int), u32 freq_cnt, u32 freq_idx);
void __exit avs_exit(void);

int avs_adjust_freq(u32 freq_index, int begin);

/* Routines exported from avs_hw.S */
#ifdef CONFIG_MSM_CPU_AVS
u32 avs_test_delays(void);
#else
static inline u32 avs_test_delays(void)
{ return 0; }
#endif

#ifdef CONFIG_MSM_AVS_HW
u32 avs_reset_delays(u32 avsdscr);
u32 avs_get_avscsr(void);
u32 avs_get_avsdscr(void);
u32 avs_get_tscsr(void);
void avs_set_tscsr(u32 to_tscsr);
void avs_disable(void);
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
static inline void avs_disable(void) {}
#endif

/*#define AVSDEBUG(x...) pr_info("AVS: " x);*/
#define AVSDEBUG(...)

#define AVS_DISABLE(cpu) do {			\
		if (get_cpu() == (cpu))		\
			avs_disable();		\
		put_cpu();			\
	} while (0);

#define AVS_ENABLE(cpu, x) do {			\
		if (get_cpu() == (cpu))		\
			avs_reset_delays((x));	\
		put_cpu();			\
	} while (0);

#endif /* AVS_H */
