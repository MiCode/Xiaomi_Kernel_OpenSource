/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/vmalloc.h>
#include <mach/board.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "kgsl_cffdump.h"
#include "kgsl_pwrctrl.h"
#include "adreno_trace.h"

#include "a2xx_reg.h"
#include "a3xx_reg.h"

#define INVALID_RB_CMD 0xaaaaaaaa
#define NUM_DWORDS_OF_RINGBUFFER_HISTORY 100

struct pm_id_name {
	enum adreno_regs id;
	char name[9];
};

static const struct pm_id_name pm0_types[] = {
	{ADRENO_REG_PA_SC_AA_CONFIG,		"RPASCAAC"},
	{ADRENO_REG_RBBM_PM_OVERRIDE2,		"RRBBPMO2"},
	{ADRENO_REG_SCRATCH_REG2,		"RSCRTRG2"},
	{ADRENO_REG_SQ_GPR_MANAGEMENT,		"RSQGPRMN"},
	{ADRENO_REG_SQ_INST_STORE_MANAGMENT,	"RSQINSTS"},
	{ADRENO_REG_TC_CNTL_STATUS,		"RTCCNTLS"},
	{ADRENO_REG_TP0_CHICKEN,		"RTP0CHCK"},
	{ADRENO_REG_CP_TIMESTAMP,		"CP_TM_ST"},
};

static const struct pm_id_name pm3_types[] = {
	{CP_COND_EXEC,			"CND_EXEC"},
	{CP_CONTEXT_UPDATE,		"CX__UPDT"},
	{CP_DRAW_INDX,			"DRW_NDX_"},
	{CP_DRAW_INDX_BIN,		"DRW_NDXB"},
	{CP_EVENT_WRITE,		"EVENT_WT"},
	{CP_MEM_WRITE,			"MEM_WRIT"},
	{CP_IM_LOAD,			"IN__LOAD"},
	{CP_IM_LOAD_IMMEDIATE,		"IM_LOADI"},
	{CP_IM_STORE,			"IM_STORE"},
	{CP_INDIRECT_BUFFER_PFE,	"IND_BUF_"},
	{CP_INDIRECT_BUFFER_PFD,	"IND_BUFP"},
	{CP_INTERRUPT,			"PM4_INTR"},
	{CP_INVALIDATE_STATE,		"INV_STAT"},
	{CP_LOAD_CONSTANT_CONTEXT,	"LD_CN_CX"},
	{CP_ME_INIT,			"ME__INIT"},
	{CP_NOP,			"PM4__NOP"},
	{CP_REG_RMW,			"REG__RMW"},
	{CP_REG_TO_MEM,		"REG2_MEM"},
	{CP_SET_BIN_BASE_OFFSET,	"ST_BIN_O"},
	{CP_SET_CONSTANT,		"ST_CONST"},
	{CP_SET_PROTECTED_MODE,	"ST_PRT_M"},
	{CP_SET_SHADER_BASES,		"ST_SHD_B"},
	{CP_WAIT_FOR_IDLE,		"WAIT4IDL"},
	{CP_WAIT_FOR_ME,		"WAIT4ME"},
	{CP_WAIT_REG_EQ,		"WAITRGEQ"},
};

static const struct pm_id_name pm3_nop_values[] = {
	{KGSL_CONTEXT_TO_MEM_IDENTIFIER,	"CTX_SWCH"},
	{KGSL_CMD_IDENTIFIER,			"CMD__EXT"},
	{KGSL_CMD_INTERNAL_IDENTIFIER,		"CMD__INT"},
	{KGSL_START_OF_IB_IDENTIFIER,		"IB_START"},
	{KGSL_END_OF_IB_IDENTIFIER,		"IB___END"},
	{KGSL_START_OF_PROFILE_IDENTIFIER,	"PRO_STRT"},
	{KGSL_END_OF_PROFILE_IDENTIFIER,	"PRO__END"},
};

static uint32_t adreno_is_pm4_len(uint32_t word)
{
	if (word == INVALID_RB_CMD)
		return 0;

	return (word >> 16) & 0x3FFF;
}

static bool adreno_is_pm4_type(struct kgsl_device *device, uint32_t word)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i;

	if (word == INVALID_RB_CMD)
		return 1;

	if (adreno_is_pm4_len(word) > 16)
		return 0;

	if ((word & (3<<30)) == CP_TYPE0_PKT) {
		for (i = 0; i < ARRAY_SIZE(pm0_types); ++i) {
			if ((word & 0x7FFF) == adreno_getreg(adreno_dev,
							pm0_types[i].id))
				return 1;
		}
		return 0;
	}
	if ((word & (3<<30)) == CP_TYPE3_PKT) {
		for (i = 0; i < ARRAY_SIZE(pm3_types); ++i) {
			if ((word & 0xFFFF) == (pm3_types[i].id << 8))
				return 1;
		}
		return 0;
	}
	return 0;
}

