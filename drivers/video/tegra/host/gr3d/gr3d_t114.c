/*
 * drivers/video/tegra/host/t20/3dctx_t114.c
 *
 * Tegra Graphics Host 3d hardware context
 *
 * Copyright (c) 2011-2012 NVIDIA Corporation.
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

#include "nvhost_hwctx.h"
#include "nvhost_channel.h"
#include "dev.h"
#include "host1x/host1x02_hardware.h"
#include "gr3d.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"
#include "scale3d_actmon.h"
#include "nvhost_job.h"
#include "nvhost_acm.h"
#include "class_ids.h"

#include <linux/slab.h>
#include <linux/scatterlist.h>

static const struct hwctx_reginfo ctxsave_regs_3d_per_pipe[] = {
	HWCTX_REGINFO(0xc30,    1, DIRECT),
	HWCTX_REGINFO(0xc40,    1, DIRECT),
	HWCTX_REGINFO(0xc50,    1, DIRECT)
};

static const struct hwctx_reginfo ctxsave_regs_3d_global[] = {
	/* bug 962360. Reg 0xe44 has to be the first one to be restored*/
	HWCTX_REGINFO_RST(0x40e, 1, DIRECT, 0xe44),
	HWCTX_REGINFO(0xe00,   35, DIRECT),
	HWCTX_REGINFO(0xe25,    2, DIRECT),
	HWCTX_REGINFO(0xe28,    2, DIRECT),
	HWCTX_REGINFO(0x001,    2, DIRECT),
	HWCTX_REGINFO(0x00c,   10, DIRECT),
	HWCTX_REGINFO(0x100,   34, DIRECT),
	HWCTX_REGINFO(0x124,    2, DIRECT),
	HWCTX_REGINFO(0x200,    5, DIRECT),
	HWCTX_REGINFO(0x205, 1024, INDIRECT),
	HWCTX_REGINFO(0x207, 1120, INDIRECT),
	HWCTX_REGINFO(0x209,    1, DIRECT),
	HWCTX_REGINFO(0x20b,    1, DIRECT),
	HWCTX_REGINFO(0x300,   64, DIRECT),
	HWCTX_REGINFO(0x343,   25, DIRECT),
	HWCTX_REGINFO(0x363,    2, DIRECT),
	HWCTX_REGINFO(0x400,    3, DIRECT),
	/* bug 976976 requires reg 0x403 to be restored before reg 0xe45 */
	/* bug 972588 requires reg 0x403 to be restored with reg 0x411's
	   value */
	HWCTX_REGINFO_RST(0x411, 1, DIRECT, 0x403),
	HWCTX_REGINFO(0x404,   15, DIRECT),
	/* bug 955371 requires reg 0x7e0 to be restored with 0x410,s value.
	   bug 982750 requires reg 0x7e0 to be restored before 0x804.
	   note: 0x803 is the offset reg for 0x804 */
	HWCTX_REGINFO_RST(0x410, 1, DIRECT, 0x7e0),
	HWCTX_REGINFO(0x414,    7, DIRECT),
	HWCTX_REGINFO(0x434,    1, DIRECT),
	HWCTX_REGINFO(0x500,    4, DIRECT),
	HWCTX_REGINFO(0x520,   32, DIRECT),
	HWCTX_REGINFO(0x540,   64, INDIRECT),
	HWCTX_REGINFO(0x545,    1, DIRECT),
	HWCTX_REGINFO(0x547,    1, DIRECT),
	HWCTX_REGINFO(0x548,   64, INDIRECT),
	/* bug 951938 requires that reg 601 should not be the last reg to be
	   saved */
	HWCTX_REGINFO(0x600,   16, INDIRECT_4X),
	HWCTX_REGINFO(0x603,  128, INDIRECT),
	HWCTX_REGINFO(0x608,    4, DIRECT),
	HWCTX_REGINFO(0x60e,    1, DIRECT),
	HWCTX_REGINFO(0x700,   64, INDIRECT),
	HWCTX_REGINFO(0x710,   50, DIRECT),
	HWCTX_REGINFO(0x750,   16, DIRECT),
	HWCTX_REGINFO(0x770,   48, DIRECT),
	HWCTX_REGINFO(0x7e0,    1, DIRECT),
	HWCTX_REGINFO(0x800,   64, INDIRECT),
	/* bug 982750 requires 0x804 to be restored after reg 0x7e0 */
	HWCTX_REGINFO(0x803, 1024, INDIRECT),
	HWCTX_REGINFO(0x805,   64, INDIRECT),
	HWCTX_REGINFO(0x807,    1, DIRECT),
	HWCTX_REGINFO(0x820,   32, DIRECT),
	HWCTX_REGINFO(0x900,   64, INDIRECT),
	HWCTX_REGINFO(0x902,    2, DIRECT),
	HWCTX_REGINFO(0x907,    1, DIRECT),
	HWCTX_REGINFO(0x90a,    1, DIRECT),
	HWCTX_REGINFO(0xa02,   10, DIRECT),
	HWCTX_REGINFO(0xe2a,    1, DIRECT),
	/* bug 976976 requires reg 0xe45 to be restored after reg 0x403 */
	/* bug 972588 requires reg 0x403 to be restored with reg 0x411's
	   value */
	HWCTX_REGINFO_RST(0x411, 1, DIRECT, 0xe45),
	HWCTX_REGINFO(0xe50,   49, DIRECT),
	/* bug 930456 requires reg 0xe2b to be restored with 0x126's value */
	HWCTX_REGINFO_RST(0x126, 1, DIRECT, 0xe2b),
};

