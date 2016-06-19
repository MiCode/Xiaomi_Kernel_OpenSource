/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/mutex.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_trace.h"
#include "mdss_debug.h"

static u32 cdm_cdwn2_cosite_h_coeff[] = {0x00000016, 0x000001cc, 0x0100009e};
static u32 cdm_cdwn2_offsite_h_coeff[] = {0x000b0005, 0x01db01eb, 0x00e40046};
static u32 cdm_cdwn2_cosite_v_coeff[] = {0x00080004};
static u32 cdm_cdwn2_offsite_v_coeff[] = {0x00060002};

#define VSYNC_TIMEOUT_US 16000

/**
 * @mdss_mdp_cdm_alloc() - Allocates a cdm block by parsing the list of
 *			     available cdm blocks.
 *
 * @mdata - structure containing the list of cdm blocks
 */
static struct mdss_mdp_cdm *mdss_mdp_cdm_alloc(struct mdss_data_type *mdata)
{
	struct mdss_mdp_cdm *cdm = NULL;
	u32 i = 0;

	mutex_lock(&mdata->cdm_lock);

	for (i = 0; i < mdata->ncdm; i++) {
		cdm = mdata->cdm_off + i;
		if (atomic_read(&cdm->kref.refcount) == 0) {
			kref_init(&cdm->kref);
			cdm->mdata = mdata;
			pr_debug("alloc cdm=%d\n", cdm->num);
			break;
		}
		cdm = NULL;
	}

	mutex_unlock(&mdata->cdm_lock);

	return cdm;
}

/**
 *  @mdss_mdp_cdm_free() - Adds the CDM block back to the available list
 *  @kref: Reference count structure
 */
static void mdss_mdp_cdm_free(struct kref *kref)
{
	struct mdss_mdp_cdm *cdm = container_of(kref, struct mdss_mdp_cdm,
						 kref);
	if (!cdm)
		return;

	complete_all(&cdm->free_comp);
	pr_debug("free cdm_num = %d\n", cdm->num);

}

/**
 * @mdss_mdp_cdm_init() - Allocates a CDM block and initializes the hardware
 *			  and software context. This should be called once at
 *			  when setting up the usecase and released when done.
 * @ctl:		 Pointer to the control structure.
 * @intf_type:		 Output interface which will be connected to CDM.
 */
struct mdss_mdp_cdm *mdss_mdp_cdm_init(struct mdss_mdp_ctl *ctl, u32 intf_type)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_cdm *cdm = NULL;

	cdm = mdss_mdp_cdm_alloc(mdata);

	/**
	 * give hdmi interface priority to alloc the cdm block. It will wait
	 * for one vsync cycle to allow wfd to finish its job and try to reserve
	 * the block the again.
	 */
	if (!cdm && (intf_type == MDP_CDM_CDWN_OUTPUT_HDMI)) {
		/* always wait for first cdm block */
		cdm = mdata->cdm_off;
		if (cdm) {
			reinit_completion(&cdm->free_comp);
			/*
			 * no need to check the return status of completion
			 * timeout. Next cdm_alloc call will try to reserve
			 * the cdm block and returns failure if allocation
			 * fails.
			 */
			wait_for_completion_timeout(&cdm->free_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
			cdm = mdss_mdp_cdm_alloc(mdata);
		}
	}

	if (!cdm) {
		pr_err("%s: Unable to allocate cdm\n", __func__);
		return ERR_PTR(-EBUSY);
	}

	cdm->out_intf = intf_type;
	cdm->is_bypassed = true;
	memset(&cdm->setup, 0x0, sizeof(struct mdp_cdm_cfg));

	return cdm;
}

/**
 * @mdss_mdp_cdm_csc_setup - Programs the CSC block.
 * @cdm:		     Pointer to the CDM structure.
 * @data:                    Pointer to the structure containing configuration
 *			     data.
 */
static int mdss_mdp_cdm_csc_setup(struct mdss_mdp_cdm *cdm,
				  struct mdp_cdm_cfg *data)
{
	int rc = 0;
	u32 op_mode = 0;

	mdss_mdp_csc_setup(MDSS_MDP_BLOCK_CDM, cdm->num, data->csc_type);

