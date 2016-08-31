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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/time.h>

struct nvhost_master;
struct nvhost_hwctx;
struct nvhost_device_power_attr;
struct nvhost_device_profile;
struct mem_mgr;
struct nvhost_as_moduleops;

#define NVHOST_MODULE_MAX_CLOCKS		7
#define NVHOST_MODULE_MAX_POWERGATE_IDS 	2
#define NVHOST_MODULE_MAX_SYNCPTS		8
#define NVHOST_MODULE_MAX_WAITBASES		3
#define NVHOST_MODULE_MAX_MODMUTEXES		5
#define NVHOST_MODULE_MAX_IORESOURCE_MEM	3
#define NVHOST_MODULE_NO_POWERGATE_IDS		.powergate_ids = {-1, -1}
#define NVHOST_DEFAULT_CLOCKGATE_DELAY		.clockgate_delay = 25
#define NVHOST_MODULE_MAX_IORESOURCE_MEM 3
#define NVHOST_NAME_SIZE			24
#define NVSYNCPT_INVALID			(-1)

#define NVSYNCPT_GRAPHICS_HOST		(0)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP0_D		(5)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP0_H		(6)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP1_H		(7)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP0_A		(8)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP1_A		(9)	/* t20, t30, t114, t148 */
#define NVSYNCPT_AVP_0			(10)	/* t20, t30, t114, t148 */
#define NVSYNCPT_CSI_VI_0		(11)	/* t20, t30, t114, t148 */
#define NVSYNCPT_CSI_VI_1		(12)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VI_ISP_0		(13)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VI_ISP_1		(14)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VI_ISP_2		(15)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VI_ISP_3		(16)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VI_ISP_4		(17)	/* t20, t30, t114, t148 */
#define NVSYNCPT_2D_0			(18)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VIC			(18)	/* t124 */
#define NVSYNCPT_2D_1			(19)	/* t20, t30, t114, t148 */
#define NVSYNCPT_MSENC_SLICE		(19)	/* t124 */
#define NVSYNCPT_DISP0_B		(20)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP1_B		(21)	/* t20, t30, t114, t148 */
#define NVSYNCPT_3D			(22)	/* t20, t30, t114, t148 */
#define NVSYNCPT_MPE			(23)	/* t20, t30 */
#define NVSYNCPT_MSENC			(23)	/* t114, t148 */
#define NVSYNCPT_DISP0_C		(24)	/* t20, t30, t114, t148 */
#define NVSYNCPT_DISP1_C		(25)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VBLANK0		(26)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VBLANK1		(27)	/* t20, t30, t114, t148 */
#define NVSYNCPT_MPE_EBM_EOF		(28)	/* t20, t30 */
#define NVSYNCPT_TSEC			(28)	/* t114, t148 */
#define NVSYNCPT_MPE_WR_SAFE		(29)	/* t20, t30 */
#define NVSYNCPT_DSI			(31)	/* t20, t30, t114, t148 */
#define NVSYNCPT_ISP_0_0		(32)	/* t124 */
#define NVSYNCPT_ISP_0_1		(33)	/* t124 */
#define NVSYNCPT_ISP_0_2		(34)	/* t124 */
#define NVSYNCPT_ISP_0_3		(35)	/* t124 */
#define NVSYNCPT_ISP_1_0		(36)	/* t124 */
#define NVSYNCPT_ISP_1_1		(37)	/* t124 */
#define NVSYNCPT_ISP_1_2		(38)	/* t124 */
#define NVSYNCPT_ISP_1_3		(39)	/* t124 */
#define NVSYNCPT_VI_0_0			(40)	/* t124 */
#define NVSYNCPT_VI_0_1			(41)	/* t124 */
#define NVSYNCPT_VI_0_2			(42)	/* t124 */
#define NVSYNCPT_VI_0_3			(43)	/* t124 */
#define NVSYNCPT_VI_0_4			(44)	/* t124 */
#define NVSYNCPT_VI_1_0			(45)	/* t124 */
#define NVSYNCPT_VI_1_1			(46)	/* t124 */
#define NVSYNCPT_VI_1_2			(47)	/* t124 */
#define NVSYNCPT_VI_1_3			(48)	/* t124 */
#define NVSYNCPT_VI_1_4			(49)	/* t124 */

#define NVWAITBASE_2D_0			(1)	/* t20, t30, t114 */
#define NVWAITBASE_2D_1			(2)	/* t20, t30, t114 */
#define NVWAITBASE_3D			(3)	/* t20, t30, t114 */
#define NVWAITBASE_MPE			(4)	/* t20, t30 */
#define NVWAITBASE_MSENC		(4)	/* t114, t148 */
#define NVWAITBASE_TSEC			(5)	/* t114, t148 */

