/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <cmdq-util.h>

#include "mtk-mml-rdma-golden.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include "mtk-mml-mmp.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#define RDMA_EN				0x000
#define RDMA_UFBDC_DCM_EN		0x004
#define RDMA_RESET			0x008
#define RDMA_INTERRUPT_ENABLE		0x010
#define RDMA_INTERRUPT_STATUS		0x018
#define RDMA_CON			0x020
#define RDMA_SHADOW_CTRL		0x024
#define RDMA_GMCIF_CON			0x028
#define RDMA_SRC_CON			0x030
#define RDMA_COMP_CON			0x038
#define RDMA_PVRIC_CRYUVAL_0		0x040
#define RDMA_PVRIC_CRYUVAL_1		0x048
#define RDMA_PVRIC_CRCH0123VAL_0	0x050
#define RDMA_PVRIC_CRCH0123VAL_1	0x058
#define RDMA_MF_BKGD_SIZE_IN_BYTE	0x060
#define RDMA_MF_BKGD_SIZE_IN_PXL	0x068
#define RDMA_MF_SRC_SIZE		0x070
#define RDMA_MF_CLIP_SIZE		0x078
#define RDMA_MF_OFFSET_1		0x080
#define RDMA_SF_BKGD_SIZE_IN_BYTE	0x090
#define RDMA_MF_BKGD_H_SIZE_IN_PXL	0x098
#define RDMA_VC1_RANGE			0x0f0
#define RDMA_SRC_OFFSET_0		0x118
#define RDMA_SRC_OFFSET_1		0x120
#define RDMA_SRC_OFFSET_2		0x128
#define RDMA_SRC_OFFSET_WP		0x148
#define RDMA_SRC_OFFSET_HP		0x150
#define RDMA_TRANSFORM_0		0x200
#define RDMA_DMABUF_CON_0		0x240
#define RDMA_URGENT_TH_CON_0		0x244
#define RDMA_ULTRA_TH_CON_0		0x248
#define RDMA_PREULTRA_TH_CON_0		0x250
#define RDMA_DMABUF_CON_1		0x254
#define RDMA_URGENT_TH_CON_1		0x258
#define RDMA_ULTRA_TH_CON_1		0x260
#define RDMA_PREULTRA_TH_CON_1		0x264
#define RDMA_DMABUF_CON_2		0x268
#define RDMA_URGENT_TH_CON_2		0x270
#define RDMA_ULTRA_TH_CON_2		0x274
#define RDMA_PREULTRA_TH_CON_2		0x278
#define RDMA_DMABUF_CON_3		0x280
#define RDMA_URGENT_TH_CON_3		0x284
#define RDMA_ULTRA_TH_CON_3		0x288
#define RDMA_PREULTRA_TH_CON_3		0x290
#define RDMA_DITHER_CON			0x2a0
#define RDMA_RESV_DUMMY_0		0x2a8
#define RDMA_UNCOMP_MON			0x2c0
#define RDMA_COMP_MON			0x2c8
#define RDMA_CHKS_EXTR			0x300
#define RDMA_CHKS_INTW			0x308
#define RDMA_CHKS_INTR			0x310
#define RDMA_CHKS_ROTO			0x318
#define RDMA_CHKS_SRIY			0x320
#define RDMA_CHKS_SRIU			0x328
#define RDMA_CHKS_SRIV			0x330
#define RDMA_CHKS_SROY			0x338
#define RDMA_CHKS_SROU			0x340
#define RDMA_CHKS_SROV			0x348
#define RDMA_CHKS_VUPI			0x350
#define RDMA_CHKS_VUPO			0x358
#define RDMA_DEBUG_CON			0x380
#define RDMA_MON_STA_0			0x400
#define RDMA_MON_STA_1			0x408
#define RDMA_MON_STA_2			0x410
#define RDMA_MON_STA_3			0x418
#define RDMA_MON_STA_4			0x420
#define RDMA_MON_STA_5			0x428
#define RDMA_MON_STA_6			0x430
#define RDMA_MON_STA_7			0x438
#define RDMA_MON_STA_8			0x440
#define RDMA_MON_STA_9			0x448
#define RDMA_MON_STA_10			0x450
#define RDMA_MON_STA_11			0x458
#define RDMA_MON_STA_12			0x460
#define RDMA_MON_STA_13			0x468
#define RDMA_MON_STA_14			0x470
#define RDMA_MON_STA_15			0x478
#define RDMA_MON_STA_16			0x480
#define RDMA_MON_STA_17			0x488
#define RDMA_MON_STA_18			0x490
#define RDMA_MON_STA_19			0x498
#define RDMA_MON_STA_20			0x4a0
#define RDMA_MON_STA_21			0x4a8
#define RDMA_MON_STA_22			0x4b0
#define RDMA_MON_STA_23			0x4b8
#define RDMA_MON_STA_24			0x4c0
#define RDMA_MON_STA_25			0x4c8
#define RDMA_MON_STA_26			0x4d0
#define RDMA_MON_STA_27			0x4d8
#define RDMA_MON_STA_28			0x4e0
#define RDMA_SRC_BASE_0			0xf00
#define RDMA_SRC_BASE_1			0xf08
#define RDMA_SRC_BASE_2			0xf10
#define RDMA_UFO_DEC_LENGTH_BASE_Y	0xf20
#define RDMA_UFO_DEC_LENGTH_BASE_C	0xf28
#define RDMA_SRC_BASE_0_MSB		0xf30
#define RDMA_SRC_BASE_1_MSB		0xf34
#define RDMA_SRC_BASE_2_MSB		0xf38
#define RDMA_UFO_DEC_LENGTH_BASE_Y_MSB	0xf3c
#define RDMA_UFO_DEC_LENGTH_BASE_C_MSB	0xf40
#define RDMA_SRC_OFFSET_0_MSB		0xf44
#define RDMA_SRC_OFFSET_1_MSB		0xf48
#define RDMA_SRC_OFFSET_2_MSB		0xf4c
#define RDMA_AFBC_PAYLOAD_OST		0xf50

enum cpr_reg_idx {
	CPR_RDMA_SRC_OFFSET_0 = 0,
	CPR_RDMA_SRC_OFFSET_1,
	CPR_RDMA_SRC_OFFSET_2,
	CPR_RDMA_SRC_OFFSET_WP,
	CPR_RDMA_SRC_OFFSET_HP,
	CPR_RDMA_TRANSFORM_0,
	CPR_RDMA_SRC_BASE_0,
	CPR_RDMA_SRC_BASE_1,
	CPR_RDMA_SRC_BASE_2,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_Y,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_C,
	CPR_RDMA_SRC_BASE_0_MSB,
	CPR_RDMA_SRC_BASE_1_MSB,
	CPR_RDMA_SRC_BASE_2_MSB,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
	CPR_RDMA_SRC_OFFSET_0_MSB,
	CPR_RDMA_SRC_OFFSET_1_MSB,
	CPR_RDMA_SRC_OFFSET_2_MSB,
	CPR_RDMA_AFBC_PAYLOAD_OST,
	CPR_RDMA_PIPE_IDX = 20,
	CPR_RDMA_COUNT = 21,
};

