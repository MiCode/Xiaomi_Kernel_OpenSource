/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 */

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
void place_marker(const char *name);
void destroy_marker(const char *name);
int boot_marker_enabled(void) { return 1; }
#else
static int init_bootkpi(void) { return 0; }
static inline void exit_bootkpi(void) { };
static inline void place_marker(char *name) { };
static inline void destroy_marker(const char *name) { };
static int boot_marker_enabled(void) { return 0; }
#endif