#define NVMODMUTEX_2D_FULL		(1)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_ISP_0		(1)	/* t124 */
#define NVMODMUTEX_2D_SIMPLE		(2)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_ISP_1		(2)	/* t124 */
#define NVMODMUTEX_2D_SB_A		(3)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_2D_SB_B		(4)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_3D			(5)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_DISPLAYA		(6)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_DISPLAYB		(7)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_VI			(8)	/* t20, t30, t114 */
#define NVMODMUTEX_VI_0			(8)	/* t148 */
#define NVMODMUTEX_DSI			(9)	/* t20, t30, t114, t148 */
#define NVMODMUTEX_VIC			(10)	/* t124 */
#define NVMODMUTEX_VI_1			(11)	/* t124 */

/* sync points that are wholly managed by the client */
#define NVSYNCPTS_CLIENT_MANAGED ( \
	BIT(NVSYNCPT_DISP0_A) | BIT(NVSYNCPT_DISP1_A) | \
	BIT(NVSYNCPT_DISP0_B) | BIT(NVSYNCPT_DISP1_B) | \
	BIT(NVSYNCPT_DISP0_C) | BIT(NVSYNCPT_DISP1_C) | \
	BIT(NVSYNCPT_DISP0_D) | \
	BIT(NVSYNCPT_DISP0_H) | BIT(NVSYNCPT_DISP1_H) | \
	BIT(NVSYNCPT_DSI) | \
	BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1) | \
	BIT(NVSYNCPT_CSI_VI_0) | BIT(NVSYNCPT_CSI_VI_1) | \
	BIT(NVSYNCPT_VI_ISP_1) | BIT(NVSYNCPT_VI_ISP_2) | \
	BIT(NVSYNCPT_VI_ISP_3) | BIT(NVSYNCPT_VI_ISP_4) | \
	BIT(NVSYNCPT_MPE_EBM_EOF) | BIT(NVSYNCPT_MPE_WR_SAFE) | \
	BIT(NVSYNCPT_2D_1) | BIT(NVSYNCPT_AVP_0))

enum nvhost_power_sysfs_attributes {
	NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY = 0,
	NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY,
	NVHOST_POWER_SYSFS_ATTRIB_MAX
};

struct nvhost_notification {
	struct {			/* 0000- */
		__u32 nanoseconds[2];	/* nanoseconds since Jan. 1, 1970 */
	} time_stamp;			/* -0007 */
	__u32 info32;	/* info returned depends on method 0008-000b */
#define	NVHOST_CHANNEL_FIFO_ERROR_IDLE_TIMEOUT	8
#define	NVHOST_CHANNEL_GR_ERROR_SW_NOTIFY	13
#define	NVHOST_CHANNEL_GR_SEMAPHORE_TIMEOUT	24
#define	NVHOST_CHANNEL_GR_ILLEGAL_NOTIFY	25
#define	NVHOST_CHANNEL_FIFO_ERROR_MMU_ERR_FLT	31
	__u16 info16;	/* info returned depends on method 000c-000d */
	__u16 status;	/* user sets bit 15, NV sets status 000e-000f */
};

struct nvhost_clock {
	char *name;
	unsigned long default_rate;
	u32 moduleid;
	int reset;
	unsigned long devfreq_rate;
};

struct nvhost_device_data {
	int		version;	/* ip version number of device */
	int		id;		/* Separates clients of same hw */
	int		index;		/* Hardware channel number */
	void __iomem	*aperture[NVHOST_MODULE_MAX_IORESOURCE_MEM];
	struct device_dma_parameters dma_parms;

	u32		syncpts[NVHOST_MODULE_MAX_SYNCPTS];
	u32		syncpt_base;	/* Device sync point base */
	u32		waitbases[NVHOST_MODULE_MAX_WAITBASES];
	u32		modulemutexes[NVHOST_MODULE_MAX_MODMUTEXES];
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

	struct platform_device *master;	/* Master of a slave device */
	struct platform_device *slave;	/* Slave device to create in probe */
	int		slave_initialized;

	int		num_clks;	/* Number of clocks opened for dev */
	struct clk	*clk[NVHOST_MODULE_MAX_CLOCKS];
	struct mutex	lock;		/* Power management lock */
	struct list_head client_list;	/* List of clients and rate requests */

	struct nvhost_channel *channel;	/* Channel assigned for the module */

	/* device node for ctrl block */
	struct device *ctrl_node;
	struct cdev ctrl_cdev;
	const struct file_operations *ctrl_ops;    /* ctrl ops for the module */

	/* address space operations */
	const struct nvhost_as_moduleops *as_ops;

	/* module debugger */
	struct device *dbg_node;
	struct cdev dbg_cdev;
	const struct file_operations *dbg_ops;

	/* module profiler */
	struct device *prof_node;
	struct cdev prof_cdev;
	const struct file_operations *prof_ops;

