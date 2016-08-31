/*
 *
 * Tegra GK20A GPU Debugger Driver Register Ops
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>
#include <linux/err.h>
#include <linux/bsearch.h>
#include <linux/nvhost_dbg_gpu_ioctl.h>

#include "dev.h"
#include "nvhost_hwctx.h"
#include "gk20a.h"
#include "gr_gk20a.h"
#include "dbg_gpu_gk20a.h"
#include "regops_gk20a.h"



struct regop_offset_range {
	u32 base:24;
	u32 count:8;
};

static int regop_bsearch_range_cmp(const void *pkey, const void *pelem)
{
	u32 key = *(u32 *)pkey;
	struct regop_offset_range *prange = (struct regop_offset_range *)pelem;
	if (key < prange->base)
		return -1;
	else if (prange->base <= key && key < (prange->base +
					       (prange->count * 4)))
		return 0;
	return 1;
}

static inline bool linear_search(u32 offset, const u32 *list, int size)
{
	int i;
	for (i = 0; i < size; i++)
		if (list[i] == offset)
			return true;
	return false;
}

static const struct regop_offset_range gk20a_global_whitelist_ranges[] = {
	{ 0x000004f0,   1 },
	{ 0x00001a00,   3 },
	{ 0x0000259c,   1 },
	{ 0x0000280c,   1 },
	{ 0x00009400,   1 },
	{ 0x00009410,   1 },
	{ 0x00020200,   1 },
	{ 0x00022430,   7 },
	{ 0x00022548,   1 },
	{ 0x00100c18,   3 },
	{ 0x00100c84,   1 },
	{ 0x00100cc4,   1 },
	{ 0x00106640,   1 },
	{ 0x0010a0a8,   1 },
	{ 0x0010a4f0,   1 },
	{ 0x0010e064,   1 },
	{ 0x0010e164,   1 },
	{ 0x0010e490,   1 },
	{ 0x00110100,   1 },
	{ 0x00140028,   1 },
	{ 0x001408dc,   1 },
	{ 0x00140a5c,   1 },
	{ 0x001410dc,   1 },
	{ 0x0014125c,   1 },
	{ 0x0017e028,   1 },
	{ 0x0017e8dc,   1 },
	{ 0x0017ea5c,   1 },
	{ 0x0017f0dc,   1 },
	{ 0x0017f25c,   1 },
	{ 0x00180000,  68 },
	{ 0x00180200,  68 },
	{ 0x001a0000,  68 },
	{ 0x001b0000,  68 },
	{ 0x001b0200,  68 },
	{ 0x001b0400,  68 },
	{ 0x001b0600,  68 },
	{ 0x001b4000,   3 },
	{ 0x001b4010,   3 },
	{ 0x001b4020,   3 },
	{ 0x001b4040,   3 },
	{ 0x001b4050,   3 },
	{ 0x001b4060,  16 },
	{ 0x001b40a4,   1 },
	{ 0x001b4100,   6 },
	{ 0x001b4124,   2 },
	{ 0x001b8000,   7 },
	{ 0x001bc000,   7 },
	{ 0x001be000,   7 },
	{ 0x00400500,   1 },
	{ 0x0040415c,   1 },
	{ 0x00405850,   1 },
	{ 0x00405908,   1 },
	{ 0x00405b40,   1 },
	{ 0x00405b50,   1 },
	{ 0x00406024,   1 },
	{ 0x00407010,   1 },
	{ 0x00407808,   1 },
	{ 0x0040803c,   1 },
	{ 0x0040880c,   1 },
	{ 0x00408910,   1 },
	{ 0x00408984,   1 },
	{ 0x004090a8,   1 },
	{ 0x004098a0,   1 },
	{ 0x0041000c,   1 },
	{ 0x00410110,   1 },
	{ 0x00410184,   1 },
	{ 0x00418384,   1 },
	{ 0x004184a0,   1 },
	{ 0x00418604,   1 },
	{ 0x00418680,   1 },
	{ 0x00418714,   1 },
	{ 0x0041881c,   1 },
	{ 0x004188c8,   2 },
	{ 0x00418b04,   1 },
	{ 0x00418c04,   1 },
	{ 0x00418c64,   2 },
	{ 0x00418c88,   1 },
	{ 0x00418cb4,   2 },
	{ 0x00418d00,   1 },
	{ 0x00418d28,   2 },
	{ 0x00418e08,   1 },
	{ 0x00418e1c,   2 },
	{ 0x00418f08,   1 },
	{ 0x00418f20,   2 },
	{ 0x00419000,   1 },
	{ 0x0041900c,   1 },
	{ 0x00419018,   1 },
	{ 0x00419854,   1 },
	{ 0x00419ab0,   1 },
	{ 0x00419ab8,   3 },
	{ 0x00419ac8,   1 },
	{ 0x00419c0c,   1 },
	{ 0x00419c8c,   3 },
	{ 0x00419ca8,   1 },
	{ 0x00419d08,   2 },
	{ 0x00419e00,   1 },
	{ 0x00419e0c,   1 },
	{ 0x00419e14,   2 },
	{ 0x00419e24,   2 },
	{ 0x00419e34,   2 },
	{ 0x00419e44,   4 },
	{ 0x00419ea4,   1 },
	{ 0x00419eb0,   1 },
	{ 0x0041a0a0,   1 },
	{ 0x0041a0a8,   1 },
	{ 0x0041a17c,   1 },
	{ 0x0041a890,   2 },
	{ 0x0041a8a0,   3 },
	{ 0x0041a8b0,   2 },
	{ 0x0041b014,   1 },
	{ 0x0041b0a0,   1 },
	{ 0x0041b0cc,   1 },
	{ 0x0041b0e8,   2 },
	{ 0x0041b1dc,   1 },
	{ 0x0041b1f8,   2 },
	{ 0x0041be14,   1 },
	{ 0x0041bea0,   1 },
	{ 0x0041becc,   1 },
	{ 0x0041bee8,   2 },
	{ 0x0041bfdc,   1 },
	{ 0x0041bff8,   2 },
	{ 0x0041c054,   1 },
	{ 0x0041c2b0,   1 },
	{ 0x0041c2b8,   3 },
	{ 0x0041c2c8,   1 },
	{ 0x0041c40c,   1 },
	{ 0x0041c48c,   3 },
	{ 0x0041c4a8,   1 },
	{ 0x0041c508,   2 },
	{ 0x0041c600,   1 },
	{ 0x0041c60c,   1 },
	{ 0x0041c614,   2 },
	{ 0x0041c624,   2 },
	{ 0x0041c634,   2 },
	{ 0x0041c644,   4 },
	{ 0x0041c6a4,   1 },
	{ 0x0041c6b0,   1 },
	{ 0x00500384,   1 },
	{ 0x005004a0,   1 },
	{ 0x00500604,   1 },
	{ 0x00500680,   1 },
	{ 0x00500714,   1 },
	{ 0x0050081c,   1 },
	{ 0x005008c8,   2 },
	{ 0x00500b04,   1 },
	{ 0x00500c04,   1 },
	{ 0x00500c64,   2 },
	{ 0x00500c88,   1 },
	{ 0x00500cb4,   2 },
	{ 0x00500d00,   1 },
	{ 0x00500d28,   2 },
	{ 0x00500e08,   1 },
	{ 0x00500e1c,   2 },
	{ 0x00500f08,   1 },
	{ 0x00500f20,   2 },
	{ 0x00501000,   1 },
	{ 0x0050100c,   1 },
	{ 0x00501018,   1 },
	{ 0x00501854,   1 },
	{ 0x00501ab0,   1 },
	{ 0x00501ab8,   3 },
	{ 0x00501ac8,   1 },
	{ 0x00501c0c,   1 },
	{ 0x00501c8c,   3 },
	{ 0x00501ca8,   1 },
	{ 0x00501d08,   2 },
	{ 0x00501e00,   1 },
	{ 0x00501e0c,   1 },
	{ 0x00501e14,   2 },
	{ 0x00501e24,   2 },
	{ 0x00501e34,   2 },
	{ 0x00501e44,   4 },
	{ 0x00501ea4,   1 },
	{ 0x00501eb0,   1 },
	{ 0x005020a0,   1 },
	{ 0x005020a8,   1 },
	{ 0x0050217c,   1 },
	{ 0x00502890,   2 },
	{ 0x005028a0,   3 },
	{ 0x005028b0,   2 },
	{ 0x00503014,   1 },
	{ 0x005030a0,   1 },
	{ 0x005030cc,   1 },
	{ 0x005030e8,   2 },
	{ 0x005031dc,   1 },
	{ 0x005031f8,   2 },
	{ 0x00503e14,   1 },
	{ 0x00503ea0,   1 },
	{ 0x00503ecc,   1 },
	{ 0x00503ee8,   2 },
	{ 0x00503fdc,   1 },
	{ 0x00503ff8,   2 },
	{ 0x00504054,   1 },
	{ 0x005042b0,   1 },
	{ 0x005042b8,   3 },
	{ 0x005042c8,   1 },
	{ 0x0050440c,   1 },
	{ 0x0050448c,   3 },
	{ 0x005044a8,   1 },
	{ 0x00504508,   2 },
	{ 0x00504600,   1 },
	{ 0x0050460c,   1 },
	{ 0x00504614,   2 },
	{ 0x00504624,   2 },
	{ 0x00504634,   2 },
	{ 0x00504644,   4 },
	{ 0x005046a4,   1 },
	{ 0x005046b0,   1 },
};
static const u32 gk20a_global_whitelist_ranges_count =
	ARRAY_SIZE(gk20a_global_whitelist_ranges);

/* context */

