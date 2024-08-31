// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#ifndef __XM_ISPV_RPROC_H
#define __XM_ISPV_RPROC_H

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/sched.h>
#include <linux/mailbox_client.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <ispv4_pcie_hdma.h>
#include <linux/thermal.h>
#include <linux/debugfs.h>
#include <ispv4_regops.h>
#include "remoteproc_internal.h"
#include <linux/delay.h>

#define XM_ISPV4_FW_NAME "ispv4_firmware"
#define XM_ISPV4_LOGSEG_NAME ".logbuf"
#define XM_ISPV4_LOCKINFOSEG_NAME ".lockinfo"
#define XM_ISPV4_BOOTINFOSEG_NAME ".bootinfo"

#define XM_ISPV4_DDRTF_SIZE (150 * 1024)
#define XM_ISPV4_DDRTF_REALSIZE (104 * 1024)
#define XM_ISPV4_DDRTF_IRAM_OFF 0x11c000

struct xm_ispv4_ops;
struct xm_ispv4_rproc;

enum xm_ispv4_rproc_state {
	XM_ISPV4_RPROC_STATE_OFFLINE,
	XM_ISPV4_RPROC_STATE_RUN,
	XM_ISPV4_RPROC_STATE_MAX,
};

#define RP_SPI(rp) (rp->ops == &ispv4_spi_ops)
#define RP_PCI(rp) (rp->ops == &ispv4_pci_ops)
#define RP_FAKE(rp) (rp->ops == &ispv4_fake_ops)

#define XM_RPROC_MEM_FLAG_DEV_MEM 0x01
#define XM_RPROC_MEM_FLAG_AP_MEM 0x02
#define XM_RPROC_MEM_FLAG_IOMMU_MAPED 0x04
#define XM_RPROC_MEM_FLAG_MMU_MAPED 0x08

struct xm_ispv4_rproc_mem {
	// AP virt address.
	void *virt_addr;
	// AP phy address.
	phys_addr_t phys_addr;
	// dma address by iommu.
	dma_addr_t dma_addr;
	// ispv4 address.
	dma_addr_t dev_addr;
	size_t size;
	size_t flag;
};

#define XM_ISPV4_RPROC_MEM_MAX 8

#define XM_ISPV4_IPC_EPT_RPMSG_ISP_NAME "rpmsg-isp"
#define XM_ISPV4_IPC_EPT_RPMSG_ASST_NAME "rpmsg-asst"

typedef int (*xm_ipc_cb_t)(void *priv, void *data, int epts, int len,
			   uint32_t addr);
typedef int (*xm_crash_nf_t)(void *thiz, int type);

typedef int (*xm_rpmsg_ready_nf_t)(void *thiz, int ept, bool st);

struct rpept_wt_ack {
	ktime_t touttime;
	struct list_head node;
	u32 msg_header_id;
};

struct rpept_recv_msg {
	struct list_head node;
	u32 len_of_data;
	u8 data[0];
};

#define RPEPT_RECV_MSG_QSIZE 64

struct ispv4_rpept_dev {
	struct rpmsg_device *rpdev;
	struct device dev;

	struct mutex up_lock;
	struct list_head waiting_ack;

	struct list_head up_msgs;
	struct list_head up_rets;
	struct list_head up_errs;

	struct timer_list cacktimer;
	struct work_struct cackwork;

	atomic_t msg_id;
	struct xm_ispv4_rproc *rp;
	int idx;
	struct completion cmd_complete;
};

struct ispv4_elog_dev {
	struct cdev cdev;
	struct device dev;
	atomic_t opened;
	struct xm_ispv4_rproc *rp;
};

struct xm_ispv4_rproc {
	struct rproc *rproc;
	struct device *dev;
	uint32_t magic_num;
	/* Read fw, remove checknum */
	struct firmware *fw;
	/* Object which fw point to */
	struct firmware fw_obj;
	/* Alloc from FS, elf with checknum */
	const struct firmware *origin_fw;
	bool prepare_fw_finish;
	struct xm_ispv4_ops *ops;

	uint32_t ramlog_da;
	uint32_t ramlog_buf_size;
	/* Ramlog restore buffer */
	void *ramlog_buf;
	/* Ramlog direct pushed addr for ap-cpu */
	void *ramlog_dma;
	/* Ramlog direct pushed addr for ep-cpu */
	u32 ramlog_dma_da;
	bool ramlog_dumped;

