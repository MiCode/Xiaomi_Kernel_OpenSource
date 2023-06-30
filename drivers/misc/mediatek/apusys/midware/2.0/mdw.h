/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_H__
#define __MTK_APU_MDW_H__

#include <linux/miscdevice.h>
#include <linux/iopoll.h>
#include <linux/irqreturn.h>
#include <linux/dma-fence.h>
#include <linux/of_device.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-fence.h>
#include <linux/hashtable.h>
#include <linux/genalloc.h>

#include "apusys_core.h"
#include "apusys_device.h"
#include "mdw_ioctl.h"
#include "mdw_import.h"

#define MDW_NAME "apusys"
#define MDW_DEV_MAX (APUSYS_DEVICE_MAX)
#define MDW_DEV_TAB_DEV_MAX (16)
#define MDW_CMD_MAX (32)
#define MDW_SUBCMD_MAX (64)
#define MDW_PRIORITY_MAX (32)
#define MDW_DEFAULT_TIMEOUT_MS (30*1000)
#define MDW_BOOST_MAX (100)
#define MDW_DEFAULT_ALIGN (16)

#define MDW_ALIGN(x, align) ((x+align-1) & (~(align-1)))


struct mdw_fpriv;
struct mdw_device;
struct mdw_mem;

enum mdw_mem_op {
	MDW_MEM_OP_NONE,
	MDW_MEM_OP_INTERNAL,
	MDW_MEM_OP_ALLOC,
	MDW_MEM_OP_IMPORT,
};

struct mdw_mem_map {
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct kref map_ref;
	struct mdw_mem *m;
};

struct mdw_mem_invoke {
	struct list_head map_node; //to mdw_mem_map
	struct list_head u_node; //to mpriv
	struct kref ref;
	struct mdw_mem *m;
	struct mdw_fpriv *invoker;
	void (*get)(struct mdw_mem_invoke *m_invoke);
	void (*put)(struct mdw_mem_invoke *m_invoke);
};

enum mdw_queue_type {
	MDW_QUEUE_COMMON,
	MDW_QUEUE_NORMAL,
	MDW_QUEUE_DEADLINE,

	MDW_QUEUE_MAX,
};

struct mdw_mem {
	/* in */
	enum mdw_mem_type type;
	unsigned int size;
	unsigned int align;
	uint64_t flags;

	/* out */
	void *vaddr;
	struct device *mdev;
	struct dma_buf *dbuf;
	void *priv;
	int (*bind)(void *session, struct mdw_mem *m);
	void (*unbind)(void *session, struct mdw_mem *m);

	/* map */
	uint64_t device_va;
	uint32_t dva_size;
	struct mdw_mem_map *map;

	/* control */
	int handle;
	bool belong_apu;
	bool need_handle;
	struct list_head maps;
	struct mdw_fpriv *mpriv;
	struct mdw_mem_pool *pool;
	struct list_head u_item; //to mpriv
	struct list_head d_node; //to mdev
	struct list_head p_chunk; //to mem pool
	struct mutex mtx;
	void (*release)(struct mdw_mem *m);
};

/* default chunk size of memory pool */
#define MDW_MEM_POOL_CHUNK_SIZE (4*1024*1024)

struct mdw_mem_pool {
	struct mdw_fpriv *mpriv;
	/* pool attribute */
	enum mdw_mem_type type;
	uint64_t flags;
	uint32_t align;
	uint32_t chunk_size;
	/* container and lock */
	struct gen_pool *gp;
	struct mutex m_mtx;
	/* list of resource chunks */
	struct list_head m_chunks;
	/* list of allocated memories from gp */
	struct list_head m_list;
	/* ref count for cmd/mem */
	struct kref m_ref;
	void (*get)(struct mdw_mem_pool *pool);
	void (*put)(struct mdw_mem_pool *pool);
};

struct mdw_dinfo {
	uint32_t type;
	uint32_t num;
	uint8_t meta[MDW_DEV_META_SIZE];
};

