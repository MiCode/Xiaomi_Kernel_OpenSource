// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dsc.h"
#include "sde_hw_pingpong.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_hw_vdc.h"
#include "sde_vdc_helper.h"

#define VDC_CMN_MAIN_CNF           0x00

/* SDE_VDC_ENC register offsets */
#define ENC_OUT_BF_CTRL            0x00
#define ENC_GENERAL_STATUS         0x04
#define ENC_HSLICE_STATUS          0x08
#define ENC_OUT_STATUS             0x0C
#define ENC_INT_STAT               0x10
#define ENC_INT_CLR                0x14
#define ENC_INT_ENABLE             0x18
#define ENC_R2B_BUF_CTRL           0x1c
#define ENC_ORIG_SLICE             0x40
#define ENC_DF_CTRL                0x44
#define ENC_VDC_VERSION            0x80
#define ENC_VDC_FRAME_SIZE         0x84
#define ENC_VDC_SLICE_SIZE         0x88
#define ENC_VDC_SLICE_PX           0x8c
#define ENC_VDC_MAIN_CONF          0x90
#define ENC_VDC_CHUNK_SIZE         0x94
#define ENC_VDC_RC_CONFIG_0        0x98
#define ENC_VDC_RC_CONFIG_1        0x9c
#define ENC_VDC_RC_CONFIG_2        0xa0
#define ENC_VDC_RC_CONFIG_3        0xa4
#define ENC_VDC_RC_CONFIG_4        0xa8
#define ENC_VDC_FLAT_CONFIG        0xac
#define ENC_VDC_FLAT_LUT_3_0       0xb0
#define ENC_VDC_FLAT_LUT_7_4       0xb4
#define ENC_VDC_MAX_QP_LUT_3_0     0xb8
#define ENC_VDC_MAX_QP_LUT_7_4     0xbc
#define ENC_VDC_TAR_RATE_LUT_3_0   0xc0
#define ENC_VDC_TAR_RATE_LUT_7_4   0xc4
#define ENC_VDC_TAR_RATE_LUT_11_8  0xc8
#define ENC_VDC_TAR_RATE_LUT_15_12 0xcc
#define ENC_VDC_MPPF_CONFIG        0xd0
#define ENC_VDC_SSM_CONFIG         0xd4
#define ENC_VDC_SLICE_NUM_BITS_0   0xd8
#define ENC_VDC_SLICE_NUM_BITS_1   0xdc
#define ENC_VDC_RC_PRECOMPUTE      0xe0
#define ENC_VDC_MPP_CONFIG         0xe4
#define ENC_VDC_LBDA_BRATE_LUT     0x100
#define ENC_VDC_LBDA_BF_LUT        0x180
#define ENC_VDC_OTHER_RC           0x1c0

/* SDE_VDC_CTL register offsets */
#define VDC_CTL                    0x00
#define VDC_CFG                    0x04
#define VDC_DATA_IN_SWAP           0x08
#define VDC_CLK_CTRL               0x0C

#define VDC_CTL_BLOCK_SIZE         0x300

