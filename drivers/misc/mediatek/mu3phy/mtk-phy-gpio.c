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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/phy/phy.h>
#include <dt-bindings/phy/phy.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "mtk-phy.h"

#define U3_PHY_LIB
#include "mtk-phy.h"
#undef U3_PHY_LIB

#ifdef CONFIG_C60802_SUPPORT
#include "mtk-phy-c60802.h"
#endif
#ifdef CONFIG_D60802_SUPPORT
#include "mtk-phy-d60802.h"
#endif
#ifdef CONFIG_E60802_SUPPORT
#include "mtk-phy-e60802.h"
#endif
#ifdef CONFIG_A60810_SUPPORT
#include "mtk-phy-a60810.h"
#endif
#ifdef CONFIG_PROJECT_PHY
#include "mtk-phy-asic.h"
#endif

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT

#ifdef CONFIG_C60802_SUPPORT
static const struct u3phy_operator c60802_operators = {
	.init = phy_init_c60802,
	.change_pipe_phase = phy_change_pipe_phase_c60802,
	.eyescan_init = eyescan_init_c60802,
	.eyescan = phy_eyescan_c60802,
	.u2_connect = u2_connect_c60802,
	.u2_disconnect = u2_disconnect_c60802,
	.u2_save_current_entry = u2_save_cur_en_c60802,
	.u2_save_current_recovery = u2_save_cur_re_c60802,
	.u2_slew_rate_calibration = u2_slew_rate_calibration_c60802,
};
#endif
#ifdef CONFIG_D60802_SUPPORT
static const struct u3phy_operator d60802_operators = {
	.init = phy_init_d60802,
	.change_pipe_phase = phy_change_pipe_phase_d60802,
	.eyescan_init = eyescan_init_d60802,
	.eyescan = phy_eyescan_d60802,
	.u2_connect = u2_connect_d60802,
	.u2_disconnect = u2_disconnect_d60802,
	/* .u2_save_current_entry = u2_save_cur_en_d60802, */
	/* .u2_save_current_recovery = u2_save_cur_re_d60802, */
	.u2_slew_rate_calibration = u2_slew_rate_calibration_d60802,
};
#endif
#ifdef CONFIG_E60802_SUPPORT
static const struct u3phy_operator e60802_operators = {
	.init = phy_init_e60802,
	.change_pipe_phase = phy_change_pipe_phase_e60802,
	.eyescan_init = eyescan_init_e60802,
	.eyescan = phy_eyescan_e60802,
	.u2_connect = u2_connect_e60802,
	.u2_disconnect = u2_disconnect_e60802,
	/* .u2_save_current_entry = u2_save_cur_en_e60802, */
	/* .u2_save_current_recovery = u2_save_cur_re_e60802, */
	.u2_slew_rate_calibration = u2_slew_rate_calibration_e60802,
};
#endif
#ifdef CONFIG_A60810_SUPPORT
static const struct u3phy_operator a60810_operators = {
	.init = phy_init_a60810,
	.change_pipe_phase = phy_change_pipe_phase_a60810,
	.eyescan_init = eyescan_init_a60810,
	.eyescan = phy_eyescan_a60810,
	.u2_connect = u2_connect_a60810,
	.u2_disconnect = u2_disconnect_a60810,
	/* .u2_save_current_entry = u2_save_cur_en_a60810, */
	/* .u2_save_current_recovery = u2_save_cur_re_a60810, */
	.u2_slew_rate_calibration = u2_slew_rate_calibration_a60810,
};
#endif

#ifdef CONFIG_PROJECT_PHY
static struct u3phy_operator project_operators = {
	.init = phy_init_soc,
	.u2_slew_rate_calibration = u2_slew_rate_calibration,
};
#endif


static struct i2c_client *usb_i2c_client;
#define U3_PHY_PAGE 0xff

static void USB_PHY_Write_Register8(u8 var, u8 addr)
{
	char buffer[2];

	if (usb_i2c_client) {
		buffer[0] = addr;
		buffer[1] = var;
		i2c_master_send(usb_i2c_client, buffer, 2);
	}
}

