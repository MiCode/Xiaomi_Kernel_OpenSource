/**************************************************************************
* File Name: OIS_user.c
* Function: User defined function.
* These functions depend on user's circumstance
*
* Rule: Use TAB 4
*
* Copyright(c)	Rohm Co.,Ltd. All rights reserved
* Copyright (C) 2018 XiaoMi, Inc.
**************************************************************************/
/***** ROHM Confidential ***************************************************/
#ifndef OIS_USER_C
#define OIS_USER_C
#endif

#include "OIS_head.h"

/* Following Variables that depend on user's environment RHM_HT 2013.03.13 add */
OIS_UWORD FOCUS_VAL = 0x0122; /* Focus Value */

#ifdef OIS_GYRO_ST
OIS_UBYTE open_flag;
/*********************************************************
* ST Gyro - Disable SPI2
* ---------------------------------------------------------
* <Function>
* To Disable SPI2 of ST gyro.
* <Input>
* none
*
* <Output>
* none
*
*********************************************************/
void Disable_SPI2_ST(void)
{
	OIS_UWORD u16_data = 0;
	OIS_UBYTE loop = 10;

	if (!open_flag) {
		goto OUT;
	} else {
		I2C_OIS_mem_write(0x7F, 0x0080);  /*Stop SPI Communication */

		I2C_OIS_per_write(0x18, 0x400F);  /*To Read CTRL1_OIS(0x70) register in LSM6DSM */
		I2C_OIS_per_write(0x1B, 0xF000);
		I2C_OIS_per_write(0x1C, 0xF000);

		u16_data = I2C_OIS_per__read(0x1C);  /*To Read P_0x1C and check if LSB of data is 0xB1 */
		u16_data &= 0x00FF;

		if (u16_data == 0xB1) {
			while (loop) {
				I2C_OIS_per_write(0x18, 0x000F);  /* To Write OIS_EN_SPI2 = 0 in CTRL1_OIS register */
				I2C_OIS_per_write(0x1B, 0x70B0);
				I2C_OIS_per_write(0x1C, 0x70B0);

				I2C_OIS_per_write(0x18, 0x400F);  /*To Check OIS_EN_SPI2 = 0, Read CTRL1_OIS(0x70) register; */
				I2C_OIS_per_write(0x1B, 0xF000);
				I2C_OIS_per_write(0x1C, 0xF000);

				u16_data = I2C_OIS_per__read(0x1C);	/* Read P_0x1C and check if LSB of data is 0x00. */
				u16_data &= 0x00FF;

				if (u16_data == 0x00) {
					pr_debug("%s:%d:loop = %d\n", __func__, __LINE__, loop);
					break;
				}
				if (!loop) {
					pr_debug("%s:%d:loop = %d\n", __func__, __LINE__, loop);
					break;
				}
				loop--;
				msleep(10);
			}
		}
	}

OUT:
	return;
}
#endif
/*********************************************************
* VCOSET function
* ---------------------------------------------------------
* <Function>
* To use external clock at CLK/PS, it need to set PLL.
* After enabling PLL, more than 30ms wait time is required to change clock source.
* So the below sequence has to be used:
* Input CLK/PS --> Call VCOSET0 --> Download Program/Coed --> Call VCOSET1
*
* <Input>  none
*
* <Output> none
********************************************************/
void VCOSET0(void)
{
	OIS_UWORD CLK_PS = 24000; /* X5=24MHz //Input Frequency [kHz] of CLK/PS terminal (Depend on your system) */
	OIS_UWORD FVCO_1 = 36000; /* Target Frequency [kHz] */
	OIS_UWORD FREF   = 25;    /* Reference Clock Frequency [kHz] */
	OIS_UWORD DIV_N  = CLK_PS / FREF - 1; /* calc DIV_N */
	OIS_UWORD DIV_M  = FVCO_1 / FREF - 1; /* calc DIV_M */

	I2C_OIS_per_write(0x62, DIV_N); /* Divider for internal reference clock */
	I2C_OIS_per_write(0x63, DIV_M); /* Divider for internal PLL clock */
	I2C_OIS_per_write(0x64, 0x4060); /* Loop Filter */
	I2C_OIS_per_write(0x60, 0x3011); /* PLL */
	I2C_OIS_per_write(0x65, 0x0080);
	I2C_OIS_per_write(0x61, 0x8002); /* VCOON */
	I2C_OIS_per_write(0x61, 0x8003); /* Circuit ON */
	I2C_OIS_per_write(0x61, 0x8809); /* PLL ON */
}

void VCOSET1(void)
{
	I2C_OIS_per_write(0x05, 0x000C); /* Prepare for PLL clock as master clock */
	I2C_OIS_per_write(0x05, 0x000D); /* Change to PLL clock */
}

struct msm_ois_ctrl_t *g_i2c_ctrl;

void WR_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	OIS_UWORD addr = ((dat[0] << 8) | dat[1]);
	OIS_UBYTE *data_wr = dat + 2;

	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(&g_i2c_ctrl->i2c_client, addr, data_wr, size - 2);
	pr_debug("WR_I2C addr:0x%x data:0x%x", addr, data_wr[0]);
}

OIS_UWORD RD_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	OIS_UWORD read_data = 0;
	OIS_UWORD addr = ((dat[0] << 8) | dat[1]);

	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_read(&g_i2c_ctrl->i2c_client, addr, &read_data, MSM_CAMERA_I2C_WORD_DATA);

	pr_debug("RD_I2C addr:0x%x data:0x%x", addr, read_data);

	return read_data;
}

