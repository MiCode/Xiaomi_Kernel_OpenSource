/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "mdss-dsi-clk:[%s] " fmt, __func__
#include <linux/clk/msm-clk.h>
#include <linux/clk.h>
#include <linux/list.h>

#include "mdss_dsi_clk.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"

#define MAX_CLIENT_NAME_LEN 20
struct dsi_core_clks {
	struct mdss_dsi_core_clk_info clks;
	u32 current_clk_state;
};

struct dsi_link_clks {
	struct mdss_dsi_link_hs_clk_info hs_clks;
	struct mdss_dsi_link_lp_clk_info lp_clks;
	u32 current_clk_state;
};

struct mdss_dsi_clk_mngr {
	char name[DSI_CLK_NAME_LEN];
	struct dsi_core_clks core_clks;
	struct dsi_link_clks link_clks;

	struct reg_bus_client *reg_bus_clt;

	pre_clockoff_cb pre_clkoff_cb;
	post_clockoff_cb post_clkoff_cb;
	post_clockon_cb post_clkon_cb;
	pre_clockon_cb pre_clkon_cb;

	struct list_head client_list;
	struct mutex clk_mutex;

	void *priv_data;
};

struct mdss_dsi_clk_client_info {
	char name[MAX_CLIENT_NAME_LEN];
	u32 core_refcount;
	u32 link_refcount;
	u32 core_clk_state;
	u32 link_clk_state;

	struct list_head list;

	struct mdss_dsi_clk_mngr *mngr;
};

static int dsi_core_clk_start(struct dsi_core_clks *c_clks)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr;

	mngr = container_of(c_clks, struct mdss_dsi_clk_mngr, core_clks);

	rc = clk_prepare_enable(c_clks->clks.mdp_core_clk);
	if (rc) {
		pr_err("failed to enable mdp_core_clock. rc=%d\n", rc);
		goto error;
	}

	rc = clk_prepare_enable(c_clks->clks.ahb_clk);
	if (rc) {
		pr_err("failed to enable ahb clock. rc=%d\n", rc);
		goto disable_core_clk;
	}

	rc = clk_prepare_enable(c_clks->clks.axi_clk);
	if (rc) {
		pr_err("failed to enable ahb clock. rc=%d\n", rc);
		goto disable_ahb_clk;
	}

	if (c_clks->clks.mmss_misc_ahb_clk) {
		rc = clk_prepare_enable(c_clks->clks.mmss_misc_ahb_clk);
		if (rc) {
			pr_err("failed to enable mmss misc ahb clk.rc=%d\n",
				rc);
			goto disable_axi_clk;
		}
	}

	rc = mdss_update_reg_bus_vote(mngr->reg_bus_clt, VOTE_INDEX_LOW);
	if (rc) {
		pr_err("failed to vote for reg bus\n");
		goto disable_mmss_misc_clk;
	}

	pr_debug("%s:CORE CLOCK IS ON\n", mngr->name);
	return rc;

disable_mmss_misc_clk:
	if (c_clks->clks.mmss_misc_ahb_clk)
		clk_disable_unprepare(c_clks->clks.mmss_misc_ahb_clk);
disable_axi_clk:
	clk_disable_unprepare(c_clks->clks.axi_clk);
disable_ahb_clk:
	clk_disable_unprepare(c_clks->clks.ahb_clk);
disable_core_clk:
	clk_disable_unprepare(c_clks->clks.mdp_core_clk);
error:
	pr_debug("%s: EXIT, rc = %d\n", mngr->name, rc);
	return rc;
}

static int dsi_core_clk_stop(struct dsi_core_clks *c_clks)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr;

	mngr = container_of(c_clks, struct mdss_dsi_clk_mngr, core_clks);

	mdss_update_reg_bus_vote(mngr->reg_bus_clt, VOTE_INDEX_DISABLE);
	if (c_clks->clks.mmss_misc_ahb_clk)
		clk_disable_unprepare(c_clks->clks.mmss_misc_ahb_clk);
	clk_disable_unprepare(c_clks->clks.axi_clk);
	clk_disable_unprepare(c_clks->clks.ahb_clk);
	clk_disable_unprepare(c_clks->clks.mdp_core_clk);

	pr_debug("%s: CORE CLOCK IS OFF\n", mngr->name);
	return rc;
}