#define SAVE_BEGIN_V1_SIZE (1 + RESTORE_BEGIN_SIZE)
#define SAVE_DIRECT_V1_SIZE (4 + RESTORE_DIRECT_SIZE)
#define SAVE_INDIRECT_V1_SIZE (6 + RESTORE_INDIRECT_SIZE)
#define SAVE_END_V1_SIZE (8 + RESTORE_END_SIZE)
#define SAVE_INCRS 3
#define RESTORE_BEGIN_SIZE 4
#define RESTORE_DIRECT_SIZE 1
#define RESTORE_INDIRECT_SIZE 2
#define RESTORE_END_SIZE 1

#ifdef CONFIG_TEGRA_FPGA_PLATFORM
#define NUM_3D_PIXEL_PIPES   2
#else
#define NUM_3D_PIXEL_PIPES   4
#endif

struct save_info {
	u32 *ptr;
	unsigned int save_count;
	unsigned int restore_count;
	unsigned int save_incrs;
	unsigned int restore_incrs;
};

/*** save ***/

static void save_push_v1(struct nvhost_hwctx *nctx, struct nvhost_cdma *cdma)
{
	struct host1x_hwctx *ctx = to_host1x_hwctx(nctx);
	struct host1x_hwctx_handler *p = host1x_hwctx_handler(ctx);

	/* wait for 3d idle */
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
			nvhost_opcode_imm_incr_syncpt(
				host1x_uclass_incr_syncpt_cond_op_done_v(),
				p->syncpt));
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					host1x_uclass_wait_syncpt_base_r(), 1),
			nvhost_class_host_wait_syncpt_base(p->syncpt,
							p->waitbase, 1));
	/* back to 3d */
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
			NVHOST_OPCODE_NOOP);

	/* invalidate the FDC to prevent cache-coherency issues across GPUs
	   note that we assume FDC_CONTROL_0 is left in the reset state by all
	   contexts.  the invalidate bit will clear itself, so the register
	   should be unchanged after this */
	/* bug 990395 T114 HW no longer can automatically clear the invalidate
	   bit. Luckily that the ctx switching always happens on the push
	   buffer boundary, and 3d driver inserts a FDC flush & invalidate &
	   clear the invalidate bit in the beginning of the each push buffer.
	   So we do not need to explicitly clear the invalidate bit here. */
	nvhost_cdma_push(cdma,
		nvhost_opcode_imm(AR3D_FDC_CONTROL_0,
			AR3D_FDC_CONTROL_0_RESET_VAL
				| AR3D_FDC_CONTROL_0_INVALIDATE),
		nvhost_opcode_imm(AR3D_GLOBAL_MEMORY_OUTPUT_READS, 1));
	/* bug 972588 requires SW to clear the reg 0x403 and 0xe45 */
	nvhost_cdma_push(cdma,
		nvhost_opcode_imm(0xe45, 0),
		nvhost_opcode_imm(0x403, 0));
	nvhost_cdma_push(cdma,
		nvhost_opcode_nonincr(AR3D_DW_MEMORY_OUTPUT_ADDRESS, 1),
		ctx->restore_phys);
	/* gather the save buffer */
	nvhost_cdma_push_gather(cdma,
			nvhost_get_host(nctx->channel->dev)->memmgr,
			p->save_buf,
			0,
			nvhost_opcode_gather(p->save_size),
			p->save_phys);
}