static u8 USB_PHY_Read_Register8(u8 addr)
{
	u8 var;

	var = 0;
	if (usb_i2c_client) {
		i2c_master_send(usb_i2c_client, &addr, 1);
		i2c_master_recv(usb_i2c_client, &var, 1);
	}
	return var;
}

static void _u3_write_bank(u32 value)
{
	USB_PHY_Write_Register8((u8)value, (u8)U3_PHY_PAGE);
}

static u32 _u3_read_reg(u32 address)
{
	u8 databuffer = 0;

	databuffer = USB_PHY_Read_Register8((u8)address);
	return databuffer;
}

static void _u3_write_reg(u32 address, u32 value)
{
	USB_PHY_Write_Register8((u8)value, (u8)address);
}

static u32 u3_phy_read_reg32(u32 addr)
{
	u32 bank;
	u32 addr8;
	u32 data;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;

	_u3_write_bank(bank);
	data = _u3_read_reg(addr8);
	data |= (_u3_read_reg(addr8 + 1) << 8);
	data |= (_u3_read_reg(addr8 + 2) << 16);
	data |= (_u3_read_reg(addr8 + 3) << 24);
	return data;
}

static u32 u3_phy_write_reg32(u32 addr, u32 data)
{
	u32 bank;
	u32 addr8;
	u32 data_0, data_1, data_2, data_3;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	data_0 = data & 0xff;
	data_1 = (data >> 8) & 0xff;
	data_2 = (data >> 16) & 0xff;
	data_3 = (data >> 24) & 0xff;

	_u3_write_bank(bank);
	_u3_write_reg(addr8, data_0);
	_u3_write_reg(addr8 + 1, data_1);
	_u3_write_reg(addr8 + 2, data_2);
	_u3_write_reg(addr8 + 3, data_3);

	return 0;
}

#if 0
static void u3_phy_write_field32(int addr, int offset, int mask, int value)
{
	u32 cur_value;
	u32 new_value;

	cur_value = u3_phy_read_reg32(addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	u3_phy_write_reg32(addr, new_value);
}
#endif

static u32 u3_phy_write_reg8(u32 addr, u8 data)
{
	u32 bank;
	u32 addr8;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	_u3_write_bank(bank);
	_u3_write_reg(addr8, data);

	return 0;
}


static int a60810_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
#ifndef CONFIG_PROJECT_PHY
	PHY_INT32 u3phy_version;
#endif
	usb_i2c_client = client;

	pr_notice("%s\n", __func__);

	if (u3phy != NULL)
		return PHY_TRUE;

	u3phy = kmalloc(sizeof(struct u3phy_info), GFP_NOIO);
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	u3phy->phyd_version_addr = 0x2000e4;
#else
	u3phy->phyd_version_addr = U3_PHYD_B2_BASE + 0xe4;
#endif
	u3phy_ops = NULL;

#ifdef CONFIG_PROJECT_PHY
	u3phy->u2phy_regs_e = (struct u2phy_reg_e *)U2_PHY_BASE;
	u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)U3_PHYD_BASE;
	u3phy->u3phyd_bank2_regs_e =
		(struct u3phyd_bank2_reg_e *)U3_PHYD_B2_BASE;
	u3phy->u3phya_regs_e = (struct u3phya_reg_e *)U3_PHYA_BASE;
	u3phy->u3phya_da_regs_e = (struct u3phya_da_reg_e *)U3_PHYA_DA_BASE;
	u3phy->sifslv_chip_regs_e =
		(struct sifslv_chip_reg_e *)SIFSLV_CHIP_BASE;
	u3phy->spllc_regs_e = (struct spllc_reg_e *)SIFSLV_SPLLC_BASE;
	u3phy->sifslv_fm_regs_e = (struct sifslv_fm_feg_e *)SIFSLV_FM_FEG_BASE;
	u3phy_ops = (struct u3phy_operator *)&project_operators;
