/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _QCN_SDIO_H_
#define _QCN_SDIO_H_

#include <linux/qcn_sdio_al.h>
#include "qcn_sdio_hwio.h"


#define MANUFACTURER_CODE		(0x70)
#define MANUFACTURER_ID_QCN_BASE	(0x400B)
#define QCN_SDIO_TTY_BLK_SZ		(512)
#define QCN_SDIO_MROM_BLK_SZ		(512)
#define QCN_SDIO_RW_REQ_MAX		(128)

#define QCN_SDIO_DMA0_RX_CNUM		(0x4)
#define QCN_SDIO_DMA0_TX_CNUM		(0xC)
#define QCN_SDIO_DMA1_RX_CNUM		(0x14)
#define QCN_SDIO_DMA1_TX_CNUM		(0x1C)

#define QCN_SDIO_CRQ_START		(0x1)
#define QCN_SDIO_CRQ_END		(0x2)

#define QCN_SDIO_META_VER_0		(0)
#define QCN_SDIO_META_VER_1		(1)

#if (QCN_SDIO_META_VER_0)
#define QCN_SDIO_LMETA_FMT_VER		(0)
#define QCN_SDIO_LMETA_VER_BMSK		(0x0000000F)
#define QCN_SDIO_LMETA_VER_SHFT		(0)
#define QCN_SDIO_LMETA_SW_BMSK		(0x000000F0)
#define QCN_SDIO_LMETA_SW_SHFT		(4)
#define QCN_SDIO_LMETA_EVENT_BMSK	(0x0000FF00)
#define QCN_SDIO_LMETA_EVENT_SHFT	(8)
#define QCN_SDIO_LMETA_DATA_BMSK	(0xFFFF0000)
#define QCN_SDIO_LMETA_DATA_SHFT	(16)
#define QCN_SDIO_LMETA_TREG_BMSK	(0x00F00000)
#define QCN_SDIO_LMETA_TREG_SHFT	(20)
#define QCN_SDIO_LMETA_TLEN_BMSK	(0x000F0000)
#define QCN_SDIO_LMETA_TLEN_SHFT	(16)

#define QCN_SDIO_HMETA_FMT_VER		(0)
#define QCN_SDIO_HMETA_VER_BMSK		(0x0000000F)
#define QCN_SDIO_HMETA_VER_SHFT		(0)
#define QCN_SDIO_HMETA_SW_BMSK		(0x000000F0)
#define QCN_SDIO_HMETA_SW_SHFT		(4)
#define QCN_SDIO_HMETA_EVENT_BMSK	(0x00003F00)
#define QCN_SDIO_HMETA_EVENT_SHFT	(8)
#define QCN_SDIO_HMETA_TRANS_BMSK	(0x00400000)
#define QCN_SDIO_HMETA_TRANS_SHFT	(22)
#define QCN_SDIO_HMETA_DATA_BMSK	(0xFF000000)
#define QCN_SDIO_HMETA_DATA_SHFT	(24)
#define QCN_SDIO_HMETA_TREG_BMSK	(0xF0000000)
#define QCN_SDIO_HMETA_TREG_SHFT	(28)
#define QCN_SDIO_HMETA_TLEN_BMSK	(0x0F000000)
#define QCN_SDIO_HMETA_TLEN_SHFT	(24)

#elif (QCN_SDIO_META_VER_1)

#define QCN_SDIO_MAJOR_VER		(0x00000000)
#define QCN_SDIO_MINOR_VER		(0x00000010)

#define QCN_SDIO_LMETA_FMT_VER		(1)
#define QCN_SDIO_LMETA_EVENT_BMSK	(0xFF000000)
#define QCN_SDIO_LMETA_EVENT_SHFT	(24)
#define QCN_SDIO_LMETA_DATA_BMSK	(0x00003FFF)
#define QCN_SDIO_LMETA_DATA_SHFT	(0)
#define QCN_SDIO_LMETA_SW_BMSK		(0x0000000F)
#define QCN_SDIO_LMETA_SW_SHFT		(0)
#define QCN_SDIO_LMETA_VER_MAJ_BMSK	(0x000000F0)
#define QCN_SDIO_LMETA_VER_MAJ_SHFT	(4)
#define QCN_SDIO_LMETA_VER_MIN_BMSK	(0x00000F00)
#define QCN_SDIO_LMETA_VER_MIN_SHFT	(8)