static const struct regop_offset_range gk20a_context_whitelist_ranges[] = {
	{ 0x0000280c,   1 },
	{ 0x00100cc4,   1 },
	{ 0x00400500,   1 },
	{ 0x00405b40,   1 },
	{ 0x00419000,   1 },
	{ 0x00419c8c,   3 },
	{ 0x00419d08,   2 },
	{ 0x00419e04,   3 },
	{ 0x00419e14,   2 },
	{ 0x00419e24,   2 },
	{ 0x00419e34,   2 },
	{ 0x00419e44,   4 },
	{ 0x00419e58,   6 },
	{ 0x00419e84,   5 },
	{ 0x00419ea4,   1 },
	{ 0x00419eac,   2 },
	{ 0x00419f30,   8 },
	{ 0x0041c48c,   3 },
	{ 0x0041c508,   2 },
	{ 0x0041c604,   3 },
	{ 0x0041c614,   2 },
	{ 0x0041c624,   2 },
	{ 0x0041c634,   2 },
	{ 0x0041c644,   4 },
	{ 0x0041c658,   6 },
	{ 0x0041c684,   5 },
	{ 0x0041c6a4,   1 },
	{ 0x0041c6ac,   2 },
	{ 0x0041c730,   8 },
	{ 0x00501000,   1 },
	{ 0x00501c8c,   3 },
	{ 0x00501d08,   2 },
	{ 0x00501e04,   3 },
	{ 0x00501e14,   2 },
	{ 0x00501e24,   2 },
	{ 0x00501e34,   2 },
	{ 0x00501e44,   4 },
	{ 0x00501e58,   6 },
	{ 0x00501e84,   5 },
	{ 0x00501ea4,   1 },
	{ 0x00501eac,   2 },
	{ 0x00501f30,   8 },
	{ 0x0050448c,   3 },
	{ 0x00504508,   2 },
	{ 0x00504604,   3 },
	{ 0x00504614,   2 },
	{ 0x00504624,   2 },
	{ 0x00504634,   2 },
	{ 0x00504644,   4 },
	{ 0x00504658,   6 },
	{ 0x00504684,   5 },
	{ 0x005046a4,   1 },
	{ 0x005046ac,   2 },
	{ 0x00504730,   8 },
};
static const u32 gk20a_context_whitelist_ranges_count =
	ARRAY_SIZE(gk20a_context_whitelist_ranges);

