/*
 * File: miniisp_top.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2017/04/11; LouisWang; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



 /******Include File******/
 /* Linux headers*/
#include <linux/delay.h>
#include <linux/buffer_head.h>
#include <linux/namei.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include "include/miniisp.h"
#include "include/miniisp_ctrl.h"
#include "include/miniisp_customer_define.h"
#include "include/miniisp_chip_base_define.h"

#include "include/error/miniisp_err.h"

/****************************************************************************
*                        Private Constant Definition                        *
****************************************************************************/
#define EN_SPIE_DEBUG 0
#define MINI_ISP_LOG_TAG "[miniisp_top]"
#define ALIGN_CEIL(x, N)    (((x) + ((N) - 1)) / (N) * (N))
static bool irqflag;
static u32 event = MINI_ISP_RCV_WAITING;
static u32 current_event = MINI_ISP_RCV_WAITING;
static DECLARE_WAIT_QUEUE_HEAD(WAITQ);
/**********************************************************************
*                         Public Function                             *
**********************************************************************/
int mini_isp_get_chip_id(void)
{
	int status = ERR_SUCCESS;
	u32 buff_id = 0;

	misp_info("%s - enter", __func__);

	mini_isp_register_read(CHIP_ID_ADDR, &buff_id);

	misp_err("[miniISP]Get Chip ID 0x%x", buff_id);

	return status;
}


void mini_isp_register_write(u32 reg_addr, u32 reg_new_value)
{
	u8 *send_buffer;
	u8 ctrlbyte = CTRL_BYTE_REGWR;
	u32 address = reg_addr, value = reg_new_value;
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;

	/* misp_info("%s - enter, reg_addr[%#08x], write_value[%#08x]",
		__func__, reg_addr, reg_new_value); */

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);

	send_buffer = devdata->tx_buf;

	memcpy(send_buffer, &ctrlbyte, 1);
	memcpy(send_buffer + 1, &address, 4);
	memcpy(send_buffer + 5, &value, 4);
	/* dev_global_variable->before_booting = 1; */
	devdata->intf_fn->write(devdata, devdata->tx_buf, devdata->rx_buf, 9);
	/* dev_global_variable->before_booting = 0; */
}

void mini_isp_register_write_one_bit(u32 reg_addr,
			u8 bit_offset, u8 bit_value)
{
	u8 *send_buffer;
	u32 reg_value;
	u8 ctrlbyte = CTRL_BYTE_REGWR;
	u32 address = reg_addr;
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;

	misp_info("%s - enter", __func__);

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = devdata->tx_buf;

	mini_isp_register_read(reg_addr, &reg_value);

	if (bit_value)
		reg_value |= 1UL << bit_offset;
	else
		reg_value &= ~(1UL << bit_offset);

	memcpy(send_buffer, &ctrlbyte, 1);
	memcpy(send_buffer + 1, &address, 4);
	memcpy(send_buffer + 5, &reg_value, 4);
	/* dev_global_variable->before_booting = 1; */
	devdata->intf_fn->write(devdata, devdata->tx_buf, devdata->rx_buf, 9);
	/* dev_global_variable->before_booting = 0; */
}


void mini_isp_register_write_bit_field(u32 reg_addr, u32 mask, u32 mask_value)
{
	u8 *send_buffer;
	u32 reg_value;
	u8 ctrlbyte = CTRL_BYTE_REGWR;
	u32 address = reg_addr, value;
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;

	misp_info("%s - enter", __func__);

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = devdata->tx_buf;

	mini_isp_register_read(reg_addr, &reg_value);
	value = (reg_value & ~mask) | mask_value;

	memcpy(send_buffer, &ctrlbyte, 1);
	memcpy(send_buffer + 1, &address, 4);
	memcpy(send_buffer + 5, &value, 4);
	/* dev_global_variable->before_booting = 1; */
	devdata->intf_fn->write(devdata, devdata->tx_buf, devdata->rx_buf, 9);
	/* dev_global_variable->before_booting = 0; */
}

void mini_isp_e_to_a(void)
{
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	misp_info("mini_isp_drv_setting(1) mini_isp_e_to_a");
	mini_isp_register_write(0xffef0240, 0x0);
	/*mdelay(1);*/
	msleep(20);
	dev_global_variable->altek_spi_mode = ALTEK_SPI_MODE_A;
}


void mini_isp_a_to_e(void)
{
	u8 *send_buffer;
	u8 ctrlbyte = CTRL_BYTE_A2E;
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = devdata->tx_buf;

	memcpy(send_buffer, &ctrlbyte, 1);
	devdata->intf_fn->write(devdata, devdata->tx_buf, devdata->rx_buf, 1);
	dev_global_variable->altek_spi_mode = ALTEK_SPI_MODE_E;
	misp_info("mini_isp_drv_setting(2) mini_isp_a_to_e");
}

void mini_isp_chip_init(void)
{
	misp_info("%s - enter", __func__);
	mini_isp_register_write(0xffe40050, 0x1);
	mini_isp_register_write(0xffef00a4, 0xe);
	udelay(70);
	mini_isp_register_write(0xffef00a0, 0xe);
	mini_isp_register_write(0xffe81080, 0x8);
	mini_isp_register_write(0xffef0090, 0x30079241);
	mini_isp_register_write(0xffe800c4, 0x0);
	udelay(100);
	misp_info("%s - leave", __func__);
}

void mini_isp_cp_mode_suspend_flow(void)
{
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	misp_info("%s - enter", __func__);

	/*# AP disable OCRAM0*/
	mini_isp_register_write_one_bit(0xffe609f4, 0, 1);/* # ocr0_disable*/

	/*# 2.AP reset ARC5*/
	mini_isp_register_write_one_bit(0xffe800c4, 1, 1);/* # base_ck_in*/

	/*# 3.AP reset modules(including arbiter bus)*/
	mini_isp_register_write_one_bit(0xffe801a4, 1, 1);/* # standby_top*/
	mini_isp_register_write_one_bit(0xffe80104, 1, 1);/* # arb_bus_stdby_0*/

	/*# 4.AP stop clock of modules(including arbiter bus, OCRAM0) and ARC5*/
	mini_isp_register_write_one_bit(0xffe800c4, 0, 1);/* # base_ck_in*/
	mini_isp_register_write_one_bit(0xffe801a4, 0, 1);/* # standby_top*/
	mini_isp_register_write_one_bit(0xffe80104, 0, 1);/* # arb_bus_stdby_0*/
	mini_isp_register_write_one_bit(0xffe80104, 2, 1);/* # ocram_0*/

	/*# 5.AP isolate standby power domain*/
	mini_isp_register_write_one_bit(0xffef00a0, 0, 1);/* # iso_pd_standby*/

	/*# 6.AP power down standby power domain*/
	mini_isp_register_write_one_bit(0xffef00a4, 0, 1);/* # psw_pd_standby*/

	/*# AP issue global reset of standby power domain*/
	/* # pd_rstn_standby_top*/
	mini_isp_register_write_one_bit(0xffe81080, 3, 0);
	/*# 7.AP keep PLL factor then disable PLLs (Keep PLL_fix)
	 *# ClkGen have kept PLL factor so don't keep here
	 *#pll_var_ext_dat = []
	 *# address of PLL factor
	 *#pll_var_ext_addr = [0xffe81120, 0xffe81124, 0xffe81128,
	 *#                    0xffe8112c, 0xffe81130, 0xffe81134,
	 *#                    0xffe81140, 0xffe81144, 0xffe81148,
	 *#                    0xffe8114c, 0xffe81150, 0xffe81154 ]
	 *#for addr in pll_var_ext_addr:
	 *#    (retsts, retdat) = altek_get_register(interface,
	 *#                        handle, I2C_SLAVE_ID, addr)
	 *#    pll_var_ext_dat.append(retdat)
	 *# bit 11: reset_pll_ext,
	 *# bit 10: reset_pll_var,
	 *# bit 3: disable_pll_ext,
	 *# bit 2: disable_pll_var
	 */
	/*# 7.AP keep PLL factor then disable PLLs
	 *(disable PLL_fix, PLL_ext and PLL_var)
	 */
	mini_isp_register_write_bit_field(0xffe81004, 0x0e0e, 0x0e0e);
	/*# AP do something*/
	/*mdelay(10);*/
	msleep(20);
	misp_info("%s - leave", __func__);
}