enum mdw_driver_type {
	MDW_DRIVER_TYPE_PLATFORM,
	MDW_DRIVER_TYPE_RPMSG,
};

enum mdw_info_type {
	MDW_INFO_KLOG,
	MDW_INFO_ULOG,
	MDW_INFO_PREEMPT_POLICY,
	MDW_INFO_SCHED_POLICY,

	MDW_INFO_NORMAL_TASK_DLA,
	MDW_INFO_NORMAL_TASK_DSP,
	MDW_INFO_NORMAL_TASK_DMA,

	MDW_INFO_MIN_DTIME,
	MDW_INFO_MIN_ETIME,

	MDW_INFO_MAX,
};

enum mdw_info_dir {
	MDW_INFO_SET,
	MDW_INFO_GET
};

enum {
	MDW_POWERPOLICY_DEFAULT = 0, //do nothing
	MDW_POWERPOLICY_SUSTAINABLE = 1,
	MDW_POWERPOLICY_PERFORMANCE = 2,
	MDW_POWERPOLICY_POWERSAVING = 3,
};

enum {
	MDW_APPTYPE_DEFAULT = 0,
	MDW_APPTYPE_ONESHOT = 1,
	MDW_APPTYPE_STREAMING = 2,
};

struct mdw_device {
	enum mdw_driver_type driver_type;
	union {
		struct platform_device *pdev;
		struct rpmsg_device *rpdev;
	};
	struct device *dev;
	struct miscdevice *misc_dev;

	/* init flag */
	bool inited;

	/* cores enable bitmask */
	uint32_t dsp_mask;
	uint32_t dla_mask;
	uint32_t dma_mask;

	/* mdw version */
	uint32_t mdw_ver;
	/* user interface version */
	uint32_t uapi_ver;

	/* device */
	struct apusys_device *adevs[MDW_DEV_MAX];

	/* device support information */
	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	struct mdw_dinfo *dinfos[MDW_DEV_MAX];
	/* memory support information */
	unsigned long mem_mask[BITS_TO_LONGS(MDW_MEM_TYPE_MAX)];
	struct mdw_mem minfos[MDW_MEM_TYPE_MAX];

	/* memory hlist */
	struct list_head m_list;
	struct mutex m_mtx;
	struct mutex mctl_mtx;

	/* device functions */
	const struct mdw_dev_func *dev_funcs;
	void *dev_specific;
};

struct mdw_fpriv {
	struct mdw_device *mdev;

	struct list_head mems;
	struct list_head invokes;
	struct list_head cmds;
	struct mutex mtx;
	struct mdw_mem_pool cmd_buf_pool;

	/* ref count for cmd/mem */
	atomic_t active;
	struct kref ref;
	void (*get)(struct mdw_fpriv *mpriv);
	void (*put)(struct mdw_fpriv *mpriv);
};

struct mdw_exec_info {
	struct mdw_cmd_exec_info c;
	struct mdw_subcmd_exec_info sc;
};

struct mdw_subcmd_kinfo {
	struct mdw_subcmd_info *info; //c->subcmds
	struct mdw_subcmd_cmdbuf *cmdbufs; //from usr
	struct mdw_mem **ori_cbs; //pointer to original cmdbuf
	struct mdw_subcmd_exec_info *sc_einfo;
	uint64_t *kvaddrs; //pointer to duplicated buf
	uint64_t *daddrs; //pointer to duplicated buf
	void *priv; //mdw_ap_sc
};

struct mdw_fence {
	struct dma_fence base_fence;
	struct mdw_device *mdev;
	spinlock_t lock;
};