/*********************************************************
* Write Factory Adjusted data to the non-volatile memory
* ---------------------------------------------------------
* <Function>
*    Factory adjusted data are sotred somewhere
*    non-volatile memory.
*
* <Input>
*    _FACT_ADJ Factory Adjusted data
*
* <Output>
*    none
*
* <Description>
*    You have to port your own system.
*
**********************************************************/
void store_FADJ_MEM_to_non_volatile_memory(_FACT_ADJ param)
{
	/* Write to the non-vollatile memory such as EEPROM or internal of the CMOS sensor... */
}

/*****************************************************
**** Digital Gyro Adjust
*****************************************************/
int g_fadj_gyro_kd = 1;
void fadj_ois_gyro_offset_calibraion(void)
{
	OIS_UWORD u16_avrN = 32; /* Averaging number */
	OIS_LONG s32_dat1;
	OIS_LONG s32_dat2;
	OIS_UWORD u16_i;
	OIS_UWORD u16_tmp_read1;
	OIS_UWORD u16_tmp_read2;
	OIS_UWORD sid;

	if (g_fadj_gyro_kd) {
		return;
	}

	s32_dat1 = 0;
	s32_dat2 = 0;
	for (u16_i = 1; u16_i <= u16_avrN; u16_i += 1) {
		usleep_range(5000, 6000);
		u16_tmp_read1 = I2C_OIS_mem__read(_M_DigGx);
		u16_tmp_read2 = I2C_OIS_mem__read(_M_DigGy);
		s32_dat1 += u16_tmp_read1;
		s32_dat2 += u16_tmp_read2;
		pr_info("%02d,g 0x%04x 0x%04x -> %d,%d", u16_i, u16_tmp_read1, u16_tmp_read2, (int16_t)u16_tmp_read1, (int16_t)u16_tmp_read2);
	}
	u16_tmp_read1 = s32_dat1 / u16_avrN;
	u16_tmp_read2 = s32_dat2 / u16_avrN;

	pr_info("gx:0x%04x gy:0x%04x -> %d,%d", u16_tmp_read1, u16_tmp_read2, (int16_t)u16_tmp_read1, (int16_t)u16_tmp_read2);
	g_fadj_gyro_kd = 1;

	/* For Gyro EV IDG-2030 */
	if ((((int16_t)u16_tmp_read1 > 1444) || ((int16_t)u16_tmp_read1 < -1444)) || (((int16_t)u16_tmp_read2 > 1444) || ((int16_t)u16_tmp_read2 < -1444)))
		return;/* ileagal calibration value */

	FADJ_MEM.gl_GX_OFS = u16_tmp_read1;
	FADJ_MEM.gl_GY_OFS = u16_tmp_read2;

	/* Write back to EEPROM to save it */
	sid = g_i2c_ctrl->i2c_client.cci_client->sid;
	g_i2c_ctrl->i2c_client.cci_client->sid = 0x50;
	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(&g_i2c_ctrl->i2c_client, 0x3E, FADJ_MEM.gl_GX_OFS, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(&g_i2c_ctrl->i2c_client, 0x3F, FADJ_MEM.gl_GX_OFS >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(&g_i2c_ctrl->i2c_client, 0x40, FADJ_MEM.gl_GY_OFS, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(&g_i2c_ctrl->i2c_client, 0x41, FADJ_MEM.gl_GY_OFS >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	g_i2c_ctrl->i2c_client.cci_client->sid = sid;

	return;
}

/*********************************************************
* Read Factory Adjusted data from the non-volatile memory
 ---------------------------------------------------------
* <Function>
*    Factory adjusted data are sotred somewhere
*    non-volatile memory.  I2C master has to read these
*    data and store the data to the OIS controller.
*
* <Input>
*    none
*
* <Output>
*    _FACT_ADT Factory Adjusted data
*
* <Description>
*    You have to port your own system.
*
**********************************************************/
static int fadj_got;
extern uint8_t g_cal_fadj_data[128];

void get_FADJ_MEM_from_non_volatile_memory(void)
{
	/* Read from the non-vollatile memory such as EEPROM or internal of the CMOS sensor... */
	/* OIS_UWORD sid; */
	OIS_UBYTE *data = (OIS_UBYTE *)(&FADJ_MEM);
	OIS_UBYTE buf[128];
	/* char i = 0; */
	/* if (fadj_got) return; */

	memcpy((void *)buf, (void *)g_cal_fadj_data, 43);
	memcpy((void *) data, (void *) (&buf[0x05]), 38);
	/* g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(&g_i2c_ctrl->i2c_client, 0x3A, data, 38); */
	pr_debug("ois fadj 0x%04x 0x%04x 0x%04x\n", FADJ_MEM.gl_CURDAT, FADJ_MEM.gl_HALOFS_X, FADJ_MEM.gl_HALOFS_Y);
	/* g_i2c_ctrl->i2c_client.cci_client->sid = sid; */

	fadj_got = 1;

	return;
}

/* ==> RHM_HT 2013/04/15 Add for DEBUG */
/*********************************************************
* Printf for DEBUG
* ---------------------------------------------------------
* <Function>
*
* <Input>
*    const char *format, ...
*    Same as printf
*
* <Output>
*    none
*
* <Description>
*
**********************************************************/
int debug_print(const char *format, ...)
{
	return 0;/* Darcy deleted/20140620 */
}