void mini_isp_cp_mode_resume_flow(void)
{
	u32 magic_number;
	struct misp_global_variable *dev_global_variable;
	errcode status = ERR_SUCCESS;

	misp_info("%s - enter", __func__);
	dev_global_variable = get_mini_isp_global_variable();
	/*# 1.AP check magic number*/
	mini_isp_register_read(0xffef0008, &magic_number);/* # magic_number*/

	/*
	 *# 2.AP check if magic number is equal to RESUME (0x19961224)
	 *then jump to step 4
	 */
	if (magic_number != 0x19961224) {
		/*
		 *# 3.Else exit resume flow
		 *(Note: AP can decide what to do.
		 *Ex: Toggle system reset of SK1 to reboot SK1)
		 */
		misp_err("%s-resume fail!,magic number not equal!", __func__);
		return;
	}

	/*# 4.AP power up standby power domain*/
	mini_isp_register_write_one_bit(0xffef00a4, 0, 0);/* # psw_pd_standby*/
	udelay(70);/* # 70us, TO-DO: depend on backend spec*/

	/*# 5.AP release isolation of standby power domain*/
	mini_isp_register_write_one_bit(0xffef00a0, 0, 0);/* # iso_pd_standby*/

	/*# AP release global reset of standby power domain*/
	/*# pd_rstn_standby_top*/
	mini_isp_register_write_one_bit(0xffe81080, 3, 1);

	/*# AP power up SRAM of ARC5*/
	mini_isp_register_write_one_bit(0xffef0090, 22, 0);/*# srampd_base_arc*/

	/*
	 *# 6.AP enable clock of modules(including arbiter bus, OCRAM0)
	 *and ARC5
	 */
	mini_isp_register_write_one_bit(0xffe800c4, 0, 0);/* # base_ck_in*/
	mini_isp_register_write_one_bit(0xffe801a4, 0, 0);/* # standby_top*/
	mini_isp_register_write_one_bit(0xffe80104, 0, 0);/* # arb_bus_stdby_0*/
	mini_isp_register_write_one_bit(0xffe80104, 2, 0);/* # ocram_0*/

	/*# 7.AP release reset of modules(including arbiter bus)*/
	mini_isp_register_write_one_bit(0xffe801a4, 1, 0);/* # standby_top*/
	mini_isp_register_write_one_bit(0xffe80104, 1, 0);/* # arb_bus_stdby_0*/

	/*# AP restore OCRAM0 setting*/
	mini_isp_register_write(0xffe609f8, 0xdbfc0);
	mini_isp_register_write(0xffe609f4, 0);
	mini_isp_register_write(0xffe60800, 0);
	mini_isp_register_write(0xffe60900, 0);

	/*# 8. AP release reset of ARC5*/
	mini_isp_register_write_one_bit(0xffe800c4, 1, 0);/* # base_ck_in*/

	/*# 9. AP wait interrupt then clean interrupt status*/
	status = mini_isp_wait_for_event(MINI_ISP_RCV_CPCHANGE);
	misp_info("%s - leave", __func__);
}

void mini_isp_check_and_leave_bypass_mode(void)
{
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	if (dev_global_variable->be_set_to_bypass) {
		/*Add code here*/
		mini_isp_register_write(0xffe80b04, 0x2a2a);/*mipidphy exc*/
		mini_isp_register_write(0xffe80944, 0x2);/*mipi tx phy 1*/
		mini_isp_register_write(0xffe80884, 0x2);/*mipi tx phy 0*/
		mini_isp_register_write(0xffe804e4, 0x2);/*ppibridge 1 exc*/
		mini_isp_register_write(0xffe804c4, 0x2);/*ppibridge 1*/
		mini_isp_register_write(0xffe804a4, 0x2);/*ppibridge 0 exc*/
		mini_isp_register_write(0xffe80484, 0x2);/*ppibridge 0*/
		mini_isp_register_write(0xffe80444, 0xa);/*mipi rx phy 1*/
		mini_isp_register_write(0xffe80404, 0xa);/*mipi rx phy 0*/

		dev_global_variable->be_set_to_bypass = 0;
	} else {
		/*do nothing*/
		misp_info("not running bypass mode yet");
	}
}


#if ENABLE_LINUX_FW_LOADER
int mini_isp_pure_bypass(u16 mini_isp_mode)
{
	struct misp_global_variable *dev_global_variable;
	errcode err = ERR_SUCCESS;
	u32 idx = 0;
	u32 CurBufPos = 0;
	u32 reg_addr;
	u32 reg_new_value;
	u32 sleepTime;
	u32 file_total_size;
	u8 byass_setting_file_location[80];
	u8 linebuf[64];
	u32 copylens;
	const struct firmware *fw = NULL;
	const u8 *fw_data;
	struct device *mini_isp_device;
	char *file_name = NULL;

	dev_global_variable = get_mini_isp_global_variable();
	snprintf(byass_setting_file_location, 64,
		"altek_bypass_setting_%d.log", mini_isp_mode);
	misp_info("altek bypass mode %d", mini_isp_mode);

	/* load boot fw file */
	mini_isp_device = mini_isp_getdev();
	if (mini_isp_device != NULL && byass_setting_file_location != NULL) {
		file_name = byass_setting_file_location;
		misp_info("%s, fw name: %s", __func__, file_name);

		err = request_firmware(&fw,
			file_name, mini_isp_device);

		if (err) {
			misp_info("%s, L: %d, err: %d",
				__func__, __LINE__, err);
			goto EXIT;
		}
	}

	if (fw == NULL) {
		misp_info("%s, fw:%s is NULL.", __func__, file_name);
		return -EINVAL;
	}

	file_total_size = fw->size;
	fw_data = fw->data;

#ifndef AL6100_SPI_NOR
	mini_isp_chip_init();
#endif
	misp_info("%s  file_total_size = %d", __func__, file_total_size);
	CurBufPos = idx = 0;
	while (CurBufPos < file_total_size) {
		/* Get line form fw_data */
		for (idx = CurBufPos; idx < file_total_size; idx++) {
			if (fw_data[idx] == '\n') {
				copylens = (idx-CurBufPos > 64) ?
					64 : (idx-CurBufPos);
				memcpy(linebuf, &(fw_data[CurBufPos]),
					copylens);
				break;
			} else if (idx == file_total_size - 1) {
				/* buf tail */
				copylens = (idx-CurBufPos > 64) ?
					64 : (idx-CurBufPos);
				memcpy(linebuf, &(fw_data[CurBufPos]),
					copylens);
				break;
			}
		}

		/* analyze line buffer */
		if (sscanf(linebuf, "ml 0x%x 0x%x",
			&reg_addr, &reg_new_value) == 2) {

			misp_info("ml 0x%x 0x%x", reg_addr, reg_new_value);
			mini_isp_register_write(reg_addr, reg_new_value);
		} else if (sscanf(linebuf, "sl %u", &sleepTime) == 1) {
			misp_info("sl %d", sleepTime);
			msleep(sleepTime);
		} else {
			misp_info("file format error! CurPos:%d", CurBufPos);
			break;
		}

		/* skip '\n' char */
		CurBufPos = idx + 1;
	}

	/*Restore segment descriptor*/
	misp_info("miniisp bypass setting send finish");
	dev_global_variable->be_set_to_bypass = 1;
EXIT:
	if (fw != NULL)
		release_firmware(fw);

	return err;
}

