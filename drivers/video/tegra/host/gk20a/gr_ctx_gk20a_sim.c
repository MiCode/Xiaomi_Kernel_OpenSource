/*
 * drivers/video/tegra/host/gk20a/gr_ctx_sim_gk20a.c
 *
 * GK20A Graphics Context for Simulation
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../dev.h"

#include "gk20a.h"
#include "gr_ctx_gk20a.h"

int gr_gk20a_init_ctx_vars_sim(struct gk20a *g, struct gr_gk20a *gr)
{
	int err = 0;
	u32 i, temp;
	char *size_path  = NULL;
	char *reg_path   = NULL;
	char *value_path = NULL;

	nvhost_dbg(dbg_fn | dbg_info,
		   "querying grctx info from chiplib");

	g->gr.ctx_vars.dynamic = true;
	g->gr.netlist = GR_NETLIST_DYNAMIC;

	/* query sizes and counts */
	gk20a_sim_esc_readl(g, "GRCTX_UCODE_INST_FECS_COUNT", 0,
			    &g->gr.ctx_vars.ucode.fecs.inst.count);
	gk20a_sim_esc_readl(g, "GRCTX_UCODE_DATA_FECS_COUNT", 0,
			    &g->gr.ctx_vars.ucode.fecs.data.count);
	gk20a_sim_esc_readl(g, "GRCTX_UCODE_INST_GPCCS_COUNT", 0,
			    &g->gr.ctx_vars.ucode.gpccs.inst.count);
	gk20a_sim_esc_readl(g, "GRCTX_UCODE_DATA_GPCCS_COUNT", 0,
			    &g->gr.ctx_vars.ucode.gpccs.data.count);
	gk20a_sim_esc_readl(g, "GRCTX_ALL_CTX_TOTAL_WORDS", 0, &temp);
	g->gr.ctx_vars.buffer_size = temp << 2;
	gk20a_sim_esc_readl(g, "GRCTX_SW_BUNDLE_INIT_SIZE", 0,
			    &g->gr.ctx_vars.sw_bundle_init.count);
	gk20a_sim_esc_readl(g, "GRCTX_SW_METHOD_INIT_SIZE", 0,
			    &g->gr.ctx_vars.sw_method_init.count);
	gk20a_sim_esc_readl(g, "GRCTX_SW_CTX_LOAD_SIZE", 0,
			    &g->gr.ctx_vars.sw_ctx_load.count);

	switch (0) { /*g->gr.ctx_vars.reg_init_override)*/
#if 0
	case NV_REG_STR_RM_GR_REG_INIT_OVERRIDE_PROD_DIFF:
		sizePath   = "GRCTX_NONCTXSW_PROD_DIFF_REG_SIZE";
		regPath    = "GRCTX_NONCTXSW_PROD_DIFF_REG:REG";
		valuePath  = "GRCTX_NONCTXSW_PROD_DIFF_REG:VALUE";
		break;
#endif
	default:
		size_path   = "GRCTX_NONCTXSW_REG_SIZE";
		reg_path    = "GRCTX_NONCTXSW_REG:REG";
		value_path  = "GRCTX_NONCTXSW_REG:VALUE";
		break;
	}

	gk20a_sim_esc_readl(g, size_path, 0,
			    &g->gr.ctx_vars.sw_non_ctx_load.count);

	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_SYS_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.sys.count);
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_GPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.gpc.count);
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_TPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.tpc.count);
#if 0
	/* looks to be unused, actually chokes the sim */
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.ppc.count);
#endif
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_ZCULL_GPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.zcull_gpc.count);
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_SYS_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.pm_sys.count);
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_GPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.pm_gpc.count);
	gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_TPC_COUNT", 0,
			    &g->gr.ctx_vars.ctxsw_regs.pm_tpc.count);

	err |= !alloc_u32_list_gk20a(&g->gr.ctx_vars.ucode.fecs.inst);
	err |= !alloc_u32_list_gk20a(&g->gr.ctx_vars.ucode.fecs.data);
	err |= !alloc_u32_list_gk20a(&g->gr.ctx_vars.ucode.gpccs.inst);
	err |= !alloc_u32_list_gk20a(&g->gr.ctx_vars.ucode.gpccs.data);
	err |= !alloc_av_list_gk20a(&g->gr.ctx_vars.sw_bundle_init);
	err |= !alloc_av_list_gk20a(&g->gr.ctx_vars.sw_method_init);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.sw_ctx_load);
	err |= !alloc_av_list_gk20a(&g->gr.ctx_vars.sw_non_ctx_load);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.sys);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.gpc);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.tpc);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.zcull_gpc);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.ppc);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.pm_sys);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.pm_gpc);
	err |= !alloc_aiv_list_gk20a(&g->gr.ctx_vars.ctxsw_regs.pm_tpc);

	if (err)
		goto fail;

	for (i = 0; i < g->gr.ctx_vars.ucode.fecs.inst.count; i++)
		gk20a_sim_esc_readl(g, "GRCTX_UCODE_INST_FECS",
				    i, &g->gr.ctx_vars.ucode.fecs.inst.l[i]);

	for (i = 0; i < g->gr.ctx_vars.ucode.fecs.data.count; i++)
		gk20a_sim_esc_readl(g, "GRCTX_UCODE_DATA_FECS",
				    i, &g->gr.ctx_vars.ucode.fecs.data.l[i]);

	for (i = 0; i < g->gr.ctx_vars.ucode.gpccs.inst.count; i++)
		gk20a_sim_esc_readl(g, "GRCTX_UCODE_INST_GPCCS",
				    i, &g->gr.ctx_vars.ucode.gpccs.inst.l[i]);

	for (i = 0; i < g->gr.ctx_vars.ucode.gpccs.data.count; i++)
		gk20a_sim_esc_readl(g, "GRCTX_UCODE_DATA_GPCCS",
				    i, &g->gr.ctx_vars.ucode.gpccs.data.l[i]);

	for (i = 0; i < g->gr.ctx_vars.sw_bundle_init.count; i++) {
		struct av_gk20a *l = g->gr.ctx_vars.sw_bundle_init.l;
		gk20a_sim_esc_readl(g, "GRCTX_SW_BUNDLE_INIT:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_SW_BUNDLE_INIT:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.sw_method_init.count; i++) {
		struct av_gk20a *l = g->gr.ctx_vars.sw_method_init.l;
		gk20a_sim_esc_readl(g, "GRCTX_SW_METHOD_INIT:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_SW_METHOD_INIT:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.sw_ctx_load.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.sw_ctx_load.l;
		gk20a_sim_esc_readl(g, "GRCTX_SW_CTX_LOAD:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_SW_CTX_LOAD:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_SW_CTX_LOAD:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.sw_non_ctx_load.count; i++) {
		struct av_gk20a *l = g->gr.ctx_vars.sw_non_ctx_load.l;
		gk20a_sim_esc_readl(g, reg_path, i, &l[i].addr);
		gk20a_sim_esc_readl(g, value_path, i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.sys.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.sys.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_SYS:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_SYS:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_SYS:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.gpc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.gpc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_GPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_GPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_GPC:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.tpc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.tpc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_TPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_TPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_TPC:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.ppc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.ppc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PPC:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.zcull_gpc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.zcull_gpc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_ZCULL_GPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_ZCULL_GPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_ZCULL_GPC:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.pm_sys.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.pm_sys.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_SYS:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_SYS:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_SYS:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.pm_gpc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.pm_gpc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_GPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_GPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_GPC:VALUE",
				    i, &l[i].value);
	}

	for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.pm_tpc.count; i++) {
		struct aiv_gk20a *l = g->gr.ctx_vars.ctxsw_regs.pm_tpc.l;
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_TPC:ADDR",
				    i, &l[i].addr);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_TPC:INDEX",
				    i, &l[i].index);
		gk20a_sim_esc_readl(g, "GRCTX_REG_LIST_PM_TPC:VALUE",
				    i, &l[i].value);
	}

	g->gr.ctx_vars.valid = true;

	gk20a_sim_esc_readl(g, "GRCTX_GEN_CTX_REGS_BASE_INDEX", 0,
			    &g->gr.ctx_vars.regs_base_index);

	nvhost_dbg(dbg_info | dbg_fn, "finished querying grctx info from chiplib");
	return 0;
fail:
	nvhost_err(dev_from_gk20a(g),
		   "failed querying grctx info from chiplib");
	return err;

}

