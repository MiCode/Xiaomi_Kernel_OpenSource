/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SUBSYS_RESTART_H
#define __SUBSYS_RESTART_H

#include <linux/spinlock.h>

#define SUBSYS_NAME_MAX_LENGTH 40

struct subsys_device;

enum {
	RESET_SOC = 1,
	RESET_SUBSYS_COUPLED,
	RESET_SUBSYS_INDEPENDENT,
	RESET_LEVEL_MAX
};

struct device;
struct module;

/**
 * struct subsys_desc - subsystem descriptor
 * @name: name of subsystem
 * @depends_on: subsystem this subsystem depends on to operate
 * @dev: parent device
 * @owner: module the descriptor belongs to
 */
struct subsys_desc {
	const char *name;
	const char *depends_on;
	struct device *dev;
	struct module *owner;

	int (*shutdown)(const struct subsys_desc *desc);
	int (*powerup)(const struct subsys_desc *desc);
	void (*crash_shutdown)(const struct subsys_desc *desc);
	int (*ramdump)(int, const struct subsys_desc *desc);
};

#if defined(CONFIG_MSM_SUBSYSTEM_RESTART)

extern int get_restart_level(void);
extern int subsystem_restart_dev(struct subsys_device *dev);
extern int subsystem_restart(const char *name);

extern struct subsys_device *subsys_register(struct subsys_desc *desc);
extern void subsys_unregister(struct subsys_device *dev);

#else

static inline int get_restart_level(void)
{
	return 0;
}

static inline int subsystem_restart_dev(struct subsys_device *dev)
{
	return 0;
}

static inline int subsystem_restart(const char *name)
{
	return 0;
}

static inline
struct subsys_device *subsys_register(struct subsys_desc *desc)
{
	return NULL;
}

static inline void subsys_unregister(struct subsys_device *dev) { }

#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#endif