#else
int mini_isp_pure_bypass(u16 mini_isp_mode)
{
	struct misp_global_variable *dev_global_variable;
	errcode err = ERR_SUCCESS;
	mm_segment_t oldfs;
	struct file *file_filp;
	off_t currpos;
	loff_t offset;
	char  *allocated_memmory;
	u8  *keep_allocated_memmory;
	char  allocated_memmory_buf[64];
	u32 reg_addr;
	u32 reg_new_value;
	u32 file_total_size;
	u8 byass_setting_file_location[80];
	u8 buf[8];
	int i;

	dev_global_variable = get_mini_isp_global_variable();
	snprintf(byass_setting_file_location, 64,
		"%saltek_bypass_setting_%d.log",
		MINIISP_BYPASS_SETTING_FILE_PATH, mini_isp_mode);
	misp_info("altek bypass mode %d", mini_isp_mode);
	misp_info("%s setting filepath : %s", __func__,
		byass_setting_file_location);

	allocated_memmory = allocated_memmory_buf;
	keep_allocated_memmory = allocated_memmory;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	set_fs(oldfs);
	return 0;
#endif
	if (IS_ERR(file_filp)) {
		err = PTR_ERR(file_filp);
		misp_err("%s open file failed. err: %d", __func__, err);
		set_fs(oldfs);
		return err;
	}
	misp_info("%s open file success!", __func__);

#ifndef AL6100_SPI_NOR
	mini_isp_chip_init();
#endif
	/*get bin filie size*/
	currpos = vfs_llseek(file_filp, 0L, SEEK_END);
	file_total_size = currpos;
	currpos = vfs_llseek(file_filp, 0L, SEEK_SET);

	misp_info("%s  file_total_size = %d", __func__, file_total_size);
	offset = file_filp->f_pos;
	while (file_total_size > 0) {

		vfs_read(file_filp, (char *)allocated_memmory_buf,
								1, &offset);

		file_filp->f_pos = offset;
		file_total_size--;
		if (allocated_memmory_buf[0] == '0') {
			vfs_read(file_filp, (char *)allocated_memmory,
				1, &offset);
			file_filp->f_pos = offset;
			file_total_size--;
			if (allocated_memmory_buf[0] == 'x') {
				vfs_read(file_filp, (char *)allocated_memmory,
					8, &offset);
				file_filp->f_pos = offset;
				file_total_size = file_total_size - 8;

				for (i = 0; i < 4; i++)
					err = hex2bin(buf+3-i,
						allocated_memmory+2*i, 1);

				while (1) {
					vfs_read(file_filp,
						(char *)allocated_memmory,
						1, &offset);
					file_filp->f_pos = offset;
					file_total_size = file_total_size - 1;

					if (allocated_memmory[0] == '0')
						break;
				}

				if (file_total_size < 0)
					break;

				vfs_read(file_filp, (char *)allocated_memmory,
							1, &offset);
				file_filp->f_pos = offset;
				file_total_size = file_total_size - 1;
				if ((allocated_memmory[0] == 'x')) {
					vfs_read(file_filp,
						(char *)allocated_memmory,
						8, &offset);
					file_filp->f_pos = offset;
					file_total_size = file_total_size - 8;

					for (i = 0; i < 4; i++)
						err = hex2bin(buf+4+3-i,
							allocated_memmory+2*i,
							1);

					memcpy(&reg_addr, buf, 4);
					memcpy(&reg_new_value, buf+4, 4);

					mini_isp_register_write(
						reg_addr,
						reg_new_value);

				}
			}
		} else if (allocated_memmory_buf[0] == 's') {
			while (1) {
				vfs_read(file_filp, (char *)allocated_memmory,
							1, &offset);
				file_filp->f_pos = offset;
				file_total_size = file_total_size - 1;

				if (allocated_memmory[0] == 13) {
					udelay(350);
					break;
				}
			}
		}
	}
	/*Restore segment descriptor*/
	misp_info("miniisp bypass setting send finish");

	dev_global_variable->be_set_to_bypass = 1;
	set_fs(oldfs);
	filp_close(file_filp, NULL);

	return err;
}
#endif