static const dma_addr_t reg_idx_to_ofst[CPR_RDMA_COUNT] = {
	[CPR_RDMA_SRC_OFFSET_0] = RDMA_SRC_OFFSET_0,
	[CPR_RDMA_SRC_OFFSET_1] = RDMA_SRC_OFFSET_1,
	[CPR_RDMA_SRC_OFFSET_2] = RDMA_SRC_OFFSET_2,
	[CPR_RDMA_SRC_OFFSET_WP] = RDMA_SRC_OFFSET_WP,
	[CPR_RDMA_SRC_OFFSET_HP] = RDMA_SRC_OFFSET_HP,
	[CPR_RDMA_TRANSFORM_0] = RDMA_TRANSFORM_0,
	[CPR_RDMA_SRC_BASE_0] = RDMA_SRC_BASE_0,
	[CPR_RDMA_SRC_BASE_1] = RDMA_SRC_BASE_1,
	[CPR_RDMA_SRC_BASE_2] = RDMA_SRC_BASE_2,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y] = RDMA_UFO_DEC_LENGTH_BASE_Y,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C] = RDMA_UFO_DEC_LENGTH_BASE_C,
	[CPR_RDMA_SRC_BASE_0_MSB] = RDMA_SRC_BASE_0_MSB,
	[CPR_RDMA_SRC_BASE_1_MSB] = RDMA_SRC_BASE_1_MSB,
	[CPR_RDMA_SRC_BASE_2_MSB] = RDMA_SRC_BASE_2_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB] = RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB] = RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
	[CPR_RDMA_SRC_OFFSET_0_MSB] = RDMA_SRC_OFFSET_0_MSB,
	[CPR_RDMA_SRC_OFFSET_1_MSB] = RDMA_SRC_OFFSET_1_MSB,
	[CPR_RDMA_SRC_OFFSET_2_MSB] = RDMA_SRC_OFFSET_2_MSB,
	[CPR_RDMA_AFBC_PAYLOAD_OST] = RDMA_AFBC_PAYLOAD_OST,
};

static const enum cpr_reg_idx lsb_to_msb[CPR_RDMA_COUNT] = {
	[CPR_RDMA_SRC_OFFSET_0] = CPR_RDMA_SRC_OFFSET_0_MSB,
	[CPR_RDMA_SRC_OFFSET_1] = CPR_RDMA_SRC_OFFSET_1_MSB,
	[CPR_RDMA_SRC_OFFSET_2] = CPR_RDMA_SRC_OFFSET_2_MSB,
	[CPR_RDMA_SRC_BASE_0] = CPR_RDMA_SRC_BASE_0_MSB,
	[CPR_RDMA_SRC_BASE_1] = CPR_RDMA_SRC_BASE_1_MSB,
	[CPR_RDMA_SRC_BASE_2] = CPR_RDMA_SRC_BASE_2_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y] = CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C] = CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
};

static const u32 rdma_dmabuf_con[4] = {
	[0] = RDMA_DMABUF_CON_0,
	[1] = RDMA_DMABUF_CON_1,
	[2] = RDMA_DMABUF_CON_2,
	[3] = RDMA_DMABUF_CON_3,
};

static const u32 rdma_urgent_th[4] = {
	[0] = RDMA_URGENT_TH_CON_0,
	[1] = RDMA_URGENT_TH_CON_1,
	[2] = RDMA_URGENT_TH_CON_2,
	[3] = RDMA_URGENT_TH_CON_3,
};

static const u32 rdma_ultra_th[4] = {
	[0] = RDMA_ULTRA_TH_CON_0,
	[1] = RDMA_ULTRA_TH_CON_1,
	[2] = RDMA_ULTRA_TH_CON_2,
	[3] = RDMA_ULTRA_TH_CON_3,
};

static const u32 rdma_preultra_th[4] = {
	[0] = RDMA_PREULTRA_TH_CON_0,
	[1] = RDMA_PREULTRA_TH_CON_1,
	[2] = RDMA_PREULTRA_TH_CON_2,
	[3] = RDMA_PREULTRA_TH_CON_3,
};

/* SMI offset */
#define SMI_LARB_NON_SEC_CON		0x380

enum rdma_label {
	RDMA_LABEL_BASE_0 = 0,
	RDMA_LABEL_BASE_0_MSB,
	RDMA_LABEL_BASE_1,
	RDMA_LABEL_BASE_1_MSB,
	RDMA_LABEL_BASE_2,
	RDMA_LABEL_BASE_2_MSB,
	RDMA_LABEL_UFO_DEC_BASE_Y,
	RDMA_LABEL_UFO_DEC_BASE_Y_MSB,
	RDMA_LABEL_UFO_DEC_BASE_C,
	RDMA_LABEL_UFO_DEC_BASE_C_MSB,
	RDMA_LABEL_TOTAL
};

static s32 rdma_write(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
		      enum cpr_reg_idx idx, u32 value, bool write_sec)
{
	if (write_sec) {
		return cmdq_pkt_assign_command(pkt,
			CMDQ_CPR_PREBUILT(CMDQ_PREBUILT_MML, hw_pipe, idx), value);
	}
	/* else */
	return cmdq_pkt_write(pkt, NULL,
		base_pa + reg_idx_to_ofst[idx], value, U32_MAX);
}

static s32 rdma_write_ofst(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
			   enum cpr_reg_idx lsb_idx, u64 value, bool write_sec)
{
	enum cpr_reg_idx msb_idx = lsb_to_msb[lsb_idx];
	s32 ret;

	ret = rdma_write(pkt, base_pa, hw_pipe,
			 lsb_idx, value & GENMASK_ULL(31, 0), write_sec);
	if (ret)
		return ret;
	ret = rdma_write(pkt, base_pa, hw_pipe,
			 msb_idx, value >> 32, write_sec);
	return ret;
}

