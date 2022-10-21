// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>
#include "sde_hw_mdss.h"
#include "sde_hw_ctl.h"
#include "sde_hw_reg_dma_v1.h"
#include "msm_drv.h"
#include "msm_mmu.h"
#include "sde_dbg.h"

#define GUARD_BYTES (BIT(8) - 1)
#define ALIGNED_OFFSET (U32_MAX & ~(GUARD_BYTES))
#define ADDR_ALIGN BIT(8)
#define MAX_RELATIVE_OFF (BIT(20) - 1)
#define ABSOLUTE_RANGE BIT(27)

#define DECODE_SEL_OP (BIT(HW_BLK_SELECT))
#define REG_WRITE_OP ((BIT(REG_SINGLE_WRITE)) | (BIT(REG_BLK_WRITE_SINGLE)) | \
	(BIT(REG_BLK_WRITE_INC)) | (BIT(REG_BLK_WRITE_MULTIPLE)) | \
	(BIT(REG_SINGLE_MODIFY)))

#define REG_DMA_OPS (DECODE_SEL_OP | REG_WRITE_OP)
#define IS_OP_ALLOWED(op, buf_op) (BIT(op) & buf_op)

#define SET_UP_REG_DMA_REG(hw, reg_dma) \
	do { \
		(hw).base_off = (reg_dma)->addr; \
		(hw).blk_off = (reg_dma)->caps->base; \
		(hw).hwversion = (reg_dma)->caps->version; \
		(hw).log_mask = SDE_DBG_MASK_REGDMA; \
} while (0)

#define SIZE_DWORD(x) ((x) / (sizeof(u32)))
#define NOT_WORD_ALIGNED(x) ((x) & 0x3)


#define GRP_VIG_HW_BLK_SELECT (VIG0 | VIG1 | VIG2 | VIG3)
#define GRP_DMA_HW_BLK_SELECT (DMA0 | DMA1 | DMA2 | DMA3)
#define GRP_DSPP_HW_BLK_SELECT (DSPP0 | DSPP1 | DSPP2 | DSPP3)
#define GRP_LTM_HW_BLK_SELECT (LTM0 | LTM1)
#define GRP_MDSS_HW_BLK_SELECT (MDSS)
#define BUFFER_SPACE_LEFT(cfg) ((cfg)->dma_buf->buffer_size - \
			(cfg)->dma_buf->index)

#define REL_ADDR_OPCODE (BIT(27))
#define SINGLE_REG_WRITE_OPCODE (BIT(28))
#define SINGLE_REG_MODIFY_OPCODE (BIT(29))
#define HW_INDEX_REG_WRITE_OPCODE (BIT(28) | BIT(29))
#define AUTO_INC_REG_WRITE_OPCODE (BIT(30))
#define BLK_REG_WRITE_OPCODE (BIT(30) | BIT(28))

#define WRAP_MIN_SIZE 2
#define WRAP_MAX_SIZE (BIT(4) - 1)
#define MAX_DWORDS_SZ (BIT(14) - 1)
#define REG_DMA_HEADERS_BUFFER_SZ (sizeof(u32) * 128)

static uint32_t reg_dma_register_count;
static uint32_t reg_dma_decode_sel;
static uint32_t reg_dma_opmode_offset;
static uint32_t reg_dma_ctl0_queue0_cmd0_offset;
static uint32_t reg_dma_intr_status_offset;
static uint32_t reg_dma_intr_4_status_offset;
static uint32_t reg_dma_intr_clear_offset;
static uint32_t reg_dma_ctl_trigger_offset;
static uint32_t reg_dma_ctl0_reset_offset;
static uint32_t reg_dma_error_clear_mask;

typedef int (*reg_dma_internal_ops) (struct sde_reg_dma_setup_ops_cfg *cfg);

static struct sde_hw_reg_dma *reg_dma;
static u32 ops_mem_size[REG_DMA_SETUP_OPS_MAX] = {
	[REG_BLK_WRITE_SINGLE] = sizeof(u32) * 2,
	[REG_BLK_WRITE_INC] = sizeof(u32) * 2,
	[REG_BLK_WRITE_MULTIPLE] = sizeof(u32) * 2,
	[HW_BLK_SELECT] = sizeof(u32) * 2,
	[REG_SINGLE_WRITE] = sizeof(u32) * 2,
	[REG_SINGLE_MODIFY] = sizeof(u32) * 3,
};

static u32 queue_sel[DMA_CTL_QUEUE_MAX] = {
	[DMA_CTL_QUEUE0] = BIT(0),
	[DMA_CTL_QUEUE1] = BIT(4),
};

static u32 reg_dma_ctl_queue_off[CTL_MAX];
static u32 dspp_read_sel[DSPP_HIST_MAX] = {
	[DSPP0_HIST] = 0,
	[DSPP1_HIST] = 1,
	[DSPP2_HIST] = 2,
	[DSPP3_HIST] = 3,
};

