/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#ifndef __MACH_PERIPHERAL_LOADER_H
#define __MACH_PERIPHERAL_LOADER_H

#ifdef CONFIG_MSM_PIL
extern void *pil_get(const char *name);
extern void pil_put(void *peripheral_handle);
extern void pil_force_shutdown(const char *name);
extern int pil_force_boot(const char *name);
#else
static inline void *pil_get(const char *name) { return NULL; }
static inline void pil_put(void *peripheral_handle) { }
static inline void pil_force_shutdown(const char *name) { }
static inline int pil_force_boot(const char *name) { return -ENOSYS; }
#endif

#endif