void mini_isp_pure_bypass_debug(u16 mini_isp_mode)
{
	mini_isp_chip_init();
	misp_info("mini_isp_pure_bypass_debug(%d) set bypass mode",
		mini_isp_mode);
	switch (mini_isp_mode) {
	case 1:
		mini_isp_register_write(0xffe40000, 0x00000008);
		mini_isp_register_write(0xffe40004, 0x00006621);
		mini_isp_register_write(0xffe40008, 0x00006621);
		mini_isp_register_write(0xffe4000c, 0x00006621);
		mini_isp_register_write(0xffe40010, 0x00006621);
		mini_isp_register_write(0xffe40050, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000200);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81100, 0x00000000);
		mini_isp_register_write(0xffe81104, 0x00000000);
		mini_isp_register_write(0xffe81108, 0x000000dc);
		mini_isp_register_write(0xffe8110c, 0x00000000);
		mini_isp_register_write(0xffe81110, 0x00000001);
		mini_isp_register_write(0xffe81114, 0x00000000);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe800c0, 0x0000000a);
		mini_isp_register_write(0xffe800e0, 0x0000000a);
		mini_isp_register_write(0xffe80100, 0x0000000a);
		mini_isp_register_write(0xffe80120, 0x0000000a);
		mini_isp_register_write(0xffe81004, 0x00000800);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81120, 0x00000000);
		mini_isp_register_write(0xffe81124, 0x00000000);
		mini_isp_register_write(0xffe81128, 0x0000017a);
		mini_isp_register_write(0xffe8112c, 0x00000000);
		mini_isp_register_write(0xffe81130, 0x00000001);
		mini_isp_register_write(0xffe81134, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe81004, 0x00000400);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81140, 0x00000000);
		mini_isp_register_write(0xffe81144, 0x00000000);
		mini_isp_register_write(0xffe81148, 0x0000017a);
		mini_isp_register_write(0xffe8114c, 0x00000000);
		mini_isp_register_write(0xffe81150, 0x00000001);
		mini_isp_register_write(0xffe81154, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe80b00, 0x00000819);
		mini_isp_register_write(0xffe80880, 0x00000400);
		mini_isp_register_write(0xffe80380, 0x00000004);
		mini_isp_register_write(0xffe80400, 0x00000802);
		mini_isp_register_write(0xffed1008, 0x0000aab0);
		mini_isp_register_write(0xffed100c, 0x00000306);
		mini_isp_register_write(0xffed1010, 0x00000147);
		mini_isp_register_write(0xffed1014, 0x0000aa73);
		mini_isp_register_write(0xffed1018, 0x00000eaa);
		mini_isp_register_write(0xffed101c, 0x00008e1a);
		mini_isp_register_write(0xffed1044, 0x000000b8);
		mini_isp_register_write(0xffed1044, 0x00000098);
		udelay(0x00000028);
		mini_isp_register_write(0xffed1044, 0x00000088);
		udelay(0x00000028);
		mini_isp_register_write(0xffed1030, 0x00080000);
		mini_isp_register_write(0xffed1034, 0x00080000);
		mini_isp_register_write(0xffed1038, 0x00080000);
		mini_isp_register_write(0xffed103c, 0x00080000);
		mini_isp_register_write(0xffed1040, 0x00080000);
		udelay(0x00000006);
		mini_isp_register_write(0xffed1030, 0x00080002);
		mini_isp_register_write(0xffed1034, 0x00080002);
		mini_isp_register_write(0xffed1038, 0x00080002);
		mini_isp_register_write(0xffed103c, 0x00080002);
		mini_isp_register_write(0xffed1040, 0x00080002);
		mini_isp_register_write(0xffed1000, 0x00000000);
		mini_isp_register_write(0xfff97000, 0x00000001);
		mini_isp_register_write(0xfff97004, 0x00003210);
		mini_isp_register_write(0xfff97008, 0x00003210);
		mini_isp_register_write(0xfff9700c, 0x145000b4);
		mini_isp_register_write(0xfff97010, 0x00000000);
		mini_isp_register_write(0xfff97014, 0x00000000);
		mini_isp_register_write(0xfff97018, 0x00000000);
		mini_isp_register_write(0xfff9701c, 0x00000000);
		mini_isp_register_write(0xfff97020, 0x00000000);
		mini_isp_register_write(0xfff97024, 0x00000010);
		mini_isp_register_write(0xfff97028, 0x0000001e);
		mini_isp_register_write(0xfff9702c, 0x0000000b);
		mini_isp_register_write(0xfff97030, 0x0f0f0f0f);
		mini_isp_register_write(0xfff97000, 0x00000000);
		mini_isp_register_write(0xfff91000, 0x1000000b);
		mini_isp_register_write(0xfff91024, 0x0000000f);
		mini_isp_register_write(0xfff91028, 0x00001010);
		mini_isp_register_write(0xfff9106c, 0x00000c0c);
		mini_isp_register_write(0xfff91040, 0x00003c02);
		udelay(0x00000028);
		mini_isp_register_write(0xfff91004, 0x00000000);
		mini_isp_register_write(0xfff91008, 0x00003033);
		mini_isp_register_write(0xfff91010, 0x00003c02);
		mini_isp_register_write(0xfff91014, 0x00003c02);
		mini_isp_register_write(0xfff9103c, 0x00000000);
		mini_isp_register_write(0xfff91098, 0x00444404);
		mini_isp_register_write(0xfff9104c, 0x000d0011);
		mini_isp_register_write(0xfff91000, 0x1000000b);
		mini_isp_register_write(0xfff91024, 0x0000000f);
		mini_isp_register_write(0xfff91028, 0x0000013f);
		mini_isp_register_write(0xfff9106c, 0x00000e0e);
		mini_isp_register_write(0xfff9104c, 0x000d0011);
		mini_isp_register_write(0xfff91070, 0x01000005);
		mini_isp_register_write(0xfff910a8, 0x00000000);
		mini_isp_register_write(0xfff91094, 0x00001021);
		mini_isp_register_write(0xfff91000, 0x1000000a);
		break;
	case 2:
		mini_isp_register_write(0xffe40000, 0x00000008);
		mini_isp_register_write(0xffe40004, 0x00006621);
		mini_isp_register_write(0xffe40008, 0x00006621);
		mini_isp_register_write(0xffe4000c, 0x00006621);
		mini_isp_register_write(0xffe40010, 0x00006621);
		mini_isp_register_write(0xffe40050, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000200);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81100, 0x00000000);
		mini_isp_register_write(0xffe81104, 0x00000000);
		mini_isp_register_write(0xffe81108, 0x000000dc);
		mini_isp_register_write(0xffe8110c, 0x00000000);
		mini_isp_register_write(0xffe81110, 0x00000001);
		mini_isp_register_write(0xffe81114, 0x00000000);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe800c0, 0x0000000a);
		mini_isp_register_write(0xffe800e0, 0x0000000a);
		mini_isp_register_write(0xffe80100, 0x0000000a);
		mini_isp_register_write(0xffe80120, 0x0000000a);
		mini_isp_register_write(0xffe81004, 0x00000800);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81120, 0x00000000);
		mini_isp_register_write(0xffe81124, 0x00000000);
		mini_isp_register_write(0xffe81128, 0x0000017a);
		mini_isp_register_write(0xffe8112c, 0x00000000);
		mini_isp_register_write(0xffe81130, 0x00000001);
		mini_isp_register_write(0xffe81134, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe81004, 0x00000400);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81140, 0x00000000);
		mini_isp_register_write(0xffe81144, 0x00000000);
		mini_isp_register_write(0xffe81148, 0x0000017a);
		mini_isp_register_write(0xffe8114c, 0x00000000);
		mini_isp_register_write(0xffe81150, 0x00000001);
		mini_isp_register_write(0xffe81154, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe80b00, 0x00000819);
		mini_isp_register_write(0xffe80940, 0x00000800);
		mini_isp_register_write(0xffe80440, 0x00000004);
		mini_isp_register_write(0xffe80460, 0x00000802);
		mini_isp_register_write(0xffed6008, 0x0000aab0);
		mini_isp_register_write(0xffed600c, 0x00000306);
		mini_isp_register_write(0xffed6010, 0x00000147);
		mini_isp_register_write(0xffed6014, 0x0000aa73);
		mini_isp_register_write(0xffed6018, 0x00000eaa);
		mini_isp_register_write(0xffed601c, 0x00008e1a);
		mini_isp_register_write(0xffed6044, 0x000000b8);
		mini_isp_register_write(0xffed6044, 0x00000098);
		udelay(0x00000028);
		mini_isp_register_write(0xffed6044, 0x00000088);
		udelay(0x00000028);
		mini_isp_register_write(0xffed6030, 0x00080000);
		mini_isp_register_write(0xffed6034, 0x00080000);
		mini_isp_register_write(0xffed6038, 0x00080000);
		mini_isp_register_write(0xffed603c, 0x00080000);
		mini_isp_register_write(0xffed6040, 0x00080000);
		udelay(0x00000006);
		mini_isp_register_write(0xffed6030, 0x00080002);
		mini_isp_register_write(0xffed6034, 0x00080002);
		mini_isp_register_write(0xffed6038, 0x00080002);
		mini_isp_register_write(0xffed603c, 0x00080002);
		mini_isp_register_write(0xffed6040, 0x00080002);
		mini_isp_register_write(0xffed6000, 0x00000000);
		mini_isp_register_write(0xfff98000, 0x00000001);
		mini_isp_register_write(0xfff98004, 0x00003210);
		mini_isp_register_write(0xfff98008, 0x00003210);
		mini_isp_register_write(0xfff9800c, 0x14500344);
		mini_isp_register_write(0xfff98010, 0x00000000);
		mini_isp_register_write(0xfff98014, 0x00000000);
		mini_isp_register_write(0xfff98018, 0x00000000);
		mini_isp_register_write(0xfff9801c, 0x00000000);
		mini_isp_register_write(0xfff98020, 0x00000000);
		mini_isp_register_write(0xfff98024, 0x000000ec);
		mini_isp_register_write(0xfff98028, 0x0000001e);
		mini_isp_register_write(0xfff9802c, 0x000000c3);
		mini_isp_register_write(0xfff98030, 0x56565656);
		mini_isp_register_write(0xfff98000, 0x00000000);
		mini_isp_register_write(0xfff94000, 0x1000000b);
		mini_isp_register_write(0xfff94024, 0x0000000f);
		mini_isp_register_write(0xfff94028, 0x00001010);
		mini_isp_register_write(0xfff9406c, 0x00000c0c);
		mini_isp_register_write(0xfff94040, 0x00003c02);
		udelay(0x00000028);
		mini_isp_register_write(0xfff94004, 0x00000000);
		mini_isp_register_write(0xfff94008, 0x00003033);
		mini_isp_register_write(0xfff94010, 0x00003c02);
		mini_isp_register_write(0xfff94014, 0x00003c02);
		mini_isp_register_write(0xfff9403c, 0x00000000);
		mini_isp_register_write(0xfff94098, 0x00444404);
		mini_isp_register_write(0xfff9404c, 0x000d0011);
		mini_isp_register_write(0xfff94000, 0x1000000b);
		mini_isp_register_write(0xfff94024, 0x0000000f);
		mini_isp_register_write(0xfff94028, 0x00003f01);
		mini_isp_register_write(0xfff9406c, 0x00000e0e);
		mini_isp_register_write(0xfff9404c, 0x000d0011);
		mini_isp_register_write(0xfff94070, 0x01000005);
		mini_isp_register_write(0xfff940a8, 0x00000000);
		mini_isp_register_write(0xfff94094, 0x00001021);
		mini_isp_register_write(0xfff94000, 0x1000000a);
		break;
	case 3:
		mini_isp_register_write(0xffe40000, 0x00000008);
		mini_isp_register_write(0xffe40004, 0x00006621);
		mini_isp_register_write(0xffe40008, 0x00006621);
		mini_isp_register_write(0xffe4000c, 0x00006621);
		mini_isp_register_write(0xffe40010, 0x00006621);
		mini_isp_register_write(0xffe40050, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000200);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81100, 0x00000000);
		mini_isp_register_write(0xffe81104, 0x00000000);
		mini_isp_register_write(0xffe81108, 0x000000dc);
		mini_isp_register_write(0xffe8110c, 0x00000000);
		mini_isp_register_write(0xffe81110, 0x00000001);
		mini_isp_register_write(0xffe81114, 0x00000000);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe800c0, 0x0000000a);
		mini_isp_register_write(0xffe800e0, 0x0000000a);
		mini_isp_register_write(0xffe80100, 0x0000000a);
		mini_isp_register_write(0xffe80120, 0x0000000a);
		mini_isp_register_write(0xffe81004, 0x00000800);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81120, 0x00000000);
		mini_isp_register_write(0xffe81124, 0x00000000);
		mini_isp_register_write(0xffe81128, 0x0000017a);
		mini_isp_register_write(0xffe8112c, 0x00000000);
		mini_isp_register_write(0xffe81130, 0x00000001);
		mini_isp_register_write(0xffe81134, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe81004, 0x00000400);
		udelay(0x00000005);
		mini_isp_register_write(0xffe81140, 0x00000000);
		mini_isp_register_write(0xffe81144, 0x00000000);
		mini_isp_register_write(0xffe81148, 0x0000017a);
		mini_isp_register_write(0xffe8114c, 0x00000000);
		mini_isp_register_write(0xffe81150, 0x00000001);
		mini_isp_register_write(0xffe81154, 0x00000001);
		mini_isp_register_write(0xffe81004, 0x00000000);
		udelay(0x0000015e);
		mini_isp_register_write(0xffe80b00, 0x00000819);
		mini_isp_register_write(0xffe80880, 0x00000400);
		mini_isp_register_write(0xffe80380, 0x00000004);
		mini_isp_register_write(0xffe80400, 0x00000802);
		mini_isp_register_write(0xffe80940, 0x00000800);
		mini_isp_register_write(0xffe80440, 0x00000004);
		mini_isp_register_write(0xffe80460, 0x00000802);
		mini_isp_register_write(0xffed1008, 0x0000aab0);
		mini_isp_register_write(0xffed100c, 0x00000306);
		mini_isp_register_write(0xffed1010, 0x00000147);
		mini_isp_register_write(0xffed1014, 0x0000aa73);
		mini_isp_register_write(0xffed1018, 0x00000eaa);
		mini_isp_register_write(0xffed101c, 0x00008e1a);
		mini_isp_register_write(0xffed1044, 0x000000b8);
		mini_isp_register_write(0xffed1044, 0x00000098);
		udelay(0x00000028);
		mini_isp_register_write(0xffed1044, 0x00000088);
		udelay(0x00000028);
		mini_isp_register_write(0xffed1030, 0x00080000);
		mini_isp_register_write(0xffed1034, 0x00080000);
		mini_isp_register_write(0xffed1038, 0x00080000);
		mini_isp_register_write(0xffed103c, 0x00080000);
		mini_isp_register_write(0xffed1040, 0x00080000);
		udelay(0x00000006);
		mini_isp_register_write(0xffed1030, 0x00080002);
		mini_isp_register_write(0xffed1034, 0x00080002);
		mini_isp_register_write(0xffed1038, 0x00080002);
		mini_isp_register_write(0xffed103c, 0x00080002);
		mini_isp_register_write(0xffed1040, 0x00080002);
		mini_isp_register_write(0xffed1000, 0x00000000);
		mini_isp_register_write(0xfff97000, 0x00000001);
		mini_isp_register_write(0xfff97004, 0x00003210);
		mini_isp_register_write(0xfff97008, 0x00003210);
		mini_isp_register_write(0xfff9700c, 0x145000b4);
		mini_isp_register_write(0xfff97010, 0x00000000);
		mini_isp_register_write(0xfff97014, 0x00000000);
		mini_isp_register_write(0xfff97018, 0x00000000);
		mini_isp_register_write(0xfff9701c, 0x00000000);
		mini_isp_register_write(0xfff97020, 0x00000000);
		mini_isp_register_write(0xfff97024, 0x00000010);
		mini_isp_register_write(0xfff97028, 0x0000001e);
		mini_isp_register_write(0xfff9702c, 0x0000000b);
		mini_isp_register_write(0xfff97030, 0x0f0f0f0f);
		mini_isp_register_write(0xfff97000, 0x00000000);
		mini_isp_register_write(0xfff91000, 0x1000000b);
		mini_isp_register_write(0xfff91024, 0x0000000f);
		mini_isp_register_write(0xfff91028, 0x00001010);
		mini_isp_register_write(0xfff9106c, 0x00000c0c);
		mini_isp_register_write(0xfff91040, 0x00003c02);
		udelay(0x00000028);
		mini_isp_register_write(0xfff91004, 0x00000000);
		mini_isp_register_write(0xfff91008, 0x00003033);
		mini_isp_register_write(0xfff91010, 0x00003c02);
		mini_isp_register_write(0xfff91014, 0x00003c02);
		mini_isp_register_write(0xfff9103c, 0x00000000);
		mini_isp_register_write(0xfff91098, 0x00444404);
		mini_isp_register_write(0xfff9104c, 0x000d0011);
		mini_isp_register_write(0xfff91000, 0x1000000b);
		mini_isp_register_write(0xfff91024, 0x0000000f);
		mini_isp_register_write(0xfff91028, 0x0000013f);
		mini_isp_register_write(0xfff9106c, 0x00000e0e);
		mini_isp_register_write(0xfff9104c, 0x000d0011);
		mini_isp_register_write(0xfff91070, 0x01000005);
		mini_isp_register_write(0xfff910a8, 0x00000000);
		mini_isp_register_write(0xfff91094, 0x00001021);
		mini_isp_register_write(0xfff91000, 0x1000000a);
		mini_isp_register_write(0xffed6008, 0x0000aab0);
		mini_isp_register_write(0xffed600c, 0x00000306);
		mini_isp_register_write(0xffed6010, 0x00000147);
		mini_isp_register_write(0xffed6014, 0x0000aa73);
		mini_isp_register_write(0xffed6018, 0x00000eaa);
		mini_isp_register_write(0xffed601c, 0x00008e1a);
		mini_isp_register_write(0xffed6044, 0x000000b8);
		mini_isp_register_write(0xffed6044, 0x00000098);
		udelay(0x00000028);
		mini_isp_register_write(0xffed6044, 0x00000088);
		udelay(0x00000028);
		mini_isp_register_write(0xffed6030, 0x00080000);
		mini_isp_register_write(0xffed6034, 0x00080000);
		mini_isp_register_write(0xffed6038, 0x00080000);
		mini_isp_register_write(0xffed603c, 0x00080000);
		mini_isp_register_write(0xffed6040, 0x00080000);
		udelay(0x00000006);
		mini_isp_register_write(0xffed6030, 0x00080002);
		mini_isp_register_write(0xffed6034, 0x00080002);
		mini_isp_register_write(0xffed6038, 0x00080002);
		mini_isp_register_write(0xffed603c, 0x00080002);
		mini_isp_register_write(0xffed6040, 0x00080002);
		mini_isp_register_write(0xffed6000, 0x00000000);
		mini_isp_register_write(0xfff98000, 0x00000001);
		mini_isp_register_write(0xfff98004, 0x00003210);
		mini_isp_register_write(0xfff98008, 0x00003210);
		mini_isp_register_write(0xfff9800c, 0x14500344);
		mini_isp_register_write(0xfff98010, 0x00000000);
		mini_isp_register_write(0xfff98014, 0x00000000);
		mini_isp_register_write(0xfff98018, 0x00000000);
		mini_isp_register_write(0xfff9801c, 0x00000000);
		mini_isp_register_write(0xfff98020, 0x00000000);
		mini_isp_register_write(0xfff98024, 0x000000ec);
		mini_isp_register_write(0xfff98028, 0x0000001e);
		mini_isp_register_write(0xfff9802c, 0x000000c3);
		mini_isp_register_write(0xfff98030, 0x56565656);
		mini_isp_register_write(0xfff98000, 0x00000000);
		mini_isp_register_write(0xfff94000, 0x1000000b);
		mini_isp_register_write(0xfff94024, 0x0000000f);
		mini_isp_register_write(0xfff94028, 0x00001010);
		mini_isp_register_write(0xfff9406c, 0x00000c0c);
		mini_isp_register_write(0xfff94040, 0x00003c02);
		udelay(0x00000028);
		mini_isp_register_write(0xfff94004, 0x00000000);
		mini_isp_register_write(0xfff94008, 0x00003033);
		mini_isp_register_write(0xfff94010, 0x00003c02);
		mini_isp_register_write(0xfff94014, 0x00003c02);
		mini_isp_register_write(0xfff9403c, 0x00000000);
		mini_isp_register_write(0xfff94098, 0x00444404);
		mini_isp_register_write(0xfff9404c, 0x000d0011);
		mini_isp_register_write(0xfff94000, 0x1000000b);
		mini_isp_register_write(0xfff94024, 0x0000000f);
		mini_isp_register_write(0xfff94028, 0x00003f01);
		mini_isp_register_write(0xfff9406c, 0x00000e0e);
		mini_isp_register_write(0xfff9404c, 0x000d0011);
		mini_isp_register_write(0xfff94070, 0x01000005);
		mini_isp_register_write(0xfff940a8, 0x00000000);
		mini_isp_register_write(0xfff94094, 0x00001021);
		mini_isp_register_write(0xfff94000, 0x1000000a);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(mini_isp_pure_bypass_debug);


