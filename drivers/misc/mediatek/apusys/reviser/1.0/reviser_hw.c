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
#include "reviser_drv.h"
#include "reviser_reg.h"
#include "reviser_hw.h"

#define FAKE_CONTEX_REG_NUM 9
#define FAKE_REMAP_REG_NUM 13

uint32_t g_ctx_reg[FAKE_CONTEX_REG_NUM];

uint32_t g_remap_reg[FAKE_REMAP_REG_NUM];

static uint32_t *_reviser_reg_fake_search(uint32_t offset);
static uint32_t _reviser_reg_read(void *base, uint32_t offset);
static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_set(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_clr(void *base, uint32_t offset, uint32_t value);
static uint32_t _reviser_get_contex_offset(enum REVISER_DEVICE_E type,
		int index);
static uint32_t _reviser_get_remap_offset(int index);
static void _reviser_set_contex_boundary(void *private,
		uint32_t offset, uint8_t boundary);
static void _reviser_set_context_ID(void *private,
		uint32_t offset, uint8_t ID);
static void _reviser_set_remap_table(void *private,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page);

void reviser_print_private(void *private)
{
	struct reviser_dev_info *info = NULL;

	DEBUG_TAG;
	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	info = (struct reviser_dev_info *)private;
	LOG_INFO("=============================");
	LOG_INFO(" reviser driver private info\n");
	LOG_INFO("-----------------------------");
	LOG_INFO("pctrl_top: %p\n",	info->pctrl_top);
	LOG_INFO("=============================");

}

void reviser_print_boundary(void *private)
{
	struct reviser_dev_info *info = NULL;
	uint32_t reg[FAKE_CONTEX_REG_NUM];
	uint32_t offset = 0;

	DEBUG_TAG;

	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	info = (struct reviser_dev_info *)private;



	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[0] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[1] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[2] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[3] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 2);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[4] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[5] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[6] = _reviser_reg_read(private, offset) & VLM_CTXT_BDY_SELECT;

	LOG_INFO("=============================");
	LOG_INFO(" reviser driver boundary info\n");
	LOG_INFO("-----------------------------");

	LOG_INFO("MDLA0: %.8x\n", reg[0]);
	LOG_INFO("MDLA1: %.8x\n", reg[1]);
	LOG_INFO("VPU0:  %.8x\n", reg[2]);
	LOG_INFO("VPU1:  %.8x\n", reg[3]);
	LOG_INFO("VPU2:  %.8x\n", reg[4]);
	LOG_INFO("EDMA0: %.8x\n", reg[5]);
	LOG_INFO("EDMA1: %.8x\n", reg[6]);

	LOG_INFO("=============================");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_context_ID(void *private)
{
	struct reviser_dev_info *info = NULL;
	uint32_t reg[FAKE_CONTEX_REG_NUM];
	uint32_t offset = 0;

	DEBUG_TAG;

	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	info = (struct reviser_dev_info *)private;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[0] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[1] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[2] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[3] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 2);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[4] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;


	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[5] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[6] = (_reviser_reg_read(private, offset) & VLM_CTXT_CTX_ID)
			>> VLM_CTXT_CTX_ID_OFFSET;

	LOG_INFO("=============================");
	LOG_INFO(" reviser driver ID info\n");
	LOG_INFO("-----------------------------");

	LOG_INFO("MDLA0: %.8x\n", reg[0]);
	LOG_INFO("MDLA1: %.8x\n", reg[1]);
	LOG_INFO("VPU0:  %.8x\n", reg[2]);
	LOG_INFO("VPU1:  %.8x\n", reg[3]);
	LOG_INFO("VPU2:  %.8x\n", reg[4]);
	LOG_INFO("EDMA0: %.8x\n", reg[5]);
	LOG_INFO("EDMA1: %.8x\n", reg[6]);

	LOG_INFO("=============================");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_remap_table(void *private)
{
	struct reviser_dev_info *info = NULL;
	uint32_t reg[FAKE_REMAP_REG_NUM];
	uint32_t offset[FAKE_REMAP_REG_NUM];
	int i = 0;
	uint32_t valid, ID, src, dst;

	DEBUG_TAG;

	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	info = (struct reviser_dev_info *)private;

	for (i = 0; i < FAKE_REMAP_REG_NUM; i++) {
		offset[i] = _reviser_get_remap_offset(i);
		if (offset[i] == REVISER_FAIL) {
			LOG_ERR("invalid argument\n");
			goto fail_offset;
		}
		reg[i] = _reviser_reg_read(private, offset[i]);
	}

	LOG_INFO("=============================");
	LOG_INFO(" reviser driver remap info\n");
	LOG_INFO("-----------------------------");

	for (i = 0; i < FAKE_REMAP_REG_NUM; i++) {
		valid = reg[i] >> VLM_REMAP_VALID_OFFSET;
		ID = (reg[i] & VLM_REMAP_CTX_ID) >> VLM_REMAP_CTX_ID_OFFSET;
		src = (reg[i] & VLM_REMAP_CTX_SRC) >> VLM_REMAP_CTX_SRC_OFFSET;
		dst = (reg[i] & VLM_REMAP_CTX_DST) >> VLM_REMAP_CTX_DST_OFFSET;

		LOG_INFO("[%02d]: valid[%d] ID[%02d] src[%02d] dst[%02d]\n",
				i, valid, ID, src, dst);
	}

	LOG_INFO("=============================");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}
static uint32_t *_reviser_reg_fake_search(uint32_t offset)
{

	switch (offset) {
	case VLM_CTXT_MDLA_0:
		return &g_ctx_reg[0];
	case VLM_CTXT_MDLA_1:
		return &g_ctx_reg[1];
	case VLM_CTXT_VPU_0:
		return &g_ctx_reg[2];
	case VLM_CTXT_VPU_1:
		return &g_ctx_reg[3];
	case VLM_CTXT_VPU_2:
		return &g_ctx_reg[4];
	case VLM_CTXT_EDMA_0:
		return &g_ctx_reg[5];
	case VLM_CTXT_EDMA_1:
		return &g_ctx_reg[6];
	case VLM_REMAP_TABLE_0:
		return &g_remap_reg[0];
	case VLM_REMAP_TABLE_1:
		return &g_remap_reg[1];
	case VLM_REMAP_TABLE_2:
		return &g_remap_reg[2];
	case VLM_REMAP_TABLE_3:
		return &g_remap_reg[3];
	case VLM_REMAP_TABLE_4:
		return &g_remap_reg[4];
	case VLM_REMAP_TABLE_5:
		return &g_remap_reg[5];
	case VLM_REMAP_TABLE_6:
		return &g_remap_reg[6];
	case VLM_REMAP_TABLE_7:
		return &g_remap_reg[7];
	case VLM_REMAP_TABLE_8:
		return &g_remap_reg[8];
	case VLM_REMAP_TABLE_9:
		return &g_remap_reg[9];
	case VLM_REMAP_TABLE_A:
		return &g_remap_reg[10];
	case VLM_REMAP_TABLE_B:
		return &g_remap_reg[11];
	case VLM_REMAP_TABLE_C:
		return &g_remap_reg[12];
	default:
		LOG_ERR("offset invalid %.8x\n", offset);
		return NULL;
	}

}

static uint32_t _reviser_reg_read(void *base, uint32_t offset)
{
	//uint32_t ret = 0;
	//return ioread32(base + offset);
	uint32_t *pReg = NULL;



	pReg = _reviser_reg_fake_search(offset);
	LOG_DEBUG("offset: %p value %.8x\n", offset, *pReg);

	return *pReg;
}

static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value)
{
	uint32_t *pReg = NULL;

	LOG_DEBUG("offset: %p value: %.8x\n", offset, value);
	pReg = _reviser_reg_fake_search(offset);
	if (pReg == NULL)
		return;

	*pReg = value;
	//iowrite32(value, base + offset);

}