static s32 rdma_write_reuse(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
			    enum cpr_reg_idx idx, u32 value,
			    struct mml_task_reuse *reuse,
			    struct mml_pipe_cache *cache,
			    u16 *label_idx, bool write_sec)
{
	if (write_sec) {
		return mml_assign(pkt,
			CMDQ_CPR_PREBUILT(CMDQ_PREBUILT_MML, hw_pipe, idx), value,
			reuse, cache, label_idx);
	}
	/* else */
	return mml_write(pkt,
		base_pa + reg_idx_to_ofst[idx], value, U32_MAX,
		reuse, cache, label_idx);
}

static s32 rdma_write_addr(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
			   enum cpr_reg_idx lsb_idx, u64 value,
			   struct mml_task_reuse *reuse,
			   struct mml_pipe_cache *cache,
			   u16 *label_idx, bool write_sec)
{
	enum cpr_reg_idx msb_idx = lsb_to_msb[lsb_idx];
	s32 ret;

	ret = rdma_write_reuse(pkt, base_pa, hw_pipe,
			       lsb_idx, value & GENMASK_ULL(31, 0),
			       reuse, cache, label_idx, write_sec);
	if (ret)
		return ret;
	ret = rdma_write_reuse(pkt, base_pa, hw_pipe,
			       msb_idx, value >> 32,
			       reuse, cache, label_idx + 1, write_sec);
	return ret;
}

static void rdma_update_addr(struct mml_task_reuse *reuse,
			     u16 label_lsb, u16 label_msb, u64 value)
{
	mml_update(reuse, label_lsb, value & GENMASK_ULL(31, 0));
	mml_update(reuse, label_msb, value >> 32);
}

enum rdma_golden_fmt {
	GOLDEN_FMT_ARGB,
	GOLDEN_FMT_RGB,
	GOLDEN_FMT_YUV420,
	GOLDEN_FMT_TOTAL
};

struct rdma_data {
	u32 tile_width;
	bool write_sec_reg;

	/* threshold golden setting for racing mode */
	struct rdma_golden golden[GOLDEN_FMT_TOTAL];
};

static const struct rdma_data mt6893_rdma_data = {
	.tile_width = 640,
};

static const struct rdma_data mt6983_rdma_data = {
	.tile_width = 1696,
	.write_sec_reg = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = 2,
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = 2,
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = 2,
			.settings = th_yuv420_mt6983,
		},
	},
};

static const struct rdma_data mt6879_rdma_data = {
	.tile_width = 1440,
	.write_sec_reg = true,
};

static const struct rdma_data mt6895_rdma0_data = {
	.tile_width = 1344,
	.write_sec_reg = false,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = 2,
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = 2,
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = 2,
			.settings = th_yuv420_mt6983,
		},
	},
};

static const struct rdma_data mt6895_rdma1_data = {
	.tile_width = 896,
	.write_sec_reg = false,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = 2,
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = 2,
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = 2,
			.settings = th_yuv420_mt6983,
		},
	},
};

struct mml_comp_rdma {
	struct mml_comp comp;
	const struct rdma_data *data;
	struct device *dev;	/* for dmabuf to iova */

	u16 event_eof;

	/* smi register to config sram/dram mode */
	phys_addr_t smi_larb_con;

	u32 sram_cnt;
	u64 sram_pa;
	struct mutex sram_mutex;
};

struct rdma_frame_data {
	u8 enable_ufo;
	u8 hw_fmt;
	u8 swap;
	u8 blk;
	u8 lb_2b_mode;
	u8 field;
	u8 blk_10bit;
	u8 blk_tile;
	u8 color_tran;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 vdo_blk_shift_w;
	u32 vdo_blk_height;
	u32 vdo_blk_shift_h;
	u32 pixel_acc;		/* pixel accumulation */
	u32 datasize;		/* qos data size in bytes */

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[RDMA_LABEL_TOTAL];
};

#define rdma_frm_data(i)	((struct rdma_frame_data *)(i->data))

static inline struct mml_comp_rdma *comp_to_rdma(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rdma, comp);
}

static s32 rdma_config_read(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	ccfg->data = kzalloc(sizeof(struct rdma_frame_data), GFP_KERNEL);
	return 0;
}

static s32 rdma_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	s32 ret = 0;

	mml_trace_ex_begin("%s", __func__);

	if (unlikely(task->config->info.mode == MML_MODE_SRAM_READ)) {
		mutex_lock(&rdma->sram_mutex);
		if (!rdma->sram_cnt)
			rdma->sram_pa = (u64)mml_sram_get(task->config->mml);
		rdma->sram_cnt++;
		mutex_unlock(&rdma->sram_mutex);

		mml_msg("%s comp %u sram pa %#llx",
			__func__, comp->id, rdma->sram_pa);
	} else {
		/* get iova */
		ret = mml_buf_iova_get(rdma->dev, &task->buf.src);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_msg("%s comp %u iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, comp->id,
			task->buf.src.dma[0].iova,
			task->buf.src.size[0],
			task->buf.src.dma[1].iova,
			task->buf.src.size[1],
			task->buf.src.dma[2].iova,
			task->buf.src.size[2]);

		mml_mmp(buf_map, MMPROFILE_FLAG_PULSE,
			((u64)task->job.jobid << 16) | comp->id,
			(unsigned long)task->buf.src.dma[0].iova);
	}

	mml_trace_ex_end();

	return ret;
}

static void rdma_buf_unmap(struct mml_comp *comp, struct mml_task *task,
			   const struct mml_path_node *node)
{
	struct mml_comp_rdma *rdma;

	if (unlikely(task->config->info.mode == MML_MODE_SRAM_READ)) {
		rdma = comp_to_rdma(comp);

		mutex_lock(&rdma->sram_mutex);
		rdma->sram_cnt--;
		if (rdma->sram_cnt == 0)
			mml_sram_put(task->config->mml);
		mutex_unlock(&rdma->sram_mutex);
	}
}

static s32 rdma_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg,
			     struct tile_func_block *func,
			     union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);

	data->rdma.src_fmt = src->format;
	data->rdma.blk_shift_w = MML_FMT_BLOCK(src->format) ? 4 : 0;
	data->rdma.blk_shift_h = MML_FMT_BLOCK(src->format) ? 5 : 0;
	data->rdma.max_width = rdma->data->tile_width;

	/* RDMA support crop capability */
	func->type = TILE_TYPE_RDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_rdma_init;
	func->for_func = tile_rdma_for;
	func->back_func = tile_rdma_back;
	func->data = data;
	func->enable_flag = true;

	func->full_size_x_in = src->width;
	func->full_size_y_in = src->height;
	func->full_size_x_out = src->width;
	func->full_size_y_out = src->height;

	if (cfg->info.dest_cnt == 1 ||
	     !memcmp(&cfg->info.dest[0].crop,
		     &cfg->info.dest[1].crop,
		     sizeof(struct mml_crop))) {
		struct mml_frame_dest *dest = &cfg->info.dest[0];
		u32 in_crop_w, in_crop_h;

		data->rdma.crop = dest->crop.r;

		in_crop_w = dest->crop.r.width;
		in_crop_h = dest->crop.r.height;
		if (in_crop_w + dest->crop.r.left > src->width)
			in_crop_w = src->width - dest->crop.r.left;
		if (in_crop_h + dest->crop.r.top > src->height)
			in_crop_h = src->height - dest->crop.r.top;

		if (dest->crop.r.width != src->width ||
		    dest->crop.r.height != src->height) {
			func->full_size_x_out = in_crop_w;
			func->full_size_y_out = in_crop_h;
		}
	} else {
		data->rdma.crop.left = 0;
		data->rdma.crop.top = 0;
		data->rdma.crop.width = src->width;
		data->rdma.crop.height = src->height;
	}
	return 0;
}