	struct kobject *power_kobj;	/* kobject to hold power sysfs entries */
	struct nvhost_device_power_attr *power_attrib;	/* sysfs attributes */
	struct dentry *debugfs;		/* debugfs directory */

	u32 nvhost_timeout_default;

	/* QoS id that denotes minimum frequency */
	unsigned int			qos_id;
	/* Data for devfreq usage */
	struct devfreq			*power_manager;
	/* Private device profile data */
	struct nvhost_device_profile	*power_profile;
	/* Should we read load estimate from hardware? */
	bool				actmon_enabled;
	/* Should we do linear emc scaling? */
	bool				linear_emc;
	/* Should the load value be delivered forward to edp? */
	bool				gpu_edp_device;
	/* Offset to actmon registers */
	u32				actmon_regs;
	/* Devfreq governor name */
	const char			*devfreq_governor;

	void *private_data;		/* private platform data */
	struct platform_device *pdev;	/* owner platform_device */

	struct dev_pm_qos_request no_poweroff_req;

#ifdef CONFIG_PM_GENERIC_DOMAINS
	struct generic_pm_domain pd;	/* power domain representing power partition */
#endif
	/* forces the context restore gather for each submit */
	bool		force_context_restore;

	/* Finalize power on. Can be used for context restore. */
	int (*finalize_poweron)(struct platform_device *dev);

	/*
	 * Reset the unit. Used for timeout recovery, resetting the unit on
	 * probe and when un-powergating.
	 */
	void (*reset)(struct platform_device *dev);

	/* Device is busy. */
	void (*busy)(struct platform_device *);

	/* Device is idle. */
	void (*idle)(struct platform_device *);

	/* Device is going to be suspended */
	void (*suspend_ndev)(struct device *);

	/* Scaling init is run on device registration */
	void (*scaling_init)(struct platform_device *dev);

	/* Scaling deinit is called on device unregistration */
	void (*scaling_deinit)(struct platform_device *dev);

	/* Postscale callback is called after frequency change */
	void (*scaling_post_cb)(struct nvhost_device_profile *profile,
				unsigned long freq);

	/* Device is initialized */
	int (*init)(struct platform_device *dev);

	/* Device is de-initialized. */
	void (*deinit)(struct platform_device *dev);

	/* Preparing for power off. Used for context save. */
	int (*prepare_poweroff)(struct platform_device *dev);

	/* Allocates a context handler for the device */
	struct nvhost_hwctx_handler *(*alloc_hwctx_handler)(u32 syncpt,
			u32 waitbase, struct nvhost_channel *ch);

	/* Read module register into memory */
	int (*read_reg)(struct platform_device *dev,
			struct nvhost_channel *ch,
			struct nvhost_hwctx *hwctx,
			u32 offset,
			u32 *value);

	/* Callback when a clock is changed */
	void (*update_clk)(struct platform_device *dev);
};


static inline
struct nvhost_device_data *nvhost_get_devdata(struct platform_device *pdev)
{
	return (struct nvhost_device_data *)platform_get_drvdata(pdev);
}

enum nvhost_devfreq_busy {
	DEVICE_IDLE = 0,
	DEVICE_BUSY = 1
};

struct nvhost_devfreq_ext_stat {
	enum nvhost_devfreq_busy	busy;
	unsigned long			max_freq;
	unsigned long			min_freq;
};

struct nvhost_device_power_attr {
	struct platform_device *ndev;
	struct kobj_attribute power_attr[NVHOST_POWER_SYSFS_ATTRIB_MAX];
};

void host1x_writel(struct platform_device *dev, u32 r, u32 v);
u32 host1x_readl(struct platform_device *dev, u32 r);

/* public host1x power management APIs */
bool nvhost_module_powered_ext(struct platform_device *dev);
void nvhost_module_busy_ext(struct platform_device *dev);
void nvhost_module_idle_ext(struct platform_device *dev);

/* public host1x sync-point management APIs */
u32 nvhost_syncpt_incr_max_ext(struct platform_device *dev, u32 id, u32 incrs);
void nvhost_syncpt_cpu_incr_ext(struct platform_device *dev, u32 id);
u32 nvhost_syncpt_read_ext(struct platform_device *dev, u32 id);
int nvhost_syncpt_wait_timeout_ext(struct platform_device *dev, u32 id, u32 thresh,
	u32 timeout, u32 *value, struct timespec *ts);
int nvhost_syncpt_create_fence_single_ext(struct platform_device *dev,
	u32 id, u32 thresh, const char *name, int *fence_fd);

/* Hacky way to get access to struct nvhost_device_data for VI device. */
extern struct nvhost_device_data t20_vi_info;
extern struct nvhost_device_data t30_vi_info;
extern struct nvhost_device_data t11_vi_info;
extern struct nvhost_device_data t14_vi_info;

#endif
