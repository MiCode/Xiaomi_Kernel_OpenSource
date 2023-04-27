/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_FMT_H
#define MTK_FMT_H
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/notifier.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <soc/mediatek/smi.h>
#include <cmdq-util.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif


#define FMT_CMDQ_CMD_MAX        (2048)
#define VDEC_FMT_DEVNAME "vdec-fmt"
#define FMT_CORE_NUM            2
#define GCE_EVENT_MAX           8
#define FMT_MAP_HW_REG_NUM      6
#define FMT_INST_MAX            1024
#define GCE_PENDING_CNT         10
#define MAP_PA_BASE_1GB  0x40000000 /* < 1GB registers */
#define SUSPEND_TIMEOUT_CNT     5000
#define FMT_PORT_NUM            4
#define MAX_FREQ_STEP           10
#define FMT_REG_GET32(addr)		(readl((void *)addr) & 0xFFFFFFFF)
#define FMT_REG_SET32(addr, val)	writel(val, ((void *)addr))

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

#if IS_ENABLED(CONFIG_COMPAT)
struct compat_gce_cmdq_obj {
	u64 cmds_user_ptr;
	u32 identifier;
	u32 secure;
	compat_uptr_t taskid;
	struct fmt_pmqos pmqos_param;
};
#endif

struct dmabuf_info {
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
};

struct gce_cmdq_task {
	struct gce_cmdq_obj cmdq_buff;
	struct cmdq_pkt *pkt_ptr;
	u32 identifier;
	u32 used;
	struct dmabuf_info iinfo;
	struct dmabuf_info oinfo;
};

struct dts_info {
	u32 pipeNum;
	u32 RDMA_baseAddr;
	u32 WROT_baseAddr;
	bool RDMA_needWA;
};

enum gce_cmd_id {
	CMD_READ = 0,   /* read register */
	CMD_WRITE,      /* write register */
	/* polling register until get some value (no timeout, blocking wait) */
	CMD_POLL_REG,
	CMD_WAIT_EVENT, /* gce wait HW done event & clear */
	CMD_SET_EVENT, /* gce set event */
	CMD_CLEAR_EVENT, /* gce clear HW done event */
	CMD_WRITE_FD,   /* write file descriptor */
	CMD_WRITE_RDMA,   /* write register (RDMA SW Workaround, write to CPR) */
	CMD_WRITE_FD_RDMA,   /* write file descriptor (RDMA SW Workaround, write to CPR) */
	CMD_MAX
};

enum FmtRDMASecureRegIdx {
	CPR_IDX_FMT_RDMA_SRC_OFFSET_0 = 0,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_1,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_2,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_WP,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_HP,
	CPR_IDX_FMT_RDMA_TRANSFORM_0,
	CPR_IDX_FMT_RDMA_SRC_BASE_0,
	CPR_IDX_FMT_RDMA_SRC_BASE_1,
	CPR_IDX_FMT_RDMA_SRC_BASE_2,
	CPR_IDX_FMT_RDMA_UFO_DEC_LENGTH_BASE_Y,
	CPR_IDX_FMT_RDMA_UFO_DEC_LENGTH_BASE_C,
	CPR_IDX_FMT_RDMA_SRC_BASE_0_MSB,
	CPR_IDX_FMT_RDMA_SRC_BASE_1_MSB,
	CPR_IDX_FMT_RDMA_SRC_BASE_2_MSB,
	CPR_IDX_FMT_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	CPR_IDX_FMT_RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_0_MSB,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_1_MSB,
	CPR_IDX_FMT_RDMA_SRC_OFFSET_2_MSB,
	CPR_IDX_FMT_RDMA_AFBC_PAYLOAD_OST,
	CPR_IDX_FMT_RDMA_PIPE_IDX = 20,
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
	SYNC_TOKEN_PREBUILT_VFMT_WAIT,
	SYNC_TOKEN_PREBUILT_VFMT_SET,
	SYNC_TOKEN_PREBUILT_VFMT_LOCK,
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
	u32 gce_th_num;
	u16 gce_codec_eid[GCE_EVENT_MAX];
	struct gce_cmds *gce_cmds[FMT_CORE_NUM];
	struct map_hw_reg map_base[FMT_MAP_HW_REG_NUM];
	u32 gce_gpr[FMT_CORE_NUM];
	bool is_entering_suspend;
	struct mutex mux_fmt;
	struct mutex *mux_gce_th[FMT_CORE_NUM];
	struct mutex mux_task;
	struct gce_cmdq_task gce_task[FMT_INST_MAX];
	atomic_t gce_task_wait_cnt[FMT_INST_MAX];
	struct semaphore fmt_sem[FMT_CORE_NUM];
	struct notifier_block pm_notifier;
	struct clk *clk_VDEC;
	struct clk *clk_MINI_MDP;
	enum fmt_gce_status gce_status[FMT_CORE_NUM];
	atomic_t gce_job_cnt[FMT_CORE_NUM];
	struct device *fmtLarb;
	int fmt_freq_cnt;
	unsigned long fmt_freqs[MAX_FREQ_STEP];
	struct regulator *fmt_reg;
	struct icc_path *fmt_qos_req[FMT_PORT_NUM];
	struct dts_info dtsInfo;
	int fmt_m4u_ports[FMT_PORT_NUM];
	atomic_t fmt_error;
	struct mutex mux_active_time;
	struct timespec64 fmt_active_time;
	struct workqueue_struct *cmdq_cb_workqueue;
	struct work_struct cmdq_cb_work;
};

#define FMT_GCE_SET_CMD_FLUSH _IOW('f', 0, struct gce_cmdq_obj)
#define FMT_GCE_WAIT_CALLBACK _IOW('f', 1, unsigned int)
#define FMT_GET_PLATFORM_DTS  _IOW('f', 2, struct dts_info)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_FMT_GCE_SET_CMD_FLUSH _IOW('f', 0, struct compat_gce_cmdq_obj)
#endif
int fmt_sync_device_init(void);

#endif /* _MTK_FMT_H */
