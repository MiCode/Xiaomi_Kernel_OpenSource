// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include "reviser_reg_v1_0.h"
#include "reviser_hw_v1_0.h"
#include "reviser_power.h"
#include "reviser_secure.h"


#define UNKNOWN_INT_MAX (500000)

#define REG_DEBUG 0



static uint32_t _reviser_ctrl_reg_read(void *drvinfo, uint32_t offset);
static uint32_t _reviser_int_reg_read(void *drvinfo, uint32_t offset);
static uint32_t _reviser_reg_read(void *base, uint32_t offset);
static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_set(void *base, uint32_t offset, uint32_t value);
static void _reviser_reg_clr(void *base, uint32_t offset, uint32_t value);
static uint32_t _reviser_get_contex_offset(enum REVISER_DEVICE_E type,
		int index);
static uint32_t _reviser_get_remap_offset(int index);

APUSYS_ATTR_USE static void _reviser_set_contex_boundary(void *drvinfo,
		uint32_t offset, uint8_t boundary);
APUSYS_ATTR_USE static void _reviser_set_context_ID(void *drvinfo,
		uint32_t offset, uint8_t ID);
APUSYS_ATTR_USE static void _reviser_set_remap_table(void *drvinfo,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page);
static uint32_t _reviser_get_remap_table_reg(
		uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page);
APUSYS_ATTR_USE static void _reviser_set_default_iova(void *drvinfo,
		uint32_t iova);


int reviser_isr(void *drvinfo)
{
	struct reviser_dev_info *rdv;
	unsigned long flags;
	int ret = 0;


	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -ENODEV;
	}

	// Check if INT is for reviser
	if (reviser_check_int_valid(rdv))
		return -EINVAL;


	if (!reviser_get_interrupt_offset(rdv)) {
		//reviser_print_remap_table(private_data, NULL);
		//reviser_print_context_ID(private_data, NULL);
		spin_lock_irqsave(&rdv->lock.lock_dump, flags);
		rdv->dump.err_count++;
		spin_unlock_irqrestore(&rdv->lock.lock_dump, flags);
		ret = 0;
	} else {
		ret = -EINVAL;
	}

	return ret;

}

void reviser_print_exception(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	uint32_t reg;
	uint32_t reg_state = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return;
	}

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser exception info\n");
	LOG_CON(s, "-----------------------------\n");

	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_MDLA_0);
	LOG_CON(s, "MDLA0: %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_MDLA_1);
	LOG_CON(s, "MDLA1: %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_0);
	LOG_CON(s, "VPU0:  %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_1);
	LOG_CON(s, "VPU1:  %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_2);
	LOG_CON(s, "VPU2:  %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_EDMA_0);
	LOG_CON(s, "EDMA0:  %.8x\n", reg);
	reg = _reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_EDMA_1);
	LOG_CON(s, "EDMA1:  %.8x\n", reg);
	reg_state = _reviser_int_reg_read(rdv,
			APUSYS_EXCEPT_INT);
	LOG_CON(s, "reg_state: %.8x\n", reg_state);
	LOG_CON(s, "=============================\n");
	return;

}

void reviser_print_boundary(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t mdla;
	uint32_t vpu;
	uint32_t edma;
	uint32_t offset = 0;
	struct seq_file *s = (struct seq_file *)s_file;
	int i;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return;
	}

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver boundary info\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_MDLA]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		mdla = _reviser_ctrl_reg_read(rdv, offset) &
				VLM_CTXT_BDY_SELECT;
		LOG_CON(s, "MDLA%d: %.8x\n", i, mdla);
	}

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_VPU]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		vpu = _reviser_ctrl_reg_read(rdv, offset) &
				VLM_CTXT_BDY_SELECT;
		LOG_CON(s, "VPU%d: %.8x\n", i, vpu);

	}

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_EDMA]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		edma = _reviser_ctrl_reg_read(rdv, offset) &
				VLM_CTXT_BDY_SELECT;
		LOG_CON(s, "EDMA%d: %.8x\n", i, edma);
	}

	LOG_CON(s, "=============================\n");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_context_ID(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t offset = 0;
	struct seq_file *s = (struct seq_file *)s_file;
	uint32_t mdla;
	uint32_t vpu;
	uint32_t edma;
	int i;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return;
	}

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver ID info\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_MDLA]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_MDLA, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		mdla = (_reviser_ctrl_reg_read(rdv, offset)
				& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;
		LOG_CON(s, "MDLA%d: %.8x\n", i, mdla);
	}


	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_VPU]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_VPU, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		vpu = (_reviser_ctrl_reg_read(rdv, offset)
				& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;
		LOG_CON(s, "VPU%d: %.8x\n", i, vpu);
	}


	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_EDMA]; i++) {
		offset = _reviser_get_contex_offset(REVISER_DEVICE_EDMA, i);
		if (offset == REVISER_FAIL)
			goto fail_offset;
		edma = (_reviser_ctrl_reg_read(rdv, offset)
				& VLM_CTXT_CTX_ID) >> VLM_CTXT_CTX_ID_OFFSET;
		LOG_CON(s, "EDMA%d: %.8x\n", i, edma);
	}


	LOG_CON(s, "=============================\n");
	return;
