/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#include "dsi_dbi.h"
#include "core/common/dsi/dsi_dbi_dsr.h"
#include "core/common/dsi/dsi_pkg_sender.h"

/*
 * Note: Don't modify/adjust DSR_COUNT.
 *
 * Some DC registers (DSPCNTR, SURFADDR, etc.) updating takes effect at the
 * rising edge of next TE interrupt, so DC should enter DSR at the next next TE.
 */
#define DSR_COUNT 2

static int __exit_dsr_locked(struct dsi_pipe *pipe)
{
	return dbi_power_on(pipe);
}

static int __enter_dsr_locked(struct dsi_pipe *pipe, int level)
{
	struct dsi_pkg_sender *sender = NULL;
	int err = 0;

	pr_debug("%s: enter dsr\n", __func__);

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	sender = &pipe->sender;

	if (!sender) {
		pr_err("Failed to get dsi sender\n");
		return -EINVAL;
	}

	if (level < DSR_EXITED) {
		pr_err("Invalid DSR level %d", level);
		return -EINVAL;
	}

	pr_debug("%s: entering DSR level %d\n", __func__, level);

	err = dsi_wait_for_fifos_empty(sender);
	if (err) {
		pr_err("%s: FIFO not empty\n", __func__);
		return err;
	}

	/*turn off dbi interface put in ulps*/
	dbi_power_off(pipe);

	pr_debug("entered\n");
	return 0;
}

int dsi_dsr_report_te(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	struct dsi_dsr *dsr = NULL;
	int err = 0;
	int dsr_level;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	dsr = config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		return 0;

	/*
	 * Simply enter DSR LEVEL0 even if HDMI is connected,
	 * to make MIO power island gated at least.
	 */
	dsr_level = DSR_ENTERED_LEVEL0;

	mutex_lock(&config->ctx_lock);

	if (!dsr->dsr_enabled)
		goto report_te_out;

	/*if panel is off, then forget it*/
	/* FIXME: to check the interface's DPMS state. */

	if (dsr_level <= dsr->dsr_state)
		goto report_te_out;
	else if (++dsr->free_count > DSR_COUNT && !dsr->ref_count) {
		/*reset free count*/
		dsr->free_count = 0;
		/*enter dsr*/
		err = __enter_dsr_locked(pipe, dsr_level);
		if (err) {
			pr_debug("Failed to enter DSR\n");
			goto report_te_out;
		}

		dsr->dsr_state = dsr_level;
	}
report_te_out:
	mutex_unlock(&config->ctx_lock);
	return err;
}

int dsi_dsr_forbid_locked(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	struct dsi_dsr *dsr = NULL;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	dsr = config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		return 0;

	/*exit dsr if necessary*/
	if (!dsr->dsr_enabled)
		goto forbid_out;

	pr_debug("%s\n", __func__);

	/*if reference count is not 0, it means dsr was forbidden*/
	if (dsr->ref_count) {
		dsr->ref_count++;
		goto forbid_out;
	}

	/*exited dsr if current dsr state is DSR_ENTERED*/
	if (dsr->dsr_state > DSR_EXITED) {
		err = __exit_dsr_locked(pipe);
		if (err) {
			pr_err("Failed to exit DSR\n");
			goto forbid_out;
		}
		dsr->dsr_state = DSR_EXITED;
	}
	dsr->ref_count++;
forbid_out:
	return err;
}

int dsi_dsr_forbid(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	mutex_lock(&config->ctx_lock);
	err = dsi_dsr_forbid_locked(pipe);
	mutex_unlock(&config->ctx_lock);

	return err;
}

int dsi_dsr_allow_locked(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	struct dsi_dsr *dsr = NULL;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	dsr = config->dsr;

	/*if no dsr attached, return 0*/
	if (!dsr)
		return 0;

	if (!dsr->dsr_enabled)
		goto allow_out;

	if (!dsr->ref_count) {
		pr_debug("Reference count is 0\n");
		goto allow_out;
	}

	pr_debug("%s\n", __func__);

	dsr->ref_count--;
allow_out:
	return 0;
}

int dsi_dsr_allow(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	int err = 0;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return -EINVAL;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return -EINVAL;
	}

	if (!config)
		return -EINVAL;

	mutex_lock(&config->ctx_lock);
	err = dsi_dsr_allow_locked(pipe);
	mutex_unlock(&config->ctx_lock);

	return err;
}

void dsi_dsr_enable(struct dsi_pipe *pipe)
{
	struct dsi_config *config = NULL;
	struct dsi_dsr *dsr = NULL;

	if (!pipe) {
		pr_err("%s: invalid DSI pipe", __func__);
		return;
	}

	config = &pipe->config;
	if (!config) {
		pr_err("%s: invalid DSI config", __func__);
		return;
	}

	dsr = config->dsr;

	pr_debug("%s\n", __func__);

	/*if no dsr attached, return 0*/
	if (!dsr)
		return;

	/*lock dsr*/
	mutex_lock(&config->ctx_lock);
	dsr->dsr_enabled = 1;
	dsr->dsr_state = DSR_EXITED;
	mutex_unlock(&config->ctx_lock);
}

static void __dbi_dsr_te_work(struct work_struct *work)
{
	struct dsi_dsr *dsr = container_of(work, struct dsi_dsr, te_work);
	struct dsi_config *config = (struct dsi_config *)dsr->config;
	struct dsi_pipe *pipe = container_of(config, struct dsi_pipe, config);

	dsi_dsr_report_te(pipe);
	dsi_dsr_allow(pipe);
}

/**
 * init dsr structure
 */
int dsi_dsr_init(struct dsi_pipe *pipe)
{
	struct dsi_dsr *dsr;

	if (!pipe)
		return -EINVAL;

	dsr = kzalloc(sizeof(*dsr), GFP_KERNEL);
	if (!dsr)
		return -ENOMEM;

	/*init reference count*/
	dsr->ref_count = 0;

	/*init free count*/
	dsr->free_count = 0;

	/*init dsr enabled*/
	dsr->dsr_enabled = 0;

	/*set dsr state*/
	dsr->dsr_state = DSR_INIT;

	/*init dsi config*/
	dsr->config = &pipe->config;

	pipe->config.dsr = dsr;

	INIT_WORK(&dsr->te_work, __dbi_dsr_te_work);

	return 0;
}

/**
 * destroy dsr structure
 */
void dsi_dsr_destroy(struct dsi_config *config)
{

	if (config) {
		kfree(config->dsr);
		config->dsr = NULL;
	}
}
