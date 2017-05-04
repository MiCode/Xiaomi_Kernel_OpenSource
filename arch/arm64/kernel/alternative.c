/*
 * alternative runtime patching
 * inspired by the x86 version
 *
 * Copyright (C) 2014 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "alternatives: " fmt

#include <linux/init.h>
#include <linux/cpu.h>
#include <asm/cacheflush.h>
#include <asm/alternative.h>
#include <asm/cpufeature.h>
#include <asm/insn.h>
#include <linux/stop_machine.h>

#define __ALT_PTR(a,f)		(u32 *)((void *)&(a)->f + (a)->f)
#define ALT_ORIG_PTR(a)		__ALT_PTR(a, orig_offset)
#define ALT_REPL_PTR(a)		__ALT_PTR(a, alt_offset)

extern struct alt_instr __alt_instructions[], __alt_instructions_end[];

struct alt_region {
	struct alt_instr *begin;
	struct alt_instr *end;
};

/*
 * Check if the target PC is within an alternative block.
 */

static void __apply_alternatives(void *alt_region)
{
	struct alt_instr *alt;
	u32 *origptr, *replptr, *endptr;
	struct alt_region *region = alt_region;

	for (alt = region->begin; alt < region->end; alt++) {

		if (!cpus_have_cap(alt->cpufeature))
			continue;

		BUG_ON(alt->alt_len != alt->orig_len);

		pr_info_once("patching kernel code\n");

		origptr = (void *)&alt->orig_offset + alt->orig_offset;
		endptr = (void *)origptr + alt->orig_len;
		replptr = (void *)&alt->alt_offset + alt->alt_offset;
		for (; origptr < endptr; origptr++, replptr++)
			BUG_ON(aarch64_insn_patch_text_nosync(origptr, *replptr));
	}
}

/*
 * We might be patching the stop_machine state machine, so implement a
 * really simple polling protocol here.
 */
static int __apply_alternatives_multi_stop(void *unused)
{
	static int patched = 0;
	struct alt_region region = {
		.begin	= __alt_instructions,
		.end	= __alt_instructions_end,
	};

	/* We always have a CPU 0 at this point (__init) */
	if (smp_processor_id()) {
		while (!READ_ONCE(patched))
			cpu_relax();
	} else {
		BUG_ON(patched);
		__apply_alternatives(&region);
		/* Barriers provided by the cache flushing */
		WRITE_ONCE(patched, 1);
	}

	return 0;
}

void __init apply_alternatives_all(void)
{
	/* better not try code patching on a live SMP system */
	stop_machine(__apply_alternatives_multi_stop, NULL, cpu_online_mask);
}

void apply_alternatives(void *start, size_t length)
{
	struct alt_region region = {
		.begin	= start,
		.end	= start + length,
	};

	__apply_alternatives(&region);
}

void free_alternatives_memory(void)
{
	free_reserved_area(__alt_instructions, __alt_instructions_end,
			   0, "alternatives");
}