static int dsi_link_hs_clk_set_rate(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr;
	struct dsi_link_clks *l_clks;
	struct mdss_dsi_ctrl_pdata *ctrl;

	l_clks = container_of(link_hs_clks, struct dsi_link_clks, hs_clks);
	mngr = container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);

	/*
	 * In an ideal world, cont_splash_enabled should not be required inside
	 * the clock manager. But, in the current driver cont_splash_enabled
	 * flag is set inside mdp driver and there is no interface event
	 * associated with this flag setting. Also, set rate for clock need not
	 * be called for every enable call. It should be done only once when
	 * coming out of suspend.
	 */
	ctrl = mngr->priv_data;
	if (ctrl->panel_data.panel_info.cont_splash_enabled)
		return 0;

	rc = clk_set_rate(link_hs_clks->byte_clk, link_hs_clks->byte_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for byte_clk rc = %d\n", rc);
		goto error;
	}

	rc = clk_set_rate(link_hs_clks->pixel_clk, link_hs_clks->pix_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for pixel_clk rc = %d\n", rc);
		goto error;
	}

error:
	return rc;
}

static int dsi_link_hs_clk_prepare(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	rc = clk_prepare(link_hs_clks->byte_clk);
	if (rc) {
		pr_err("Failed to prepare dsi byte clk\n");
		goto byte_clk_err;
	}

	rc = clk_prepare(link_hs_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to prepare dsi pixel_clk\n");
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_unprepare(link_hs_clks->byte_clk);
byte_clk_err:
	return rc;
}

static int dsi_link_hs_clk_unprepare(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	clk_unprepare(link_hs_clks->pixel_clk);
	clk_unprepare(link_hs_clks->byte_clk);

	return rc;
}

static int dsi_link_hs_clk_enable(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	rc = clk_enable(link_hs_clks->byte_clk);
	if (rc) {
		pr_err("Failed to enable dsi byte clk\n");
		goto byte_clk_err;
	}

	rc = clk_enable(link_hs_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to enable dsi pixel_clk\n");
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_disable(link_hs_clks->byte_clk);
byte_clk_err:
	return rc;
}

static int dsi_link_hs_clk_disable(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	clk_disable(link_hs_clks->pixel_clk);
	clk_disable(link_hs_clks->byte_clk);

	return rc;
}


static int dsi_link_hs_clk_start(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks,
	enum mdss_dsi_link_clk_op_type op_type)
{
	int rc = 0;
	struct dsi_link_clks *l_clks;
	struct mdss_dsi_clk_mngr *mngr;

	l_clks = container_of(link_hs_clks, struct dsi_link_clks, hs_clks);
	mngr = container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);

	if (op_type & MDSS_DSI_LINK_CLK_SET_RATE) {
		rc = dsi_link_hs_clk_set_rate(link_hs_clks);
		if (rc) {
			pr_err("failed to set HS clk rates, rc = %d\n", rc);
			goto error;
		}
	}

	if (op_type & MDSS_DSI_LINK_CLK_PREPARE) {
		rc = dsi_link_hs_clk_prepare(link_hs_clks);
		if (rc) {
			pr_err("failed to prepare link HS clks, rc = %d\n", rc);
			goto error;
		}
	}

	if (op_type & MDSS_DSI_LINK_CLK_ENABLE) {
		rc = dsi_link_hs_clk_enable(link_hs_clks);
		if (rc) {
			pr_err("failed to enable link HS clks, rc = %d\n", rc);
			goto error_unprepare;
		}
	}

	pr_debug("%s: LINK HS CLOCK IS ON\n", mngr->name);
	return rc;
error_unprepare:
	dsi_link_hs_clk_unprepare(link_hs_clks);
error:
	return rc;
}

static int dsi_link_lp_clk_start(
	struct mdss_dsi_link_lp_clk_info *link_lp_clks)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr;
	struct dsi_link_clks *l_clks;
	struct mdss_dsi_ctrl_pdata *ctrl;

	l_clks = container_of(link_lp_clks, struct dsi_link_clks, lp_clks);
	mngr = container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);
	/*
	 * In an ideal world, cont_splash_enabled should not be required inside
	 * the clock manager. But, in the current driver cont_splash_enabled
	 * flag is set inside mdp driver and there is no interface event
	 * associated with this flag setting. Also, set rate for clock need not
	 * be called for every enable call. It should be done only once when
	 * coming out of suspend.
	 */
	ctrl = mngr->priv_data;
	if (ctrl->panel_data.panel_info.cont_splash_enabled)
		goto prepare;

	rc = clk_set_rate(link_lp_clks->esc_clk, link_lp_clks->esc_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for esc_clk rc = %d\n", rc);
		goto error;
	}

prepare:
	rc = clk_prepare(link_lp_clks->esc_clk);
	if (rc) {
		pr_err("Failed to prepare dsi esc clk\n");
		goto error;
	}

	rc = clk_enable(link_lp_clks->esc_clk);
	if (rc) {
		pr_err("Failed to enable dsi esc clk\n");
		clk_unprepare(l_clks->lp_clks.esc_clk);
		goto error;
	}
