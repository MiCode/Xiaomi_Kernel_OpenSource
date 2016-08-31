/*
 * drivers/video/tegra/host/gk20a/gr_ctx_gk20a.c
 *
 * GK20A Graphics Context
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

#include <linux/firmware.h>

#include "dev.h"
#include "bus_client.h"

#include "gk20a.h"
#include "gr_ctx_gk20a.h"
#include "hw_gr_gk20a.h"

static int gr_gk20a_alloc_load_netlist_u32(u32 *src, u32 len,
			struct u32_list_gk20a *u32_list)
{
	u32_list->count = (len + sizeof(u32) - 1) / sizeof(u32);
	if (!alloc_u32_list_gk20a(u32_list))
		return -ENOMEM;

	memcpy(u32_list->l, src, len);

	return 0;
}

static int gr_gk20a_alloc_load_netlist_av(u32 *src, u32 len,
			struct av_list_gk20a *av_list)
{
	av_list->count = len / sizeof(struct av_gk20a);
	if (!alloc_av_list_gk20a(av_list))
		return -ENOMEM;

	memcpy(av_list->l, src, len);

	return 0;
}

static int gr_gk20a_alloc_load_netlist_aiv(u32 *src, u32 len,
			struct aiv_list_gk20a *aiv_list)
{
	aiv_list->count = len / sizeof(struct aiv_gk20a);
	if (!alloc_aiv_list_gk20a(aiv_list))
		return -ENOMEM;

	memcpy(aiv_list->l, src, len);

	return 0;
}

static int gr_gk20a_get_netlist_name(int index, char *name)
{
	switch (index) {
#ifdef GK20A_NETLIST_IMAGE_FW_NAME
	case NETLIST_FINAL:
		sprintf(name, GK20A_NETLIST_IMAGE_FW_NAME);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_A
	case NETLIST_SLOT_A:
		sprintf(name, GK20A_NETLIST_IMAGE_A);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_B
	case NETLIST_SLOT_B:
		sprintf(name, GK20A_NETLIST_IMAGE_B);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_C
	case NETLIST_SLOT_C:
		sprintf(name, GK20A_NETLIST_IMAGE_C);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_D
	case NETLIST_SLOT_D:
		sprintf(name, GK20A_NETLIST_IMAGE_D);
		return 0;
#endif
	default:
		return -1;
	}

	return -1;
}

static int gr_gk20a_init_ctx_vars_fw(struct gk20a *g, struct gr_gk20a *gr)
{
	struct device *d = dev_from_gk20a(g);
	const struct firmware *netlist_fw;
	struct netlist_image *netlist = NULL;
	char name[MAX_NETLIST_NAME];
	u32 i, major_v = ~0, major_v_hw, netlist_num;
	int net, max, err = -ENOENT;

	nvhost_dbg_fn("");

#ifdef GK20A_NETLIST_IMAGE_FW_NAME
	net = NETLIST_FINAL;
	max = 0;
	major_v_hw = ~0;
	g->gr.ctx_vars.dynamic = false;
#else
	net = NETLIST_SLOT_A;
	max = MAX_NETLIST;
	major_v_hw = gk20a_readl(g, gr_fecs_ctx_state_store_major_rev_id_r());
	g->gr.ctx_vars.dynamic = true;
#endif

	for (; net < max; net++) {

		if (gr_gk20a_get_netlist_name(net, name) != 0) {
			nvhost_warn(d, "invalid netlist index %d", net);
			continue;
		}

		netlist_fw = nvhost_client_request_firmware(g->dev, name);
		if (!netlist_fw) {
			nvhost_warn(d, "failed to load netlist %s", name);
			continue;
		}

		netlist = (struct netlist_image *)netlist_fw->data;

		for (i = 0; i < netlist->header.regions; i++) {
			u32 *src = (u32 *)((u8 *)netlist + netlist->regions[i].data_offset);
			u32 size = netlist->regions[i].data_size;

			switch (netlist->regions[i].region_id) {
			case NETLIST_REGIONID_FECS_UCODE_DATA:
				nvhost_dbg_info("NETLIST_REGIONID_FECS_UCODE_DATA");
				err = gr_gk20a_alloc_load_netlist_u32(
					src, size, &g->gr.ctx_vars.ucode.fecs.data);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_FECS_UCODE_INST:
				nvhost_dbg_info("NETLIST_REGIONID_FECS_UCODE_INST");
				err = gr_gk20a_alloc_load_netlist_u32(
					src, size, &g->gr.ctx_vars.ucode.fecs.inst);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_GPCCS_UCODE_DATA:
				nvhost_dbg_info("NETLIST_REGIONID_GPCCS_UCODE_DATA");
				err = gr_gk20a_alloc_load_netlist_u32(
					src, size, &g->gr.ctx_vars.ucode.gpccs.data);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_GPCCS_UCODE_INST:
				nvhost_dbg_info("NETLIST_REGIONID_GPCCS_UCODE_INST");
				err = gr_gk20a_alloc_load_netlist_u32(
					src, size, &g->gr.ctx_vars.ucode.gpccs.inst);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_SW_BUNDLE_INIT:
				nvhost_dbg_info("NETLIST_REGIONID_SW_BUNDLE_INIT");
				err = gr_gk20a_alloc_load_netlist_av(
					src, size, &g->gr.ctx_vars.sw_bundle_init);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_SW_METHOD_INIT:
				nvhost_dbg_info("NETLIST_REGIONID_SW_METHOD_INIT");
				err = gr_gk20a_alloc_load_netlist_av(
					src, size, &g->gr.ctx_vars.sw_method_init);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_SW_CTX_LOAD:
				nvhost_dbg_info("NETLIST_REGIONID_SW_CTX_LOAD");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.sw_ctx_load);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_SW_NON_CTX_LOAD:
				nvhost_dbg_info("NETLIST_REGIONID_SW_NON_CTX_LOAD");
				err = gr_gk20a_alloc_load_netlist_av(
					src, size, &g->gr.ctx_vars.sw_non_ctx_load);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_SYS:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_SYS");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.sys);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_GPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_GPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.gpc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_TPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_TPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.tpc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_ZCULL_GPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_ZCULL_GPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.zcull_gpc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_PPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_PPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.ppc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_PM_SYS:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_PM_SYS");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.pm_sys);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_PM_GPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_PM_GPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.pm_gpc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_CTXREG_PM_TPC:
				nvhost_dbg_info("NETLIST_REGIONID_CTXREG_PM_TPC");
				err = gr_gk20a_alloc_load_netlist_aiv(
					src, size, &g->gr.ctx_vars.ctxsw_regs.pm_tpc);
				if (err)
					goto clean_up;
				break;
			case NETLIST_REGIONID_BUFFER_SIZE:
				g->gr.ctx_vars.buffer_size = *src;
				nvhost_dbg_info("NETLIST_REGIONID_BUFFER_SIZE : %d",
					g->gr.ctx_vars.buffer_size);
				break;
			case NETLIST_REGIONID_CTXSW_REG_BASE_INDEX:
				g->gr.ctx_vars.regs_base_index = *src;
				nvhost_dbg_info("NETLIST_REGIONID_CTXSW_REG_BASE_INDEX : %d",
					g->gr.ctx_vars.regs_base_index);
				break;
			case NETLIST_REGIONID_MAJORV:
				major_v = *src;
				nvhost_dbg_info("NETLIST_REGIONID_MAJORV : %d",
					major_v);
				break;
			case NETLIST_REGIONID_NETLIST_NUM:
				netlist_num = *src;
				nvhost_dbg_info("NETLIST_REGIONID_NETLIST_NUM : %d",
					netlist_num);
				break;
			case NETLIST_REGIONID_CTXREG_PMPPC:
				nvhost_warn(d, "NETLIST_REGIONID_CTXREG_PMPPC skipped");
				break;
			default:
				nvhost_warn(d, "unrecognized region %d skipped", i);
				break;
			}
		}

		if (net != NETLIST_FINAL && major_v != major_v_hw) {
			nvhost_dbg_info("skip %s: major_v 0x%08x doesn't match hw 0x%08x",
				name, major_v, major_v_hw);
			goto clean_up;
		}

		g->gr.ctx_vars.valid = true;
		g->gr.netlist = net;

		release_firmware(netlist_fw);
		nvhost_dbg_fn("done");
		goto done;

clean_up:
		kfree(g->gr.ctx_vars.ucode.fecs.inst.l);
		kfree(g->gr.ctx_vars.ucode.fecs.data.l);
		kfree(g->gr.ctx_vars.ucode.gpccs.inst.l);
		kfree(g->gr.ctx_vars.ucode.gpccs.data.l);
		kfree(g->gr.ctx_vars.sw_bundle_init.l);
		kfree(g->gr.ctx_vars.sw_method_init.l);
		kfree(g->gr.ctx_vars.sw_ctx_load.l);
		kfree(g->gr.ctx_vars.sw_non_ctx_load.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.sys.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.gpc.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.tpc.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.zcull_gpc.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.ppc.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.pm_sys.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.pm_gpc.l);
		kfree(g->gr.ctx_vars.ctxsw_regs.pm_tpc.l);
		release_firmware(netlist_fw);
		err = -ENOENT;
	}

done:
	if (g->gr.ctx_vars.valid) {
		nvhost_dbg_info("netlist image %s loaded", name);
		return 0;
	} else {
		nvhost_err(d, "failed to load netlist image!!");
		return err;
	}
}

int gr_gk20a_init_ctx_vars(struct gk20a *g, struct gr_gk20a *gr)
{
	if (tegra_platform_is_linsim())
		return gr_gk20a_init_ctx_vars_sim(g, gr);
	else
		return gr_gk20a_init_ctx_vars_fw(g, gr);
}