static inline _vdc_subblk_offset(struct sde_hw_vdc *hw_vdc, int s_id,
		u32 *idx)
{
	int rc = 0;
	const struct sde_vdc_sub_blks *sblk;

	if (!hw_vdc)
		return -EINVAL;

	sblk = hw_vdc->caps->sblk;

	switch (s_id) {
	case SDE_VDC_ENC:
		*idx = sblk->enc.base;
		break;
	case SDE_VDC_CTL:
		*idx = sblk->ctl.base;
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}

static void sde_hw_vdc_disable(struct sde_hw_vdc *hw_vdc)
{
	struct sde_hw_blk_reg_map *vdc_reg;
	u32 idx;

	if (!hw_vdc)
		return;

	if (_vdc_subblk_offset(hw_vdc, SDE_VDC_CTL, &idx))
		return;

	vdc_reg = &hw_vdc->hw;
	SDE_REG_WRITE(vdc_reg, VDC_CFG + idx, 0);

	/* common register */
	SDE_REG_WRITE(vdc_reg, VDC_CMN_MAIN_CNF, 0);
}

static void sde_hw_vdc_config(struct sde_hw_vdc *hw_vdc,
	struct msm_display_vdc_info *vdc)
{
	struct sde_hw_blk_reg_map *vdc_reg = &hw_vdc->hw;
	u32 idx;
	u32 data = 0;
	int i = 0;
	u8 bits_per_component;
	int addr_off = 0;
	u32 slice_num_bits_ub, slice_num_bits_ldw;

	if (!hw_vdc)
		return;

	if (_vdc_subblk_offset(hw_vdc, SDE_VDC_ENC, &idx))
		return;

	data = ((vdc->ob1_max_addr & 0xffff) << 16);
	data |= (vdc->ob0_max_addr & 0xffff);
	SDE_REG_WRITE(vdc_reg, ENC_OUT_BF_CTRL + idx, data);

	data = ((vdc->r2b1_max_addr & 0xffff) << 16);
	data |= (vdc->r2b0_max_addr & 0xffff);
	SDE_REG_WRITE(vdc_reg, ENC_R2B_BUF_CTRL + idx, data);

	data = vdc->slice_width_orig;
	SDE_REG_WRITE(vdc_reg, ENC_ORIG_SLICE + idx, data);

	data = 0;
	if (vdc->panel_mode == VDC_VIDEO_MODE)
		data |= BIT(9);
	data |= ((vdc->num_of_active_ss - 1) << 12);
	data |= vdc->initial_lines;
	SDE_REG_WRITE(vdc_reg, ENC_DF_CTRL + idx, data);

	data = 0;
	data |= (vdc->version_major << 24);
	data |= (vdc->version_minor << 16);
	data |= (vdc->version_release << 8);
	SDE_REG_WRITE(vdc_reg, ENC_VDC_VERSION + idx, data);

	data = 0;
	data |= (vdc->frame_width << 16);
	data |= vdc->frame_height;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_FRAME_SIZE + idx, data);

	data = 0;
	data |= (vdc->slice_width << 16);
	data |=  vdc->slice_height;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_SLICE_SIZE + idx, data);

	SDE_REG_WRITE(vdc_reg, ENC_VDC_SLICE_PX + idx,
		vdc->slice_num_px);

	data = 0;
	data |= (vdc->bits_per_pixel << 16);
	if (vdc->bits_per_component == 8)
		bits_per_component = 0;
	else if (vdc->bits_per_component == 10)
		bits_per_component = 1;
	else
		bits_per_component = 2;
	data |= (bits_per_component << 4);
	data |= (vdc->source_color_space << 2);
	data |= vdc->chroma_format;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_MAIN_CONF + idx,
		data);

	SDE_REG_WRITE(vdc_reg, ENC_VDC_CHUNK_SIZE + idx,
		vdc->chunk_size);

	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_CONFIG_0 + idx,
		vdc->rc_buffer_init_size);

	data = 0;
	data |= (vdc->rc_stuffing_bits << 24);
	data |= (vdc->rc_init_tx_delay << 16);
	data |= vdc->rc_buffer_max_size;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_CONFIG_1 + idx, data);

	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_CONFIG_2 + idx,
		vdc->rc_target_rate_threshold);

	data = 0;
	data |= (vdc->rc_tar_rate_scale << 24);
	data |= (vdc->rc_buffer_fullness_scale << 16);
	data |= vdc->rc_fullness_offset_thresh;

	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_CONFIG_3 + idx, data);

	data = 0;
	data |= (vdc->rc_fullness_offset_slope << 8);
	data |= RC_TARGET_RATE_EXTRA_FTBLS;

	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_CONFIG_4 + idx, data);

	data = 0;
	data |= (vdc->flatqp_vf_fbls << 24);
	data |= (vdc->flatqp_vf_nbls << 16);
	data |= (vdc->flatqp_sw_fbls << 8);
	data |= vdc->flatqp_sw_nbls;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_FLAT_CONFIG + idx, data);

	data = 0;
	data |= (vdc->flatness_qp_lut[0] << 24);
	data |= (vdc->flatness_qp_lut[1] << 16);
	data |= (vdc->flatness_qp_lut[2] << 8);
	data |= vdc->flatness_qp_lut[3];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_FLAT_LUT_3_0 + idx, data);

	data = 0;
	data |= (vdc->flatness_qp_lut[4] << 24);
	data |= (vdc->flatness_qp_lut[5] << 16);
	data |= (vdc->flatness_qp_lut[6] << 8);
	data |= vdc->flatness_qp_lut[7];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_FLAT_LUT_7_4 + idx, data);

	data = 0;
	data |= (vdc->max_qp_lut[0] << 24);
	data |= (vdc->max_qp_lut[1] << 16);
	data |= (vdc->max_qp_lut[2] << 8);
	data |= vdc->max_qp_lut[3];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_MAX_QP_LUT_3_0 + idx, data);

	data = 0;
	data |= (vdc->max_qp_lut[4] << 24);
	data |= (vdc->max_qp_lut[5] << 16);
	data |= (vdc->max_qp_lut[6] << 8);
	data |= vdc->max_qp_lut[7];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_MAX_QP_LUT_7_4 + idx, data);

	data = 0;
	data |= (vdc->tar_del_lut[0] << 24);
	data |= (vdc->tar_del_lut[1] << 16);
	data |= (vdc->tar_del_lut[2] << 8);
	data |= vdc->tar_del_lut[3];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_TAR_RATE_LUT_3_0 + idx, data);

	data = 0;
	data |= (vdc->tar_del_lut[4] << 24);
	data |= (vdc->tar_del_lut[5] << 16);
	data |= (vdc->tar_del_lut[6] << 8);
	data |= vdc->tar_del_lut[7];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_TAR_RATE_LUT_7_4 + idx, data);

	data = 0;
	data |= (vdc->tar_del_lut[8] << 24);
	data |= (vdc->tar_del_lut[9] << 16);
	data |= (vdc->tar_del_lut[10] << 8);
	data |= vdc->tar_del_lut[11];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_TAR_RATE_LUT_11_8 + idx, data);

	data = 0;
	data |= (vdc->tar_del_lut[12] << 24);
	data |= (vdc->tar_del_lut[13] << 16);
	data |= (vdc->tar_del_lut[14] << 8);
	data |= vdc->tar_del_lut[15];
	SDE_REG_WRITE(vdc_reg, ENC_VDC_TAR_RATE_LUT_15_12 + idx, data);

	data = 0;
	data |= (vdc->mppf_bpc_r_y << 20);
	data |= (vdc->mppf_bpc_g_cb << 16);
	data |= (vdc->mppf_bpc_b_cr << 12);
	data |= (vdc->mppf_bpc_y << 8);
	data |= (vdc->mppf_bpc_co << 4);
	data |= vdc->mppf_bpc_cg;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_MPPF_CONFIG + idx, data);

	SDE_REG_WRITE(vdc_reg, ENC_VDC_SSM_CONFIG + idx,
			SSM_MAX_SE_SIZE);

	slice_num_bits_ldw = (u32)vdc->slice_num_bits;
	slice_num_bits_ub = vdc->slice_num_bits >> 32;

	SDE_REG_WRITE(vdc_reg, ENC_VDC_SLICE_NUM_BITS_0 + idx,
			(slice_num_bits_ub & 0x0ff));

	SDE_REG_WRITE(vdc_reg, ENC_VDC_SLICE_NUM_BITS_1 + idx,
			slice_num_bits_ldw);

	data = 0;
	data |= (vdc->chunk_adj_bits << 16);
	data |= vdc->num_extra_mux_bits;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_RC_PRECOMPUTE + idx, data);

	for (i = 0; i < VDC_LBDA_BRATE_REG_SIZE; i += 2) {
		data = 0;
		data |= (vdc->lbda_brate_lut_interp[i] << 16);
		data |= vdc->lbda_brate_lut_interp[i + 1];
		SDE_REG_WRITE(vdc_reg,
			ENC_VDC_LBDA_BRATE_LUT + idx +
			(addr_off * 4),
			data);
		addr_off++;
	}

	for (i = 0; i < VDC_LBDA_BRATE_REG_SIZE; i += 4) {
		data = 0;
		data |= (vdc->lbda_bf_lut_interp[i] << 24);
		data |= (vdc->lbda_bf_lut_interp[i + 1] << 16);
		data |= (vdc->lbda_bf_lut_interp[i + 2] << 8);
		data |= vdc->lbda_bf_lut_interp[i + 3];
		SDE_REG_WRITE(vdc_reg, ENC_VDC_LBDA_BF_LUT + idx + i,
				data);
	}

	data = 0;
	data |= (vdc->min_block_bits << 16);
	data |= vdc->rc_lambda_bitrate_scale;
	SDE_REG_WRITE(vdc_reg, ENC_VDC_OTHER_RC + idx,
			data);
	/* program the vdc wrapper */
	if (_vdc_subblk_offset(hw_vdc, SDE_VDC_CTL, &idx))
		return;

	data = 0;
	data = BIT(0); /* encoder enable */
	if (vdc->bits_per_component == 8)
		data |= BIT(11);
	if (vdc->chroma_format == MSM_CHROMA_422) {
		data |= BIT(8);
		data |= BIT(10);
	}

	SDE_REG_WRITE(vdc_reg, VDC_CFG + idx, data);
}

