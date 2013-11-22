/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __ADRENO_IB_PARSER__
#define __ADRENO_IB_PARSER__

#include "adreno.h"

extern const unsigned int a3xx_cp_addr_regs[];
extern const unsigned int a4xx_cp_addr_regs[];

/*
 * struct adreno_ib_object - Structure containing information about an
 * address range found in an IB
 * @gpuaddr: The starting gpuaddress of the range
 * @size: Size of the range
 * @snapshot_obj_type - Type of range used in snapshot
 * @entry: The memory entry in which this range is found
 */
struct adreno_ib_object {
	unsigned int gpuaddr;
	unsigned int size;
	int snapshot_obj_type;
	struct kgsl_mem_entry *entry;
};

/*
 * struct adreno_ib_object_list - List of address ranges found in IB
 * @obj_list: The address range list
 * @num_objs: Number of objects in list
 */
struct adreno_ib_object_list {
	struct adreno_ib_object *obj_list;
	int num_objs;
};

/*
 * adreno registers used during IB parsing, there contain addresses
 * and sizes of the addresses that present in an IB
 */
enum adreno_cp_addr_regs {
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_0 = 0,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_0,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_1,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_1,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_2,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_2,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_3,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_3,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_4,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_4,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_5,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_5,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_6,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_6,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_ADDRESS_7,
	ADRENO_CP_ADDR_VSC_PIPE_DATA_LENGTH_7,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_0,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_1,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_2,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_3,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_4,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_5,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_6,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_7,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_8,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_9,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_10,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_11,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_12,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_13,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_14,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_15,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_16,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_17,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_18,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_19,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_20,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_21,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_22,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_23,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_24,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_25,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_26,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_27,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_28,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_29,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_30,
	ADRENO_CP_ADDR_VFD_FETCH_INSTR_1_31,
	ADRENO_CP_ADDR_VSC_SIZE_ADDRESS,
	ADRENO_CP_ADDR_SP_VS_PVT_MEM_ADDR,
	ADRENO_CP_ADDR_SP_FS_PVT_MEM_ADDR,
	ADRENO_CP_ADDR_SP_VS_OBJ_START_REG,
	ADRENO_CP_ADDR_SP_FS_OBJ_START_REG,
	ADRENO_CP_UCHE_INVALIDATE0,
	ADRENO_CP_UCHE_INVALIDATE1,
	ADRENO_CP_ADDR_MAX,
};

/*
 * adreno_ib_init_ib_obj() - Create an ib object structure and initialize it
 * with gpuaddress and size
 * @gpuaddr: gpuaddr with which to initialize the object with
 * @size: Size in bytes with which the object is initialized
 * @ib_type: The IB type used by snapshot
 *
 * Returns the object pointer on success else error code in the pointer
 */
static inline void adreno_ib_init_ib_obj(unsigned int gpuaddr,
			unsigned int size, int obj_type,
			struct kgsl_mem_entry *entry,
			struct adreno_ib_object *ib_obj)
{
	ib_obj->gpuaddr = gpuaddr;
	ib_obj->size = size;
	ib_obj->snapshot_obj_type = obj_type;
	ib_obj->entry = entry;
}

/*
 * adreno_cp_parser_getreg() - Returns the value of register offset
 * @adreno_dev: The adreno device being operated upon
 * @reg_enum: Enum index of the register whose offset is returned
 */
static inline int adreno_cp_parser_getreg(struct adreno_device *adreno_dev,
					enum adreno_cp_addr_regs reg_enum)
{
	if (adreno_is_a3xx(adreno_dev))
		return a3xx_cp_addr_regs[reg_enum];
	else if (adreno_is_a4xx(adreno_dev))
		return a4xx_cp_addr_regs[reg_enum];
	else
		return -EEXIST;
}

/*
 * adreno_cp_parser_regindex() - Returns enum index for a given register offset
 * @adreno_dev: The adreno device being operated upon
 * @offset: Register offset
 * @start: The start index to search from
 * @end: The last index to search
 *
 * Checks the list of registers defined for the device and returns the index
 * whose offset value matches offset parameter.
 */
static inline int adreno_cp_parser_regindex(struct adreno_device *adreno_dev,
				unsigned int offset,
				enum adreno_cp_addr_regs start,
				enum adreno_cp_addr_regs end)
{
	int i;
	const unsigned int *regs;
	if (adreno_is_a4xx(adreno_dev))
		regs = a4xx_cp_addr_regs;
	else if (adreno_is_a3xx(adreno_dev))
		regs = a3xx_cp_addr_regs;
	else
		return -EEXIST;

	for (i = start; i <= end; i++)
		if (regs[i] == offset)
			return i;
	return -EEXIST;
}

int adreno_ib_create_object_list(
		struct kgsl_device *device, phys_addr_t ptbase,
		unsigned int gpuaddr, unsigned int dwords,
		struct adreno_ib_object_list **out_ib_obj_list);

void adreno_ib_destroy_obj_list(struct adreno_ib_object_list *ib_obj_list);

#endif
