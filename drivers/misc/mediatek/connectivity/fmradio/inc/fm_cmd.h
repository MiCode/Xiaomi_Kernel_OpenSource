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
#include "fm_reg_utils.h"

extern unsigned char *cmd_buf;
extern struct fm_lock *cmd_buf_lock;
extern struct fm_res_ctx *fm_res;
extern unsigned char top_index;

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

signed int fm_bop_write(unsigned char addr, unsigned short value, unsigned char *buf, signed int size);
signed int fm_bop_udelay(unsigned int value, unsigned char *buf, signed int size);
signed int fm_bop_rd_until(unsigned char addr, unsigned short mask, unsigned short value, unsigned char *buf,
						signed int size);
signed int fm_bop_modify(unsigned char addr, unsigned short mask_and, unsigned short mask_or, unsigned char *buf,
						signed int size);
signed int fm_bop_top_write(unsigned short addr, unsigned int value, unsigned char *buf, signed int size);
signed int fm_bop_top_rd_until(unsigned short addr, unsigned int mask, unsigned int value, unsigned char *buf,
						signed int size);
signed int fm_op_seq_combine_cmd(unsigned char *buf, unsigned char opcode, signed int pkt_size);
signed int fm_get_reg(unsigned char *buf, signed int buf_size, unsigned char addr);
signed int fm_set_reg(unsigned char *buf, signed int buf_size, unsigned char addr, unsigned short value);
signed int fm_patch_download(unsigned char *buf, signed int buf_size, unsigned char seg_num, unsigned char seg_id,
						const unsigned char *src, signed int seg_len);
signed int fm_coeff_download(unsigned char *buf, signed int buf_size, unsigned char seg_num, unsigned char seg_id,
						const unsigned char *src, signed int seg_len);
signed int fm_full_cqi_req(unsigned char *buf, signed int buf_size, unsigned short *freq, signed int cnt,
						signed int type);
signed int fm_top_get_reg(unsigned char *buf, signed int buf_size, unsigned short addr);
signed int fm_top_set_reg(unsigned char *buf, signed int buf_size, unsigned short addr, unsigned int value);
signed int fm_host_get_reg(unsigned char *buf, signed int buf_size, unsigned int addr);
signed int fm_host_set_reg(unsigned char *buf, signed int buf_size, unsigned int addr, unsigned int value);
signed int fm_set_bits_reg(unsigned char *buf, signed int buf_size, unsigned char addr, unsigned short bits,
						unsigned short mask);
signed int fm_pmic_get_reg(unsigned char *buf, signed int buf_size, unsigned char addr);
signed int fm_pmic_set_reg(unsigned char *buf, signed int buf_size, unsigned char addr, unsigned int val);
signed int fm_pmic_mod_reg(unsigned char *buf, signed int buf_size, unsigned char addr, unsigned int mask_and,
						unsigned int mask_or);
signed int fm_get_patch_path(signed int ver, unsigned char *buff, int buffsize, struct fm_patch_tbl *patch_tbl);
signed int fm_get_coeff_path(signed int ver, unsigned char *buff, int buffsize, struct fm_patch_tbl *patch_tbl);
signed int fm_download_patch(const unsigned char *img, signed int len, enum IMG_TYPE type);
signed int fm_get_read_result(struct fm_res_ctx *result);
signed int fm_reg_read(unsigned char addr, unsigned short *val);
signed int fm_reg_write(unsigned char addr, unsigned short val);
signed int fm_set_bits(unsigned char addr, unsigned short bits, unsigned short mask);
signed int fm_top_reg_read(unsigned short addr, unsigned int *val);
signed int fm_top_reg_write(unsigned short addr, unsigned int val);
signed int fm_host_reg_read(unsigned int addr, unsigned int *val);
signed int fm_host_reg_write(unsigned int addr, unsigned int val);

/*
 * fm_get_channel_space - get the spcace of gived channel
 * @freq - value in 760~1080 or 7600~10800
 *
 * Return 0, if 760~1080; return 1, if 7600 ~ 10800, else err code < 0
 */
extern signed int fm_get_channel_space(int freq);

#endif
