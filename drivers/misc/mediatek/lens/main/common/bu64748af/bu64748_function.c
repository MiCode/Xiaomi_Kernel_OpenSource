/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "bu64748_function.h"
#include "OIS_coef.h"
#include "OIS_prog.h"
#include <linux/delay.h>
#include <linux/fs.h>

static void I2C_func_PER_WRITE(unsigned char u08_adr,
			unsigned short u16_dat)
{
	unsigned char out[4] = {0};

	out[0] = _OP_Periphe_RW;
	out[1] = u08_adr;
	out[2] = u16_dat & 0xFF;
	out[3] = (u16_dat >> 8) & 0xFF;

	main_SOutEx(_SLV_FBAF_, out, 4);
}

static unsigned short I2C_func_PER_READ(unsigned char u08_adr)
{
	unsigned char in[2] = {0};
	unsigned char read[2] = {0};
	unsigned short u16_dat;

	in[0] = _OP_Periphe_RW;
	in[1] = u08_adr;

	main_SInEx(_SLV_FBAF_, in, 2, read, 2);

	u16_dat = (read[0] * 256) + read[1];
	return u16_dat;
}

static void I2C_func_MEM_WRITE(unsigned char u08_adr,
			unsigned short u16_dat)
{
	unsigned char out[4];

	out[0] = _OP_Memory__RW;
	out[1] = u08_adr;
	out[2] = u16_dat & 0xFF;
	out[3] = (u16_dat >> 8 & 0xFF);

	main_SOutEx(_SLV_FBAF_, out, 4);
}

static unsigned short I2C_func_MEM_READ(unsigned char u08_adr)
{
	unsigned char in[2] = {0};
	unsigned char read[2] = {0};
	unsigned short u16_dat;

	in[0] = _OP_Memory__RW;
	in[1] = u08_adr;

	main_SInEx(_SLV_FBAF_, in, 2, read, 2);

	u16_dat = (read[0] * 256) + read[1];
	return u16_dat;
}

static void I2C_func_PON______(void)
{
	I2C_func_PER_WRITE(0xEF, 0x0080);
}

static void I2C_func_POFF_____(void)
{
	I2C_func_PER_WRITE(0xEF, 0x0000);
}

static void func_CHK_VERSION(void)
{
	unsigned short u16_dat = 0;

	u16_dat = I2C_func_PER_READ(0x5F);
	pr_debug("[bu64748af]IC Version : 0x%x.\n", u16_dat);
}

static void Set_Close_Mode(void)
{
	I2C_func_MEM_WRITE(_M_30_EQCTL, 0x000D);
}

static void download(int type)
{
/* Data Transfer Size per one I2C access */
#define DWNLD_TRNS_SIZE (32)

	unsigned char temp[DWNLD_TRNS_SIZE + 1];
	int block_cnt;
	int total_cnt;
	int lp;
	int n;
	int u16_i;

	if (type == 0)
		n = MAIN_DOWNLOAD_BIN_LEN;
	else
		n = MAIN_DOWNLOAD_COEF_LEN; /* RHM_HT 2013/07/10    Modified */

	block_cnt = n / DWNLD_TRNS_SIZE + 1;
	total_cnt = block_cnt;

	while (1) {
		/* Residual Number Check */
		if (block_cnt == 1)
			lp = n % DWNLD_TRNS_SIZE;
		else
			lp = DWNLD_TRNS_SIZE;

		/* Transfer Data set */
		if (lp != 0) {
			if (type == 0) {
				temp[0] = _OP_FIRM_DWNLD;
				for (u16_i = 1; u16_i <= lp; u16_i += 1)
					temp[u16_i] = MAIN_DOWNLOAD_BIN
						[(total_cnt - block_cnt) *
							 DWNLD_TRNS_SIZE +
						 u16_i - 1];
			} else {
				temp[0] = _OP_COEF_DWNLD;
				for (u16_i = 1; u16_i <= lp; u16_i += 1)
					temp[u16_i] = MAIN_DOWNLOAD_COEF
						[(total_cnt - block_cnt) *
							 DWNLD_TRNS_SIZE +
						 u16_i - 1];
			}

			/* Data Transfer */
			/* WR_I2C(_SLV_OIS_, lp + 1, temp); */
			main_SOutEx(_SLV_FBAF_, temp, lp + 1);
		}

		/* Block Counter Decrement */
		block_cnt = block_cnt - 1;

		if (block_cnt == 0)
			break;
	}
}

static int func_PROGRAM_DOWNLOAD(void)
{
	int sts = ADJ_OK;
	int ver_check = 0;
	unsigned short u16_dat;

	download(0);

	ver_check = I2C_func_MEM_READ(_M_F7_FBAF_STS);
	pr_debug("[bu64748af]ver_check : 0x%x\n", ver_check);

	if ((ver_check & 0x0004) == 0x0004) {
		u16_dat = I2C_func_MEM_READ(_M_FIRMVER);

		pr_debug("[bu64748af]FW Ver : %d\n", u16_dat);
		pr_debug("[bu64748af]FW Download OK.\n");
	} else {
		pr_debug("[bu64748af]FW Download NG.\n");
		return PROG_DL_ERR;
	}
	return sts;
}

static int func_COEF_DOWNLOAD(void)
{
	int sts = ADJ_OK;
	unsigned short u16_dat;

	download(1);

	u16_dat = I2C_func_MEM_READ(_M_CD_CEFTYP);

	pr_debug("[bu64748af]COEF Ver : %d\n", u16_dat);
	pr_debug("[bu64748af]COEF Download OK.\n");
	return sts;
}

static void I2C_func_DSP_START(void)
{
	unsigned char out[2] = {0};

	out[0] = _OP_SpecialCMD;
	out[1] = _cmd_8C_EI;

	main_SOutEx(_SLV_FBAF_, out, 2);
}

void main_AF_TARGET(unsigned short target)
{
	unsigned char out[3] = {0};

	out[0] = 0xF2;
	out[1] = (target >> 8) & 0xFF;
	out[2] = target & 0xFF;

	main_SOutEx(_SLV_FBAF_, out, 3);
}

int BU64748_main_Initial(void)
{
	int str = ADJ_OK;

	I2C_func_POFF_____();
	I2C_func_PON______();
	func_CHK_VERSION();

	str = func_PROGRAM_DOWNLOAD();

	if (str != ADJ_OK)
		return str;

	str = func_COEF_DOWNLOAD();

	if (str != ADJ_OK)
		return str;

	I2C_func_DSP_START();

	main_AF_TARGET(0x200);
	Set_Close_Mode();

	return str;
}

unsigned short bu64748_main_af_cur_pos(void)
{
	unsigned char in[2] = {0};
	unsigned char read[2] = {0};
	unsigned short u16_dat;

	in[0] = 0x84;
	in[1] = 0x23;

	main_SInEx(_SLV_FBAF_, in, 2, read, 2);

	u16_dat = (read[0] * 256) + read[1];
	return u16_dat;
}

void BU64748_main_soft_power_ctrl(int On)
{
	if (On) {
		I2C_func_MEM_WRITE(0x59, 0x000C);
		I2C_func_MEM_WRITE(0x3D, 0x0080);
		I2C_func_MEM_WRITE(0x72, 0x1111);
		I2C_func_MEM_WRITE(0x30, 0x000D);
	} else {
		I2C_func_MEM_WRITE(0x30, 0x0000);
		I2C_func_MEM_WRITE(0x59, 0x0000);
		I2C_func_MEM_WRITE(0x3D, 0x0000);
		I2C_func_MEM_WRITE(0x72, 0x0000);
	}
}
