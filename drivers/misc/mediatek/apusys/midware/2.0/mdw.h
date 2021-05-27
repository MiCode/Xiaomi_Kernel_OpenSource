/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_H__
#define __APUSYS_MDW_H__

#include <linux/cdev.h>
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

struct mdw_mem {
	int handle;
	void *vaddr;
	unsigned int size;
	uint64_t device_va;
	unsigned int align;

	uint64_t flags;
	uint8_t cacheable;
	struct list_head u_item;
	struct list_head m_item;

	bool is_alloced;
	bool is_user;
	struct kref ref;
	struct mdw_fpriv *mpriv;

	void *priv;
};

struct mdw_dinfo {
	uint32_t type;
	uint32_t num;
	char meta[MDW_DEV_META_SIZE];
};

struct mdw_device {
	struct platform_device *pdev;
	struct cdev cdev;
	struct device *dev;
	uint32_t major;

	uint64_t vlm_start;
	uint32_t vlm_size;

	uint32_t version;
	uint32_t dsp_mask;
	uint32_t dla_mask;
	uint32_t dma_mask;

	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	struct mdw_dinfo *dinfos[MDW_DEV_MAX];

	const struct mdw_dev_func *dev_funcs;
	struct list_head iommu_tables;
};

struct mdw_fpriv {
	struct mdw_device *mdev;

	struct list_head mems;
	struct idr cmds_idr;
	struct mutex mtx;
};

struct mdw_subcmd_kinfo {
	struct mdw_subcmd_info *info;
	struct mdw_subcmd_cmdbuf *cmdbufs;
	uint64_t *kvaddrs;
	uint64_t *daddrs;
	void *priv; //mdw_ap_sc
};

enum mdw_cmd_state {
	MDW_CMD_STATE_IDLE,
	MDW_CMD_STATE_RUN,
	MDW_CMD_STATE_DONE,
	MDW_CMD_STATE_ABORT,
};

struct mdw_cmd {
	int id;
	uint64_t kid;
	pid_t exec_pid;
	uint64_t uid;
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t num_subcmds;
	struct mdw_subcmd_info *subcmds;
	struct mdw_subcmd_kinfo *ksubcmds;
	uint32_t num_cmdbufs;
	uint32_t size_cmdbufs;
	int state;
	//struct kref ref;
	struct mutex mtx;
	bool is_executed;
	uint8_t *adj_matrix;
	struct mdw_mem *cmdbufs;
	void *priv;
	struct mdw_fpriv *mpriv;
	int (*complete)(struct mdw_cmd *c, int ret);

	struct dma_fence base_fence;
	spinlock_t lock;
};

struct mdw_dev_func {
	int (*sw_init)(struct mdw_device *mdev);
	void (*sw_deinit)(struct mdw_device *mdev);
	int (*late_init)(struct mdw_device *mdev);
	void (*late_deinit)(struct mdw_device *mdev);

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

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, uint32_t size,
	uint32_t align, uint8_t cacheable);
int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_map(struct mdw_fpriv *mpriv, struct mdw_mem *mem);
int mdw_mem_unmap(struct mdw_fpriv *mpriv, struct mdw_mem *mem);

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