error:
	pr_debug("%s: LINK LP CLOCK IS ON\n", mngr->name);
	return rc;
}

static int dsi_link_hs_clk_stop(
	struct mdss_dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;
	struct dsi_link_clks *l_clks;
	struct mdss_dsi_clk_mngr *mngr;

	l_clks = container_of(link_hs_clks, struct dsi_link_clks, hs_clks);
	mngr = container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);

	(void)dsi_link_hs_clk_disable(link_hs_clks);

	(void)dsi_link_hs_clk_unprepare(link_hs_clks);
	pr_debug("%s: LINK HS CLOCK IS OFF\n", mngr->name);

	return rc;
}

static int dsi_link_lp_clk_stop(
	struct mdss_dsi_link_lp_clk_info *link_lp_clks)
{
	struct dsi_link_clks *l_clks;
	struct mdss_dsi_clk_mngr *mngr;

	l_clks = container_of(link_lp_clks, struct dsi_link_clks, lp_clks);
	mngr = container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);

	clk_disable(l_clks->lp_clks.esc_clk);
	clk_unprepare(l_clks->lp_clks.esc_clk);

	pr_debug("%s: LINK LP CLOCK IS OFF\n", mngr->name);
	return 0;
}


static int dsi_update_clk_state(struct dsi_core_clks *c_clks, u32 c_state,
				struct dsi_link_clks *l_clks, u32 l_state)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr;
	bool l_c_on = false;

	if (c_clks) {
		mngr =
		container_of(c_clks, struct mdss_dsi_clk_mngr, core_clks);
	} else if (l_clks) {
		mngr =
		container_of(l_clks, struct mdss_dsi_clk_mngr, link_clks);
	} else {
		mngr = NULL;
	}

	if (!mngr)
		return -EINVAL;

	pr_debug("%s: c_state = %d, l_state = %d\n", mngr ? mngr->name : "NA",
		 c_clks ? c_state : -1, l_clks ? l_state : -1);
	/*
	 * Clock toggle order:
	 *	1. When turning on, Core clocks before link clocks
	 *	2. When turning off, Link clocks before core clocks.
	 */
	if (c_clks && (c_state == MDSS_DSI_CLK_ON)) {
		if (c_clks->current_clk_state == MDSS_DSI_CLK_OFF) {
			rc = mngr->pre_clkon_cb(mngr->priv_data,
				MDSS_DSI_CORE_CLK, MDSS_DSI_LINK_NONE,
				MDSS_DSI_CLK_ON);
			if (rc) {
				pr_err("failed to turn on MDP FS rc= %d\n", rc);
				goto error;
			}
		}
		rc = dsi_core_clk_start(c_clks);
		if (rc) {
			pr_err("failed to turn on core clks rc = %d\n", rc);
			goto error;
		}

		if (mngr->post_clkon_cb) {
			rc = mngr->post_clkon_cb(mngr->priv_data,
				 MDSS_DSI_CORE_CLK, MDSS_DSI_LINK_NONE,
				 MDSS_DSI_CLK_ON);
			if (rc)
				pr_err("post clk on cb failed, rc = %d\n", rc);
		}
		c_clks->current_clk_state = MDSS_DSI_CLK_ON;
	}

	if (l_clks) {

		if (l_state == MDSS_DSI_CLK_ON) {
			if (mngr->pre_clkon_cb) {
				rc = mngr->pre_clkon_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_LP_CLK,
					l_state);
				if (rc)
					pr_err("pre link LP clk on cb failed\n");
			}
			rc = dsi_link_lp_clk_start(&l_clks->lp_clks);
			if (rc) {
				pr_err("failed to start LP link clk clk\n");
				goto error;
			}
			if (mngr->post_clkon_cb) {
				rc = mngr->post_clkon_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_LP_CLK,
					l_state);
				if (rc)
					pr_err("post LP clk on cb failed\n");
			}

			if (mngr->pre_clkon_cb) {
				rc = mngr->pre_clkon_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_HS_CLK,
					l_state);
				if (rc)
					pr_err("pre HS clk on cb failed\n");
			}
			rc = dsi_link_hs_clk_start(&l_clks->hs_clks,
				(MDSS_DSI_LINK_CLK_SET_RATE |
				MDSS_DSI_LINK_CLK_PREPARE));
			if (rc) {
				pr_err("failed to prepare HS clk rc= %d\n", rc);
				goto error;
			}
			if (mngr->post_clkon_cb) {
				rc = mngr->post_clkon_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_HS_CLK,
					l_state);
				if (rc)
					pr_err("post HS clk on cb failed\n");
			}
			rc = dsi_link_hs_clk_start(&l_clks->hs_clks,
				MDSS_DSI_LINK_CLK_ENABLE);
			if (rc) {
				pr_err("failed to enable HS clk rc= %d\n", rc);
				goto error;
			}
		} else {
			/*
			 * Two conditions that need to be checked for Link
			 * clocks:
			 * 1. Link clocks need core clocks to be on when
			 *    transitioning from EARLY_GATE to OFF state.
			 * 2. ULPS mode might have to be enabled in case of OFF
			 *    state. For ULPS, Link clocks should be turned ON
			 *    first before they are turned off again.
			 *
			 * If Link is going from EARLY_GATE to OFF state AND
			 * Core clock is already in EARLY_GATE or OFF state,
			 * turn on Core clocks and link clocks.
			 *
			 * ULPS state is managed as part of the pre_clkoff_cb.
			 */
			if ((l_state == MDSS_DSI_CLK_OFF) &&
			    (l_clks->current_clk_state ==
			    MDSS_DSI_CLK_EARLY_GATE) &&
			    (mngr->core_clks.current_clk_state !=
			    MDSS_DSI_CLK_ON)) {
				rc = dsi_core_clk_start(&mngr->core_clks);
				if (rc) {
					pr_err("core clks did not start\n");
					goto error;
				}

				rc = dsi_link_lp_clk_start(&l_clks->lp_clks);
				if (rc) {
					pr_err("LP Link clks did not start\n");
					goto error;
				}

				rc = dsi_link_hs_clk_start(&l_clks->hs_clks,
						MDSS_DSI_LINK_CLK_START);
				if (rc) {
					pr_err("HS Link clks did not start\n");
					goto error;
				}
				l_c_on = true;
				pr_debug("ECG: core and Link_on\n");
			}

			if (mngr->pre_clkoff_cb) {
				rc = mngr->pre_clkoff_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_HS_CLK,
					l_state);
				if (rc)
					pr_err("pre HS clk off cb failed\n");
			}

			rc = dsi_link_hs_clk_stop(&l_clks->hs_clks);
			if (rc) {
				pr_err("failed to stop HS clk, rc = %d\n",
				       rc);
				goto error;
			}

			if (mngr->post_clkoff_cb) {
				rc = mngr->post_clkoff_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_HS_CLK,
					l_state);
				if (rc)
					pr_err("post HS clk off cb failed\n");
			}

			if (mngr->pre_clkoff_cb) {
				rc = mngr->pre_clkoff_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_LP_CLK,
					l_state);
				if (rc)
					pr_err("pre LP clk off cb failed\n");
			}

			rc = dsi_link_lp_clk_stop(&l_clks->lp_clks);
			if (rc) {
				pr_err("failed to stop LP link clk, rc = %d\n",
				       rc);
				goto error;
			}

			if (mngr->post_clkoff_cb) {
				rc = mngr->post_clkoff_cb(mngr->priv_data,
					MDSS_DSI_LINK_CLK, MDSS_DSI_LINK_LP_CLK,
					l_state);
				if (rc)
					pr_err("post LP clk off cb failed\n");
			}

			/*
			 * This check is to save unnecessary clock state
			 * change when going from EARLY_GATE to OFF. In the
			 * case where the request happens for both Core and Link
			 * clocks in the same call, core clocks need to be
			 * turned on first before OFF state can be entered.
			 *
			 * Core clocks are turned on here for Link clocks to go
			 * to OFF state. If core clock request is also present,
			 * then core clocks can be turned off Core clocks are
			 * transitioned to OFF state.
			 */
			if (l_c_on && (!(c_clks && (c_state == MDSS_DSI_CLK_OFF)
					 && (c_clks->current_clk_state ==
					     MDSS_DSI_CLK_EARLY_GATE)))) {
				rc = dsi_core_clk_stop(&mngr->core_clks);
				if (rc) {
					pr_err("core clks did not stop\n");
					goto error;
				}

				l_c_on = false;
				pr_debug("ECG: core off\n");
			} else
				pr_debug("ECG: core off skip\n");
		}

		l_clks->current_clk_state = l_state;
	}

	if (c_clks && (c_state != MDSS_DSI_CLK_ON)) {

		/*
		 * When going to OFF state from EARLY GATE state, Core clocks
		 * should be turned on first so that the IOs can be clamped.
		 * l_c_on flag is set, then the core clocks were turned before
		 * to the Link clocks go to OFF state. So Core clocks are
		 * already ON and this step can be skipped.
		 *
		 * IOs are clamped in pre_clkoff_cb callback.
		 */
		if ((c_state == MDSS_DSI_CLK_OFF) &&
		    (c_clks->current_clk_state ==
		    MDSS_DSI_CLK_EARLY_GATE) && !l_c_on) {
			rc = dsi_core_clk_start(&mngr->core_clks);
			if (rc) {
				pr_err("core clks did not start\n");
				goto error;
			}
			pr_debug("ECG: core on\n");
		} else
			pr_debug("ECG: core on skip\n");

		if (mngr->pre_clkoff_cb) {
			rc = mngr->pre_clkoff_cb(mngr->priv_data,
				 MDSS_DSI_CORE_CLK, MDSS_DSI_LINK_NONE,
				 c_state);
			if (rc)
				pr_err("pre core clk off cb failed\n");
		}

		rc = dsi_core_clk_stop(c_clks);
		if (rc) {
			pr_err("failed to turn off core clks rc = %d\n", rc);
			goto error;
		}

		if (c_state == MDSS_DSI_CLK_OFF) {
			if (mngr->post_clkoff_cb) {
				rc = mngr->post_clkoff_cb(mngr->priv_data,
					MDSS_DSI_CORE_CLK, MDSS_DSI_LINK_NONE,
						MDSS_DSI_CLK_OFF);
				if (rc)
					pr_err("post clkoff cb fail, rc = %d\n",
					       rc);
			}
		}
		c_clks->current_clk_state = c_state;
	}

