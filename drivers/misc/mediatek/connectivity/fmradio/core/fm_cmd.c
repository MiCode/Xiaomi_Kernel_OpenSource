/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#include <linux/kernel.h>
#include <linux/types.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_rds.h"
#include "fm_config.h"
#include "fm_link.h"
#include "fm_cmd.h"

fm_s32 fm_bop_write(fm_u8 addr, fm_u16 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_WRITE_BASIC_OP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_WRITE_BASIC_OP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_WRITE_BASIC_OP;
	buf[1] = FM_WRITE_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((value) & 0x00FF);
	buf[4] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

	return FM_WRITE_BASIC_OP_SIZE + 2;
}

fm_s32 fm_bop_udelay(fm_u32 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_UDELAY_BASIC_OP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_UDELAY_BASIC_OP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_UDELAY_BASIC_OP;
	buf[1] = FM_UDELAY_BASIC_OP_SIZE;
	buf[2] = (fm_u8) ((value) & 0x000000FF);
	buf[3] = (fm_u8) ((value >> 8) & 0x000000FF);
	buf[4] = (fm_u8) ((value >> 16) & 0x000000FF);
	buf[5] = (fm_u8) ((value >> 24) & 0x000000FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return FM_UDELAY_BASIC_OP_SIZE + 2;
}

fm_s32 fm_bop_rd_until(fm_u8 addr, fm_u16 mask, fm_u16 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_RD_UNTIL_BASIC_OP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_RD_UNTIL_BASIC_OP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_RD_UNTIL_BASIC_OP;
	buf[1] = FM_RD_UNTIL_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((mask) & 0x00FF);
	buf[4] = (fm_u8) ((mask >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((value) & 0x00FF);
	buf[6] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return FM_RD_UNTIL_BASIC_OP_SIZE + 2;
}

fm_s32 fm_bop_modify(fm_u8 addr, fm_u16 mask_and, fm_u16 mask_or, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_MODIFY_BASIC_OP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_MODIFY_BASIC_OP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_MODIFY_BASIC_OP;
	buf[1] = FM_MODIFY_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((mask_and) & 0x00FF);
	buf[4] = (fm_u8) ((mask_and >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((mask_or) & 0x00FF);
	buf[6] = (fm_u8) ((mask_or >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return FM_MODIFY_BASIC_OP_SIZE + 2;
}

fm_s32 fm_bop_top_write(fm_u16 addr, fm_u32 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_TOP_WRITE_BOP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_TOP_WRITE_BOP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_TOP_WRITE_BASIC_OP;
	buf[1] = FM_TOP_WRITE_BOP_SIZE;
	buf[2] = 04;
	buf[3] = (fm_u8) ((addr) & 0x00FF);
	buf[4] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((value) & 0x00FF);
	buf[6] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[8] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1],
		buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

	return FM_TOP_WRITE_BOP_SIZE + 2;
}

fm_s32 fm_bop_top_rd_until(fm_u16 addr, fm_u32 mask, fm_u32 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_TOP_RD_UNTIL_BOP_SIZE + 2)) {
		WCN_DBG(FM_ERR | CHIP, "%s : left size(%d)/need size(%d)\n",
			__func__, size, FM_TOP_RD_UNTIL_BOP_SIZE + 2);
		return -1;
	}

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -2;
	}

	buf[0] = FM_TOP_RD_UNTIL_BASIC_OP;
	buf[1] = FM_TOP_RD_UNTIL_BOP_SIZE;
	buf[2] = 04;
	buf[3] = (fm_u8) ((addr) & 0x00FF);
	buf[4] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((mask) & 0x00FF);
	buf[6] = (fm_u8) ((mask >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((mask >> 16) & 0x00FF);
	buf[8] = (fm_u8) ((mask >> 24) & 0x00FF);
	buf[9] = (fm_u8) ((value) & 0x00FF);
	buf[10] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[11] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[12] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
		buf[10], buf[11], buf[12]);

	return FM_TOP_RD_UNTIL_BOP_SIZE + 2;
}

fm_s32 fm_op_seq_combine_cmd(fm_u8 *buf, fm_u8 opcode, fm_s32 pkt_size)
{
	fm_s32 total_size = 0;

	if (NULL == buf) {
		WCN_DBG(FM_ERR | CHIP, "%s :buf invalid pointer\n", __func__);
		return -1;
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = opcode;
	buf[2] = (fm_u8) (pkt_size & 0x00FF);
	buf[3] = (fm_u8) ((pkt_size >> 8) & 0x00FF);
	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);

	total_size = pkt_size + 4;

	return total_size;
}

/*
 * fm_patch_download - Wholechip FM Power Up: step 3, download patch to f/w,
 * @buf - target buf
 * @buf_size - buffer size
 * @seg_num - total segments that this patch divided into
 * @seg_id - No. of Segments: segment that will now be sent
 * @src - patch source buffer
 * @seg_len - segment size: segment that will now be sent
 * return package size
 */
fm_s32 fm_patch_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len)
{
	fm_s32 pkt_size = 0;
	fm_u8 *dst = NULL;

	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_PATCH_DOWNLOAD_OPCODE;
	pkt_size = 4;

	buf[pkt_size++] = seg_num;
	buf[pkt_size++] = seg_id;

	if (seg_len > (buf_size - pkt_size))
		return -1;

	dst = &buf[pkt_size];
	pkt_size += seg_len;

	/* copy patch to tx buffer */
	while (seg_len--) {
		*dst = *src;
		src++;
		dst++;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);
	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return pkt_size;
}

/*
 * fm_coeff_download - Wholechip FM Power Up: step 3,download coeff to f/w,
 * @buf - target buf
 * @buf_size - buffer size
 * @seg_num - total segments that this patch divided into
 * @seg_id - No. of Segments: segment that will now be sent
 * @src - patch source buffer
 * @seg_len - segment size: segment that will now be sent
 * return package size
 */
fm_s32 fm_coeff_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len)
{
	fm_s32 pkt_size = 0;
	fm_u8 *dst = NULL;

	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_COEFF_DOWNLOAD_OPCODE;
	pkt_size = 4;

	buf[pkt_size++] = seg_num;
	buf[pkt_size++] = seg_id;

	if (seg_len > (buf_size - pkt_size))
		return -1;

	dst = &buf[pkt_size];
	pkt_size += seg_len;

	/* copy patch to tx buffer */
	while (seg_len--) {
		*dst = *src;
		src++;
		dst++;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);
	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return pkt_size;
}

/*
 * fm_full_cqi_req - execute request cqi info action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 7600 ~ 10800, freq array
 * @cnt - channel count
 * @type - request type, 1: a single channel; 2: multi channel; 3:multi channel with 100Khz step; 4: multi channel with 50Khz step
 *
 * return package size
 */
fm_s32 fm_full_cqi_req(fm_u8 *buf, fm_s32 buf_size, fm_u16 *freq, fm_s32 cnt, fm_s32 type)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_SOFT_MUTE_TUNE_OPCODE;
	pkt_size = 4;

	switch (type) {
	case 1:
		buf[pkt_size] = 0x0001;
		pkt_size++;
		buf[pkt_size] = (fm_u8) ((*freq) & 0x00FF);
		pkt_size++;
		buf[pkt_size] = (fm_u8) ((*freq >> 8) & 0x00FF);
		pkt_size++;
		break;
	case 2:
		buf[pkt_size] = 0x0002;
		pkt_size++;
		break;
	case 3:
		buf[pkt_size] = 0x0003;
		pkt_size++;
		break;
	case 4:
		buf[pkt_size] = 0x0004;
		pkt_size++;
		break;
	default:
		buf[pkt_size] = (fm_u16) type;
		pkt_size++;
		break;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


fm_s32 fm_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FSPI_READ_OPCODE;
	buf[2] = 0x01;
	buf[3] = 0x00;
	buf[4] = addr;

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
	return 5;
}

fm_s32 fm_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 value)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FSPI_WRITE_OPCODE;
	buf[2] = 0x03;
	buf[3] = 0x00;
	buf[4] = addr;
	buf[5] = (fm_u8) ((value) & 0x00FF);
	buf[6] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);
	return 7;
}

