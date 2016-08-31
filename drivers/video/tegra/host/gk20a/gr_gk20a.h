/*
 * GK20A Graphics Engine
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __GR_GK20A_H__
#define __GR_GK20A_H__

#include <linux/slab.h>

#include "gr_ctx_gk20a.h"

#define GR_IDLE_CHECK_DEFAULT		100 /* usec */
#define GR_IDLE_CHECK_MAX		5000 /* usec */

#define INVALID_SCREEN_TILE_ROW_OFFSET	0xFFFFFFFF
#define INVALID_MAX_WAYS		0xFFFFFFFF

#define GK20A_FECS_UCODE_IMAGE	"fecs.bin"
#define GK20A_GPCCS_UCODE_IMAGE	"gpccs.bin"

enum /* global_ctx_buffer */ {
	CIRCULAR		= 0,
	PAGEPOOL		= 1,
	ATTRIBUTE		= 2,
	CIRCULAR_VPR		= 3,
	PAGEPOOL_VPR		= 4,
	ATTRIBUTE_VPR		= 5,
	GOLDEN_CTX		= 6,
	PRIV_ACCESS_MAP		= 7,
	NR_GLOBAL_CTX_BUF	= 8
};

/* either ATTRIBUTE or ATTRIBUTE_VPR maps to ATTRIBUTE_VA */
enum  /*global_ctx_buffer_va */ {
	CIRCULAR_VA		= 0,
	PAGEPOOL_VA		= 1,
	ATTRIBUTE_VA		= 2,
	GOLDEN_CTX_VA		= 3,
	PRIV_ACCESS_MAP_VA	= 4,
	NR_GLOBAL_CTX_BUF_VA	= 5
};

enum {
	WAIT_UCODE_LOOP,
	WAIT_UCODE_TIMEOUT,
	WAIT_UCODE_ERROR,
	WAIT_UCODE_OK
};

enum {
	GR_IS_UCODE_OP_EQUAL,
	GR_IS_UCODE_OP_NOT_EQUAL,
	GR_IS_UCODE_OP_AND,
	GR_IS_UCODE_OP_LESSER,
	GR_IS_UCODE_OP_LESSER_EQUAL,
	GR_IS_UCODE_OP_SKIP
};

enum {
	eUcodeHandshakeInitComplete = 1,
	eUcodeHandshakeMethodFinished
};

enum {
	ELCG_RUN,	/* clk always run, i.e. disable elcg */
	ELCG_STOP,	/* clk is stopped */
	ELCG_AUTO	/* clk will run when non-idle, standard elcg mode */
};

enum {
	BLCG_RUN,	/* clk always run, i.e. disable blcg */
	BLCG_AUTO	/* clk will run when non-idle, standard blcg mode */
};

#ifndef GR_GO_IDLE_BUNDLE
#define GR_GO_IDLE_BUNDLE	0x0000e100 /* --V-B */
#endif

struct gr_channel_map_tlb_entry {
	u32 curr_ctx;
	u32 hw_chid;
};

struct gr_zcull_gk20a {
	u32 aliquot_width;
	u32 aliquot_height;
	u32 aliquot_size;
	u32 total_aliquots;

	u32 width_align_pixels;
	u32 height_align_pixels;
	u32 pixel_squares_by_aliquots;
};

struct gr_zcull_info {
	u32 width_align_pixels;
	u32 height_align_pixels;
	u32 pixel_squares_by_aliquots;
	u32 aliquot_total;
	u32 region_byte_multiplier;
	u32 region_header_size;
	u32 subregion_header_size;
	u32 subregion_width_align_pixels;
	u32 subregion_height_align_pixels;
	u32 subregion_count;
};

#define GK20A_ZBC_COLOR_VALUE_SIZE	4  /* RGBA */

#define GK20A_STARTOF_ZBC_TABLE		1   /* index zero reserved to indicate "not ZBCd" */
#define GK20A_SIZEOF_ZBC_TABLE		16  /* match ltcs_ltss_dstg_zbc_index_address width (4) */
#define GK20A_ZBC_TABLE_SIZE		(16 - 1)

#define GK20A_ZBC_TYPE_INVALID		0
#define GK20A_ZBC_TYPE_COLOR		1
#define GK20A_ZBC_TYPE_DEPTH		2