static void _reviser_reg_set(void *base, uint32_t offset, uint32_t value)
{
	DEBUG_TAG;
	_reviser_reg_write(base,
			offset, _reviser_reg_read(base, offset) | value);
}

static void _reviser_reg_clr(void *base, uint32_t offset, uint32_t value)
{
	DEBUG_TAG;
	_reviser_reg_write(base,
			offset, _reviser_reg_read(base, offset) & (~value));
}

static uint32_t  _reviser_get_remap_offset(int index)
{
	return reviser_get_remap_offset(index);
}


static uint32_t  _reviser_get_contex_offset(enum REVISER_DEVICE_E type,
		int index)
{
	uint32_t offset = 0;


	switch (type) {
	case REVISER_DEVICE_MDLA:
		offset = reviser_get_contex_offset_MDLA(index);
		break;
	case REVISER_DEVICE_VPU:
		offset = reviser_get_contex_offset_VPU(index);
		break;
	case REVISER_DEVICE_EDMA:
		offset = reviser_get_contex_offset_EDMA(index);
		break;
	default:
		LOG_ERR("invalid argument type\n");
		offset = REVISER_FAIL;
		break;
	}


	return offset;
}

static void _reviser_set_contex_boundary(void *private,
		uint32_t offset, uint8_t boundary)
{
	struct reviser_dev_info *info = NULL;

	info = (struct reviser_dev_info *)private;

	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_CTXT_BDY_SELECT);
	_reviser_reg_set(info->pctrl_top,
			offset, boundary & VLM_CTXT_BDY_SELECT);

}
static void _reviser_set_context_ID(void *private, uint32_t offset, uint8_t ID)
{
	struct reviser_dev_info *info = NULL;

	DEBUG_TAG;
	info = (struct reviser_dev_info *)private;

	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_CTXT_CTX_ID);
	_reviser_reg_set(info->pctrl_top,
			offset, (ID << VLM_CTXT_CTX_ID_OFFSET));

}

