/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/slab.h>

#include "reviser_cmn.h"
#include "reviser_reg.h"

uint32_t  reviser_get_remap_offset(uint32_t index)
{
	uint32_t offset = 0;


	if (index <= VLM_REMAP_TABLE_DST_MAX)
		offset = VLM_REMAP_TABLE_BASE + (index + 1) * 4;
	else
		offset = REVISER_FAIL;
	return offset;
}

uint32_t  reviser_get_contex_offset_MDLA(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = VLM_CTXT_MDLA_0;
		break;
	case 1:
		offset = VLM_CTXT_MDLA_1;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}

uint32_t  reviser_get_contex_offset_VPU(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = VLM_CTXT_VPU_0;
		break;
	case 1:
		offset = VLM_CTXT_VPU_1;
		break;
	case 2:
		offset = VLM_CTXT_VPU_2;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}

uint32_t  reviser_get_contex_offset_EDMA(uint32_t index)
{
	uint32_t offset = 0;

	switch (index) {
	case 0:
		offset = VLM_CTXT_EDMA_0;
		break;
	case 1:
		offset = VLM_CTXT_EDMA_1;
		break;
	default:
		offset = REVISER_FAIL;
		break;
	}

	return offset;
}
