/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _RAMDUMP_HEADER
#define _RAMDUMP_HEADER

struct device;

struct ramdump_segment {
	unsigned long address;
	unsigned long size;
};

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
void *create_ramdump_device(const char *dev_name, struct device *parent);
void destroy_ramdump_device(void *dev);
int do_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments);
int do_elf_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments);

#else
static inline void *create_ramdump_device(const char *dev_name,
		struct device *parent)
{
	return NULL;
}

static inline void destroy_ramdump_device(void *dev)
{
}

static inline int do_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments)
{
	return -ENODEV;
}

static inline int do_elf_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments)
{
	return -ENODEV;
}
#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#endif
