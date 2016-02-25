/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "kgsl_snapshot.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "a3xx_reg.h"
#include "adreno_cp_parser.h"

#define MAX_IB_OBJS 1000
#define NUM_SET_DRAW_GROUPS 32

struct set_draw_state {
	uint64_t cmd_stream_addr;
	uint64_t cmd_stream_dwords;
};

/* List of variables used when parsing an IB */
struct ib_parser_variables {
	/* List of registers containing addresses and their sizes */
	unsigned int cp_addr_regs[ADRENO_CP_ADDR_MAX];
	/* 32 groups of command streams in set draw state packets */
	struct set_draw_state set_draw_groups[NUM_SET_DRAW_GROUPS];
};

/*
 * Used for locating shader objects. This array holds the unit size of shader
 * objects based on type and block of shader. The type can be 0 or 1 hence there
 * are 2 columns and block can be 0-7 hence 7 rows.
 */
static int load_state_unit_sizes[7][2] = {
	{ 2, 4 },
	{ 0, 1 },
	{ 2, 4 },
	{ 0, 1 },
	{ 8, 2 },
	{ 8, 2 },
	{ 8, 2 },
};

static int adreno_ib_find_objs(struct kgsl_device *device,
				struct kgsl_process_private *process,
				uint64_t gpuaddr, uint64_t dwords,
				int obj_type,
				struct adreno_ib_object_list *ib_obj_list,
				int ib_level);

static int ib_parse_set_draw_state(struct kgsl_device *device,
	unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars);

static int ib_parse_type7_set_draw_state(struct kgsl_device *device,
	unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list);

/*
 * adreno_ib_merge_range() - Increases the address range tracked by an ib
 * object
 * @ib_obj: The ib object
 * @gpuaddr: The start address which is to be merged
 * @size: Size of the merging address
 */
static void adreno_ib_merge_range(struct adreno_ib_object *ib_obj,
		uint64_t gpuaddr, uint64_t size)
{
	uint64_t addr_end1 = ib_obj->gpuaddr + ib_obj->size;
	uint64_t addr_end2 = gpuaddr + size;
	if (gpuaddr < ib_obj->gpuaddr)
		ib_obj->gpuaddr = gpuaddr;
	if (addr_end2 > addr_end1)
		ib_obj->size = addr_end2 - ib_obj->gpuaddr;
	else
		ib_obj->size = addr_end1 - ib_obj->gpuaddr;
}

/*
 * adreno_ib_check_overlap() - Checks if an address range overlap
 * @gpuaddr: The start address range to check for overlap
 * @size: Size of the address range
 * @type: The type of address range
 * @ib_obj_list: The list of address ranges to check for overlap
 *
 * Checks if an address range overlaps with a list of address ranges
 * Returns the entry from list which overlaps else NULL
 */
static struct adreno_ib_object *adreno_ib_check_overlap(uint64_t gpuaddr,
		uint64_t size, int type,
		struct adreno_ib_object_list *ib_obj_list)
{
	struct adreno_ib_object *ib_obj;
	int i;

	for (i = 0; i < ib_obj_list->num_objs; i++) {
		ib_obj = &(ib_obj_list->obj_list[i]);
		if ((type == ib_obj->snapshot_obj_type) &&
			kgsl_addr_range_overlap(ib_obj->gpuaddr, ib_obj->size,
			gpuaddr, size))
			/* regions overlap */
			return ib_obj;
	}
	return NULL;
}

/*
 * adreno_ib_add() - Add a gpuaddress range to list
 * @process: Process in which the gpuaddress is mapped
 * @type: The type of address range
 * @ib_obj_list: List of the address ranges in which the given range is to be
 * added
 *
 * Add a gpuaddress range as an ib object to a given list after checking if it
 * overlaps with another entry on the list. If it conflicts then change the
 * existing entry to incorporate this range
 *
 * Returns 0 on success else error code
 */
static int adreno_ib_add(struct kgsl_process_private *process,
				uint64_t gpuaddr, int type,
				struct adreno_ib_object_list *ib_obj_list)
{
	uint64_t size;
	struct adreno_ib_object *ib_obj;
	struct kgsl_mem_entry *entry;

