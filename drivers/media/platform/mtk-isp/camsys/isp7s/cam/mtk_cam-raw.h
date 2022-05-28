/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_H
#define __MTK_CAM_RAW_H

#include <linux/kfifo.h>
#include "mtk_cam-raw_pipeline.h"

#include "mtk_cam-dvfs_qos.h"

struct mtk_cam_request_stream_data;

#define RAW_PIPELINE_NUM 3 //TODO: remove
#define SCQ_DEADLINE_MS  15 // ~1/2 frame length
#define SCQ_DEFAULT_CLK_RATE 208 // default 208MHz
#define USINGSCQ 1

enum raw_module_id {
	RAW_A = 0,
	RAW_B = 1,
	RAW_C = 2,
	RAW_NUM = 3,
};

struct mtk_cam_dev;
struct mtk_cam_ctx;

struct mtk_raw_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq;
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *yuv_base;
	void __iomem *yuv_base_inner;
	unsigned int num_clks;
	struct clk **clks;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif
	/* larb */
	struct platform_device *larb_pdev;
	struct mtk_camsys_qos qos;

	int		fifo_size;
	void		*msg_buffer;
	struct kfifo	msg_fifo;
	atomic_t	is_fifo_overflow;

	bool enable_hsf;
	int fps;
	int subsample_ratio;

	bool is_slave;

	u64 sof_count;
	u64 vsync_count;
	u64 last_sof_time_ns;

	/* for subsample, sensor-control */
	bool sub_sensor_ctrl_en;
	int set_sensor_idx;
	int cur_vsync_idx;

	u8 time_shared_busy;
	u8 time_shared_busy_ctx_id;
	atomic_t vf_en;
	u32 stagger_en;
	int overrun_debug_dump_cnt;

	int default_printk_cnt;
};

struct mtk_yuv_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk **clks;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif
	struct platform_device *larb_pdev;
	struct mtk_camsys_qos qos;
};

/* AE information */
struct mtk_ae_debug_data {
	u64 OBC_R1_Sum[4];
	u64 OBC_R2_Sum[4];
	u64 OBC_R3_Sum[4];
	u64 AA_Sum[4];
	u64 LTM_Sum[4];
};

#ifdef NOT_READY
/*
 * struct mtk_raw - the raw information
 *
 * //FIXME for raw info comments
 *
 */
struct mtk_raw {
	struct device *cam_dev;
	struct device *devs[RAW_PIPELINE_NUM];
	struct device *yuvs[RAW_PIPELINE_NUM];
	struct mtk_raw_pipeline pipelines[RAW_PIPELINE_NUM];
};
#endif

/* CQ setting */
void initialize(struct mtk_raw_device *dev, int is_slave);
void subsample_enable(struct mtk_raw_device *dev, u32 ratio);
void stagger_enable(struct mtk_raw_device *dev);
void stagger_disable(struct mtk_raw_device *dev);

void apply_cq(struct mtk_raw_device *dev,
	      int initial, dma_addr_t cq_addr,
	      unsigned int cq_size, unsigned int cq_offset,
	      unsigned int sub_cq_size, unsigned int sub_cq_offset);
/* db */
void dbload_force(struct mtk_raw_device *dev);
void toggle_db(struct mtk_raw_device *dev);
void enable_tg_db(struct mtk_raw_device *dev, int en);


/* trigger */
void stream_on(struct mtk_raw_device *dev, int on,
	int scq_period_ms, bool pass_vf_en);
void immediate_stream_off(struct mtk_raw_device *dev);
void trigger_rawi(struct mtk_raw_device *dev, signed int hw_scene);

/* reset */
void reset(struct mtk_raw_device *dev);

void dump_aa_info(struct mtk_cam_ctx *ctx,
		  struct mtk_ae_debug_data *ae_info);

int mtk_cam_translation_fault_callback(int port, dma_addr_t mva, void *data);

extern struct platform_driver mtk_cam_raw_driver;
extern struct platform_driver mtk_cam_yuv_driver;

static inline u32 dmaaddr_lsb(dma_addr_t addr)
{
	return addr & (BIT_MASK(32) - 1UL);
}

static inline u32 dmaaddr_msb(dma_addr_t addr)
{
	return addr >> 32;
}

#endif /*__MTK_CAM_RAW_H*/