/* runcontrol */
static const u32 gk20a_runcontrol_whitelist[] = {
	0x00419e10,
	0x0041c610,
	0x00501e10,
	0x00504610,
};
static const u32 gk20a_runcontrol_whitelist_count =
	ARRAY_SIZE(gk20a_runcontrol_whitelist);

static const struct regop_offset_range gk20a_runcontrol_whitelist_ranges[] = {
	{ 0x00419e10,   1 },
	{ 0x0041c610,   1 },
	{ 0x00501e10,   1 },
	{ 0x00504610,   1 },
};
static const u32 gk20a_runcontrol_whitelist_ranges_count =
	ARRAY_SIZE(gk20a_runcontrol_whitelist_ranges);


/* quad ctl */
static const u32 gk20a_qctl_whitelist[] = {
	0x00504670,
    0x00504674,
    0x00504678,
    0x0050467c,
    0x00504680,
	0x00504730,
	0x00504734,
	0x00504738,
	0x0050473c,
};
static const u32 gk20a_qctl_whitelist_count =
	ARRAY_SIZE(gk20a_qctl_whitelist);

static const struct regop_offset_range gk20a_qctl_whitelist_ranges[] = {
	{ 0x00504670,   1 },
	{ 0x00504730,   4 },
};
static const u32 gk20a_qctl_whitelist_ranges_count =
	ARRAY_SIZE(gk20a_qctl_whitelist_ranges);