static const struct mml_comp_tile_ops rdma_tile_ops = {
	.prepare = rdma_tile_prepare,
};

static u32 rdma_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return RDMA_LABEL_TOTAL;
}

static void rdma_color_fmt(struct mml_frame_config *cfg,
			   struct rdma_frame_data *rdma_frm)
{
	u32 fmt = cfg->info.src.format;
	u16 profile_in = cfg->info.src.profile;

	rdma_frm->color_tran = 0;
	rdma_frm->matrix_sel = 0;

	rdma_frm->enable_ufo = MML_FMT_UFO(fmt);
	rdma_frm->hw_fmt = MML_FMT_HW_FORMAT(fmt);
	rdma_frm->swap = MML_FMT_SWAP(fmt);
	rdma_frm->blk = MML_FMT_BLOCK(fmt);
	rdma_frm->lb_2b_mode = rdma_frm->blk ? 0 : 1;
	rdma_frm->field = MML_FMT_INTERLACED(fmt);
	rdma_frm->blk_10bit = MML_FMT_10BIT_PACKED(fmt);
	rdma_frm->blk_tile = MML_FMT_10BIT_TILE(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_RGB565:
	case MML_FMT_BGR565:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGB888:
	case MML_FMT_BGR888:
		rdma_frm->bits_per_pixel_y = 24;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGBA8888:
	case MML_FMT_BGRA8888:
	case MML_FMT_ARGB8888:
	case MML_FMT_ABGR8888:
	case MML_FMT_RGBA1010102:
	case MML_FMT_BGRA1010102:
	case MML_FMT_RGBA8888_AFBC:
	case MML_FMT_BGRA8888_AFBC:
	case MML_FMT_RGBA1010102_AFBC:
	case MML_FMT_BGRA1010102_AFBC:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_UYVY:
	case MML_FMT_VYUY:
	case MML_FMT_YUYV:
	case MML_FMT_YVYU:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I420:
	case MML_FMT_YV12:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_I422:
	case MML_FMT_YV16:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I444:
	case MML_FMT_YV24:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12:
	case MML_FMT_NV21:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_AFBC:
	case MML_FMT_YVU420_AFBC:
		rdma_frm->bits_per_pixel_y = 12;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_UFO:
	case MML_FMT_BLK_UFO_AUO:
	case MML_FMT_BLK:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_NV16:
	case MML_FMT_NV61:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV24:
	case MML_FMT_NV42:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10L:
	case MML_FMT_NV21_10L:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 32;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV4441010102:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10P:
	case MML_FMT_NV21_10P:
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_10P_AFBC:
	case MML_FMT_YVU420_10P_AFBC:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_10H:
	case MML_FMT_BLK_10V:
	case MML_FMT_BLK_10HJ:
	case MML_FMT_BLK_10VJ:
	case MML_FMT_BLK_UFO_10H:
	case MML_FMT_BLK_UFO_10V:
	case MML_FMT_BLK_UFO_10HJ:
	case MML_FMT_BLK_UFO_10VJ:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	default:
		mml_err("[rdma] not support format %x", fmt);
		break;
	}

	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;

	if (rdma_frm->color_tran == 1) {
		if (profile_in == MML_YCBCR_PROFILE_BT601)
			rdma_frm->matrix_sel = 2;
		else if (profile_in == MML_YCBCR_PROFILE_BT709)
			rdma_frm->matrix_sel = 3;
		else if (profile_in == MML_YCBCR_PROFILE_FULL_BT601)
			rdma_frm->matrix_sel = 0;
		else
			mml_err("[rdma] unknown color conversion %x",
				profile_in);
	}
}

s32 check_setting(struct mml_file_buf *src_buf, struct mml_frame_data *src)
{
	s32 plane = MML_FMT_PLANE(src->format);
	/* block format error check */
	if (MML_FMT_BLOCK(src->format)) {
		if ((src->width & 0x0f) || (src->height & 0x1f)) {
			mml_err("invalid blk, width %u, height %u",
				src->width, src->height);
			mml_err("alignment width 16x, height 32x");
			return -1;
		}

		/* 16-byte error */
		/* secure handle will not check */
		if (!src->secure && ((src_buf->dma[0].iova & 0x0f) ||
		    (plane > 1 && (src_buf->dma[1].iova & 0x0f)) ||
		    (plane > 2 && (src_buf->dma[2].iova & 0x0f)))) {
			mml_err("invalid block format setting, buffer %#11llx %#11llx %#11llx",
				src_buf->dma[0].iova, src_buf->dma[1].iova,
				src_buf->dma[2].iova);
			mml_err("buffer should be 16 align");
			return -1;
		}
		/* 128-byte warning for performance */
		if (!src->secure && ((src_buf->dma[0].iova & 0x7f) ||
		    (plane > 1 && (src_buf->dma[1].iova & 0x7f)) ||
		    (plane > 2 && (src_buf->dma[2].iova & 0x7f)))) {
			mml_log("warning: block format setting, buffer %#11llx %#11llx %#11llx",
				src_buf->dma[0].iova, src_buf->dma[1].iova,
				src_buf->dma[2].iova);
		}
	}

	if ((MML_FMT_PLANE(src->format) > 1) && src->uv_stride <= 0) {
		src->uv_stride = mml_color_get_min_uv_stride(src->format,
			src->width);
	}
	if ((plane == 1 && !src_buf->dma[0].iova) ||
	    (plane == 2 && (!src_buf->dma[1].iova || !src_buf->dma[0].iova)) ||
	    (plane == 3 && (!src_buf->dma[2].iova || !src_buf->dma[1].iova ||
	    !src_buf->dma[0].iova))) {
		mml_err("buffer plane number error, format = %#x, plane = %d",
			src->format, plane);
		return -1;
	}

	if (MML_FMT_H_SUBSAMPLE(src->format))
		src->width &= ~1;

	if (MML_FMT_V_SUBSAMPLE(src->format))
		src->height &= ~1;

	return 0;
}