	uint32_t debuginfo_da;
	uint32_t debuginfo_buf_size;
	void *debuginfo_buf;
	bool debuginfo_dumped;

	uint32_t bootinfo_da;
	uint32_t bootinfo_buf_size;
	void *bootinfo_buf;
	bool bootinfo_dumped;

	void *ddrtf_buf;
	dma_addr_t ddrtf_iova;
	bool ddr_buf_avalid;
	bool ddr_buf_update;

	struct bin_attribute ramlog_attr;
	struct debugfs_blob_wrapper ramlog_blob;
	struct dentry *ramlog_file;

	struct bin_attribute debuginfo_attr;
	struct debugfs_blob_wrapper debuginfo_blob;
	struct dentry *debuginfo_file;

	struct bin_attribute bootinfo_attr;
	struct debugfs_blob_wrapper bootinfo_blob;
	struct dentry *bootinfo_file;

	struct bin_attribute ddrt_attr;
	struct debugfs_blob_wrapper ddrt_blob;
	struct dentry *ddrt_file;

	uint32_t bootaddr;

	bool reload_fw;
	struct dentry *debug_dir;
	struct dentry *debug_dump_tr;
	struct dentry *debug_boot;

	xm_crash_nf_t crash_notify;
	void *crash_notify_thiz;
	xm_ipc_cb_t ipc_ept_recv_notify;
	void *ipc_ept_recv_notify_thiz;

	xm_rpmsg_ready_nf_t rpmsg_ready_cb;
	void *rpmsg_ready_cb_thiz;
	u64 rpmsg_kick_idx;
	struct work_struct rpmsg_recvwork;

	struct work_struct crash;
	int crash_type;

	enum xm_ispv4_rproc_state state;

	struct task_struct *rpmsg_th;
	spinlock_t rpmsg_recv_lock;
	uint32_t rpmsg_recv_count;

	struct xm_ispv4_rproc_mem memlist[XM_ISPV4_RPROC_MEM_MAX];

	struct xm_ispv4_rproc_mem ipc_memlist;

	void *ipc_epts[XM_ISPV4_IPC_EPT_MAX];
	bool ipc_stopsend[XM_ISPV4_IPC_EPT_MAX];
	struct mutex send_mbox_lock;

	// Crash send irq by msi
	int irq_crash;

	// Change to mbox
	int irq_ipc;

	bool rpmsg_inited;

	struct mbox_client mbox_exception;
	struct mbox_chan *mbox_exception_chan;

	int (*mbox_excep_notify)(void *priv,
				 void *data, int len);
	void *mbox_excep_notify_priv;

	struct mbox_client mbox_rpmsg;
	struct mbox_chan *mbox_rpmsg_chan;

	struct mbox_client mbox_log;
	struct mbox_chan *mbox_log_chan;

#define BOOT_STAGE0 0
#define BOOT_STAGE1 1
#define BOOT_STAGE2 2
#define BOOT_DDR_QUICK_BOOT_OK 8
#define BOOT_DDR_FULL_BOOT 9
#define BOOT_DDR_FULL_BOOT_OK 10
#define BOOT_DDR_FULL_BOOT_FAIL 10
#define BOOT_STAGE_ERR 0xff

#define BOOT_INFO_DDR_SAVE_DDRTF 0x1

	struct mbox_client mbox_boot;
	struct mbox_chan *mbox_boot_chan;
	int boot_stage;
	int boot_stage_info;
	struct completion cpl_boot;
	struct task_struct *fullboot_th;
	int (*notify_updateddr)(void *, int);
	void *notify_updateddr_priv;

	/*rpmsg char device*/
	// struct class *rpept_class[XM_ISPV4_IPC_EPT_MAX];
	// dev_t rpept_devt[XM_ISPV4_IPC_EPT_MAX];
	struct ispv4_rpept_dev *rpeptdev[XM_ISPV4_IPC_EPT_MAX];
	struct mutex rpeptdev_lock[XM_ISPV4_IPC_EPT_MAX];

	void (*rpmsgnotify[XM_ISPV4_IPC_EPT_MAX])(void *priv, void *id,
						  void *data, int len);
	void *rpmsgnotify_priv[XM_ISPV4_IPC_EPT_MAX];

