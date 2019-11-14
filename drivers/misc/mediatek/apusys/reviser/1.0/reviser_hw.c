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
#include <linux/bitmap.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "apusys_device.h"
#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_reg.h"
#include "reviser_hw.h"
#include "reviser_mem.h"
#include "reviser_secure.h"
#include "apusys_power.h"

#define FAKE_CONTEX_REG_NUM 9
#define FAKE_REMAP_REG_NUM 13

#define REG_DEBUG 0

static uint32_t g_ctx_reg[FAKE_CONTEX_REG_NUM];
static uint32_t g_remap_reg[FAKE_REMAP_REG_NUM];
static uint32_t g_mva_reg;
static struct reviser_mem g_mem_sys;

static uint32_t *_reviser_reg_fake_search(uint32_t offset)
		__attribute__((unused));
static uint32_t _reviser_reg_read(void *base, uint32_t offset);
static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_set(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_clr(void *base, uint32_t offset, uint32_t value);
static uint32_t _reviser_get_contex_offset(enum REVISER_DEVICE_E type,
		int index);
static uint32_t _reviser_get_remap_offset(int index);

APUSYS_ATTR_USE static void _reviser_set_contex_boundary(void *drvinfo,
		uint32_t offset, uint8_t boundary);
static void _reviser_set_context_ID(void *drvinfo,
		uint32_t offset, uint8_t ID);
static void _reviser_set_remap_table(void *drvinfo,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page);
static void _reviser_set_default_iova(void *drvinfo,
		uint32_t iova);

void reviser_print_private(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	reviser_device = (struct reviser_dev_info *)drvinfo;
	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;
	LOG_INFO("=============================");
	LOG_INFO(" reviser driver private reviser_device\n");
	LOG_INFO("-----------------------------");
	LOG_INFO("pctrl_top: %p\n",	reviser_device->pctrl_top);
	LOG_INFO("=============================");

}

void reviser_print_error(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	unsigned long flags;
	int count = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	spin_lock_irqsave(&reviser_device->lock_dump, flags);
	count = reviser_device->dump.err_count;
	spin_unlock_irqrestore(&reviser_device->lock_dump, flags);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser error info\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "count: %d\n", count);
	LOG_CON(s, "=============================\n");


}

void reviser_print_boundary(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t reg[FAKE_CONTEX_REG_NUM];
	uint32_t offset = 0;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;



	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[0] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[1] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[2] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[3] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 2);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[4] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[5] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[6] = _reviser_reg_read(reviser_device->pctrl_top, offset) &
			VLM_CTXT_BDY_SELECT;

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver boundary info\n");
	LOG_CON(s, "-----------------------------\n");

	LOG_CON(s, "MDLA0: %.8x\n", reg[0]);
	LOG_CON(s, "MDLA1: %.8x\n", reg[1]);
	LOG_CON(s, "VPU0:  %.8x\n", reg[2]);
	LOG_CON(s, "VPU1:  %.8x\n", reg[3]);
	LOG_CON(s, "VPU2:  %.8x\n", reg[4]);
	LOG_CON(s, "EDMA0: %.8x\n", reg[5]);
	LOG_CON(s, "EDMA1: %.8x\n", reg[6]);

	LOG_CON(s, "=============================\n");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_context_ID(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t reg[FAKE_CONTEX_REG_NUM];
	uint32_t offset = 0;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[0] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[1] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[2] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[3] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, 2);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[4] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;


	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 0);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[5] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, 1);
	if (offset == REVISER_FAIL)
		goto fail_offset;
	reg[6] = (_reviser_reg_read(reviser_device->pctrl_top, offset)
			& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver ID info\n");
	LOG_CON(s, "-----------------------------\n");

	LOG_CON(s, "MDLA0: %.8x\n", reg[0]);
	LOG_CON(s, "MDLA1: %.8x\n", reg[1]);
	LOG_CON(s, "VPU0:  %.8x\n", reg[2]);
	LOG_CON(s, "VPU1:  %.8x\n", reg[3]);
	LOG_CON(s, "VPU2:  %.8x\n", reg[4]);
	LOG_CON(s, "EDMA0: %.8x\n", reg[5]);
	LOG_CON(s, "EDMA1: %.8x\n", reg[6]);

	LOG_CON(s, "=============================\n");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_remap_table(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t reg[FAKE_REMAP_REG_NUM];
	uint32_t offset[FAKE_REMAP_REG_NUM];
	int i = 0;
	uint32_t valid, ID, src, dst;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	for (i = 0; i < FAKE_REMAP_REG_NUM; i++) {
		offset[i] = _reviser_get_remap_offset(i);
		if (offset[i] == REVISER_FAIL) {
			LOG_ERR("invalid argument\n");
			goto fail_offset;
		}
		reg[i] = _reviser_reg_read(
				reviser_device->pctrl_top,
				offset[i]);
	}

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver remap info\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < FAKE_REMAP_REG_NUM; i++) {
		valid = reg[i] >> VLM_REMAP_VALID_OFFSET;
		ID = (reg[i] & VLM_REMAP_CTX_ID) >> VLM_REMAP_CTX_ID_OFFSET;
		src = (reg[i] & VLM_REMAP_CTX_SRC) >> VLM_REMAP_CTX_SRC_OFFSET;
		dst = (reg[i] & VLM_REMAP_CTX_DST) >> VLM_REMAP_CTX_DST_OFFSET;

		LOG_CON(s, "[%02d]: valid[%d] ID[%02d] src[%02d] dst[%02d]\n",
				i, valid, ID, src, dst);
	}

	LOG_CON(s, "=============================\n");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_default_iova(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t reg;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	reg = _reviser_reg_read(reviser_device->pctrl_top, VLM_DEFAULT_MVA);

	LOG_INFO("=============================");
	LOG_INFO(" reviser driver default iova\n");
	LOG_INFO("-----------------------------");
	LOG_INFO("Default: %.8x\n", reg);
	LOG_INFO("=============================");

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
	case VLM_DEFAULT_MVA:
		return &g_mva_reg;
	default:
		LOG_ERR("offset invalid %.8x\n", offset);
		return NULL;
	}

}

