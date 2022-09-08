/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_DRV_H_
#define _MTK_VCODEC_DRV_H_


#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect-provider.h>
#include <linux/types.h>
#include <linux/list.h>
#include "mtk_vcodec_pm.h"
#define MAX_CODEC_FREQ_STEP	10

#define MTK_VDEC_PORT_NUM	1
#define MTK_VENC_PORT_NUM	1

/**
 * enum mtk_instance_type - The type of an MTK Vcodec instance.
 */
enum mtk_instance_type {
	MTK_INST_DECODER                = 0,
	MTK_INST_ENCODER                = 1,
};

struct mtk_vcodec_dev {
	struct platform_device *plat_dev;
	unsigned int dec_irq;
	unsigned int enc_irq;
	int vdec_freq_cnt;
	int venc_freq_cnt;
	struct regulator *vdec_reg;
	struct regulator *venc_reg;
	unsigned long long vdec_freqs[MAX_CODEC_FREQ_STEP];
	unsigned long long venc_freqs[MAX_CODEC_FREQ_STEP];
	unsigned long long dec_freq;
	unsigned long long enc_freq;
	void __iomem *dec_reg_base[NUM_MAX_VDEC_REG_BASE];
	void __iomem *enc_reg_base[NUM_MAX_VENC_REG_BASE];
	struct mtk_vcodec_pm pm;
	struct icc_path *vdec_qos_req[MTK_VDEC_PORT_NUM];
	struct icc_path *venc_qos_req[MTK_VENC_PORT_NUM];
};

#endif /* _MTK_VCODEC_INTR_H_ */