	struct device comp_dev;
	struct device *pci_dev;
	struct pcie_hdma *pdma;
	bool met_crash;

	void (*krpmsg_notify)(void *priv, void *data, int len);
	void *krpmsg_notify_priv;

	struct thermal_zone_device *tz_dev;
};

struct ispv4_dev_addr {
	uint32_t start;
	uint32_t size;
};

enum xm_ispv4_memregion {
	ISPV4_MEMREGION_SRAM,
	ISPV4_MEMREGION_DDR,
	ISPV4_MEMREGION_MAX,
	ISPV4_MEMREGION_UNKNOWN,
};

#define BOOT_ERROR_INFO (14)
#define BOOT_RET_INFO (15)

#define BOOT_ERR_DDR_FAIL (1 << 0)

#define BOOT_PASS_1STAGE (1 | (1 << 1))
#define BOOT_PASS_2STAGE (1 | (2 << 1))
#define BOOT_FAIL_1STAGE (0 | (1 << 1))
#define BOOT_FAIL_2STAGE (0 | (2 << 1))

#define DDR_QUICK_BOOT 0
#define DDR_FULL_BOOT 1

struct xm_ispv4_ops {
	int (*init)(struct xm_ispv4_rproc *zisp_rproc);
	void (*deinit)(struct xm_ispv4_rproc *zisp_rproc);
	int (*boot)(struct xm_ispv4_rproc *zisp_rproc);
	void (*earlydown)(struct xm_ispv4_rproc *zisp_rproc);
	void (*deboot)(struct xm_ispv4_rproc *zisp_rproc);
	void (*shutdown)(struct xm_ispv4_rproc *zisp_rproc);
	void (*remove)(struct xm_ispv4_rproc *zisp_rproc);
	// int (*parse_regmap)(struct platform_device *pdev,
	// 		    void __iomem *regs);
	// int (*parse_mem)(struct platform_device *pdev,
	// 		 struct resource **_iores);
	// int (*parse_irq)(struct platform_device *pdev,
	// 		 int irq);
	struct rproc_ops *rproc_ops;
};

extern struct xm_ispv4_ops ispv4_pci_ops;
extern struct xm_ispv4_ops ispv4_spi_ops;
extern struct xm_ispv4_ops ispv4_fake_ops;
int ispv4_load_rsc_table(struct rproc *rproc, const struct firmware *fw);
irqreturn_t xm_ispv4_rpmsg_irq(int irq, void *data);
void xm_ispv4_rpmsg_mbox_cb(struct mbox_client *cl, void *mssg);
irqreturn_t ispv4_crash_irq(int irq, void *p);
int xm_ispv4_rpmsg_init(struct xm_ispv4_rproc *rp);
void xm_ispv4_rpmsg_exit(struct xm_ispv4_rproc *rp);
void xm_ispv4_flush_send_stop(struct xm_ispv4_rproc *rp);
void xm_ispv4_rpmsg_stopdeal(struct xm_ispv4_rproc *rp);
int ispv4_elog_init(struct xm_ispv4_rproc *rp);
void ispv4_elog_exit(struct xm_ispv4_rproc *rp);
void ispv4_rproc_pci_earlydown(struct xm_ispv4_rproc *rp);

static inline int enable_cpu_pmu(void)
{
	int ret = 0;
	int read_val = 0;
	int retry = 100;

	ret = ispv4_regops_write(0xD451218, 0x1);
	if (ret != 0)
		return ret;

	while (ret == 0 && retry-- > 0 &&
	       FIELD_GET(GENMASK(3, 0), read_val) != 0x3) {
		ret = ispv4_regops_read(0xD451214, &read_val);
		msleep(1);
	}

	if (retry <= 0)
		ret = -ETIMEDOUT;

	return ret;
}

static inline int enable_cpu_pmu_pcie(void* virt_addr)
{
	int ret = 0;
	int read_val = 0;
	int retry = 100;

	writel(0x1, virt_addr + 0x51218);

	while (ret == 0 && retry-- > 0 &&
	       FIELD_GET(GENMASK(3, 0), read_val) != 0x3) {
		usleep_range(300, 500);
		read_val = readl(virt_addr + 0x51214);
	}

	if (retry <= 0)
		ret = -ETIMEDOUT;

	return ret;
}

#endif