fm_s32 fm_set_bits_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 bits, fm_u16 mask)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = 0x11;		/* 0x11 this opcode won't be parsed as an opcode, so set here as spcial case. */
	pkt_size = 4;
	pkt_size += fm_bop_modify(addr, mask, bits, &buf[pkt_size], buf_size - pkt_size);

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*top register read*/
fm_s32 fm_top_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = CSPI_READ_OPCODE;
	buf[2] = 0x03;
	buf[3] = 0x00;
	buf[4] = 0x04;		/* top 04,fm 02 */
	buf[5] = (fm_u8) ((addr) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);
	return 7;
}

fm_s32 fm_top_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr, fm_u32 value)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = CSPI_WRITE_OPCODE;
	buf[2] = 0x07;
	buf[3] = 0x00;
	buf[4] = 0x04;		/* top 04,fm 02 */
	buf[5] = (fm_u8) ((addr) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((value) & 0x00FF);
	buf[8] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[9] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[10] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0],
		buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10]);
	return 11;
}

/*host register read*/
fm_s32 fm_host_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_HOST_READ_OPCODE;
	buf[2] = 0x04;
	buf[3] = 0x00;
	buf[4] = (fm_u8) ((addr) & 0x00FF);
	buf[5] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 16) & 0x00FF);
	buf[7] = (fm_u8) ((addr >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6], buf[7]);
	return 8;
}