fail_offset:
	LOG_ERR("invalid argument\n");
}

void reviser_print_remap_table(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t reg[VLM_REMAP_TABLE_MAX];
	uint32_t offset[VLM_REMAP_TABLE_MAX];
	int i = 0;
	uint32_t valid, ID, src, dst;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return;
	}
	for (i = 0; i < VLM_REMAP_TABLE_MAX; i++) {
		offset[i] = _reviser_get_remap_offset(i);
		if (offset[i] == REVISER_FAIL) {
			LOG_ERR("invalid argument\n");
			goto fail_offset;
		}
		reg[i] = _reviser_ctrl_reg_read(
				rdv,
				offset[i]);
	}

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver remap info\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < VLM_REMAP_TABLE_MAX; i++) {
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

void reviser_print_default_iova(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	uint32_t reg;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return;
	}
	reg = _reviser_ctrl_reg_read(rdv, VLM_DEFAULT_MVA);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser driver default iova\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "Default: %.8x\n", reg);
	LOG_CON(s, "=============================\n");

}


static uint32_t _reviser_ctrl_reg_read(void *drvinfo, uint32_t offset)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;
	size_t value = 0;
	struct arm_smccc_res res;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return ret;
	}

	rdv = (struct reviser_dev_info *)drvinfo;
#if APUSYS_SECURE

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_REVISER_CHK_VALUE,
		offset, 0, 0, 0, 0, 0, &res);

	ret = res.a0;
	value = res.a1;
	if (ret) {
		LOG_ERR("invalid argument %.8x\n", offset);
		ret = 0;
		return ret;
	}

#else
	value = _reviser_reg_read(rdv->rsc.ctrl.base, offset);

#endif

	return value;

}

static uint32_t _reviser_int_reg_read(void *drvinfo, uint32_t offset)
{
	struct reviser_dev_info *rdv = NULL;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	return _reviser_reg_read(rdv->rsc.isr.base, offset);
}

static uint32_t _reviser_reg_read(void *base, uint32_t offset)
{
	return ioread32(base + offset);

}

static void _reviser_reg_write(void *base, uint32_t offset, uint32_t value)
{
	iowrite32(value, base + offset);
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
	struct reviser_dev_info *rdv = NULL;

	rdv = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_CTXT_BDY_SELECT);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, boundary & VLM_CTXT_BDY_SELECT);

}
static void _reviser_set_context_ID(void *drvinfo, uint32_t offset, uint8_t ID)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;
	rdv = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_CTXT_CTX_ID);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, (ID << VLM_CTXT_CTX_ID_OFFSET));

}