static uint32_t _reviser_reg_read(void *base, uint32_t offset)
{
#if REG_DEBUG
	uint32_t *pReg = NULL;

	pReg = _reviser_reg_fake_search(offset);
	LOG_DEBUG("offset: %p value %.8x\n", offset, *pReg);
	return *pReg;
#else
#if 0 //Print for debug
	uint32_t value = 0;

	value = ioread32(base + offset);
	LOG_DEBUG("offset: %p value %.8x\n", offset, value);
#endif
	return ioread32(base + offset);
#endif

}

static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value)
{
#if REG_DEBUG
	uint32_t *pReg = NULL;

	LOG_DEBUG("offset: %p value: %.8x\n", offset, value);
	pReg = _reviser_reg_fake_search(offset);
	if (pReg == NULL)
		return;

	*pReg = value;
#else
	iowrite32(value, base + offset);
#endif
}

static void _reviser_reg_set(void *base, uint32_t offset, uint32_t value)
{
	//DEBUG_TAG;
	_reviser_reg_write(base,
			offset, _reviser_reg_read(base, offset) | value);
}

static void _reviser_reg_clr(void *base, uint32_t offset, uint32_t value)
{
	//DEBUG_TAG;
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

static void _reviser_set_contex_boundary(void *drvinfo,
		uint32_t offset, uint8_t boundary)
{
	struct reviser_dev_info *reviser_device = NULL;

	reviser_device = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_CTXT_BDY_SELECT);
	_reviser_reg_set(reviser_device->pctrl_top,
			offset, boundary & VLM_CTXT_BDY_SELECT);

}
static void _reviser_set_context_ID(void *drvinfo, uint32_t offset, uint8_t ID)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;
	reviser_device = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_CTXT_CTX_ID);
	_reviser_reg_set(reviser_device->pctrl_top,
			offset, (ID << VLM_CTXT_CTX_ID_OFFSET));

}

static void  _reviser_set_remap_table(void *drvinfo,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;
	reviser_device = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_REMAP_VALID);

	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_REMAP_CTX_ID);
	_reviser_reg_set(reviser_device->pctrl_top,
			offset, (ID << VLM_REMAP_CTX_ID_OFFSET));
	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_REMAP_CTX_SRC);
	_reviser_reg_set(reviser_device->pctrl_top,
			offset, (src_page << VLM_REMAP_CTX_SRC_OFFSET));
	_reviser_reg_clr(reviser_device->pctrl_top,
			offset, VLM_REMAP_CTX_DST);
	_reviser_reg_set(reviser_device->pctrl_top,
			offset, (dst_page << VLM_REMAP_CTX_DST_OFFSET));

	if (valid)
		_reviser_reg_set(reviser_device->pctrl_top,
				offset, (1 << VLM_REMAP_VALID_OFFSET));
}

int reviser_type_convert(int type, enum REVISER_DEVICE_E *reviser_type)
{
	int ret = 0;

	DEBUG_TAG;

	switch (type) {

	case APUSYS_DEVICE_MDLA:
		*reviser_type = REVISER_DEVICE_MDLA;
		break;
	case APUSYS_DEVICE_VPU:
		*reviser_type = REVISER_DEVICE_VPU;
		break;
	case APUSYS_DEVICE_EDMA:
		*reviser_type = REVISER_DEVICE_EDMA;
		break;
	default:
		*reviser_type = REVISER_DEVICE_MAX;
		ret = -EINVAL;
		break;
	}

	return ret;
}

