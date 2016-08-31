/*
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __GR_CTX_GK20A_H__
#define __GR_CTX_GK20A_H__


/* production netlist, one and only one from below */
/*#undef GK20A_NETLIST_IMAGE_FW_NAME*/
#define GK20A_NETLIST_IMAGE_FW_NAME GK20A_NETLIST_IMAGE_B
/* emulation netlists, match majorV with HW */
#define GK20A_NETLIST_IMAGE_A	"NETA_img.bin"
#define GK20A_NETLIST_IMAGE_B	"NETB_img.bin"
#define GK20A_NETLIST_IMAGE_C	"NETC_img.bin"
#define GK20A_NETLIST_IMAGE_D   "NETD_img.bin"

union __max_name {
#ifdef GK20A_NETLIST_IMAGE_A
	char __name_a[sizeof(GK20A_NETLIST_IMAGE_A)];
#endif
#ifdef GK20A_NETLIST_IMAGE_B
	char __name_b[sizeof(GK20A_NETLIST_IMAGE_B)];
#endif
#ifdef GK20A_NETLIST_IMAGE_C
	char __name_c[sizeof(GK20A_NETLIST_IMAGE_C)];
#endif
#ifdef GK20A_NETLIST_IMAGE_D
	char __name_d[sizeof(GK20A_NETLIST_IMAGE_D)];
#endif
};

#define MAX_NETLIST_NAME sizeof(union __max_name)

/* index for emulation netlists */
#define NETLIST_FINAL		-1
#define NETLIST_SLOT_A		0
#define NETLIST_SLOT_B		1
#define NETLIST_SLOT_C		2
#define NETLIST_SLOT_D		3
#define MAX_NETLIST		4

/* netlist regions */
#define NETLIST_REGIONID_FECS_UCODE_DATA	0
#define NETLIST_REGIONID_FECS_UCODE_INST	1
#define NETLIST_REGIONID_GPCCS_UCODE_DATA	2
#define NETLIST_REGIONID_GPCCS_UCODE_INST	3
#define NETLIST_REGIONID_SW_BUNDLE_INIT		4
#define NETLIST_REGIONID_SW_CTX_LOAD		5
#define NETLIST_REGIONID_SW_NON_CTX_LOAD	6
#define NETLIST_REGIONID_SW_METHOD_INIT		7
#define NETLIST_REGIONID_CTXREG_SYS		8
#define NETLIST_REGIONID_CTXREG_GPC		9
#define NETLIST_REGIONID_CTXREG_TPC		10
#define NETLIST_REGIONID_CTXREG_ZCULL_GPC	11
#define NETLIST_REGIONID_CTXREG_PM_SYS		12
#define NETLIST_REGIONID_CTXREG_PM_GPC		13
#define NETLIST_REGIONID_CTXREG_PM_TPC		14
#define NETLIST_REGIONID_MAJORV			15
#define NETLIST_REGIONID_BUFFER_SIZE		16
#define NETLIST_REGIONID_CTXSW_REG_BASE_INDEX	17
#define NETLIST_REGIONID_NETLIST_NUM		18
#define NETLIST_REGIONID_CTXREG_PPC		19
#define NETLIST_REGIONID_CTXREG_PMPPC		20

struct netlist_region {
	u32 region_id;
	u32 data_size;
	u32 data_offset;
};

struct netlist_image_header {
	u32 version;
	u32 regions;
};

struct netlist_image {
	struct netlist_image_header header;
	struct netlist_region regions[1];
};

struct av_gk20a {
	u32 addr;
	u32 value;
};
struct aiv_gk20a {
	u32 addr;
	u32 index;
	u32 value;
};
struct aiv_list_gk20a {
	struct aiv_gk20a *l;
	u32 count;
};
struct av_list_gk20a {
	struct av_gk20a *l;
	u32 count;
};
struct u32_list_gk20a {
	u32 *l;
	u32 count;
};

static inline
struct av_gk20a *alloc_av_list_gk20a(struct av_list_gk20a *avl)
{
	avl->l = kzalloc(avl->count * sizeof(*avl->l), GFP_KERNEL);
	return avl->l;
}

static inline
struct aiv_gk20a *alloc_aiv_list_gk20a(struct aiv_list_gk20a *aivl)
{
	aivl->l = kzalloc(aivl->count * sizeof(*aivl->l), GFP_KERNEL);
	return aivl->l;
}

static inline
u32 *alloc_u32_list_gk20a(struct u32_list_gk20a *u32l)
{
	u32l->l = kzalloc(u32l->count * sizeof(*u32l->l), GFP_KERNEL);
	return u32l->l;
}

struct gr_ucode_gk20a {
	struct {
		struct u32_list_gk20a inst;
		struct u32_list_gk20a data;
	} gpccs, fecs;
};

/* main entry for grctx loading */
int gr_gk20a_init_ctx_vars(struct gk20a *g, struct gr_gk20a *gr);
int gr_gk20a_init_ctx_vars_sim(struct gk20a *g, struct gr_gk20a *gr);

#endif /*__GR_CTX_GK20A_H__*/