	if (MAX_IB_OBJS <= ib_obj_list->num_objs)
		return -E2BIG;

	entry = kgsl_sharedmem_find(process, gpuaddr);
	if (!entry)
		/*
		 * Do not fail if gpuaddr not found, we can continue
		 * to search for other objects even if few objects are
		 * not found
		 */
		return 0;

	size = entry->memdesc.size;
	gpuaddr = entry->memdesc.gpuaddr;

	ib_obj = adreno_ib_check_overlap(gpuaddr, size, type, ib_obj_list);
	if (ib_obj) {
		adreno_ib_merge_range(ib_obj, gpuaddr, size);
		kgsl_mem_entry_put(entry);
	} else {
		adreno_ib_init_ib_obj(gpuaddr, size, type, entry,
			&(ib_obj_list->obj_list[ib_obj_list->num_objs]));
		ib_obj_list->num_objs++;
	}
	return 0;
}

/*
 * ib_save_mip_addresses() - Find mip addresses
 * @pkt: Pointer to the packet in IB
 * @process: The process in which IB is mapped
 * @ib_obj_list: List in which any objects found are added
 *
 * Returns 0 on success else error code
 */
static int ib_save_mip_addresses(unsigned int *pkt,
		struct kgsl_process_private *process,
		struct adreno_ib_object_list *ib_obj_list)
{
	int ret = 0;
	int num_levels = (pkt[1] >> 22) & 0x03FF;
	int i;
	unsigned int *hostptr;
	struct kgsl_mem_entry *ent;
	unsigned int block, type;
	int unitsize = 0;

	block = (pkt[1] >> 19) & 0x07;
	type = pkt[2] & 0x03;

	if (type == 0)
		unitsize = load_state_unit_sizes[block][0];
	else
		unitsize = load_state_unit_sizes[block][1];

	if (3 == block && 1 == type) {
		uint64_t gpuaddr = pkt[2] & 0xFFFFFFFC;
		uint64_t size = (num_levels * unitsize) << 2;

		ent = kgsl_sharedmem_find(process, gpuaddr);
		if (ent == NULL)
			return 0;

		if (!kgsl_gpuaddr_in_memdesc(&ent->memdesc,
			gpuaddr, size)) {
			kgsl_mem_entry_put(ent);
			return 0;
		}

		hostptr = kgsl_gpuaddr_to_vaddr(&ent->memdesc, gpuaddr);
		if (hostptr != NULL) {
			for (i = 0; i < num_levels; i++) {
				ret = adreno_ib_add(process, hostptr[i],
					SNAPSHOT_GPU_OBJECT_GENERIC,
					ib_obj_list);
				if (ret)
					break;
			}
		}

		kgsl_memdesc_unmap(&ent->memdesc);
		kgsl_mem_entry_put(ent);
	}
	return ret;
}

/*
 * ib_parse_load_state() - Parse load state packet
 * @pkt: Pointer to the packet in IB
 * @process: The pagetable in which the IB is mapped
 * @ib_obj_list: List in which any objects found are added
 * @ib_parse_vars: VAriable list that store temporary addressses
 *
 * Parse load state packet found in an IB and add any memory object found to
 * a list
 * Returns 0 on success else error code
 */