static void calc_ufo(struct mml_file_buf *src_buf, struct mml_frame_data *src,
		     u64 *ufo_dec_length_y, u64 *ufo_dec_length_c,
		     u32 *u4pic_size_bs, u32 *u4pic_size_y_bs)
{
	u32 u4pic_size_y = src->width * src->height;
	u32 u4ufo_len_size_y =
		((((u4pic_size_y + 255) >> 8) + 63 + (16*8)) >> 6) << 6;
	u32 u4pic_size_c_bs;

	if (MML_FMT_10BIT_PACKED(src->format)) {
		if (MML_FMT_10BIT_JUMP(src->format)) {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y * 5 >> 3) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y * 5 >> 3;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	} else {
		if (MML_FMT_AUO(src->format)) {
			u4ufo_len_size_y = u4ufo_len_size_y << 1;
			*u4pic_size_y_bs = ((u4pic_size_y + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y >> 1) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs = ((u4pic_size_y + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y >> 1;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	}

	if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
		/* Y YL C CL*/
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_y_bs;
		*ufo_dec_length_c = src_buf->dma[1].iova + src->plane_offset[1] +
				   u4pic_size_c_bs;
	} else {
		/* Y C YL CL */
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs;
		*ufo_dec_length_c = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs + u4ufo_len_size_y;
	}
}

static void rdma_cpr_trigger(struct cmdq_pkt *pkt, u8 hw_pipe)
{
	cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_MML_LOCK);
	cmdq_pkt_assign_command(pkt, CMDQ_CPR_PREBUILT_PIPE(CMDQ_PREBUILT_MML),
				hw_pipe);
	cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_MML_WAIT);
	cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_MML_SET);
	cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_MML_LOCK);
}

static void rdma_select_threshold(struct mml_comp_rdma *rdma,
	struct cmdq_pkt *pkt, const phys_addr_t base_pa,
	u32 format, u32 width, u32 height)
{
	const struct rdma_golden *golden;
	const struct golden_setting *golden_set;
	u32 pixel = width * height;
	u32 idx, i;

	if (MML_FMT_PLANE(format) == 1) {
		if (MML_FMT_BITS_PER_PIXEL(format) >= 32)
			golden = &rdma->data->golden[GOLDEN_FMT_ARGB];
		else
			golden = &rdma->data->golden[GOLDEN_FMT_RGB];
	} else
		golden = &rdma->data->golden[GOLDEN_FMT_YUV420];

	for (idx = 0; idx < golden->cnt - 1; idx++)
		if (golden->settings[idx].pixel > pixel)
			break;
	golden_set = &golden->settings[idx];

	/* config threshold for all plane */
	for (i = 0; i < MML_FMT_PLANE(format); i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + rdma_dmabuf_con[i],
			3 << 24 | 32, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rdma_urgent_th[i],
			golden_set->plane[i].urgent, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rdma_ultra_th[i],
			golden_set->plane[i].ultra, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rdma_preultra_th[i],
			golden_set->plane[i].preultra, U32_MAX);
	}
}

