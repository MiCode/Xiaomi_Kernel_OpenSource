/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef MTK_FMT_H
#define MTK_FMT_H
#include <mt-plat/sync_write.h>
#include <linux/time.h>

#define FMT_CMDQ_CMD_MAX        (2048)
#define VDEC_FMT_DEVNAME "vdec-fmt"
#define FMT_CORE_NUM           2
#define GCE_EVENT_MAX           8
#define FMT_MAP_HW_REG_NUM      6
#define FMT_INST_MAX            64
#define GCE_PENDING_CNT         10
#define MAP_PA_BASE_1GB  0x40000000 /* < 1GB registers */
#define SUSPEND_TIMEOUT_CNT     5000

#define FMT_REG_GET32(addr)		(readl((void *)addr) & 0xFFFFFFFF)
#define FMT_REG_SET32(addr, val)	mt_reg_sync_writel(val, (addr))


struct gce_cmds {
	u8 cmd[FMT_CMDQ_CMD_MAX];
	u64 addr[FMT_CMDQ_CMD_MAX];
	u64 data[FMT_CMDQ_CMD_MAX];
	u32 mask[FMT_CMDQ_CMD_MAX];
	u32 dma_offset[FMT_CMDQ_CMD_MAX];
	u32 dma_size[FMT_CMDQ_CMD_MAX];
	u32 cmd_cnt;
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
	unsigned long va;
};

struct fmt_pmqos {
	u32 tv_sec;
	u32 tv_usec;
	u32 pixel_size;
	u32 rdma_datasize;
	u32 wdma_datasize;
};

struct gce_cmdq_obj {
	u64 cmds_user_ptr;
	u32 identifier;
	u32 secure;
	u32 *taskid;
	struct fmt_pmqos pmqos_param;
};

#ifdef CONFIG_COMPAT
struct compat_gce_cmdq_obj {
	u64 cmds_user_ptr;
	u32 identifier;
	u32 secure;
	compat_uptr_t taskid;
	struct fmt_pmqos pmqos_param;
};
#endif


struct gce_cmdq_task {
	struct gce_cmdq_obj cmdq_buff;
	struct cmdq_pkt *pkt_ptr;
	u32 identifier;
	u32 used;
};

enum gce_cmd_id {
	CMD_READ = 0,   /* read register */
	CMD_WRITE,      /* write register */
	/* polling register until get some value (no timeout, blocking wait) */
	CMD_POLL_REG,
	CMD_WAIT_EVENT, /* gce wait HW done event & clear */
	CMD_CLEAR_EVENT, /* gce clear HW done event */
	CMD_WRITE_FD,   /* write file descriptor */
	CMD_MAX
};

enum gce_event_id {
	FMT_RDMA0_SW_RST_DONE_ENG,
	FMT_RDMA0_TILE_DONE,
	FMT_WDMA0_SW_RST_DONE_ENG,
	FMT_WDMA0_TILE_DONE,
	FMT_RDMA1_SW_RST_DONE_ENG,
	FMT_RDMA1_TILE_DONE,
	FMT_WDMA1_SW_RST_DONE_ENG,
	FMT_WDMA1_TILE_DONE,
};

enum fmt_gce_status {
	GCE_NONE,
	GCE_NORMAL,
	GCE_SECURE
};

struct mtk_vdec_fmt {
	dev_t fmt_devno;
	struct cdev *fmt_cdev;
	struct class *fmt_class;
	struct device *fmt_device;
	struct device *dev;
	const char *fmtname;
	struct cmdq_base *clt_base;
	struct cmdq_client *clt_fmt[FMT_CORE_NUM];
	struct cmdq_client *clt_fmt_sec[FMT_CORE_NUM];
	int gce_th_num;
	u16 gce_codec_eid[GCE_EVENT_MAX];
	struct gce_cmds *gce_cmds[FMT_CORE_NUM];
	struct map_hw_reg map_base[FMT_MAP_HW_REG_NUM];
	u32 gce_gpr[FMT_CORE_NUM];
	bool is_entering_suspend;
	struct mutex mux_fmt;
	struct mutex mux_gce_th[FMT_CORE_NUM];
	struct mutex mux_task;
	struct gce_cmdq_task gce_task[FMT_INST_MAX];
	struct semaphore fmt_sem[FMT_CORE_NUM];
	struct notifier_block pm_notifier;
	struct clk *clk_VDEC;
	enum fmt_gce_status gce_status[FMT_CORE_NUM];
	atomic_t gce_job_cnt[FMT_CORE_NUM];
};

#define FMT_GCE_SET_CMD_FLUSH _IOW('f', 0, struct gce_cmdq_obj)
#define FMT_GCE_WAIT_CALLBACK _IOW('f', 1, unsigned int)

#ifdef CONFIG_COMPAT
#define COMPAT_FMT_GCE_SET_CMD_FLUSH _IOW('f', 0, struct compat_gce_cmdq_obj)
#endif

#endif /* _MTK_FMT_H */