static void save_begin_v1(struct host1x_hwctx_handler *p, u32 *ptr)
{
	ptr[0] = nvhost_opcode_nonincr(AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_DATA,
			RESTORE_BEGIN_SIZE);
	nvhost_3dctx_restore_begin(p, ptr + 1);
	ptr += RESTORE_BEGIN_SIZE;
}

static void save_direct_v1(u32 *ptr, u32 start_reg, u32 count,
			u32 rst_reg, unsigned int pipe)
{
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID,
			(AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_DATA + pipe), 1);
	nvhost_3dctx_restore_direct(ptr + 1, rst_reg, count);
	ptr += RESTORE_DIRECT_SIZE;
	ptr[1] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					host1x_uclass_indoff_r(), 1);
	ptr[2] = nvhost_class_host_indoff_reg_read(
			host1x_uclass_indoff_indmodid_gr3d_v(),
			start_reg, true);
	/* TODO could do this in the setclass if count < 6 */
	ptr[3] = nvhost_opcode_nonincr(host1x_uclass_inddata_r(), count);
}

static void save_indirect_v1(u32 *ptr, u32 offset_reg, u32 offset,
			u32 data_reg, u32 count,
			u32 rst_reg, u32 rst_data_reg, unsigned int pipe)
{
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	ptr[1] = nvhost_opcode_nonincr(
			(AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_DATA + pipe),
			RESTORE_INDIRECT_SIZE);
	nvhost_3dctx_restore_indirect(ptr + 2, rst_reg, offset, rst_data_reg,
			count);
	ptr += RESTORE_INDIRECT_SIZE;
	ptr[2] = nvhost_opcode_imm(offset_reg, offset);
	ptr[3] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					host1x_uclass_indoff_r(), 1);
	ptr[4] = nvhost_class_host_indoff_reg_read(
			host1x_uclass_indoff_indmodid_gr3d_v(),
			data_reg, false);
	ptr[5] = nvhost_opcode_nonincr(host1x_uclass_inddata_r(), count);
}

static void save_end_v1(struct host1x_hwctx_handler *p, u32 *ptr)
{
	/* write end of restore buffer */
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID,
			AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_DATA, 1);
	nvhost_3dctx_restore_end(p, ptr + 1);
	ptr += RESTORE_END_SIZE;
	/* op_done syncpt incr to flush FDC */
	ptr[1] = nvhost_opcode_imm_incr_syncpt(
			host1x_uclass_incr_syncpt_cond_op_done_v(), p->syncpt);
	/* host wait for that syncpt incr, and advance the wait base */
	ptr[2] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			host1x_uclass_wait_syncpt_base_r(),
			nvhost_mask2(
				host1x_uclass_wait_syncpt_base_r(),
				host1x_uclass_incr_syncpt_base_r()));
	ptr[3] = nvhost_class_host_wait_syncpt_base(p->syncpt,
			p->waitbase, p->save_incrs - 1);
	ptr[4] = nvhost_class_host_incr_syncpt_base(p->waitbase,
			p->save_incrs);
	/* set class back to 3d */
	ptr[5] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	/* send reg reads back to host */
	ptr[6] = nvhost_opcode_imm(AR3D_GLOBAL_MEMORY_OUTPUT_READS, 0);
	/* final syncpt increment to release waiters */
	ptr[7] = nvhost_opcode_imm(0, p->syncpt);
}