struct zbc_color_table {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 format;
	u32 ref_cnt;
};

struct zbc_depth_table {
	u32 depth;
	u32 format;
	u32 ref_cnt;
};

struct zbc_entry {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 type;	/* color or depth */
	u32 format;
};

struct zbc_query_params {
	u32 color_ds[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[GK20A_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 ref_cnt;
	u32 format;
	u32 type;	/* color or depth */
	u32 index_size;	/* [out] size, [in] index */
};

struct gr_gk20a {
	struct gk20a *g;
	struct {
		bool dynamic;

		u32 buffer_size;
		u32 buffer_total_size;

		bool golden_image_initialized;
		u32 golden_image_size;
		u32 *local_golden_image;

		u32 zcull_ctxsw_image_size;

		u32 buffer_header_size;

		u32 priv_access_map_size;

		struct gr_ucode_gk20a ucode;

		struct av_list_gk20a  sw_bundle_init;
		struct av_list_gk20a  sw_method_init;
		struct aiv_list_gk20a sw_ctx_load;
		struct av_list_gk20a  sw_non_ctx_load;
		struct {
			struct aiv_list_gk20a sys;
			struct aiv_list_gk20a gpc;
			struct aiv_list_gk20a tpc;
			struct aiv_list_gk20a zcull_gpc;
			struct aiv_list_gk20a ppc;
			struct aiv_list_gk20a pm_sys;
			struct aiv_list_gk20a pm_gpc;
			struct aiv_list_gk20a pm_tpc;
		} ctxsw_regs;
		int regs_base_index;
		bool valid;
	} ctx_vars;

	struct mutex ctx_mutex; /* protect golden ctx init */
	struct mutex fecs_mutex; /* protect fecs method */

#define GR_NETLIST_DYNAMIC	-1
#define GR_NETLIST_STATIC_A	'A'
	int netlist;

	int initialized;
	u32 num_fbps;

	u32 max_gpc_count;
	u32 max_fbps_count;
	u32 max_tpc_per_gpc_count;
	u32 max_zcull_per_gpc_count;
	u32 max_tpc_count;

	u32 sys_count;
	u32 gpc_count;
	u32 pe_count_per_gpc;
	u32 ppc_count;
	u32 *gpc_ppc_count;
	u32 tpc_count;
	u32 *gpc_tpc_count;
	u32 zcb_count;
	u32 *gpc_zcb_count;
	u32 *pes_tpc_count[2];
	u32 *pes_tpc_mask[2];
	u32 *gpc_skip_mask;

	u32 bundle_cb_default_size;
	u32 min_gpm_fifo_depth;
	u32 bundle_cb_token_limit;
	u32 attrib_cb_default_size;
	u32 attrib_cb_size;
	u32 alpha_cb_default_size;
	u32 alpha_cb_size;
	u32 timeslice_mode;

	struct mem_desc global_ctx_buffer[NR_GLOBAL_CTX_BUF];

	struct mmu_desc mmu_wr_mem;
	u32 mmu_wr_mem_size;
	struct mmu_desc mmu_rd_mem;
	u32 mmu_rd_mem_size;

	u8 *map_tiles;
	u32 map_tile_count;
	u32 map_row_offset;

#define COMP_TAG_LINE_SIZE_SHIFT	(17)	/* one tag covers 128K */
#define COMP_TAG_LINE_SIZE		(1 << COMP_TAG_LINE_SIZE_SHIFT)

	u32 max_comptag_mem; /* max memory size (MB) for comptag */
	struct compbit_store_desc compbit_store;
	struct nvhost_allocator comp_tags;

	struct gr_zcull_gk20a zcull;

	struct zbc_color_table zbc_col_tbl[GK20A_ZBC_TABLE_SIZE];
	struct zbc_depth_table zbc_dep_tbl[GK20A_ZBC_TABLE_SIZE];

	s32 max_default_color_index;
	s32 max_default_depth_index;

	s32 max_used_color_index;
	s32 max_used_depth_index;

	u32 status_disable_mask;

#define GR_CHANNEL_MAP_TLB_SIZE		2 /* must of power of 2 */
	struct gr_channel_map_tlb_entry chid_tlb[GR_CHANNEL_MAP_TLB_SIZE];
	u32 channel_tlb_flush_index;
	spinlock_t ch_tlb_lock;

	void (*remove_support)(struct gr_gk20a *gr);
	bool sw_ready;
	bool skip_ucode_init;
};

void gk20a_fecs_dump_falcon_stats(struct gk20a *g);

struct gk20a_ctxsw_ucode_segment {
	u32 offset;
	u32 size;
};

struct gk20a_ctxsw_ucode_inst {
	u32 boot_entry;
	u32 boot_imem_offset;
	struct gk20a_ctxsw_ucode_segment boot;
	struct gk20a_ctxsw_ucode_segment code;
	struct gk20a_ctxsw_ucode_segment data;
};

struct gk20a_ctxsw_ucode_info {
	u64 *p_va;
	struct inst_desc inst_blk_desc;
	struct surface_mem_desc surface_desc;
	u64 ucode_gpuva;
	struct gk20a_ctxsw_ucode_inst fecs;
	struct gk20a_ctxsw_ucode_inst gpcs;
};

struct gk20a_ctxsw_bootloader_desc {
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
};

int gk20a_init_gr_support(struct gk20a *g);
void gk20a_gr_reset(struct gk20a *g);

int gk20a_init_gr_channel(struct channel_gk20a *ch_gk20a);

int gr_gk20a_init_ctx_vars(struct gk20a *g, struct gr_gk20a *gr);

struct nvhost_alloc_obj_ctx_args;
struct nvhost_free_obj_ctx_args;

int gk20a_alloc_obj_ctx(struct channel_gk20a *c,
			struct nvhost_alloc_obj_ctx_args *args);
int gk20a_free_obj_ctx(struct channel_gk20a *c,
			struct nvhost_free_obj_ctx_args *args);
void gk20a_free_channel_ctx(struct channel_gk20a *c);

int gk20a_gr_isr(struct gk20a *g);
int gk20a_gr_nonstall_isr(struct gk20a *g);

int gk20a_gr_clear_comptags(struct gk20a *g, u32 min, u32 max);
int gk20a_gr_flush_comptags(struct gk20a *g, bool invalidate);

/* zcull */
u32 gr_gk20a_get_ctxsw_zcull_size(struct gk20a *g, struct gr_gk20a *gr);
int gr_gk20a_bind_ctxsw_zcull(struct gk20a *g, struct gr_gk20a *gr,
			struct channel_gk20a *c, u64 zcull_va, u32 mode);
int gr_gk20a_get_zcull_info(struct gk20a *g, struct gr_gk20a *gr,
			struct gr_zcull_info *zcull_params);
/* zbc */
int gr_gk20a_add_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val);
int gr_gk20a_query_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_query_params *query_params);
int gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val);
/* pmu */
int gr_gk20a_fecs_get_reglist_img_size(struct gk20a *g, u32 *size);
int gr_gk20a_fecs_set_reglist_bind_inst(struct gk20a *g, phys_addr_t addr);
int gr_gk20a_fecs_set_reglist_virual_addr(struct gk20a *g, u64 pmu_va);

void gr_gk20a_init_elcg_mode(struct gk20a *g, u32 mode, u32 engine);
void gr_gk20a_init_blcg_mode(struct gk20a *g, u32 mode, u32 engine);

/* sm */
bool gk20a_gr_sm_debugger_attached(struct gk20a *g);

#define gr_gk20a_elpg_protected_call(g, func) \
	({ \
		int err; \
		if (support_gk20a_pmu()) { \
			mutex_lock(&g->pmu.pg_init_mutex); \
			gk20a_pmu_disable_elpg(g); \
		} \
		err = func; \
		if (support_gk20a_pmu()) { \
			gk20a_pmu_enable_elpg(g); \
			mutex_unlock(&g->pmu.pg_init_mutex); \
		} \
		err; \
	})

int gk20a_gr_suspend(struct gk20a *g);

struct nvhost_dbg_gpu_reg_op;
int gr_gk20a_exec_ctx_ops(struct channel_gk20a *ch,
			  struct nvhost_dbg_gpu_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops);
int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr,
				    u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets,
				    bool is_quad, u32 quad);
int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				 struct channel_gk20a *c,
				    bool enable_smpc_ctxsw);
#endif /*__GR_GK20A_H__*/