static s32 rdma_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;
	u32 i;
	u8 simple_mode = 1;
	u8 filterMode;
	u8 loose = 0;
	u8 bit_number = 0;
	u8 auo = 0;
	u8 jump = 0;
	u8 afbc = 0;
	u8 afbc_y2r = 0;
	u8 hyfbc = 0;
	u8 ufbdc = 0;
	u8 ufbdc_sec_mode = 0;
	u32 write_mask = 0;
	u8 output_10bit = 0;
	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;
	u32 gmcif_con;

	mml_msg("use config %p rdma %p", cfg, rdma);

	/* Enable engine */
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_EN, 0x1, 0x00000001);

	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SHADOW_CTRL, 0x1, U32_MAX);

	rdma_color_fmt(cfg, rdma_frm);

	if (MML_FMT_V_SUBSAMPLE(src->format) &&
	    !MML_FMT_V_SUBSAMPLE(cfg->info.dest[0].data.format) &&
	    !MML_FMT_BLOCK(src->format))
		/* 420 to 422 interpolation solution */
		filterMode = 2;
	else
		/* config.enRDMACrop ? 3 : 2 */
		/* RSZ uses YUV422, RDMA could use V filter unless cropping */
		filterMode = 3;

	if (cfg->alpharot)
		rdma_frm->color_tran = 0;

	if (rdma_frm->blk_10bit && MML_FMT_IS_YUV(src->format)) {
		rdma_frm->matrix_sel = 15;
		rdma_frm->color_tran = 1;
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESV_DUMMY_0,
			       0x7, 0x00000007);
	} else {
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESV_DUMMY_0,
			       0x0, 0x00000007);
	}

	gmcif_con = BIT(0) |		/* COMMAND_DIV */
		    GENMASK(6, 4) |	/* READ_REQUEST_TYPE */
		    BIT(16);		/* PRE_ULTRA_EN */
	/* racing case also enable urgent/ultra to not blocking disp */
	if (unlikely(mml_racing_urgent)) {
		gmcif_con ^= BIT(16) | BIT(15);	/* URGENT_EN: always */
		for (i = 0; i < MML_FMT_PLANE(src->format); i++)
			cmdq_pkt_write(pkt, NULL,
				base_pa + rdma_urgent_th[i],
				0, U32_MAX);
	} else if (cfg->info.mode == MML_MODE_RACING) {
		gmcif_con |= BIT(12) |	/* ULTRA_EN */
			     BIT(14);	/* URGENT_EN */
		rdma_select_threshold(rdma, pkt, base_pa, src->format,
			src->width, src->height);
	} else {
		for (i = 0; i < MML_FMT_PLANE(src->format); i++)
			cmdq_pkt_write(pkt, NULL,
				base_pa + rdma_preultra_th[i],
				0, U32_MAX);
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_GMCIF_CON, gmcif_con, 0x0003f0f1);

	if (MML_FMT_IS_ARGB(src->format) &&
	    cfg->info.dest[0].pq_config.en_hdr &&
	    !cfg->info.dest[0].pq_config.en_dre &&
	    !cfg->info.dest[0].pq_config.en_sharp)
		rdma_frm->color_tran = 0;

	if (MML_FMT_10BIT_LOOSE(src->format))
		loose = 1;
	if (MML_FMT_10BIT(src->format))
		bit_number = 1;

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_CON,
		       (rdma_frm->hw_fmt << 0) +
		       (filterMode << 9) +
		       (loose << 11) +
		       (rdma_frm->field << 12) +
		       (rdma_frm->swap << 14) +
		       (rdma_frm->blk << 15) +
		       (bit_number << 18) +
		       (rdma_frm->blk_tile << 23) +
		       (0 << 24) +
		       (cfg->alpharot << 25),
		       0x038cfe0f);

	write_mask |= 0xb0000000 | 0x0643000;
	if (rdma_frm->blk_10bit)
		jump = MML_FMT_10BIT_JUMP(src->format);
	else
		auo = MML_FMT_AUO(src->format);

	if (MML_FMT_COMPRESS(src->format)) {
		rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			   CPR_RDMA_AFBC_PAYLOAD_OST,
			   0, write_sec);
		afbc = 1;
		if (MML_FMT_IS_ARGB(src->format))
			afbc_y2r = 1;
		ufbdc = 1;
		if (MML_FMT_IS_YUV(src->format)) {
			cmdq_pkt_write(pkt, NULL,
				base_pa + RDMA_MF_BKGD_SIZE_IN_PXL,
				((src->width + 15) >> 4) << 4, U32_MAX);
			cmdq_pkt_write(pkt, NULL,
				base_pa + RDMA_MF_BKGD_H_SIZE_IN_PXL,
				((src->height + 15) >> 4) << 4, U32_MAX);
		} else {
			cmdq_pkt_write(pkt, NULL,
				base_pa + RDMA_MF_BKGD_SIZE_IN_PXL,
				((src->width + 31) >> 5) << 5, U32_MAX);
			cmdq_pkt_write(pkt, NULL,
				base_pa + RDMA_MF_BKGD_H_SIZE_IN_PXL,
				((src->height + 7) >> 3) << 3, U32_MAX);
		}
		if (src->secure)
			ufbdc_sec_mode = 1;
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_COMP_CON,
		       (rdma_frm->enable_ufo << 31) +
		       (auo << 29) +
		       (jump << 28) +
		       (afbc << 22) +
		       (afbc_y2r << 21) +
		       (ufbdc_sec_mode << 18) +
		       (hyfbc << 13) +
		       (ufbdc << 12),
		       write_mask);

	if (src->secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rdma_write_addr(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_Y,
				ufo_dec_length_y,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				write_sec);

		rdma_write_addr(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_C,
				ufo_dec_length_c,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				write_sec);

		if (rdma_frm->blk_10bit)
			cmdq_pkt_write(pkt, NULL,
				       base_pa + RDMA_MF_BKGD_SIZE_IN_PXL,
				       (src->y_stride << 2) / 5, U32_MAX);
	}

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(cfg->info.dest[0].data.format))
		output_10bit = 1;
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_CON,
		       (rdma_frm->lb_2b_mode << 12) +
		       (output_10bit << 5) +
		       (simple_mode << 4),
		       U32_MAX);

	/* Write frame base address */
	if (unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {
		iova[0] = rdma->sram_pa + src->plane_offset[0];
		iova[1] = rdma->sram_pa + src->plane_offset[1];
		iova[2] = rdma->sram_pa + src->plane_offset[2];

		cmdq_pkt_write(pkt, NULL, rdma->smi_larb_con,
			GENMASK(19, 16), GENMASK(19, 16));

	} else if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
	}

	mml_msg("%s src %#011llx %#011llx %#011llx",
		__func__, iova[0], iova[1], iova[2]);

	if (!mml_slt) {
		rdma_write_addr(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				CPR_RDMA_SRC_BASE_0,
				iova[0],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_0],
				write_sec);
		rdma_write_addr(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				CPR_RDMA_SRC_BASE_1,
				iova[1],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_1],
				write_sec);
		rdma_write_addr(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				CPR_RDMA_SRC_BASE_2,
				iova[2],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_2],
				write_sec);
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_BKGD_SIZE_IN_BYTE,
		       src->y_stride, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SF_BKGD_SIZE_IN_BYTE,
		       src->uv_stride, U32_MAX);

	rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe, CPR_RDMA_TRANSFORM_0,
		   (rdma_frm->matrix_sel << 23) +
		   (rdma_frm->color_tran << 16),
		   write_sec);

	/* TODO: write ESL settings */
	return 0;
}

static s32 rdma_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 plane;

	const phys_addr_t base_pa = comp->base_pa;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u64 src_offset_0;
	u64 src_offset_1;
	u64 src_offset_2;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 mf_clip_w;
	u32 mf_clip_h;
	u32 mf_offset_w_1;
	u32 mf_offset_h_1;

	/* Following data retrieve from tile calc result */
	u64 in_xs = tile->in.xs;
	const u32 in_xe = tile->in.xe;
	u64 in_ys = tile->in.ys;
	const u32 in_ye = tile->in.ye;
	const u32 out_xs = tile->out.xs;
	const u32 out_xe = tile->out.xe;
	const u64 out_ys = tile->out.ys;
	const u32 out_ye = tile->out.ye;
	const u32 crop_ofst_x = tile->luma.x;
	const u32 crop_ofst_y = tile->luma.y;

	if (rdma_frm->blk) {
		/* Alignment X left in block boundary */
		in_xs = ((in_xs >> rdma_frm->vdo_blk_shift_w) <<
			rdma_frm->vdo_blk_shift_w);
		/* Alignment Y top in block boundary */
		in_ys = ((in_ys >> rdma_frm->vdo_blk_shift_h) <<
			rdma_frm->vdo_blk_shift_h);
	}

	if (MML_FMT_COMPRESS(src->format)) {
		rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			   CPR_RDMA_SRC_OFFSET_WP,
			   in_xs, write_sec);
		rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			   CPR_RDMA_SRC_OFFSET_HP,
			   in_ys, write_sec);
	}

	if (!rdma_frm->blk) {
		/* Set Y pixel offset */
		src_offset_0 = (in_xs * rdma_frm->bits_per_pixel_y >> 3) +
				in_ys * cfg->info.src.y_stride;

		/* Set U pixel offset */
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				cfg->info.src.uv_stride;

		/* Set V pixel offset */
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				cfg->info.src.uv_stride;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		/* Set crop offset */
		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else {
		/* Set Y pixel offset */
		src_offset_0 = (in_xs *
			       (rdma_frm->vdo_blk_height << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_y >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.y_stride;

		/* Set 10bit UFO mode */
		if (MML_FMT_10BIT_PACKED(src->format) && rdma_frm->enable_ufo) {
			rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				   CPR_RDMA_SRC_OFFSET_WP,
				   (src_offset_0 << 2) / 5,
				   write_sec);
			rdma_write(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
				   CPR_RDMA_SRC_OFFSET_HP,
				   0, write_sec);
		}

		/* Set U pixel offset */
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.uv_stride;

		/* Set V pixel offset */
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.uv_stride;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = (in_ye - in_ys + 1) << rdma_frm->field;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = (out_ye - out_ys + 1) << rdma_frm->field;

		/* Set crop offset */
		mf_offset_w_1 = out_xs - in_xs;
		mf_offset_h_1 = (out_ys - in_ys) << rdma_frm->field;
	}

	rdma_write_ofst(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			CPR_RDMA_SRC_OFFSET_0, src_offset_0, write_sec);
	rdma_write_ofst(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			CPR_RDMA_SRC_OFFSET_1, src_offset_1, write_sec);
	rdma_write_ofst(pkt, base_pa, cfg->path[ccfg->pipe]->hw_pipe,
			CPR_RDMA_SRC_OFFSET_2, src_offset_2, write_sec);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_SRC_SIZE,
		       (mf_src_h << 16) + mf_src_w, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_CLIP_SIZE,
		       (mf_clip_h << 16) + mf_clip_w, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_OFFSET_1,
		       (mf_offset_h_1 << 16) + mf_offset_w_1, U32_MAX);

	/* qos accumulate tile pixel */
	rdma_frm->pixel_acc += mf_src_w * mf_src_h;

	/* calculate qos for later use */
	plane = MML_FMT_PLANE(cfg->info.src.format);
	rdma_frm->datasize += mml_color_get_min_y_size(cfg->info.src.format,
		mf_src_w, mf_src_h);
	if (plane > 1)
		rdma_frm->datasize += mml_color_get_min_uv_size(cfg->info.src.format,
			mf_src_w, mf_src_h);
	if (plane > 2)
		rdma_frm->datasize += mml_color_get_min_uv_size(cfg->info.src.format,
			mf_src_w, mf_src_h);

	if (write_sec)
		rdma_cpr_trigger(pkt, cfg->path[ccfg->pipe]->hw_pipe);
	return 0;
}