static void sde_hw_vdc_bind_pingpong_blk(
		struct sde_hw_vdc *hw_vdc,
		bool enable,
		const enum sde_pingpong pp)
{
	struct sde_hw_blk_reg_map *vdc_reg;
	int idx;
	int mux_cfg = 0xF; /* Disabled */

	if (!hw_vdc)
		return;

	if (_vdc_subblk_offset(hw_vdc, SDE_VDC_CTL, &idx))
		return;

	vdc_reg = &hw_vdc->hw;
	if (enable)
		mux_cfg = (pp - PINGPONG_0) & 0xf;

	SDE_REG_WRITE(vdc_reg, VDC_CTL + idx, mux_cfg);
}

static struct sde_vdc_cfg *_vdc_offset(enum sde_vdc vdc,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->vdc_count; i++) {
		if (vdc == m->vdc[i].id) {
			b->base_off = addr;
			b->blk_off = m->vdc[i].base;
			b->length = m->vdc[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_VDC;
			return &m->vdc[i];
		}
	}

	return NULL;
}

static void _setup_vdc_ops(struct sde_hw_vdc_ops *ops,
		unsigned long features)
{
	ops->vdc_disable = sde_hw_vdc_disable;
	ops->vdc_config = sde_hw_vdc_config;
	ops->bind_pingpong_blk = sde_hw_vdc_bind_pingpong_blk;
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_vdc *sde_hw_vdc_init(enum sde_vdc idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_vdc *c;
	struct sde_vdc_cfg *cfg;
	int rc;
	u32 vdc_ctl_reg;
	char blk_name[32];

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _vdc_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;

	_setup_vdc_ops(&c->ops, c->caps->features);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_VDC, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	if (_vdc_subblk_offset(c, SDE_VDC_CTL, &vdc_ctl_reg)) {
		SDE_ERROR("vdc ctl not found\n");
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
		c->hw.blk_off + c->hw.length, c->hw.xin_id);

	snprintf(blk_name, sizeof(blk_name), "vdc_enc_%u",
			c->idx - VDC_0);

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME,
			blk_name,
			c->hw.blk_off + c->caps->sblk->enc.base,
			c->hw.blk_off + c->caps->sblk->enc.base +
			c->caps->sblk->enc.len,
			c->hw.xin_id);

	snprintf(blk_name, sizeof(blk_name), "vdc_ctl_%u",
			c->idx - VDC_0);

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME,
			blk_name,
			c->hw.blk_off + c->caps->sblk->ctl.base,
			c->hw.blk_off + c->caps->sblk->ctl.base +
			c->caps->sblk->ctl.len,
			c->hw.xin_id);

	return c;

blk_init_error:
	kfree(c);

	return ERR_PTR(rc);
}

void sde_hw_vdc_destroy(struct sde_hw_vdc *vdc)
{
	if (vdc) {
		sde_hw_blk_destroy(&vdc->base);
		kfree(vdc);
	}
}
