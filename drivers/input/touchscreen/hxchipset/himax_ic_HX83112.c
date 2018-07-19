/*
 * Himax Android Driver Sample Code for HX83112 chipset
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "himax_ic_HX83112.h"


extern unsigned char IC_TYPE;


static bool hx83112_sense_off(void)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];

	do {
		/*
		 *===========================================
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *===========================================
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *===========================================
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 *===========================================
		 */
		tmp_data[0] = 0x00;

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *==========================================
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *==========================================
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *==========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *=====================
		 * Check enter_save_mode
		 *=====================
		 */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, FOUR_BYTE_ADDR_SZ, tmp_data, 0);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*
			 *===================================
			 * Reset TCON
			 *====================================
			 */
			g_core_fp.fp_flash_write_burst(pic_op->addr_tcon_on_rst, pic_op->data_rst);
			msleep(20);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_flash_write_burst(pic_op->addr_tcon_on_rst, tmp_data);
			/*
			 *===================================
			 * Reset ADC
			 *====================================
			 */
			g_core_fp.fp_flash_write_burst(pic_op->addr_adc_on_rst, pic_op->data_rst);
			msleep(20);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_flash_write_burst(pic_op->addr_adc_on_rst, tmp_data);
			return true;
		}

		msleep(20);
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#endif

	} while (cnt++ < 15);

	return false;
}

static void hx83112_func_re_init(void)
{
	g_core_fp.fp_sense_off = hx83112_sense_off;
}

static void hx83112_reg_re_init(void)
{
}

static bool hx83112_chip_detect(void)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	bool ret_data = false;
	int i = 0;

	himax_mcu_in_cmd_struct_init();
	himax_mcu_in_cmd_init();

	hx83112_reg_re_init();
	hx83112_func_re_init();

	g_core_fp.fp_sense_off();

	for (i = 0; i < 5; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_icid_addr, FOUR_BYTE_DATA_SZ, tmp_data, false);
		I("%s:Read driver IC ID = %X, %X, %X\n",  __func__, tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x11) && ((tmp_data[1] == 0x2a) || (tmp_data[1] == 0x2b))) {
			if (tmp_data[1] == 0x2a)
				strlcpy(private_ts->chip_name, HX_83112A_SERIES_PWON, 30);
			else if (tmp_data[1] == 0x2b)
				strlcpy(private_ts->chip_name, HX_83112B_SERIES_PWON, 30);

			I("%s:IC name = %s\n", __func__, private_ts->chip_name);

			I("Himax IC package %x%x%x in\n", tmp_data[3], tmp_data[2], tmp_data[1]);
			ret_data = true;
			break;
		}

		ret_data = false;
		E("%s:Read driver ID register Fail:\n",  __func__);

	}

	return ret_data;
}

static void hx83112_chip_init(void)
{

	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n",  __func__,  private_ts->chip_cell_type);
	IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
	/* Himax: Set FW and CFG Flash Address */
	FW_VER_MAJ_FLASH_ADDR  = 49157;  /* 0x00C005 */
	FW_VER_MAJ_FLASH_LENG  = 1;
	FW_VER_MIN_FLASH_ADDR  = 49158;  /* 0x00C006 */
	FW_VER_MIN_FLASH_LENG  = 1;
	CFG_VER_MAJ_FLASH_ADDR = 49408;  /* 0x00C100 */
	CFG_VER_MAJ_FLASH_LENG = 1;
	CFG_VER_MIN_FLASH_ADDR = 49409;  /* 0x00C101 */
	CFG_VER_MIN_FLASH_LENG = 1;
	CID_VER_MAJ_FLASH_ADDR = 49154;  /* 0x00C002 */
	CID_VER_MAJ_FLASH_LENG = 1;
	CID_VER_MIN_FLASH_ADDR = 49155;  /* 0x00C003 */
	CID_VER_MIN_FLASH_LENG = 1;

#ifdef HX_AUTO_UPDATE_FW
	g_i_FW_VER = (i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR] << 8) | i_CTPM_FW[FW_VER_MIN_FLASH_ADDR];
	g_i_CFG_VER = (i_CTPM_FW[CFG_VER_MAJ_FLASH_ADDR] << 8) | i_CTPM_FW[CFG_VER_MIN_FLASH_ADDR];
	g_i_CID_MAJ = i_CTPM_FW[CID_VER_MAJ_FLASH_ADDR];
	g_i_CID_MIN = i_CTPM_FW[CID_VER_MIN_FLASH_ADDR];
#endif
}

#ifdef CONFIG_CHIP_DTCFG
static int himax_hx83112_probe(struct platform_device *pdev)
{
	I("%s:Enter\n", __func__);
	g_core_fp.fp_chip_detect = hx83112_chip_detect;
	g_core_fp.fp_chip_init = hx83112_chip_init;
	return 0;
}

static int himax_hx83112_remove(struct platform_device *pdev)
{
	g_core_fp.fp_chip_detect = NULL;
	g_core_fp.fp_chip_init = NULL;
	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id himax_hx83112_mttable[] = {
	{ .compatible = "himax,hx83112"},
	{ },
};
#else
#define himax_hx83112_mttabl NULL
#endif

static struct platform_driver himax_hx83112_driver = {
	.probe	= himax_hx83112_probe,
	.remove	= himax_hx83112_remove,
	.driver	= {
		.name           = "HIMAX_HX83112",
		.owner          = THIS_MODULE,
		.of_match_table = himax_hx83112_mttable,
	},
};

static int __init himax_hx83112_init(void)
{
	I("%s\n", __func__);
	platform_driver_register(&himax_hx83112_driver);
	return 0;
}

static void __exit himax_hx83112_exit(void)
{
	platform_driver_unregister(&himax_hx83112_driver);
}

#else
static int himax_hx83112_probe(void)
{
	I("%s:Enter\n", __func__);

	g_core_fp.fp_chip_detect = hx83112_chip_detect;
	g_core_fp.fp_chip_init = hx83112_chip_init;

	return 0;
}

static int himax_hx83112_remove(void)
{
	g_core_fp.fp_chip_detect = NULL;
	g_core_fp.fp_chip_init = NULL;
	return 0;
}

static int __init himax_hx83112_init(void)
{
	int ret = 0;

	I("%s\n", __func__);
	ret = himax_hx83112_probe();
	return 0;
}

static void __exit himax_hx83112_exit(void)
{
	himax_hx83112_remove();
}
#endif

module_init(himax_hx83112_init);
module_exit(himax_hx83112_exit);

MODULE_DESCRIPTION("HIMAX HX83112 touch driver");
MODULE_LICENSE("GPL v2");


