/*
 * drivers/misc/tegra-profiler/exh_tables.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include "eh_unwind.h"
#include "backtrace.h"

#define QUADD_EXTABS_SIZE	0x100

#define GET_NR_PAGES(a, l) \
	((PAGE_ALIGN((a) + (l)) - ((a) & PAGE_MASK)) / PAGE_SIZE)

enum regs {
	FP_THUMB = 7,
	FP_ARM = 11,

	SP = 13,
	LR = 14,
	PC = 15
};

struct extab_info {
	unsigned long addr;
	unsigned long length;
};

struct extables {
	struct extab_info exidx;
	struct extab_info extab;
};

struct ex_region_info {
	unsigned long vm_start;
	unsigned long vm_end;

	struct extables tabs;
};

struct quadd_unwind_ctx {
	struct ex_region_info *regions;
	unsigned long ri_nr;
	unsigned long ri_size;

	struct task_struct *task;

	unsigned long pinned_pages;
	unsigned long pinned_size;

	spinlock_t lock;
};

struct unwind_idx {
	unsigned long addr_offset;
	unsigned long insn;
};

struct stackframe {
	unsigned long fp_thumb;
	unsigned long fp_arm;

	unsigned long sp;
	unsigned long lr;
	unsigned long pc;
};

struct unwind_ctrl_block {
	unsigned long vrs[16];		/* virtual register set */
	const unsigned long *insn;	/* pointer to the current instr word */
	int entries;			/* number of entries left */
	int byte;			/* current byte in the instr word */
};

struct pin_pages_work {
	struct work_struct work;
	unsigned long vm_start;
};

struct quadd_unwind_ctx ctx;

#define read_user_data(addr, retval)			\
({							\
	long ret;					\
	ret = probe_kernel_address(addr, retval);	\
	if (ret)					\
		ret = -QUADD_URC_EACCESS;		\
	ret;						\
})

static int
add_ex_region(struct ex_region_info *new_entry)
{
	unsigned int i_min, i_max, mid;
	struct ex_region_info *array = ctx.regions;
	unsigned long size = ctx.ri_nr;

	if (!array)
		return 0;

	if (size == 0) {
		memcpy(&array[0], new_entry, sizeof(*new_entry));
		return 1;
	} else if (size == 1 && array[0].vm_start == new_entry->vm_start) {
		return 0;
	}

	i_min = 0;
	i_max = size;

	if (array[0].vm_start > new_entry->vm_start) {
		memmove(array + 1, array,
			size * sizeof(*array));
		memcpy(&array[0], new_entry, sizeof(*new_entry));
		return 1;
	} else if (array[size - 1].vm_start < new_entry->vm_start) {
		memcpy(&array[size], new_entry, sizeof(*new_entry));
		return 1;
	}

	while (i_min < i_max) {
		mid = i_min + (i_max - i_min) / 2;

		if (new_entry->vm_start <= array[mid].vm_start)
			i_max = mid;
		else
			i_min = mid + 1;
	}

	if (array[i_max].vm_start == new_entry->vm_start) {
		return 0;
	} else {
		memmove(array + i_max + 1,
			array + i_max,
			(size - i_max) * sizeof(*array));
		memcpy(&array[i_max], new_entry, sizeof(*new_entry));
		return 1;
	}
}

static struct ex_region_info *
search_ex_region(unsigned long key, struct extables *tabs)
{
	unsigned int i_min, i_max, mid;
	struct ex_region_info *array = ctx.regions;
	unsigned long size = ctx.ri_nr;

	if (size == 0)
		return NULL;

	i_min = 0;
	i_max = size;

	while (i_min < i_max) {
		mid = i_min + (i_max - i_min) / 2;

		if (key <= array[mid].vm_start)
			i_max = mid;
		else
			i_min = mid + 1;
	}

	if (array[i_max].vm_start == key) {
		memcpy(tabs, &array[i_max].tabs, sizeof(*tabs));
		return &array[i_max];
	}

	return NULL;
}

