/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>

struct subsys_device;

enum {
	RESET_SOC = 0,
	RESET_SUBSYS_COUPLED,
	RESET_LEVEL_MAX
};

enum crash_status {
	CRASH_STATUS_NO_CRASH = 0,
	CRASH_STATUS_ERR_FATAL,
	CRASH_STATUS_WDOG_BITE,
};

struct device;
struct module;

/**
 * struct subsys_desc - subsystem descriptor
 * @name: name of subsystem
 * @fw_name: firmware name
 * @depends_on: subsystem this subsystem depends on to operate
 * @dev: parent device
 * @owner: module the descriptor belongs to
 * @shutdown: Stop a subsystem
 * @powerup: Start a subsystem
 * @crash_shutdown: Shutdown a subsystem when the system crashes (can't sleep)
 * @ramdump: Collect a ramdump of the subsystem
 * @free_memory: Free the memory associated with this subsystem
 * @is_not_loadable: Indicate if subsystem firmware is not loadable via pil
 * framework
 * @no_auth: Set if subsystem does not rely on PIL to authenticate and bring
 * it out of reset
 * @ssctl_instance_id: Instance id used to connect with SSCTL service
 * @sysmon_pid:	pdev id that sysmon is probed with for the subsystem
 * @sysmon_shutdown_ret: Return value for the call to sysmon_send_shutdown
 * @system_debug: If "set", triggers a device restart when the
 * subsystem's wdog bite handler is invoked.
 * @ignore_ssr_failure: SSR failures are usually fatal and results in panic. If
 * set will ignore failure.
 * @edge: GLINK logical name of the subsystem
 */
struct subsys_desc {
	const char *name;
	char fw_name[256];
	const char *depends_on;
	struct device *dev;
	struct module *owner;

	int (*shutdown)(const struct subsys_desc *desc, bool force_stop);
	int (*powerup)(const struct subsys_desc *desc);
	void (*crash_shutdown)(const struct subsys_desc *desc);
	int (*ramdump)(int, const struct subsys_desc *desc);
	void (*free_memory)(const struct subsys_desc *desc);
	irqreturn_t (*err_fatal_handler)(int irq, void *dev_id);
	irqreturn_t (*stop_ack_handler)(int irq, void *dev_id);
	irqreturn_t (*wdog_bite_handler)(int irq, void *dev_id);
	irqreturn_t (*generic_handler)(int irq, void *dev_id);
	int is_not_loadable;
	int err_fatal_gpio;
	unsigned int err_fatal_irq;
	unsigned int err_ready_irq;
	unsigned int stop_ack_irq;
	unsigned int wdog_bite_irq;
	unsigned int generic_irq;
	int force_stop_gpio;
	int ramdump_disable_gpio;
	int shutdown_ack_gpio;
	int ramdump_disable;
	bool no_auth;
	bool pil_mss_memsetup;
	int ssctl_instance_id;
	u32 sysmon_pid;
	int sysmon_shutdown_ret;
	bool system_debug;
	bool ignore_ssr_failure;
	const char *edge;
};

/**
 * struct notif_data - additional notif information
 * @crashed: indicates if subsystem has crashed due to wdog bite or err fatal
 * @enable_ramdump: ramdumps disabled if set to 0
 * @enable_mini_ramdumps: enable flag for minimized critical-memory-only
 * ramdumps
 * @no_auth: set if subsystem does not use PIL to bring it out of reset
 * @pdev: subsystem platform device pointer
 */
struct notif_data {
	enum crash_status crashed;
	int enable_ramdump;
	int enable_mini_ramdumps;
	bool no_auth;
	struct platform_device *pdev;
};

#if defined(CONFIG_MSM_SUBSYSTEM_RESTART)

extern int subsys_get_restart_level(struct subsys_device *dev);
extern int subsystem_restart_dev(struct subsys_device *dev);
extern int subsystem_restart(const char *name);
extern int subsystem_crashed(const char *name);

extern void *subsystem_get(const char *name);
extern void *subsystem_get_with_fwname(const char *name, const char *fw_name);
extern int subsystem_set_fwname(const char *name, const char *fw_name);
extern void subsystem_put(void *subsystem);

extern struct subsys_device *subsys_register(struct subsys_desc *desc);
extern void subsys_unregister(struct subsys_device *dev);

extern void subsys_default_online(struct subsys_device *dev);
extern void subsys_set_crash_status(struct subsys_device *dev,
					enum crash_status crashed);
extern enum crash_status subsys_get_crash_status(struct subsys_device *dev);
void notify_proxy_vote(struct device *device);
void notify_proxy_unvote(struct device *device);
void complete_err_ready(struct subsys_device *subsys);
extern int wait_for_shutdown_ack(struct subsys_desc *desc);
#else

static inline int subsys_get_restart_level(struct subsys_device *dev)
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

static inline int subsystem_crashed(const char *name)
{
	return 0;
}

static inline void *subsystem_get(const char *name)
{
	return NULL;
}

static inline void *subsystem_get_with_fwname(const char *name,
				const char *fw_name) {
	return NULL;
}

static inline int subsystem_set_fwname(const char *name,
				const char *fw_name) {
	return 0;
}

static inline void subsystem_put(void *subsystem) { }

static inline
struct subsys_device *subsys_register(struct subsys_desc *desc)
{
	return NULL;
}

static inline void subsys_unregister(struct subsys_device *dev) { }

static inline void subsys_default_online(struct subsys_device *dev) { }
static inline void subsys_set_crash_status(struct subsys_device *dev,
						enum crash_status crashed) { }
static inline
enum crash_status subsys_get_crash_status(struct subsys_device *dev)
{
	return false;
}
static inline void notify_proxy_vote(struct device *device) { }
static inline void notify_proxy_unvote(struct device *device) { }
static inline int wait_for_shutdown_ack(struct subsys_desc *desc)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#endif