static void setup_save_regs(struct save_info *info,
			const struct hwctx_reginfo *regs,
			unsigned int nr_regs,
			unsigned int pipe)
{
	const struct hwctx_reginfo *rend = regs + nr_regs;
	u32 *ptr = info->ptr;
	unsigned int save_count = info->save_count;
	unsigned int restore_count = info->restore_count;

	for ( ; regs != rend; ++regs) {
		u32 offset = regs->offset + pipe;
		u32 count = regs->count;
		u32 rstoff = regs->rst_off + pipe;
		u32 indoff = offset + 1;
		u32 indrstoff = rstoff + 1;

		switch (regs->type) {
		case HWCTX_REGINFO_DIRECT:
			if (ptr) {
				save_direct_v1(ptr, offset,
						count, rstoff, pipe);
				ptr += SAVE_DIRECT_V1_SIZE;
			}
			save_count += SAVE_DIRECT_V1_SIZE;
			restore_count += RESTORE_DIRECT_SIZE;
			break;
		case HWCTX_REGINFO_INDIRECT_4X:
			++indoff;
			++indrstoff;
			/* fall through */
		case HWCTX_REGINFO_INDIRECT:
			if (ptr) {
				save_indirect_v1(ptr, offset, 0,
						indoff, count,
						rstoff, indrstoff,
						pipe);
				ptr += SAVE_INDIRECT_V1_SIZE;
			}
			save_count += SAVE_INDIRECT_V1_SIZE;
			restore_count += RESTORE_INDIRECT_SIZE;
			break;
		}
		if (ptr) {
			/* SAVE cases only: reserve room for incoming data */
			u32 k = 0;
			/*
			 * Create a signature pattern for indirect data (which
			 * will be overwritten by true incoming data) for
			 * better deducing where we are in a long command
			 * sequence, when given only a FIFO snapshot for debug
			 * purposes.
			*/
			for (k = 0; k < count; k++)
				*(ptr + k) = 0xd000d000 | (offset << 16) | k;
			ptr += count;
		}
		save_count += count;
		restore_count += count;
	}

	info->ptr = ptr;
	info->save_count = save_count;
	info->restore_count = restore_count;
}

static void incr_mem_output_pointer(struct save_info *info,
			unsigned int pipe,
			unsigned int incr)
{
	unsigned int i;
	u32 *ptr = info->ptr;
	if (ptr) {
		*ptr = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
		ptr++;
		for (i = 0; i < incr; i++)
			*(ptr + i) = nvhost_opcode_imm(
				(AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_INCR +	pipe),
				1);
		ptr += incr;
	}
	info->ptr = ptr;
	info->save_count += incr+1;
}

static void setup_save(struct host1x_hwctx_handler *p, u32 *ptr)
{
	int pipe, i;
	unsigned int old_restore_count, incr_count;
	struct save_info info = {
		ptr,
		SAVE_BEGIN_V1_SIZE,
		RESTORE_BEGIN_SIZE,
		SAVE_INCRS,
		1
	};

	if (info.ptr) {
		save_begin_v1(p, info.ptr);
		info.ptr += SAVE_BEGIN_V1_SIZE;
	}

	/* save regs for per pixel pipe, this has to be before the global
	 * one. Advance the rest of the pipes' output pointer to match with
	 * the pipe 0's.
	*/
	for (pipe = 1; pipe < NUM_3D_PIXEL_PIPES; pipe++)
		incr_mem_output_pointer(&info, pipe, RESTORE_BEGIN_SIZE);

	for (pipe = NUM_3D_PIXEL_PIPES - 1; pipe >= 0; pipe--) {
		old_restore_count = info.restore_count;
		setup_save_regs(&info,
				ctxsave_regs_3d_per_pipe,
				ARRAY_SIZE(ctxsave_regs_3d_per_pipe),
				(unsigned int) pipe);
		/* Advance the rest of the pipes' output pointer to match with
		 * the current pipe's one.
		*/
		incr_count = info.restore_count - old_restore_count;
		for (i = 0; i < pipe; i++)
			incr_mem_output_pointer(&info, (unsigned int) i,
						incr_count);
	}

	/* save regs for global. Use pipe 0 to do the save */
	setup_save_regs(&info,
			ctxsave_regs_3d_global,
			ARRAY_SIZE(ctxsave_regs_3d_global), 0);

	if (info.ptr) {
		save_end_v1(p, info.ptr);
		info.ptr += SAVE_END_V1_SIZE;
	}

	wmb();

	p->save_size = info.save_count + SAVE_END_V1_SIZE;
	p->restore_size = info.restore_count + RESTORE_END_SIZE;
	p->save_incrs = info.save_incrs;
	p->save_thresh = p->save_incrs;
	p->restore_incrs = info.restore_incrs;
}