static void pin_user_pages(struct extables *tabs)
{
	long ret;
	struct extab_info *ti;
	unsigned long nr_pages, addr;
	struct mm_struct *mm = ctx.task->mm;

	if (!mm)
		return;

	down_write(&mm->mmap_sem);

	ti = &tabs->exidx;
	addr = ti->addr & PAGE_MASK;
	nr_pages = GET_NR_PAGES(ti->addr, ti->length);

	ret = get_user_pages(ctx.task, mm, addr, nr_pages, 0, 0,
			     NULL, NULL);
	if (ret < 0) {
		pr_debug("%s: warning: addr/nr_pages: %#lx/%lu\n",
			 __func__, ti->addr, nr_pages);
		goto error_out;
	}

	ctx.pinned_pages += ret;
	ctx.pinned_size += ti->length;

	pr_debug("%s: pin exidx: addr/nr_pages: %#lx/%lu\n",
		 __func__, ti->addr, nr_pages);

	ti = &tabs->extab;
	addr = ti->addr & PAGE_MASK;
	nr_pages = GET_NR_PAGES(ti->addr, ti->length);

	ret = get_user_pages(ctx.task, mm, addr, nr_pages, 0, 0,
			     NULL, NULL);
	if (ret < 0) {
		pr_debug("%s: warning: addr/nr_pages: %#lx/%lu\n",
			 __func__, ti->addr, nr_pages);
		goto error_out;
	}

	ctx.pinned_pages += ret;
	ctx.pinned_size += ti->length;

	pr_debug("%s: pin extab: addr/nr_pages: %#lx/%lu\n",
		 __func__, ti->addr, nr_pages);

error_out:
	up_write(&mm->mmap_sem);
}

static void
pin_user_pages_work(struct work_struct *w)
{
	struct extables tabs;
	struct ex_region_info *ri;
	struct pin_pages_work *work;

	work = container_of(w, struct pin_pages_work, work);

	spin_lock(&ctx.lock);
	ri = search_ex_region(work->vm_start, &tabs);
	spin_unlock(&ctx.lock);
	if (ri)
		pin_user_pages(&tabs);

	kfree(w);
}

static int
__pin_user_pages(unsigned long vm_start)
{
	struct pin_pages_work *work;

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (!work)
		return -ENOMEM;

	INIT_WORK(&work->work, pin_user_pages_work);
	work->vm_start = vm_start;

	schedule_work(&work->work);

	return 0;
}

int quadd_unwind_set_extab(struct quadd_extables *extabs)
{
	int err = 0;
	struct ex_region_info ri_entry;
	struct extab_info *ti;

	spin_lock(&ctx.lock);

	if (!ctx.regions) {
		err = -ENOMEM;
		goto error_out;
	}

	if (ctx.ri_nr >= ctx.ri_size) {
		struct ex_region_info *new_regions;
		unsigned long newlen = ctx.ri_size + (ctx.ri_size >> 1);

		new_regions = krealloc(ctx.regions, newlen, GFP_KERNEL);
		if (!new_regions) {
			err = -ENOMEM;
			goto error_out;
		}
		ctx.regions = new_regions;
		ctx.ri_size = newlen;
	}

	ri_entry.vm_start = extabs->vm_start;
	ri_entry.vm_end = extabs->vm_end;

	ti = &ri_entry.tabs.exidx;
	ti->addr = extabs->exidx.addr;
	ti->length = extabs->exidx.length;

	ti = &ri_entry.tabs.extab;
	ti->addr = extabs->extab.addr;
	ti->length = extabs->extab.length;

	ctx.ri_nr += add_ex_region(&ri_entry);

	spin_unlock(&ctx.lock);

	__pin_user_pages(ri_entry.vm_start);

	return 0;

error_out:
	spin_unlock(&ctx.lock);
	return err;
}

static unsigned long
prel31_to_addr(const unsigned long *ptr)
{
	unsigned long value;
	long offset;

	if (read_user_data(ptr, value))
		return 0;

	/* sign-extend to 32 bits */
	offset = (((long)value) << 1) >> 1;
	return (unsigned long)ptr + offset;
}

static const struct unwind_idx *
unwind_find_origin(const struct unwind_idx *start,
		   const struct unwind_idx *stop)
{
	while (start < stop) {
		unsigned long addr_offset;
		const struct unwind_idx *mid = start + ((stop - start) >> 1);

		if (read_user_data(&mid->addr_offset, addr_offset))
			return ERR_PTR(-EFAULT);

		if (addr_offset >= 0x40000000)
			/* negative offset */
			start = mid + 1;
		else
			/* positive offset */
			stop = mid;
	}

	return stop;
}

/*
 * Binary search in the unwind index. The entries are
 * guaranteed to be sorted in ascending order by the linker.
 *
 * start = first entry
 * origin = first entry with positive offset (or stop if there is no such entry)
 * stop - 1 = last entry
 */
