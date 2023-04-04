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
#include <linux/interconnect-provider.h>
#include <linux/interconnect.h>

#include "jpeg_dma_buf.h"

#define HW_CORE_NUMBER 2
#define JPEG_LARB_COUNT 3
#define MAX_FREQ_STEP 10

extern int jpg_dbg_level;

struct JpegClk {
	struct clk *clk_venc_jpgDec;
	struct clk *clk_venc_jpgDec_c1;
	struct clk *clk_venc_c1_jpgDec;
	struct clk *clk_venc_jpgEnc;
};

struct JpegDeviceStruct {
	struct device *pDev[JPEG_LARB_COUNT];
	long encRegBaseVA;
	long hybriddecRegBaseVA[HW_CORE_NUMBER];
	uint32_t encIrqId;
	uint32_t hybriddecIrqId[HW_CORE_NUMBER];
	struct JpegClk jpegClk;
	struct device *jpegLarb[JPEG_LARB_COUNT];
	int jpeg_freq_cnt[JPEG_LARB_COUNT];
	unsigned long jpeg_freqs[JPEG_LARB_COUNT][MAX_FREQ_STEP];
	struct regulator *jpeg_reg[JPEG_LARB_COUNT];
	struct notifier_block pm_notifier;
	bool is_suspending;
	struct icc_path *jpeg_enc_qos_req;
};

const long jpeg_dev_get_hybrid_decoder_base_VA(int id);
const long jpeg_dev_get_encoder_base_VA(void);

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

/* JPEG Encoder Structure */
struct JPEG_ENC_DRV_IN {
	unsigned int dstBufferAddr;
	unsigned int dstBufferSize;

	unsigned int encWidth;	/* HW directly fill to header */
	unsigned int encHeight;	/* HW directly fill to header */

	unsigned char enableEXIF;
	unsigned char allocBuffer;
	/* unsigned char enableSyncReset; */

	unsigned int encQuality;
	unsigned int encFormat;

	/* extend in mt6589 */
	unsigned int disableGMC;	/* TBD: not support */
	unsigned int restartInterval;
	unsigned int srcBufferAddr;	/* YUV420: Luma */
	unsigned int srcChromaAddr;
	unsigned int imgStride;
	unsigned int memStride;
	unsigned int totalEncDU;
	unsigned int dstBufAddrOffset;
	unsigned int dstBufAddrOffsetMask;
	int srcFd;
	int srcFd2;
	int dstFd;
	unsigned int memHeight;
};


struct JPEG_ENC_DRV_OUT {
	long timeout;
	unsigned int *fileSize;
	unsigned int *result;
	unsigned int *cycleCount;

};


struct JPEG_PMEM_RANGE {
	unsigned long startAddr;	/* In : */
	unsigned long size;
	unsigned long result;

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

struct compat_JPEG_ENC_DRV_OUT {
	compat_long_t timeout;
	compat_uptr_t fileSize;
	compat_uptr_t result;
	compat_uptr_t cycleCount;

};

#endif

#define JPEG_IOCTL_MAGIC        'x'

#define JPEG_DEC_IOCTL_HYBRID_START \
	_IOWR(JPEG_IOCTL_MAGIC, 18, struct JPEG_DEC_DRV_HYBRID_TASK)
#define JPEG_DEC_IOCTL_HYBRID_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 19, struct JPEG_DEC_DRV_HYBRID_P_N_S)
#define JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS \
	_IOWR(JPEG_IOCTL_MAGIC, 20, struct JPEG_DEC_DRV_HYBRID_P_N_S)

/* /////////////////// JPEG ENC IOCTL ///////////////////////////////////// */

#define JPEG_ENC_IOCTL_INIT \
	_IO(JPEG_IOCTL_MAGIC, 11)
#define JPEG_ENC_IOCTL_CONFIG \
	_IOW(JPEG_IOCTL_MAGIC, 12, struct JPEG_ENC_DRV_IN)
#define JPEG_ENC_IOCTL_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 13, struct JPEG_ENC_DRV_OUT)
#define JPEG_ENC_IOCTL_DEINIT \
	_IO(JPEG_IOCTL_MAGIC, 14)
#define JPEG_ENC_IOCTL_START \
	_IO(JPEG_IOCTL_MAGIC, 15)

#define JPEG_ENC_IOCTL_WARM_RESET \
	_IO(JPEG_IOCTL_MAGIC, 20)
#define JPEG_ENC_IOCTL_DUMP_REG \
	_IO(JPEG_IOCTL_MAGIC, 21)
#define JPEG_ENC_IOCTL_RW_REG \
	_IO(JPEG_IOCTL_MAGIC, 22)
#define JPEG_ENC_IOCTL_FLUSH_BUFF \
	_IOWR(JPEG_IOCTL_MAGIC, 23, unsigned int)
#define JPEG_ENC_IOCTL_INVALIDATE_BUFF \
	_IOWR(JPEG_IOCTL_MAGIC, 24, unsigned int)


#if IS_ENABLED(CONFIG_COMPAT)

#define COMPAT_JPEG_DEC_IOCTL_HYBRID_START \
	_IOWR(JPEG_IOCTL_MAGIC, 18, struct compat_JPEG_DEC_DRV_HYBRID_TASK)
#define COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 19, struct compat_JPEG_DEC_DRV_HYBRID_P_N_S)
#define COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS \
	_IOWR(JPEG_IOCTL_MAGIC, 20, struct compat_JPEG_DEC_DRV_HYBRID_P_N_S)
#define COMPAT_JPEG_ENC_IOCTL_WAIT \
	_IOWR(JPEG_IOCTL_MAGIC, 13, struct compat_JPEG_ENC_DRV_OUT)

#endif

#define MTK_PLATFORM_MT6765				"platform:mt6765"
#define MTK_PLATFORM_MT6761				"platform:mt6761"
#define MTK_PLATFORM_MT6739				"platform:mt6739"
#define MTK_PLATFORM_MT6580				"platform:mt6580"


#define JPEG_LOG(level, format, args...)                       \
	do {                                                        \
		if ((jpg_dbg_level & level) == level)              \
			pr_info("[JPEG] level=%d %s(),%d: " format "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

#endif // __JPEG_DRV_H__
