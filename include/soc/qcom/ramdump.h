/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011-2014, 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _RAMDUMP_HEADER
#define _RAMDUMP_HEADER

struct device;

struct ramdump_segment {
	char *name;
	unsigned long address;
	void *v_address;
	unsigned long size;
};

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
extern void *create_ramdump_device(const char *dev_name, struct device *parent);
extern void destroy_ramdump_device(void *dev);
extern int do_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments);
extern int do_elf_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments);
extern int do_minidump(void *handle, struct ramdump_segment *segments,
		       int nsegments);
extern int do_minidump_elf32(void *handle, struct ramdump_segment *segments,
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