u32 mini_isp_register_read_then_write_file(
			u32 start_reg_addr, u32 end_reg_addr,
			char *dest_path, char *module_name)
{
	u32 retval = ERR_SUCCESS;
	u32 count;
	u8 *dump_memory = NULL;
	u8 *keep_dump_memory = NULL;
	u32 ouput_size;
	u8 filename[128];
	struct file *f;
	mm_segment_t fs;

	/* how many registers(4 bytes) do you want to read? */
	count = ((end_reg_addr - start_reg_addr) / 4) + 1;
	/* read 4 bytes register value */
	ouput_size = (count + 2) * 4;

	dump_memory = kzalloc(ouput_size, GFP_KERNEL);
	if (!dump_memory) {
		retval = -ENOMEM;
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_memory_fail;
	}
	keep_dump_memory = dump_memory;


	memcpy(dump_memory, &start_reg_addr, 4);
	dump_memory = dump_memory + 4;
	memcpy(dump_memory, &count, 4);
	dump_memory = dump_memory + 4;

	while (start_reg_addr <= end_reg_addr) {

		mini_isp_register_read(start_reg_addr, (u32 *)dump_memory);

		start_reg_addr = start_reg_addr + 4;
		dump_memory = dump_memory + 4;
	}



	snprintf(filename, 128, "%s/%s_%x.regx",
		dest_path, module_name, start_reg_addr);
#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	goto file_open_fail;
#endif
	/*Get current segment descriptor*/
	fs = get_fs();
	/*Set segment descriptor associated*/
	set_fs(get_ds());

	if (IS_ERR(f)) {
		retval = PTR_ERR(f);
		set_fs(fs);
		misp_err("%s open file failed. err: %d", __func__, retval);
		goto file_open_fail;
	}

	/*write the file*/
	vfs_write(f, (char *)keep_dump_memory, ouput_size, &f->f_pos);

	/*Restore segment descriptor*/
	set_fs(fs);
	filp_close(f, NULL);

file_open_fail:
allocate_memory_fail:
	kfree(keep_dump_memory);

	return retval;
}

