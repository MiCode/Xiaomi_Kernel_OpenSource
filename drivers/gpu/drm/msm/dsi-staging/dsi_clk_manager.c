/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/msm-bus.h>
#include "dsi_clk.h"

struct dsi_core_clks {
	struct dsi_core_clk_info clks;
	u32 bus_handle;
};

struct dsi_link_clks {
	struct dsi_link_hs_clk_info hs_clks;
	struct dsi_link_lp_clk_info lp_clks;
	struct link_clk_freq freq;
};

struct dsi_clk_mngr {
	char name[MAX_STRING_LEN];
	struct mutex clk_mutex;
	struct list_head client_list;

	u32 dsi_ctrl_count;
	u32 master_ndx;
	struct dsi_core_clks core_clks[MAX_DSI_CTRL];
	struct dsi_link_clks link_clks[MAX_DSI_CTRL];
	u32 ctrl_index[MAX_DSI_CTRL];
	u32 core_clk_state;
	u32 link_clk_state;

	pre_clockoff_cb pre_clkoff_cb;
	post_clockoff_cb post_clkoff_cb;
	post_clockon_cb post_clkon_cb;
	pre_clockon_cb pre_clkon_cb;

	bool is_cont_splash_enabled;
	void *priv_data;
};

struct dsi_clk_client_info {
	char name[MAX_STRING_LEN];
	u32 core_refcount;
	u32 link_refcount;
	u32 core_clk_state;
	u32 link_clk_state;
	struct list_head list;
	struct dsi_clk_mngr *mngr;
};