static u32 v1_supported[REG_DMA_FEATURES_MAX]  = {
	[GAMUT] = GRP_VIG_HW_BLK_SELECT | GRP_DSPP_HW_BLK_SELECT,
	[VLUT] = GRP_DSPP_HW_BLK_SELECT,
	[GC] = GRP_DSPP_HW_BLK_SELECT,
	[IGC] = DSPP_IGC | GRP_DSPP_HW_BLK_SELECT,
	[PCC] = GRP_DSPP_HW_BLK_SELECT,
};

static u32 ctl_trigger_done_mask[CTL_MAX][DMA_CTL_QUEUE_MAX] = {
	[CTL_0][0] = BIT(16),
	[CTL_0][1] = BIT(21),
	[CTL_1][0] = BIT(17),
	[CTL_1][1] = BIT(22),
	[CTL_2][0] = BIT(18),
	[CTL_2][1] = BIT(23),
	[CTL_3][0] = BIT(19),
	[CTL_3][1] = BIT(24),
	[CTL_4][0] = BIT(25),
	[CTL_4][1] = BIT(27),
	[CTL_5][0] = BIT(26),
	[CTL_5][1] = BIT(28),
};

static int validate_dma_cfg(struct sde_reg_dma_setup_ops_cfg *cfg);
static int validate_write_decode_sel(struct sde_reg_dma_setup_ops_cfg *cfg);
static int validate_write_reg(struct sde_reg_dma_setup_ops_cfg *cfg);
static int validate_write_multi_lut_reg(struct sde_reg_dma_setup_ops_cfg *cfg);
static int validate_last_cmd(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_decode_sel(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_single_reg(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_multi_reg_index(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_multi_reg_inc(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_multi_lut_reg(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_single_modify(struct sde_reg_dma_setup_ops_cfg *cfg);
static int write_last_cmd(struct sde_reg_dma_setup_ops_cfg *cfg);
static int reset_reg_dma_buffer_v1(struct sde_reg_dma_buffer *lut_buf);
static int check_support_v1(enum sde_reg_dma_features feature,
		enum sde_reg_dma_blk blk, bool *is_supported);
static int setup_payload_v1(struct sde_reg_dma_setup_ops_cfg *cfg);
static int kick_off_v1(struct sde_reg_dma_kickoff_cfg *cfg);
static int reset_v1(struct sde_hw_ctl *ctl);
static int last_cmd_v1(struct sde_hw_ctl *ctl, enum sde_reg_dma_queue q,
		enum sde_reg_dma_last_cmd_mode mode);
static struct sde_reg_dma_buffer *alloc_reg_dma_buf_v1(u32 size);
static int dealloc_reg_dma_v1(struct sde_reg_dma_buffer *lut_buf);
static void dump_regs_v1(void);

static reg_dma_internal_ops write_dma_op_params[REG_DMA_SETUP_OPS_MAX] = {
	[HW_BLK_SELECT] = write_decode_sel,
	[REG_SINGLE_WRITE] = write_single_reg,
	[REG_BLK_WRITE_SINGLE] = write_multi_reg_inc,
	[REG_BLK_WRITE_INC] = write_multi_reg_index,
	[REG_BLK_WRITE_MULTIPLE] = write_multi_lut_reg,
	[REG_SINGLE_MODIFY] = write_single_modify,
};

static reg_dma_internal_ops validate_dma_op_params[REG_DMA_SETUP_OPS_MAX] = {
	[HW_BLK_SELECT] = validate_write_decode_sel,
	[REG_SINGLE_WRITE] = validate_write_reg,
	[REG_BLK_WRITE_SINGLE] = validate_write_reg,
	[REG_BLK_WRITE_INC] = validate_write_reg,
	[REG_BLK_WRITE_MULTIPLE] = validate_write_multi_lut_reg,
	[REG_SINGLE_MODIFY] = validate_write_reg,
};

static struct sde_reg_dma_buffer *last_cmd_buf[CTL_MAX];

static void get_decode_sel(unsigned long blk, u32 *decode_sel)
{
	int i = 0;

	*decode_sel = 0;
	for_each_set_bit(i, &blk, REG_DMA_BLK_MAX) {
		switch (BIT(i)) {
		case VIG0:
			*decode_sel |= BIT(0);
			break;
		case VIG1:
			*decode_sel |= BIT(1);
			break;
		case VIG2:
			*decode_sel |= BIT(2);
			break;
		case VIG3:
			*decode_sel |= BIT(3);
			break;
		case DMA0:
			*decode_sel |= BIT(5);
			break;
		case DMA1:
			*decode_sel |= BIT(6);
			break;
		case DMA2:
			*decode_sel |= BIT(7);
			break;
		case DMA3:
			*decode_sel |= BIT(8);
			break;
		case DSPP0:
			*decode_sel |= BIT(17);
			break;
		case DSPP1:
			*decode_sel |= BIT(18);
			break;
		case DSPP2:
			*decode_sel |= BIT(19);
			break;
		case DSPP3:
			*decode_sel |= BIT(20);
			break;
		case SSPP_IGC:
			*decode_sel |= BIT(4);
			break;
		case DSPP_IGC:
			*decode_sel |= BIT(21);
			break;
		case LTM0:
			*decode_sel |= BIT(22);
			break;
		case LTM1:
			*decode_sel |= BIT(23);
			break;
		case MDSS:
			*decode_sel |= BIT(31);
			break;
		default:
			DRM_ERROR("block not supported %zx\n", (size_t)BIT(i));
			break;
		}
	}
}

static int write_multi_reg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u8 *loc = NULL;

	loc =  (u8 *)cfg->dma_buf->vaddr + cfg->dma_buf->index;
	memcpy(loc, cfg->data, cfg->data_size);
	cfg->dma_buf->index += cfg->data_size;
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP | DECODE_SEL_OP;
	cfg->dma_buf->ops_completed |= REG_WRITE_OP;

	return 0;
}

int write_multi_reg_index(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = HW_INDEX_REG_WRITE_OPCODE;
	loc[0] |= (cfg->blk_offset & MAX_RELATIVE_OFF);
	if (cfg->blk == MDSS)
		loc[0] |= ABSOLUTE_RANGE;

	loc[1] = SIZE_DWORD(cfg->data_size);
	cfg->dma_buf->index += ops_mem_size[cfg->ops];

	return write_multi_reg(cfg);
}

int write_multi_reg_inc(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = AUTO_INC_REG_WRITE_OPCODE;
	if (cfg->blk == MDSS)
		loc[0] |= ABSOLUTE_RANGE;

	loc[0] |= (cfg->blk_offset & MAX_RELATIVE_OFF);
	loc[1] = SIZE_DWORD(cfg->data_size);
	cfg->dma_buf->index += ops_mem_size[cfg->ops];

	return write_multi_reg(cfg);
}

static int write_multi_lut_reg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = BLK_REG_WRITE_OPCODE;
	loc[0] |= (cfg->blk_offset & MAX_RELATIVE_OFF);
	if (cfg->blk == MDSS)
		loc[0] |= ABSOLUTE_RANGE;

	loc[1] = (cfg->inc) ? 0 : BIT(31);
	loc[1] |= (cfg->wrap_size & WRAP_MAX_SIZE) << 16;
	loc[1] |= ((SIZE_DWORD(cfg->data_size)) & MAX_DWORDS_SZ);
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP;
	cfg->dma_buf->index += ops_mem_size[cfg->ops];

	return write_multi_reg(cfg);
}

static int write_single_reg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = SINGLE_REG_WRITE_OPCODE;
	loc[0] |= (cfg->blk_offset & MAX_RELATIVE_OFF);
	if (cfg->blk == MDSS)
		loc[0] |= ABSOLUTE_RANGE;

	loc[1] = *cfg->data;
	cfg->dma_buf->index += ops_mem_size[cfg->ops];
	cfg->dma_buf->ops_completed |= REG_WRITE_OP;
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP | DECODE_SEL_OP;

	return 0;
}

static int write_single_modify(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = SINGLE_REG_MODIFY_OPCODE;
	loc[0] |= (cfg->blk_offset & MAX_RELATIVE_OFF);
	if (cfg->blk == MDSS)
		loc[0] |= ABSOLUTE_RANGE;

	loc[1] = cfg->mask;
	loc[2] = *cfg->data;
	cfg->dma_buf->index += ops_mem_size[cfg->ops];
	cfg->dma_buf->ops_completed |= REG_WRITE_OP;
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP | DECODE_SEL_OP;

	return 0;
}

static int write_decode_sel(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = reg_dma_decode_sel;
	get_decode_sel(cfg->blk, &loc[1]);
	cfg->dma_buf->index += ops_mem_size[cfg->ops];
	cfg->dma_buf->ops_completed |= DECODE_SEL_OP;
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP;

	return 0;
}

static int validate_write_multi_lut_reg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	int rc;

	rc = validate_write_reg(cfg);
	if (rc)
		return rc;

	if (cfg->wrap_size < WRAP_MIN_SIZE || cfg->wrap_size > WRAP_MAX_SIZE) {
		DRM_ERROR("invalid wrap sz %d min %d max %zd\n",
			cfg->wrap_size, WRAP_MIN_SIZE, (size_t)WRAP_MAX_SIZE);
		rc = -EINVAL;
	}

	return rc;
}

static int validate_write_reg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 remain_len, write_len;

	remain_len = BUFFER_SPACE_LEFT(cfg);
	write_len = ops_mem_size[cfg->ops] + cfg->data_size;
	if (remain_len < write_len) {
		DRM_ERROR("buffer is full sz %d needs %d bytes\n",
				remain_len, write_len);
		return -EINVAL;
	}

	if (!cfg->data) {
		DRM_ERROR("invalid data %pK size %d exp sz %d\n", cfg->data,
			cfg->data_size, write_len);
		return -EINVAL;
	}
	if ((SIZE_DWORD(cfg->data_size)) > MAX_DWORDS_SZ ||
	    NOT_WORD_ALIGNED(cfg->data_size)) {
		DRM_ERROR("Invalid data size %d max %zd align %x\n",
			cfg->data_size, (size_t)MAX_DWORDS_SZ,
			NOT_WORD_ALIGNED(cfg->data_size));
		return -EINVAL;
	}

	if (cfg->blk_offset > MAX_RELATIVE_OFF ||
			NOT_WORD_ALIGNED(cfg->blk_offset)) {
		DRM_ERROR("invalid offset %d max %zd align %x\n",
				cfg->blk_offset, (size_t)MAX_RELATIVE_OFF,
				NOT_WORD_ALIGNED(cfg->blk_offset));
		return -EINVAL;
	}

	return 0;
}

static int validate_write_decode_sel(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 remain_len;
	bool vig_blk, dma_blk, dspp_blk, mdss_blk;

	remain_len = BUFFER_SPACE_LEFT(cfg);
	if (remain_len < ops_mem_size[HW_BLK_SELECT]) {
		DRM_ERROR("buffer is full needs %d bytes\n",
				ops_mem_size[HW_BLK_SELECT]);
		return -EINVAL;
	}

	if (!cfg->blk) {
		DRM_ERROR("blk set as 0\n");
		return -EINVAL;
	}

	vig_blk = (cfg->blk & GRP_VIG_HW_BLK_SELECT) ? true : false;
	dma_blk = (cfg->blk & GRP_DMA_HW_BLK_SELECT) ? true : false;
	dspp_blk = (cfg->blk & GRP_DSPP_HW_BLK_SELECT) ? true : false;
	mdss_blk = (cfg->blk & MDSS) ? true : false;

	if ((vig_blk && dspp_blk) || (dma_blk && dspp_blk) ||
			(vig_blk && dma_blk) ||
			(mdss_blk && (vig_blk | dma_blk | dspp_blk))) {
		DRM_ERROR("invalid blk combination %x\n", cfg->blk);
		return -EINVAL;
	}

	return 0;
}

static int validate_dma_cfg(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	int rc = 0;
	bool supported;

	if (!cfg || cfg->ops >= REG_DMA_SETUP_OPS_MAX || !cfg->dma_buf) {
		DRM_ERROR("invalid param cfg %pK ops %d dma_buf %pK\n",
			cfg, ((cfg) ? cfg->ops : REG_DMA_SETUP_OPS_MAX),
			((cfg) ? cfg->dma_buf : NULL));
		return -EINVAL;
	}

	rc = check_support_v1(cfg->feature, cfg->blk, &supported);
	if (rc || !supported) {
		DRM_ERROR("check support failed rc %d supported %d\n",
				rc, supported);
		rc = -EINVAL;
		return rc;
	}

	if (cfg->dma_buf->index >= cfg->dma_buf->buffer_size ||
	    NOT_WORD_ALIGNED(cfg->dma_buf->index)) {
		DRM_ERROR("Buf Overflow index %d max size %d align %x\n",
			cfg->dma_buf->index, cfg->dma_buf->buffer_size,
			NOT_WORD_ALIGNED(cfg->dma_buf->index));
		return -EINVAL;
	}

	if (cfg->dma_buf->iova & GUARD_BYTES || !cfg->dma_buf->vaddr) {
		DRM_ERROR("iova not aligned to %zx iova %llx kva %pK",
				(size_t)ADDR_ALIGN, cfg->dma_buf->iova,
				cfg->dma_buf->vaddr);
		return -EINVAL;
	}
	if (!IS_OP_ALLOWED(cfg->ops, cfg->dma_buf->next_op_allowed)) {
		DRM_ERROR("invalid op %x allowed %x\n", cfg->ops,
				cfg->dma_buf->next_op_allowed);
		return -EINVAL;
	}

	if (!validate_dma_op_params[cfg->ops] ||
	    !write_dma_op_params[cfg->ops]) {
		DRM_ERROR("invalid op %d validate %pK write %pK\n", cfg->ops,
			validate_dma_op_params[cfg->ops],
			write_dma_op_params[cfg->ops]);
		return -EINVAL;
	}
	return rc;
}

static int validate_kick_off_v1(struct sde_reg_dma_kickoff_cfg *cfg)
{

	if (!cfg || !cfg->ctl || !cfg->dma_buf) {
		DRM_ERROR("invalid cfg %pK ctl %pK dma_buf %pK\n",
			cfg, ((!cfg) ? NULL : cfg->ctl),
			((!cfg) ? NULL : cfg->dma_buf));
		return -EINVAL;
	}

	if (cfg->ctl->idx < CTL_0 && cfg->ctl->idx >= CTL_MAX) {
		DRM_ERROR("invalid ctl idx %d\n", cfg->ctl->idx);
		return -EINVAL;
	}

	if (cfg->op >= REG_DMA_OP_MAX) {
		DRM_ERROR("invalid op %d\n", cfg->op);
		return -EINVAL;
	}

	if ((cfg->op == REG_DMA_WRITE) &&
	     (!(cfg->dma_buf->ops_completed & DECODE_SEL_OP) ||
	     !(cfg->dma_buf->ops_completed & REG_WRITE_OP))) {
		DRM_ERROR("incomplete write ops %x\n",
				cfg->dma_buf->ops_completed);
		return -EINVAL;
	}

	if (cfg->op == REG_DMA_READ && cfg->block_select >= DSPP_HIST_MAX) {
		DRM_ERROR("invalid block for read %d\n", cfg->block_select);
		return -EINVAL;
	}

	/* Only immediate triggers are supported now hence hardcode */
	cfg->trigger_mode = (cfg->op == REG_DMA_READ) ? (READ_TRIGGER) :
				(WRITE_TRIGGER);

	if (cfg->dma_buf->iova & GUARD_BYTES) {
		DRM_ERROR("Address is not aligned to %zx iova %llx",
				(size_t)ADDR_ALIGN, cfg->dma_buf->iova);
		return -EINVAL;
	}

	if (cfg->queue_select >= DMA_CTL_QUEUE_MAX) {
		DRM_ERROR("invalid queue selected %d\n", cfg->queue_select);
		return -EINVAL;
	}

	if (SIZE_DWORD(cfg->dma_buf->index) > MAX_DWORDS_SZ ||
			!cfg->dma_buf->index) {
		DRM_ERROR("invalid dword size %zd max %zd\n",
			(size_t)SIZE_DWORD(cfg->dma_buf->index),
				(size_t)MAX_DWORDS_SZ);
		return -EINVAL;
	}
	return 0;
}

static int write_kick_off_v1(struct sde_reg_dma_kickoff_cfg *cfg)
{
	u32 cmd1, mask = 0, val = 0;
	struct sde_hw_blk_reg_map hw;

	memset(&hw, 0, sizeof(hw));
	msm_gem_sync(cfg->dma_buf->buf);
	cmd1 = (cfg->op == REG_DMA_READ) ?
		(dspp_read_sel[cfg->block_select] << 30) : 0;
	cmd1 |= (cfg->last_command) ? BIT(24) : 0;
	cmd1 |= (cfg->op == REG_DMA_READ) ? (2 << 22) : 0;
	cmd1 |= (cfg->op == REG_DMA_WRITE) ? (BIT(22)) : 0;
	cmd1 |= (SIZE_DWORD(cfg->dma_buf->index) & MAX_DWORDS_SZ);

	SET_UP_REG_DMA_REG(hw, reg_dma);
	SDE_REG_WRITE(&hw, reg_dma_opmode_offset, BIT(0));
	val = SDE_REG_READ(&hw, reg_dma_intr_4_status_offset);
	if (val) {
		DRM_DEBUG("LUT dma status %x\n", val);
		mask = reg_dma_error_clear_mask;
		SDE_REG_WRITE(&hw, reg_dma_intr_clear_offset + sizeof(u32) * 4,
				mask);
		SDE_EVT32(val);
	}

	SDE_REG_WRITE(&hw, reg_dma_ctl_queue_off[cfg->ctl->idx],
			cfg->dma_buf->iova);
	SDE_REG_WRITE(&hw, reg_dma_ctl_queue_off[cfg->ctl->idx] + 0x4,
			cmd1);
	if (cfg->last_command) {
		mask = ctl_trigger_done_mask[cfg->ctl->idx][cfg->queue_select];
		SDE_REG_WRITE(&hw, reg_dma_intr_clear_offset, mask);
		SDE_REG_WRITE(&cfg->ctl->hw, reg_dma_ctl_trigger_offset,
			queue_sel[cfg->queue_select]);
	}

	return 0;
}

int init_v1(struct sde_hw_reg_dma *cfg)
{
	int i = 0, rc = 0;

	if (!cfg)
		return -EINVAL;

	reg_dma = cfg;
	for (i = CTL_0; i < CTL_MAX; i++) {
		if (!last_cmd_buf[i]) {
			last_cmd_buf[i] =
			    alloc_reg_dma_buf_v1(REG_DMA_HEADERS_BUFFER_SZ);
			if (IS_ERR_OR_NULL(last_cmd_buf[i])) {
				/*
				 * This will allow reg dma to fall back to
				 * AHB domain
				 */
				pr_info("Failed to allocate reg dma, ret:%lu\n",
						PTR_ERR(last_cmd_buf[i]));
				return 0;
			}
		}
	}
	if (rc) {
		for (i = 0; i < CTL_MAX; i++) {
			if (!last_cmd_buf[i])
				continue;
			dealloc_reg_dma_v1(last_cmd_buf[i]);
			last_cmd_buf[i] = NULL;
		}
		return rc;
	}

	reg_dma->ops.check_support = check_support_v1;
	reg_dma->ops.setup_payload = setup_payload_v1;
	reg_dma->ops.kick_off = kick_off_v1;
	reg_dma->ops.reset = reset_v1;
	reg_dma->ops.alloc_reg_dma_buf = alloc_reg_dma_buf_v1;
	reg_dma->ops.dealloc_reg_dma = dealloc_reg_dma_v1;
	reg_dma->ops.reset_reg_dma_buf = reset_reg_dma_buffer_v1;
	reg_dma->ops.last_command = last_cmd_v1;
	reg_dma->ops.dump_regs = dump_regs_v1;

	reg_dma_register_count = 60;
	reg_dma_decode_sel = 0x180ac060;
	reg_dma_opmode_offset = 0x4;
	reg_dma_ctl0_queue0_cmd0_offset = 0x14;
	reg_dma_intr_status_offset = 0x90;
	reg_dma_intr_4_status_offset = 0xa0;
	reg_dma_intr_clear_offset = 0xb0;
	reg_dma_ctl_trigger_offset = 0xd4;
	reg_dma_ctl0_reset_offset = 0xe4;
	reg_dma_error_clear_mask = BIT(0) | BIT(1) | BIT(2) | BIT(16);

	reg_dma_ctl_queue_off[CTL_0] = reg_dma_ctl0_queue0_cmd0_offset;
	for (i = CTL_1; i < ARRAY_SIZE(reg_dma_ctl_queue_off); i++)
		reg_dma_ctl_queue_off[i] = reg_dma_ctl_queue_off[i - 1] +
			(sizeof(u32) * 4);

	return 0;
}

int init_v11(struct sde_hw_reg_dma *cfg)
{
	int ret = 0, i = 0;

	ret = init_v1(cfg);
	if (ret) {
		DRM_ERROR("failed to initialize v1: ret %d\n", ret);
		return -EINVAL;
	}

	/* initialize register offsets and v1_supported based on version */
	reg_dma_register_count = 133;
	reg_dma_decode_sel = 0x180ac114;
	reg_dma_opmode_offset = 0x4;
	reg_dma_ctl0_queue0_cmd0_offset = 0x14;
	reg_dma_intr_status_offset = 0x160;
	reg_dma_intr_4_status_offset = 0x170;
	reg_dma_intr_clear_offset = 0x1a0;
	reg_dma_ctl_trigger_offset = 0xd4;
	reg_dma_ctl0_reset_offset = 0x200;
	reg_dma_error_clear_mask = BIT(0) | BIT(1) | BIT(2) | BIT(16) |
		BIT(17) | BIT(18);

	reg_dma_ctl_queue_off[CTL_0] = reg_dma_ctl0_queue0_cmd0_offset;
	for (i = CTL_1; i < ARRAY_SIZE(reg_dma_ctl_queue_off); i++)
		reg_dma_ctl_queue_off[i] = reg_dma_ctl_queue_off[i - 1] +
			(sizeof(u32) * 4);

	v1_supported[IGC] = DSPP_IGC | GRP_DSPP_HW_BLK_SELECT |
				GRP_VIG_HW_BLK_SELECT | GRP_DMA_HW_BLK_SELECT;
	v1_supported[GC] = GRP_DMA_HW_BLK_SELECT | GRP_DSPP_HW_BLK_SELECT;
	v1_supported[HSIC] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[SIX_ZONE] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[MEMC_SKIN] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[MEMC_SKY] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[MEMC_FOLIAGE] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[MEMC_PROT] = GRP_DSPP_HW_BLK_SELECT;
	v1_supported[QSEED] = GRP_VIG_HW_BLK_SELECT;

	return 0;
}

int init_v12(struct sde_hw_reg_dma *cfg)
{
	int ret = 0;

	ret = init_v11(cfg);
	if (ret) {
		DRM_ERROR("failed to initialize v11: ret %d\n", ret);
		return ret;
	}

	v1_supported[LTM_INIT] = GRP_LTM_HW_BLK_SELECT;
	v1_supported[LTM_ROI] = GRP_LTM_HW_BLK_SELECT;
	v1_supported[LTM_VLUT] = GRP_LTM_HW_BLK_SELECT;
	v1_supported[RC_DATA] = (GRP_DSPP_HW_BLK_SELECT |
			GRP_MDSS_HW_BLK_SELECT);

	return 0;
}

static int check_support_v1(enum sde_reg_dma_features feature,
		     enum sde_reg_dma_blk blk,
		     bool *is_supported)
{
	int ret = 0;

	if (!is_supported)
		return -EINVAL;

	if (feature >= REG_DMA_FEATURES_MAX
			|| blk >=  BIT_ULL(REG_DMA_BLK_MAX)) {
		*is_supported = false;
		return ret;
	}

	*is_supported = (blk & v1_supported[feature]) ? true : false;
	return ret;
}

static int setup_payload_v1(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	int rc = 0;

	rc = validate_dma_cfg(cfg);

	if (!rc)
		rc = validate_dma_op_params[cfg->ops](cfg);

	if (!rc)
		rc = write_dma_op_params[cfg->ops](cfg);

	return rc;
}


static int kick_off_v1(struct sde_reg_dma_kickoff_cfg *cfg)
{
	int rc = 0;

	rc = validate_kick_off_v1(cfg);
	if (rc)
		return rc;

	rc = write_kick_off_v1(cfg);
	return rc;
}

int reset_v1(struct sde_hw_ctl *ctl)
{
	struct sde_hw_blk_reg_map hw;
	u32 index, val, i = 0;

	if (!ctl || ctl->idx > CTL_MAX) {
		DRM_ERROR("invalid ctl %pK ctl idx %d\n",
			ctl, ((ctl) ? ctl->idx : 0));
		return -EINVAL;
	}

	memset(&hw, 0, sizeof(hw));
	index = ctl->idx - CTL_0;
	SET_UP_REG_DMA_REG(hw, reg_dma);
	SDE_REG_WRITE(&hw, reg_dma_opmode_offset, BIT(0));
	SDE_REG_WRITE(&hw, (reg_dma_ctl0_reset_offset + index * sizeof(u32)),
			BIT(0));

	i = 0;
	do {
		udelay(1000);
		i++;
		val = SDE_REG_READ(&hw,
			(reg_dma_ctl0_reset_offset + index * sizeof(u32)));
	} while (i < 2 && val);

	return 0;
}

static void sde_reg_dma_aspace_cb_locked(void *cb_data, bool is_detach)
{
	struct sde_reg_dma_buffer *dma_buf = NULL;
	struct msm_gem_address_space *aspace = NULL;
	u32 iova_aligned, offset;
	int rc;

	if (!cb_data) {
		DRM_ERROR("aspace cb called with invalid dma_buf\n");
		return;
	}

	dma_buf = (struct sde_reg_dma_buffer *)cb_data;
	aspace = dma_buf->aspace;

	if (is_detach) {
		/* invalidate the stored iova */
		dma_buf->iova = 0;

		/* return the virtual address mapping */
		msm_gem_put_vaddr(dma_buf->buf);
		msm_gem_vunmap(dma_buf->buf, OBJ_LOCK_NORMAL);

	} else {
		rc = msm_gem_get_iova(dma_buf->buf, aspace,
				&dma_buf->iova);
		if (rc) {
			DRM_ERROR("failed to get the iova rc %d\n", rc);
			return;
		}

		dma_buf->vaddr = msm_gem_get_vaddr(dma_buf->buf);
		if (IS_ERR_OR_NULL(dma_buf->vaddr)) {
			DRM_ERROR("failed to get va rc %d\n", rc);
			return;
		}

		iova_aligned = (dma_buf->iova + GUARD_BYTES) & ALIGNED_OFFSET;
		offset = iova_aligned - dma_buf->iova;
		dma_buf->iova = dma_buf->iova + offset;
		dma_buf->vaddr = (void *)(((u8 *)dma_buf->vaddr) + offset);
		dma_buf->next_op_allowed = DECODE_SEL_OP;
	}
}

static struct sde_reg_dma_buffer *alloc_reg_dma_buf_v1(u32 size)
{
	struct sde_reg_dma_buffer *dma_buf = NULL;
	u32 iova_aligned, offset;
	u32 rsize = size + GUARD_BYTES;
	struct msm_gem_address_space *aspace = NULL;
	int rc = 0;

	if (!size || SIZE_DWORD(size) > MAX_DWORDS_SZ) {
		DRM_ERROR("invalid buffer size %d, max %d\n",
				SIZE_DWORD(size), MAX_DWORDS_SZ);
		return ERR_PTR(-EINVAL);
	}

	dma_buf = kzalloc(sizeof(*dma_buf), GFP_KERNEL);
	if (!dma_buf)
		return ERR_PTR(-ENOMEM);

	dma_buf->buf = msm_gem_new(reg_dma->drm_dev,
				    rsize, MSM_BO_UNCACHED);
	if (IS_ERR_OR_NULL(dma_buf->buf)) {
		rc = -EINVAL;
		goto fail;
	}

	aspace = msm_gem_smmu_address_space_get(reg_dma->drm_dev,
			MSM_SMMU_DOMAIN_UNSECURE);
	if (!aspace) {
		DRM_ERROR("failed to get aspace\n");
		rc = -EINVAL;
		goto free_gem;
	}

	/* register to aspace */
	rc = msm_gem_address_space_register_cb(aspace,
			sde_reg_dma_aspace_cb_locked,
			(void *)dma_buf);
	if (rc) {
		DRM_ERROR("failed to register callback %d", rc);
		goto free_gem;
	}

	dma_buf->aspace = aspace;
	rc = msm_gem_get_iova(dma_buf->buf, aspace, &dma_buf->iova);
	if (rc) {
		DRM_ERROR("failed to get the iova rc %d\n", rc);
		goto free_aspace_cb;
	}

	dma_buf->vaddr = msm_gem_get_vaddr(dma_buf->buf);
	if (IS_ERR_OR_NULL(dma_buf->vaddr)) {
		DRM_ERROR("failed to get va rc %d\n", rc);
		rc = -EINVAL;
		goto put_iova;
	}

	dma_buf->buffer_size = size;
	iova_aligned = (dma_buf->iova + GUARD_BYTES) & ALIGNED_OFFSET;
	offset = iova_aligned - dma_buf->iova;
	dma_buf->iova = dma_buf->iova + offset;
	dma_buf->vaddr = (void *)(((u8 *)dma_buf->vaddr) + offset);
	dma_buf->next_op_allowed = DECODE_SEL_OP;

	return dma_buf;

put_iova:
	msm_gem_put_iova(dma_buf->buf, aspace);
free_aspace_cb:
	msm_gem_address_space_unregister_cb(aspace,
			sde_reg_dma_aspace_cb_locked, dma_buf);
free_gem:
	mutex_lock(&reg_dma->drm_dev->struct_mutex);
	msm_gem_free_object(dma_buf->buf);
	mutex_unlock(&reg_dma->drm_dev->struct_mutex);
fail:
	kfree(dma_buf);
	return ERR_PTR(rc);
}

static int dealloc_reg_dma_v1(struct sde_reg_dma_buffer *dma_buf)
{
	if (!dma_buf) {
		DRM_ERROR("invalid param reg_buf %pK\n", dma_buf);
		return -EINVAL;
	}

	if (dma_buf->buf) {
		msm_gem_put_iova(dma_buf->buf, 0);
		msm_gem_address_space_unregister_cb(dma_buf->aspace,
				sde_reg_dma_aspace_cb_locked, dma_buf);
		mutex_lock(&reg_dma->drm_dev->struct_mutex);
		msm_gem_free_object(dma_buf->buf);
		mutex_unlock(&reg_dma->drm_dev->struct_mutex);
	}

	kfree(dma_buf);
	return 0;
}

static int reset_reg_dma_buffer_v1(struct sde_reg_dma_buffer *lut_buf)
{
	if (!lut_buf)
		return -EINVAL;

	lut_buf->index = 0;
	lut_buf->ops_completed = 0;
	lut_buf->next_op_allowed = DECODE_SEL_OP;
	return 0;
}

static int validate_last_cmd(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 remain_len, write_len;

	remain_len = BUFFER_SPACE_LEFT(cfg);
	write_len = sizeof(u32);
	if (remain_len < write_len) {
		DRM_ERROR("buffer is full sz %d needs %d bytes\n",
				remain_len, write_len);
		return -EINVAL;
	}
	return 0;
}

static int write_last_cmd(struct sde_reg_dma_setup_ops_cfg *cfg)
{
	u32 *loc = NULL;

	loc =  (u32 *)((u8 *)cfg->dma_buf->vaddr +
			cfg->dma_buf->index);
	loc[0] = reg_dma_decode_sel;
	loc[1] = 0;
	cfg->dma_buf->index = sizeof(u32) * 2;
	cfg->dma_buf->ops_completed = REG_WRITE_OP | DECODE_SEL_OP;
	cfg->dma_buf->next_op_allowed = REG_WRITE_OP;

	return 0;
}

static int last_cmd_v1(struct sde_hw_ctl *ctl, enum sde_reg_dma_queue q,
		enum sde_reg_dma_last_cmd_mode mode)
{
	struct sde_reg_dma_setup_ops_cfg cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_blk_reg_map hw;
	u32 val;
	int rc;

	if (!ctl || ctl->idx >= CTL_MAX || q >= DMA_CTL_QUEUE_MAX) {
		DRM_ERROR("ctl %pK q %d index %d\n", ctl, q,
				((ctl) ? ctl->idx : -1));
		return -EINVAL;
	}

	if (!last_cmd_buf[ctl->idx] || !last_cmd_buf[ctl->idx]->iova) {
		DRM_DEBUG("invalid last cmd buf for idx %d\n", ctl->idx);
		return 0;
	}

	cfg.dma_buf = last_cmd_buf[ctl->idx];
	reset_reg_dma_buffer_v1(last_cmd_buf[ctl->idx]);
	if (validate_last_cmd(&cfg)) {
		DRM_ERROR("validate buf failed\n");
		return -EINVAL;
	}

	if (write_last_cmd(&cfg)) {
		DRM_ERROR("write buf failed\n");
		return -EINVAL;
	}

	kick_off.ctl = ctl;
	kick_off.queue_select = q;
	kick_off.trigger_mode = WRITE_IMMEDIATE;
	kick_off.last_command = 1;
	kick_off.op = REG_DMA_WRITE;
	kick_off.dma_buf = last_cmd_buf[ctl->idx];
	if (kick_off_v1(&kick_off)) {
		DRM_ERROR("kick off last cmd failed\n");
		return -EINVAL;
	}

	memset(&hw, 0, sizeof(hw));
	SET_UP_REG_DMA_REG(hw, reg_dma);

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, mode);
	if (mode == REG_DMA_WAIT4_COMP) {
		rc = readl_poll_timeout(hw.base_off + hw.blk_off +
			reg_dma_intr_status_offset, val,
			(val & ctl_trigger_done_mask[ctl->idx][q]),
			10, 20000);
		if (rc)
			DRM_ERROR("poll wait failed %d val %x mask %x\n",
			    rc, val, ctl_trigger_done_mask[ctl->idx][q]);
		SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, mode);
	}

	return 0;
}

void deinit_v1(void)
{
	int i = 0;

	for (i = CTL_0; i < CTL_MAX; i++) {
		if (last_cmd_buf[i])
			dealloc_reg_dma_v1(last_cmd_buf[i]);
		last_cmd_buf[i] = NULL;
	}
}

static void dump_regs_v1(void)
{
	uint32_t i = 0;
	u32 val;
	struct sde_hw_blk_reg_map hw;

	memset(&hw, 0, sizeof(hw));
	SET_UP_REG_DMA_REG(hw, reg_dma);

	for (i = 0; i < reg_dma_register_count; i++) {
		val = SDE_REG_READ(&hw, i * sizeof(u32));
		DRM_ERROR("offset %x val %x\n", (u32)(i * sizeof(u32)), val);
	}
}