static const struct unwind_idx *
search_index(unsigned long addr,
	     const struct unwind_idx *start,
	     const struct unwind_idx *origin,
	     const struct unwind_idx *stop)
{
	unsigned long addr_prel31;

	pr_debug("%08lx, %p, %p, %p\n", addr, start, origin, stop);

	/*
	 * only search in the section with the matching sign. This way the
	 * prel31 numbers can be compared as unsigned longs.
	 */
	if (addr < (unsigned long)start)
		/* negative offsets: [start; origin) */
		stop = origin;
	else
		/* positive offsets: [origin; stop) */
		start = origin;

	/* prel31 for address relavive to start */
	addr_prel31 = (addr - (unsigned long)start) & 0x7fffffff;

	while (start < stop - 1) {
		unsigned long addr_offset;

		const struct unwind_idx *mid = start + ((stop - start) >> 1);

		/*
		 * As addr_prel31 is relative to start an offset is needed to
		 * make it relative to mid.
		 */
		if (read_user_data(&mid->addr_offset, addr_offset))
			return ERR_PTR(-EFAULT);

		if (addr_prel31 - ((unsigned long)mid - (unsigned long)start) <
				addr_offset) {
			stop = mid;
		} else {
			/* keep addr_prel31 relative to start */
			addr_prel31 -= ((unsigned long)mid -
					(unsigned long)start);
			start = mid;
		}
	}

	if (likely(start->addr_offset <= addr_prel31))
		return start;

	pr_debug("Unknown address %08lx\n", addr);
	return NULL;
}

static const struct unwind_idx *
unwind_find_idx(struct extab_info *exidx, unsigned long addr)
{
	const struct unwind_idx *start;
	const struct unwind_idx *origin;
	const struct unwind_idx *stop;
	const struct unwind_idx *idx = NULL;

	start = (const struct unwind_idx *)exidx->addr;
	stop = start + exidx->length / sizeof(*start);

	origin = unwind_find_origin(start, stop);
	if (IS_ERR(origin))
		return origin;

	idx = search_index(addr, start, origin, stop);

	pr_debug("addr: %#lx, start: %p, origin: %p, stop: %p, idx: %p\n",
		addr, start, origin, stop, idx);

	return idx;
}

static unsigned long
unwind_get_byte(struct unwind_ctrl_block *ctrl, int *err)
{
	unsigned long ret, insn_word;

	*err = 0;

	if (ctrl->entries <= 0) {
		pr_debug("error: corrupt unwind table\n");
		*err = -QUADD_URC_TBL_IS_CORRUPT;
		return 0;
	}

	*err = read_user_data(ctrl->insn, insn_word);
	if (*err < 0)
		return 0;

	ret = (insn_word >> (ctrl->byte * 8)) & 0xff;

	if (ctrl->byte == 0) {
		ctrl->insn++;
		ctrl->entries--;
		ctrl->byte = 3;
	} else
		ctrl->byte--;

	return ret;
}

/*
 * Execute the current unwind instruction.
 */
