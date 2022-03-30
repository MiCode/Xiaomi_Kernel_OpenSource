// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "reviser_cmn.h"
#include "reviser_hw_cmn.h"
#include "reviser_hw_mgt.h"

struct reviser_hw_mgr {

	struct reviser_hw_ops hops;
};

static struct reviser_hw_mgr g_rvr_hw_mgr;

void *reviser_hw_mgt_get_cb(void)
{
	return (void *) &g_rvr_hw_mgr.hops;
}

void reviser_mgt_dmp_boundary(void *drvinfo, void *s_file)
{
	if (g_rvr_hw_mgr.hops.dmp_boundary == NULL) {
		LOG_WARN("ByPass\n");
		return;
	}
	g_rvr_hw_mgr.hops.dmp_boundary(drvinfo, s_file);
}
void reviser_mgt_dmp_ctx(void *drvinfo, void *s_file)
{
	if (g_rvr_hw_mgr.hops.dmp_ctx == NULL) {
		LOG_WARN("ByPass\n");
		return;
	}
	g_rvr_hw_mgr.hops.dmp_ctx(drvinfo, s_file);
}
void reviser_mgt_dmp_rmp(void *drvinfo, void *s_file)
{
	if (g_rvr_hw_mgr.hops.dmp_rmp == NULL) {
		LOG_WARN("ByPass\n");
		return;
	}
	g_rvr_hw_mgr.hops.dmp_rmp(drvinfo, s_file);
}
void reviser_mgt_dmp_default(void *drvinfo, void *s_file)
{
	if (g_rvr_hw_mgr.hops.dmp_default == NULL) {
		LOG_WARN("ByPass\n");
		return;
	}
	g_rvr_hw_mgr.hops.dmp_default(drvinfo, s_file);
}
void reviser_mgt_dmp_exception(void *drvinfo, void *s_file)
{
	if (g_rvr_hw_mgr.hops.dmp_exception == NULL) {
		LOG_WARN("ByPass\n");
		return;
	}
	g_rvr_hw_mgr.hops.dmp_exception(drvinfo, s_file);
}
int reviser_mgt_set_boundary(void *drvinfo, uint8_t boundary)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.set_boundary == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}
	ret = g_rvr_hw_mgr.hops.set_boundary(drvinfo, boundary);

	return ret;
}
int reviser_mgt_set_default(void *drvinfo)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.set_default == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}
	ret = g_rvr_hw_mgr.hops.set_default(drvinfo);

	return ret;
}
int reviser_mgt_set_ctx(void *drvinfo, int type, int index, uint8_t ctx)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.set_ctx == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}
	ret = g_rvr_hw_mgr.hops.set_ctx(drvinfo, type, index, ctx);

	return ret;
}
int reviser_mgt_set_rmp(void *drvinfo, int index, uint8_t valid, uint8_t ctx,
		uint8_t src_page, uint8_t dst_page)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.set_rmp == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}
	ret = g_rvr_hw_mgr.hops.set_rmp(drvinfo, index, valid, ctx, src_page, dst_page);

	return ret;
}

int reviser_mgt_isr_cb(void *drvinfo)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.isr_cb == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}
	ret = g_rvr_hw_mgr.hops.isr_cb(drvinfo);

	return ret;
}

int reviser_mgt_set_int(void *drvinfo, uint8_t enable)
{
	int ret = 0;

	if (g_rvr_hw_mgr.hops.set_int == NULL) {
		LOG_WARN("ByPass\n");
		return ret;
	}


	ret = g_rvr_hw_mgr.hops.set_int(drvinfo, enable);

	return ret;
}
