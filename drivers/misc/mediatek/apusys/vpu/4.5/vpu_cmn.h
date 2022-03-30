/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/interrupt.h>
#include <linux/seq_file.h>

#include "apusys_device.h"
#include "apusys_power.h"
#include "apu_bmap.h"
#include "vpu_cfg.h"
#include "vpu_cmd.h"
#include "vpu_mem.h"
#include "vpu_met.h"
#include "vpu_dump.h"
#include "vpu_algo.h"
#include "vpu_ioctl.h"
#include "apu_tags.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#define vpu_aee aee_kernel_exception
#else
#define vpu_aee(...)
#endif

/**
 * vpu_excp_locked() - VPU exception handler
 *
 * @vd: vpu device
 * @req: vpu reqeuest that caused exception, can be NULL (optional)
 * @key: dispatch keyword passed to AEE db
 * @format: exception message format string
 * @args: exception message arguments
 *
 * Note: device lock (vd->lock) must be acquired when calling
 *       vpu_excp_locked()
 */
#define vpu_excp_locked(vd, req, key, format, args...) \
	do { \
		if (vd->state <= VS_DOWN) { \
			pr_info(format ", crashed by other thread", ##args); \
			break; \
		} \
		pr_info(format, ##args); \
		vpu_dmp_create_locked(vd, req, format, ##args); \
		vpu_aee("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
		vpu_pwr_down_locked(vd); \
		vpu_cmd_wake_all(vd); \
	} while (0)

#define vpu_excp(vd, req, key, format, args...) \
	do { \
		mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV); \
		vpu_excp_locked(vd, req, key, format, ##args); \
		mutex_unlock(&vd->lock); \
	} while (0)

#define vpu_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		vpu_aee("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)

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

struct vpu_platform;
struct vpu_device;

// driver common data
struct vpu_driver {
	int bin_type;
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

	/* memory */
	struct apu_bmap ab;  /* bitmap used by v2 allocator */
	struct list_head vi; /* list of all mapped vpu_iova */
	struct mutex vi_lock;

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

	/* platform data */
	struct vpu_platform *vp;
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

#define vd_is_available(vd)  \
	(!((vd)->state >= VS_REMOVING || (vd)->state <= VS_DISALBED))

struct vpu_iomem {
	void __iomem *m;
	struct resource *res;
};

struct vpu_bin_ops {
	/* Image header */
	void *(*header)(int index);

	/* Normal algorithm */
	struct vpu_algo_info *(*alg_info)(void *header, int j);
	int (*alg_info_cnt)(void *header);

	/* Preload algorithm */
	uint32_t (*pre_info)(void *header);
	int (*pre_info_cnt)(void *header);
};

struct vpu_sys_ops {
	/* XOS */
	int (*xos_lock)(struct vpu_device *vd);
	void (*xos_unlock)(struct vpu_device *vd);
	int (*xos_wait_idle)(struct vpu_device *vd);

	/* ISR */
	irqreturn_t (*isr)(int irq, void *dev_id);
	wait_queue_head_t* (*isr_check_cmd)(struct vpu_device *vd,
		uint32_t inf, uint32_t prio);
	bool (*isr_check_unlock)(uint32_t inf);
};

struct vpu_mem_ops {
	int (*init)(void);
	void (*exit)(void);
	dma_addr_t (*alloc)(struct device *dev, struct vpu_iova *i);
	void (*free)(struct device *dev, struct vpu_iova *i);
	dma_addr_t (*map_sg_to_iova)(struct device *dev,
		struct scatterlist *sg,	unsigned int nents,
		size_t len, dma_addr_t given_iova);
	void (*unmap_iova_from_sg)(struct device *dev, struct vpu_iova *i);
	void (*sync_for_device)(struct device *dev,	struct vpu_iova *i);
	void (*sync_for_cpu)(struct device *dev, struct vpu_iova *i);
	int (*dts)(struct device *dev,
		const char *name, struct vpu_iova *i);
	void (*show)(struct seq_file *s);
};

struct vpu_misc_ops {
	void (*emi_mpu_set)(unsigned long start, unsigned int size);
	bool (*is_disabled)(struct vpu_device *vd);
};

struct vpu_register {
	uint32_t mbox_inbox_0;
	uint32_t mbox_inbox_1;
	uint32_t mbox_inbox_2;
	uint32_t mbox_inbox_3;
	uint32_t mbox_inbox_4;
	uint32_t mbox_inbox_5;
	uint32_t mbox_inbox_6;
	uint32_t mbox_inbox_7;
	uint32_t mbox_inbox_8;
	uint32_t mbox_inbox_9;
	uint32_t mbox_inbox_10;
	uint32_t mbox_inbox_11;
	uint32_t mbox_inbox_12;
	uint32_t mbox_inbox_13;
	uint32_t mbox_inbox_14;
	uint32_t mbox_inbox_15;
	uint32_t mbox_inbox_16;
	uint32_t mbox_inbox_17;
	uint32_t mbox_inbox_18;
	uint32_t mbox_inbox_19;
	uint32_t mbox_inbox_20;
	uint32_t mbox_inbox_21;
	uint32_t mbox_inbox_22;
	uint32_t mbox_inbox_23;
	uint32_t mbox_inbox_24;
	uint32_t mbox_inbox_25;
	uint32_t mbox_inbox_26;
	uint32_t mbox_inbox_27;
	uint32_t mbox_inbox_28;
	uint32_t mbox_inbox_29;
	uint32_t mbox_inbox_30;
	uint32_t mbox_inbox_31;
	uint32_t mbox_dummy_0;
	uint32_t mbox_dummy_1;
	uint32_t mbox_dummy_2;
	uint32_t mbox_dummy_3;
	uint32_t mbox_dummy_4;
	uint32_t mbox_dummy_5;
	uint32_t mbox_dummy_6;
	uint32_t mbox_dummy_7;
	uint32_t mbox_inbox_irq;
	uint32_t mbox_inbox_mask;
	uint32_t mbox_inbox_pri_mask;
	uint32_t cg_con;
	uint32_t cg_clr;
	uint32_t sw_rst;
	uint32_t done_st;
	uint32_t ctrl;
	uint32_t xtensa_int;
	uint32_t ctl_xtensa_int;
	uint32_t default0;
	uint32_t default1;
	uint32_t default2;
	uint32_t xtensa_info00;
	uint32_t xtensa_info01;
	uint32_t xtensa_info02;
	uint32_t xtensa_info03;
	uint32_t xtensa_info04;
	uint32_t xtensa_info05;
	uint32_t xtensa_info06;
	uint32_t xtensa_info07;
	uint32_t xtensa_info08;
	uint32_t xtensa_info09;
	uint32_t xtensa_info10;
	uint32_t xtensa_info11;
	uint32_t xtensa_info12;
	uint32_t xtensa_info13;
	uint32_t xtensa_info14;
	uint32_t xtensa_info15;
	uint32_t xtensa_info16;
	uint32_t xtensa_info17;
	uint32_t xtensa_info18;
	uint32_t xtensa_info19;
	uint32_t xtensa_info20;
	uint32_t xtensa_info21;
	uint32_t xtensa_info22;
	uint32_t xtensa_info23;
	uint32_t xtensa_info24;
	uint32_t xtensa_info25;
	uint32_t xtensa_info26;
	uint32_t xtensa_info27;
	uint32_t xtensa_info28;
	uint32_t xtensa_info29;
	uint32_t xtensa_info30;
	uint32_t xtensa_info31;
	uint32_t debug_info00;
	uint32_t debug_info01;
	uint32_t debug_info02;
	uint32_t debug_info03;
	uint32_t debug_info04;
	uint32_t debug_info05;
	uint32_t debug_info06;
	uint32_t debug_info07;
	uint32_t xtensa_altresetvec;

	/* Register Config: CTRL */
	uint32_t p_debug_enable;
	uint32_t state_vector_select;
	uint32_t pbclk_enable;
	uint32_t prid;
	uint32_t pif_gated;
	uint32_t stall;

	/* Register Config: SW_RST */
	uint32_t apu_d_rst;
	uint32_t apu_b_rst;
	uint32_t ocdhaltonreset;

	/* Register Config: DEFAULT0 */
	uint32_t aruser;
	uint32_t awuser;
	uint32_t qos_swap;

	/* Register Config: DEFAULT1 */
	uint32_t aruser_idma;
	uint32_t awuser_idma;

	/* Register Config: DEFAULT2 */
	uint32_t dbg_en;

	/* Register Config: CG_CLR */
	uint32_t jtag_cg_clr;

	/* Register Mask: DONE_ST */
	uint32_t pwaitmode;
};

struct vpu_config {
	uint32_t host_ver;
	uint64_t iova_bank;
	uint32_t iova_start;
	uint32_t iova_heap; // Heap begin address for dynamic allocated iova
	uint32_t iova_end;

	uint32_t bin_type;  // VPU_IMG_LEGACY, VPU_IMG_PRELOAD

	uint32_t bin_sz_code;
	uint32_t bin_ofs_algo;
	uint32_t bin_ofs_imem;
	uint32_t bin_ofs_header;

	uint32_t cmd_timeout;
	uint32_t pw_off_latency_ms;

	uint32_t wait_cmd_latency_us;
	uint32_t wait_cmd_retry;

	uint32_t wait_xos_latency_us;
	uint32_t wait_xos_retry;

	uint32_t xos;
	uint32_t xos_timeout;
	uint32_t max_prio;

	/* Log Buffer */
	uint32_t log_ofs;
	uint32_t log_header_sz;

	/* Dump: Sizes */
	uint32_t dmp_sz_reset;
	uint32_t dmp_sz_main;
	uint32_t dmp_sz_kernel;
	uint32_t dmp_sz_preload;
	uint32_t dmp_sz_iram;
	uint32_t dmp_sz_work;
	uint32_t dmp_sz_reg;
	uint32_t dmp_sz_imem;
	uint32_t dmp_sz_dmem;

	/* Dump: Registers */
	uint32_t dmp_reg_cnt_info;
	uint32_t dmp_reg_cnt_dbg;
	uint32_t dmp_reg_cnt_mbox;
};

// driver platform data
struct vpu_platform {
	struct vpu_bin_ops *bops;
	struct vpu_mem_ops *mops;
	struct vpu_sys_ops *sops;
	struct vpu_misc_ops *cops;
	struct vpu_register *reg;
	struct vpu_config *cfg;
};

// device data
struct vpu_device {
	/* APU power management data. MUST be placed at the begin. */
	struct apu_dev_power_data pd;

	/* general */
	int id;
	char name[8];
	struct list_head list;   /* link in vpu driver */
	struct vpu_driver *drv;
	enum vpu_state state;
	struct mutex lock;
	struct apusys_device adev;
	struct apusys_device adev_rt;
	struct device *dev;
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
#if IS_ENABLED(CONFIG_PM_SLEEP)
	struct wakeup_source *ws;
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

#define __vp (vpu_drv->vp)
#define vd_reg(vd)	(__vp->reg)
#define vd_cfg(vd)	(__vp->cfg)
#define vd_mops(vd)	(__vp->mops)
#define vd_cops(vd)	(__vp->cops)
#define vd_sops(vd)	(__vp->sops)
#define vd_bops(vd)	(__vp->bops)

#define xos_type(vd)	(vd_cfg(vd)->xos)
#define bin_type(vd)	(vd_cfg(vd)->bin_type)

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

