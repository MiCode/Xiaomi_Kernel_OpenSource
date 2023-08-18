/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __JPEG_DRV_H__
#define __JPEG_DRV_H__

#include <linux/ioctl.h>
#include <linux/notifier.h>

#if IS_ENABLED(CONFIG_COMPAT)
/* 32-64 bit conversion */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include "jpeg_dma_buf.h"

#define HW_CORE_NUMBER 2
#define JPEG_LARB_COUNT 2
#define MAX_FREQ_STEP 10

extern int jpg_dbg_level;

struct JpegClk {
	struct clk *clk_venc_jpgDec;
	struct clk *clk_venc_jpgDec_c1;
	struct clk *clk_venc_c1_jpgDec;
};

struct JpegDeviceStruct {
	struct device *pDev[JPEG_LARB_COUNT];
	long hybriddecRegBaseVA[HW_CORE_NUMBER];
	uint32_t hybriddecIrqId[HW_CORE_NUMBER];
	struct JpegClk jpegClk;
	struct device *jpegLarb[JPEG_LARB_COUNT];
	int jpeg_freq_cnt[JPEG_LARB_COUNT];
	unsigned long jpeg_freqs[JPEG_LARB_COUNT][MAX_FREQ_STEP];
	struct regulator *jpeg_reg[JPEG_LARB_COUNT];
	struct notifier_block pm_notifier;
	bool is_suspending;
	bool is_dec_started[HW_CORE_NUMBER];
};

const long jpeg_dev_get_hybrid_decoder_base_VA(int id);

struct dmabuf_info {
	struct dma_buf *i_dbuf;
	struct dma_buf_attachment *i_attach;
	struct sg_table *i_sgt;
	struct dma_buf *o_dbuf;
	struct dma_buf_attachment *o_attach;
	struct sg_table *o_sgt;
};

struct JPEG_DEC_DRV_HYBRID_TASK {
	long timeout;
	int *hwid;
	int *index_buf_fd;
	unsigned int data[21];
};

struct JPEG_DEC_DRV_HYBRID_P_N_S {
	int  hwid;
	int *progress_n_status;
};

#if IS_ENABLED(CONFIG_COMPAT)

struct compat_JPEG_DEC_DRV_HYBRID_TASK {
	compat_long_t timeout;
	compat_uptr_t hwid;
	compat_uptr_t index_buf_fd;
	unsigned int  data[21];
};

struct compat_JPEG_DEC_DRV_HYBRID_P_N_S {
	int  hwid;
	compat_uptr_t progress_n_status;
};

#endif

#define JPEG_IOCTL_MAGIC        'x'

#define JPEG_DEC_IOCTL_HYBRID_START \
	_IOWR(JPEG_IOCTL_MAGIC, 18, struct JPEG_DEC_DRV_HYBRID_TASK)
#define JPEG_DEC_IOCTL_HYBRID_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 19, struct JPEG_DEC_DRV_HYBRID_P_N_S)
#define JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS \
	_IOWR(JPEG_IOCTL_MAGIC, 20, struct JPEG_DEC_DRV_HYBRID_P_N_S)

#if IS_ENABLED(CONFIG_COMPAT)

#define COMPAT_JPEG_DEC_IOCTL_HYBRID_START \
	_IOWR(JPEG_IOCTL_MAGIC, 18, struct compat_JPEG_DEC_DRV_HYBRID_TASK)
#define COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 19, struct compat_JPEG_DEC_DRV_HYBRID_P_N_S)
#define COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS \
	_IOWR(JPEG_IOCTL_MAGIC, 20, struct compat_JPEG_DEC_DRV_HYBRID_P_N_S)
#endif

#define JPEG_LOG(level, format, args...)                       \
	do {                                                        \
		if ((jpg_dbg_level & level) == level)              \
			pr_info("[JPEG] level=%d %s(),%d: " format "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

#endif // __JPEG_DRV_H__