static int ib_parse_load_state(unsigned int *pkt,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int ret = 0;
	int i;

	/*
	 * The object here is to find indirect shaders i.e - shaders loaded from
	 * GPU memory instead of directly in the command.  These should be added
	 * to the list of memory objects to dump. So look at the load state
	 * if the block is indirect (source = 4). If so then add the memory
	 * address to the list.  The size of the object differs depending on the
	 * type per the load_state_unit_sizes array above.
	 */

	if (type3_pkt_size(pkt[0]) < 2)
		return 0;

	/*
	 * Anything from 3rd ordinal onwards of packet can be a memory object,
	 * no need to be fancy about parsing it, just save it if it looks
	 * like memory
	 */
	for (i = 0; i <= (type3_pkt_size(pkt[0]) - 2); i++) {
		ret |= adreno_ib_add(process, pkt[2 + i] & 0xFFFFFFFC,
				SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		if (ret)
			break;
	}
	/* get the mip addresses */
	if (!ret)
		ret = ib_save_mip_addresses(pkt, process, ib_obj_list);
	return ret;
}

/*
 * This opcode sets the base addresses for the visibilty stream buffer and the
 * visiblity stream size buffer.
 */

static int ib_parse_set_bin_data(unsigned int *pkt,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int ret = 0;

	if (type3_pkt_size(pkt[0]) < 2)
		return 0;

	/* Visiblity stream buffer */
	ret = adreno_ib_add(process, pkt[1],
		SNAPSHOT_GPU_OBJECT_GENERIC, ib_obj_list);
	if (ret)
		return ret;

	/* visiblity stream size buffer (fixed size 8 dwords) */
	ret = adreno_ib_add(process, pkt[2],
		SNAPSHOT_GPU_OBJECT_GENERIC, ib_obj_list);

	return ret;
}

/*
 * This opcode writes to GPU memory - if the buffer is written to, there is a
 * good chance that it would be valuable to capture in the snapshot, so mark all
 * buffers that are written to as frozen
 */

static int ib_parse_mem_write(unsigned int *pkt,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	if (type3_pkt_size(pkt[0]) < 1)
		return 0;

	/*
	 * The address is where the data in the rest of this packet is written
	 * to, but since that might be an offset into the larger buffer we need
	 * to get the whole thing. Pass a size of 0 tocapture the entire buffer.
	 */

	return adreno_ib_add(process, pkt[1] & 0xFFFFFFFC,
		SNAPSHOT_GPU_OBJECT_GENERIC, ib_obj_list);
}

/*
 * ib_add_type0_entries() - Add memory objects to list
 * @device: The device on which the IB will execute
 * @process: The process in which IB is mapped
 * @ib_obj_list: The list of gpu objects
 * @ib_parse_vars: addresses ranges found in type0 packets
 *
 * Add memory objects to given list that are found in type0 packets
 * Returns 0 on success else 0
 */
static int ib_add_type0_entries(struct kgsl_device *device,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;
	int i;
	int vfd_end;
	unsigned int mask;
	/* First up the visiblity stream buffer */
	if (adreno_is_a4xx(adreno_dev))
		mask = 0xFFFFFFFC;
	else
		mask = 0xFFFFFFFF;
	for (i = ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0;
		i < ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7; i++) {
		if (ib_parse_vars->cp_addr_regs[i]) {
			ret = adreno_ib_add(process,
				ib_parse_vars->cp_addr_regs[i] & mask,
				SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
			if (ret)
				return ret;
			ib_parse_vars->cp_addr_regs[i] = 0;
			ib_parse_vars->cp_addr_regs[i + 1] = 0;
			i++;
		}
	}

	vfd_end = adreno_is_a4xx(adreno_dev) ?
		ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_31 :
		ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15;
	for (i = ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0;
		i <= vfd_end; i++) {
		if (ib_parse_vars->cp_addr_regs[i]) {
			ret = adreno_ib_add(process,
				ib_parse_vars->cp_addr_regs[i],
				SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
			if (ret)
				return ret;
			ib_parse_vars->cp_addr_regs[i] = 0;
		}
	}

	if (ib_parse_vars->cp_addr_regs[ADRENO_CP_ADDR_VSC_SIZE_ADDRESS]) {
		ret = adreno_ib_add(process,
			ib_parse_vars->cp_addr_regs[
				ADRENO_CP_ADDR_VSC_SIZE_ADDRESS] & mask,
			SNAPSHOT_GPU_OBJECT_GENERIC, ib_obj_list);
		if (ret)
			return ret;
		ib_parse_vars->cp_addr_regs[
			ADRENO_CP_ADDR_VSC_SIZE_ADDRESS] = 0;
	}
	mask = 0xFFFFFFE0;
	for (i = ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR;
		i <= ADRENO_CP_ADDR_SP_FS_OBJ_START_REG; i++) {
		ret = adreno_ib_add(process,
			ib_parse_vars->cp_addr_regs[i] & mask,
			SNAPSHOT_GPU_OBJECT_GENERIC, ib_obj_list);
		if (ret)
			return ret;
		ib_parse_vars->cp_addr_regs[i] = 0;
	}
	return ret;
}
/*
 * The DRAW_INDX opcode sends a draw initator which starts a draw operation in
 * the GPU, so this is the point where all the registers and buffers become
 * "valid".  The DRAW_INDX may also have an index buffer pointer that should be
 * frozen with the others
 */

static int ib_parse_draw_indx(struct kgsl_device *device, unsigned int *pkt,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int ret = 0;
	int i;
	int opcode = cp_type3_opcode(pkt[0]);

	switch (opcode) {
	case CP_DRAW_INDX:
		if (type3_pkt_size(pkt[0]) > 3) {
			ret = adreno_ib_add(process,
				pkt[4], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		}
		break;
	case CP_DRAW_INDX_OFFSET:
		if (type3_pkt_size(pkt[0]) == 6) {
			ret = adreno_ib_add(process,
				pkt[5], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		}
		break;
	case CP_DRAW_INDIRECT:
		if (type3_pkt_size(pkt[0]) == 2) {
			ret = adreno_ib_add(process,
				pkt[2], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		}
		break;
	case CP_DRAW_INDX_INDIRECT:
		if (type3_pkt_size(pkt[0]) == 4) {
			ret = adreno_ib_add(process,
				pkt[2], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
			if (ret)
				break;
			ret = adreno_ib_add(process,
				pkt[4], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		}
		break;
	case CP_DRAW_AUTO:
		if (type3_pkt_size(pkt[0]) == 6) {
			ret = adreno_ib_add(process,
				 pkt[3], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
			if (ret)
				break;
			ret = adreno_ib_add(process,
				pkt[4], SNAPSHOT_GPU_OBJECT_GENERIC,
				ib_obj_list);
		}
		break;
	}

	if (ret)
		return ret;
	/*
	 * All of the type0 writes are valid at a draw initiator, so freeze
	 * the various buffers that we are tracking
	 */
	ret = ib_add_type0_entries(device, process, ib_obj_list,
				ib_parse_vars);
	if (ret)
		return ret;
	/* Process set draw state command streams if any */
	for (i = 0; i < NUM_SET_DRAW_GROUPS; i++) {
		if (!ib_parse_vars->set_draw_groups[i].cmd_stream_dwords)
			continue;
		ret = adreno_ib_find_objs(device, process,
			ib_parse_vars->set_draw_groups[i].cmd_stream_addr,
			ib_parse_vars->set_draw_groups[i].cmd_stream_dwords,
			SNAPSHOT_GPU_OBJECT_DRAW,
			ib_obj_list, 2);
		if (ret)
			break;
	}
	return ret;
}

/*
 * Parse all the type7 opcode packets that may contain important information,
 * such as additional GPU buffers to grab or a draw initator
 */

static int ib_parse_type7(struct kgsl_device *device, unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int opcode = cp_type7_opcode(*ptr);

	switch (opcode) {
	case CP_SET_DRAW_STATE:
		return ib_parse_type7_set_draw_state(device, ptr, process,
					ib_obj_list);
	}

	return 0;
}

/*
 * Parse all the type3 opcode packets that may contain important information,
 * such as additional GPU buffers to grab or a draw initator
 */

static int ib_parse_type3(struct kgsl_device *device, unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int opcode = cp_type3_opcode(*ptr);

	switch (opcode) {
	case  CP_LOAD_STATE:
		return ib_parse_load_state(ptr, process, ib_obj_list,
					ib_parse_vars);
	case CP_SET_BIN_DATA:
		return ib_parse_set_bin_data(ptr, process, ib_obj_list,
					ib_parse_vars);
	case CP_MEM_WRITE:
		return ib_parse_mem_write(ptr, process, ib_obj_list,
					ib_parse_vars);
	case CP_DRAW_INDX:
	case CP_DRAW_INDX_OFFSET:
	case CP_DRAW_INDIRECT:
	case CP_DRAW_INDX_INDIRECT:
		return ib_parse_draw_indx(device, ptr, process, ib_obj_list,
					ib_parse_vars);
	case CP_SET_DRAW_STATE:
		return ib_parse_set_draw_state(device, ptr, process,
					ib_obj_list, ib_parse_vars);
	}

	return 0;
}

/*
 * Parse type0 packets found in the stream.  Some of the registers that are
 * written are clues for GPU buffers that we need to freeze.  Register writes
 * are considred valid when a draw initator is called, so just cache the values
 * here and freeze them when a CP_DRAW_INDX is seen.  This protects against
 * needlessly caching buffers that won't be used during a draw call
 */

static int ib_parse_type0(struct kgsl_device *device, unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int size = type0_pkt_size(*ptr);
	int offset = type0_pkt_offset(*ptr);
	int i;
	int reg_index;
	int ret = 0;

	for (i = 0; i < size; i++, offset++) {
		/* Visiblity stream buffer */
		if (offset >= adreno_cp_parser_getreg(adreno_dev,
				ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0) &&
			offset <= adreno_cp_parser_getreg(adreno_dev,
				ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7)) {
			reg_index = adreno_cp_parser_regindex(
					adreno_dev, offset,
					ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0,
					ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7);
			if (reg_index >= 0)
				ib_parse_vars->cp_addr_regs[reg_index] =
								ptr[i + 1];
			continue;
		} else if ((offset >= adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0)) &&
			(offset <= adreno_cp_parser_getreg(adreno_dev,
				ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15))) {
			reg_index = adreno_cp_parser_regindex(adreno_dev,
					offset,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15);
			if (reg_index >= 0)
				ib_parse_vars->cp_addr_regs[reg_index] =
								ptr[i + 1];
			continue;
		} else if ((offset >= adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_16)) &&
			(offset <= adreno_cp_parser_getreg(adreno_dev,
				ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_31))) {
			reg_index = adreno_cp_parser_regindex(adreno_dev,
					offset,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_16,
					ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_31);
			if (reg_index >= 0)
				ib_parse_vars->cp_addr_regs[reg_index] =
								ptr[i + 1];
			continue;
		} else {
			if (offset ==
				adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_VSC_SIZE_ADDRESS))
				ib_parse_vars->cp_addr_regs[
					ADRENO_CP_ADDR_VSC_SIZE_ADDRESS] =
						ptr[i + 1];
			else if (offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR))
				ib_parse_vars->cp_addr_regs[
					ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR] =
						ptr[i + 1];
			else if (offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_SP_FS_PVT_MEM_ADDR))
				ib_parse_vars->cp_addr_regs[
					ADRENO_CP_ADDR_SP_FS_PVT_MEM_ADDR] =
						ptr[i + 1];
			else if (offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_SP_VS_OBJ_START_REG))
				ib_parse_vars->cp_addr_regs[
					ADRENO_CP_ADDR_SP_VS_OBJ_START_REG] =
						ptr[i + 1];
			else if (offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_ADDR_SP_FS_OBJ_START_REG))
				ib_parse_vars->cp_addr_regs[
					ADRENO_CP_ADDR_SP_FS_OBJ_START_REG] =
						ptr[i + 1];
			else if ((offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_UCHE_INVALIDATE0)) ||
				(offset == adreno_cp_parser_getreg(adreno_dev,
					ADRENO_CP_UCHE_INVALIDATE1))) {
					ret = adreno_ib_add(process,
						ptr[i + 1] & 0xFFFFFFC0,
						SNAPSHOT_GPU_OBJECT_GENERIC,
						ib_obj_list);
					if (ret)
						break;
			}
		}
	}
	return ret;
}

static int ib_parse_type7_set_draw_state(struct kgsl_device *device,
	unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list)
{
	int size = type7_pkt_size(*ptr);
	int i;
	int grp_id;
	int ret = 0;
	int flags;
	uint64_t cmd_stream_dwords;
	uint64_t cmd_stream_addr;

	/*
	 * size is the size of the packet that does not include the DWORD
	 * for the packet header, we only want to loop here through the
	 * packet parameters from ptr[1] till ptr[size] where ptr[0] is the
	 * packet header. In each loop we look at 3 DWORDS hence increment
	 * loop counter by 3 always
	 */
	for (i = 1; i <= size; i += 3) {
		grp_id = (ptr[i] & 0x1F000000) >> 24;
		/* take action based on flags */
		flags = (ptr[i] & 0x000F0000) >> 16;

		/*
		 * dirty flag or no flags both mean we need to load it for
		 * next draw. No flags is used when the group is activated
		 * or initialized for the first time in the IB
		 */
		if (flags & 0x1 || !flags) {
			cmd_stream_dwords = ptr[i] & 0x0000FFFF;
			cmd_stream_addr = ptr[i + 2];
			cmd_stream_addr = cmd_stream_addr << 32 | ptr[i + 1];
			if (cmd_stream_dwords)
				ret = adreno_ib_find_objs(device, process,
					cmd_stream_addr, cmd_stream_dwords,
					SNAPSHOT_GPU_OBJECT_DRAW, ib_obj_list,
					2);
			if (ret)
				break;
			continue;
		}
		/* load immediate */
		if (flags & 0x8) {
			uint64_t gpuaddr = ptr[i + 2];
			gpuaddr = gpuaddr << 32 | ptr[i + 1];
			ret = adreno_ib_find_objs(device, process,
				gpuaddr, (ptr[i] & 0x0000FFFF),
				SNAPSHOT_GPU_OBJECT_IB,
				ib_obj_list, 2);
			if (ret)
				break;
		}
	}
	return ret;
}

static int ib_parse_set_draw_state(struct kgsl_device *device,
	unsigned int *ptr,
	struct kgsl_process_private *process,
	struct adreno_ib_object_list *ib_obj_list,
	struct ib_parser_variables *ib_parse_vars)
{
	int size = type0_pkt_size(*ptr);
	int i;
	int grp_id;
	int ret = 0;
	int flags;

	/*
	 * size is the size of the packet that does not include the DWORD
	 * for the packet header, we only want to loop here through the
	 * packet parameters from ptr[1] till ptr[size] where ptr[0] is the
	 * packet header. In each loop we look at 2 DWORDS hence increment
	 * loop counter by 2 always
	 */
	for (i = 1; i <= size; i += 2) {
		grp_id = (ptr[i] & 0x1F000000) >> 24;
		/* take action based on flags */
		flags = (ptr[i] & 0x000F0000) >> 16;
		/* Disable all groups */
		if (flags & 0x4) {
			int j;
			for (j = 0; j < NUM_SET_DRAW_GROUPS; j++)
				ib_parse_vars->set_draw_groups[j].
					cmd_stream_dwords = 0;
			continue;
		}
		/* disable flag */
		if (flags & 0x2) {
			ib_parse_vars->set_draw_groups[grp_id].
						cmd_stream_dwords = 0;
			continue;
		}
		/*
		 * dirty flag or no flags both mean we need to load it for
		 * next draw. No flags is used when the group is activated
		 * or initialized for the first time in the IB
		 */
		if (flags & 0x1 || !flags) {
			ib_parse_vars->set_draw_groups[grp_id].
				cmd_stream_dwords = ptr[i] & 0x0000FFFF;
			ib_parse_vars->set_draw_groups[grp_id].
				cmd_stream_addr = ptr[i + 1];
			continue;
		}
		/* load immediate */
		if (flags & 0x8) {
			ret = adreno_ib_find_objs(device, process,
				ptr[i + 1], (ptr[i] & 0x0000FFFF),
				SNAPSHOT_GPU_OBJECT_IB,
				ib_obj_list, 2);
			if (ret)
				break;
		}
	}
	return ret;
}

/*
 * adreno_cp_parse_ib2() - Wrapper function around IB2 parsing
 * @device: Device pointer
 * @process: Process in which the IB is allocated
 * @gpuaddr: IB2 gpuaddr
 * @dwords: IB2 size in dwords
 * @ib_obj_list: List of objects found in IB
 * @ib_level: The level from which function is called, either from IB1 or IB2
 *
 * Function does some checks to ensure that IB2 parsing is called from IB1
 * and then calls the function to find objects in IB2.
 */
static int adreno_cp_parse_ib2(struct kgsl_device *device,
			struct kgsl_process_private *process,
			uint64_t gpuaddr, uint64_t dwords,
			struct adreno_ib_object_list *ib_obj_list,
			int ib_level)
{
	struct adreno_ib_object *ib_obj;
	int i;
	/*
	 * We can only expect an IB2 in IB1, if we are
	 * already processing an IB2 then return error
	 */
	if (2 == ib_level)
		return -EINVAL;
	/*
	 * only try to find sub objects iff this IB has
	 * not been processed already
	 */
	for (i = 0; i < ib_obj_list->num_objs; i++) {
		ib_obj = &(ib_obj_list->obj_list[i]);
		if ((SNAPSHOT_GPU_OBJECT_IB == ib_obj->snapshot_obj_type) &&
			(gpuaddr >= ib_obj->gpuaddr) &&
			(gpuaddr + dwords * sizeof(unsigned int) <=
			ib_obj->gpuaddr + ib_obj->size))
			return 0;
	}

	return adreno_ib_find_objs(device, process, gpuaddr, dwords,
		SNAPSHOT_GPU_OBJECT_IB, ib_obj_list, 2);
}

/*
 * adreno_ib_find_objs() - Find all IB objects in a given IB
 * @device: The device pointer on which the IB executes
 * @process: The process in which the IB and all contained objects are mapped.
 * @gpuaddr: The gpu address of the IB
 * @dwords: Size of ib in dwords
 * @obj_type: The object type can be either an IB or a draw state sequence
 * @ib_obj_list: The list in which the IB and the objects in it are added.
 * @ib_level: Indicates if IB1 or IB2 is being processed
 *
 * Finds all IB objects in a given IB and puts then in a list. Can be called
 * recursively for the IB2's in the IB1's
 * Returns 0 on success else error code
 */
static int adreno_ib_find_objs(struct kgsl_device *device,
				struct kgsl_process_private *process,
				uint64_t gpuaddr, uint64_t dwords,
				int obj_type,
				struct adreno_ib_object_list *ib_obj_list,
				int ib_level)
{
	int ret = 0;
	uint64_t rem = dwords;
	int i;
	struct ib_parser_variables ib_parse_vars;
	unsigned int *src;
	struct adreno_ib_object *ib_obj;
	struct kgsl_mem_entry *entry;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* check that this IB is not already on list */
	for (i = 0; i < ib_obj_list->num_objs; i++) {
		ib_obj = &(ib_obj_list->obj_list[i]);
		if ((obj_type == ib_obj->snapshot_obj_type) &&
			(ib_obj->gpuaddr <= gpuaddr) &&
			((ib_obj->gpuaddr + ib_obj->size) >=
			(gpuaddr + (dwords << 2))))
			return 0;
	}

	entry = kgsl_sharedmem_find(process, gpuaddr);
	if (!entry)
		return -EINVAL;

	if (!kgsl_gpuaddr_in_memdesc(&entry->memdesc, gpuaddr, (dwords << 2))) {
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	src = kgsl_gpuaddr_to_vaddr(&entry->memdesc, gpuaddr);
	if (!src) {
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	memset(&ib_parse_vars, 0, sizeof(struct ib_parser_variables));

	ret = adreno_ib_add(process, gpuaddr, obj_type, ib_obj_list);
	if (ret)
		goto done;

	for (i = 0; rem > 0; rem--, i++) {
		int pktsize;

		if (pkt_is_type0(src[i]))
			pktsize = type0_pkt_size(src[i]);

		else if (pkt_is_type3(src[i]))
			pktsize = type3_pkt_size(src[i]);

		else if (pkt_is_type4(src[i]))
			pktsize = type4_pkt_size(src[i]);

		else if (pkt_is_type7(src[i]))
			pktsize = type7_pkt_size(src[i]);

		/*
		 * If the packet isn't a type 1, type 3, type 4 or type 7 then
		 * don't bother parsing it - it is likely corrupted
		 */
		else
			break;

		if (((pkt_is_type0(src[i]) || pkt_is_type3(src[i])) && !pktsize)
			|| ((pktsize + 1) > rem))
			break;

		if (pkt_is_type3(src[i])) {
			if (adreno_cmd_is_ib(adreno_dev, src[i])) {
				uint64_t gpuaddrib2 = src[i + 1];
				uint64_t size = src[i + 2];

				ret = adreno_cp_parse_ib2(device, process,
						gpuaddrib2, size,
						ib_obj_list, ib_level);
				if (ret)
					goto done;
			} else {
				ret = ib_parse_type3(device, &src[i], process,
						ib_obj_list,
						&ib_parse_vars);
				/*
				 * If the parse function failed (probably
				 * because of a bad decode) then bail out and
				 * just capture the binary IB data
				 */

				if (ret)
					goto done;
			}
		}

		else if (pkt_is_type7(src[i])) {
			if (adreno_cmd_is_ib(adreno_dev, src[i])) {
				uint64_t size = src[i + 3];
				uint64_t gpuaddrib2 = src[i + 2];
				gpuaddrib2 = gpuaddrib2 << 32 | src[i + 1];

				ret = adreno_cp_parse_ib2(device, process,
						gpuaddrib2, size,
						ib_obj_list, ib_level);
				if (ret)
					goto done;
			} else {
				ret = ib_parse_type7(device, &src[i], process,
						ib_obj_list,
						&ib_parse_vars);
				/*
				 * If the parse function failed (probably
				 * because of a bad decode) then bail out and
				 * just capture the binary IB data
				 */

				if (ret)
					goto done;
			}
		}

		else if (pkt_is_type0(src[i])) {
			ret = ib_parse_type0(device, &src[i], process,
					ib_obj_list, &ib_parse_vars);
			if (ret)
				goto done;
		}

		i += pktsize;
		rem -= pktsize;
	}

done:
	/*
	 * For set draw objects there may not be a draw_indx packet at its end
	 * to signal that we need to save the found objects in it, so just save
	 * it here.
	 */
	if (!ret && SNAPSHOT_GPU_OBJECT_DRAW == obj_type)
		ret = ib_add_type0_entries(device, process, ib_obj_list,
			&ib_parse_vars);

	kgsl_memdesc_unmap(&entry->memdesc);
	kgsl_mem_entry_put(entry);
	return ret;
}


/*
 * adreno_ib_create_object_list() - Find all the memory objects in IB
 * @device: The device pointer on which the IB executes
 * @process: The process in which the IB and all contained objects are mapped
 * @gpuaddr: The gpu address of the IB
 * @dwords: Size of ib in dwords
 * @ib_obj_list: The list in which the IB and the objects in it are added.
 *
 * Find all the memory objects that an IB needs for execution and place
 * them in a list including the IB.
 * Returns the ib object list. On success 0 is returned, on failure error
 * code is returned along with number of objects that was saved before
 * error occurred. If no objects found then the list pointer is set to
 * NULL.
 */
int adreno_ib_create_object_list(struct kgsl_device *device,
		struct kgsl_process_private *process,
		uint64_t gpuaddr, uint64_t dwords,
		struct adreno_ib_object_list **out_ib_obj_list)
{
	int ret = 0;
	struct adreno_ib_object_list *ib_obj_list;

	if (!out_ib_obj_list)
		return -EINVAL;

	*out_ib_obj_list = NULL;

	ib_obj_list = kzalloc(sizeof(*ib_obj_list), GFP_KERNEL);
	if (!ib_obj_list)
		return -ENOMEM;

	ib_obj_list->obj_list = vmalloc(MAX_IB_OBJS *
					sizeof(struct adreno_ib_object));

	if (!ib_obj_list->obj_list) {
		kfree(ib_obj_list);
		return -ENOMEM;
	}

	ret = adreno_ib_find_objs(device, process, gpuaddr, dwords,
		SNAPSHOT_GPU_OBJECT_IB, ib_obj_list, 1);

	/* Even if there was an error return the remaining objects found */
	if (ib_obj_list->num_objs)
		*out_ib_obj_list = ib_obj_list;

	return ret;
}

/*
 * adreno_ib_destroy_obj_list() - Destroy an ib object list
 * @ib_obj_list: List to destroy
 *
 * Free up all resources used by an ib_obj_list
 */
void adreno_ib_destroy_obj_list(struct adreno_ib_object_list *ib_obj_list)
{
	int i;

	if (!ib_obj_list)
		return;

	for (i = 0; i < ib_obj_list->num_objs; i++) {
		if (ib_obj_list->obj_list[i].entry)
			kgsl_mem_entry_put(ib_obj_list->obj_list[i].entry);
	}
	vfree(ib_obj_list->obj_list);
	kfree(ib_obj_list);
}
