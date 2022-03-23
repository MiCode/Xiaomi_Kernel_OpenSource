// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 - Google LLC
 * Author: David Brazdil <dbrazdil@google.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "../../../tools/testing/selftests/kselftest_module.h"

#include <linux/slab.h>

#include <asm/kvm_s2mpu.h>

KSTM_MODULE_GLOBALS();

#define ASSERT(cond)							\
	do {								\
		if (!(cond)) {						\
			pr_err("line %d: assertion failed: %s\n",	\
			       __LINE__, #cond);			\
			return -1;					\
		}							\
	} while (0)

static struct fmpt g_fmpt;
static u32 g_smpt[SMPT_NUM_WORDS];

static void __init init_smpt(enum mpt_prot prot)
{
	memset(g_smpt, (char)mpt_prot_doubleword[prot], SMPT_SIZE);
}

static void __init init_fmpt(enum mpt_prot prot, bool gran_1g)
{
	init_smpt(prot);
	g_fmpt = (struct fmpt){
		.gran_1g = gran_1g,
		.prot = prot,
		.smpt = g_smpt,
	};
}

static enum mpt_prot __init get_prot_at(size_t gb_byte_off)
{
	size_t page_idx = gb_byte_off / SMPT_GRAN;
	size_t word_idx = page_idx / SMPT_ELEMS_PER_WORD;
	size_t bit_shift = (page_idx % SMPT_ELEMS_PER_WORD) * MPT_PROT_BITS;

	return (g_smpt[word_idx] >> bit_shift) & MPT_PROT_MASK;
}

static bool __init check_smpt(size_t start_byte, size_t end_byte,
			      enum mpt_prot prot_out, enum mpt_prot prot_in)
{
	size_t off;

	for (off = 0; off < start_byte; off += PAGE_SIZE) {
		if (get_prot_at(off) != prot_out)
			return false;
	}
	for (off = start_byte; off < end_byte; off += PAGE_SIZE) {
		if (get_prot_at(off) != prot_in)
			return false;
	}
	for (off = end_byte; off < SZ_1G; off += PAGE_SIZE) {
		if (get_prot_at(off) != prot_out)
			return false;
	}
	return true;
}

/* Start with 1G granule, overwrite the whole 1G. */
static int __init test_set_fmpt__fmpt_to_fmpt_whole(void)
{
	init_fmpt(MPT_PROT_NONE, /*gran_1g*/ true);
	__set_fmpt_range(&g_fmpt, 0, SZ_1G, MPT_PROT_R);
	ASSERT(g_fmpt.flags == MPT_UPDATE_L1);
	ASSERT(g_fmpt.gran_1g);
	ASSERT(g_fmpt.prot == MPT_PROT_R);
	return 0;
}

/* Start with 1G granule, overwrite the whole 1G with the same prot. */
static int __init test_set_fmpt__fmpt_no_change_whole(void)
{
	init_fmpt(MPT_PROT_R, /*gran_1g*/ true);
	__set_fmpt_range(&g_fmpt, 0, SZ_1G, MPT_PROT_R);
	ASSERT(g_fmpt.flags == 0);
	ASSERT(g_fmpt.gran_1g);
	ASSERT(g_fmpt.prot == MPT_PROT_R);
	return 0;
}

/* Start with 1G granule, partially overwrite with the same prot. */
static int __init test_set_fmpt__fmpt_no_change_partial(void)
{
	init_fmpt(MPT_PROT_R, /*gran_1g*/ true);
	__set_fmpt_range(&g_fmpt, 0, PAGE_SIZE, MPT_PROT_R);
	ASSERT(g_fmpt.flags == 0);
	ASSERT(g_fmpt.gran_1g);
	ASSERT(g_fmpt.prot == MPT_PROT_R);
	return 0;
}

/* Convert from 1G to PAGE_SIZE granule. */
static int __init test_set_fmpt__fmpt_to_smpt(void)
{
	size_t start = 5 * SMPT_WORD_BYTE_RANGE / 2;
	size_t end = 20 * SMPT_WORD_BYTE_RANGE;

	init_fmpt(MPT_PROT_R, /*gran_1g*/ true);
	__set_fmpt_range(&g_fmpt, start, end, MPT_PROT_RW);
	ASSERT(g_fmpt.flags == (MPT_UPDATE_L1 | MPT_UPDATE_L2));
	ASSERT(!g_fmpt.gran_1g);
	return check_smpt(start, end, MPT_PROT_R, MPT_PROT_RW) ? 0 : 1;
}

/* Convert from PAGE_SIZE to 1G granule by overwriting the whole 1G. */
static int __init test_set_fmpt__smpt_to_fmpt_whole(void)
{
	init_fmpt(MPT_PROT_NONE, /*gran_1g*/ false);
	__set_fmpt_range(&g_fmpt, 0, SZ_1G, MPT_PROT_R);
	ASSERT(g_fmpt.flags == MPT_UPDATE_L1);
	ASSERT(g_fmpt.gran_1g);
	ASSERT(g_fmpt.prot == MPT_PROT_R);
	return 0;
}

/* Convert from PAGE_SIZE to 1G granule by making the SMPT uniform. */
static int __init test_set_fmpt__smpt_to_fmpt_partial(void)
{
	size_t start = 5 * SMPT_WORD_BYTE_RANGE / 2;
	size_t end = 20 * SMPT_WORD_BYTE_RANGE;

	/* Create SMPT with all PROT_W except a small subrange. */
	init_fmpt(MPT_PROT_W, /*gran_1g*/ false);
	__set_smpt_range(g_smpt, start, end, MPT_PROT_RW);

	/* Fill the subrange with PROT_W to make the SMPT uniform. */
	__set_fmpt_range(&g_fmpt, start, end, MPT_PROT_W);
	ASSERT(g_fmpt.flags == MPT_UPDATE_L1);
	ASSERT(g_fmpt.gran_1g);
	ASSERT(g_fmpt.prot == MPT_PROT_W);
	return 0;
}

/* Keep PAGE_SIZE granule when SMPT not uniform after update. */
static int __init test_set_fmpt__smpt_to_smpt(void)
{
	size_t start = SZ_1G - SMPT_GRAN;
	size_t end = SZ_1G;

	init_fmpt(MPT_PROT_NONE, /*gran_1g*/ false);
	ASSERT(__is_smpt_uniform(g_smpt, MPT_PROT_NONE));

	/* Fill the subrange with PROT_W to make the SMPT uniform. */
	__set_fmpt_range(&g_fmpt, start, end, MPT_PROT_RW);
	ASSERT(g_fmpt.flags == MPT_UPDATE_L2);
	ASSERT(!g_fmpt.gran_1g);
	ASSERT(!__is_smpt_uniform(g_smpt, MPT_PROT_NONE));
	return 0;
}

static int __init __test_set_smpt(size_t start_byte, size_t end_byte)
{
	init_smpt(MPT_PROT_NONE);
	__set_smpt_range(g_smpt, start_byte, end_byte, MPT_PROT_W);
	return check_smpt(start_byte, end_byte, MPT_PROT_NONE, MPT_PROT_W) ? 0 : 1;
}

/* Range within one SMPT word, force a fallback to __set_smpt_range_slow. */
static int __init test_set_smpt__within_one_word(void)
{
	return __test_set_smpt(3 * SMPT_WORD_BYTE_RANGE + 5 * PAGE_SIZE,
			       3 * SMPT_WORD_BYTE_RANGE + 6 * PAGE_SIZE);
}

/* No whole SMPT word, force a fallback to __set_smpt_range_slow. */
static int __init test_set_smpt__no_whole_word(void)
{
	return __test_set_smpt(3 * SMPT_WORD_BYTE_RANGE + 5 * PAGE_SIZE,
			       4 * SMPT_WORD_BYTE_RANGE + 2 * PAGE_SIZE);
}

/* Both start and end aligned to SMPT word. */
static int __init test_set_smpt__no_prologue_or_epilogue(void)
{
	return __test_set_smpt(10 * SMPT_WORD_BYTE_RANGE,
			       20 * SMPT_WORD_BYTE_RANGE);
}

/* Start not aligned to SMPT word. */
static int __init test_set_smpt__prologue(void)
{
	return __test_set_smpt(17 * SMPT_WORD_BYTE_RANGE / 2,
			       20 * SMPT_WORD_BYTE_RANGE);
}

/* End not aligned to SMPT word. */
static int __init test_set_smpt__epilogue(void)
{
	return __test_set_smpt(0, 17 * SMPT_WORD_BYTE_RANGE / 2);
}

/* Neither start nor end aligned to SMPT word. */
static int __init test_set_smpt__prologue_and_epilogue(void)
{
	return __test_set_smpt(17 * SMPT_WORD_BYTE_RANGE / 2,
			       31 * SMPT_WORD_BYTE_RANGE / 2);
}

static int __init __test_set_smpt_slow(size_t start_byte, size_t end_byte)
{
	init_smpt(MPT_PROT_NONE);
	__set_smpt_range_slow(g_smpt, start_byte, end_byte, MPT_PROT_RW);
	return check_smpt(start_byte, end_byte, MPT_PROT_NONE, MPT_PROT_RW) ? 0 : 1;
}

static int __init test_set_smpt_slow__empty_word_align(void)
{
	return __test_set_smpt_slow(3 * SMPT_WORD_BYTE_RANGE,
				    3 * SMPT_WORD_BYTE_RANGE);
}

static int __init test_set_smpt_slow__empty_page_align(void)
{
	return __test_set_smpt_slow(3 * SMPT_WORD_BYTE_RANGE + PAGE_SIZE,
				    3 * SMPT_WORD_BYTE_RANGE + PAGE_SIZE);
}

static int __init test_set_smpt_slow__one_whole_word(void)
{
	return __test_set_smpt_slow(3 * SMPT_WORD_BYTE_RANGE,
				    4 * SMPT_WORD_BYTE_RANGE);
}

static int __init test_set_smpt_slow__one_partial_word(void)
{
	return __test_set_smpt_slow(3 * SMPT_WORD_BYTE_RANGE + PAGE_SIZE,
				    4 * SMPT_WORD_BYTE_RANGE - PAGE_SIZE);
}

static int __init test_set_smpt_slow__multiple_whole_words(void)
{
	return __test_set_smpt_slow(13 * SMPT_WORD_BYTE_RANGE,
				    17 * SMPT_WORD_BYTE_RANGE);
}

static int __init test_set_smpt_slow__multiple_partial_words(void)
{
	return __test_set_smpt_slow((13 * 2 + 1) * SMPT_WORD_BYTE_RANGE / 2,
				    (17 * 4 + 1) * SMPT_WORD_BYTE_RANGE / 4);
}

static void __init selftest(void)
{
	KSTM_CHECK_ZERO(test_set_fmpt__fmpt_to_fmpt_whole());
	KSTM_CHECK_ZERO(test_set_fmpt__fmpt_no_change_whole());
	KSTM_CHECK_ZERO(test_set_fmpt__fmpt_no_change_partial());
	KSTM_CHECK_ZERO(test_set_fmpt__fmpt_to_smpt());
	KSTM_CHECK_ZERO(test_set_fmpt__smpt_to_fmpt_whole());
	KSTM_CHECK_ZERO(test_set_fmpt__smpt_to_fmpt_partial());
	KSTM_CHECK_ZERO(test_set_fmpt__smpt_to_smpt());

	KSTM_CHECK_ZERO(test_set_smpt__within_one_word());
	KSTM_CHECK_ZERO(test_set_smpt__no_whole_word());
	KSTM_CHECK_ZERO(test_set_smpt__no_prologue_or_epilogue());
	KSTM_CHECK_ZERO(test_set_smpt__prologue());
	KSTM_CHECK_ZERO(test_set_smpt__epilogue());
	KSTM_CHECK_ZERO(test_set_smpt__prologue_and_epilogue());

	KSTM_CHECK_ZERO(test_set_smpt_slow__empty_word_align());
	KSTM_CHECK_ZERO(test_set_smpt_slow__empty_page_align());
	KSTM_CHECK_ZERO(test_set_smpt_slow__one_whole_word());
	KSTM_CHECK_ZERO(test_set_smpt_slow__one_partial_word());
	KSTM_CHECK_ZERO(test_set_smpt_slow__multiple_whole_words());
	KSTM_CHECK_ZERO(test_set_smpt_slow__multiple_partial_words());
}

KSTM_MODULE_LOADERS(test_kvm_s2mpu);
MODULE_AUTHOR("David Brazdil <dbrazdil@google.com>");
MODULE_LICENSE("GPL v2");