u32 mini_isp_register_read(u32 reg_addr, u32 *reg_value)
{
	int status = ERR_SUCCESS;
	u8 *send_buffer;
	u8 *recv_buffer;
	u8 ctrlbyte = 0;
	u32 address = reg_addr;
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;

	u8 send_buffer_value[64] = {0};
	u8 recv_buffer_value[64] = {0};
	u32 rx_dummy_len;

	dev_global_variable = get_mini_isp_global_variable();

	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = send_buffer_value;
	recv_buffer = recv_buffer_value;
	if (SPI_SHORT_LEN_MODE && SPI_SHORT_LEN_MODE_READ_ENABLE) {
		/* Setp1, Send address: { ctrlbyte[1byte] + addr[4byte] } */
		ctrlbyte = CTRL_BYTE_REGRD_W;
		memcpy(send_buffer, &ctrlbyte, 1);
		memcpy(send_buffer + 1, &address, 4);

		/* read 4 bytes register value */
		status = devdata->intf_fn->read(devdata,
						send_buffer_value,
						EMODE_TXCMD_LEN,
						recv_buffer_value,
						0);

		if (status) {
			misp_err("%s - sync error: status = %d",
				__func__, status);
			goto mini_isp_register_read_end;
		}


		udelay(50);

		/* Setp2, wait a moment and
		 * get data{ dummy[1byte] + data[4byte] }
		 */
		ctrlbyte = CTRL_BYTE_GETDATA_W;
		memcpy(send_buffer, &ctrlbyte, 1);

		status = devdata->intf_fn->read(devdata,
						send_buffer_value,
						1,
						recv_buffer_value,
						5);

		if (status) {
			misp_err("%s - sync error: status = %d",
				__func__, status);
			goto mini_isp_register_read_end;
		}

		recv_buffer = (u8 *)&recv_buffer_value[1];

	} else {
		ctrlbyte = CTRL_BYTE_REGRD;
		rx_dummy_len = mini_isp_get_rx_dummy_byte(ctrlbyte);
	#if EN_SPIE_REG_DUMMY_BYTE
		mini_isp_set_spie_dummy_byte(rx_dummy_len-1); /* 0 base */
	#endif

		memcpy(send_buffer, &ctrlbyte, 1);
		memcpy(send_buffer + 1, &address, 4);

		/* read 4 bytes register value */
		status = devdata->intf_fn->read(devdata,
						send_buffer_value,
						EMODE_TXCMD_LEN,
						recv_buffer_value,
						rx_dummy_len + 4);

		if (status) {
			misp_err("%s - sync error: status = %d",
				__func__, status);
			goto mini_isp_register_read_end;
		}

		if (dev_global_variable->intf_status & INTF_SPI_READY) {
			/* check if send len + recv len > 32. if yes then
			 * rx_dummy_len need + EMODE_TXCMD_LEN
			 */
			if (EMODE_TXCMD_LEN + rx_dummy_len + 4 > 32)
				status = mini_isp_check_rx_dummy(&recv_buffer,
					rx_dummy_len+EMODE_TXCMD_LEN);
			else
				status = mini_isp_check_rx_dummy(&recv_buffer,
					rx_dummy_len);

			if (status) {
				misp_err("[miniisp]Can't get reg");
				goto mini_isp_register_read_end;
			}
		}

	}
	memcpy(reg_value, recv_buffer, 4);

mini_isp_register_read_end:

	return status;
}

void mini_isp_memory_write(u32 memory_addr, u8 *write_buffer, u32 write_len)
{
	u8 *send_buffer;
	u8 ctrlbyte = CTRL_BYTE_MEMWR;
	u32 address = memory_addr;
	u32 start_mem_addr = memory_addr;
	u32 end_mem_addr = memory_addr + write_len;
	struct misp_data *devdata;
	u8 send_buffer_value[EMODE_TXCMD_LEN + write_len];
	struct misp_global_variable *dev_global_variable;
	u32 wt_len = write_len;
	u8 *wt_buffer;

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = send_buffer_value;

	/* dev_global_variable->before_booting = 1; */


	if (SPI_SHORT_LEN_MODE && SPI_SHORT_LEN_MODE_WRITE_ENABLE) {
		wt_buffer = write_buffer;

		while (start_mem_addr < end_mem_addr) {

			wt_len = end_mem_addr - start_mem_addr;

			if (wt_len >= 4) {
				memcpy(send_buffer, &ctrlbyte, 1);
				memcpy(send_buffer + 1, &start_mem_addr, 4);
				memcpy(send_buffer + 5, wt_buffer, 4);
				devdata->intf_fn->write(
							devdata,
							send_buffer,
							NULL,
							EMODE_TXCMD_LEN + 4);
			} else {
				memcpy(send_buffer, &ctrlbyte, 1);
				memcpy(send_buffer + 1, &start_mem_addr, 4);
				memcpy(send_buffer + 5, wt_buffer, wt_len);
				devdata->intf_fn->write(
						devdata,
						send_buffer,
						NULL,
						EMODE_TXCMD_LEN + wt_len);
			}

			wt_buffer = wt_buffer + 4;
			start_mem_addr = start_mem_addr + 4;
		}
	} else {
		memcpy(send_buffer, &ctrlbyte, 1);
		memcpy(send_buffer + 1, &address, 4);
		memcpy(send_buffer + 5, write_buffer, write_len);
		devdata->intf_fn->write(
					devdata,
					send_buffer,
					NULL,
					EMODE_TXCMD_LEN + write_len);
	}
	/* dev_global_variable->before_booting = 0; */
}

u32 mini_isp_memory_read_then_write_file(u32 start_addr, u32 len,
	char *dest_path, char *file_name)
{
	u32 retval = ERR_SUCCESS;
	u8 *dump_memory   = NULL;
	u8 filename[128];
	struct file *f;
	mm_segment_t fs;

	/* misp_err("%s - entering", __func__); */

	dump_memory = kzalloc(len, GFP_KERNEL);
	if (!dump_memory) {
		retval = -ENOMEM;
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_memory_fail;
	}

	mini_isp_memory_read(start_addr, dump_memory, len);


	snprintf(filename, 128, "%s/%s.raw", dest_path, file_name);
#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	goto file_open_fail;
#endif
	/*Get current segment descriptor*/
	fs = get_fs();
	/*Set segment descriptor associated*/
	set_fs(get_ds());

	if (IS_ERR(f)) {
		retval = PTR_ERR(f);
		set_fs(fs);
		misp_err("%s open file failed. err: %d", __func__, retval);
		goto file_open_fail;
	}
	/*write the file*/
	vfs_write(f, (char *)dump_memory, len, &f->f_pos);

	/*Restore segment descriptor*/
	set_fs(fs);
	filp_close(f, NULL);
file_open_fail:
allocate_memory_fail:
	kfree(dump_memory);

	return retval;
}
EXPORT_SYMBOL(mini_isp_memory_read_then_write_file);
/**
 *\brief  Get woi image and save as file.
 *\param start_addr [In], start address
 *\param lineoffset [In], line offset (byte)
 *\param width      [In], image width (byte), it can't exceed 60000bytes.
 *\param height     [In], image height(byte)
 *\param dest_path  [In], save path
 *\param file_name  [In], save file name
 *\return
 */
u32 mini_isp_woi_memory_read_then_write_file(u32 start_addr,
		u32 lineoffset, u32 width, u32 height,
		char *dest_path, char *file_name)
{
	u32 retval = ERR_SUCCESS;
	u8 *dump_memory   = NULL;
	u8 *keep_dump_memory = NULL;
	u32 dump_addr = start_addr;
	u32 ouput_size;
	u32 one_size;
	u32 loopidx = 0;
	s32 remain_size;
	u8 filename[128];
	struct file *f;
	mm_segment_t fs;

	misp_info("%s, lineoffset: %d, width: %d, height: %d, filename: %s",
		__func__, lineoffset, width, height, file_name);

	ouput_size = width * height;
	dump_memory = kzalloc(ouput_size, GFP_KERNEL);
	if (!dump_memory) {
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_dump_memory_fail;
	}
	keep_dump_memory = dump_memory;


	/* memory read */
	for (loopidx = 0, remain_size = ouput_size; remain_size > 0;
		remain_size -= width, loopidx++) {
		/* update read memory size and address */
		one_size = (remain_size > width) ?	width : remain_size;
		dump_addr = start_addr + loopidx*lineoffset;

		retval = mini_isp_memory_read(dump_addr, dump_memory, one_size);
		if (retval) {
			misp_err("%s get failed.", __func__);
			goto mini_isp_memory_read_get_fail;
		}

		misp_info("%s dump_addr = 0x%x one_size = %d",
			__func__, dump_addr, one_size);

		dump_memory += one_size;
		dump_addr += one_size;
	}

	misp_info("%s read_finish", __func__);

	snprintf(filename, 128, "%s/%s.raw", dest_path, file_name);
#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	goto file_open_fail;
#endif
	/*Get current segment descriptor*/
	fs = get_fs();
	/*Set segment descriptor associated*/
	set_fs(get_ds());

	if (IS_ERR(f)) {
		retval = PTR_ERR(f);
		set_fs(fs);
		misp_err("%s open file failed. err: %d", __func__, retval);
		goto file_open_fail;
	}
	/*write the file*/
	vfs_write(f, (char *)keep_dump_memory, ouput_size, &f->f_pos);

	/*Restore segment descriptor*/
	set_fs(fs);
	filp_close(f, NULL);
file_open_fail:
mini_isp_memory_read_get_fail:
allocate_dump_memory_fail:
	kfree(keep_dump_memory);

	return retval;
}
EXPORT_SYMBOL(mini_isp_woi_memory_read_then_write_file);
u32 mini_isp_memory_read_shortlen(u32 start_addr, u32 *read_buffer)
{
	int status = ERR_SUCCESS;
	u8 *send_buffer;
	u8 *recv_buffer;
	u8 ctrlbyte = 0;
	u32 address = start_addr;
	struct misp_data *devdata;
	u8 send_buffer_value[64] = {0};
	u8 recv_buffer_value[64] = {0};


	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	send_buffer = send_buffer_value;
	recv_buffer = recv_buffer_value;


	/* misp_err("%s - entering", __func__); */

	/* Setp1, Send memory address: { ctrlbyte[1byte] + addr[4byte] } */
	ctrlbyte = CTRL_BYTE_MEMRD_W;
	memcpy(send_buffer, &ctrlbyte, 1);
	memcpy(send_buffer + 1, &address, 4);

	/* read 4 bytes memory value */
	status = devdata->intf_fn->read(
						devdata,
						send_buffer_value,
						EMODE_TXCMD_LEN,
						recv_buffer_value,
						0);

	if (status) {
		misp_err("%s - sync error: status = %d", __func__, status);
		goto mini_isp_memory_read_shortlen_end;
	}

	udelay(500);

	/* Setp2, wait a moment and get data{ dummy[1byte] + data[4byte] } */
	ctrlbyte = CTRL_BYTE_GETDATA_W;
	memcpy(send_buffer, &ctrlbyte, 1);

	status = devdata->intf_fn->read(devdata,
					send_buffer_value,
					1,
					recv_buffer_value,
					5);

	if (status) {
		misp_err("%s - sync error: status = %d", __func__, status);
		goto mini_isp_memory_read_shortlen_end;
	}

	recv_buffer = (u8 *)&recv_buffer_value[1];



	memcpy(read_buffer, recv_buffer, 4);

mini_isp_memory_read_shortlen_end:

	return status;

}