	if ((data->csc_type == MDSS_MDP_CSC_RGB2YUV_601L) ||
		(data->csc_type == MDSS_MDP_CSC_RGB2YUV_601FR) ||
		(data->csc_type == MDSS_MDP_CSC_RGB2YUV_709L)) {
		op_mode |= BIT(2);  /* DST_DATA_FORMAT = YUV */
		op_mode &= ~BIT(1); /* SRC_DATA_FORMAT = RGB */
		op_mode |= BIT(0);  /* EN = 1 */
		cdm->is_bypassed = false;
	} else {
		op_mode = 0;
		cdm->is_bypassed = true;
	}

	writel_relaxed(op_mode, cdm->base + MDSS_MDP_REG_CDM_CSC_10_OPMODE);

	return rc;
}

/**
 * @mdss_mdp_cdm_cdwn_setup - Programs the chroma down block.
 * @cdm:		      Pointer to the CDM structure.
 * @data:                     Pointer to the structure containing configuration
 *			      data.
 */
static int mdss_mdp_cdm_cdwn_setup(struct mdss_mdp_cdm *cdm,
			       struct mdp_cdm_cfg *data)
{
	int rc = 0;
	u32 opmode = 0;
	u32 out_size = 0;
	if (data->mdp_csc_bit_depth == MDP_CDM_CSC_10BIT)
		opmode &= ~BIT(7);
	else
		opmode |= BIT(7);

	/* ENABLE DWNS_H bit */
	opmode |= BIT(1);

	switch (data->horz_downsampling_type) {
	case MDP_CDM_CDWN_DISABLE:
		/* CLEAR METHOD_H field */
		opmode &= ~(0x18);
		/* CLEAR DWNS_H bit */
		opmode &= ~BIT(1);
		break;
	case MDP_CDM_CDWN_PIXEL_DROP:
		/* Clear METHOD_H field (pixel drop is 0) */
		opmode &= ~(0x18);
		break;
	case MDP_CDM_CDWN_AVG:
		/* Clear METHOD_H field (Average is 0x1) */
		opmode &= ~(0x18);
		opmode |= (0x1 << 0x3);
		break;
	case MDP_CDM_CDWN_COSITE:
		/* Clear METHOD_H field (Average is 0x2) */
		opmode &= ~(0x18);
		opmode |= (0x2 << 0x3);
		/* Co-site horizontal coefficients */
		writel_relaxed(cdm_cdwn2_cosite_h_coeff[0], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_COSITE_H_0);
		writel_relaxed(cdm_cdwn2_cosite_h_coeff[1], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_COSITE_H_1);
		writel_relaxed(cdm_cdwn2_cosite_h_coeff[2], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_COSITE_H_2);
		break;
	case MDP_CDM_CDWN_OFFSITE:
		/* Clear METHOD_H field (Average is 0x3) */
		opmode &= ~(0x18);
		opmode |= (0x3 << 0x3);

		/* Off-site horizontal coefficients */
		writel_relaxed(cdm_cdwn2_offsite_h_coeff[0], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_OFFSITE_H_0);
		writel_relaxed(cdm_cdwn2_offsite_h_coeff[1], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_OFFSITE_H_1);
		writel_relaxed(cdm_cdwn2_offsite_h_coeff[2], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_OFFSITE_H_2);
		break;
	default:
		pr_err("%s invalid horz down sampling type\n", __func__);
		return -EINVAL;
	}

	/* ENABLE DWNS_V bit */
	opmode |= BIT(2);

	switch (data->vert_downsampling_type) {
	case MDP_CDM_CDWN_DISABLE:
		/* CLEAR METHOD_V field */
		opmode &= ~(0x60);
		/* CLEAR DWNS_V bit */
		opmode &= ~BIT(2);
		break;
	case MDP_CDM_CDWN_PIXEL_DROP:
		/* Clear METHOD_V field (pixel drop is 0) */
		opmode &= ~(0x60);
		break;
	case MDP_CDM_CDWN_AVG:
		/* Clear METHOD_V field (Average is 0x1) */
		opmode &= ~(0x60);
		opmode |= (0x1 << 0x5);
		break;
	case MDP_CDM_CDWN_COSITE:
		/* Clear METHOD_V field (Average is 0x2) */
		opmode &= ~(0x60);
		opmode |= (0x2 << 0x5);
		/* Co-site vertical coefficients */
		writel_relaxed(cdm_cdwn2_cosite_v_coeff[0], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_COSITE_V);
		break;
	case MDP_CDM_CDWN_OFFSITE:
		/* Clear METHOD_V field (Average is 0x3) */
		opmode &= ~(0x60);
		opmode |= (0x3 << 0x5);

		/* Off-site vertical coefficients */
		writel_relaxed(cdm_cdwn2_offsite_v_coeff[0], cdm->base +
			       MDSS_MDP_REG_CDM_CDWN2_COEFF_OFFSITE_V);
		break;
	default:
		pr_err("%s invalid vert down sampling type\n", __func__);
		return -EINVAL;
	}