static int _get_clk_mngr_index(struct dsi_clk_mngr *mngr,
				u32 dsi_ctrl_index,
				u32 *clk_mngr_index)
{
	int i;

	for (i = 0; i < mngr->dsi_ctrl_count; i++) {
		if (mngr->ctrl_index[i] == dsi_ctrl_index) {
			*clk_mngr_index = i;
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * dsi_clk_set_link_frequencies() - set frequencies for link clks
 * @clks:         Link clock information
 * @pixel_clk:    pixel clock frequency in KHz.
 * @byte_clk:     Byte clock frequency in KHz.
 * @esc_clk:      Escape clock frequency in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_link_frequencies(void *client, struct link_clk_freq freq,
				u32 index)
{
	int rc = 0, clk_mngr_index = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;

	if (!client) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	mngr = c->mngr;
	rc = _get_clk_mngr_index(mngr, index, &clk_mngr_index);
	if (rc) {
		pr_err("failed to map control index %d\n", index);
		return -EINVAL;
	}

	memcpy(&mngr->link_clks[clk_mngr_index].freq, &freq,
		sizeof(struct link_clk_freq));

	return rc;
}

/**
 * dsi_clk_set_pixel_clk_rate() - set frequency for pixel clock
 * @clks:      DSI link clock information.
 * @pixel_clk: Pixel clock rate in KHz.
 *
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_pixel_clk_rate(void *client, u64 pixel_clk, u32 index)
{
	int rc = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;

	mngr = c->mngr;
	rc = clk_set_rate(mngr->link_clks[index].hs_clks.pixel_clk, pixel_clk);
	if (rc)
		pr_err("failed to set clk rate for pixel clk, rc=%d\n", rc);
	else
		mngr->link_clks[index].freq.pix_clk_rate = pixel_clk;

	return rc;
}

/**
 * dsi_clk_set_byte_clk_rate() - set frequency for byte clock
 * @client:       DSI clock client pointer.
 * @byte_clk: Pixel clock rate in Hz.
 * @index:      Index of the DSI controller.
 * return: error code in case of failure or 0 for success.
 */
int dsi_clk_set_byte_clk_rate(void *client, u64 byte_clk, u32 index)
{
	int rc = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;

	mngr = c->mngr;
	rc = clk_set_rate(mngr->link_clks[index].hs_clks.byte_clk, byte_clk);
	if (rc)
		pr_err("failed to set clk rate for byte clk, rc=%d\n", rc);
	else
		mngr->link_clks[index].freq.byte_clk_rate = byte_clk;

	return rc;

}

/**
 * dsi_clk_update_parent() - update parent clocks for specified clock
 * @parent:       link clock pair which are set as parent.
 * @child:        link clock pair whose parent has to be set.
 */
int dsi_clk_update_parent(struct dsi_clk_link_set *parent,
			  struct dsi_clk_link_set *child)
{
	int rc = 0;

	rc = clk_set_parent(child->byte_clk, parent->byte_clk);
	if (rc) {
		pr_err("failed to set byte clk parent\n");
		goto error;
	}

	rc = clk_set_parent(child->pixel_clk, parent->pixel_clk);
	if (rc) {
		pr_err("failed to set pixel clk parent\n");
		goto error;
	}
error:
	return rc;
}

int dsi_core_clk_start(struct dsi_core_clks *c_clks)
{
	int rc = 0;

	if (c_clks->clks.mdp_core_clk) {
		rc = clk_prepare_enable(c_clks->clks.mdp_core_clk);
		if (rc) {
			pr_err("failed to enable mdp_core_clk, rc=%d\n", rc);
			goto error;
		}
	}

	if (c_clks->clks.mnoc_clk) {
		rc = clk_prepare_enable(c_clks->clks.mnoc_clk);
		if (rc) {
			pr_err("failed to enable mnoc_clk, rc=%d\n", rc);
			goto error_disable_core_clk;
		}
	}

	if (c_clks->clks.iface_clk) {
		rc = clk_prepare_enable(c_clks->clks.iface_clk);
		if (rc) {
			pr_err("failed to enable iface_clk, rc=%d\n", rc);
			goto error_disable_mnoc_clk;
		}
	}

	if (c_clks->clks.bus_clk) {
		rc = clk_prepare_enable(c_clks->clks.bus_clk);
		if (rc) {
			pr_err("failed to enable bus_clk, rc=%d\n", rc);
			goto error_disable_iface_clk;
		}
	}

	if (c_clks->clks.core_mmss_clk) {
		rc = clk_prepare_enable(c_clks->clks.core_mmss_clk);
		if (rc) {
			pr_err("failed to enable core_mmss_clk, rc=%d\n", rc);
			goto error_disable_bus_clk;
		}
	}

	if (c_clks->bus_handle) {
		rc = msm_bus_scale_client_update_request(c_clks->bus_handle, 1);
		if (rc) {
			pr_err("bus scale client enable failed, rc=%d\n", rc);
			goto error_disable_mmss_clk;
		}
	}

	return rc;

error_disable_mmss_clk:
	if (c_clks->clks.core_mmss_clk)
		clk_disable_unprepare(c_clks->clks.core_mmss_clk);
error_disable_bus_clk:
	if (c_clks->clks.bus_clk)
		clk_disable_unprepare(c_clks->clks.bus_clk);
error_disable_iface_clk:
	if (c_clks->clks.iface_clk)
		clk_disable_unprepare(c_clks->clks.iface_clk);
error_disable_mnoc_clk:
	if (c_clks->clks.mnoc_clk)
		clk_disable_unprepare(c_clks->clks.mnoc_clk);
error_disable_core_clk:
	if (c_clks->clks.mdp_core_clk)
		clk_disable_unprepare(c_clks->clks.mdp_core_clk);
error:
	return rc;
}

int dsi_core_clk_stop(struct dsi_core_clks *c_clks)
{
	int rc = 0;

	if (c_clks->bus_handle) {
		rc = msm_bus_scale_client_update_request(c_clks->bus_handle, 0);
		if (rc) {
			pr_err("bus scale client disable failed, rc=%d\n", rc);
			return rc;
		}
	}

	if (c_clks->clks.core_mmss_clk)
		clk_disable_unprepare(c_clks->clks.core_mmss_clk);

	if (c_clks->clks.bus_clk)
		clk_disable_unprepare(c_clks->clks.bus_clk);

	if (c_clks->clks.iface_clk)
		clk_disable_unprepare(c_clks->clks.iface_clk);

	if (c_clks->clks.mnoc_clk)
		clk_disable_unprepare(c_clks->clks.mnoc_clk);

	if (c_clks->clks.mdp_core_clk)
		clk_disable_unprepare(c_clks->clks.mdp_core_clk);

	return rc;
}

static int dsi_link_hs_clk_set_rate(struct dsi_link_hs_clk_info *link_hs_clks,
		int index)
{
	int rc = 0;
	struct dsi_clk_mngr *mngr;
	struct dsi_link_clks *l_clks;

	if (index >= MAX_DSI_CTRL) {
		pr_err("Invalid DSI ctrl index\n");
		return -EINVAL;
	}

	l_clks = container_of(link_hs_clks, struct dsi_link_clks, hs_clks);
	mngr = container_of(l_clks, struct dsi_clk_mngr, link_clks[index]);

	/*
	 * In an ideal world, cont_splash_enabled should not be required inside
	 * the clock manager. But, in the current driver cont_splash_enabled
	 * flag is set inside mdp driver and there is no interface event
	 * associated with this flag setting.
	 */
	if (mngr->is_cont_splash_enabled)
		return 0;

	rc = clk_set_rate(link_hs_clks->byte_clk,
		l_clks->freq.byte_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for byte_clk rc = %d\n", rc);
		goto error;
	}

	rc = clk_set_rate(link_hs_clks->pixel_clk,
		l_clks->freq.pix_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for pixel_clk rc = %d\n", rc);
		goto error;
	}

	/*
	 * If byte_intf_clk is present, set rate for that too.
	 * For DPHY: byte_intf_clk_rate = byte_clk_rate / 2
	 * todo: this needs to be revisited when support for CPHY is added
	 */
	if (link_hs_clks->byte_intf_clk) {
		rc = clk_set_rate(link_hs_clks->byte_intf_clk,
			(l_clks->freq.byte_clk_rate / 2));
		if (rc) {
			pr_err("set_rate failed for byte_intf_clk rc = %d\n",
				rc);
			goto error;
		}
	}
error:
	return rc;
}

static int dsi_link_hs_clk_prepare(struct dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	rc = clk_prepare(link_hs_clks->byte_clk);
	if (rc) {
		pr_err("Failed to prepare dsi byte clk, rc=%d\n", rc);
		goto byte_clk_err;
	}

	rc = clk_prepare(link_hs_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to prepare dsi pixel clk, rc=%d\n", rc);
		goto pixel_clk_err;
	}

	if (link_hs_clks->byte_intf_clk) {
		rc = clk_prepare(link_hs_clks->byte_intf_clk);
		if (rc) {
			pr_err("Failed to prepare dsi byte intf clk, rc=%d\n",
				rc);
			goto byte_intf_clk_err;
		}
	}

	return rc;

byte_intf_clk_err:
	clk_unprepare(link_hs_clks->pixel_clk);
pixel_clk_err:
	clk_unprepare(link_hs_clks->byte_clk);
byte_clk_err:
	return rc;
}

static void dsi_link_hs_clk_unprepare(struct dsi_link_hs_clk_info *link_hs_clks)
{
	if (link_hs_clks->byte_intf_clk)
		clk_unprepare(link_hs_clks->byte_intf_clk);
	clk_unprepare(link_hs_clks->pixel_clk);
	clk_unprepare(link_hs_clks->byte_clk);
}

static int dsi_link_hs_clk_enable(struct dsi_link_hs_clk_info *link_hs_clks)
{
	int rc = 0;

	rc = clk_enable(link_hs_clks->byte_clk);
	if (rc) {
		pr_err("Failed to enable dsi byte clk, rc=%d\n", rc);
		goto byte_clk_err;
	}

	rc = clk_enable(link_hs_clks->pixel_clk);
	if (rc) {
		pr_err("Failed to enable dsi pixel clk, rc=%d\n", rc);
		goto pixel_clk_err;
	}

	if (link_hs_clks->byte_intf_clk) {
		rc = clk_enable(link_hs_clks->byte_intf_clk);
		if (rc) {
			pr_err("Failed to enable dsi byte intf clk, rc=%d\n",
				rc);
			goto byte_intf_clk_err;
		}
	}

	return rc;

byte_intf_clk_err:
	clk_disable(link_hs_clks->pixel_clk);
pixel_clk_err:
	clk_disable(link_hs_clks->byte_clk);
byte_clk_err:
	return rc;
}

static void dsi_link_hs_clk_disable(struct dsi_link_hs_clk_info *link_hs_clks)
{
	if (link_hs_clks->byte_intf_clk)
		clk_disable(link_hs_clks->byte_intf_clk);
	clk_disable(link_hs_clks->pixel_clk);
	clk_disable(link_hs_clks->byte_clk);
}

/**
 * dsi_link_clk_start() - enable dsi link clocks
 */
static int dsi_link_hs_clk_start(struct dsi_link_hs_clk_info *link_hs_clks,
	enum dsi_link_clk_op_type op_type, int index)
{
	int rc = 0;

	if (index >= MAX_DSI_CTRL) {
		pr_err("Invalid DSI ctrl index\n");
		return -EINVAL;
	}

	if (op_type & DSI_LINK_CLK_SET_RATE) {
		rc = dsi_link_hs_clk_set_rate(link_hs_clks, index);
		if (rc) {
			pr_err("failed to set HS clk rates, rc = %d\n", rc);
			goto error;
		}
	}

	if (op_type & DSI_LINK_CLK_PREPARE) {
		rc = dsi_link_hs_clk_prepare(link_hs_clks);
		if (rc) {
			pr_err("failed to prepare link HS clks, rc = %d\n", rc);
			goto error;
		}
	}

	if (op_type & DSI_LINK_CLK_ENABLE) {
		rc = dsi_link_hs_clk_enable(link_hs_clks);
		if (rc) {
			pr_err("failed to enable link HS clks, rc = %d\n", rc);
			goto error_unprepare;
		}
	}

	pr_debug("HS Link clocks are enabled\n");
	return rc;
error_unprepare:
	dsi_link_hs_clk_unprepare(link_hs_clks);
error:
	return rc;
}

/**
 * dsi_link_clk_stop() - Stop DSI link clocks.
 */
static int dsi_link_hs_clk_stop(struct dsi_link_hs_clk_info *link_hs_clks)
{
	struct dsi_link_clks *l_clks;

	l_clks = container_of(link_hs_clks, struct dsi_link_clks, hs_clks);

	dsi_link_hs_clk_disable(link_hs_clks);
	dsi_link_hs_clk_unprepare(link_hs_clks);

	pr_debug("HS Link clocks disabled\n");

	return 0;
}

static int dsi_link_lp_clk_start(struct dsi_link_lp_clk_info *link_lp_clks,
	int index)
{
	int rc = 0;
	struct dsi_clk_mngr *mngr;
	struct dsi_link_clks *l_clks;

	if (index >= MAX_DSI_CTRL) {
		pr_err("Invalid DSI ctrl index\n");
		return -EINVAL;
	}

	l_clks = container_of(link_lp_clks, struct dsi_link_clks, lp_clks);

	mngr = container_of(l_clks, struct dsi_clk_mngr, link_clks[index]);
	if (!mngr)
		return -EINVAL;

	/*
	 * In an ideal world, cont_splash_enabled should not be required inside
	 * the clock manager. But, in the current driver cont_splash_enabled
	 * flag is set inside mdp driver and there is no interface event
	 * associated with this flag setting. Also, set rate for clock need not
	 * be called for every enable call. It should be done only once when
	 * coming out of suspend.
	 */
	if (mngr->is_cont_splash_enabled)
		goto prepare;

	rc = clk_set_rate(link_lp_clks->esc_clk, l_clks->freq.esc_clk_rate);
	if (rc) {
		pr_err("clk_set_rate failed for esc_clk rc = %d\n", rc);
		goto error;
	}

prepare:
	rc = clk_prepare_enable(link_lp_clks->esc_clk);
	if (rc) {
		pr_err("Failed to enable dsi esc clk\n");
		clk_unprepare(l_clks->lp_clks.esc_clk);
	}
error:
	pr_debug("LP Link clocks are enabled\n");
	return rc;
}

static int dsi_link_lp_clk_stop(
	struct dsi_link_lp_clk_info *link_lp_clks)
{
	struct dsi_link_clks *l_clks;

	l_clks = container_of(link_lp_clks, struct dsi_link_clks, lp_clks);

	clk_disable_unprepare(l_clks->lp_clks.esc_clk);

	pr_debug("LP Link clocks are disabled\n");
	return 0;
}

static int dsi_display_core_clk_enable(struct dsi_core_clks *clks,
	u32 ctrl_count, u32 master_ndx)
{
	int rc = 0;
	int i;
	struct dsi_core_clks *clk, *m_clks;

	/*
	 * In case of split DSI usecases, the clock for master controller should
	 * be enabled before the other controller. Master controller in the
	 * clock context refers to the controller that sources the clock.
	 */

	m_clks = &clks[master_ndx];
	rc = sde_power_resource_enable(m_clks->clks.phandle,
			m_clks->clks.dsi_core_client, true);

	if (rc) {
		pr_err("Power resource enable failed, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_core_clk_start(m_clks);
	if (rc) {
		pr_err("failed to turn on master clocks, rc=%d\n", rc);
		goto error_disable_master_resource;
	}

	/* Turn on rest of the core clocks */
	for (i = 0; i < ctrl_count; i++) {
		clk = &clks[i];
		if (!clk || (clk == m_clks))
			continue;

		rc = sde_power_resource_enable(clk->clks.phandle,
				clk->clks.dsi_core_client, true);
		if (rc) {
			pr_err("Power resource enable failed, rc=%d\n", rc);
			goto error_disable_master;
		}

		rc = dsi_core_clk_start(clk);
		if (rc) {
			pr_err("failed to turn on clocks, rc=%d\n", rc);
			(void)sde_power_resource_enable(clk->clks.phandle,
					clk->clks.dsi_core_client, false);
			goto error_disable_master;
		}
	}
	return rc;
error_disable_master:
	(void)dsi_core_clk_stop(m_clks);

error_disable_master_resource:
	(void)sde_power_resource_enable(m_clks->clks.phandle,
				m_clks->clks.dsi_core_client, false);
error:
	return rc;
}

static int dsi_display_link_clk_enable(struct dsi_link_clks *clks,
	enum dsi_lclk_type l_type, u32 ctrl_count, u32 master_ndx)
{
	int rc = 0;
	int i;
	struct dsi_link_clks *clk, *m_clks;

	/*
	 * In case of split DSI usecases, the clock for master controller should
	 * be enabled before the other controller. Master controller in the
	 * clock context refers to the controller that sources the clock.
	 */

	m_clks = &clks[master_ndx];

	if (l_type & DSI_LINK_LP_CLK) {
		rc = dsi_link_lp_clk_start(&m_clks->lp_clks, master_ndx);
		if (rc) {
			pr_err("failed to turn on master lp link clocks, rc=%d\n",
				rc);
			goto error;
		}
	}

	if (l_type & DSI_LINK_HS_CLK) {
		rc = dsi_link_hs_clk_start(&m_clks->hs_clks,
			DSI_LINK_CLK_START, master_ndx);
		if (rc) {
			pr_err("failed to turn on master hs link clocks, rc=%d\n",
				rc);
			goto error;
		}
	}

	for (i = 0; i < ctrl_count; i++) {
		clk = &clks[i];
		if (!clk || (clk == m_clks))
			continue;

		if (l_type & DSI_LINK_LP_CLK) {
			rc = dsi_link_lp_clk_start(&clk->lp_clks, i);
			if (rc) {
				pr_err("failed to turn on lp link clocks, rc=%d\n",
					rc);
				goto error_disable_master;
			}
		}

		if (l_type & DSI_LINK_HS_CLK) {
			rc = dsi_link_hs_clk_start(&clk->hs_clks,
				DSI_LINK_CLK_START, i);
			if (rc) {
				pr_err("failed to turn on hs link clocks, rc=%d\n",
					rc);
				goto error_disable_master;
			}
		}
	}
	return rc;

error_disable_master:
	if (l_type == DSI_LINK_LP_CLK)
		(void)dsi_link_lp_clk_stop(&m_clks->lp_clks);
	else if (l_type == DSI_LINK_HS_CLK)
		(void)dsi_link_hs_clk_stop(&m_clks->hs_clks);
error:
	return rc;
}

static int dsi_display_core_clk_disable(struct dsi_core_clks *clks,
	u32 ctrl_count, u32 master_ndx)
{
	int rc = 0;
	int i;
	struct dsi_core_clks *clk, *m_clks;

	/*
	 * In case of split DSI usecases, clock for slave DSI controllers should
	 * be disabled first before disabling clock for master controller. Slave
	 * controllers in the clock context refer to controller which source
	 * clock from another controller.
	 */

	m_clks = &clks[master_ndx];

	/* Turn off non-master core clocks */
	for (i = 0; i < ctrl_count; i++) {
		clk = &clks[i];
		if (!clk || (clk == m_clks))
			continue;

		rc = dsi_core_clk_stop(clk);
		if (rc) {
			pr_debug("failed to turn off clocks, rc=%d\n", rc);
			goto error;
		}

		rc = sde_power_resource_enable(clk->clks.phandle,
				clk->clks.dsi_core_client, false);
		if (rc) {
			pr_err("Power resource disable failed: %d\n", rc);
			goto error;
		}
	}

	rc = dsi_core_clk_stop(m_clks);
	if (rc) {
		pr_err("failed to turn off master clocks, rc=%d\n", rc);
		goto error;
	}

	rc = sde_power_resource_enable(m_clks->clks.phandle,
				m_clks->clks.dsi_core_client, false);
	if (rc)
		pr_err("Power resource disable failed: %d\n", rc);
error:
	return rc;
}

static int dsi_display_link_clk_disable(struct dsi_link_clks *clks,
	enum dsi_lclk_type l_type, u32 ctrl_count, u32 master_ndx)
{
	int rc = 0;
	int i;
	struct dsi_link_clks *clk, *m_clks;

	/*
	 * In case of split DSI usecases, clock for slave DSI controllers should
	 * be disabled first before disabling clock for master controller. Slave
	 * controllers in the clock context refer to controller which source
	 * clock from another controller.
	 */

	m_clks = &clks[master_ndx];

	/* Turn off non-master link clocks */
	for (i = 0; i < ctrl_count; i++) {
		clk = &clks[i];
		if (!clk || (clk == m_clks))
			continue;

		if (l_type & DSI_LINK_LP_CLK) {
			rc = dsi_link_lp_clk_stop(&clk->lp_clks);
			if (rc)
				pr_err("failed to turn off lp link clocks, rc=%d\n",
					rc);
		}

		if (l_type & DSI_LINK_HS_CLK) {
			rc = dsi_link_hs_clk_stop(&clk->hs_clks);
			if (rc)
				pr_err("failed to turn off hs link clocks, rc=%d\n",
					rc);
		}
	}

	if (l_type & DSI_LINK_LP_CLK) {
		rc = dsi_link_lp_clk_stop(&m_clks->lp_clks);
		if (rc)
			pr_err("failed to turn off master lp link clocks, rc=%d\n",
				rc);
	}

	if (l_type & DSI_LINK_HS_CLK) {
		rc = dsi_link_hs_clk_stop(&m_clks->hs_clks);
		if (rc)
			pr_err("failed to turn off master hs link clocks, rc=%d\n",
				rc);
	}

	return rc;
}

static int dsi_clk_update_link_clk_state(struct dsi_clk_mngr *mngr,
	struct dsi_link_clks *l_clks, enum dsi_lclk_type l_type, u32 l_state,
	bool enable)
{
	int rc = 0;

	if (!mngr)
		return -EINVAL;

	if (enable) {
		if (mngr->pre_clkon_cb) {
			rc = mngr->pre_clkon_cb(mngr->priv_data, DSI_LINK_CLK,
				l_type, l_state);
			if (rc) {
				pr_err("pre link clk on cb failed for type %d\n",
					l_type);
				goto error;
			}
		}
		rc = dsi_display_link_clk_enable(l_clks, l_type,
				mngr->dsi_ctrl_count, mngr->master_ndx);
		if (rc) {
			pr_err("failed to start link clk type %d rc=%d\n",
				l_type, rc);
			goto error;
		}

		if (mngr->post_clkon_cb) {
			rc = mngr->post_clkon_cb(mngr->priv_data, DSI_LINK_CLK,
				l_type, l_state);
			if (rc) {
				pr_err("post link clk on cb failed for type %d\n",
					l_type);
				goto error;
			}
		}
	} else {
		if (mngr->pre_clkoff_cb) {
			rc = mngr->pre_clkoff_cb(mngr->priv_data,
				DSI_LINK_CLK, l_type, l_state);
			if (rc)
				pr_err("pre link clk off cb failed\n");
		}

		rc = dsi_display_link_clk_disable(l_clks, l_type,
			mngr->dsi_ctrl_count, mngr->master_ndx);
		if (rc) {
			pr_err("failed to stop link clk type %d, rc = %d\n",
			       l_type, rc);
			goto error;
		}

		if (mngr->post_clkoff_cb) {
			rc = mngr->post_clkoff_cb(mngr->priv_data,
				DSI_LINK_CLK, l_type, l_state);
			if (rc)
				pr_err("post link clk off cb failed\n");
		}
	}

error:
	return rc;
}

static int dsi_update_clk_state(struct dsi_clk_mngr *mngr,
	struct dsi_core_clks *c_clks, u32 c_state,
	struct dsi_link_clks *l_clks, u32 l_state)
{
	int rc = 0;
	bool l_c_on = false;

	if (!mngr)
		return -EINVAL;

	pr_debug("c_state = %d, l_state = %d\n",
		 c_clks ? c_state : -1, l_clks ? l_state : -1);
	/*
	 * Below is the sequence to toggle DSI clocks:
	 *	1. For ON sequence, Core clocks before link clocks
	 *	2. For OFF sequence, Link clocks before core clocks.
	 */
	if (c_clks && (c_state == DSI_CLK_ON)) {
		if (mngr->core_clk_state == DSI_CLK_OFF) {
			rc = mngr->pre_clkon_cb(mngr->priv_data,
						DSI_CORE_CLK,
						DSI_LINK_NONE,
						DSI_CLK_ON);
			if (rc) {
				pr_err("failed to turn on MDP FS rc= %d\n", rc);
				goto error;
			}
		}
		rc = dsi_display_core_clk_enable(c_clks, mngr->dsi_ctrl_count,
				mngr->master_ndx);
		if (rc) {
			pr_err("failed to turn on core clks rc = %d\n", rc);
			goto error;
		}

		if (mngr->post_clkon_cb) {
			rc = mngr->post_clkon_cb(mngr->priv_data,
						 DSI_CORE_CLK,
						 DSI_LINK_NONE,
						 DSI_CLK_ON);
			if (rc)
				pr_err("post clk on cb failed, rc = %d\n", rc);
		}
		mngr->core_clk_state = DSI_CLK_ON;
	}

	if (l_clks) {
		if (l_state == DSI_CLK_ON) {
			rc = dsi_clk_update_link_clk_state(mngr, l_clks,
				DSI_LINK_LP_CLK, l_state, true);
			if (rc)
				goto error;

			rc = dsi_clk_update_link_clk_state(mngr, l_clks,
				DSI_LINK_HS_CLK, l_state, true);
			if (rc)
				goto error;
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
			if ((l_state == DSI_CLK_OFF) &&
			    (mngr->link_clk_state ==
			    DSI_CLK_EARLY_GATE) &&
			    (mngr->core_clk_state !=
			    DSI_CLK_ON)) {
				rc = dsi_display_core_clk_enable(
					mngr->core_clks, mngr->dsi_ctrl_count,
					mngr->master_ndx);
				if (rc) {
					pr_err("core clks did not start\n");
					goto error;
				}

				rc = dsi_display_link_clk_enable(l_clks,
					(DSI_LINK_LP_CLK & DSI_LINK_HS_CLK),
					mngr->dsi_ctrl_count, mngr->master_ndx);
				if (rc) {
					pr_err("LP Link clks did not start\n");
					goto error;
				}
				l_c_on = true;
				pr_debug("ECG: core and Link_on\n");
			}

			rc = dsi_clk_update_link_clk_state(mngr, l_clks,
				DSI_LINK_HS_CLK, l_state, false);
			if (rc)
				goto error;

			rc = dsi_clk_update_link_clk_state(mngr, l_clks,
				DSI_LINK_LP_CLK, l_state, false);
			if (rc)
				goto error;

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
			if (l_c_on && (!(c_clks && (c_state == DSI_CLK_OFF)
					 && (mngr->core_clk_state ==
					     DSI_CLK_EARLY_GATE)))) {
				rc = dsi_display_core_clk_disable(
					mngr->core_clks, mngr->dsi_ctrl_count,
					mngr->master_ndx);
				if (rc) {
					pr_err("core clks did not stop\n");
					goto error;
				}

				l_c_on = false;
				pr_debug("ECG: core off\n");
			} else
				pr_debug("ECG: core off skip\n");
		}

		mngr->link_clk_state = l_state;
	}

	if (c_clks && (c_state != DSI_CLK_ON)) {
		/*
		 * When going to OFF state from EARLY GATE state, Core clocks
		 * should be turned on first so that the IOs can be clamped.
		 * l_c_on flag is set, then the core clocks were turned before
		 * to the Link clocks go to OFF state. So Core clocks are
		 * already ON and this step can be skipped.
		 *
		 * IOs are clamped in pre_clkoff_cb callback.
		 */
		if ((c_state == DSI_CLK_OFF) &&
		    (mngr->core_clk_state ==
		    DSI_CLK_EARLY_GATE) && !l_c_on) {
			rc = dsi_display_core_clk_enable(mngr->core_clks,
				mngr->dsi_ctrl_count, mngr->master_ndx);
			if (rc) {
				pr_err("core clks did not start\n");
				goto error;
			}
			pr_debug("ECG: core on\n");
		} else
			pr_debug("ECG: core on skip\n");

		if (mngr->pre_clkoff_cb) {
			rc = mngr->pre_clkoff_cb(mngr->priv_data,
						 DSI_CORE_CLK,
						 DSI_LINK_NONE,
						 c_state);
			if (rc)
				pr_err("pre core clk off cb failed\n");
		}

		rc = dsi_display_core_clk_disable(c_clks, mngr->dsi_ctrl_count,
			mngr->master_ndx);
		if (rc) {
			pr_err("failed to turn off core clks rc = %d\n", rc);
			goto error;
		}

		if (c_state == DSI_CLK_OFF) {
			if (mngr->post_clkoff_cb) {
				rc = mngr->post_clkoff_cb(mngr->priv_data,
						DSI_CORE_CLK,
						DSI_LINK_NONE,
						DSI_CLK_OFF);
				if (rc)
					pr_err("post clkoff cb fail, rc = %d\n",
					       rc);
			}
		}
		mngr->core_clk_state = c_state;
	}

error:
	return rc;
}

static int dsi_recheck_clk_state(struct dsi_clk_mngr *mngr)
{
	int rc = 0;
	struct list_head *pos = NULL;
	struct dsi_clk_client_info *c;
	u32 new_core_clk_state = DSI_CLK_OFF;
	u32 new_link_clk_state = DSI_CLK_OFF;
	u32 old_c_clk_state = DSI_CLK_OFF;
	u32 old_l_clk_state = DSI_CLK_OFF;
	struct dsi_core_clks *c_clks = NULL;
	struct dsi_link_clks *l_clks = NULL;

	/*
	 * Conditions to maintain DSI manager clock state based on
	 *		clock states of various clients:
	 *	1. If any client has clock in ON state, DSI manager clock state
	 *		should be ON.
	 *	2. If any client is in ECG state with rest of them turned OFF,
	 *	   go to Early gate state.
	 *	3. If all clients have clocks as OFF, then go to OFF state.
	 */
	list_for_each(pos, &mngr->client_list) {
		c = list_entry(pos, struct dsi_clk_client_info, list);
		if (c->core_clk_state == DSI_CLK_ON) {
			new_core_clk_state = DSI_CLK_ON;
			break;
		} else if (c->core_clk_state == DSI_CLK_EARLY_GATE) {
			new_core_clk_state = DSI_CLK_EARLY_GATE;
		}
	}

	list_for_each(pos, &mngr->client_list) {
		c = list_entry(pos, struct dsi_clk_client_info, list);
		if (c->link_clk_state == DSI_CLK_ON) {
			new_link_clk_state = DSI_CLK_ON;
			break;
		} else if (c->link_clk_state == DSI_CLK_EARLY_GATE) {
			new_link_clk_state = DSI_CLK_EARLY_GATE;
		}
	}

	if (new_core_clk_state != mngr->core_clk_state)
		c_clks = mngr->core_clks;

	if (new_link_clk_state != mngr->link_clk_state)
		l_clks = mngr->link_clks;

	old_c_clk_state = mngr->core_clk_state;
	old_l_clk_state = mngr->link_clk_state;

	pr_debug("c_clk_state (%d -> %d)\n",
		old_c_clk_state, new_core_clk_state);
	pr_debug("l_clk_state (%d -> %d)\n",
		old_l_clk_state, new_link_clk_state);

	if (c_clks || l_clks) {
		rc = dsi_update_clk_state(mngr, c_clks, new_core_clk_state,
					  l_clks, new_link_clk_state);
		if (rc) {
			pr_err("failed to update clock state, rc = %d\n", rc);
			goto error;
		}
	}

error:
	return rc;
}

int dsi_clk_req_state(void *client, enum dsi_clk_type clk,
	enum dsi_clk_state state)
{
	int rc = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;
	bool changed = false;

	if (!client || !clk || clk > (DSI_CORE_CLK | DSI_LINK_CLK) ||
	    state > DSI_CLK_EARLY_GATE) {
		pr_err("Invalid params, client = %pK, clk = 0x%x, state = %d\n",
		       client, clk, state);
		return -EINVAL;
	}

	mngr = c->mngr;
	mutex_lock(&mngr->clk_mutex);

	pr_debug("[%s]%s: CLK=%d, new_state=%d, core=%d, linkl=%d\n",
	       mngr->name, c->name, clk, state, c->core_clk_state,
	       c->link_clk_state);

	/*
	 * Clock refcount handling as below:
	 *	i. Increment refcount whenever ON is called.
	 *	ii. Decrement refcount when transitioning from ON state to
	 *		either OFF or EARLY_GATE.
	 *	iii. Do not decrement refcount when changing from
	 *		EARLY_GATE to OFF.
	 */
	if (state == DSI_CLK_ON) {
		if (clk & DSI_CORE_CLK) {
			c->core_refcount++;
			if (c->core_clk_state != DSI_CLK_ON) {
				c->core_clk_state = DSI_CLK_ON;
				changed = true;
			}
		}
		if (clk & DSI_LINK_CLK) {
			c->link_refcount++;
			if (c->link_clk_state != DSI_CLK_ON) {
				c->link_clk_state = DSI_CLK_ON;
				changed = true;
			}
		}
	} else if ((state == DSI_CLK_EARLY_GATE) ||
		   (state == DSI_CLK_OFF)) {
		if (clk & DSI_CORE_CLK) {
			if (c->core_refcount == 0) {
				if ((c->core_clk_state ==
				    DSI_CLK_EARLY_GATE) &&
				    (state == DSI_CLK_OFF)) {
					changed = true;
					c->core_clk_state = DSI_CLK_OFF;
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
		if (clk & DSI_LINK_CLK) {
			if (c->link_refcount == 0) {
				if ((c->link_clk_state ==
				    DSI_CLK_EARLY_GATE) &&
				    (state == DSI_CLK_OFF)) {
					changed = true;
					c->link_clk_state = DSI_CLK_OFF;
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
		 mngr->name, c->name, changed, c->core_refcount,
		 c->core_clk_state, c->link_refcount, c->link_clk_state);

	if (changed) {
		rc = dsi_recheck_clk_state(mngr);
		if (rc)
			pr_err("Failed to adjust clock state rc = %d\n", rc);
	}

	mutex_unlock(&mngr->clk_mutex);
	return rc;
}

DEFINE_MUTEX(dsi_mngr_clk_mutex);

static int dsi_display_link_clk_force_update(void *client)
{
	int rc = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;
	struct dsi_link_clks *l_clks;

	mngr = c->mngr;
	mutex_lock(&mngr->clk_mutex);

	l_clks = mngr->link_clks;

	/*
	 * When link_clk_state is DSI_CLK_OFF, don't change DSI clock rate
	 * since it is possible to be overwritten, and return -EAGAIN to
	 * dynamic DSI writing interface to defer the reenabling to the next
	 * drm commit.
	 */
	if (mngr->link_clk_state == DSI_CLK_OFF) {
		rc = -EAGAIN;
		goto error;
	}

	rc = dsi_display_link_clk_disable(l_clks,
			(DSI_LINK_LP_CLK | DSI_LINK_HS_CLK),
			mngr->dsi_ctrl_count, mngr->master_ndx);
	if (rc) {
		pr_err("%s, failed to stop link clk, rc = %d\n",
			__func__, rc);
		goto error;
	}

	rc = dsi_display_link_clk_enable(l_clks,
			(DSI_LINK_LP_CLK | DSI_LINK_HS_CLK),
			mngr->dsi_ctrl_count, mngr->master_ndx);
	if (rc) {
		pr_err("%s, failed to start link clk rc= %d\n",
			__func__, rc);
		goto error;
	}

error:
	mutex_unlock(&mngr->clk_mutex);
	return rc;

}

int dsi_display_link_clk_force_update_ctrl(void *handle)
{
	int rc = 0;

	if (!handle) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dsi_mngr_clk_mutex);

	rc = dsi_display_link_clk_force_update(handle);

	mutex_unlock(&dsi_mngr_clk_mutex);

	return rc;
}

int dsi_display_clk_ctrl(void *handle, u32 clk_type, u32 clk_state)
{
	int rc = 0;

	if (!handle) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dsi_mngr_clk_mutex);
	rc = dsi_clk_req_state(handle, clk_type, clk_state);
	if (rc)
		pr_err("%s: failed set clk state, rc = %d\n", __func__, rc);
	mutex_unlock(&dsi_mngr_clk_mutex);

	return rc;
}

void *dsi_register_clk_handle(void *clk_mngr, char *client)
{
	void *handle = NULL;
	struct dsi_clk_mngr *mngr = clk_mngr;
	struct dsi_clk_client_info *c;

	if (!mngr) {
		pr_err("bad params\n");
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&mngr->clk_mutex);

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c) {
		handle = ERR_PTR(-ENOMEM);
		goto error;
	}

	strlcpy(c->name, client, MAX_STRING_LEN);
	c->mngr = mngr;

	list_add(&c->list, &mngr->client_list);

	pr_debug("[%s]: Added new client (%s)\n", mngr->name, c->name);
	handle = c;
error:
	mutex_unlock(&mngr->clk_mutex);
	return handle;
}

int dsi_deregister_clk_handle(void *client)
{
	int rc = 0;
	struct dsi_clk_client_info *c = client;
	struct dsi_clk_mngr *mngr;
	struct list_head *pos = NULL;
	struct list_head *tmp = NULL;
	struct dsi_clk_client_info *node = NULL;

	if (!client) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mngr = c->mngr;
	pr_debug("%s: ENTER\n", mngr->name);
	mutex_lock(&mngr->clk_mutex);
	c->core_clk_state = DSI_CLK_OFF;
	c->link_clk_state = DSI_CLK_OFF;

	rc = dsi_recheck_clk_state(mngr);
	if (rc) {
		pr_err("clock state recheck failed rc = %d\n", rc);
		goto error;
	}

	list_for_each_safe(pos, tmp, &mngr->client_list) {
		node = list_entry(pos, struct dsi_clk_client_info,
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

void dsi_display_clk_mngr_update_splash_status(void *clk_mgr, bool status)
{
	struct dsi_clk_mngr *mngr;

	if (!clk_mgr) {
		pr_err("Invalid params\n");
		return;
	}

	mngr = (struct dsi_clk_mngr *)clk_mgr;
	mngr->is_cont_splash_enabled = status;
}

void *dsi_display_clk_mngr_register(struct dsi_clk_info *info)
{
	struct dsi_clk_mngr *mngr;
	int i = 0;

	if (!info) {
		pr_err("Invalid params\n");
		return ERR_PTR(-EINVAL);
	}

	mngr = kzalloc(sizeof(*mngr), GFP_KERNEL);
	if (!mngr) {
		mngr = ERR_PTR(-ENOMEM);
		goto error;
	}

	mutex_init(&mngr->clk_mutex);
	mngr->dsi_ctrl_count = info->dsi_ctrl_count;
	mngr->master_ndx = info->master_ndx;

	if (mngr->dsi_ctrl_count > MAX_DSI_CTRL) {
		kfree(mngr);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < mngr->dsi_ctrl_count; i++) {
		memcpy(&mngr->core_clks[i].clks, &info->c_clks[i],
			sizeof(struct dsi_core_clk_info));
		memcpy(&mngr->link_clks[i].hs_clks, &info->l_hs_clks[i],
			sizeof(struct dsi_link_hs_clk_info));
		memcpy(&mngr->link_clks[i].lp_clks, &info->l_lp_clks[i],
			sizeof(struct dsi_link_lp_clk_info));
		mngr->core_clks[i].bus_handle = info->bus_handle[i];
		mngr->ctrl_index[i] = info->ctrl_index[i];
	}

	INIT_LIST_HEAD(&mngr->client_list);
	mngr->pre_clkon_cb = info->pre_clkon_cb;
	mngr->post_clkon_cb = info->post_clkon_cb;
	mngr->pre_clkoff_cb = info->pre_clkoff_cb;
	mngr->post_clkoff_cb = info->post_clkoff_cb;
	mngr->priv_data = info->priv_data;
	memcpy(mngr->name, info->name, MAX_STRING_LEN);

error:
	pr_debug("EXIT, rc = %ld\n", PTR_ERR(mngr));
	return mngr;
}

int dsi_display_clk_mngr_deregister(void *clk_mngr)
{
	int rc = 0;
	struct dsi_clk_mngr *mngr = clk_mngr;
	struct list_head *position = NULL;
	struct list_head *tmp = NULL;
	struct dsi_clk_client_info *node = NULL;

	if (!mngr) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("%s: ENTER\n", mngr->name);
	mutex_lock(&mngr->clk_mutex);

	list_for_each_safe(position, tmp, &mngr->client_list) {
		node = list_entry(position, struct dsi_clk_client_info,
			  list);
		list_del(&node->list);
		pr_debug("Removed device (%s)\n", node->name);
		kfree(node);
	}

	rc = dsi_recheck_clk_state(mngr);
	if (rc)
		pr_err("failed to disable all clocks\n");

	mutex_unlock(&mngr->clk_mutex);
	pr_debug("%s: EXIT, rc = %d\n", mngr->name, rc);
	kfree(mngr);
	return rc;
}