u32 mini_isp_memory_read(u32 start_addr, u8 *read_buffer, u32 len)
{
	struct misp_data *devdata;
	struct misp_global_variable *dev_global_variable;
	u32 retval = ERR_SUCCESS;
	u8 *io_buffer = NULL;
	u8 *send_buffer;
	u8 *recv_buffer;
	u8 *dump_memory   = NULL;
	u8 *keep_dump_memory = NULL;
	u32 dump_addr = start_addr;
	u32 start_mem_addr = start_addr;
	u32 ouput_size;
	u32 io_size, remain_size, one_size, recv_buffer_size, send_buffer_size;
	u32 rx_dummy_len;
	u8 ctrlbyte = CTRL_BYTE_MEMRD;
	u32 len_align;

	misp_err("%s - entering", __func__);

	/* 4byte alignment for 'mini_isp_memory_read_shortlen()' */
	len_align = ALIGN_CEIL(len, 4);

	dump_memory = kzalloc(len_align, GFP_KERNEL);
	if (!dump_memory) {
		retval = -ENOMEM;
		misp_err("%s Allocate memory failed.", __func__);
		goto allocate_memory_fail;
	}
	keep_dump_memory = dump_memory;
	ouput_size = len;


	if (SPI_SHORT_LEN_MODE && SPI_SHORT_LEN_MODE_READ_ENABLE) {
		while (start_mem_addr < start_addr + len) {
			mini_isp_memory_read_shortlen(start_mem_addr,
					(u32 *)dump_memory);

			start_mem_addr = start_mem_addr + 4;
			dump_memory = dump_memory + 4;
		}
	} else {
		dev_global_variable = get_mini_isp_global_variable();
		devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);

		rx_dummy_len = mini_isp_get_rx_dummy_byte(ctrlbyte);

	#if EN_SPIE_REG_DUMMY_BYTE
		mini_isp_set_spie_dummy_byte(rx_dummy_len-1); /* 0 base */
	#endif

		send_buffer_size = EMODE_TXCMD_LEN;
		recv_buffer_size = EMODE_TXCMD_LEN + rx_dummy_len + 60000;

		/* read 60000 bytes at a time;*/
		io_size = send_buffer_size + recv_buffer_size;
		io_buffer = kzalloc(io_size, GFP_KERNEL);
		if (!io_buffer) {
			misp_err("%s Allocate memory failed.", __func__);
			goto allocate_memory_fail;
		}

		/* memory read */
		for (remain_size = ouput_size; remain_size > 0;
			remain_size -= one_size) {

			one_size = (remain_size > 60000) ?
				60000 : remain_size;

			memset(io_buffer, 0, io_size);
			send_buffer = io_buffer;
			recv_buffer = io_buffer + EMODE_TXCMD_LEN;

			memcpy(send_buffer, &ctrlbyte, 1);
			memcpy(send_buffer + 1, &dump_addr, 4);

			retval = devdata->intf_fn->read((void *)devdata,
						send_buffer,
						EMODE_TXCMD_LEN,
						recv_buffer,
						one_size + rx_dummy_len);
			if (retval) {
				misp_err("%s get failed.", __func__);
				goto mini_isp_memory_read_get_fail;
			}

			if (dev_global_variable->intf_status & INTF_SPI_READY) {
				/* check if send len + recv len > 32. if yes then
				 * rx_dummy_len need + EMODE_TXCMD_LEN
				 */
				if (EMODE_TXCMD_LEN + rx_dummy_len + one_size > 32)
					retval = mini_isp_check_rx_dummy(
						&recv_buffer,
						rx_dummy_len+EMODE_TXCMD_LEN);
				else
					retval = mini_isp_check_rx_dummy(
						&recv_buffer,
						rx_dummy_len);

				if (retval) {
					misp_err("%s get failed.", __func__);
					goto mini_isp_memory_read_get_fail;
				}
			}

			memcpy(dump_memory, recv_buffer, one_size);
			misp_info("%s dump_addr = 0x%x one_size = %d",
				__func__, dump_addr, one_size);
			dump_memory += one_size;
			dump_addr += one_size;
		}

	}

	misp_info("%s read_finish", __func__);

	memcpy(read_buffer, keep_dump_memory, ouput_size);

mini_isp_memory_read_get_fail:
allocate_memory_fail:
	kfree(keep_dump_memory);
	kfree(io_buffer);

	return retval;
}
EXPORT_SYMBOL(mini_isp_memory_read);


int mini_isp_get_bulk(struct misp_data *devdata, u8 *response_buf,
		u32 total_size, u32 block_size)
{
	int status = ERR_SUCCESS, count = 0;
	int remain_size, one_size;
	/* 1byte ctrlbyte, 2bytes recv */
	u8 io_buffer[3] = {0};
	u8 *send_buffer;
	u8 *recv_buffer;
	u8 ctrlbyte = USPICTRL_MS_CB_DIS;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	send_buffer = io_buffer;
	send_buffer[0] = ctrlbyte;
	recv_buffer = io_buffer + 1;

	misp_info("%s started.", __func__);

	status = devdata->intf_fn->read(
						(void *)devdata,
						send_buffer,
						1,
						recv_buffer,
						2);
	if (status) {
		misp_err(
			"mini_isp_send_bulk send ctrl byte failed. status:%d",
			status);
		status = -EINVAL;
		goto G_EXIT;
	}

	for (remain_size = total_size;
		 remain_size > 0;
		 remain_size -= one_size) {

		one_size = (remain_size > block_size) ?
					block_size : remain_size;
		/*get the data*/
		misp_info("%s dump start", __func__);
		status = devdata->intf_fn->read(
						(void *)devdata,
						response_buf,
						0,
						response_buf,
						one_size);

		if (status != 0) {
			misp_err("%s failed!! block:%d status: %d", __func__,
				count, status);
			break;
		}

		response_buf += one_size;
		count++;
	}

G_EXIT:

	if (status != ERR_SUCCESS)
		misp_info("%s - error: %d", __func__, status);
	else
		misp_info("%s - success.", __func__);

	return status;
}
EXPORT_SYMBOL_GPL(mini_isp_get_bulk);



int mini_isp_get_altek_status(void *devdata, u32 *altek_status)
{
	int status = ERR_SUCCESS;

	misp_info("%s - entering", __func__);

	mini_isp_register_read(INTERRUPT_STATUS_REGISTER_ADDR, altek_status);

	misp_info("%s - altek_status = %#x", __func__, *altek_status);

	return status;
}