static void  _reviser_set_remap_table(void *drvinfo,
		uint32_t offset, uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;
	rdv = (struct reviser_dev_info *)drvinfo;

	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_REMAP_VALID);

	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_REMAP_CTX_ID);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, (ID << VLM_REMAP_CTX_ID_OFFSET));
	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_REMAP_CTX_SRC);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, (src_page << VLM_REMAP_CTX_SRC_OFFSET));
	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, VLM_REMAP_CTX_DST);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, (dst_page << VLM_REMAP_CTX_DST_OFFSET));

	if (valid)
		_reviser_reg_set(rdv->rsc.ctrl.base,
				offset, (1 << VLM_REMAP_VALID_OFFSET));
}
static uint32_t  _reviser_get_remap_table_reg(
		uint8_t valid, uint8_t ID,
		uint8_t src_page, uint8_t dst_page)
{
	uint32_t value = 0;

	DEBUG_TAG;

	//if(valid) {
	//	value = value | (1 << VLM_REMAP_VALID_OFFSET);
	//}

	value = value | (ID << VLM_REMAP_CTX_ID_OFFSET);
	value = value | (src_page << VLM_REMAP_CTX_SRC_OFFSET);
	value = value | (dst_page << VLM_REMAP_CTX_DST_OFFSET);

	//LOG_INFO("value %.8x\n", value);

	return value;
}
int reviser_type_convert(int type, enum REVISER_DEVICE_E *reviser_type)
{
	int ret = 0;

	DEBUG_TAG;

	switch (type) {

	case APUSYS_DEVICE_MDLA:
	case APUSYS_DEVICE_MDLA_RT:
		*reviser_type = REVISER_DEVICE_MDLA;
		break;
	case APUSYS_DEVICE_VPU:
	case APUSYS_DEVICE_VPU_RT:
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
	int ret = 0;
	uint32_t value = 0;
	struct arm_smccc_res res;

	DEBUG_TAG;

	if (index > VLM_REMAP_TABLE_DST_MAX) {
		LOG_ERR("invalid index (out of range) %d\n",
				index);
		return -EINVAL;
	}
	if (ID >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range) %d\n",
				ID);
		return -EINVAL;
	}
	if (src_page > VLM_REMAP_TABLE_SRC_MAX) {
		LOG_ERR("invalid src page (out of range) %d\n",
				src_page);
		return -EINVAL;
	}

	if (dst_page > VLM_REMAP_TABLE_DST_MAX) {
		LOG_ERR("invalid dst page (out of range) %d\n",
				dst_page);
		return -EINVAL;
	}

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	LOG_DBG_RVR_HW("index: %u valid: %u ID: %u src: %u dst: %u\n",
			index, valid, ID,	src_page, dst_page);

	offset = _reviser_get_remap_offset(index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
#if APUSYS_SECURE

	value = _reviser_get_remap_table_reg(valid,
			ID, src_page, dst_page);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
			MTK_APUSYS_KERNEL_OP_REVISER_SET_REMAP_TABLE,
		offset, valid, value, 0, 0, 0, &res);
	ret = res.a0;
	if (ret) {
		LOG_ERR("Set HW RemapTable Fail\n");
		return ret;
	}
#else
	_reviser_set_remap_table(drvinfo,
			offset, valid, ID, src_page, dst_page);
#endif
	return ret;
}