static s32 rdma_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* wait rdma frame done */
	cmdq_pkt_wfe(pkt, rdma->event_eof);
	return 0;
}

static s32 rdma_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];

	/* ufo case */
	if (MML_FMT_UFO(cfg->info.src.format))
		rdma_frm->datasize = (u32)div_u64((u64)rdma_frm->datasize * 7, 10);

	/* Data size add to task and pixel,
	 * it is ok for rdma to directly assign and accumulate in wrot.
	 */
	cache->total_datasize = rdma_frm->datasize;
	cache->max_pixel = rdma_frm->pixel_acc;

	mml_msg("%s task %p pipe %hhu data %u pixel %u",
		__func__, task, ccfg->pipe, rdma_frm->datasize, rdma_frm->pixel_acc);

	/* for sram test rollback smi config */
	if (unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {
		struct mml_comp_rdma *rdma = comp_to_rdma(comp);

		cmdq_pkt_write(task->pkts[ccfg->pipe], NULL, rdma->smi_larb_con,
			0, GENMASK(19, 16));
	}

	return 0;
}

static s32 rdma_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;

	if (src->secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	if (unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {
		/* no need update addr for sram case */
		return 0;
	}

	if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rdma_update_addr(reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y_MSB],
				 ufo_dec_length_y);
		rdma_update_addr(reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C_MSB],
				 ufo_dec_length_c);
	}

	/* Write frame base address */
	if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
	}

	rdma_update_addr(reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_0],
			 rdma_frm->labels[RDMA_LABEL_BASE_0_MSB],
			 iova[0]);
	rdma_update_addr(reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_1],
			 rdma_frm->labels[RDMA_LABEL_BASE_1_MSB],
			 iova[1]);
	rdma_update_addr(reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_2],
			 rdma_frm->labels[RDMA_LABEL_BASE_2_MSB],
			 iova[2]);

	return 0;
}

static const struct mml_comp_config_ops rdma_cfg_ops = {
	.prepare = rdma_config_read,
	.buf_map = rdma_buf_map,
	.buf_unmap = rdma_buf_unmap,
	.get_label_count = rdma_get_label_count,
	.frame = rdma_config_frame,
	.tile = rdma_config_tile,
	.wait = rdma_wait,
	.post = rdma_post,
	.reframe = rdma_reconfig_frame,
};

u32 rdma_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);

	return rdma_frm->datasize;
}

u32 rdma_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.src.format;
}

static const struct mml_comp_hw_ops rdma_hw_ops = {
	.pw_enable = &mml_comp_pw_enable,
	.pw_disable = &mml_comp_pw_disable,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_datasize_get = &rdma_datasize_get,
	.qos_format_get = &rdma_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
};

static const char *rdma_state(u32 state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "wait sof";
	case 0x4:
		return "reg update";
	case 0x8:
		return "clear0";
	case 0x10:
		return "clear1";
	case 0x20:
		return "int0";
	case 0x40:
		return "int1";
	case 0x80:
		return "data running";
	case 0x100:
		return "wait done";
	case 0x200:
		return "warm reset";
	case 0x400:
		return "wait reset";
	default:
		return "";
	}
}