static int unwind_exec_insn(struct unwind_ctrl_block *ctrl)
{
	int i, err;
	unsigned long insn = unwind_get_byte(ctrl, &err);

	if (err < 0)
		return err;

	pr_debug("%s: insn = %08lx\n", __func__, insn);

	if ((insn & 0xc0) == 0x00) {
		ctrl->vrs[SP] += ((insn & 0x3f) << 2) + 4;

		pr_debug("CMD_DATA_POP: vsp = vsp + %lu (new: %#lx)\n",
			((insn & 0x3f) << 2) + 4, ctrl->vrs[SP]);
	} else if ((insn & 0xc0) == 0x40) {
		ctrl->vrs[SP] -= ((insn & 0x3f) << 2) + 4;

		pr_debug("CMD_DATA_PUSH: vsp = vsp – %lu (new: %#lx)\n",
			((insn & 0x3f) << 2) + 4, ctrl->vrs[SP]);
	} else if ((insn & 0xf0) == 0x80) {
		unsigned long mask;
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int load_sp, reg = 4;

		insn = (insn << 8) | unwind_get_byte(ctrl, &err);
		if (err < 0)
			return err;

		mask = insn & 0x0fff;
		if (mask == 0) {
			pr_debug("CMD_REFUSED: unwind: 'Refuse to unwind' instruction %04lx\n",
				   insn);
			return -QUADD_URC_REFUSE_TO_UNWIND;
		}

		/* pop R4-R15 according to mask */
		load_sp = mask & (1 << (13 - 4));
		while (mask) {
			if (mask & 1) {
				err = read_user_data(vsp++, ctrl->vrs[reg]);
				if (err < 0)
					return err;

				pr_debug("CMD_REG_POP: pop {r%d}\n", reg);
			}
			mask >>= 1;
			reg++;
		}
		if (!load_sp)
			ctrl->vrs[SP] = (unsigned long)vsp;

		pr_debug("new vsp: %#lx\n", ctrl->vrs[SP]);
	} else if ((insn & 0xf0) == 0x90 &&
		   (insn & 0x0d) != 0x0d) {
		ctrl->vrs[SP] = ctrl->vrs[insn & 0x0f];
		pr_debug("CMD_REG_TO_SP: vsp = {r%lu}\n", insn & 0x0f);
	} else if ((insn & 0xf0) == 0xa0) {
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg;

		/* pop R4-R[4+bbb] */
		for (reg = 4; reg <= 4 + (insn & 7); reg++) {
			err = read_user_data(vsp++, ctrl->vrs[reg]);
			if (err < 0)
				return err;

			pr_debug("CMD_REG_POP: pop {r%d}\n", reg);
		}

		if (insn & 0x08) {
			err = read_user_data(vsp++, ctrl->vrs[14]);
			if (err < 0)
				return err;

			pr_debug("CMD_REG_POP: pop {r14}\n");
		}
		ctrl->vrs[SP] = (unsigned long)vsp;
		pr_debug("new vsp: %#lx\n", ctrl->vrs[SP]);
	} else if (insn == 0xb0) {
		if (ctrl->vrs[PC] == 0)
			ctrl->vrs[PC] = ctrl->vrs[LR];
		/* no further processing */
		ctrl->entries = 0;

		pr_debug("CMD_FINISH\n");
	} else if (insn == 0xb1) {
		unsigned long mask = unwind_get_byte(ctrl, &err);
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];
		int reg = 0;

		if (err < 0)
			return err;

		if (mask == 0 || mask & 0xf0) {
			pr_debug("unwind: Spare encoding %04lx\n",
			       (insn << 8) | mask);
			return -QUADD_URC_SPARE_ENCODING;
		}

		/* pop R0-R3 according to mask */
		while (mask) {
			if (mask & 1) {
				err = read_user_data(vsp++, ctrl->vrs[reg]);
				if (err < 0)
					return err;

				pr_debug("CMD_REG_POP: pop {r%d}\n", reg);
			}
			mask >>= 1;
			reg++;
		}
		ctrl->vrs[SP] = (unsigned long)vsp;
		pr_debug("new vsp: %#lx\n", ctrl->vrs[SP]);
	} else if (insn == 0xb2) {
		unsigned long uleb128 = unwind_get_byte(ctrl, &err);
		if (err < 0)
			return err;

		ctrl->vrs[SP] += 0x204 + (uleb128 << 2);

		pr_debug("CMD_DATA_POP: vsp = vsp + %lu, new vsp: %#lx\n",
			 0x204 + (uleb128 << 2), ctrl->vrs[SP]);
	} else if (insn == 0xb3 || insn == 0xc8 || insn == 0xc9) {
		unsigned long data, reg_from, reg_to;
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];

		data = unwind_get_byte(ctrl, &err);
		if (err < 0)
			return err;

		reg_from = (data & 0xf0) >> 4;
		reg_to = reg_from + (data & 0x0f);

		if (insn == 0xc8) {
			reg_from += 16;
			reg_to += 16;
		}

		for (i = reg_from; i <= reg_to; i++)
			vsp += 2;

		if (insn == 0xb3)
			vsp++;

		ctrl->vrs[SP] = (unsigned long)vsp;

		pr_debug("CMD_VFP_POP (%#lx %#lx): pop {D%lu-D%lu}\n",
			 insn, data, reg_from, reg_to);
		pr_debug("new vsp: %#lx\n", ctrl->vrs[SP]);
	} else if ((insn & 0xf8) == 0xb8 || (insn & 0xf8) == 0xd0) {
		unsigned long reg_to;
		unsigned long data = insn & 0x07;
		unsigned long *vsp = (unsigned long *)ctrl->vrs[SP];

		reg_to = 8 + data;

		for (i = 8; i <= reg_to; i++)
			vsp += 2;

		if ((insn & 0xf8) == 0xb8)
			vsp++;

		ctrl->vrs[SP] = (unsigned long)vsp;

		pr_debug("CMD_VFP_POP (%#lx): pop {D8-D%lu}\n",
			 insn, reg_to);
		pr_debug("new vsp: %#lx\n", ctrl->vrs[SP]);
	} else {
		pr_debug("error: unhandled instruction %02lx\n", insn);
		return -QUADD_URC_UNHANDLED_INSTRUCTION;
	}

	pr_debug("%s: fp_arm: %#lx, fp_thumb: %#lx, sp: %#lx, lr = %#lx, pc: %#lx\n",
		 __func__,
		 ctrl->vrs[FP_ARM], ctrl->vrs[FP_THUMB], ctrl->vrs[SP],
		 ctrl->vrs[LR], ctrl->vrs[PC]);

	return 0;
}