fm_s32 fm_host_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr, fm_u32 value)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_HOST_WRITE_OPCODE;
	buf[2] = 0x08;
	buf[3] = 0x00;
	buf[4] = (fm_u8) ((addr) & 0x00FF);
	buf[5] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 16) & 0x00FF);
	buf[7] = (fm_u8) ((addr >> 24) & 0x00FF);
	buf[8] = (fm_u8) ((value) & 0x00FF);
	buf[9] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[10] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[11] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
	return 12;
}

fm_s32 fm_pmic_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	if (buf == NULL)
		return -2;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_READ_PMIC_CR_OPCODE;
	buf[2] = 0x01;
	buf[3] = 0x00;
	buf[4] = addr;

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4]);
	return 5;
}

fm_s32 fm_pmic_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u32 val)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	if (buf == NULL)
		return -2;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_WRITE_PMIC_CR_OPCODE;
	buf[2] = 0x05;
	buf[3] = 0x00;
	buf[4] = addr;
	buf[5] = (fm_u8) ((val) & 0x00FF);
	buf[6] = (fm_u8) ((val >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((val >> 16) & 0x00FF);
	buf[8] = (fm_u8) ((val >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
	return 9;
}

fm_s32 fm_pmic_mod_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u32 mask_and, fm_u32 mask_or)
{
	if (buf_size < TX_BUF_SIZE)
		return -1;

	if (buf == NULL)
		return -2;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_MODIFY_PMIC_CR_OPCODE;
	buf[2] = 0x09;
	buf[3] = 0x00;
	buf[4] = addr;
	buf[5] = (fm_u8) ((mask_and) & 0x00FF);
	buf[6] = (fm_u8) ((mask_and >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((mask_and >> 16) & 0x00FF);
	buf[8] = (fm_u8) ((mask_and >> 24) & 0x00FF);
	buf[9] = (fm_u8) ((mask_or) & 0x00FF);
	buf[10] = (fm_u8) ((mask_or >> 8) & 0x00FF);
	buf[11] = (fm_u8) ((mask_or >> 16) & 0x00FF);
	buf[12] = (fm_u8) ((mask_or >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0],
		buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12]);

	return 13;
}

fm_s32 fm_get_patch_path(fm_s32 ver, fm_u8 *buff, int buffsize, struct fm_patch_tbl *patch_tbl)
{
	fm_s32 i;
	fm_s32 patch_len = 0;
	fm_s32 max = FM_ROM_MAX;
	const fm_s8 *ppath = NULL;

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if (patch_tbl[i].idx == ver) {
			ppath = patch_tbl[i].patch;
			WCN_DBG(FM_NTC | CHIP, "Get ROM version OK\n");
			break;
		}
	}

	if (ppath == NULL) {
		/* Load latest default patch */
		for (i = max; i > 0; i--) {
			patch_len = fm_file_read(patch_tbl[i - 1].patch, buff, buffsize, 0);
			if (patch_len >= 0) {
				WCN_DBG(FM_NTC | CHIP, "undefined ROM version, load %s\n", patch_tbl[i - 1].patch);
				return patch_len;
			}
		}
	} else {
		/* Load  patch */
		patch_len = fm_file_read(ppath, buff, buffsize, 0);
		if (patch_len >= 0)
			return patch_len;
	}

	/* get path failed */
	WCN_DBG(FM_ERR | CHIP, "No valid patch file\n");
	return -FM_EPATCH;
}

fm_s32 fm_get_coeff_path(fm_s32 ver, fm_u8 *buff, int buffsize, struct fm_patch_tbl *patch_tbl)
{
	fm_s32 i;
	fm_s32 patch_len = 0;
	const fm_s8 *ppath = NULL;
	fm_s32 max = FM_ROM_MAX;

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if (patch_tbl[i].idx == ver) {
			ppath = patch_tbl[i].coeff;
			WCN_DBG(FM_NTC | CHIP, "Get ROM version OK\n");
			break;
		}
	}

	if (ppath == NULL) {
		/* Load default patch */
		for (i = max; i > 0; i--) {
			patch_len = fm_file_read(patch_tbl[i - 1].coeff, buff, buffsize, 0);
			if (patch_len >= 0) {
				WCN_DBG(FM_NTC | CHIP, "undefined ROM version, load %s\n", patch_tbl[i - 1].coeff);
				return patch_len;
			}
		}
	} else {
		/* Load patch by patch path*/
		patch_len = fm_file_read(ppath, buff, buffsize, 0);
		if (patch_len >= 0)
			return patch_len;
	}

	/* get path failed */
	WCN_DBG(FM_ERR | CHIP, "No valid coeff file\n");
	return -FM_EPATCH;
}

/*
*  DspPatch - DSP download procedure
*  @img - source dsp bin code
*  @len - patch length in byte
*  @type - rom/patch/coefficient/hw_coefficient
*/
fm_s32 fm_download_patch(const fm_u8 *img, fm_s32 len, enum IMG_TYPE type)
{
	fm_u8 seg_num;
	fm_u8 seg_id = 0;
	fm_s32 seg_len;
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (img == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (len <= 0)
		return -1;

	seg_num = len / PATCH_SEG_LEN + 1;
	WCN_DBG(FM_NTC | CHIP, "binary len:%d, seg num:%d\n", len, seg_num);

	switch (type) {
	case IMG_PATCH:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "patch,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return -FM_ELOCK;
			pkt_size =
			    fm_patch_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_PATCH, SW_RETRY_CNT, PATCH_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "fm_patch_download failed\n");
				return ret;
			}
		}

		break;

	case IMG_COEFFICIENT:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "coeff,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return -FM_ELOCK;
			pkt_size =
			    fm_coeff_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_COEFF, SW_RETRY_CNT, COEFF_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "fm_coeff_download failed\n");
				return ret;
			}
		}

		break;
	default:
		break;
	}

	return 0;
}

