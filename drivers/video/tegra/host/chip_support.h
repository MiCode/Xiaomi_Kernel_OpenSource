/*
 * drivers/video/tegra/host/chip_support.h
 *
 * Tegra Graphics Host Chip Support
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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
struct nvhost_syncpt;
struct dentry;
struct nvhost_job;
struct nvhost_job_unpin;
struct nvhost_intr_syncpt;
struct mem_handle;
struct mem_mgr;
struct platform_device;

struct nvhost_channel_ops {
	const char * soc_name;
	int (*init)(struct nvhost_channel *,
		    struct nvhost_master *,
		    int chid);
	int (*submit)(struct nvhost_job *job);
	int (*save_context)(struct nvhost_channel *channel);
	int (*drain_read_fifo)(struct nvhost_channel *ch,
		u32 *ptr, unsigned int count, unsigned int *pending);
};

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
	void (*timeout_cpu_incr)(struct nvhost_cdma *,
				 u32 getptr,
				 u32 syncpt_incrs,
				 u32 syncval,
				 u32 nr_slots,
				 u32 waitbases);
};

struct nvhost_pushbuffer_ops {
	void (*reset)(struct push_buffer *);
	int (*init)(struct push_buffer *);
	void (*destroy)(struct push_buffer *);
	void (*push_to)(struct push_buffer *,
			struct mem_mgr *, struct mem_handle *,
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
	void (*enable_general_irq)(struct nvhost_intr *, int num);
	void (*disable_general_irq)(struct nvhost_intr *, int num);
	int (*free_syncpt_irq)(struct nvhost_intr *);
};

struct nvhost_dev_ops {
	struct nvhost_channel *(*alloc_nvhost_channel)(
			struct platform_device *dev);
	void (*free_nvhost_channel)(struct nvhost_channel *ch);
};

struct nvhost_mem_ops {
	struct mem_mgr *(*alloc_mgr)(void);
	void (*put_mgr)(struct mem_mgr *);
	struct mem_mgr *(*get_mgr)(struct mem_mgr *);
	struct mem_mgr *(*get_mgr_file)(int fd);
	struct mem_handle *(*alloc)(struct mem_mgr *,
			size_t size, size_t align,
			int flags);
	struct mem_handle *(*get)(struct mem_mgr *,
			u32 id, struct platform_device *);
	void (*put)(struct mem_mgr *, struct mem_handle *);
	struct sg_table *(*pin)(struct mem_mgr *, struct mem_handle *);
	void (*unpin)(struct mem_mgr *, struct mem_handle *, struct sg_table *);
	void *(*mmap)(struct mem_handle *);
	void (*munmap)(struct mem_handle *, void *);
	void *(*kmap)(struct mem_handle *, unsigned int);
	void (*kunmap)(struct mem_handle *, unsigned int, void *);
	int (*pin_array_ids)(struct mem_mgr *,
			struct platform_device *,
			u32 *,
			dma_addr_t *,
			u32,
			struct nvhost_job_unpin *);
};

struct nvhost_actmon_ops {
	int (*init)(struct nvhost_master *host);
	void (*deinit)(struct nvhost_master *host);
	int (*read_avg)(struct nvhost_master *host, u32 *val);
	int (*above_wmark_count)(struct nvhost_master *host);
	int (*below_wmark_count)(struct nvhost_master *host);
	int (*read_avg_norm)(struct nvhost_master *host, u32 *val);
	void (*update_sample_period)(struct nvhost_master *host);
	void (*set_sample_period_norm)(struct nvhost_master *host, long usecs);
	long (*get_sample_period_norm)(struct nvhost_master *host);
	long (*get_sample_period)(struct nvhost_master *host);
	void (*set_k)(struct nvhost_master *host, u32 k);
	u32 (*get_k)(struct nvhost_master *host);

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
	struct nvhost_channel_ops channel;
	struct nvhost_cdma_ops cdma;
	struct nvhost_pushbuffer_ops push_buffer;
	struct nvhost_debug_ops debug;
	struct nvhost_syncpt_ops syncpt;
	struct nvhost_intr_ops intr;
	struct nvhost_dev_ops nvhost_dev;
	struct nvhost_mem_ops mem;
	struct nvhost_actmon_ops actmon;
	struct nvhost_tickctrl_ops tickctrl;
};

struct nvhost_chip_support *nvhost_get_chip_ops(void);

#define host_device_op()	(nvhost_get_chip_ops()->nvhost_dev)
#define channel_cdma_op()	(nvhost_get_chip_ops()->cdma)
#define channel_op()		(nvhost_get_chip_ops()->channel)
#define syncpt_op()		(nvhost_get_chip_ops()->syncpt)
#define intr_op()		(nvhost_get_chip_ops()->intr)
#define cdma_op()		(nvhost_get_chip_ops()->cdma)
#define cdma_pb_op()		(nvhost_get_chip_ops()->push_buffer)
#define mem_op()		(nvhost_get_chip_ops()->mem)
#define actmon_op()		(nvhost_get_chip_ops()->actmon)
#define tickctrl_op()		(nvhost_get_chip_ops()->tickctrl)

int nvhost_init_chip_support(struct nvhost_master *host);

#endif /* _NVHOST_CHIP_SUPPORT_H_ */