	if (data->vert_downsampling_type || data->horz_downsampling_type)
		opmode |= BIT(0); /* EN CDWN module */
	else
		opmode &= ~BIT(0);

	out_size = (data->output_width & 0xFFFF) |
			((data->output_height & 0xFFFF) << 16);
	writel_relaxed(out_size, cdm->base + MDSS_MDP_REG_CDM_CDWN2_OUT_SIZE);
	writel_relaxed(opmode, cdm->base + MDSS_MDP_REG_CDM_CDWN2_OP_MODE);
	writel_relaxed(((0x3FF << 16) | 0x0),
		       cdm->base + MDSS_MDP_REG_CDM_CDWN2_CLAMP_OUT);
	return rc;

}

/**
 * @mdss_mdp_cdm_out_packer_setup - Programs the output packer block.
 * @cdm:			    Pointer to the CDM structure.
 * @data:			    Pointer to the structure containing
 *				    configuration data.
 */
static int mdss_mdp_cdm_out_packer_setup(struct mdss_mdp_cdm *cdm,
					 struct mdp_cdm_cfg *data)
{
	int rc = 0;
	u32 opmode = 0;
	u32 cdm_enable = 0;
	struct mdss_mdp_format_params *fmt;

	if (cdm->out_intf == MDP_CDM_CDWN_OUTPUT_HDMI) {
		/* Enable HDMI packer */
		opmode |= BIT(0);
		fmt = mdss_mdp_get_format_params(data->out_format);
		if (!fmt) {
			pr_err("cdm format = %d, not supported\n",
			       data->out_format);
			return -EINVAL;
		}
		opmode &= ~0x6;
		opmode |= (fmt->chroma_sample << 1);
		if (!cdm->is_bypassed)
			cdm_enable |= BIT(19);

	} else {
		/* Disable HDMI pacler for WB */
		opmode = 0;
		if (!cdm->is_bypassed)
			cdm_enable |= BIT(24);
	}
	writel_relaxed(cdm_enable, cdm->mdata->mdp_base +
					MDSS_MDP_MDP_OUT_CTL_0);
	writel_relaxed(opmode, cdm->base + MDSS_MDP_REG_CDM_HDMI_PACK_OP_MODE);

	return rc;
}

/**
 * @mdss_mdp_cdm_setup - Sets up the CDM block based on the usecase. The CDM
 *			 block should be initialized before calling this
 *			 function.
 * @cdm:	         Pointer to the CDM structure.
 * @data:                Pointer to the structure containing configuration
 *			 data.
 */
int mdss_mdp_cdm_setup(struct mdss_mdp_cdm *cdm, struct mdp_cdm_cfg *data)
{
	int rc = 0;

	if (!cdm || !data) {
		pr_err("%s: invalid arguments\n", __func__);
		return -EINVAL;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mutex_lock(&cdm->lock);
	/* Setup CSC block */
	rc = mdss_mdp_cdm_csc_setup(cdm, data);
	if (rc) {
		pr_err("%s: csc configuration failure\n", __func__);
		goto fail;
	}

	/* Setup chroma down sampler */
	rc = mdss_mdp_cdm_cdwn_setup(cdm, data);
	if (rc) {
		pr_err("%s: cdwn configuration failure\n", __func__);
		goto fail;
	}

	/* Setup HDMI packer */
	rc = mdss_mdp_cdm_out_packer_setup(cdm, data);
	if (rc) {
		pr_err("%s: out packer configuration failure\n", __func__);
		goto fail;
	}

	memcpy(&cdm->setup, data, sizeof(struct mdp_cdm_cfg));

fail:
	mutex_unlock(&cdm->lock);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	return rc;
}

/**
 * @mdss_mdp_cdm_destroy - Destroys the CDM configuration and return it to
 *			   default state.
 * @cdm:                   Pointer to the CDM structure
 */
int mdss_mdp_cdm_destroy(struct mdss_mdp_cdm *cdm)
{
	int rc = 0;

	if (!cdm) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	kref_put(&cdm->kref, mdss_mdp_cdm_free);

	return rc;
}
