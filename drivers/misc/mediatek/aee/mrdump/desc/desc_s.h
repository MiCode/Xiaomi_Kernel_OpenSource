/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#define E_EX(member) e_rrr_##member
#define E_PER_CPU_EX(member) e_rrr_per_cpu_##member
#define DF(member, fmt) E_EX(member)
#define DF_A(member, fmt, total) E_EX(member)
#define DF_S(member, fmt) E_EX(member)
#define DF_PER_CPU(member, fmt) E_PER_CPU_EX(member)
enum {
	#include "desc_def.h"
	E_EX(num_total),
};

enum {
	#include "desc_def_per_cpu.h"
	E_PER_CPU_EX(num_total),
};

enum DESC_DATA_TYPE_SPECIAL {
	D_SEG_SINGLE_BYTE = 0xabe,
	D_SINGLE_VALUE = 0xaee,
	D_PER_CPU_VALUE = 0xaed,
};

#define FMT_MAX_LEN 128
struct rrr_desc_data {
	/* D_SINGLE_VARIABLE	: single;
	 * D_PER_CPU_VALUE	: per cpu;
	 * D_SEG_SINGLE_BYTE	: segmented into single byte;
	 * > 0			: array
	 * == 0			: invalid
	 */
	uint32_t type;
	/* offset in structure of last_reboot_reason */
	uint32_t off;
	/* varaible type e.g. char size=1; uint32_t size=4; uint64_t size=8; */
	uint32_t size;
	/* real size of fmt string for seq_printf */
	uint32_t fmtsize;
	char fmt[FMT_MAX_LEN];
};

struct rrr_desc {
	uint32_t magic;
	uint32_t version;
	uint32_t off_pl;
	uint32_t off_linux;
	uint32_t off_desc_data;
	uint32_t total;
	uint32_t per_cpu_total;
	uint32_t cpu_num;
	uint32_t fmt_max_len;
	uint32_t reserve[6];
	uint32_t magic_end;
	struct rrr_desc_data desc[E_EX(num_total)];
	struct rrr_desc_data per_cpu_desc[E_PER_CPU_EX(num_total)];
};

#undef DF
#define DF(member, fmt) \
	[E_EX(member)] = fmt
#undef DF_A
#define DF_A(member, fmt, total) \
	[E_EX(member)] = fmt
#undef DF_S
#define DF_S(member, fmt) \
	[E_EX(member)] = fmt
#undef DF_PER_CPU
#define DF_PER_CPU(member, fmt) \
	[E_PER_CPU_EX(member)] = fmt
static char *buf[] = {
	#include "desc_def.h"
};

static char *per_cpu_buf[] = {
	#include "desc_def_per_cpu.h"
};

static struct last_reboot_reason dummy_var;
#define DF_C(type, member) { \
	type, \
	offsetof(struct last_reboot_reason, member), \
	sizeof(typeof(dummy_var.member)) \
	}

#undef DF
#define DF(member, fmt) \
	[E_EX(member)] = DF_C(D_SINGLE_VALUE, member)
#undef DF_A
#define DF_A(member, fmt, total) \
	[E_EX(member)] = DF_C(total, member[0])
#undef DF_S
#define DF_S(member, fmt) \
	[E_EX(member)] = DF_C(D_SEG_SINGLE_BYTE, member)
#undef DF_PER_CPU
#define DF_PER_CPU(member, fmt) \
	[E_PER_CPU_EX(member)] = DF_C(D_PER_CPU_VALUE, member[0])
static struct rrr_desc /*__maybe_unused*/ idesc_init = {
	REBOOT_REASON_SIG,
	0x100,
	sizeof(struct mboot_params_buffer),
	0x0,
	(offsetof(struct rrr_desc, desc)),
	E_EX(num_total),
	E_PER_CPU_EX(num_total),
	AEE_MTK_CPU_NUMS,
	FMT_MAX_LEN,
	{0},
	REBOOT_REASON_SIG,
	{
		#include "desc_def.h"
	},
	/* per cpu */
	{
		#include "desc_def_per_cpu.h"
	}
};

static struct rrr_desc *idesc;

#define IDESC_ADDR ((unsigned long)idesc)
#define IDESC_SIZE (sizeof(struct rrr_desc))
static uint32_t start_pos;
#define IDESC_START_POS ((unsigned long)&start_pos)

static int mboot_params_init_desc(uint32_t off_linux)
{
	int i;
	size_t len;

	idesc = kzalloc(IDESC_SIZE, GFP_KERNEL);
	if (!idesc)
		return -1;

	memcpy(idesc, &idesc_init, IDESC_SIZE);
	idesc->off_linux = off_linux;
	idesc->cpu_num = nr_cpu_ids;
	for (i = 0; i < E_EX(num_total); i++) {
		if (buf[i]) {
			len = strlen(buf[i]);
			if (len < FMT_MAX_LEN) {
				strncpy(idesc->desc[i].fmt,
					buf[i], FMT_MAX_LEN);
				idesc->desc[i].fmt[FMT_MAX_LEN - 1] = '\0';
				idesc->desc[i].fmtsize = len;
			} else {
				/* idesc->desc[i].size = i; */
				idesc->desc[i].type = 0x0;
			}
		} else {
			/* idesc->desc[i].size = i; */
			idesc->desc[i].type = 0x0;
		}
	}
	for (i = 0; i < E_PER_CPU_EX(num_total); i++) {
		if (per_cpu_buf[i]) {
			len = strlen(per_cpu_buf[i]);
			if (len < FMT_MAX_LEN) {
				strncpy(idesc->per_cpu_desc[i].fmt,
					per_cpu_buf[i], FMT_MAX_LEN);
				idesc->per_cpu_desc[i].fmt[FMT_MAX_LEN - 1] =
					'\0';
				idesc->per_cpu_desc[i].fmtsize =
					strlen(idesc->per_cpu_desc[i].fmt);
			} else {
				/* idesc->per_cpu_desc[i].size = i; */
				idesc->per_cpu_desc[i].type = 0x0;
			}
		} else {
			/* idesc->per_cpu_desc[i].size = i; */
			idesc->per_cpu_desc[i].type = 0x0;
		}
	}
	return 0;
}
