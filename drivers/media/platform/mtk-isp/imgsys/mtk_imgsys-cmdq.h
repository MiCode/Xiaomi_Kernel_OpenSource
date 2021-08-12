/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-sys.h"

#define CMDQ_STOP_FUNC         (0)

#define DVFS_QOS_READY         (1)

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
	void (*user_cmdq_cb)(struct cmdq_cb_data data, uint32_t subfidx);
	void (*user_cmdq_err_cb)(struct cmdq_cb_data data, uint32_t fail_subfidx, bool isHWhang);
	s32 err;
	u32 frm_idx;
	u32 frm_num;
	u32 is_earlycb;
	s32 group_id;
	bool isBlkLast;
	bool isFrmLast;
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
	IMGSYS_CMD_STOP
};

enum mtk_imgsys_task_pri {
	IMGSYS_PRI_HIGH   = 95,
	IMGSYS_PRI_MEDIUM = 85,
	IMGSYS_PRI_LOW    = 50
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
		};

		// For write and poll
		struct {
			uint32_t dummy;
			uint64_t address;
			uint32_t value;
		};

		// For wait and update event
		struct {
			uint32_t event;
			uint32_t action;
		};
	} u;
};

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev);
void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamon(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamoff(struct mtk_imgsys_dev *imgsys_dev);
int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data,
					uint32_t uinfo_idx),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
					uint32_t fail_uinfo_idx, bool isHWhang));
int imgsys_cmdq_parser(struct cmdq_pkt *pkt, struct Command *cmd);

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
void mtk_imgsys_mmdvfs_mmqos_cal(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				bool isSet);
void mtk_imgsys_power_ctrl(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn);
#endif