#define QCN_SDIO_HMETA_FMT_VER		(1)
#define QCN_SDIO_HMETA_EVENT_BMSK	(0xFF000000)
#define QCN_SDIO_HMETA_EVENT_SHFT	(24)
#define QCN_SDIO_HMETA_DATA_BMSK	(0x00003FFF)
#define QCN_SDIO_HMETA_DATA_SHFT	(0)
#define QCN_SDIO_HMETA_TRANS_BMSK	(0x00400000)
#define QCN_SDIO_HMETA_TRANS_SHFT	(22)
#define QCN_SDIO_HMETA_SW_BMSK		(0x0000000F)
#define QCN_SDIO_HMETA_SW_SHFT		(0)
#define QCN_SDIO_HMETA_VER_MAJ_BMSK	(0x000000F0)
#define QCN_SDIO_HMETA_VER_MAJ_SHFT	(4)
#define QCN_SDIO_HMETA_VER_MIN_BMSK	(0x00000F00)
#define QCN_SDIO_HMETA_VER_MIN_SHFT	(8)

#define QCN_SDIO_META_START_CH0		(0x20)
#define QCN_SDIO_META_START_CH1		(0x40)
#define QCN_SDIO_META_START_CH2		(0x60)
#define QCN_SDIO_META_START_CH3		(0x80)
#define QCN_SDIO_META_END		(0xA0)

#endif /* QCN_SDIO_META_VER */

enum qcn_sdio_cli_id {
	QCN_SDIO_CLI_ID_INVALID = 0,
	QCN_SDIO_CLI_ID_TTY,
	QCN_SDIO_CLI_ID_WLAN,
	QCN_SDIO_CLI_ID_QMI,
	QCN_SDIO_CLI_ID_DIAG,
	QCN_SDIO_CLI_ID_MAX
};

enum qcn_sdio_ch_id {
	QCN_SDIO_CH_0 = 0,
	QCN_SDIO_CH_1,
	QCN_SDIO_CH_2,
	QCN_SDIO_CH_3,
	QCN_SDIO_CH_MAX,
};

enum qcn_sdio_sw_mode {
	QCN_SDIO_SW_RESET = 0,
	QCN_SDIO_SW_PBL,
	QCN_SDIO_SW_SBL,
	QCN_SDIO_SW_RDDM,
	QCN_SDIO_SW_MROM,
	QCN_SDIO_SW_MAX,
};

enum qcn_sdio_host_event {
	QCN_SDIO_INVALID_HEVENT = 0,
	QCN_SDIO_SW_MODE_HEVENT,
	QCN_SDIO_BLK_SZ_HEVENT,
	QCN_SDIO_DOORBELL_HEVENT,
	QCN_SDIO_MAX_HEVENT = 63,
};

enum qcn_sdio_local_event {
	QCN_SDIO_INVALID_LEVENT = 0,
	QCN_SDIO_SW_MODE_LEVENT,
	QCN_SDIO_MAX_LEVENT = 255,
};


struct qcn_sdio_client_info {
	int is_probed;
	struct sdio_al_client_data cli_data;
	struct sdio_al_client_handle cli_handle;
	struct list_head cli_list;
	struct list_head ch_head;
};

struct qcn_sdio_ch_info {
	struct sdio_al_xfer_result result;
	struct sdio_al_client_handle *chandle;
	struct sdio_al_channel_data ch_data;
	struct sdio_al_channel_handle ch_handle;
	struct list_head ch_list;
	u32 crq_len;
};

struct qcn_sdio_rw_info {
	struct list_head list;
	u32 cid;
	enum sdio_al_dma_direction dir;
	void *buf;
	size_t len;
	void *ctxt;
};

#endif /* _QCN_SDIO_H_ */