static const char *adreno_pm4_name(uint32_t word)
{
	int i;

	if (word == INVALID_RB_CMD)
		return "--------";

	if ((word & (3<<30)) == CP_TYPE0_PKT) {
		for (i = 0; i < ARRAY_SIZE(pm0_types); ++i) {
			if ((word & 0x7FFF) == pm0_types[i].id)
				return pm0_types[i].name;
		}
		return "????????";
	}
	if ((word & (3<<30)) == CP_TYPE3_PKT) {
		for (i = 0; i < ARRAY_SIZE(pm3_types); ++i) {
			if ((word & 0xFFFF) == (pm3_types[i].id << 8))
				return pm3_types[i].name;
		}
		return "????????";
	}
	return "????????";
}

static bool adreno_is_pm3_nop_value(uint32_t word)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pm3_nop_values); ++i) {
		if (word == pm3_nop_values[i].id)
			return 1;
	}
	return 0;
}

static const char *adreno_pm3_nop_name(uint32_t word)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pm3_nop_values); ++i) {
		if (word == pm3_nop_values[i].id)
			return pm3_nop_values[i].name;
	}
	return "????????";
}

static void adreno_dump_regs(struct kgsl_device *device,
			   const int *registers, int size)
{
	int range = 0, offset = 0;

	for (range = 0; range < size; range++) {
		/* start and end are in dword offsets */
		int start = registers[range * 2];
		int end = registers[range * 2 + 1];

		unsigned char linebuf[32 * 3 + 2 + 32 + 1];
		int linelen, i;

		for (offset = start; offset <= end; offset += linelen) {
			unsigned int regvals[32/4];
			linelen = min(end+1-offset, 32/4);

			for (i = 0; i < linelen; ++i)
				kgsl_regread(device, offset+i, regvals+i);

			hex_dump_to_buffer(regvals, linelen*4, 32, 4,
				linebuf, sizeof(linebuf), 0);
			KGSL_LOG_DUMP(device,
				"REG: %5.5X: %s\n", offset, linebuf);
		}
	}
}

static void dump_ib(struct kgsl_device *device, char *buffId,
	phys_addr_t pt_base, uint32_t base_offset, uint32_t ib_base,
	uint32_t ib_size, bool dump)
{
	struct kgsl_mem_entry *ent = NULL;
	uint8_t *base_addr = adreno_convertaddr(device, pt_base,
		ib_base, ib_size*sizeof(uint32_t), &ent);

	if (base_addr && dump)
		print_hex_dump(KERN_ERR, buffId, DUMP_PREFIX_OFFSET,
				 32, 4, base_addr, ib_size*4, 0);
	else
		KGSL_LOG_DUMP(device, "%s base:%8.8X  ib_size:%d  "
			"offset:%5.5X%s\n",
			buffId, ib_base, ib_size*4, base_offset,
			base_addr ? "" : " [Invalid]");
	if (ent) {
		kgsl_memdesc_unmap(&ent->memdesc);
		kgsl_mem_entry_put(ent);
	}
}

#define IB_LIST_SIZE	64
struct ib_list {
	int count;
	uint32_t bases[IB_LIST_SIZE];
	uint32_t sizes[IB_LIST_SIZE];
	uint32_t offsets[IB_LIST_SIZE];
};

static void dump_ib1(struct kgsl_device *device, phys_addr_t pt_base,
			uint32_t base_offset,
			uint32_t ib1_base, uint32_t ib1_size,
			struct ib_list *ib_list, bool dump)
{
	int i, j;
	uint32_t value;
	uint32_t *ib1_addr;
	struct kgsl_mem_entry *ent = NULL;

	dump_ib(device, "IB1:", pt_base, base_offset, ib1_base,
		ib1_size, dump);

	/* fetch virtual address for given IB base */
	ib1_addr = (uint32_t *)adreno_convertaddr(device, pt_base,
		ib1_base, ib1_size*sizeof(uint32_t), &ent);
	if (!ib1_addr)
		return;