int reviser_set_remap_table(void *drvinfo,
		int index, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	uint32_t offset = 0;

	DEBUG_TAG;

	if (index > VLM_REMAP_TABLE_DST_MAX) {
		LOG_ERR("invalid index (out of range) %d\n",
				index);
		return -1;
	}
	if (ID >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range) %d\n",
				ID);
		return -1;
	}
	if (src_page > VLM_REMAP_TABLE_SRC_MAX) {
		LOG_ERR("invalid src page (out of range) %d\n",
				src_page);
		return -1;
	}

	if (dst_page > VLM_REMAP_TABLE_DST_MAX) {
		LOG_ERR("invalid dst page (out of range) %d\n",
				dst_page);
		return -1;
	}

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	//LOG_DEBUG("index: %u valid: %u ID: %u src: %u dst: %u\n",
	//		index, valid, ID,	src_page, dst_page);

	offset = _reviser_get_remap_offset(index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}

	_reviser_set_remap_table(drvinfo,
			offset, valid, ID, src_page, dst_page);

	return 0;
}

int reviser_set_boundary(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary)
{
	APUSYS_ATTR_USE uint32_t offset;
	uint32_t value = 0;

	DEBUG_TAG;

	if (boundary > VLM_CTXT_BDY_SELECT_MAX) {
		LOG_ERR("invalid boundary (out of range) %d\n",
				VLM_CTXT_BDY_SELECT_MAX);
		return -1;
	}
	if (type >= REVISER_DEVICE_MAX) {
		LOG_ERR("invalid type (out of range) %d\n",
				type);
		return -1;
	}
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
#if APUSYS_SECURE
	value = ((BOUNDARY_ALL_NO_CHANGE) & ~(BOUNDARY_BIT_MASK << (index*4)));
	value = (value | boundary << (index*4));
	//LOG_DEBUG("value 0x%x\n", value);

	switch (type) {
	case REVISER_DEVICE_MDLA:

		mt_secure_call(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				value,
				BOUNDARY_ALL_NO_CHANGE,
				BOUNDARY_ALL_NO_CHANGE
				);
		break;
	case REVISER_DEVICE_VPU:
		mt_secure_call(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				BOUNDARY_ALL_NO_CHANGE,
				value,
				BOUNDARY_ALL_NO_CHANGE
				);
		break;
	case REVISER_DEVICE_EDMA:

		mt_secure_call(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				BOUNDARY_ALL_NO_CHANGE,
				BOUNDARY_ALL_NO_CHANGE,
				value
				);
		break;
	default:
		LOG_ERR("invalid argument\n");
		return -1;
	}

#else
	offset = _reviser_get_contex_offset(type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	_reviser_set_contex_boundary(drvinfo, offset, boundary);
#endif
	return 0;
}


int reviser_set_context_ID(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t ID)
{
	uint32_t offset = 0;

	DEBUG_TAG;

	if (ID >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range) %d\n", VLM_CTXT_CTX_ID_MAX);
		return -1;
	}

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	offset = _reviser_get_contex_offset(type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	_reviser_set_context_ID(drvinfo, offset, ID);

	return 0;
}


static void _reviser_set_default_iova(void *drvinfo,
		uint32_t iova)
{
	struct reviser_dev_info *reviser_device = NULL;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(reviser_device->pctrl_top,
			VLM_DEFAULT_MVA, REVISER_DEFAULT);
	_reviser_reg_set(reviser_device->pctrl_top,
			VLM_DEFAULT_MVA, iova);

}

int reviser_get_interrupt_offset(void *drvinfo)
{
	uint32_t offset = 0;
	int ret = 0;

	struct reviser_dev_info *reviser_device = NULL;


	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_MDLA_0)) {
		offset = AXI_EXCEPTION_MDLA_0;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_MDLA_0\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_MDLA_1)) {
		offset = AXI_EXCEPTION_MDLA_1;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_MDLA_1\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_VPU_0)) {
		offset = AXI_EXCEPTION_VPU_0;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_VPU_0\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_VPU_1)) {
		offset = AXI_EXCEPTION_VPU_1;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_VPU_1\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_VPU_2)) {
		offset = AXI_EXCEPTION_VPU_2;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_VPU_2\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_EDMA_0)) {
		offset = AXI_EXCEPTION_EDMA_0;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_EDMA_0\n");
	} else if (_reviser_reg_read(reviser_device->pctrl_top,
			AXI_EXCEPTION_EDMA_1)) {
		offset = AXI_EXCEPTION_EDMA_1;
		LOG_DEBUG("Interrupt from AXI_EXCEPTION_EDMA_1\n");
	} else {
		//LOG_ERR("Unknown Interrupt\n");
		return -EINVAL;
	}

	if (offset > 0) {
		_reviser_reg_set(reviser_device->pctrl_top,
				offset, 1);
	}


	return ret;
}
int reviser_set_default_iova(void *drvinfo)
{
	if (g_mem_sys.iova == 0) {
		LOG_ERR("invalid iova\n");
		return -1;
	}
	_reviser_set_default_iova(drvinfo, g_mem_sys.iova);

	LOG_INFO("Set IOVA %x\n", g_mem_sys.iova);

	return 0;
}