/*interrupt handler function */
extern irqreturn_t mini_isp_irq(int irq, void *handle)
{
	struct misp_data *devdata = NULL;
	struct misp_global_variable *dev_global_variable;
	int errcode = ERR_SUCCESS;
	int original_altek_spi_mode;
	u32 altek_event_state = 0;

	misp_info("%s - enter", __func__);

	irqflag = true;

	dev_global_variable = get_mini_isp_global_variable();
	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	if (!devdata || !dev_global_variable)
		return -IRQ_NONE;

	original_altek_spi_mode = dev_global_variable->altek_spi_mode;
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
	  (dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
			mini_isp_a_to_e();
	}
	errcode = mini_isp_get_altek_status(devdata,
		&altek_event_state);
	misp_info("%s - read spi register: %#x",
			__func__, altek_event_state);

	event = MINI_ISP_RCV_WAITING;

	if (errcode == ERR_SUCCESS) {
		/*error event*/
		if (altek_event_state & SYSTEM_ERROR_LEVEL1) {
			event = event | MINI_ISP_RCV_ERROR;
			if (dev_global_variable->intf_status & INTF_SPI_READY)
				mini_isp_e_to_a();
			/* need to port out of this ISR */
			mini_isp_drv_get_err_code_cmd_in_irq();
			if (dev_global_variable->intf_status & INTF_SPI_READY)
				mini_isp_a_to_e();
		}
		if (altek_event_state & SYSTEM_ERROR_LEVEL2) {
			event = event | MINI_ISP_RCV_ERROR2;
			mini_isp_utility_read_reg_e_mode();
		}
		/*set sensor mode event*/
		if (altek_event_state & SET_SENSOR_MODE_READY)
			event = event | MINI_ISP_RCV_SETSENSORMODE;
		/*change cp mode event*/
		if (altek_event_state & CP_STATUS_CHANGE_DONE)
			event = event | MINI_ISP_RCV_CPCHANGE;
		/*ready event*/
		/*CMD*/
		if (altek_event_state & COMMAND_COMPLETE)
			event = event | MINI_ISP_RCV_CMD_READY;
		/*Bulk Data*/
		if (altek_event_state & BULK_DATA_COMPLETE)
			event = event | MINI_ISP_RCV_BULKDATA;
		/* streamoff event*/
		if (altek_event_state & STRMOFF_READY)
			event = event | MINI_ISP_RCV_STRMOFF;

		mini_isp_register_write(
				INTERRUPT_STATUS_REGISTER_ADDR,
				altek_event_state);
		} else {
			misp_err("%s - err: %d", __func__, errcode);
		}

		if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
			original_altek_spi_mode !=
			dev_global_variable->altek_spi_mode)
			mini_isp_e_to_a();

	wake_up_interruptible(&WAITQ);

	misp_info("%s - leave", __func__);

	return IRQ_HANDLED;
}

int mini_isp_wait_for_event(u32 e)
{
	int state  = 0;

#if (ISR_MECHANISM == INTERRUPT_METHOD)
	misp_info("%s - entering. event: %#x", __func__, e);
	do {
		state = wait_event_interruptible(WAITQ, event & e);
	} while (state == (-ERESTARTSYS));

	current_event = event;

	event = (~e) & event;/*MINI_ISP_RCV_WAITING;*/

	if (state)
		misp_err("%s - irq error. err: %d", __func__, state);

#else /* ISR_MECHANISM == POLLING_METHOD */

	struct misp_data *devdata = NULL;
	struct misp_global_variable *dev_global_variable;
	u32 altek_event_state = 0;
	int original_altek_spi_mode;

	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	dev_global_variable = get_mini_isp_global_variable();
	original_altek_spi_mode = dev_global_variable->altek_spi_mode;

	msleep(100);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
	  (dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
			mini_isp_a_to_e();
	}

	misp_info("%s - entering. event: %#x", __func__, e);
	do {
		misp_info("%s - polling...", __func__);

		mini_isp_get_altek_status(devdata, &altek_event_state);

		if (altek_event_state & SYSTEM_ERROR_LEVEL1) {
			if (dev_global_variable->intf_status & INTF_SPI_READY)
				mini_isp_e_to_a();
			/* need to port out of this ISR */
			mini_isp_drv_get_err_code_cmd_in_irq();
			if (dev_global_variable->intf_status & INTF_SPI_READY)
				mini_isp_a_to_e();
		} else if (altek_event_state & SYSTEM_ERROR_LEVEL2) {
			mini_isp_utility_read_reg_e_mode();
		} else if (altek_event_state & e) {
			/* find target, clear status */
			mini_isp_register_write(0xffef00b4, e);
			break;
		}
		msleep(50);
	} while ((altek_event_state & e) == 0);

	current_event = e;
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(original_altek_spi_mode !=
			dev_global_variable->altek_spi_mode))
			mini_isp_e_to_a();
#endif
	misp_info("%s - leaving. event: %#x", __func__, e);

	return state;
}

u32 mini_isp_get_currentevent(void)
{
	return current_event;
}

u32 mini_isp_check_rx_dummy(u8 **recv_buffer, u32 rx_dummy_len)
{
	u32 ret = 0;
#if (EN_SPIE_REG_DUMMY_BYTE == 0)
	u32 get_count = 0;
#endif

#if EN_SPIE_DEBUG
	/*u8 idx = 0;*/
	u8 col_idx = 0;
	u8 row_idx = 0;
	u8 *ptest = NULL;

	misp_info("%s, dummy byte: %d", __func__, rx_dummy_len);
	ptest = *recv_buffer;

	misp_info("  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
	for (col_idx = 0; col_idx < ((rx_dummy_len+1)/16); col_idx++) {
		misp_info(" %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
			(u32)*(ptest),    (u32)*(ptest+1),  (u32)*(ptest+2),  (u32)*(ptest+3),
			(u32)*(ptest+4),  (u32)*(ptest+5),  (u32)*(ptest+6),  (u32)*(ptest+7),
			(u32)*(ptest+8),  (u32)*(ptest+9),  (u32)*(ptest+10), (u32)*(ptest+11),
			(u32)*(ptest+12), (u32)*(ptest+13), (u32)*(ptest+14), (u32)*(ptest+15));

		ptest = ptest + 16;
	}

	/* remain less 16 byte data */
	col_idx -= 1;
	for (row_idx = 0; row_idx < (rx_dummy_len+1)-(col_idx*16); row_idx++) {
		misp_info(" %02X", (u32)*(ptest));
		ptest++;
	}
	#endif

#if EN_SPIE_REG_DUMMY_BYTE
	(*recv_buffer) = (*recv_buffer) + rx_dummy_len;
#else
	/* find first 0xa5 */
	while (**recv_buffer != 0xa5) {
		(*recv_buffer)++;
		get_count++;
		if (get_count >= rx_dummy_len) {
			misp_info("%s, dummy byte excess 32!", __func__);
			return -EIO;
		}
	}

	/* check last 0xa5*/
	while (*(*recv_buffer+1) == 0xa5) {
		(*recv_buffer)++;
		get_count++;
		if (get_count >= rx_dummy_len) {
			misp_info("%s, dummy byte excess 32!", __func__);
			return -EIO;
		}
	}

	/* find available data */
	(*recv_buffer)++;
#endif

#if EN_SPIE_DEBUG
	misp_info("%s, available byte: 0x%x", __func__, (u32)(**recv_buffer));
#endif
	return ret;
}


errcode mini_isp_create_directory(char *dir_name)
{
	size_t errcode = ERR_SUCCESS;
	struct dentry *dentry;
	struct path path;

	dentry = kern_path_create(AT_FDCWD, dir_name,
		&path, LOOKUP_DIRECTORY);
	if (IS_ERR(dentry)) {
		misp_info("%s, fail %d", __func__, IS_ERR(dentry));
		return PTR_ERR(dentry);
	}

	errcode = vfs_mkdir(path.dentry->d_inode, dentry, 0777);

	done_path_create(&path, dentry);

	misp_info("%s, create directory %s", __func__, dir_name);
	return errcode;
}

/**
 *\brief  Get rx dummy byte.
 *\param spie_rx_mode [In], spie rx mode. memory read or register read.
 *\                         If it's not spie mode, set 0 for default.
 *\return dummy byte lens
 */

u32 mini_isp_get_rx_dummy_byte(u8 spie_rx_mode)
{
	u32 rx_dummy_byte = 0;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	/* only spi-e need configure rx dummy byte */
	if (dev_global_variable->intf_status & INTF_SPI_READY) {
#if EN_SPIE_REG_DUMMY_BYTE
		if (spie_rx_mode == CTRL_BYTE_REGRD)
			rx_dummy_byte = SPIE_RX_REG_DUMMY_LEN;
		else if (spie_rx_mode == CTRL_BYTE_MEMRD)
			rx_dummy_byte = SPIE_RX_MEM_DUMMY_LEN;
#else
		rx_dummy_byte = SPIE_RX_DUMMY_LEN;
#endif
	} else if (dev_global_variable->intf_status & INTF_I2C_READY)
		rx_dummy_byte = 0;
	else
		rx_dummy_byte = 0;

	return rx_dummy_byte;
}

void mini_isp_set_spie_dummy_byte(u32 dummy_lens)
{
	mini_isp_register_write(SPIE_DUMMY_BYTE_ADDR, dummy_lens);
}
