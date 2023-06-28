/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_DRIVER_H__
#define __ADSP_PLATFORM_DRIVER_H__

#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include "adsp_platform.h"
#include "adsp_clk.h"
#include "adsp_helper.h"

struct adsp_priv;
struct log_ctrl_s;
struct sharedmem_info {
	unsigned int offset;
	unsigned int size;
};

struct adsp_core_operations {
	int (*initialize)(struct adsp_priv *pdata);
	int (*after_bootup)(struct adsp_priv *pdata);
};

struct adsp_core_description {
	u32 id;
	const char *name;
	const struct sharedmem_info sharedmems[ADSP_SHAREDMEM_NUM];
	const struct adsp_core_operations ops;
};

struct adspsys_description {
	const char *platform_name;
	const u32 version;
	const int semaphore_ways;
	const int semaphore_ctrl;
	const int semaphore_retry;
	const u32 axibus_idle_val;
};

struct irq_t {
	u32 cid;
	u32 seq;
	void (*clear_irq)(u32 cid);
	void (*irq_cb)(int irq, void *data, int cid);
	void (*thread_fn)(int irq, void *data, int cid);
	void *data;
};

struct adsp_priv {
	u32 id;
	const char *name;
	int state;
	unsigned int feature_set;

	/* address & size */
	void __iomem *itcm;
	void __iomem *dtcm;
	void __iomem *sysram;
	size_t itcm_size;
	size_t dtcm_size;
	size_t sysram_size;
	phys_addr_t sysram_phys;

	const struct sharedmem_info *mapping_table;

	/* irq */
	struct irq_t irq[ADSP_IRQ_NUM];
	/* mailbox info */
	struct mtk_mbox_pin_send *send_mbox;
	struct mtk_mbox_pin_recv *recv_mbox;

	/* logger control */
	struct log_ctrl_s *log_ctrl;

	struct device *dev;
	struct dentry *debugfs;
	struct miscdevice mdev;
	struct workqueue_struct *wq;
	struct completion done;

	/* method */
	const struct adsp_core_operations *ops;
};

struct adspsys_priv {
	u32 num_cores;

	/* address & size */
	void __iomem *cfg;
	void __iomem *cfg_secure;
	void __iomem *cfg2;
	size_t cfg_size;
	size_t cfg_secure_size;
	size_t cfg2_size;

	struct device *dev;
	struct miscdevice mdev;
	struct wait_queue_head waitq;
	struct workqueue_struct *workq;

	struct adsp_clk_operations clk_ops;

	const struct adspsys_description *desc;
};

struct adsp_c2c_share_dram_info_t {
	u32 share_dram_addr;
	u32 share_dram_size;
};

extern const struct file_operations adspsys_file_ops;
extern struct attribute_group adsp_excep_attr_group;
extern const struct file_operations adsp_debug_ops;
extern const struct file_operations adsp_core_file_ops;
extern struct attribute_group adsp_default_attr_group;

extern struct adsp_priv *adsp_cores[ADSP_CORE_TOTAL];
extern struct adspsys_priv *adspsys;

#endif
