/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */
#ifndef _MTK_IMGSYS_CMDQ_H_
#define _MTK_IMGSYS_CMDQ_H_

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-sys.h"

#define CMDQ_STOP_FUNC         (1)

#define DVFS_QOS_READY         (1)

#define MAX_FRAME_IN_TASK		64

struct task_timestamp {
	dma_addr_t dma_pa;
	uint32_t *dma_va;
	uint32_t num;
	uint32_t ofst;
};

struct mtk_imgsys_cmdq_timestamp {
	u64 tsReqStart;
	u64 tsFlushStart;
	u64 tsCmdqCbStart;
	u64 tsCmdqCbEnd;
	u64 tsCmdqCbWorkStart;
	u64 tsUserCbStart;
	u64 tsUserCbEnd;
	u64 tsReqEnd;
	u64 tsDvfsQosStart;
	u64 tsDvfsQosEnd;
};

struct mtk_imgsys_cb_param {
	struct work_struct cmdq_cb_work;
	struct cmdq_pkt *pkt;
	struct swfrm_info_t *frm_info;
	struct mtk_imgsys_cmdq_timestamp cmdqTs;
	struct mtk_imgsys_dev *imgsys_dev;
	struct cmdq_client *clt;
	struct task_timestamp taskTs;
	void (*user_cmdq_cb)(struct cmdq_cb_data data, uint32_t subfidx, bool isLastTaskInReq);
	void (*user_cmdq_err_cb)(struct cmdq_cb_data data, uint32_t fail_subfidx, bool isHWhang);
	int req_fd;
	int req_no;
	int frm_no;
	u32 hw_comb;
	s32 err;
	u32 frm_idx;
	u32 frm_num;
	u32 blk_idx;
	u32 blk_num;
	u32 is_earlycb;
	s32 group_id;
	u32 thd_idx;
	u32 task_id;
	u32 task_num;
	u32 task_cnt;
	size_t pkt_ofst[MAX_FRAME_IN_TASK];
	bool isBlkLast;
	bool isFrmLast;
	bool isTaskLast;
};

enum mtk_imgsys_cmd {
	IMGSYS_CMD_LOAD = 0,
	IMGSYS_CMD_MOVE,
	IMGSYS_CMD_READ,
	IMGSYS_CMD_WRITE,
	IMGSYS_CMD_POLL,
	IMGSYS_CMD_WAIT,
	IMGSYS_CMD_UPDATE,
	IMGSYS_CMD_ACQUIRE,
	IMGSYS_CMD_TIME,
	IMGSYS_CMD_STOP
};

enum mtk_imgsys_task_pri {
	IMGSYS_PRI_HIGH   = 95,
	IMGSYS_PRI_MEDIUM = 85,
	IMGSYS_PRI_LOW    = 50
};

struct imgsys_event_info {
	int req_fd;
	int req_no;
	int frm_no;
	u64 ts;
	struct swfrm_info_t *frm_info;
	struct cmdq_pkt *pkt;
	struct mtk_imgsys_cb_param *cb_param;
};

struct imgsys_event_history {
	int st;
	struct imgsys_event_info set;
	struct imgsys_event_info wait;
};

struct imgsys_event_table {
	u16 event;	/* cmdq event enum value */
	char dts_name[256];
};

struct imgsys_dvfs_group {
	u16 g_id;
	u32 g_hw;
};

struct Command {
	enum mtk_imgsys_cmd opcode;

	union {
		// For load, move, read
		struct {
			uint32_t mask;
			uint64_t target;
			uint64_t source;
		} __packed;

		// For write and poll
		struct {
			uint32_t dummy;
			uint64_t address;
			uint32_t value;
		} __packed;

		// For wait and update event
		struct {
			uint32_t event;
			uint32_t action;
		} __packed;
	} u;
} __packed;

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev);
void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamon(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamoff(struct mtk_imgsys_dev *imgsys_dev);
int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data,
					uint32_t uinfo_idx, bool isLastTaskInReq),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
					uint32_t fail_uinfo_idx, bool isHWhang));
int imgsys_cmdq_parser(struct swfrm_info_t *frm_info, struct cmdq_pkt *pkt,
				struct Command *cmd, u32 hw_comb, u32 cmd_num,
				dma_addr_t dma_pa, uint32_t *num, u32 thd_idx);
int imgsys_cmdq_sec_sendtask(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_sec_cmd(struct cmdq_pkt *pkt);
void imgsys_cmdq_clearevent(int event_id);

#if DVFS_QOS_READY
void mtk_imgsys_mmdvfs_init(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mmdvfs_uninit(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mmdvfs_set(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet);
void mtk_imgsys_mmqos_init(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mmqos_uninit(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mmqos_set(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet);
void mtk_imgsys_mmqos_set_by_scen(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet);
void mtk_imgsys_mmqos_reset(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mmdvfs_mmqos_cal(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet);
void mtk_imgsys_mmqos_bw_cal(struct mtk_imgsys_dev *imgsys_dev,
				void *smi_port, uint32_t hw_comb,
				uint32_t port_st, uint32_t port_num, uint32_t port_id);
void mtk_imgsys_mmqos_ts_cal(struct mtk_imgsys_dev *imgsys_dev,
				struct mtk_imgsys_cb_param *cb_param, uint32_t hw_comb);
void mtk_imgsys_power_ctrl(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn);

void mtk_imgsys_pwr(struct platform_device *pdev, bool on);
#endif

bool imgsys_cmdq_ts_enable(void);
u32 imgsys_wpe_bwlog_enable(void);
bool imgsys_cmdq_ts_dbg_enable(void);
bool imgsys_dvfs_dbg_enable(void);
bool imgsys_quick_onoff_enable(void);

#endif /* _MTK_IMGSYS_CMDQ_H_ */
