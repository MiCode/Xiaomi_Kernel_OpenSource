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

#ifndef __FM_CMD_H__
#define __FM_CMD_H__

#include <linux/types.h>
#include "fm_typedef.h"
#include "fm_patch.h"
#include "fm_link.h"

extern fm_u8 *cmd_buf;
extern struct fm_lock *cmd_buf_lock;
extern struct fm_res_ctx *fm_res;

/* FM basic-operation's opcode */
#define FM_BOP_BASE (0x80)
enum {
	FM_WRITE_BASIC_OP = (FM_BOP_BASE + 0x00),
	FM_UDELAY_BASIC_OP = (FM_BOP_BASE + 0x01),
	FM_RD_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x02),
	FM_MODIFY_BASIC_OP = (FM_BOP_BASE + 0x03),
	FM_MSLEEP_BASIC_OP = (FM_BOP_BASE + 0x04),
	FM_TOP_WRITE_BASIC_OP = (FM_BOP_BASE + 0x05),
	FM_TOP_RD_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x06),
	FM_TOP_MODIFY_BASIC_OP = (FM_BOP_BASE + 0x07),
	FM_MAX_BASIC_OP = (FM_BOP_BASE + 0x08)
};

#define PATCH_SEG_LEN 512
enum IMG_TYPE {
	IMG_WRONG = 0,
	IMG_ROM,
	IMG_PATCH,
	IMG_COEFFICIENT,
	IMG_HW_COEFFICIENT
};

/* FM BOP's size */
#define FM_TOP_WRITE_BOP_SIZE      (7)
#define FM_TOP_RD_UNTIL_BOP_SIZE     (11)
#define FM_TOP_MODIFY_BOP_SIZE     (11)

#define FM_WRITE_BASIC_OP_SIZE      (3)
#define FM_UDELAY_BASIC_OP_SIZE     (4)
#define FM_RD_UNTIL_BASIC_OP_SIZE   (5)
#define FM_MODIFY_BASIC_OP_SIZE     (5)
#define FM_MSLEEP_BASIC_OP_SIZE     (4)

fm_s32 fm_bop_write(fm_u8 addr, fm_u16 value, fm_u8 *buf, fm_s32 size);
fm_s32 fm_bop_udelay(fm_u32 value, fm_u8 *buf, fm_s32 size);
fm_s32 fm_bop_rd_until(fm_u8 addr, fm_u16 mask, fm_u16 value, fm_u8 *buf, fm_s32 size);
fm_s32 fm_bop_modify(fm_u8 addr, fm_u16 mask_and, fm_u16 mask_or, fm_u8 *buf, fm_s32 size);
fm_s32 fm_bop_top_write(fm_u16 addr, fm_u32 value, fm_u8 *buf, fm_s32 size);
fm_s32 fm_bop_top_rd_until(fm_u16 addr, fm_u32 mask, fm_u32 value, fm_u8 *buf, fm_s32 size);
fm_s32 fm_op_seq_combine_cmd(fm_u8 *buf, fm_u8 opcode, fm_s32 pkt_size);
fm_s32 fm_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr);
fm_s32 fm_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 value);
fm_s32 fm_patch_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			 const fm_u8 *src, fm_s32 seg_len);
fm_s32 fm_coeff_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			 const fm_u8 *src, fm_s32 seg_len);
fm_s32 fm_full_cqi_req(fm_u8 *buf, fm_s32 buf_size, fm_u16 *freq, fm_s32 cnt, fm_s32 type);
fm_s32 fm_top_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr);
fm_s32 fm_top_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr, fm_u32 value);
fm_s32 fm_host_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr);
fm_s32 fm_host_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr, fm_u32 value);
fm_s32 fm_set_bits_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 bits, fm_u16 mask);
fm_s32 fm_pmic_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr);
fm_s32 fm_pmic_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u32 val);
fm_s32 fm_pmic_mod_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u32 mask_and, fm_u32 mask_or);
fm_s32 fm_get_patch_path(fm_s32 ver, fm_u8 *buff, int buffsize, struct fm_patch_tbl *patch_tbl);
fm_s32 fm_get_coeff_path(fm_s32 ver, fm_u8 *buff, int buffsize, struct fm_patch_tbl *patch_tbl);
fm_s32 fm_download_patch(const fm_u8 *img, fm_s32 len, enum IMG_TYPE type);
fm_s32 fm_get_read_result(struct fm_res_ctx *result);
fm_s32 fm_reg_read(fm_u8 addr, fm_u16 *val);
fm_s32 fm_reg_write(fm_u8 addr, fm_u16 val);
fm_s32 fm_set_bits(fm_u8 addr, fm_u16 bits, fm_u16 mask);
fm_s32 fm_top_reg_read(fm_u16 addr, fm_u32 *val);
fm_s32 fm_top_reg_write(fm_u16 addr, fm_u32 val);
fm_s32 fm_host_reg_read(fm_u32 addr, fm_u32 *val);
fm_s32 fm_host_reg_write(fm_u32 addr, fm_u32 val);









/*
 * fm_get_channel_space - get the spcace of gived channel
 * @freq - value in 760~1080 or 7600~10800
 *
 * Return 0, if 760~1080; return 1, if 7600 ~ 10800, else err code < 0
 */
extern fm_s32 fm_get_channel_space(int freq);

#endif