error:
	return rc;
}

static int dsi_recheck_clk_state(struct mdss_dsi_clk_mngr *mngr)
{
	int rc = 0;
	struct list_head *pos = NULL;
	struct mdss_dsi_clk_client_info *c;
	u32 new_core_clk_state = MDSS_DSI_CLK_OFF;
	u32 new_link_clk_state = MDSS_DSI_CLK_OFF;
	u32 old_c_clk_state = MDSS_DSI_CLK_OFF;
	u32 old_l_clk_state = MDSS_DSI_CLK_OFF;
	struct dsi_core_clks *c_clks = NULL;
	struct dsi_link_clks *l_clks = NULL;

	/*
	 * Rules to maintain clock state:
	 *	1. If any client is in ON state, clocks should be ON.
	 *	2. If any client is in ECG state with rest of them turned OFF,
	 *	   go to Early gate state.
	 *	3. If all clients are off, then goto OFF state.
	 */
	list_for_each(pos, &mngr->client_list) {
		c = list_entry(pos, struct mdss_dsi_clk_client_info, list);
		if (c->core_clk_state == MDSS_DSI_CLK_ON) {
			new_core_clk_state = MDSS_DSI_CLK_ON;
			break;
		} else if (c->core_clk_state == MDSS_DSI_CLK_EARLY_GATE) {
			new_core_clk_state = MDSS_DSI_CLK_EARLY_GATE;
		}
	}

	list_for_each(pos, &mngr->client_list) {
		c = list_entry(pos, struct mdss_dsi_clk_client_info, list);
		if (c->link_clk_state == MDSS_DSI_CLK_ON) {
			new_link_clk_state = MDSS_DSI_CLK_ON;
			break;
		} else if (c->link_clk_state == MDSS_DSI_CLK_EARLY_GATE) {
			new_link_clk_state = MDSS_DSI_CLK_EARLY_GATE;
		}
	}

	if (new_core_clk_state != mngr->core_clks.current_clk_state)
		c_clks = &mngr->core_clks;

	if (new_link_clk_state != mngr->link_clks.current_clk_state)
		l_clks = &mngr->link_clks;

	old_c_clk_state = mngr->core_clks.current_clk_state;
	old_l_clk_state = mngr->link_clks.current_clk_state;

	pr_debug("%s: c_clk_state (%d -> %d)\n", mngr->name,
		 old_c_clk_state, new_core_clk_state);
	pr_debug("%s: l_clk_state (%d -> %d)\n", mngr->name,
		 old_l_clk_state, new_link_clk_state);

	MDSS_XLOG(old_c_clk_state, new_core_clk_state, old_l_clk_state,
		  new_link_clk_state);
	if (c_clks || l_clks) {
		rc = dsi_update_clk_state(c_clks, new_core_clk_state,
					  l_clks, new_link_clk_state);
		if (rc) {
			pr_err("failed to update clock state, rc = %d\n", rc);
			goto error;
		}
	}

error:
	return rc;
}