static bool validate_reg_ops(struct dbg_session_gk20a *dbg_s,
			     u32 *ctx_rd_count, u32 *ctx_wr_count,
			     struct nvhost_dbg_gpu_reg_op *ops,
			     u32 op_count);


int exec_regops_gk20a(struct dbg_session_gk20a *dbg_s,
		      struct nvhost_dbg_gpu_reg_op *ops,
		      u64 num_ops)
{
	int err = 0, i;
	struct channel_gk20a *ch = NULL;
	struct gk20a *g = dbg_s->g;
	/*struct gr_gk20a *gr = &g->gr;*/
	u32 data32_lo = 0, data32_hi = 0;
	u32 ctx_rd_count = 0, ctx_wr_count = 0;
	bool skip_read_lo, skip_read_hi;
	bool ok;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	ch = dbg_s->ch;

	ok = validate_reg_ops(dbg_s,
			      &ctx_rd_count, &ctx_wr_count,
			      ops, num_ops);
	if (!ok) {
		dev_err(dbg_s->dev, "invalid op(s)");
		err = -EINVAL;
		/* each op has its own err/status */
		goto clean_up;
	}

	for (i = 0; i < num_ops; i++) {
		/* if it isn't global then it is done in the ctx ops... */
		if (ops[i].type != REGOP(TYPE_GLOBAL))
			continue;

		switch (ops[i].op) {

		case REGOP(READ_32):
			ops[i].value_hi = 0;
			ops[i].value_lo = gk20a_readl(g, ops[i].offset);
			nvhost_dbg(dbg_gpu_dbg, "read_32 0x%08x from 0x%08x",
				   ops[i].value_lo, ops[i].offset);

			break;

		case REGOP(READ_64):
			ops[i].value_lo = gk20a_readl(g, ops[i].offset);
			ops[i].value_hi =
				gk20a_readl(g, ops[i].offset + 4);

			nvhost_dbg(dbg_gpu_dbg, "read_64 0x%08x:%08x from 0x%08x",
				   ops[i].value_hi, ops[i].value_lo,
				   ops[i].offset);
		break;

		case REGOP(WRITE_32):
		case REGOP(WRITE_64):
			/* some of this appears wonky/unnecessary but
			   we've kept it for compat with existing
			   debugger code.  just in case... */
			skip_read_lo = skip_read_hi = false;
			if (ops[i].and_n_mask_lo == ~(u32)0) {
				data32_lo = ops[i].value_lo;
				skip_read_lo = true;
			}

			if ((ops[i].op == REGOP(WRITE_64)) &&
			    (ops[i].and_n_mask_hi == ~(u32)0)) {
				data32_hi = ops[i].value_hi;
				skip_read_hi = true;
			}

			/* read first 32bits */
			if (unlikely(skip_read_lo == false)) {
				data32_lo = gk20a_readl(g, ops[i].offset);
				data32_lo &= ~ops[i].and_n_mask_lo;
				data32_lo |= ops[i].value_lo;
			}

			/* if desired, read second 32bits */
			if ((ops[i].op == REGOP(WRITE_64)) &&
			    !skip_read_hi) {
				data32_hi = gk20a_readl(g, ops[i].offset + 4);
				data32_hi &= ~ops[i].and_n_mask_hi;
				data32_hi |= ops[i].value_hi;
			}

			/* now update first 32bits */
			gk20a_writel(g, ops[i].offset, data32_lo);
			nvhost_dbg(dbg_gpu_dbg, "Wrote 0x%08x to 0x%08x ",
				   data32_lo, ops[i].offset);
			/* if desired, update second 32bits */
			if (ops[i].op == REGOP(WRITE_64)) {
				gk20a_writel(g, ops[i].offset + 4, data32_hi);
				nvhost_dbg(dbg_gpu_dbg, "Wrote 0x%08x to 0x%08x ",
					   data32_hi, ops[i].offset + 4);

			}


			break;

		/* shouldn't happen as we've already screened */
		default:
			BUG();
			err = -EINVAL;
			goto clean_up;
			break;
		}
	}

	if (ctx_wr_count | ctx_rd_count) {
		err = gr_gk20a_exec_ctx_ops(ch, ops, num_ops,
					    ctx_wr_count, ctx_rd_count);
		if (err) {
			dev_warn(dbg_s->dev,
				 "failed to perform ctx ops\n");
			goto clean_up;
		}
	}

 clean_up:
	nvhost_dbg(dbg_gpu_dbg, "ret=%d", err);
	return err;

}


