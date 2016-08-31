/*
 * drivers/video/tegra/host/chip_support.h
 *
 * Tegra Graphics Host Chip Support
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _NVHOST_CHIP_SUPPORT_H_
#define _NVHOST_CHIP_SUPPORT_H_

#include <linux/nvhost_ioctl.h>

#include <linux/types.h>

struct output;

struct nvhost_master;
struct nvhost_intr;
struct nvhost_syncpt;
struct nvhost_userctx_timeout;
struct nvhost_channel;
struct nvhost_hwctx;
struct nvhost_cdma;
struct nvhost_job;
struct push_buffer;
struct nvhost_as;
struct nvhost_syncpt;
struct dentry;
struct nvhost_job;
struct nvhost_job_unpin;
struct nvhost_intr_syncpt;
struct mem_handle;
struct mem_mgr;
struct platform_device;
struct host1x_actmon;

struct nvhost_cdma_ops {
	void (*start)(struct nvhost_cdma *);
	void (*stop)(struct nvhost_cdma *);
	void (*kick)(struct  nvhost_cdma *);
	int (*timeout_init)(struct nvhost_cdma *,
			    u32 syncpt_id);
	void (*timeout_destroy)(struct nvhost_cdma *);
	void (*timeout_teardown_begin)(struct nvhost_cdma *);
	void (*timeout_teardown_end)(struct nvhost_cdma *,
				     u32 getptr);
	void (*timeout_pb_cleanup)(struct nvhost_cdma *,
				 u32 getptr, u32 nr_slots);
};

struct nvhost_pushbuffer_ops {
	void (*reset)(struct push_buffer *);
	int (*init)(struct push_buffer *);
	void (*destroy)(struct push_buffer *);
	void (*push_to)(struct push_buffer *,
			struct mem_mgr *, struct mem_handle *,
			u32 op1, u32 op2);
	void (*_push_to)(struct push_buffer *,
			dma_addr_t iova,
			u32 op1, u32 op2);
	void (*pop_from)(struct push_buffer *,
			 unsigned int slots);
	u32 (*space)(struct push_buffer *);
	u32 (*putptr)(struct push_buffer *);
};

struct nvhost_debug_ops {
	void (*debug_init)(struct dentry *de);
	void (*show_channel_cdma)(struct nvhost_master *,
				  struct nvhost_channel *,
				  struct output *,
				  int chid);
	void (*show_channel_fifo)(struct nvhost_master *,
				  struct nvhost_channel *,
				  struct output *,
				  int chid);
	void (*show_mlocks)(struct nvhost_master *m,
			    struct output *o);

};

struct nvhost_syncpt_ops {
	void (*reset)(struct nvhost_syncpt *, u32 id);
	void (*reset_wait_base)(struct nvhost_syncpt *, u32 id);
	void (*read_wait_base)(struct nvhost_syncpt *, u32 id);
	u32 (*update_min)(struct nvhost_syncpt *, u32 id);
	void (*cpu_incr)(struct nvhost_syncpt *, u32 id);
	int (*patch_wait)(struct nvhost_syncpt *sp,
			void *patch_addr);
	void (*debug)(struct nvhost_syncpt *);
	const char * (*name)(struct nvhost_syncpt *, u32 id);
	int (*mutex_try_lock)(struct nvhost_syncpt *,
			      unsigned int idx);
	void (*mutex_unlock)(struct nvhost_syncpt *,
			     unsigned int idx);
	void (*mutex_owner)(struct nvhost_syncpt *,
			    unsigned int idx,
			    bool *cpu, bool *ch,
			    unsigned int *chid);
};

struct nvhost_intr_ops {
	void (*init_host_sync)(struct nvhost_intr *);
	void (*set_host_clocks_per_usec)(
		struct nvhost_intr *, u32 clocks);
	void (*set_syncpt_threshold)(
		struct nvhost_intr *, u32 id, u32 thresh);
	void (*enable_syncpt_intr)(struct nvhost_intr *, u32 id);
	void (*disable_syncpt_intr)(struct nvhost_intr *, u32 id);
	void (*disable_all_syncpt_intrs)(struct nvhost_intr *);
	int  (*request_host_general_irq)(struct nvhost_intr *);
	void (*free_host_general_irq)(struct nvhost_intr *);
	int (*free_syncpt_irq)(struct nvhost_intr *);
};

struct nvhost_dev_ops {
	struct nvhost_channel *(*alloc_nvhost_channel)(
			struct platform_device *dev);
	void (*free_nvhost_channel)(struct nvhost_channel *ch);
};

struct nvhost_actmon_ops {
	int (*init)(struct host1x_actmon *actmon);
	void (*deinit)(struct host1x_actmon *actmon);
	int (*read_avg)(struct host1x_actmon *actmon, u32 *val);
	int (*above_wmark_count)(struct host1x_actmon *actmon);
	int (*below_wmark_count)(struct host1x_actmon *actmon);
	int (*read_avg_norm)(struct host1x_actmon *actmon, u32 *val);
	void (*update_sample_period)(struct host1x_actmon *actmon);
	void (*set_sample_period_norm)(struct host1x_actmon *actmon,
				       long usecs);
	long (*get_sample_period_norm)(struct host1x_actmon *actmon);
	long (*get_sample_period)(struct host1x_actmon *actmon);
	void (*set_k)(struct host1x_actmon *actmon, u32 k);
	u32 (*get_k)(struct host1x_actmon *actmon);
	void (*debug_init)(struct host1x_actmon *actmon, struct dentry *de);
};

struct nvhost_tickctrl_ops {
	int (*init_channel)(struct platform_device *dev);
	void (*deinit_channel)(struct platform_device *dev);
	int (*tickcount)(struct platform_device *dev, u64 *val);
	int (*stallcount)(struct platform_device *dev, u64 *val);
	int (*xfercount)(struct platform_device *dev, u64 *val);
};

struct nvhost_chip_support {
	const char * soc_name;
	struct nvhost_cdma_ops cdma;
	struct nvhost_pushbuffer_ops push_buffer;
	struct nvhost_debug_ops debug;
	struct nvhost_syncpt_ops syncpt;
	struct nvhost_intr_ops intr;
	struct nvhost_dev_ops nvhost_dev;
	struct nvhost_actmon_ops actmon;
	struct nvhost_tickctrl_ops tickctrl;
	void (*remove_support)(struct nvhost_chip_support *op);
	void *priv;
};

/* The reason these accumulate is that 3x uses
 * some of the 2x code, and likewise 12x vs prior.
 */
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define TEGRA_2X_OR_HIGHER_CONFIG
#endif

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#define TEGRA_3X_OR_HIGHER_CONFIG
#define TEGRA_2X_OR_HIGHER_CONFIG
#endif

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define TEGRA_11X_OR_HIGHER_CONFIG
#define TEGRA_3X_OR_HIGHER_CONFIG
#define TEGRA_2X_OR_HIGHER_CONFIG
#endif

