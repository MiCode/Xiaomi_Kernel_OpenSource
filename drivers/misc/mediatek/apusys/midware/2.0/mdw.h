/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_H__
#define __APUSYS_MDW_H__

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

#include "apusys_drv.h"
#include "apusys_core.h"
#include "apusys_device.h"
#include "mdw_import.h"

//#define MDW_UP_POC_SUPPORT

#define MDW_NAME "apusys"
#define MDW_DEV_MAX (APUSYS_DEVICE_MAX)
#define MDW_CMD_MAX (32)
#define MDW_SUBCMD_MAX (63)
#define MDW_PRIORITY_MAX (32)
#define MDW_DEFAULT_TIMEOUT_MS (30*1000)
#define MDW_BOOST_MAX (100)
#define MDW_DEFAULT_ALIGN (16)

#define MDW_ALIGN(x, align) ((x+align-1) & (~(align-1)))

enum {
	MDW_PARAM_UPLOG,
	MDW_PARAM_PREEMPT_POLICY,
	MDW_PARAM_SCHED_POLICY,

	MDW_PARAM_MAX,
};

struct mdw_fpriv;
struct mdw_device;

enum mdw_mem_type {
	MDW_MEM_TYPE_NONE,
	MDW_MEM_TYPE_INTERNAL,
	MDW_MEM_TYPE_ALLOC,
	MDW_MEM_TYPE_IMPORT,
};

struct mdw_mem {
	/* in */
	unsigned int size;
	unsigned int align;
	uint64_t flags;
	struct mdw_fpriv *mpriv;

	/* out */
	int handle;
	void *vaddr;
	uint64_t device_va;
	uint32_t dva_size;
	void *priv;

	/* control */
	enum mdw_mem_type type;
	bool is_invalid;
	bool is_released;
	struct list_head u_item;
	struct list_head m_item;
	struct kref map_ref;
	void (*release)(struct mdw_mem *m);
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

struct mdw_device {
	enum mdw_driver_type driver_type;
	union {
		struct platform_device *pdev;
		struct rpmsg_device *rpdev;
	};
	struct device *dev;
	struct miscdevice misc_dev;

	uint64_t vlm_start;
	uint32_t vlm_size;

	uint32_t version;
	uint32_t dsp_mask;
	uint32_t dla_mask;
	uint32_t dma_mask;

	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	struct mdw_dinfo *dinfos[MDW_DEV_MAX];

	const struct mdw_dev_func *dev_funcs;
	void *dev_specific;
};

struct mdw_fpriv {
	struct mdw_device *mdev;

	struct list_head mems;
	struct idr cmds_idr;
	struct mutex mtx;
};

struct mdw_subcmd_kinfo {
	struct mdw_subcmd_info *info; //c->subcmds
	struct mdw_subcmd_cmdbuf *cmdbufs; //from usr
	struct mdw_mem **ori_mems; //pointer to original buf
	uint64_t *kvaddrs; //pointer to duplicated buf
	uint64_t *daddrs; //pointer to duplicated buf
	void *priv; //mdw_ap_sc
};

enum mdw_cmd_state {
	MDW_CMD_STATE_IDLE, //ready to exec
	MDW_CMD_STATE_WAIT, //wait for other fence
	MDW_CMD_STATE_RUN, //executing
	MDW_CMD_STATE_DONE, //done
};

struct mdw_fence {
	struct dma_fence base_fence;
	struct mdw_device *mdev;
	void *priv;
};

struct mdw_cmd {
	pid_t pid;
	pid_t tgid;
	int id;
	uint64_t kid;
	uint64_t uid;
	uint64_t usr_id;
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t num_subcmds;
	struct mdw_subcmd_info *subcmds; //from usr
	struct mdw_subcmd_kinfo *ksubcmds;
	uint32_t num_cmdbufs;
	uint32_t size_cmdbufs;
	struct mdw_mem *cmdbufs;
	uint8_t *adj_matrix;

	int state;
	struct mutex mtx;
	struct kref ref;
	spinlock_t fence_lock;

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
	int (*lock)(void);
	int (*unlock)(void);
	int (*set_power)(uint32_t type, uint32_t idx, uint32_t boost);
	int (*ucmd)(uint32_t type, void *vaddr, uint32_t size);
	int (*set_param)(uint32_t idx, uint32_t val);
	uint32_t (*get_param)(uint32_t idx);
};

void mdw_ap_set_func(struct mdw_device *mdev);
void mdw_rv_set_func(struct mdw_device *mdev);

long mdw_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
int mdw_hs_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data);
int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data);

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv);
void mdw_mem_mpriv_release(struct mdw_fpriv *mpriv);

struct mdw_mem *mdw_mem_get(struct mdw_fpriv *mpriv, int handle);
struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint64_t flags, enum mdw_mem_type type);
int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *m);
int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *m);

int mdw_sysfs_init(struct mdw_device *mdev);
void mdw_sysfs_deinit(struct mdw_device *mdev);

int mdw_dbg_init(struct apusys_core_info *info);
void mdw_dbg_deinit(void);
void mdw_dbg_aee(char *name);

int mdw_dev_init(struct mdw_device *mdev);
void mdw_dev_deinit(struct mdw_device *mdev);
int mdw_dev_set_param(struct mdw_device *mdev, uint32_t idx, uint32_t val);
uint32_t mdw_dev_get_param(struct mdw_device *mdev, uint32_t idx);

#endif