static int validate_reg_op_info(struct dbg_session_gk20a *dbg_s,
				struct nvhost_dbg_gpu_reg_op *op)
{
	int err = 0;

	op->status = REGOP(STATUS_SUCCESS);

	switch (op->op) {
	case REGOP(READ_32):
	case REGOP(READ_64):
	case REGOP(WRITE_32):
	case REGOP(WRITE_64):
		break;
	default:
		op->status |= REGOP(STATUS_UNSUPPORTED_OP);
		/*nvhost_err(dbg_s->dev, "Invalid regops op %d!", op->op);*/
		err = -EINVAL;
		break;
	}

	switch (op->type) {
	case REGOP(TYPE_GLOBAL):
	case REGOP(TYPE_GR_CTX):
	case REGOP(TYPE_GR_CTX_TPC):
	case REGOP(TYPE_GR_CTX_SM):
	case REGOP(TYPE_GR_CTX_CROP):
	case REGOP(TYPE_GR_CTX_ZROP):
	case REGOP(TYPE_GR_CTX_QUAD):
		break;
	/*
	case NVHOST_DBG_GPU_REG_OP_TYPE_FB:
	*/
	default:
		op->status |= REGOP(STATUS_INVALID_TYPE);
		/*nvhost_err(dbg_s->dev, "Invalid regops type %d!", op->type);*/
		err = -EINVAL;
		break;
	}

	return err;
}


/* note: the op here has already been through validate_reg_op_info */
static int validate_reg_op_offset(struct dbg_session_gk20a *dbg_s,
				  struct nvhost_dbg_gpu_reg_op *op)
{
	int err;
	u32 buf_offset_lo, buf_offset_addr, num_offsets, offset;
	bool valid = false;

	op->status = 0;
	offset = op->offset;

	/* support only 24-bit 4-byte aligned offsets */
	if (offset & 0xFF000003) {
		nvhost_err(dbg_s->dev, "invalid regop offset: 0x%x\n", offset);
		op->status |= REGOP(STATUS_INVALID_OFFSET);
		return -EINVAL;
	}