#else

	/* parse phy version */
	u3phy_version = U3PhyReadReg32(u3phy->phyd_version_addr);
	pr_notice("phy version: %x\n", u3phy_version);
	u3phy->phy_version = u3phy_version;

	if (u3phy_version == 0xc60802a) {
#ifdef CONFIG_C60802_SUPPORT
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_c = (struct u2phy_reg_c *)0x0;
		u3phy->u3phyd_regs_c = (struct u3phyd_reg_c *)0x100000;
		u3phy->u3phyd_bank2_regs_c =
			(struct u3phyd_bank2_reg_c *)0x200000;
		u3phy->u3phya_regs_c = (struct u3phya_reg_c *)0x300000;
		u3phy->u3phya_da_regs_c = (struct u3phya_da_reg_c *)0x400000;
		u3phy->sifslv_chip_regs_c =
			(struct sifslv_chip_reg_c *)0x500000;
		u3phy->sifslv_fm_regs_c = (struct sifslv_fm_feg_c *)0xf00000;
#else
		u3phy->u2phy_regs_c = (struct u2phy_reg_c *)U2_PHY_BASE;
		u3phy->u3phyd_regs_c = (struct u3phyd_reg_c *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_c =
			(struct u3phyd_bank2_reg_c *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_c = (struct u3phya_reg_c *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_c =
			(struct u3phya_da_reg_c *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_c =
			(struct sifslv_chip_reg_c *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_c =
			(struct sifslv_fm_feg_c *)SIFSLV_FM_FEG_BASE;
#endif
		u3phy_ops = (struct u3phy_operator *)&c60802_operators;
#endif
	} else if (u3phy_version == 0xd60802a) {
#ifdef CONFIG_D60802_SUPPORT
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_d = (struct u2phy_reg_d *)0x0;
		u3phy->u3phyd_regs_d = (struct u3phyd_reg_d *)0x100000;
		u3phy->u3phyd_bank2_regs_d =
			(struct u3phyd_bank2_reg_d *)0x200000;
		u3phy->u3phya_regs_d = (struct u3phya_reg_d *)0x300000;
		u3phy->u3phya_da_regs_d = (struct u3phya_da_reg_d *)0x400000;
		u3phy->sifslv_chip_regs_d =
			(struct sifslv_chip_reg_d *)0x500000;
		u3phy->sifslv_fm_regs_d = (struct sifslv_fm_feg_d *)0xf00000;
#else
		u3phy->u2phy_regs_d = (struct u2phy_reg_d *)U2_PHY_BASE;
		u3phy->u3phyd_regs_d = (struct u3phyd_reg_d *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_d =
			(struct u3phyd_bank2_reg_d *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_d = (struct u3phya_reg_d *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_d =
			(struct u3phya_da_reg_d *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_d =
			(struct sifslv_chip_reg_d *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_d =
			(struct sifslv_fm_feg_d *)SIFSLV_FM_FEG_BASE;
#endif
		u3phy_ops = ((struct u3phy_operator *)&d60802_operators);
#endif
	} else if (u3phy_version == 0xe60802a) {
#ifdef CONFIG_E60802_SUPPORT
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_e = (struct u2phy_reg_e *)0x0;
		u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)0x100000;
		u3phy->u3phyd_bank2_regs_e =
			(struct u3phyd_bank2_reg_e *)0x200000;
		u3phy->u3phya_regs_e = (struct u3phya_reg_e *)0x300000;
		u3phy->u3phya_da_regs_e = (struct u3phya_da_reg_e *)0x400000;
		u3phy->sifslv_chip_regs_e =
			(struct sifslv_chip_reg_e *)0x500000;
		u3phy->spllc_regs_e = (struct spllc_reg_e *)0x600000;
		u3phy->sifslv_fm_regs_e = (struct sifslv_fm_feg_e *)0xf00000;
#else
		u3phy->u2phy_regs_e = (struct u2phy_reg_e *)U2_PHY_BASE;
		u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_e =
			(struct u3phyd_bank2_reg_e *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_e = (struct u3phya_reg_e *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_e =
			(struct u3phya_da_reg_e *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_e =
			(struct sifslv_chip_reg_e *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_e =
			(struct sifslv_fm_feg_e *)SIFSLV_FM_FEG_BASE;
#endif
		u3phy_ops = ((struct u3phy_operator *)&e60802_operators);
#endif
	} else if (u3phy_version == 0xa60810a) {
#ifdef CONFIG_A60810_SUPPORT
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_a = (struct u2phy_reg_a *)0x0;
		u3phy->u3phyd_regs_a = (struct u3phyd_reg_a *)0x100000;
		u3phy->u3phyd_bank2_regs_a =
			(struct u3phyd_bank2_reg_a *)0x200000;
		u3phy->u3phya_regs_a = (struct u3phya_reg_a *)0x300000;
		u3phy->u3phya_da_regs_a = (struct u3phya_da_reg_a *)0x400000;
		u3phy->sifslv_chip_regs_a =
			(struct sifslv_chip_reg_a *)0x500000;
		u3phy->spllc_regs_a = (struct spllc_reg_a *)0x600000;
		u3phy->sifslv_fm_regs_a = (struct sifslv_fm_reg_a *)0xf00000;
#else
		u3phy->u2phy_regs_a = (struct u2phy_reg_a *)U2_PHY_BASE;
		u3phy->u3phyd_regs_a = (struct u3phyd_reg_a *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_a =
			(struct u3phyd_bank2_reg_a *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_a = (struct u3phya_reg_a *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_a =
			(struct u3phya_da_reg_a *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_a =
			(struct sifslv_chip_reg_a *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_a =
			(struct sifslv_fm_reg_a *)SIFSLV_FM_FEG_BASE;
#endif
		u3phy_ops = ((struct u3phy_operator *)&a60810_operators);
#endif
	} else {
		pr_err("No match phy version, version: %x\n", u3phy_version);
		return PHY_FALSE;
	}

#endif

	if (!u3phy_ops)
		return 1;
	else
		return 0;
}
static int a60810_i2c_remove(struct i2c_client *client)
{

	return 0;
}

static struct i2c_device_id usb_i2c_id[] = { {"mtk-a60810", 2}, {} };
struct i2c_board_info usb_i2c_dev = {
	I2C_BOARD_INFO("mtk-a60810", 0x60)
};

static const struct of_device_id a60810_match_table[] = {
	{.compatible = "mediatek,mtk-a60810",},
	{},
};

struct i2c_driver a60810_driver = {
	.driver = {
		.name = "mtk-a60810",
		.owner = THIS_MODULE,
		.of_match_table = a60810_match_table,
	},
	.probe = a60810_i2c_probe,
	.remove = a60810_i2c_remove,
	.id_table = usb_i2c_id,
};

static int mtk_phy_drv_init(void)
{
	pr_notice("%s\n", __func__);

	i2c_register_board_info(2, &usb_i2c_dev, 1);
	if ((i2c_add_driver(&a60810_driver)) !=  0)
		pr_notice("%s usb_i2c_driver init failed!!\n", __func__);

	pr_notice("%s usb_i2c_driver init succeed!!\n", __func__);

	return 0;
}

module_init(mtk_phy_drv_init);

PHY_INT32 U3PhyWriteReg32(PHY_UINT32 addr, PHY_UINT32 data)
{
	u3_phy_write_reg32(addr, data);

	return 0;
}

PHY_INT32 U3PhyReadReg32(PHY_UINT32 addr)
{
	PHY_INT32 data;

	data = u3_phy_read_reg32(addr);

	return data;
}

PHY_INT32 U3PhyWriteReg8(PHY_UINT32 addr, PHY_UINT8 data)
{
	u3_phy_write_reg8(addr, data);

	return PHY_TRUE;
}

PHY_INT8 U3PhyReadReg8(PHY_UINT32 addr)
{
	PHY_INT32 data;

	data = _u3_read_reg(addr);

	return data;
}

PHY_INT32 _U3Write_Reg(PHY_INT32 address, PHY_INT32 value)
{
	U3PhyWriteReg8(address, value);
	return PHY_TRUE;
}

PHY_INT32 _U3Read_Reg(PHY_INT32 address)
{
	return U3PhyReadReg8(address);
}

#if 0
/* TEST CHIP PHY define, edit this in different platform */
#define U3_PHY_I2C_DEV			0x60
#define U3_PHY_PAGE			0xff
#define GPIO_BASE			(u3_sif_base + 0x700) /* 0xf0044700 */
#define SSUSB_I2C_OUT			(GPIO_BASE + 0xd0)
#define SSUSB_I2C_IN			(GPIO_BASE + 0xd4)

#ifdef NEVER /*USE_GPIO */

/* /////////////////////////////////////////////////////////////// */

#define OUTPUT		1
#define INPUT		0

#define SDA		0	/* / GPIO #0: I2C data pin */
#define SCL		1	/* / GPIO #1: I2C clock pin */

/* /////////////////////////////////////////////////////////////// */

#define SDA_OUT		(1<<0)
#define SDA_OEN		(1<<1)
#define SCL_OUT		(1<<2)
#define SCL_OEN		(1<<3)

#define SDA_IN_OFFSET		0
#define SCL_IN_OFFSET		1

/* #define       GPIO_PULLEN1_SET        (GPIO_BASE+0x0030+0x04) */
/* #define       GPIO_DIR1_SET           (GPIO_BASE+0x0000+0x04) */
/* #define       GPIO_PULLEN1_CLR        (GPIO_BASE+0x0030+0x08) */
/* #define       GPIO_DIR1_CLR           (GPIO_BASE+0x0000+0x08) */
/* #define       GPIO_DOUT1_SET          (GPIO_BASE+0x00C0+0x04) */
/* #define       GPIO_DOUT1_CLR          (GPIO_BASE+0x00C0+0x08) */
/* #define       GPIO_DIN1               (GPIO_BASE+0x00F0) */

void gpio_dir_set(PHY_INT32 pin)
{
	PHY_INT32 temp;
	u64 addr;

	addr = (u64)SSUSB_I2C_OUT;
	temp = DRV_Reg32(addr);
	if (pin == SDA) {
		temp |= SDA_OEN;
		DRV_WriteReg32(addr, temp);
	} else {
		temp |= SCL_OEN;
		DRV_WriteReg32(addr, temp);
	}
}

void gpio_dir_clr(PHY_INT32 pin)
{
	PHY_INT32 temp;
	u64 addr;

	addr = (u64)SSUSB_I2C_OUT;
	temp = DRV_Reg32(addr);
	if (pin == SDA) {
		temp &= ~SDA_OEN;
		DRV_WriteReg32(addr, temp);
	} else {
		temp &= ~SCL_OEN;
		DRV_WriteReg32(addr, temp);
	}
}

void gpio_dout_set(PHY_INT32 pin)
{
	PHY_INT32 temp;
	u64 addr;

	addr = (u64)SSUSB_I2C_OUT;
	temp = DRV_Reg32(addr);
	if (pin == SDA) {
		temp |= SDA_OUT;
		DRV_WriteReg32(addr, temp);
	} else {
		temp |= SCL_OUT;
		DRV_WriteReg32(addr, temp);
	}
}

void gpio_dout_clr(PHY_INT32 pin)
{
	PHY_INT32 temp;
	u64 addr;

	addr = (u64)SSUSB_I2C_OUT;
	temp = DRV_Reg32(addr);
	if (pin == SDA) {
		temp &= ~SDA_OUT;
		DRV_WriteReg32(addr, temp);
	} else {
		temp &= ~SCL_OUT;
		DRV_WriteReg32(addr, temp);
	}
}

PHY_INT32 gpio_din(PHY_INT32 pin)
{
	PHY_INT32 temp;
	u64 addr;

	addr = (u64)SSUSB_I2C_IN;
	temp = DRV_Reg32(addr);
	if (pin == SDA)
		temp = (temp >> SDA_IN_OFFSET) & 1;
	else
		temp = (temp >> SCL_IN_OFFSET) & 1;
	return temp;
}

/* #define     GPIO_PULLEN_SET(_no)  (GPIO_PULLEN1_SET+(0x10*(_no))) */
#define     GPIO_DIR_SET(pin)   gpio_dir_set(pin)
#define     GPIO_DOUT_SET(pin)  gpio_dout_set(pin)
/* #define     GPIO_PULLEN_CLR(_no) (GPIO_PULLEN1_CLR+(0x10*(_no))) */
#define     GPIO_DIR_CLR(pin)   gpio_dir_clr(pin)
#define     GPIO_DOUT_CLR(pin)  gpio_dout_clr(pin)
#define     GPIO_DIN(pin)       gpio_din(pin)


PHY_UINT32 i2c_dummy_cnt;

#define I2C_DELAY 10
#define I2C_DUMMY_DELAY(_delay) for (i2c_dummy_cnt = ((_delay)); i2c_dummy_cnt != 0; i2c_dummy_cnt--)

void GPIO_InitIO(PHY_UINT32 dir, PHY_UINT32 pin)
{
	if (dir == OUTPUT) {
		/* DRV_WriteReg16(GPIO_PULLEN_SET(no),(1 << remainder)); */
		GPIO_DIR_SET(pin);
	} else {
		/* DRV_WriteReg16(GPIO_PULLEN_CLR(no),(1 << remainder)); */
		GPIO_DIR_CLR(pin);
	}
	I2C_DUMMY_DELAY(100);
}

void GPIO_WriteIO(PHY_UINT32 data, PHY_UINT32 pin)
{
	if (data == 1)
		GPIO_DOUT_SET(pin);
	else
		GPIO_DOUT_CLR(pin);
}

PHY_UINT32 GPIO_ReadIO(PHY_UINT32 pin)
{
	PHY_UINT16 data;

	data = GPIO_DIN(pin);
	return (PHY_UINT32) data;
}


void SerialCommStop(void)
{
	GPIO_InitIO(OUTPUT, SDA);
	GPIO_WriteIO(0, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(0, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_InitIO(INPUT, SCL);
	GPIO_InitIO(INPUT, SDA);
}

void SerialCommStart(void)
{				/* Prepare the SDA and SCL for sending/receiving */
	GPIO_InitIO(OUTPUT, SCL);
	GPIO_InitIO(OUTPUT, SDA);
	GPIO_WriteIO(1, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(0, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(0, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
}

PHY_UINT32 SerialCommTxByte(PHY_UINT8 data)
{				/* return 0 --> ack */
	PHY_INT32 i, ack;

	GPIO_InitIO(OUTPUT, SDA);

	for (i = 8; --i > 0;) {
		GPIO_WriteIO((data >> i) & 0x01, SDA);
		I2C_DUMMY_DELAY(I2C_DELAY);
		GPIO_WriteIO(1, SCL);	/* high */
		I2C_DUMMY_DELAY(I2C_DELAY);
		GPIO_WriteIO(0, SCL);	/* low */
		I2C_DUMMY_DELAY(I2C_DELAY);
	}
	GPIO_WriteIO((data >> i) & 0x01, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SCL);	/* high */
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(0, SCL);	/* low */
	I2C_DUMMY_DELAY(I2C_DELAY);

	GPIO_WriteIO(0, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_InitIO(INPUT, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	ack = GPIO_ReadIO(SDA);	/* / ack 1: error , 0:ok */
	GPIO_WriteIO(0, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);

	if (ack == 1)
		return PHY_FALSE;
	else
		return PHY_TRUE;
}

void SerialCommRxByte(PHY_UINT8 *data, PHY_UINT8 ack)
{
	PHY_INT32 i;
	PHY_UINT32 dataCache;

	dataCache = 0;
	GPIO_InitIO(INPUT, SDA);
	for (i = 8; --i >= 0;) {
		dataCache <<= 1;
		I2C_DUMMY_DELAY(I2C_DELAY);
		GPIO_WriteIO(1, SCL);
		I2C_DUMMY_DELAY(I2C_DELAY);
		dataCache |= GPIO_ReadIO(SDA);
		GPIO_WriteIO(0, SCL);
		I2C_DUMMY_DELAY(I2C_DELAY);
	}
	GPIO_InitIO(OUTPUT, SDA);
	GPIO_WriteIO(ack, SDA);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(1, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	GPIO_WriteIO(0, SCL);
	I2C_DUMMY_DELAY(I2C_DELAY);
	*data = (unsigned char)dataCache;
}

PHY_INT32 I2cWriteReg(PHY_UINT8 dev_id, PHY_UINT8 Addr, PHY_UINT8 Data)
{
	PHY_INT32 acknowledge = 0;

	SerialCommStart();
	acknowledge = SerialCommTxByte((dev_id << 1) & 0xff);
	if (acknowledge)
		acknowledge = SerialCommTxByte(Addr);
	else
		return PHY_FALSE;
	acknowledge = SerialCommTxByte(Data);
	if (acknowledge) {
		SerialCommStop();
		return PHY_FALSE;
	} else {
		return PHY_TRUE;
	}
}

PHY_INT32 I2cReadReg(PHY_UINT8 dev_id, PHY_UINT8 Addr, PHY_UINT8 *Data)
{
	PHY_INT32 acknowledge = 0;

	SerialCommStart();
	acknowledge = SerialCommTxByte((dev_id << 1) & 0xff);
	if (acknowledge)
		acknowledge = SerialCommTxByte(Addr);
	else
		return PHY_FALSE;
	SerialCommStart();
	acknowledge = SerialCommTxByte(((dev_id << 1) & 0xff) | 0x01);
	if (acknowledge)
		SerialCommRxByte(Data, 1);	/* ack 0: ok , 1 error */
	else
		return PHY_FALSE;
	SerialCommStop();
	return acknowledge;
}

#else				/* Use I2C controller */

#define REG_I2C_START_BIT	    0x1
#define I2C_READ_BIT         0x1

#define PHY_I2C_BASE      (i2c_base)

/* "volatile" type class should not be used, see volatile-considered-harmful.txt */
#define REG_I2C_DATA_PORT    (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x00)))
#define REG_I2C_SLAVE_ADDR   (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x04)))
#define REG_I2C_TRANSFER_LEN (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x14)))
#define REG_I2C_START        (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x24)))
#define REG_I2C_SOFT_RESET   (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x50)))
#define REG_I2C_CONTROL		 (*((volatile unsigned short int *) (PHY_I2C_BASE + 0x10)))

#define IS_PRINT 0

PHY_INT32 I2cWriteReg(PHY_UINT8 dev_id, PHY_UINT8 addr, PHY_UINT8 val)
{
	if (IS_PRINT)
		pr_info("I2C Write@%x [%x]=%x\n", dev_id, addr, val);

	REG_I2C_SLAVE_ADDR = dev_id << 1;
	REG_I2C_TRANSFER_LEN = 2;

	REG_I2C_DATA_PORT = addr;
	REG_I2C_DATA_PORT = val;

	REG_I2C_START = REG_I2C_START_BIT;

	while ((REG_I2C_START & REG_I2C_START_BIT))
		;

	return PHY_TRUE;
}

PHY_INT32 I2cReadReg(PHY_UINT8 dev_id, PHY_UINT8 addr, PHY_UINT8 *data)
{
	if (IS_PRINT)
		pr_info("I2C Read@%x [%x]\n", dev_id, addr);

	REG_I2C_SLAVE_ADDR = dev_id << 1;
	REG_I2C_TRANSFER_LEN = 0x01;
	REG_I2C_DATA_PORT = addr;
	REG_I2C_START = REG_I2C_START_BIT;

	while ((REG_I2C_START & REG_I2C_START_BIT))
		;

	REG_I2C_SLAVE_ADDR = (dev_id << 1) | I2C_READ_BIT;
	REG_I2C_TRANSFER_LEN = 0x01;
	REG_I2C_START = REG_I2C_START_BIT;

	while ((REG_I2C_START & REG_I2C_START_BIT))
		;

	*data = REG_I2C_DATA_PORT;

	if (IS_PRINT)
		pr_info("I2C Read [%x]=%x\n", addr, *data);

	return PHY_TRUE;	/* !!(PHY_INT32)*data; */
}
#endif

void _U3_Write_Bank(PHY_INT32 bankValue)
{
	I2cWriteReg(U3_PHY_I2C_DEV, U3_PHY_PAGE, bankValue);
}

PHY_INT32 _U3Write_Reg(PHY_INT32 address, PHY_INT32 value)
{
	I2cWriteReg(U3_PHY_I2C_DEV, address, value);
	return PHY_TRUE;
}

PHY_INT32 _U3Read_Reg(PHY_INT32 address)
{
	PHY_INT8 *pu1Buf;
	PHY_INT32 ret;

	pu1Buf = kmalloc(1, GFP_NOIO);
	ret = I2cReadReg(U3_PHY_I2C_DEV, address, pu1Buf);
	if (ret == PHY_FALSE) {
		pr_err("Read failed\n");
		return PHY_FALSE;
	}
	ret = (char)pu1Buf[0];
	kfree(pu1Buf);
	return ret;

}

PHY_INT32 U3PhyWriteReg32(PHY_UINT32 addr, PHY_UINT32 data)
{
	PHY_INT32 bank;
	PHY_INT32 addr8;
	PHY_INT32 data_0, data_1, data_2, data_3;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	data_0 = data & 0xff;
	data_1 = (data >> 8) & 0xff;
	data_2 = (data >> 16) & 0xff;
	data_3 = (data >> 24) & 0xff;

	pr_debug("addr: %x, data: %x\n", addr8, data);
	_U3_Write_Bank(bank);
	pr_debug("addr: %x, data: %x\n", addr8, data_0);
	_U3Write_Reg(addr8, data_0);
	pr_debug("addr: %x, data: %x\n", addr8 + 1, data_1);
	_U3Write_Reg(addr8 + 1, data_1);
	pr_debug("addr: %x, data: %x\n", addr8 + 2, data_2);
	_U3Write_Reg(addr8 + 2, data_2);
	pr_debug("addr: %x, data: %x\n", addr8 + 3, data_3);
	_U3Write_Reg(addr8 + 3, data_3);

	return 0;
}

PHY_INT32 U3PhyReadReg32(PHY_UINT32 addr)
{
	PHY_INT32 bank;
	PHY_INT32 addr8;
	PHY_INT32 data;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;

	_U3_Write_Bank(bank);
	data = _U3Read_Reg(addr8);
	data |= (_U3Read_Reg(addr8 + 1) << 8);
	data |= (_U3Read_Reg(addr8 + 2) << 16);
	data |= (_U3Read_Reg(addr8 + 3) << 24);

	return data;
}

PHY_INT32 U3PhyWriteReg8(PHY_UINT32 addr, PHY_UINT8 data)
{
	PHY_INT32 bank;
	PHY_INT32 addr8;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	_U3_Write_Bank(bank);
	_U3Write_Reg(addr8, data);

	pr_debug("addr: %x, data: %x\n", addr8, data);

	return PHY_TRUE;
}

PHY_INT8 U3PhyReadReg8(PHY_UINT32 addr)
{
	PHY_INT32 bank;
	PHY_INT32 addr8;
	PHY_INT32 data;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	_U3_Write_Bank(bank);
	data = _U3Read_Reg(addr8);

	return data;
}
#endif

#endif