static void rdma_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	void __iomem *base = comp->base;
	u32 value[30];
	u32 mon[29];
	u32 state, greq;
	u32 shadow_ctrl;
	u32 i;

	mml_err("rdma component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + RDMA_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + RDMA_SHADOW_CTRL);

	if (rdma->data->write_sec_reg)
		cmdq_util_prebuilt_dump(0, CMDQ_TOKEN_PREBUILT_MML_WAIT);

	value[0] = readl(base + RDMA_EN);
	value[1] = readl(base + RDMA_RESET);
	value[2] = readl(base + RDMA_SRC_CON);
	value[3] = readl(base + RDMA_COMP_CON);
	/* for afbc case enable more debug info */
	if (value[3] & BIT(22)) {
		u32 debug_con = readl(base + RDMA_DEBUG_CON);

		debug_con |= 0xe000;
		writel(debug_con, base + RDMA_DEBUG_CON);
	}

	value[4] = readl(base + RDMA_MF_BKGD_SIZE_IN_BYTE);
	value[5] = readl(base + RDMA_MF_BKGD_SIZE_IN_PXL);
	value[6] = readl(base + RDMA_MF_SRC_SIZE);
	value[7] = readl(base + RDMA_MF_CLIP_SIZE);
	value[8] = readl(base + RDMA_MF_OFFSET_1);
	value[9] = readl(base + RDMA_SF_BKGD_SIZE_IN_BYTE);
	value[10] = readl(base + RDMA_MF_BKGD_H_SIZE_IN_PXL);
	if (!rdma->data->write_sec_reg) {
		value[11] = readl(base + RDMA_SRC_OFFSET_0_MSB);
		value[12] = readl(base + RDMA_SRC_OFFSET_0);
		value[13] = readl(base + RDMA_SRC_OFFSET_1_MSB);
		value[14] = readl(base + RDMA_SRC_OFFSET_1);
		value[15] = readl(base + RDMA_SRC_OFFSET_2_MSB);
		value[16] = readl(base + RDMA_SRC_OFFSET_2);
		value[17] = readl(base + RDMA_SRC_OFFSET_WP);
		value[18] = readl(base + RDMA_SRC_OFFSET_HP);
		value[19] = readl(base + RDMA_SRC_BASE_0_MSB);
		value[20] = readl(base + RDMA_SRC_BASE_0);
		value[21] = readl(base + RDMA_SRC_BASE_1_MSB);
		value[22] = readl(base + RDMA_SRC_BASE_1);
		value[23] = readl(base + RDMA_SRC_BASE_2_MSB);
		value[24] = readl(base + RDMA_SRC_BASE_2);
		value[25] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_Y_MSB);
		value[26] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_Y);
		value[27] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_C_MSB);
		value[28] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_C);
		value[29] = readl(base + RDMA_AFBC_PAYLOAD_OST);
	}

	/* mon sta from 0 ~ 28 */
	for (i = 0; i < ARRAY_SIZE(mon); i++)
		mon[i] = readl(base + RDMA_MON_STA_0 + i * 8);

	mml_err("RDMA_EN %#010x RDMA_RESET %#010x RDMA_SRC_CON %#010x RDMA_COMP_CON %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("RDMA_MF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_SIZE_IN_PXL %#010x",
		value[4], value[5]);
	mml_err("RDMA_MF_SRC_SIZE %#010x RDMA_MF_CLIP_SIZE %#010x RDMA_MF_OFFSET_1 %#010x",
		value[6], value[7], value[8]);
	mml_err("RDMA_SF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_H_SIZE_IN_PXL %#010x",
		value[9], value[10]);
	if (!rdma->data->write_sec_reg) {
		mml_err("RDMA_SRC OFFSET_0_MSB %#010x OFFSET_0 %#010x",
			value[11], value[12]);
		mml_err("RDMA_SRC OFFSET_1_MSB %#010x OFFSET_1 %#010x",
			value[13], value[14]);
		mml_err("RDMA_SRC OFFSET_2_MSB %#010x OFFSET_2 %#010x",
			value[15], value[16]);
		mml_err("RDMA_SRC_OFFSET_WP %#010x RDMA_SRC_OFFSET_HP %#010x",
			value[17], value[18]);
		mml_err("RDMA_SRC BASE_0_MSB %#010x BASE_0 %#010x",
			value[19], value[20]);
		mml_err("RDMA_SRC BASE_1_MSB %#010x BASE_1 %#010x",
			value[21], value[22]);
		mml_err("RDMA_SRC BASE_2_MSB %#010x BASE_2 %#010x",
			value[23], value[24]);
		mml_err("RDMA_UFO_DEC_LENGTH BASE_Y_MSB %#010x BASE_Y %#010x",
			value[25], value[26]);
		mml_err("RDMA_UFO_DEC_LENGTH BASE_C_MSB %#010x BASE_C %#010x",
			value[27], value[28]);
		mml_err("RDMA_AFBC_PAYLOAD_OST %#010x",
			value[29]);
	}

	for (i = 0; i < ARRAY_SIZE(mon) / 3; i++) {
		mml_err("RDMA_MON_STA_%-2u %#010x RDMA_MON_STA_%-2u %#010x RDMA_MON_STA_%-2u %#010x",
			i * 3, mon[i * 3],
			i * 3 + 1, mon[i * 3 + 1],
			i * 3 + 2, mon[i * 3 + 2]);
	}
	mml_err("RDMA_MON_STA_27 %#010x RDMA_MON_STA_28 %#010x",
		mon[27], mon[28]);

	/* parse state */
	mml_err("RDMA ack:%u req:%d ufo:%u",
		(mon[0] >> 11) & 0x1, (mon[0] >> 10) & 0x1,
		(mon[0] >> 25) & 0x1);
	state = (mon[1] >> 8) & 0x7ff;
	greq = (mon[0] >> 21) & 0x1;
	mml_err("RDMA state: %#x (%s)", state, rdma_state(state));
	mml_err("RDMA horz_cnt %u vert_cnt %u",
		mon[26] & 0xffff, (mon[26] >> 16) & 0xffff);
	mml_err("RDMA greq:%u => suggest to ask SMI help:%u", greq, greq);
}

static const struct mml_comp_debug_ops rdma_debug_ops = {
	.dump = &rdma_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rdma *rdma = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &rdma->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rdma *rdma = dev_get_drvdata(dev);

	mml_unregister_comp(master, &rdma->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_rdma *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rdma *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);
	priv->dev = dev;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret)
		dev_err(dev, "fail to config rdma dma mask %d\n", ret);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* init larb for smi and mtcmos */
	ret = mml_comp_init_larb(&priv->comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d",
			priv->comp.id, ret);
	}

	/* store smi larb con register for later use */
	priv->smi_larb_con = priv->comp.larb_base +
		SMI_LARB_NON_SEC_CON + priv->comp.larb_port * 4;
	mutex_init(&priv->sram_mutex);
	mml_log("comp(rdma) %u smi larb con %#lx", priv->comp.id, priv->smi_larb_con);

	if (of_property_read_u16(dev->of_node, "event_frame_done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	/* assign ops */
	priv->comp.tile_ops = &rdma_tile_ops;
	priv->comp.config_ops = &rdma_cfg_ops;
	priv->comp.hw_ops = &rdma_hw_ops;
	priv->comp.debug_ops = &rdma_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_rdma_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_rdma",
		.data = &mt6983_rdma_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_rdma",
		.data = &mt6893_rdma_data
	},
	{
		.compatible = "mediatek,mt6879-mml_rdma",
		.data = &mt6879_rdma_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rdma0",
		.data = &mt6895_rdma0_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rdma1",
		.data = &mt6895_rdma1_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_rdma_driver_dt_match);

struct platform_driver mml_rdma_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rdma",
		.owner = THIS_MODULE,
		.of_match_table = mml_rdma_driver_dt_match,
	},
};

//module_platform_driver(mml_rdma_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%08x name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%08x pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(rdma_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(rdma_debug, "mml rdma debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RDMA driver");
MODULE_LICENSE("GPL v2");