struct mdw_cmd {
	pid_t pid;
	pid_t tgid;
	uint64_t kid;
	uint64_t uid;
	uint64_t usr_id;
	uint64_t rvid;
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t power_plcy;
	uint32_t power_dtime;
	uint32_t app_type;
	uint32_t num_subcmds;
	struct mdw_subcmd_info *subcmds; //from usr
	struct mdw_subcmd_kinfo *ksubcmds;
	uint32_t num_cmdbufs;
	uint32_t size_cmdbufs;
	struct mdw_mem *cmdbufs;
	struct mdw_mem *exec_infos;
	struct mdw_exec_info *einfos;
	uint8_t *adj_matrix;

	struct mutex mtx;
	struct list_head u_item;

	struct timespec64 start_ts;
	struct timespec64 end_ts;

	struct mdw_fpriv *mpriv;
	int (*complete)(struct mdw_cmd *c, int ret);

	struct mdw_fence *fence;
	struct work_struct t_wk;
	struct dma_fence *wait_fence;
};

struct mdw_dev_func {
	int (*late_init)(struct mdw_device *mdev);
	void (*late_deinit)(struct mdw_device *mdev);
	int (*sw_init)(struct mdw_device *mdev);
	void (*sw_deinit)(struct mdw_device *mdev);
	int (*run_cmd)(struct mdw_fpriv *mpriv, struct mdw_cmd *c);
	int (*set_power)(struct mdw_device *mdev, uint32_t type, uint32_t idx, uint32_t boost);
	int (*ucmd)(struct mdw_device *mdev, uint32_t type, void *vaddr, uint32_t size);
	int (*set_param)(struct mdw_device *mdev, enum mdw_info_type type, uint32_t val);
	uint32_t (*get_info)(struct mdw_device *mdev, enum mdw_info_type type);
	int (*register_device)(struct apusys_device *adev);
	int (*unregister_device)(struct apusys_device *adev);
};

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#define mdw_exception(format, args...) \
	do { \
		pr_info("apusys mdw:" format, ##args); \
		aee_kernel_warning("APUSYS_AP_EXCEPTION_APUSYS_MIDDLEWARE", \
			"\nCRDISPATCH_KEY:APUSYS_MIDDLEWARE\n" format, \
			##args); \
	} while (0)
#define dma_exception(format, args...) \
	do { \
		pr_info("apusys mdw:" format, ##args); \
		aee_kernel_warning("APUSYS_AP_EXCEPTION_APUSYS_MIDDLEWARE", \
			"\nCRDISPATCH_KEY:APUSYS_EDMA\n" format, \
	##args); \
	} while (0)
#else
#define mdw_exception(format, args...)
#define dma_exception(format, args...)
#endif

void mdw_ap_set_func(struct mdw_device *mdev);
void mdw_rv_set_func(struct mdw_device *mdev);

long mdw_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
int mdw_hs_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data);

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv);
void mdw_mem_mpriv_release(struct mdw_fpriv *mpriv);

void mdw_mem_all_print(struct mdw_fpriv *mpriv);

void mdw_mem_put(struct mdw_fpriv *mpriv, struct mdw_mem *m);
struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle);
struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, enum mdw_mem_type type,
	uint32_t size, uint32_t align, uint64_t flags, bool need_handle);
void mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m);
long mdw_mem_set_name(struct mdw_mem *m, const char *buf);
int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_flush(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_invalidate(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_init(struct mdw_device *mdev);
void mdw_mem_deinit(struct mdw_device *mdev);

int mdw_sysfs_init(struct mdw_device *mdev);
void mdw_sysfs_deinit(struct mdw_device *mdev);

int mdw_dbg_init(struct apusys_core_info *info);
void mdw_dbg_deinit(void);

int mdw_dev_init(struct mdw_device *mdev);
void mdw_dev_deinit(struct mdw_device *mdev);
void mdw_dev_session_create(struct mdw_fpriv *mpriv);
void mdw_dev_session_delete(struct mdw_fpriv *mpriv);
int mdw_dev_validation(struct mdw_fpriv *mpriv, uint32_t dtype,
	struct apusys_cmdbuf *cbs, uint32_t num);

#endif
