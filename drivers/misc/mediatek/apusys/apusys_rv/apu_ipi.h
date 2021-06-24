/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_IPI_H__
#define __APU_IPI_H__

#include "apu.h"

#include "mtk_apu_rpmsg.h"

enum {
	APU_IPI_INIT = 0,
	APU_IPI_NS_SERVICE,
	APU_IPI_POWER_ON_DUMMY,
	APU_IPI_CTRL_RPMSG,
	APU_IPI_MIDDLEWARE,
	APU_IPI_REVISER_RPMSG,
	APU_IPI_MAX,
};

struct apu_ipi_desc {
	struct mutex lock;
	ipi_handler_t handler;
	void *priv;
};

#define APU_FW_VER_LEN	       (32)
#define APU_SHARE_BUFFER_SIZE  (256)
struct mtk_share_obj {
	//u32 id;
	//u32 len;
	u8 share_buf[APU_SHARE_BUFFER_SIZE];
};
#define APU_SHARE_BUF_SIZE (round_up(sizeof(struct mtk_share_obj)*2, PAGE_SIZE))

struct apu_run {
	//u32 signaled;
	s8 fw_ver[APU_FW_VER_LEN];
	u32 signaled;
	wait_queue_head_t wq;
};

#endif /* __APU_IPI_ID_H__ */


