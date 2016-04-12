/* Copyright (c) 2013-2014,2016 The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_MSM_BOOT_STATS

#define TIMER_KHZ 32768
extern struct boot_stats __iomem *boot_stats;

struct boot_stats {
	uint32_t bootloader_start;
	uint32_t bootloader_end;
	uint32_t bootloader_display;
	uint32_t bootloader_load_kernel;
};

int boot_stats_init(void);
int boot_stats_exit(void);
unsigned long long int msm_timer_get_sclk_ticks(void);
#else
static inline int boot_stats_init(void) { return 0; }
unsigned long long int msm_timer_get_sclk_ticks(void) { return 0; }
#endif

#ifdef CONFIG_MSM_BOOT_TIME_MARKER

static inline int boot_marker_enabled(void) { return 1; }
void place_marker(const char *name);
#else
inline void place_marker(char *name);
static inline int boot_marker_enabled(void) { return 0; }
#endif