int reviser_set_boundary(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary)
{
	APUSYS_ATTR_USE uint32_t offset;
	uint32_t value = 0;
	struct arm_smccc_res res;

	DEBUG_TAG;

	if (boundary > VLM_CTXT_BDY_SELECT_MAX) {
		LOG_ERR("invalid boundary (out of range) %d\n",
				VLM_CTXT_BDY_SELECT_MAX);
		return -EINVAL;
	}
	if (type >= REVISER_DEVICE_MAX) {
		LOG_ERR("invalid type (out of range) %d\n",
				type);
		return -EINVAL;
	}
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
#if APUSYS_SECURE
	value = ((BOUNDARY_ALL_NO_CHANGE) & ~(BOUNDARY_BIT_MASK << (index*4)));
	value = (value | boundary << (index*4));
	//LOG_DBG_RVR_HW("value 0x%x\n", value);

	switch (type) {
	case REVISER_DEVICE_MDLA:
		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				value, BOUNDARY_ALL_NO_CHANGE,
				BOUNDARY_ALL_NO_CHANGE, 0, 0, 0, &res);
		break;
	case REVISER_DEVICE_VPU:
		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				BOUNDARY_ALL_NO_CHANGE, value,
				BOUNDARY_ALL_NO_CHANGE, 0, 0, 0, &res);
		break;
	case REVISER_DEVICE_EDMA:
		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_SET_BOUNDARY,
				BOUNDARY_ALL_NO_CHANGE,
				BOUNDARY_ALL_NO_CHANGE, value, 0, 0, 0, &res);
		break;
	default:
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

#else
	offset = _reviser_get_contex_offset(type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	_reviser_set_contex_boundary(drvinfo, offset, boundary);
#endif
	return 0;
}
int reviser_set_context_ID(void *drvinfo, int type,
		int index, uint8_t ctx)
{
	uint32_t offset = 0;
	int ret = 0;
	enum REVISER_DEVICE_E reviser_type = REVISER_DEVICE_NONE;
	struct arm_smccc_res res;

	DEBUG_TAG;

	if (type == APUSYS_DEVICE_SAMPLE) {
		LOG_WARN("Ignore Set context\n");
		return ret;
	}

	if (reviser_type_convert(type, &reviser_type)) {
		LOG_ERR("Invalid type\n");
		ret = -EINVAL;
		return ret;
	}

	if (ctx >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid ID (out of range %d) %d\n",
				VLM_CTXT_CTX_ID_MAX, ctx);
		return -EINVAL;
	}

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_power(drvinfo)) {
		LOG_ERR("Can Not set contxet when power disable\n");
		ret = -EINVAL;
		return ret;
	}


	offset = _reviser_get_contex_offset(reviser_type, index);
	if (offset == REVISER_FAIL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
#if APUSYS_SECURE
	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
			MTK_APUSYS_KERNEL_OP_REVISER_SET_CONTEXT_ID,
			offset, ctx, 0, 0, 0, 0, &res);
	ret = res.a0;
	if (ret) {
		LOG_ERR("Set HW CtxID Fail\n");
		return -1;
	}
#else
	_reviser_set_context_ID(drvinfo, offset, ctx);
#endif


	return ret;
}


static void _reviser_set_default_iova(void *drvinfo,
		uint32_t iova)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t offset = 0;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	offset = reviser_get_default_offset();
	_reviser_reg_clr(rdv->rsc.ctrl.base,
			offset, REVISER_DEFAULT);
	_reviser_reg_set(rdv->rsc.ctrl.base,
			offset, iova);

}