#ifdef CONFIG_ARCH_TEGRA_14x_SOC
#define TEGRA_14X_OR_HIGHER_CONFIG
#define TEGRA_11X_OR_HIGHER_CONFIG
#define TEGRA_3X_OR_HIGHER_CONFIG
#define TEGRA_2X_OR_HIGHER_CONFIG
#endif

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
#define TEGRA_12X_OR_HIGHER_CONFIG
#define TEGRA_14X_OR_HIGHER_CONFIG
#define TEGRA_11X_OR_HIGHER_CONFIG
#define TEGRA_3X_OR_HIGHER_CONFIG
#define TEGRA_2X_OR_HIGHER_CONFIG
#endif

struct nvhost_chip_support *nvhost_get_chip_ops(void);

#define host_device_op()	(nvhost_get_chip_ops()->nvhost_dev)
#define channel_cdma_op()	(nvhost_get_chip_ops()->cdma)
#define syncpt_op()		(nvhost_get_chip_ops()->syncpt)
#define intr_op()		(nvhost_get_chip_ops()->intr)
#define cdma_op()		(nvhost_get_chip_ops()->cdma)
#define cdma_pb_op()		(nvhost_get_chip_ops()->push_buffer)

#define actmon_op()		(nvhost_get_chip_ops()->actmon)
#define tickctrl_op()		(nvhost_get_chip_ops()->tickctrl)

int nvhost_init_chip_support(struct nvhost_master *);

#endif /* _NVHOST_CHIP_SUPPORT_H_ */