static void  _reviser_set_remap_table(void *private,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	struct reviser_dev_info *info = NULL;

	DEBUG_TAG;
	info = (struct reviser_dev_info *)private;

	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_REMAP_VALID);

	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_REMAP_CTX_ID);
	_reviser_reg_set(info->pctrl_top,
			offset, (ID << VLM_REMAP_CTX_ID_OFFSET));
	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_REMAP_CTX_SRC);
	_reviser_reg_set(info->pctrl_top,
			offset, (src_page << VLM_REMAP_CTX_SRC_OFFSET));
	_reviser_reg_clr(info->pctrl_top,
			offset, VLM_REMAP_CTX_DST);
	_reviser_reg_set(info->pctrl_top,
			offset, (dst_page << VLM_REMAP_CTX_DST_OFFSET));

	if (valid)
		_reviser_reg_set(info->pctrl_top,
				offset, (1 << VLM_REMAP_VALID_OFFSET));
}

void reviser_set_remap_talbe(void *private,
		int index, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	uint32_t offset = 0;

	DEBUG_TAG;

	if (ID > VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range) %d\n",
				VLM_CTXT_CTX_ID_MAX);
		return;
	}
	if (src_page > VLM_REMAP_TABLE_SRC_MAX) {
		LOG_ERR("invalid src page (out of range) %d\n",
				VLM_REMAP_TABLE_SRC_MAX);
		return;
	}

	if (dst_page > VLM_REMAP_TABLE_DST_MAX) {
		LOG_ERR("invalid dst page (out of range) %d\n",
				VLM_REMAP_TABLE_DST_MAX);
		return;
	}

	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	offset = _reviser_get_remap_offset(index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	_reviser_set_remap_table(private,
			offset, valid, ID, src_page, dst_page);

}

void reviser_set_context_boundary(void *private,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary)
{
	uint32_t offset = 0;

	DEBUG_TAG;

	if (boundary > VLM_CTXT_BDY_SELECT_MAX) {
		LOG_ERR("invalid boundary (out of range) %d\n",
				VLM_CTXT_BDY_SELECT_MAX);
		return;
	}
	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	offset = _reviser_get_contex_offset(type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	_reviser_set_contex_boundary(private, offset, boundary);
}


void reviser_set_context_ID(void *private,
		enum REVISER_DEVICE_E type, int index, uint8_t ID)
{
	uint32_t offset = 0;

	DEBUG_TAG;

	if (ID > VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range) %d\n", VLM_CTXT_CTX_ID_MAX);
		return;
	}


	if (private == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	offset = _reviser_get_contex_offset(type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	_reviser_set_context_ID(private, offset, ID);
}