int reviser_get_interrupt_offset(void *drvinfo)
{
	uint32_t offset = 0;
	int ret = 0;
	size_t reg_value;
	struct arm_smccc_res res;

	struct reviser_dev_info *rdv = NULL;


	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_MDLA_0)) {
		offset = AXI_EXCEPTION_MDLA_0;
		LOG_ERR("Interrupt from AXI_EXCEPTION_MDLA_0\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_MDLA_1)) {
		offset = AXI_EXCEPTION_MDLA_1;
		LOG_ERR("Interrupt from AXI_EXCEPTION_MDLA_1\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_0)) {
		offset = AXI_EXCEPTION_VPU_0;
		LOG_ERR("Interrupt from AXI_EXCEPTION_VPU_0\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_1)) {
		offset = AXI_EXCEPTION_VPU_1;
		LOG_ERR("Interrupt from AXI_EXCEPTION_VPU_1\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_VPU_2)) {
		offset = AXI_EXCEPTION_VPU_2;
		LOG_ERR("Interrupt from AXI_EXCEPTION_VPU_2\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_EDMA_0)) {
		offset = AXI_EXCEPTION_EDMA_0;
		LOG_ERR("Interrupt from AXI_EXCEPTION_EDMA_0\n");
	} else if (_reviser_ctrl_reg_read(rdv,
			AXI_EXCEPTION_EDMA_1)) {
		offset = AXI_EXCEPTION_EDMA_1;
		LOG_ERR("Interrupt from AXI_EXCEPTION_EDMA_1\n");
	} else {
		LOG_ERR("Unknown Interrupt\n");
		return -EINVAL;
	}

	if (offset > 0) {
#if APUSYS_SECURE
		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
				MTK_APUSYS_KERNEL_OP_REVISER_GET_INTERRUPT_STATUS,
				offset, 0, 0, 0, 0, 0, &res);
		ret = res.a0;
		reg_value = res.a1;
#else
		_reviser_reg_set(rdv->rsc.ctrl.base,
				offset, 1);
#endif
	}


	return ret;
}
int reviser_set_default_iova(void *drvinfo)
{
	int ret = 0;
	struct reviser_dev_info *rdv = NULL;
	struct arm_smccc_res res;
	uint32_t iova;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	if (rdv->plat.dram[0] == 0) {
		LOG_ERR("invalid iova\n");
		return -EINVAL;
	}

	iova = rdv->plat.dram[0];

#if APUSYS_SECURE
	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
			MTK_APUSYS_KERNEL_OP_REVISER_SET_DEFAULT_IOVA,
			iova, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	if (ret) {
		LOG_ERR("Set IOVA Fail\n");
		return -1;
	}

#else
	_reviser_set_default_iova(drvinfo, iova);
#endif

	LOG_DBG_RVR_HW("Set IOVA %x\n", iova);

	return ret;
}


int reviser_boundary_init(void *drvinfo, uint8_t boundary)
{
	int i = 0;
	struct reviser_dev_info *rdv = NULL;


	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	DEBUG_TAG;
	LOG_DBG_RVR_HW("boundary %u\n", boundary);

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_MDLA]; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_MDLA,
			i, boundary)) {
			return -EIO;
		}
	}

	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_VPU]; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_VPU,
			i, boundary)) {
			return -EIO;
		}
	}


	for (i = 0; i < rdv->plat.device[REVISER_DEVICE_EDMA]; i++) {
		if (reviser_set_boundary(
			drvinfo, REVISER_DEVICE_EDMA,
			i, boundary)) {
			return -EIO;
		}
	}


	return 0;
}

int reviser_enable_interrupt(void *drvinfo,
		uint8_t enable)
{
	struct reviser_dev_info *rdv = NULL;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	if (enable) {
		_reviser_reg_set(rdv->rsc.isr.base,
						REVISER_INT_EN,
						REVISER_INT_EN_MASK);
	} else {
		_reviser_reg_clr(rdv->rsc.isr.base,
						REVISER_INT_EN,
						REVISER_INT_EN_MASK);
	}

	return 0;
}



int reviser_check_int_valid(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = -1;
	uint32_t value = 0;
	unsigned int unknown_count = 0;
	unsigned long flags;

	rdv = (struct reviser_dev_info *)drvinfo;

	value = _reviser_int_reg_read(rdv, APUSYS_EXCEPT_INT);

	if ((value & REVISER_INT_EN_MASK) > 0) {
		//LOG_ERR("APUSYS_EXCEPT_INT  (%x)\n", value);
		ret = 0;
	} else {
		//LOG_ERR("Not Reviser INT (%x)\n", value);
		spin_lock_irqsave(&rdv->lock.lock_dump, flags);
		rdv->dump.unknown_count++;
		unknown_count = rdv->dump.unknown_count;
		spin_unlock_irqrestore(&rdv->lock.lock_dump, flags);

		//Show unknown INT
		if (unknown_count % UNKNOWN_INT_MAX == UNKNOWN_INT_MAX - 1) {
			LOG_ERR("unknown INT over %u (%.8x)\n",
					UNKNOWN_INT_MAX, value);
		}

		ret = -1;
	}

	return ret;
}
