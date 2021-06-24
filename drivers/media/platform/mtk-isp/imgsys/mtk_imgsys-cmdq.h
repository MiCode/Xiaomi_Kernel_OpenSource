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

#define DVFS_QOS_READY         (0)

#define IMGSYS_DVFS_ENABLE     (1)
#define IMGSYS_QOS_ENABLE      (1)

/* Record info definitions */
#define GCE_REC_MAX_FRAME_BLOCK     (32)
#define GCE_REC_MAX_TILE_BLOCK      (2048)

#define WPE_SMI_PORT_NUM 5
#define ME_SMI_PORT_NUM 2
#define PQ_DIP_SMI_PORT_NUM 4
#define TRAW_SMI_PORT_NUM 13
#define LTRAW_SMI_PORT_NUM 7
#define DIP_SMI_PORT_NUM 16

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
	void (*user_cmdq_cb)(struct cmdq_cb_data data);
	void (*user_cmdq_err_cb)(struct cmdq_cb_data data, uint32_t fail_subfidx);
	s32 err;
	u32 frm_idx;
	u32 frm_num;
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

struct BlockRecord {
	uint32_t            label_min;
	uint32_t            label_max;
	uint32_t            label_count;
	uint32_t            cmd_offset;
	uint32_t            cmd_length;
};

struct GCERecoder {
	// Record command offset
	uint32_t            cmd_offset;

	// Reocrd command buffer info
	uint32_t            *pOutput;
	uint32_t            *pBuffer;
	uint32_t            max_length;
	uint32_t            curr_length;

	// Each frame block info
	struct BlockRecord  frame_record[GCE_REC_MAX_FRAME_BLOCK];
	uint32_t            frame_block;
	uint32_t            curr_frame;

	// Each tile block info
	struct BlockRecord  tile_record[GCE_REC_MAX_TILE_BLOCK];
	uint32_t            tile_block;
	uint32_t            curr_tile;
};

struct smi_port_t {
	uint32_t portenum;
	uint32_t portbw;
} __attribute__((__packed__));

struct wpe_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[WPE_SMI_PORT_NUM];
} __attribute__((__packed__));

struct me_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[ME_SMI_PORT_NUM];
} __attribute__((__packed__));

struct pqdip_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[PQ_DIP_SMI_PORT_NUM];
} __attribute__((__packed__));

struct traw_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[TRAW_SMI_PORT_NUM];
} __attribute__((__packed__));

struct ltraw_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[LTRAW_SMI_PORT_NUM];
} __attribute__((__packed__));

struct dip_bw_t {
	uint32_t totalbw;
	struct smi_port_t smiport[DIP_SMI_PORT_NUM];
} __attribute__((__packed__));

struct frame_bw_t {
	struct wpe_bw_t wpe_eis;
	struct wpe_bw_t wpe_tnr;
	struct wpe_bw_t wpe_lite;
	struct me_bw_t me;
	struct pqdip_bw_t pqdip_a;
	struct pqdip_bw_t pqdip_b;
	struct traw_bw_t traw;
	struct ltraw_bw_t ltraw;
	struct dip_bw_t dip;
} __attribute__((__packed__));

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev);
void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev);
int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
				uint32_t fail_uinfo_idx));
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
#endif