/*** ctx3d ***/

static struct nvhost_hwctx *ctx3d_alloc_v1(struct nvhost_hwctx_handler *h,
		struct nvhost_channel *ch)
{
	struct host1x_hwctx_handler *p = to_host1x_hwctx_handler(h);
	struct host1x_hwctx *ctx = nvhost_3dctx_alloc_common(p, ch, false);

	if (ctx)
		return &ctx->hwctx;
	else
		return NULL;
}

struct nvhost_hwctx_handler *nvhost_gr3d_t114_ctxhandler_init(
		u32 syncpt, u32 waitbase,
		struct nvhost_channel *ch)
{
	struct mem_mgr *memmgr;
	u32 *save_ptr;
	struct host1x_hwctx_handler *p;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	memmgr = nvhost_get_host(ch->dev)->memmgr;

	p->syncpt = syncpt;
	p->waitbase = waitbase;

	setup_save(p, NULL);

	p->save_buf = mem_op().alloc(memmgr, p->save_size * 4, 32,
				mem_mgr_flag_write_combine);
	if (IS_ERR_OR_NULL(p->save_buf))
		goto fail_alloc;

	save_ptr = mem_op().mmap(p->save_buf);
	if (IS_ERR_OR_NULL(save_ptr))
		goto fail_mmap;

	p->save_sgt = mem_op().pin(memmgr, p->save_buf);
	if (IS_ERR_OR_NULL(p->save_sgt))
		goto fail_pin;
	p->save_phys = sg_dma_address(p->save_sgt->sgl);

	setup_save(p, save_ptr);

	mem_op().munmap(p->save_buf, save_ptr);

	p->save_slots = 5;
	p->h.alloc = ctx3d_alloc_v1;
	p->h.save_push = save_push_v1;
	p->h.save_service = NULL;
	p->h.get = nvhost_3dctx_get;
	p->h.put = nvhost_3dctx_put;

	return &p->h;

fail_pin:
	mem_op().munmap(p->save_buf, save_ptr);
fail_mmap:
	mem_op().put(memmgr, p->save_buf);
fail_alloc:
	kfree(p);
	return NULL;
}

void nvhost_gr3d_t114_init(struct platform_device *dev)
{
	if (actmon_op().init)
		actmon_op().init(nvhost_get_host(dev));
}

void nvhost_gr3d_t114_deinit(struct platform_device *dev)
{
	if (actmon_op().deinit)
		actmon_op().deinit(nvhost_get_host(dev));
}

int nvhost_gr3d_t114_prepare_power_off(struct platform_device *dev)
{
	if (actmon_op().deinit)
		actmon_op().deinit(nvhost_get_host(dev));
	return nvhost_gr3d_prepare_power_off(dev);
}

void nvhost_gr3d_t114_finalize_power_on(struct platform_device *dev)
{
	/* actmon needs to be reinitialized when we come back from
	 * power gated state */
	if (actmon_op().init)
		actmon_op().init(nvhost_get_host(dev));
}