	if (op->type == REGOP(TYPE_GLOBAL)) {
		/* search global list */
		valid = !!bsearch(&offset,
				  gk20a_global_whitelist_ranges,
				  gk20a_global_whitelist_ranges_count,
				  sizeof(*gk20a_global_whitelist_ranges),
				  regop_bsearch_range_cmp);

		/* if debug session and channel is bound search context list */
		if ((!valid) && (!dbg_s->is_profiler && dbg_s->ch)) {
			/* binary search context list */
			valid = !!bsearch(&offset,
					  gk20a_context_whitelist_ranges,
					  gk20a_context_whitelist_ranges_count,
					  sizeof(*gk20a_context_whitelist_ranges),
					  regop_bsearch_range_cmp);
		}

		/* if debug session and channel is bound search runcontrol list */
		if ((!valid) && (!dbg_s->is_profiler && dbg_s->ch)) {
			valid = linear_search(offset,
					      gk20a_runcontrol_whitelist,
					      gk20a_runcontrol_whitelist_count);
		}
	} else if (op->type == REGOP(TYPE_GR_CTX)) {
		/* it's a context-relative op */
		if (!dbg_s->ch) {
			nvhost_err(dbg_s->dev, "can't perform ctx regop unless bound");
			op->status = REGOP(STATUS_UNSUPPORTED_OP);
			return -ENODEV;
		}

		/* binary search context list */
		valid = !!bsearch(&offset,
				  gk20a_context_whitelist_ranges,
				  gk20a_context_whitelist_ranges_count,
				  sizeof(*gk20a_context_whitelist_ranges),
				  regop_bsearch_range_cmp);

		/* if debug session and channel is bound search runcontrol list */
		if ((!valid) && (!dbg_s->is_profiler && dbg_s->ch)) {
			valid = linear_search(offset,
					      gk20a_runcontrol_whitelist,
					      gk20a_runcontrol_whitelist_count);
		}

	} else if (op->type == REGOP(TYPE_GR_CTX_QUAD)) {
		valid = linear_search(offset,
				      gk20a_qctl_whitelist,
				      gk20a_qctl_whitelist_count);
	}

	if (valid && (op->type != REGOP(TYPE_GLOBAL))) {
			err = gr_gk20a_get_ctx_buffer_offsets(dbg_s->g,
							      op->offset,
							      1,
							      &buf_offset_lo,
							      &buf_offset_addr,
							      &num_offsets,
							      op->type == REGOP(TYPE_GR_CTX_QUAD),
							      op->quad);
			if (err) {
				op->status |= REGOP(STATUS_INVALID_OFFSET);
				return -EINVAL;
			}
			if (!buf_offset_lo) {
				op->status |= REGOP(STATUS_INVALID_OFFSET);
				return -EINVAL;
			}
	}

	if (!valid) {
		nvhost_err(dbg_s->dev, "invalid regop offset: 0x%x\n", offset);
		op->status |= REGOP(STATUS_INVALID_OFFSET);
		return -EINVAL;
	}

	return 0;
}

static bool validate_reg_ops(struct dbg_session_gk20a *dbg_s,
			    u32 *ctx_rd_count, u32 *ctx_wr_count,
			    struct nvhost_dbg_gpu_reg_op *ops,
			    u32 op_count)
{
	u32 i;
	int err;
	bool ok = true;

	/* keep going until the end so every op can get
	 * a separate error code if needed */
	for (i = 0; i < op_count; i++) {

		err = validate_reg_op_info(dbg_s, &ops[i]);
		ok &= !err;

		if (reg_op_is_gr_ctx(ops[i].type)) {
			if (reg_op_is_read(ops[i].op))
				(*ctx_rd_count)++;
			else
				(*ctx_wr_count)++;
		}

		err = validate_reg_op_offset(dbg_s, &ops[i]);
		ok &= !err;
	}

	nvhost_dbg(dbg_gpu_dbg, "ctx_wrs:%d ctx_rds:%d\n",
		   *ctx_wr_count, *ctx_rd_count);

	return ok;
}

/* exported for tools like cyclestats, etc */
bool is_bar0_global_offset_whitelisted_gk20a(u32 offset)
{

	bool valid = !!bsearch(&offset,
			       gk20a_global_whitelist_ranges,
			       gk20a_global_whitelist_ranges_count,
			       sizeof(*gk20a_global_whitelist_ranges),
			       regop_bsearch_range_cmp);
	return valid;
}
