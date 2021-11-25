/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_IPI_H
#define APU_IPI_H

#include "mtk_apu_rpmsg.h"

#define APU_FW_VER_LEN	       (250)
#define APU_SHARE_BUFFER_SIZE  (256)
#define APU_SHARE_BUF_SIZE (round_up(sizeof(struct mtk_share_obj)*2, PAGE_SIZE))

struct mtk_apu;

enum {
	APU_IPI_INIT = 0,
	APU_IPI_NS_SERVICE,
	APU_IPI_DEEP_IDLE,
	APU_IPI_CTRL_RPMSG,
	APU_IPI_MIDDLEWARE,
	APU_IPI_REVISER_RPMSG,
	APU_IPI_PWR_TX,	// cmd direction from ap to up
	APU_IPI_PWR_RX, // cmd direction from up to ap
	APU_IPI_MDLA_TX,
	APU_IPI_MDLA_RX,
	APU_IPI_TIMESYNC,
	APU_IPI_EDMA_TX,
	APU_IPI_MNOC_TX,
	APU_IPI_MVPU_TX,
	APU_IPI_MVPU_RX,
	APU_IPI_LOG_LEVEL,
	APU_IPI_MAX,
};

struct apu_run {
	//u32 signaled;
	s8 fw_ver[APU_FW_VER_LEN];
	u32 signaled;
	wait_queue_head_t wq;
};

struct apu_ipi_desc {
	struct mutex lock;
	ipi_handler_t handler;
	void *priv;

	/*
	 * positive: host-initiated ipi outstanding count
	 * negative: apu-initiated ipi outstanding count
	 */
	int usage_cnt;
};

struct mtk_share_obj {
	//u32 id;
	//u32 len;
	u8 share_buf[APU_SHARE_BUFFER_SIZE];
};

void apu_ipi_remove(struct mtk_apu *apu);
int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu);
int apu_ipi_register(struct mtk_apu *apu, u32 id,
		ipi_handler_t handler, void *priv);
void apu_ipi_unregister(struct mtk_apu *apu, u32 id);
int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms);
int apu_ipi_lock(struct mtk_apu *apu);
void apu_ipi_unlock(struct mtk_apu *apu);
int apu_ipi_affin_enable(void);
int apu_ipi_affin_disable(void);

#endif /* APU_IPI_H */