	for (i = 0; i+3 < ib1_size; ) {
		value = ib1_addr[i++];
		if (adreno_cmd_is_ib(value)) {
			uint32_t ib2_base = ib1_addr[i++];
			uint32_t ib2_size = ib1_addr[i++];

			/* find previous match */
			for (j = 0; j < ib_list->count; ++j)
				if (ib_list->sizes[j] == ib2_size
					&& ib_list->bases[j] == ib2_base)
					break;

			if (j < ib_list->count || ib_list->count
				>= IB_LIST_SIZE)
				continue;

			/* store match */
			ib_list->sizes[ib_list->count] = ib2_size;
			ib_list->bases[ib_list->count] = ib2_base;
			ib_list->offsets[ib_list->count] = i<<2;
			++ib_list->count;
		}
	}
	if (ent) {
		kgsl_memdesc_unmap(&ent->memdesc);
		kgsl_mem_entry_put(ent);
	}
}

static void adreno_dump_rb_buffer(struct kgsl_device *device, const void *buf,
		size_t len, char *linebuf, size_t linebuflen, int *argp)
{
	const u32 *ptr4 = buf;
	const int ngroups = len;
	int lx = 0, j;
	bool nxsp = 1;

	for (j = 0; j < ngroups; j++) {
		if (*argp < 0) {
			lx += scnprintf(linebuf + lx, linebuflen - lx, " <");
			*argp = -*argp;
		} else if (nxsp)
			lx += scnprintf(linebuf + lx, linebuflen - lx, "  ");
		else
			nxsp = 1;
		if (!*argp && adreno_is_pm4_type(device, ptr4[j])) {
			lx += scnprintf(linebuf + lx, linebuflen - lx,
				"%s", adreno_pm4_name(ptr4[j]));
			*argp = -(adreno_is_pm4_len(ptr4[j])+1);
		} else {
			if (adreno_is_pm3_nop_value(ptr4[j]))
				lx += scnprintf(linebuf + lx, linebuflen - lx,
					"%s", adreno_pm3_nop_name(ptr4[j]));
			else
				lx += scnprintf(linebuf + lx, linebuflen - lx,
					"%8.8X", ptr4[j]);

			if (*argp > 1)
				--*argp;
			else if (*argp == 1) {
				*argp = 0;
				nxsp = 0;
				lx += scnprintf(linebuf + lx, linebuflen - lx,
					"> ");
			}
		}
	}
	linebuf[lx] = '\0';
}

static bool adreno_rb_use_hex(void)
{
#ifdef CONFIG_MSM_KGSL_PSTMRTMDMP_RB_HEX
	return 1;
#else
	return 0;
#endif
}

void adreno_dump_rb(struct kgsl_device *device, const void *buf,
			 size_t len, int start, int size)
{
	const uint32_t *ptr = buf;
	int i, remaining, args = 0;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];
	const int rowsize = 8;

	len >>= 2;
	remaining = len;
	for (i = 0; i < len; i += rowsize) {
		int linelen = min(remaining, rowsize);
		remaining -= rowsize;

		if (adreno_rb_use_hex())
			hex_dump_to_buffer(ptr+i, linelen*4, rowsize*4, 4,
				linebuf, sizeof(linebuf), 0);
		else
			adreno_dump_rb_buffer(device, ptr+i, linelen, linebuf,
				sizeof(linebuf), &args);
		KGSL_LOG_DUMP(device,
			"RB: %4.4X:%s\n", (start+i)%size, linebuf);
	}
}

static int adreno_dump_fields_line(struct kgsl_device *device,
				 const char *start, char *str, int slen,
				 const struct log_field **lines,
				 int num)
{
	const struct log_field *l = *lines;
	int sptr, count  = 0;

	sptr = snprintf(str, slen, "%s", start);

	for (  ; num && sptr < slen; num--, l++) {
		int ilen = strlen(l->display);

		if (!l->show)
			continue;

		if (count)
			ilen += strlen("  | ");

		if (ilen > (slen - sptr))
			break;

		if (count++)
			sptr += snprintf(str + sptr, slen - sptr, " | ");

		sptr += snprintf(str + sptr, slen - sptr, "%s", l->display);
	}

	KGSL_LOG_DUMP(device, "%s\n", str);

	*lines = l;
	return num;
}

void adreno_dump_fields(struct kgsl_device *device,
			     const char *start, const struct log_field *lines,
			     int num)
{
	char lb[90];
	const char *sstr = start;

	lb[sizeof(lb)  - 1] = '\0';

	while (num) {
		int ret = adreno_dump_fields_line(device, sstr, lb,
			sizeof(lb) - 1, &lines, num);

		if (ret == num)
			break;

		num = ret;
		sstr = "        ";
	}
}
EXPORT_SYMBOL(adreno_dump_fields);