/*
 * Unwind a single frame starting with *sp for the symbol at *pc. It
 * updates the *pc and *sp with the new values.
 */
static int
unwind_frame(struct extab_info *exidx,
	     struct stackframe *frame,
	     struct vm_area_struct *vma_sp)
{
	unsigned long high, low;
	const struct unwind_idx *idx;
	struct unwind_ctrl_block ctrl;
	unsigned long val, err;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = vma_sp->vm_end;

	pr_debug("pc: %#lx, lr: %#lx, sp:%#lx, low/high: %#lx/%#lx\n",
		frame->pc, frame->lr, frame->sp, low, high);

	idx = unwind_find_idx(exidx, frame->pc);
	if (IS_ERR_OR_NULL(idx))
		return -QUADD_URC_IDX_NOT_FOUND;

	pr_debug("index was found by pc (%#lx): %p\n", frame->pc, idx);

	ctrl.vrs[FP_THUMB] = frame->fp_thumb;
	ctrl.vrs[FP_ARM] = frame->fp_arm;

	ctrl.vrs[SP] = frame->sp;
	ctrl.vrs[LR] = frame->lr;
	ctrl.vrs[PC] = 0;

	err = read_user_data(&idx->insn, val);
	if (err < 0)
		return err;

	if (val == 1) {
		/* can't unwind */
		return -QUADD_URC_CANTUNWIND;
	} else if ((val & 0x80000000) == 0) {
		/* prel31 to the unwind table */
		ctrl.insn = (unsigned long *)prel31_to_addr(&idx->insn);
		if (!ctrl.insn)
			return -QUADD_URC_EACCESS;
	} else if ((val & 0xff000000) == 0x80000000) {
		/* only personality routine 0 supported in the index */
		ctrl.insn = &idx->insn;
	} else {
		pr_debug("unsupported personality routine %08lx in the index at %p\n",
			 val, idx);
		return -QUADD_URC_UNSUPPORTED_PR;
	}

	err = read_user_data(ctrl.insn, val);
	if (err < 0)
		return err;

	/* check the personality routine */
	if ((val & 0xff000000) == 0x80000000) {
		ctrl.byte = 2;
		ctrl.entries = 1;
	} else if ((val & 0xff000000) == 0x81000000) {
		ctrl.byte = 1;
		ctrl.entries = 1 + ((val & 0x00ff0000) >> 16);
	} else {
		pr_debug("unsupported personality routine %08lx at %p\n",
			 val, ctrl.insn);
		return -QUADD_URC_UNSUPPORTED_PR;
	}

	while (ctrl.entries > 0) {
		int err = unwind_exec_insn(&ctrl);
		if (err < 0)
			return err;

		if (ctrl.vrs[SP] < low || ctrl.vrs[SP] >= high)
			return -QUADD_URC_SP_INCORRECT;
	}

	if (ctrl.vrs[PC] == 0)
		ctrl.vrs[PC] = ctrl.vrs[LR];

	/* check for infinite loop */
	if (frame->pc == ctrl.vrs[PC])
		return -QUADD_URC_FAILURE;

	frame->fp_thumb = ctrl.vrs[FP_THUMB];
	frame->fp_arm = ctrl.vrs[FP_ARM];

	frame->sp = ctrl.vrs[SP];
	frame->lr = ctrl.vrs[LR];
	frame->pc = ctrl.vrs[PC];

	return 0;
}

static void
unwind_backtrace(struct quadd_callchain *cc,
		 struct extab_info *exidx,
		 struct pt_regs *regs,
		 struct vm_area_struct *vma_sp)
{
	struct extables tabs;
	struct stackframe frame;

