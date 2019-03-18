/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_VCODEC_FW_H_
#define _MTK_VCODEC_FW_H_

#include <linux/remoteproc.h>

// TODO move these into .c file!
#include "../mtk-vpu/mtk_vpu.h"

struct mtk_vcodec_dev;

enum mtk_vcodec_fw_type {
	VPU,
};

struct mtk_vcodec_fw;

typedef void (*mtk_vcodec_ipi_handler) (void *data,
	unsigned int len, void *priv);

struct mtk_vcodec_fw *mtk_vcodec_fw_select(struct mtk_vcodec_dev *dev,
					   enum mtk_vcodec_fw_type type,
					   phandle rproc_phandle,
					   enum rst_id rst_id);

int mtk_vcodec_fw_load_firmware(struct mtk_vcodec_fw *fw);
unsigned int mtk_vcodec_fw_get_vdec_capa(struct mtk_vcodec_fw *fw);
unsigned int mtk_vcodec_fw_get_venc_capa(struct mtk_vcodec_fw *fw);
void *mtk_vcodec_fw_map_dm_addr(struct mtk_vcodec_fw *fw, u32 mem_addr);
int mtk_vcodec_fw_ipi_register(struct mtk_vcodec_fw *fw, int id,
	mtk_vcodec_ipi_handler handler, const char *name, void *priv);
int mtk_vcodec_fw_ipi_send(struct mtk_vcodec_fw *fw,
	int id, void *buf, unsigned int len, unsigned int wait);

#endif /* _MTK_VCODEC_FW_H_ */