fm_s32 fm_get_read_result(struct fm_res_ctx *result)
{
	if (result == NULL) {
		WCN_DBG(FM_ERR | CHIP, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_res = result;

	return 0;
}

fm_s32 fm_reg_read(fm_u8 addr, fm_u16 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_RD, SW_RETRY_CNT, FSPI_RD_TIMEOUT, fm_get_read_result);

	if (!ret && fm_res)
		*val = fm_res->fspi_rd;

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_reg_write(fm_u8 addr, fm_u16 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_WR, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_set_bits(fm_u8 addr, fm_u16 bits, fm_u16 mask)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_set_bits_reg(cmd_buf, TX_BUF_SIZE, addr, bits, mask);
	ret = fm_cmd_tx(cmd_buf, pkt_size, (1 << 0x11), SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	/* 0x11 this opcode won't be parsed as an opcode, so set here as spcial case. */
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_top_reg_read(fm_u16 addr, fm_u32 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_top_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_CSPI_READ, SW_RETRY_CNT, FSPI_RD_TIMEOUT, fm_get_read_result);

	if (!ret && fm_res)
		*val = fm_res->cspi_rd;

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_top_reg_write(fm_u16 addr, fm_u32 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_top_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_CSPI_WRITE, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_host_reg_read(fm_u32 addr, fm_u32 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_host_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_READ, SW_RETRY_CNT, FSPI_RD_TIMEOUT, fm_get_read_result);

	if (!ret && fm_res)
		*val = fm_res->cspi_rd;

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

fm_s32 fm_host_reg_write(fm_u32 addr, fm_u32 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return -FM_ELOCK;
	pkt_size = fm_host_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_WRITE, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

