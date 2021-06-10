/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_CMN_H__
#define __VPU_CMN_H__

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>

#include "apusys_device.h"
#include "vpu_cfg.h"
#include "vpu_cmd.h"
#include "vpu_mem.h"
#include "vpu_met.h"
#include "vpu_dump.h"
#include "vpu_algo.h"
#include "vpu_ioctl.h"
#include "apu_tags.h"

#include <aee.h>

#ifdef CONFIG_MTK_AEE_FEATURE
/**
 * vpu_aee_excp_locked() - VPU exception handler
 *
 * @vd: vpu device
 * @req: vpu reqeuest that caused exception, can be NULL (optional)
 * @key: dispatch keyword passed to AEE db
 * @format: exception message format string
 * @args: exception message arguments
 *
 * Note: device lock (vd->lock) must be acquired when calling
 *       vpu_aee_excp_locked()
 */
#define vpu_aee_excp_locked(vd, req, key, format, args...) \
	do { \
		if (vd->state <= VS_DOWN) { \
			pr_info(format ", crashed by other thread", ##args); \
			break; \
		} \
		pr_info(format, ##args); \
		vpu_dmp_create_locked(vd, req, format, ##args); \
		aee_kernel_exception("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
		vpu_pwr_down_locked(vd); \
		vpu_cmd_wake_all(vd); \
	} while (0)

#define vpu_aee_excp(vd, req, key, format, args...) \
	do { \
		mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV); \
		vpu_aee_excp_locked(vd, req, key, format, ##args); \
		mutex_unlock(&vd->lock); \
	} while (0)

#define vpu_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define vpu_aee_excp_locked(vd, req, key, format, args...)
#define vpu_aee_excp(vd, req, key, format, args...)
#define vpu_aee_warn(key, format, args...)
#endif

/*
 * mutex lock order
 * 1. driver lock (vpu_drv->lock)
 *    - protects driver's data (vpu_drv)
 * 2. command lock (vd->cmd[i].lock)
 *    - protects device's command execution of priority "i"
 * 3. device lock (vd->lock)
 *    - protects device's power control, and boot sequence
 **/

enum vpu_mutex_class {
	VPU_MUTEX_DRV = 0,
	VPU_MUTEX_CMD = 1,
	VPU_MUTEX_DEV = (VPU_MAX_PRIORITY + VPU_MUTEX_CMD),
};

struct vpu_met_hrt {
	spinlock_t lock;
	struct hrtimer t;
	struct kref ref;
	uint64_t latency;
};

// driver data
struct vpu_driver {
	void *bin_va;
	unsigned long bin_pa;
	unsigned int bin_size;
	unsigned int bin_head_ofs;
	unsigned int bin_preload_ofs;

	/* power work queue */
	struct workqueue_struct *wq;

	/* iova settings */
	struct kref iova_ref;
	struct device *iova_dev;
	struct vpu_iova iova_algo;
	struct vpu_iova iova_share;

	/* shared */
	uint64_t mva_algo;

	/* list of devices */
	struct list_head devs;
	struct mutex lock;

	/* debugfs entry */
	struct dentry *droot;

	/* procfs entry */
	struct proc_dir_entry *proc_root;

	/* device references */
	struct kref ref;

	/* met */
	uint32_t ilog;
	uint32_t met;
	struct vpu_met_hrt met_hrt;

	/* tags */
	struct apu_tags *tags;
};

enum vpu_state {
	VS_UNKNOWN = 0,
	VS_DISALBED,   // disabled by e-fuse
	VS_DOWN,       // power down
	VS_UP,         // power up
	VS_BOOT,       // booting
	VS_IDLE,       // power on, idle
	VS_CMD_ALG,
	VS_CMD_D2D,
	VS_CMD_D2D_EXT,
	VS_REMOVING,
	VS_TOTAL
};

struct vpu_iomem {
	void __iomem *m;
	struct resource *res;
};

// device data
struct vpu_device {
	int id;
	char name[8];
	struct list_head list;   // link in vpu driver
	enum vpu_state state;
	struct mutex lock;
	struct apusys_device adev;
	struct apusys_device adev_rt;
	struct device *dev;      // platform device
	struct rproc *rproc;

	/* xos */
	atomic_t xos_state;

	/* iomem */
	spinlock_t reg_lock;
	struct vpu_iomem reg;
	struct vpu_iomem dmem;
	struct vpu_iomem imem;
	struct vpu_iomem dbg;

	/* power */
	struct kref pw_ref;
	struct delayed_work pw_off_work;
	wait_queue_head_t pw_wait;
	uint64_t pw_off_latency;   /* ms, 0 = always on */
	atomic_t pw_boost;         /* current boost */
#ifdef CONFIG_PM_SLEEP
	struct wakeup_source pw_wake_lock;
#endif

	/* iova settings */
	struct vpu_iova iova_reset;
	struct vpu_iova iova_main;
	struct vpu_iova iova_kernel;
	struct vpu_iova iova_iram;
	struct vpu_iova iova_work;

	/* work buffer */
	uint32_t wb_log_size;
	uint32_t wb_log_data;

	/* algorithm */
	struct vpu_algo_list aln;  /* normal */
	struct vpu_algo_list alp;  /* preload */

	/* irq */
	unsigned int irq_num;

	/* command */
	atomic_t cmd_prio;     /* current running command's priority */
	int cmd_prio_max;      /* eq. VPU_MAX_PRIORITY */
	struct vpu_cmd_ctl cmd[VPU_MAX_PRIORITY];
	atomic_t cmd_active;   /* number of active command controls */
	uint32_t dev_state;    /* last known device state */
	uint64_t cmd_timeout;  /* ms */

	/* memory */
	uint64_t mva_iram;

	/* debug */
	struct dentry *droot;  /* debugfs entry */
	struct proc_dir_entry *proc_root; /* procfs entry */
	bool ftrace_avail;     /* trace */
	bool jtag_enabled;     /* jtag */
	struct vpu_dmp *dmp;   /* dump */

	/* MET */
	struct vpu_met_work met;
	struct vpu_met_pm pm;
};


extern struct vpu_driver *vpu_drv;

int vpu_init_dev_hw(struct platform_device *pdev, struct vpu_device *vd);
int vpu_init_drv_hw(void);

int vpu_exit_dev_hw(struct platform_device *pdev, struct vpu_device *vd);
int vpu_exit_drv_hw(void);

int vpu_alloc_request(struct vpu_request **rreq);
int vpu_free_request(struct vpu_request *req);

int vpu_alloc_algo(struct __vpu_algo **ralgo);
int vpu_free_algo(struct __vpu_algo *algo);

int vpu_execute(struct vpu_device *vd, struct vpu_request *req);
int vpu_preempt(struct vpu_device *vd, struct vpu_request *req);

int vpu_kbuf_alloc(struct vpu_device *vd);
int vpu_kbuf_free(struct vpu_device *vd);
#endif