bool reviser_is_power(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;
	unsigned long flags;
	bool is_power = false;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return is_power;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	spin_lock_irqsave(&reviser_device->lock_power, flags);
	if (reviser_device->power) {
		//LOG_ERR("Can Not Read when power disable\n");
		is_power = true;
	}
	spin_unlock_irqrestore(&reviser_device->lock_power, flags);

	return is_power;
}

int reviser_dram_remap_init(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	reviser_mem_init();

	g_mem_sys.size = REMAP_DRAM_SIZE;
	if (reviser_mem_alloc(&g_mem_sys)) {
		LOG_ERR("alloc fail\n");
		return -ENOMEM;
	}

	//_reviser_set_default_iova(drvinfo, g_mem_sys.iova);

	reviser_device->dram_base = (void *) g_mem_sys.kva;

	return 0;
}
int reviser_dram_remap_destroy(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	reviser_mem_free(&g_mem_sys);
	reviser_mem_destroy();
	reviser_device->dram_base = NULL;
	return 0;
}
int reviser_alloc_mem(void *usr)
{
	return reviser_mem_alloc((struct reviser_mem *) usr);
}
int reviser_free_mem(void *usr)
{
	return reviser_mem_free((struct reviser_mem *) usr);
}

void reviser_init_mem(void)
{
	reviser_mem_init();
}
void reviser_destroy_mem(void)
{
	reviser_mem_destroy();
}

int reviser_boundary_init(void *drvinfo, uint8_t boundary)
{
	int i = 0;

	DEBUG_TAG;

	LOG_INFO("boundary %u\n", boundary);

	for (i = 0; i < VLM_CTXT_MDLA_MAX; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_MDLA,
			i, boundary)) {
			return -1;
		}
	}
	for (i = 0; i < VLM_CTXT_VPU_MAX; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_VPU,
			i, boundary)) {
			return -1;
		}
	}
	for (i = 0; i < VLM_CTXT_EDMA_MAX; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_EDMA,
			i, boundary)) {
			return -1;
		}
	}

	return 0;
}

void reviser_enable_interrupt(void *drvinfo,
		uint8_t enable)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t value = 0;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;

	if (enable) {
		_reviser_reg_clr(reviser_device->int_base,
				REVISER_INT_EN, REVISER_INT_EN_MASK);
	} else {
		_reviser_reg_set(reviser_device->int_base,
				REVISER_INT_EN, REVISER_INT_EN_MASK);
	}

	value = _reviser_reg_read(reviser_device->int_base, REVISER_INT_EN);

	LOG_DEBUG("value: %x\n", value);


}



int reviser_alloc_tcm(void *drvinfo, void *usr)
{
	struct reviser_dev_info *reviser_device = NULL;
	struct reviser_mem *mem;

	reviser_device = (struct reviser_dev_info *)drvinfo;
	mem = (struct reviser_mem *) usr;


	mem->kva = (uint64_t) reviser_device->tcm_base;
	mem->iova = TCM_BASE;

	LOG_DEBUG("kva:%llx, mva:%x,size:%x\n", mem->kva, mem->iova,
			mem->size);

	return 0;
}
int reviser_free_tcm(void *drvinfo, void *usr)
{
	struct reviser_mem *mem;
	struct reviser_dev_info *reviser_device = NULL;

	reviser_device = (struct reviser_dev_info *)drvinfo;
	mem = (struct reviser_mem *) usr;

	mem->kva = 0;
	mem->iova = 0;

	LOG_DEBUG("kva:%llx, mva:%x,size:%x\n", mem->kva, mem->iova,
			mem->size);

	return 0;
}

int reviser_power_on(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;
	int ret = 0;

	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_power);
	if (reviser_device->power_count == 0) {

		ret = apu_device_power_on(REVISER);
		if (ret < 0)
			LOG_ERR("PowerON Fail (%d)\n", ret);

	}
	reviser_device->power_count++;
	mutex_unlock(&reviser_device->mutex_power);

	return ret;
}

int reviser_power_off(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;
	int ret = 0;

	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_power);
	reviser_device->power_count--;

	if (reviser_device->power_count == 0) {

		ret = apu_device_power_off(REVISER);
		if (ret < 0)
			LOG_ERR("PowerON Fail (%d)\n", ret);

	}
	mutex_unlock(&reviser_device->mutex_power);

	return ret;
}