int adreno_dump(struct kgsl_device *device, int manual)
{
	unsigned int cp_ib1_base;
	unsigned int cp_ib2_base;
	phys_addr_t pt_base, cur_pt_base;
	unsigned int cp_rb_base, cp_rb_ctrl, rb_count;
	unsigned int cp_rb_wptr, cp_rb_rptr;
	unsigned int i;
	int result = 0;
	uint32_t *rb_copy;
	const uint32_t *rb_vaddr;
	int num_item = 0;
	int read_idx, write_idx;
	unsigned int ts_processed = 0xdeaddead;
	struct kgsl_context *context;
	unsigned int context_id;

	static struct ib_list ib_list;

	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	int num_iommu_units = 0;

	mb();

	msm_clk_dump_debug_info();

	if (adreno_dev->gpudev->postmortem_dump)
		adreno_dev->gpudev->postmortem_dump(adreno_dev);

	pt_base = kgsl_mmu_get_current_ptbase(&device->mmu);
	cur_pt_base = pt_base;

	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_BASE),
		&cp_rb_base);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_CNTL),
		&cp_rb_ctrl);
	rb_count = 2 << (cp_rb_ctrl & (BIT(6) - 1));
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_RPTR),
		&cp_rb_rptr);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_WPTR),
		&cp_rb_wptr);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB1_BASE),
		&cp_ib1_base);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB2_BASE),
		&cp_ib2_base);

	kgsl_sharedmem_readl(&device->memstore,
			(unsigned int *) &context_id,
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
				current_context));

	context = kgsl_context_get(device, context_id);

	if (context) {
		ts_processed = kgsl_readtimestamp(device, context,
						  KGSL_TIMESTAMP_RETIRED);
		KGSL_LOG_DUMP(device, "FT CTXT: %d  TIMESTM RTRD: %08X\n",
				context->id, ts_processed);
	} else
		KGSL_LOG_DUMP(device, "BAD CTXT: %d\n", context_id);

	kgsl_context_put(context);

	num_item = adreno_ringbuffer_count(&adreno_dev->ringbuffer,
						cp_rb_rptr);
	if (num_item <= 0)
		KGSL_LOG_POSTMORTEM_WRITE(device, "Ringbuffer is Empty.\n");

	rb_copy = vmalloc(rb_count<<2);
	if (!rb_copy) {
		KGSL_LOG_POSTMORTEM_WRITE(device,
			"vmalloc(%d) failed\n", rb_count << 2);
		result = -ENOMEM;
		goto end;
	}

	KGSL_LOG_DUMP(device, "RB: rd_addr:%8.8x  rb_size:%d  num_item:%d\n",
		cp_rb_base, rb_count<<2, num_item);

	if (adreno_dev->ringbuffer.buffer_desc.gpuaddr != cp_rb_base)
		KGSL_LOG_POSTMORTEM_WRITE(device,
			"rb address mismatch, should be 0x%08x\n",
			adreno_dev->ringbuffer.buffer_desc.gpuaddr);

	rb_vaddr = adreno_dev->ringbuffer.buffer_desc.hostptr;
	if (!rb_vaddr) {
		KGSL_LOG_POSTMORTEM_WRITE(device,
			"rb has no kernel mapping!\n");
		goto error_vfree;
	}

	read_idx = (int)cp_rb_rptr - NUM_DWORDS_OF_RINGBUFFER_HISTORY;
	if (read_idx < 0)
		read_idx += rb_count;
	write_idx = (int)cp_rb_wptr + 16;
	if (write_idx > rb_count)
		write_idx -= rb_count;
	num_item += NUM_DWORDS_OF_RINGBUFFER_HISTORY+16;
	if (num_item > rb_count)
		num_item = rb_count;
	if (write_idx >= read_idx)
		memcpy(rb_copy, rb_vaddr+read_idx, num_item<<2);
	else {
		int part1_c = rb_count-read_idx;
		memcpy(rb_copy, rb_vaddr+read_idx, part1_c<<2);
		memcpy(rb_copy+part1_c, rb_vaddr, (num_item-part1_c)<<2);
	}

	/* extract the latest ib commands from the buffer */
	ib_list.count = 0;
	i = 0;
	/* get the register mapped array in case we are using IOMMU */
	num_iommu_units = kgsl_mmu_get_num_iommu_units(&device->mmu);
	for (read_idx = 0; read_idx < num_item; ) {
		uint32_t this_cmd = rb_copy[read_idx++];
		if (adreno_cmd_is_ib(this_cmd)) {
			uint32_t ib_addr = rb_copy[read_idx++];
			uint32_t ib_size = rb_copy[read_idx++];
			dump_ib1(device, cur_pt_base, (read_idx-3)<<2, ib_addr,
				ib_size, &ib_list, 0);
			for (; i < ib_list.count; ++i)
				dump_ib(device, "IB2:", cur_pt_base,
					ib_list.offsets[i],
					ib_list.bases[i],
					ib_list.sizes[i], 0);
		} else if (this_cmd == cp_type0_packet(MH_MMU_PT_BASE, 1) ||
			(num_iommu_units && this_cmd ==
			kgsl_mmu_get_reg_gpuaddr(&device->mmu, 0,
						KGSL_IOMMU_CONTEXT_USER,
						KGSL_IOMMU_CTX_TTBR0)) ||
			(num_iommu_units && this_cmd == cp_type0_packet(
						kgsl_mmu_get_reg_ahbaddr(
						&device->mmu, 0,
						KGSL_IOMMU_CONTEXT_USER,
						KGSL_IOMMU_CTX_TTBR0), 1))) {
			KGSL_LOG_DUMP(device,
				"Current pagetable: %x\t pagetable base: %pa\n",
				kgsl_mmu_get_ptname_from_ptbase(&device->mmu,
								cur_pt_base),
				&cur_pt_base);

			/* Set cur_pt_base to the new pagetable base */
			cur_pt_base = rb_copy[read_idx++];

			KGSL_LOG_DUMP(device,
				"New pagetable: %x\t pagetable base: %pa\n",
				kgsl_mmu_get_ptname_from_ptbase(&device->mmu,
								cur_pt_base),
				&cur_pt_base);
		}
	}

	/* Restore cur_pt_base back to the pt_base of
	   the process in whose context the GPU hung */
	cur_pt_base = pt_base;

	read_idx = (int)cp_rb_rptr - NUM_DWORDS_OF_RINGBUFFER_HISTORY;
	if (read_idx < 0)
		read_idx += rb_count;
	KGSL_LOG_DUMP(device,
		"RB: addr=%8.8x  window:%4.4x-%4.4x, start:%4.4x\n",
		cp_rb_base, cp_rb_rptr, cp_rb_wptr, read_idx);
	adreno_dump_rb(device, rb_copy, num_item<<2, read_idx, rb_count);

	if (device->pm_ib_enabled) {
		for (read_idx = NUM_DWORDS_OF_RINGBUFFER_HISTORY;
			read_idx >= 0; --read_idx) {
			uint32_t this_cmd = rb_copy[read_idx];
			if (adreno_cmd_is_ib(this_cmd)) {
				uint32_t ib_addr = rb_copy[read_idx+1];
				uint32_t ib_size = rb_copy[read_idx+2];
				if (ib_size && cp_ib1_base == ib_addr) {
					KGSL_LOG_DUMP(device,
						"IB1: base:%8.8X  "
						"count:%d\n", ib_addr, ib_size);
					dump_ib(device, "IB1: ", cur_pt_base,
						read_idx<<2, ib_addr, ib_size,
						1);
				}
			}
		}
		for (i = 0; i < ib_list.count; ++i) {
			uint32_t ib_size = ib_list.sizes[i];
			uint32_t ib_offset = ib_list.offsets[i];
			if (ib_size && cp_ib2_base == ib_list.bases[i]) {
				KGSL_LOG_DUMP(device,
					"IB2: base:%8.8X  count:%d\n",
					cp_ib2_base, ib_size);
				dump_ib(device, "IB2: ", cur_pt_base, ib_offset,
					ib_list.bases[i], ib_size, 1);
			}
		}
	}

	/* Dump the registers if the user asked for it */
	if (device->pm_regs_enabled) {
		if (adreno_is_a20x(adreno_dev))
			adreno_dump_regs(device, a200_registers,
					a200_registers_count);
		else if (adreno_is_a22x(adreno_dev))
			adreno_dump_regs(device, a220_registers,
					a220_registers_count);
		else if (adreno_is_a225(adreno_dev))
			adreno_dump_regs(device, a225_registers,
				a225_registers_count);
		else if (adreno_is_a3xx(adreno_dev)) {
			adreno_dump_regs(device, a3xx_registers,
					a3xx_registers_count);

			if (adreno_is_a330(adreno_dev) ||
				adreno_is_a305b(adreno_dev))
				adreno_dump_regs(device, a330_registers,
					a330_registers_count);
		}
	}

error_vfree:
	vfree(rb_copy);
end:
	/* Restart the dispatcher after a manually triggered dump */
	if (manual)
		adreno_dispatcher_start(device);

	return result;
}