	frame.fp_thumb = regs->ARM_r7;
	frame.fp_arm = regs->ARM_fp;

	frame.pc = instruction_pointer(regs);
	frame.sp = user_stack_pointer(regs);

	frame.lr = regs->ARM_lr;

	cc->unw_rc = QUADD_URC_FAILURE;

	pr_debug("fp_arm: %#lx, fp_thumb: %#lx, sp: %#lx, lr: %#lx, pc: %#lx\n",
		 frame.fp_arm, frame.fp_thumb, frame.sp, frame.lr, frame.pc);
	pr_debug("vma_sp: %#lx - %#lx, length: %#lx\n",
		 vma_sp->vm_start, vma_sp->vm_end,
		 vma_sp->vm_start - vma_sp->vm_end);

	while (1) {
		int err;
		unsigned long where = frame.pc;
		struct vm_area_struct *vma_pc;
		struct mm_struct *mm = current->mm;

		if (!mm)
			break;

		vma_pc = find_vma(mm, frame.pc);
		if (!vma_pc)
			break;

		if (!is_vma_addr(exidx->addr, vma_pc)) {
			struct ex_region_info *ri;

			spin_lock(&ctx.lock);
			ri = search_ex_region(vma_pc->vm_start, &tabs);
			spin_unlock(&ctx.lock);
			if (!ri) {
				cc->unw_rc = QUADD_URC_TBL_NOT_EXIST;
				break;
			}

			exidx = &tabs.exidx;
		}

		err = unwind_frame(exidx, &frame, vma_sp);
		if (err < 0) {
			pr_debug("end unwind, urc: %d\n", err);
			cc->unw_rc = -err;
			break;
		}

		pr_debug("function at [<%08lx>] from [<%08lx>]\n",
			 where, frame.pc);

		quadd_callchain_store(cc, frame.pc);
	}
}

unsigned int
quadd_get_user_callchain_ut(struct pt_regs *regs,
			    struct quadd_callchain *cc)
{
	unsigned long ip, sp;
	struct vm_area_struct *vma, *vma_sp;
	struct mm_struct *mm = current->mm;
	struct ex_region_info *ri;
	struct extables tabs;

	cc->unw_method = QUADD_UNW_METHOD_EHT;
	cc->unw_rc = QUADD_URC_FAILURE;

	if (!regs || !mm)
		return 0;

	ip = instruction_pointer(regs);

	vma = find_vma(mm, ip);
	if (!vma)
		return 0;

	sp = user_stack_pointer(regs);

	vma_sp = find_vma(mm, sp);
	if (!vma_sp)
		return 0;

	spin_lock(&ctx.lock);
	ri = search_ex_region(vma->vm_start, &tabs);
	spin_unlock(&ctx.lock);
	if (!ri) {
		cc->unw_rc = QUADD_URC_TBL_NOT_EXIST;
		return 0;
	}

	unwind_backtrace(cc, &tabs.exidx, regs, vma_sp);

	return cc->nr;
}

int quadd_unwind_start(struct task_struct *task)
{
	spin_lock(&ctx.lock);

	kfree(ctx.regions);

	ctx.ri_nr = 0;
	ctx.ri_size = 0;

	ctx.pinned_pages = 0;
	ctx.pinned_size = 0;

	ctx.regions = kzalloc(QUADD_EXTABS_SIZE * sizeof(*ctx.regions),
			      GFP_KERNEL);
	if (!ctx.regions) {
		spin_unlock(&ctx.lock);
		return -ENOMEM;
	}
	ctx.ri_size = QUADD_EXTABS_SIZE;

	ctx.task = task;

	spin_unlock(&ctx.lock);

	return 0;
}

void quadd_unwind_stop(void)
{
	spin_lock(&ctx.lock);

	kfree(ctx.regions);
	ctx.regions = NULL;

	ctx.ri_size = 0;
	ctx.ri_nr = 0;

	ctx.task = NULL;

	spin_unlock(&ctx.lock);

	pr_info("exception tables size: %lu bytes\n", ctx.pinned_size);
	pr_info("pinned pages: %lu (%lu bytes)\n", ctx.pinned_pages,
		ctx.pinned_pages * PAGE_SIZE);
}

int quadd_unwind_init(void)
{
	ctx.regions = NULL;
	ctx.ri_size = 0;
	ctx.ri_nr = 0;

	spin_lock_init(&ctx.lock);

	return 0;
}

void quadd_unwind_deinit(void)
{
	quadd_unwind_stop();
}
