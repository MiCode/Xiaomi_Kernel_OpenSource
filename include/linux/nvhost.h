/*
 * include/linux/nvhost.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2009-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __LINUX_NVHOST_H
#define __LINUX_NVHOST_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>

struct nvhost_master;
struct nvhost_hwctx;
struct nvhost_device_power_attr;

#define NVHOST_MODULE_MAX_CLOCKS		3
#define NVHOST_MODULE_MAX_POWERGATE_IDS 	2
#define NVHOST_MODULE_NO_POWERGATE_IDS		.powergate_ids = {-1, -1}
#define NVHOST_DEFAULT_CLOCKGATE_DELAY		.clockgate_delay = 25
#define NVHOST_NAME_SIZE			24
#define NVSYNCPT_INVALID			(-1)

/* FIXME:
 * Sync point ids are now split into 2 files.
 * 1 if this one and other is in
 * drivers/video/tegra/host/host1x/host1x_syncpt.h
 * So if someone decides to add new sync point in future
 * please check both the header files
 */
#define NVSYNCPT_DISP0_D		(5)
#define NVSYNCPT_DISP0_H		(6)
#define NVSYNCPT_DISP1_H		(7)
#define NVSYNCPT_DISP0_A		(8)
#define NVSYNCPT_DISP1_A		(9)
#define NVSYNCPT_AVP_0			(10)
#define NVSYNCPT_DISP0_B		(20)
#define NVSYNCPT_DISP1_B		(21)
#define NVSYNCPT_DISP0_C		(24)
#define NVSYNCPT_DISP1_C		(25)
#define NVSYNCPT_VBLANK0		(26)
#define NVSYNCPT_VBLANK1		(27)
#define NVSYNCPT_DSI			(31)

enum nvhost_power_sysfs_attributes {
	NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY = 0,
	NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY,
	NVHOST_POWER_SYSFS_ATTRIB_REFCOUNT,
	NVHOST_POWER_SYSFS_ATTRIB_MAX
};

struct nvhost_clock {
	char *name;
	unsigned long default_rate;
	u32 moduleid;
	int reset;
	unsigned long devfreq_rate;
};

enum nvhost_device_powerstate_t {
	NVHOST_POWER_STATE_DEINIT,
	NVHOST_POWER_STATE_RUNNING,
	NVHOST_POWER_STATE_CLOCKGATED,
	NVHOST_POWER_STATE_POWERGATED
};

struct nvhost_device_data {
	int		version;	/* ip version number of device */
	int		id;		/* Separates clients of same hw */
	int		index;		/* Hardware channel number */
	struct resource	*reg_mem;
	void __iomem	*aperture;	/* Iomem mapped to kernel */

	u32		syncpts;	/* Bitfield of sync points used */
	u32		waitbases;	/* Bit field of wait bases */
	u32		modulemutexes;	/* Bit field of module mutexes */
	u32		moduleid;	/* Module id for user space API */

	u32		class;		/* Device class */
	bool		exclusive;	/* True if only one user at a time */
	bool		keepalive;	/* Do not power gate when opened */
	bool		waitbasesync;	/* Force sync of wait bases */
	bool		powerup_reset;	/* Do a reset after power un-gating */
	bool		serialize;	/* Serialize submits in the channel */

	int		powergate_ids[NVHOST_MODULE_MAX_POWERGATE_IDS];
	bool		can_powergate;	/* True if module can be power gated */
	int		clockgate_delay;/* Delay before clock gated */
	int		powergate_delay;/* Delay before power gated */
	struct nvhost_clock clocks[NVHOST_MODULE_MAX_CLOCKS];/* Clock names */

	struct delayed_work powerstate_down;/* Power state management */
	int		num_clks;	/* Number of clocks opened for dev */
	struct clk	*clk[NVHOST_MODULE_MAX_CLOCKS];
	struct mutex	lock;		/* Power management lock */
	int		powerstate;	/* Current power state */
	int		refcount;	/* Number of tasks active */
	wait_queue_head_t idle_wq;	/* Work queue for idle */
	struct list_head client_list;	/* List of clients and rate requests */

	struct nvhost_channel *channel;	/* Channel assigned for the module */
	struct kobject *power_kobj;	/* kobject to hold power sysfs entries */
	struct nvhost_device_power_attr *power_attrib;	/* sysfs attributes */
	struct devfreq	*power_manager;	/* Device power management */
	struct dentry *debugfs;		/* debugfs directory */

	void *private_data;		/* private platform data */
	struct platform_device *pdev;	/* owner platform_device */

	/* Finalize power on. Can be used for context restore. */
	void (*finalize_poweron)(struct platform_device *dev);

	/* Device is busy. */
	void (*busy)(struct platform_device *);

	/* Device is idle. */
	void (*idle)(struct platform_device *);

	/* Device is going to be suspended */
	void (*suspend_ndev)(struct platform_device *);

	/* Scaling init is run on device registration */
	void (*scaling_init)(struct platform_device *dev);

	/* Scaling deinit is called on device unregistration */
	void (*scaling_deinit)(struct platform_device *dev);

	/* Device is initialized */
	void (*init)(struct platform_device *dev);

	/* Device is de-initialized. */
	void (*deinit)(struct platform_device *dev);

	/* Preparing for power off. Used for context save. */
	int (*prepare_poweroff)(struct platform_device *dev);

	/* Allocates a context handler for the device */
	struct nvhost_hwctx_handler *(*alloc_hwctx_handler)(u32 syncpt,
			u32 waitbase, struct nvhost_channel *ch);

	/* Clock gating callbacks */
	int (*prepare_clockoff)(struct platform_device *dev);
	void (*finalize_clockon)(struct platform_device *dev);

	/* Read module register into memory */
	int (*read_reg)(struct platform_device *dev,
			struct nvhost_channel *ch,
			struct nvhost_hwctx *hwctx,
			u32 offset,
			u32 *value);

	/* Callback when a clock is changed */
	void (*update_clk)(struct platform_device *dev);
};

struct nvhost_devfreq_ext_stat {
	int		busy;
	unsigned long	max_freq;
	unsigned long	min_freq;
};

struct nvhost_device_power_attr {
	struct platform_device *ndev;
	struct kobj_attribute power_attr[NVHOST_POWER_SYSFS_ATTRIB_MAX];
};

void nvhost_device_writel(struct platform_device *dev, u32 r, u32 v);
u32 nvhost_device_readl(struct platform_device *dev, u32 r);

/* public host1x power management APIs */
bool nvhost_module_powered_ext(struct platform_device *dev);
void nvhost_module_busy_ext(struct platform_device *dev);
void nvhost_module_idle_ext(struct platform_device *dev);

/* public host1x sync-point management APIs */
u32 nvhost_syncpt_incr_max_ext(struct platform_device *dev, u32 id, u32 incrs);
void nvhost_syncpt_cpu_incr_ext(struct platform_device *dev, u32 id);
u32 nvhost_syncpt_read_ext(struct platform_device *dev, u32 id);
int nvhost_syncpt_wait_timeout_ext(struct platform_device *dev, u32 id, u32 thresh,
	u32 timeout, u32 *value);

void nvhost_scale3d_set_throughput_hint(int hint);

#endif