static int dsi_set_clk_rate(struct mdss_dsi_clk_mngr *mngr, int clk, u32 rate,
			    u32 flags)
{
	int rc = 0;

	pr_debug("%s: clk = %d, rate = %d, flags = %d\n", mngr->name,
		 clk, rate, flags);

	MDSS_XLOG(clk, rate, flags);
	switch (clk) {
	case MDSS_DSI_LINK_ESC_CLK:
		mngr->link_clks.lp_clks.esc_clk_rate = rate;
		if (!flags) {
			rc = clk_set_rate(mngr->link_clks.lp_clks.esc_clk,
				rate);
			if (rc)
				pr_err("set rate failed for esc clk rc=%d\n",
				       rc);
		}
		break;
	case MDSS_DSI_LINK_BYTE_CLK:
		mngr->link_clks.hs_clks.byte_clk_rate = rate;
		if (!flags) {
			rc = clk_set_rate(mngr->link_clks.hs_clks.byte_clk,
				rate);
			if (rc)
				pr_err("set rate failed for byte clk rc=%d\n",
				       rc);
		}
		break;
	case MDSS_DSI_LINK_PIX_CLK:
		mngr->link_clks.hs_clks.pix_clk_rate = rate;
		if (!flags) {
			rc = clk_set_rate(mngr->link_clks.hs_clks.pixel_clk,
				rate);
			if (rc)
				pr_err("failed to set rate for pix clk rc=%d\n",
				       rc);
		}
		break;
	default:
		pr_err("Unsupported clock (%d)\n", clk);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

void *mdss_dsi_clk_register(void *clk_mngr, struct mdss_dsi_clk_client *client)
{
	void *handle = NULL;
	struct mdss_dsi_clk_mngr *mngr = clk_mngr;
	struct mdss_dsi_clk_client_info *c;

	if (!mngr) {
		pr_err("bad params\n");
		return ERR_PTR(-EINVAL);
	}

	pr_debug("%s: ENTER\n", mngr->name);

	mutex_lock(&mngr->clk_mutex);

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c) {
		handle = ERR_PTR(-ENOMEM);
		goto error;
	}

	strlcpy(c->name, client->client_name, MAX_CLIENT_NAME_LEN);
	c->mngr = mngr;

	list_add(&c->list, &mngr->client_list);

	pr_debug("%s: Added new client (%s)\n", mngr->name, c->name);
	handle = c;
error:
	mutex_unlock(&mngr->clk_mutex);
	pr_debug("%s: EXIT, rc = %ld\n", mngr->name, PTR_ERR(handle));
	return handle;
}

int mdss_dsi_clk_deregister(void *client)
{
	int rc = 0;
	struct mdss_dsi_clk_client_info *c = client;
	struct mdss_dsi_clk_mngr *mngr;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct mdss_dsi_clk_client_info *node;

	if (!client) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mngr = c->mngr;
	pr_debug("%s: ENTER\n", mngr->name);
	mutex_lock(&mngr->clk_mutex);
	c->core_clk_state = MDSS_DSI_CLK_OFF;
	c->link_clk_state = MDSS_DSI_CLK_OFF;

	rc = dsi_recheck_clk_state(mngr);
	if (rc) {
		pr_err("clock state recheck failed rc = %d\n", rc);
		goto error;
	}

	list_for_each_safe(pos, tmp, &mngr->client_list) {
		node = list_entry(pos, struct mdss_dsi_clk_client_info,
				  list);
		if (node == c) {
			list_del(&node->list);
			pr_debug("Removed device (%s)\n", node->name);
			kfree(node);
			break;
		}
	}

error:
	mutex_unlock(&mngr->clk_mutex);
	pr_debug("%s: EXIT, rc = %d\n", mngr->name, rc);
	return rc;
}

bool is_dsi_clk_in_ecg_state(void *client)
{
	struct mdss_dsi_clk_client_info *c = client;
	struct mdss_dsi_clk_mngr *mngr;
	bool is_ecg = false;


	if (!client) {
		pr_err("Invalid client params\n");
		goto end;
	}

	mngr = c->mngr;

	mutex_lock(&mngr->clk_mutex);
	is_ecg = (c->core_clk_state == MDSS_DSI_CLK_EARLY_GATE);
	mutex_unlock(&mngr->clk_mutex);

end:
	return is_ecg;
}

int mdss_dsi_clk_req_state(void *client, enum mdss_dsi_clk_type clk,
	enum mdss_dsi_clk_state state, u32 index)
{
	int rc = 0;
	struct mdss_dsi_clk_client_info *c = client;
	struct mdss_dsi_clk_mngr *mngr;
	bool changed = false;

	if (!client || !clk || clk > (MDSS_DSI_CORE_CLK | MDSS_DSI_LINK_CLK) ||
	    state > MDSS_DSI_CLK_EARLY_GATE) {
		pr_err("Invalid params, client = %pK, clk = 0x%x, state = %d\n",
		       client, clk, state);
		return -EINVAL;
	}

	mngr = c->mngr;
	mutex_lock(&mngr->clk_mutex);

	pr_debug("[%s]%s: CLK=%d, new_state=%d, core=%d, linkl=%d\n",
	       c->name, mngr->name, clk, state, c->core_clk_state,
	       c->link_clk_state);

	MDSS_XLOG(index, clk, state, c->core_clk_state, c->link_clk_state);
	/*
	 * Refcount handling rules:
	 *	1. Increment refcount whenever ON is called
	 *	2. Do not decrement when going from EARLY_GATE to OFF.
	 *	3. Decrement refcount when either OFF or EARLY_GATE is called
	 */
	if (state == MDSS_DSI_CLK_ON) {
		if (clk & MDSS_DSI_CORE_CLK) {
			c->core_refcount++;
			if (c->core_clk_state != MDSS_DSI_CLK_ON) {
				c->core_clk_state = MDSS_DSI_CLK_ON;
				changed = true;
			}
		}
		if (clk & MDSS_DSI_LINK_CLK) {
			c->link_refcount++;
			if (c->link_clk_state != MDSS_DSI_CLK_ON) {
				c->link_clk_state = MDSS_DSI_CLK_ON;
				changed = true;
			}
		}
	} else if ((state == MDSS_DSI_CLK_EARLY_GATE) ||
		   (state == MDSS_DSI_CLK_OFF)) {
		if (clk & MDSS_DSI_CORE_CLK) {
			if (c->core_refcount == 0) {
				if ((c->core_clk_state ==
				    MDSS_DSI_CLK_EARLY_GATE) &&
				    (state == MDSS_DSI_CLK_OFF)) {
					changed = true;
					c->core_clk_state = MDSS_DSI_CLK_OFF;
				} else {
					pr_warn("Core refcount is zero for %s",
						c->name);
				}
			} else {
				c->core_refcount--;
				if (c->core_refcount == 0) {
					c->core_clk_state = state;
					changed = true;
				}
			}
		}
		if (clk & MDSS_DSI_LINK_CLK) {
			if (c->link_refcount == 0) {
				if ((c->link_clk_state ==
				    MDSS_DSI_CLK_EARLY_GATE) &&
				    (state == MDSS_DSI_CLK_OFF)) {
					changed = true;
					c->link_clk_state = MDSS_DSI_CLK_OFF;
				} else {
					pr_warn("Link refcount is zero for %s",
						c->name);
				}
			} else {
				c->link_refcount--;
				if (c->link_refcount == 0) {
					c->link_clk_state = state;
					changed = true;
				}
			}
		}
	}
	pr_debug("[%s]%s: change=%d, Core (ref=%d, state=%d), Link (ref=%d, state=%d)\n",
		 c->name, mngr->name, changed, c->core_refcount,
		 c->core_clk_state, c->link_refcount, c->link_clk_state);
	MDSS_XLOG(index, clk, state, c->core_clk_state, c->link_clk_state);

	if (changed) {
		rc = dsi_recheck_clk_state(mngr);
		if (rc)
			pr_err("Failed to adjust clock state rc = %d\n", rc);
	}

	mutex_unlock(&mngr->clk_mutex);
	return rc;
}

int mdss_dsi_clk_set_link_rate(void *client, enum mdss_dsi_link_clk_type clk,
			       u32 rate, u32 flags)
{
	int rc = 0;
	struct mdss_dsi_clk_client_info *c = client;
	struct mdss_dsi_clk_mngr *mngr;

	if (!client || (clk > MDSS_DSI_LINK_CLK_MAX)) {
		pr_err("Invalid params, client = %pK, clk = 0x%x", client, clk);
		return -EINVAL;
	}

	mngr = c->mngr;
	pr_debug("%s: ENTER\n", mngr->name);
	mutex_lock(&mngr->clk_mutex);

	rc = dsi_set_clk_rate(mngr, clk, rate, flags);
	if (rc)
		pr_err("Failed to set rate for clk %d, rate = %d, rc = %d\n",
		       clk, rate, rc);

	mutex_unlock(&mngr->clk_mutex);
	pr_debug("%s: EXIT, rc = %d\n", mngr->name, rc);
	return rc;
}

void *mdss_dsi_clk_init(struct mdss_dsi_clk_info *info)
{
	struct mdss_dsi_clk_mngr *mngr;

	if (!info) {
		pr_err("Invalid params\n");
		return ERR_PTR(-EINVAL);
	}
	pr_debug("ENTER %s\n", info->name);
	mngr = kzalloc(sizeof(*mngr), GFP_KERNEL);
	if (!mngr) {
		mngr = ERR_PTR(-ENOMEM);
		goto error;
	}

	mutex_init(&mngr->clk_mutex);
	memcpy(&mngr->core_clks.clks, &info->core_clks, sizeof(struct
						 mdss_dsi_core_clk_info));
	memcpy(&mngr->link_clks.hs_clks, &info->link_hs_clks, sizeof(struct
						 mdss_dsi_link_hs_clk_info));
	memcpy(&mngr->link_clks.lp_clks, &info->link_lp_clks, sizeof(struct
						 mdss_dsi_link_lp_clk_info));

	INIT_LIST_HEAD(&mngr->client_list);
	mngr->pre_clkon_cb = info->pre_clkon_cb;
	mngr->post_clkon_cb = info->post_clkon_cb;
	mngr->pre_clkoff_cb = info->pre_clkoff_cb;
	mngr->post_clkoff_cb = info->post_clkoff_cb;
	mngr->priv_data = info->priv_data;
	mngr->reg_bus_clt = mdss_reg_bus_vote_client_create(info->name);
	if (IS_ERR(mngr->reg_bus_clt)) {
		pr_err("Unable to get handle for reg bus vote\n");
		kfree(mngr);
		mngr = ERR_PTR(-EINVAL);
		goto error;
	}
	memcpy(mngr->name, info->name, DSI_CLK_NAME_LEN);
error:
	pr_debug("EXIT %s, rc = %ld\n", mngr->name, PTR_ERR(mngr));
	return mngr;
}

int mdss_dsi_clk_deinit(void *clk_mngr)
{
	int rc = 0;
	struct mdss_dsi_clk_mngr *mngr = clk_mngr;
	struct list_head *position = NULL;
	struct list_head *tmp = NULL;
	struct mdss_dsi_clk_client_info *node;

	if (!mngr) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("%s: ENTER\n", mngr->name);
	mutex_lock(&mngr->clk_mutex);

	list_for_each_safe(position, tmp, &mngr->client_list) {
		node = list_entry(position, struct mdss_dsi_clk_client_info,
				  list);
		list_del(&node->list);
		pr_debug("Removed device (%s)\n", node->name);
		kfree(node);
	}

	rc = dsi_recheck_clk_state(mngr);
	if (rc)
		pr_err("failed to disable all clocks\n");
	mdss_reg_bus_vote_client_destroy(mngr->reg_bus_clt);
	mutex_unlock(&mngr->clk_mutex);
	pr_debug("%s: EXIT, rc = %d\n", mngr->name, rc);
	kfree(mngr);
	return rc;
}

int mdss_dsi_clk_force_toggle(void *client, u32 clk)
{
	int rc = 0;
	struct mdss_dsi_clk_client_info *c = client;
	struct mdss_dsi_clk_mngr *mngr;

	if (!client || !clk || clk >= MDSS_DSI_CLKS_MAX) {
		pr_err("Invalid params, client = %pK, clk = 0x%x\n",
		       client, clk);
		return -EINVAL;
	}

	mngr = c->mngr;
	mutex_lock(&mngr->clk_mutex);

	if ((clk & MDSS_DSI_CORE_CLK) &&
	    (mngr->core_clks.current_clk_state == MDSS_DSI_CLK_ON)) {

		rc = dsi_core_clk_stop(&mngr->core_clks);
		if (rc) {
			pr_err("failed to stop core clks\n");
			goto error;
		}

		rc = dsi_core_clk_start(&mngr->core_clks);
		if (rc)
			pr_err("failed to start core clks\n");

	} else if (clk & MDSS_DSI_CORE_CLK) {
		pr_err("cannot reset, core clock is off\n");
		rc = -ENOTSUPP;
		goto error;
	}

	if ((clk & MDSS_DSI_LINK_CLK) &&
	    (mngr->link_clks.current_clk_state == MDSS_DSI_CLK_ON)) {

		rc = dsi_link_hs_clk_stop(&mngr->link_clks.hs_clks);
		if (rc) {
			pr_err("failed to stop HS link clks\n");
			goto error;
		}

		rc = dsi_link_lp_clk_stop(&mngr->link_clks.lp_clks);
		if (rc) {
			pr_err("failed to stop LP link clks\n");
			goto error;
		}

		rc = dsi_link_lp_clk_start(&mngr->link_clks.lp_clks);
		if (rc)
			pr_err("failed to start LP link clks\n");

		rc = dsi_link_hs_clk_start(&mngr->link_clks.hs_clks,
				MDSS_DSI_LINK_CLK_START);
		if (rc)
			pr_err("failed to start HS link clks\n");

	} else if (clk & MDSS_DSI_LINK_CLK) {
		pr_err("cannot reset, link clock is off\n");
		rc = -ENOTSUPP;
		goto error;
	}

error:
	mutex_unlock(&mngr->clk_mutex);
	return rc;
}
