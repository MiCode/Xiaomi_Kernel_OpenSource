// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/phy/phy.h>
#include "phy-mtk-fpga.h"

/* ------------------------ I2C IO API ------------------------------ */

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
static inline void i2c_dummy_delay(int count)
{
	udelay(count);
}

static void gpio_set_direction(void __iomem *port,
		enum i2c_pin pin, enum i2c_dir dir)
{
	void __iomem  *addr;
	u32 temp;

	addr = port + SSUSB_FPGA_I2C_OUT;
	temp = readl(addr);

	if (pin == I2C_SDA) {
		if (dir == I2C_OUTPUT)
			temp |= SSUSB_FPGA_I2C_SDA_OEN;
		else
			temp &= ~SSUSB_FPGA_I2C_SDA_OEN;
	} else {
		if (dir == I2C_OUTPUT)
			temp |= SSUSB_FPGA_I2C_SCL_OEN;
		else
			temp &= ~SSUSB_FPGA_I2C_SCL_OEN;
	}
	writel(temp, addr);
}

static void gpio_set_value(void __iomem *port,
		enum i2c_pin pin, bool value)
{
	void __iomem  *addr;
	u32 temp;

	addr = port + SSUSB_FPGA_I2C_OUT;
	temp = readl(addr);

	if (pin == I2C_SDA) {
		if (value)
			temp |= SSUSB_FPGA_I2C_SDA_OUT;
		else
			temp &= ~SSUSB_FPGA_I2C_SDA_OUT;
	} else {
		if (value)
			temp |= SSUSB_FPGA_I2C_SCL_OUT;
		else
			temp &= ~SSUSB_FPGA_I2C_SCL_OUT;
	}
	writel(temp, addr);
}

static int gpio_get_value(void __iomem *port, enum i2c_pin pin)
{
	void __iomem *addr;
	u32 temp;

	addr = port + SSUSB_FPGA_I2C_IN;
	temp = readl(addr);

	if (pin == I2C_SDA)
		temp &= SSUSB_FPGA_I2C_SDA_IN;
	else
		temp &= SSUSB_FPGA_I2C_SCL_IN;

	return !!temp;
}

static void i2c_stop(void __iomem *port)
{
	gpio_set_direction(port, I2C_SDA, I2C_OUTPUT);
	gpio_set_value(port, I2C_SCL, 0);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SDA, 0);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SDA, 1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_direction(port, I2C_SCL, I2C_INPUT);
	gpio_set_direction(port, I2C_SDA, I2C_INPUT);
}

/* Prepare the I2C_SDA and I2C_SCL for sending/receiving */
static void i2c_start(void __iomem *port)
{
	gpio_set_direction(port, I2C_SCL, I2C_OUTPUT);
	gpio_set_direction(port, I2C_SDA, I2C_OUTPUT);
	gpio_set_value(port, I2C_SDA, 1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SDA, 0);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 0);
	i2c_dummy_delay(I2C_DELAY);
}

/* return 0 --> ack */
static u32 i2c_send_byte(void __iomem *port, u8 data)
{
	int i, ack;

	gpio_set_direction(port, I2C_SDA, I2C_OUTPUT);

	for (i = 8; --i > 0;) {
		gpio_set_value(port, I2C_SDA, (data >> i) & 0x1);
		i2c_dummy_delay(I2C_DELAY);
		gpio_set_value(port, I2C_SCL,  1);
		i2c_dummy_delay(I2C_DELAY);
		gpio_set_value(port, I2C_SCL,  0);
		i2c_dummy_delay(I2C_DELAY);
	}
	gpio_set_value(port, I2C_SDA, (data >> i) & 0x1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL,  1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL,  0);
	i2c_dummy_delay(I2C_DELAY);

	gpio_set_value(port, I2C_SDA, 0);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_direction(port, I2C_SDA, I2C_INPUT);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 1);
	i2c_dummy_delay(I2C_DELAY);
	/* ack 1: error, 0:ok */
	ack = gpio_get_value(port, I2C_SDA);
	gpio_set_value(port, I2C_SCL, 0);
	i2c_dummy_delay(I2C_DELAY);

	return (ack == 1) ? PHY_FALSE : PHY_TRUE;
}

static void i2c_receive_byte(void __iomem *port, u8 *data, u8 ack)
{
	int i;
	u32 dataCache = 0;

	gpio_set_direction(port, I2C_SDA, I2C_INPUT);

	for (i = 8; --i >= 0;) {
		dataCache <<= 1;
		i2c_dummy_delay(I2C_DELAY);
		gpio_set_value(port, I2C_SCL, 1);
		i2c_dummy_delay(I2C_DELAY);
		dataCache |= gpio_get_value(port, I2C_SDA);
		gpio_set_value(port, I2C_SCL, 0);
		i2c_dummy_delay(I2C_DELAY);
	}
	gpio_set_direction(port, I2C_SDA, I2C_OUTPUT);
	gpio_set_value(port, I2C_SDA, ack);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 1);
	i2c_dummy_delay(I2C_DELAY);
	gpio_set_value(port, I2C_SCL, 0);
	i2c_dummy_delay(I2C_DELAY);
	*data = (u8)dataCache;
}

static int i2c_write_reg(void __iomem *port, u8 i2c_addr, u8 addr, u8 data)
{
	int ack = 0;

	i2c_start(port);

	ack = i2c_send_byte(port, (i2c_addr << 1) & 0xff);
	if (ack)
		ack = i2c_send_byte(port, addr);
	else
		return PHY_FALSE;

	ack = i2c_send_byte(port, data);
	if (ack) {
		i2c_stop(port);
		return PHY_FALSE;
	} else {
		return PHY_TRUE;
	}
}

static int i2c_read_reg(void __iomem *port, u8 i2c_addr, u8 addr, u8 *data)
{
	int ack = 0;

	i2c_start(port);

	ack = i2c_send_byte(port, (i2c_addr << 1) & 0xff);
	if (ack)
		ack = i2c_send_byte(port, addr);
	else
		return PHY_FALSE;

	i2c_start(port);

	ack = i2c_send_byte(port, ((i2c_addr << 1) & 0xff) | 0x1);
	/* ack 0: ok, 1 error */
	if (ack)
		i2c_receive_byte(port, data, 1);
	else
		return PHY_FALSE;

	i2c_stop(port);
	return ack;
}

int phy_writeb(void __iomem *port, u8 i2c_addr, u8 addr, u8 value)
{
	i2c_write_reg(port, i2c_addr, addr, value);
	return PHY_TRUE;
}

u8 phy_readb(void __iomem *port, u8 i2c_addr,  u8 addr)
{
	u8 buf;
	int ret;

	ret = i2c_read_reg(port, i2c_addr, addr, &buf);
	if (ret == PHY_FALSE) {
		pr_info("Read failed(i2c_addr: %d, addr: 0x%x)\n",
				i2c_addr, addr);
		return ret;
	}

	return buf;
}

static int phy_writel(void __iomem *port, u8 i2c_addr, u32 addr, u32 data)
{
	u8 addr8;
	u8 data_0, data_1, data_2, data_3;

	addr8 = addr & 0xff;
	data_0 = data & 0xff;
	data_1 = (data >> 8) & 0xff;
	data_2 = (data >> 16) & 0xff;
	data_3 = (data >> 24) & 0xff;

	phy_writeb(port, i2c_addr, addr8, data_0);
	phy_writeb(port, i2c_addr, addr8 + 1, data_1);
	phy_writeb(port, i2c_addr, addr8 + 2, data_2);
	phy_writeb(port, i2c_addr, addr8 + 3, data_3);

	return 0;
}

static u32 phy_readl(void __iomem *port, u8 i2c_addr, u32 addr)
{
	u8 addr8;
	u32 data;

	addr8 = addr & 0xff;

	data = phy_readb(port, i2c_addr, addr8);
	data |= (phy_readb(port, i2c_addr, addr8 + 1) << 8);
	data |= (phy_readb(port, i2c_addr, addr8 + 2) << 16);
	data |= (phy_readb(port, i2c_addr, addr8 + 3) << 24);

	return data;
}

static u32 __maybe_unused phy_readlmsk(void __iomem *port,	u8 i2c_addr,
		u32 reg_addr32, u32 offset, u32 mask)
{
	u32 value;

	value = phy_readl(port, i2c_addr, reg_addr32);
	return ((value & mask) >> offset);
}

static int phy_writelmsk(void __iomem *port, u8 i2c_addr,
		u32 reg_addr32, u32 offset, u32 mask, u32 data)
{
	u32 cur_value;
	u32 new_value;

	cur_value = phy_readl(port, i2c_addr, reg_addr32);
	new_value = (cur_value & (~mask)) | ((data << offset) & mask);
	phy_writel(port, i2c_addr, reg_addr32, new_value);

	return 0;
}
#else
unsigned int I2cWriteReg(unsigned char dev_id, unsigned char addr, unsigned char val)
{
	if (IS_PRINT)
		pr_info("I2C Write@%x [%x]=%x\n", dev_id, addr, val);

	writew((dev_id << 1), ADDR_I2C_SLAVE_ADDR);
	writew(2, ADDR_I2C_TRANSFER_LEN);

	/* ADDITIONAL CONTROL */
	writew(0x1, REG_I2C_TRANSAC_LEN);
	writew(0x1303, REG_I2C_HTIMING);
	writew(0x13C3, REG_I2C_LTIMING);

	writeb(addr, ADDR_I2C_DATA_PORT);
	writeb(val, ADDR_I2C_DATA_PORT);

	writew(REG_I2C_START_BIT, ADDR_I2C_START);

	while ((readw(ADDR_I2C_START) & REG_I2C_START_BIT))
		;

	return PHY_TRUE;
}

unsigned int I2cReadReg(unsigned char dev_id, unsigned char addr, unsigned char *data)
{
	if (IS_PRINT)
		pr_info("I2C Read@%x [%x]\n", dev_id, addr);

	writew((dev_id << 1), ADDR_I2C_SLAVE_ADDR);
	writew(1, ADDR_I2C_TRANSFER_LEN);

	/* ADDITIONAL CONTROL */
	writew(0x1, REG_I2C_TRANSAC_LEN);
	writew(0x1303, REG_I2C_HTIMING);
	writew(0x13C3, REG_I2C_LTIMING);

	writeb(addr, ADDR_I2C_DATA_PORT);
	writew(REG_I2C_START_BIT, ADDR_I2C_START);

	while ((readw(ADDR_I2C_START) & REG_I2C_START_BIT))
		;

	writew((dev_id << 1) | I2C_READ_BIT, ADDR_I2C_SLAVE_ADDR);
	writew(1, ADDR_I2C_TRANSFER_LEN);

	/* ADDITIONAL CONTROL */
	writew(0x1, REG_I2C_TRANSAC_LEN);
	writew(0x1303, REG_I2C_HTIMING);
	writew(0x13C3, REG_I2C_LTIMING);

	writew(REG_I2C_START_BIT, ADDR_I2C_START);

	while ((readw(ADDR_I2C_START) & REG_I2C_START_BIT))
		;

	*data = readb(ADDR_I2C_DATA_PORT);

	if (IS_PRINT)
		pr_info("I2C Read [%x]=%x\n", addr, *data);

	return PHY_TRUE;	/* !!(unsigned int)*data; */
}

void _U3_Write_Bank(unsigned int bankValue)
{
	I2cWriteReg(U3_PHY_I2C_DEV, U3_PHY_PAGE, bankValue);
}

unsigned int _U3Write_Reg(unsigned int address, unsigned int value)
{
	I2cWriteReg(U3_PHY_I2C_DEV, address, value);
	return PHY_TRUE;
}

unsigned int _U3Read_Reg(unsigned int address)
{
	char *pu1Buf;
	unsigned int ret;

	pu1Buf = kmalloc(1, GFP_NOIO);
	ret = I2cReadReg(U3_PHY_I2C_DEV, address, pu1Buf);
	if (ret == PHY_FALSE) {
		pr_info("Read failed\n");
		return PHY_FALSE;
	}
	ret = (char)pu1Buf[0];
	kfree(pu1Buf);
	return ret;

}

unsigned int U3PhyWriteReg32(unsigned int addr, unsigned int data)
{
	unsigned int bank;
	unsigned int addr8;
	unsigned int data_0, data_1, data_2, data_3;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	data_0 = data & 0xff;
	data_1 = (data >> 8) & 0xff;
	data_2 = (data >> 16) & 0xff;
	data_3 = (data >> 24) & 0xff;

	pr_info("addr: %x, data: %x\n", addr8, data);
	_U3_Write_Bank(bank);
	pr_info("addr: %x, data: %x\n", addr8, data_0);
	_U3Write_Reg(addr8, data_0);
	pr_info("addr: %x, data: %x\n", addr8 + 1, data_1);
	_U3Write_Reg(addr8 + 1, data_1);
	pr_info("addr: %x, data: %x\n", addr8 + 2, data_2);
	_U3Write_Reg(addr8 + 2, data_2);
	pr_info("addr: %x, data: %x\n", addr8 + 3, data_3);
	_U3Write_Reg(addr8 + 3, data_3);

	return 0;
}

unsigned int U3PhyReadReg32(unsigned int addr)
{
	unsigned int bank;
	unsigned int addr8;
	unsigned int data;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;

	_U3_Write_Bank(bank);
	data = _U3Read_Reg(addr8);
	data |= (_U3Read_Reg(addr8 + 1) << 8);
	data |= (_U3Read_Reg(addr8 + 2) << 16);
	data |= (_U3Read_Reg(addr8 + 3) << 24);

	return data;
}

unsigned int U3PhyWriteReg8(unsigned int addr, unsigned char data)
{
	unsigned int bank;
	unsigned int addr8;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	_U3_Write_Bank(bank);
	_U3Write_Reg(addr8, data);

	pr_debug("addr: %x, data: %x\n", addr8, data);

	return PHY_TRUE;
}

unsigned char U3PhyReadReg8(unsigned int addr)
{
	unsigned int bank;
	unsigned int addr8;
	unsigned int data;

	bank = (addr >> 16) & 0xff;
	addr8 = addr & 0xff;
	_U3_Write_Bank(bank);
	data = _U3Read_Reg(addr8);

	return data;
}

unsigned int U3PhyWriteField8(phys_addr_t addr, unsigned int offset, unsigned int mask,
	unsigned int value)
{
	char cur_value;
	char new_value;

	cur_value = U3PhyReadReg8((u3phy_addr_t) addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	mb(); /* avoid context switch */
	U3PhyWriteReg8((u3phy_addr_t) addr, new_value);

	mb(); /* avoid context switch */
	return PHY_TRUE;
}

unsigned int U3PhyWriteField32(phys_addr_t addr, unsigned int offset, unsigned int mask,
	unsigned int value)
{
	unsigned int cur_value;
	unsigned int new_value;

	cur_value = U3PhyReadReg32((u3phy_addr_t) addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	mb(); /* avoid context switch */
	U3PhyWriteReg32((u3phy_addr_t) addr, new_value);

	mb(); /* avoid context switch */
	return PHY_TRUE;
}

unsigned int U3PhyReadField8(phys_addr_t addr, unsigned int offset, unsigned int mask)
{
	return (U3PhyReadReg8((u3phy_addr_t) addr) & mask) >> offset;
}

unsigned int U3PhyReadField32(phys_addr_t addr, unsigned int offset, unsigned int mask)
{

	return (U3PhyReadReg32((u3phy_addr_t) addr) & mask) >> offset;
}
#endif

/* ------------------------ SUB BOARD INIT API ------------------------------ */

unsigned int ssusb_read_phy_version(struct fpga_u3phy *u3phy,
		struct fpga_phy_instance *instance)
{
	u32 version;
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	void __iomem *i2c = instance->i2c_base;


	phy_writeb(i2c, 0x60, 0xff, SSUSB_PHY_VERSION_BANK);
	version = phy_readl(i2c, 0x60, SSUSB_PHY_VERSION_ADDR);
#else
	version = U3PhyReadReg32(0x2000e4);
#endif
	dev_info(u3phy->dev, "ssusb phy version: %x\n", version);

	return version;
}

static int a60931_u3phy_init(struct fpga_u3phy *u3phy,
		struct fpga_phy_instance *instance)
{
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	void __iomem *i2c = instance->i2c_base;

	dev_info(u3phy->dev, "%s\n", __func__);

	/* 0xFC[31:24], Change bank address to 0 */
	phy_writeb(i2c, 0x60, 0xff, 0x0);
	/* 0x14[14:12],  RG_USB20_HSTX_SRCTRL, set U2 slew rate as 4 */
	phy_writelmsk(i2c, 0x60, 0x14, 12, GENMASK(14, 12), 0x4);
	/* 0x18[23:23],  RG_USB20_BC11_SW_EN, Disable BC 1.1 */
	phy_writelmsk(i2c, 0x60, 0x18, 23, BIT(23), 0x0);
	/* 0x68[18:18],  force_suspendm = 0 */
	phy_writelmsk(i2c, 0x60, 0x68, 18, BIT(18), 0x0);
	/* 0xFC[31:24], Change bank address to 0x30 */
	phy_writeb(i2c, 0x60, 0xff, 0x30);
	/* 0x04[29:29],  RG_VUSB10_ON, SSUSB 1.0V power ON */
	phy_writelmsk(i2c, 0x60, 0x04, 29, BIT(29), 0x1);
	/* 0x04[25:21], RG_SSUSB_XTAL_TOP_RESERVE */
	phy_writelmsk(i2c, 0x60, 0x04, 21, GENMASK(25, 21), 0x11);
	/* 0xFC[31:24], Change bank address to 0x40 */
	phy_writeb(i2c, 0x60, 0xff, 0x40);
	/* 0x38[15:0], DA_SSUSB_PLL_SSC_DELTA1 */
	/* fine tune SSC delta1 to let SSC min average ~0ppm */
	phy_writelmsk(i2c, 0x60, 0x38, 0, GENMASK(15, 0)<<0, 0x47);
	/* 0x40[31:16], DA_SSUSB_PLL_SSC_DELTA */
	/* fine tune SSC delta to let SSC min average ~0ppm */
	phy_writelmsk(i2c, 0x60, 0x40, 16, GENMASK(31, 16), 0x44);
	/* 0xFC[31:24], Change bank address to 0x30 */
	phy_writeb(i2c, 0x60, 0xff, 0x30);
	/* 0x14[15:0],  RG_SSUSB_PLL_SSC_PRD */
	/* fine tune SSC PRD to let SSC freq average 31.5KHz */
	phy_writelmsk(i2c, 0x60, 0x14, 0, GENMASK(15, 0), 0x190);
	/* 0xFC[31:24], Change bank address to 0x70 */
	phy_writeb(i2c, 0x70, 0xff, 0x70);
	/* 0x88[3:2], Pipe reset, clk driving current */
	phy_writelmsk(i2c, 0x70, 0x88, 2, GENMASK(3, 2), 0x1);
	/* 0x88[5:4], Data lane 0 driving current */
	phy_writelmsk(i2c, 0x70, 0x88, 4, GENMASK(5, 4), 0x1);
	/* 0x88[7:6], Data lane 1 driving current */
	phy_writelmsk(i2c, 0x70, 0x88, 6, GENMASK(7, 6), 0x1);
	/* 0x88[9:8], Data lane 2 driving current */
	phy_writelmsk(i2c, 0x70, 0x88, 8, GENMASK(9, 8), 0x1);
	/* 0x88[11:10], Data lane 3 driving current */
	phy_writelmsk(i2c, 0x70, 0x88, 10, GENMASK(11, 10), 0x1);
	/* 0x9C[4:0],  rg_ssusb_ckphase, PCLK phase 0x00~0x1F */
	phy_writelmsk(i2c, 0x70, 0x9c, 0, GENMASK(4, 0), 0x19);

	/* Set INTR & TX/RX Impedance */

	/* 0xFC[31:24], Change bank address to 0x30 */
	phy_writeb(i2c, 0x60, 0xff, 0x30);
	/* 0x00[26:26],  RG_SSUSB_INTR_EN */
	phy_writelmsk(i2c, 0x60, 0x00, 26, BIT(26), 0x1);
	/* 0x00[15:10],  RG_SSUSB_IEXT_INTR_CTRL, Set Iext R selection */
	phy_writelmsk(i2c, 0x60, 0x00, 10, GENMASK(15, 10), 0x26);
	/* 0xFC[31:24], Change bank address to 0x10 */
	phy_writeb(i2c, 0x60, 0xff, 0x10);
	/* 0x10[31:31],  rg_ssusb_force_tx_impsel,  enable */
	phy_writelmsk(i2c, 0x60, 0x10, 31, BIT(31), 0x1);
	/* 0x10[28:24],  rg_ssusb_tx_impsel, Set TX Impedance */
	phy_writelmsk(i2c, 0x60, 0x10, 24, GENMASK(28, 24), 0x10);
	/* 0x14[31:31],  rg_ssusb_force_rx_impsel, enable */
	phy_writelmsk(i2c, 0x60, 0x14, 31, BIT(31), 0x1);
	/* 0x14[28:24],  rg_ssusb_rx_impsel, Set RX Impedance */
	phy_writelmsk(i2c, 0x60, 0x14, 24, GENMASK(28, 24), 0x10);
	/* 0xFC[31:24], Change bank address to 0x00 */
	phy_writeb(i2c, 0x60, 0xff, 0x00);
	/* 0x00[05:05],  RG_USB20_INTR_EN, U2 INTR_EN */
	phy_writelmsk(i2c, 0x60, 0x00, 5, BIT(5), 0x1);
	/* 0x04[23:19],  RG_USB20_INTR_CAL, Set Iext R selection */
	phy_writelmsk(i2c, 0x60, 0x04, 19, GENMASK(23, 19), 0x14);
#else
	dev_info(u3phy->dev, "%s\n", __func__);

	/*
	 * U2PHY initial
	 * Change bank address to 0x00
	 * I2C  0x60  0xFC[31:24] 0x00  RW
	 * USB20_BGR_EN
	 * I2C  0x60  0x00[00:00] 0x01  RW  RG_SIFSLV_BGR_EN
	 * RG_USB20_INTR_EN
	 * I2C  0x60  0x00[05:05] 0x01  RW  RG_USB20_INTR_EN
	 * RG_USB20_INTR_CAL
	 * I2C  0x60  0x04[23:19] 0x10  RW  RG_USB20_INTR_CAL
	 * RG_USB20_BC11_SW_EN, Disable BC 1.1
	 * I2C  0x60  0x18[23:23] 0x00  RW  RG_USB20_BC11_SW_EN
	 * force_suspendm = 0
	 * I2C  0x60  0x68[18:18] 0x00  RW  force_suspendm
	 * RG_SUSPENDM when force_suspendm = 1
	 * I2C  0x60  0x68[03:03] 0x01  RW  RG_SUSPENDM
	 * U2PHY patch
	 * I2C  0x60  0x14[14:12] 0x04  RW  RG_USB20_HSTX_SRCTRL
	 * I2C  0x60  0x18[03:00] 0x03  RW  RG_USB20_SQTH
	 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr0),
		A60931_RG_SIFSLV_BGR_EN_OFST, A60931_RG_SIFSLV_BGR_EN, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr0),
		A60931_RG_USB20_INTR_EN_OFST, A60931_RG_USB20_INTR_EN, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr1),
		A60931_RG_USB20_INTR_CAL_OFST, A60931_RG_USB20_INTR_CAL, 0x10);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr6),
		A60931_RG_USB20_BC11_SW_EN_OFST, A60931_RG_USB20_BC11_SW_EN, 0x0);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->u2phydtm0),
		A60931_FORCE_SUSPENDM_OFST, A60931_FORCE_SUSPENDM, 0x0);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->u2phydtm0),
		A60931_RG_SUSPENDM_OFST, A60931_RG_SUSPENDM, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr5),
		A60931_RG_USB20_HSTX_SRCTRL_OFST, A60931_RG_USB20_HSTX_SRCTRL, 0x4);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr6),
		A60931_RG_USB20_SQTH_OFST, A60931_RG_USB20_SQTH, 0x3);

	/*
	 * USB 3.0
	 * TPHY Init
	 * Change bank address to 0x30
	 * I2C  0x60  0xFC[31:24] 0x30  RW
	 * AVDD10 source select
	 * I2C  0x60  0x18[24:24] 0x01  RW  RG_SSUSB_LN0_CDR_RESERVE
	 * Change bank address to 0x40
	 * I2C  0x60  0xFC[31:24] 0x40  RW
	 * I2C  0x60  0x60[13:12] 0x02  RW  RG_SSUSB_LFPS_DEGLITCH_U3

	 * Change bank address to 0x30
	 * I2C  0x60  0xFC[31:24] 0x30  RW
	 * SSUSB 1.0V power ON
	 * I2C  0x60  0x04[29:29] 0x01  RW  RG_VUSB10_ON
	 * RG_SSUSB_XTAL_TOP_RESERVE<15:11> =10001
	 * I2C  0x60  0x04[25:21] 0x11  RW  RG_SSUSB_XTAL_TOP_RESERVE

	 * Change bank address to 0x40
	 * I2C  0x60  0xFC[31:24] 0x40  RW
	 * fine tune SSC delta1 to let SSC min average ~0ppm
	 * I2C  0x60  0x38[15:00] 0x47  RW  DA_SSUSB_PLL_SSC_DELTA1
	 * fine tune SSC delta to let SSC min average ~0ppm
	 * I2C  0x60  0x40[31:16] 0x44  RW  DA_SSUSB_PLL_SSC_DELTA
	 * Change bank address to 0x30
	 * I2C  0x60  0xFC[31:24] 0x30  RW
	 * fine tune SSC PRD to let SSC freq average 31.5KHz
	 * I2C  0x60  0x14[15:00] 0x190  RW  RG_SSUSB_PLL_SSC_PRD

	 * Change bank address to 0x10
	 * I2C  0x60  0xFC[31:24]  0x10  RW
	 * disable ssusb_p3_entry to work around resume from P3
	 * I2C  0x60  0x08[22:22]  0x00  RW  rg_ssusb_p3_entry
	 * force disable ssusb_p3_entry to work around resume from P3
	 * I2C  0x60  0x08[23:23]  0x01  RW  rg_ssusb_p3_entry_sel
	 * Change bank address to 0x60
	 * I2C  0x60  0xFC[31:24]  0x60  RW
	 * disable ssusb_p3_bias_pwd to work around resume from P3
	 * I2C  0x60  0x14[24:24]  0x00  RW  rg_ssusb_p3_bias_pwd
	 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg6),
		A60931_RG_SSUSB_LN0_CDR_RESERVE_OFST, A60931_RG_SSUSB_LN0_CDR_RESERVE, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_da_reg_a->reg32),
		A60931_RG_SSUSB_LFPS_DEGLITCH_U3_OFST,
		A60931_RG_SSUSB_LFPS_DEGLITCH_U3, 0x02);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg1),
		A60931_RG_VUSB10_ON_OFST, A60931_RG_VUSB10_ON, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg1),
		A60931_RG_SSUSB_XTAL_TOP_RESERVE_OFST,
		A60931_RG_SSUSB_XTAL_TOP_RESERVE, 0x11);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_da_reg_a->reg19),
		A60931_RG_SSUSB_PLL_SSC_DELTA1_U3_OFST,
		A60931_RG_SSUSB_PLL_SSC_DELTA1_U3, 0x47);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_da_reg_a->reg21),
		A60931_RG_SSUSB_PLL_SSC_DELTA_U3_OFST,
		A60931_RG_SSUSB_PLL_SSC_DELTA_U3, 0x44);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg7),
		A60931_RG_SSUSB_PLL_SSC_PRD_OFST, A60931_RG_SSUSB_PLL_SSC_PRD, 0x190);

	/*
	 * Set INTR & TX/RX Impedance
	 * Change bank address to 0x30
	 * I2C  0x60  0xFC[31:24] 0x30  RW
	 * INTR_EN
	 * I2C  0x60  0x00[26:26] 0x01  RW  RG_SSUSB_INTR_EN
	 * Set Iext R selection
	 * I2C  0x60  0x00[15:10] 0x28  RW  RG_SSUSB_IEXT_INTR_CTRL
	 * Change bank address to 0x10
	 * I2C  0x60  0xFC[31:24] 0x10  RW
	 * Force da_ssusb_tx_impsel enable
	 * I2C  0x60  0x10[31:31] 0x01  RW  rg_ssusb_force_tx_impsel
	 * Set TX Impedance
	 * I2C  0x60  0x10[28:24] 0x11  RW  rg_ssusb_tx_impsel
	 * Force da_ssusb_rx_impsel enable
	 * I2C  0x60  0x14[31:31] 0x01  RW  rg_ssusb_force_rx_impsel
	 * Set RX Impedance
	 * I2C  0x60  0x14[28:24] 0x10  RW  rg_ssusb_rx_impsel
	 * Change bank address to 0x00
	 * I2C  0x60  0xFC[31:24] 0x00  RW
	 * U2 INTR_EN
	 * I2C  0x60  0x00[05:05] 0x01  RW  RG_USB20_INTR_EN
	 * Set Iext R selection
	 * I2C  0x60  0x04[23:19] 0x13  RW  RG_USB20_INTR_CAL
	 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg0),
		A60931_RG_SSUSB_INTR_EN_OFST, A60931_RG_SSUSB_INTR_EN, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg0),
		A60931_RG_SSUSB_IEXT_INTR_CTRL_OFST, A60931_RG_SSUSB_IEXT_INTR_CTRL, 0x28);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_reg_a->phyd_impcal0),
		A60931_RG_SSUSB_FORCE_TX_IMPSEL_OFST, A60931_RG_SSUSB_FORCE_TX_IMPSEL, 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_reg_a->phyd_impcal0),
		A60931_RG_SSUSB_TX_IMPSEL_OFST, A60931_RG_SSUSB_TX_IMPSEL, 0x11);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_reg_a->phyd_impcal1),
		A60931_RG_SSUSB_FORCE_RX_IMPSEL_OFST, A60931_RG_SSUSB_FORCE_RX_IMPSEL, 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_reg_a->phyd_impcal1),
		A60931_RG_SSUSB_RX_IMPSEL_OFST, A60931_RG_SSUSB_RX_IMPSEL, 0x10);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr0),
		A60931_RG_USB20_INTR_EN_OFST, A60931_RG_USB20_INTR_EN, 0x1);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u2phy_reg_a->usbphyacr1),
		A60931_RG_USB20_INTR_CAL_OFST, A60931_RG_USB20_INTR_CAL, 0x13);

	/*
	 * RX_DET timing patch
	 * I2C  0x60  0xFC[31:24] 0x20  RW
	 * I2C  0x60  0x28[08:00] 0x50  RW  rg_ssusb_rxdet_stb1_set
	 * I2C  0x60  0x28[17:09] 0x10  RW  rg_ssusb_rxdet_stb2_set
	 * I2C  0x60  0x2C[08:00] 0x10  RW  rg_ssusb_rxdet_stb2_set_p3
	 * I2C  0x60  0xFC[31:24] 0x30  RW
	 * I2C  0x60  0x1C[11:10] 0x02  RW  RG_SSUSB_RXDET_VTHSEL_L
	 */
	U3PhyWriteField32(
	((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_bank2_reg_a->b2_phyd_rxdet1),
		A60931_RG_SSUSB_RXDET_STB1_SET_OFST, A60931_RG_SSUSB_RXDET_STB1_SET, 0x50);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_bank2_reg_a->b2_phyd_rxdet1),
		A60931_RG_SSUSB_RXDET_STB2_SET_OFST, A60931_RG_SSUSB_RXDET_STB2_SET, 0x10);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phyd_bank2_reg_a->b2_phyd_rxdet2),
		A60931_RG_SSUSB_RXDET_STB2_SET_P3_OFST, A60931_RG_SSUSB_RXDET_STB2_SET_P3, 0x10);
	U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.a60931_u3phya_reg_a->reg7),
			A60931_RG_SSUSB_RXDET_VTHSEL_L_OFST, A60931_RG_SSUSB_RXDET_VTHSEL_L, 0x2);

	/*
	 * Adjust pipe_clk phase for FPGA
	 * Change bank address to 0x70
	 * I2C  0x70  0xFC[31:24] 0x70  RW
	 * Pipe reset, clk driving current
	 * I2C  0x70  0x88[03:02] 0x01  RW
	 * Data lane 0 driving current
	 * I2C  0x70  0x88[05:04] 0x01  RW
	 * Data lane 1 driving current
	 * I2C  0x70  0x88[07:06] 0x01  RW
	 * Data lane 2 driving current
	 * I2C  0x70  0x88[09:08] 0x01  RW
	 * Data lane 3 driving current
	 * I2C  0x70  0x88[11:10] 0x01  RW
	 * PCLK phase 0x00~0x1F <-- adjust timing
	 * I2C  0x70  0x9C[04:00] 0x03  RW  rg_ssusb_ckphase
	 */
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve6),
		(2), (0x3<<2), 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve6),
		(4), (0x3<<4), 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve6),
		(6), (0x3<<6), 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve6),
		(8), (0x3<<8), 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve6),
		(10), (0x3<<10), 0x1);
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve11),
		(0), (0x1f<<0), 0x3);
#endif
	return 0;
}

static void a60931_u3phy_set_pclk(struct fpga_u3phy *u3phy,
		struct fpga_phy_instance *instance, int pclk)
{
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	void __iomem *i2c = instance->i2c_base;

	phy_writeb(i2c, 0x70, 0xff, 0x70);
	phy_writelmsk(i2c, 0x70, 0x9c, 0, GENMASK(4, 0), pclk);
#else
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t)&instance->info.a60931_sifslv_fm_reg_a->reserve11),
		(0), (0x1f<<0), pclk);
#endif
}

static int a60810_u3phy_init(struct fpga_u3phy *u3phy,
		struct fpga_phy_instance *instance)
{
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	void __iomem *i2c = instance->i2c_base;

	dev_info(u3phy->dev, "%s\n", __func__);

	phy_writeb(i2c, 0x60, 0xFF, 0x00);
	phy_writeb(i2c, 0x60, 0x05, 0x55);
	phy_writeb(i2c, 0x60, 0x18, 0x84);

	phy_writeb(i2c, 0x60, 0xFF, 0x10);
	phy_writeb(i2c, 0x60, 0x0A, 0x84);

	phy_writeb(i2c, 0x60, 0xFF, 0x40);
	phy_writeb(i2c, 0x60, 0x38, 0x46);
	phy_writeb(i2c, 0x60, 0x42, 0x40);
	phy_writeb(i2c, 0x60, 0x08, 0xAB);
	phy_writeb(i2c, 0x60, 0x09, 0x0C);
	phy_writeb(i2c, 0x60, 0x0C, 0x71);
	phy_writeb(i2c, 0x60, 0x0E, 0x4F);
	phy_writeb(i2c, 0x60, 0x10, 0xE1);
	phy_writeb(i2c, 0x60, 0x14, 0x5F);

	phy_writeb(i2c, 0x60, 0xFF, 0x60);
	phy_writeb(i2c, 0x60, 0x14, 0x03);

	phy_writeb(i2c, 0x60, 0xFF, 0x00);
	phy_writeb(i2c, 0x60, 0x6A, 0x04);
	phy_writeb(i2c, 0x60, 0x68, 0x08);
	phy_writeb(i2c, 0x60, 0x6C, 0x26);
	phy_writeb(i2c, 0x60, 0x6D, 0x36);
#else
	dev_info(u3phy->dev, "%s\n", __func__);

	/* BANK 0x00 */
	/* for U2 hS eye diagram */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u2phy_reg_a->usbphyacr1)
			  , A60810_RG_USB20_TERM_VREF_SEL_OFST, A60810_RG_USB20_TERM_VREF_SEL,
			  0x05);
	/* for U2 hS eye diagram */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u2phy_reg_a->usbphyacr1)
			  , A60810_RG_USB20_VRT_VREF_SEL_OFST, A60810_RG_USB20_VRT_VREF_SEL, 0x05);
	/* for U2 sensititvity */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u2phy_reg_a->usbphyacr6)
			  , A60810_RG_USB20_SQTH_OFST, A60810_RG_USB20_SQTH, 0x04);

	/* BANK 0x10 */
	/* disable ssusb_p3_entry to work around resume from P3 bug */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phyd_reg_a->phyd_lfps0)
			  , A60810_RG_SSUSB_P3_ENTRY_OFST, A60810_RG_SSUSB_P3_ENTRY, 0x00);
	/* force disable ssusb_p3_entry to work around resume from P3 bug */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phyd_reg_a->phyd_lfps0)
			  , A60810_RG_SSUSB_P3_ENTRY_SEL_OFST, A60810_RG_SSUSB_P3_ENTRY_SEL, 0x01);


	/* BANK 0x40 */
	/* fine tune SSC delta1 to let SSC min average ~0ppm */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg19)
			  , A60810_RG_SSUSB_PLL_SSC_DELTA1_U3_OFST,
			  A60810_RG_SSUSB_PLL_SSC_DELTA1_U3, 0x46);
	/* U3PhyWriteField32(((phys_addr_t)(uintptr_t)&instance->info.u3phya_da_reg_a->reg19) */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg21)
			  , A60810_RG_SSUSB_PLL_SSC_DELTA1_PE1H_OFST,
			  A60810_RG_SSUSB_PLL_SSC_DELTA1_PE1H, 0x40);


	/* fine tune SSC delta to let SSC min average ~0ppm */

	/* Fine tune SYSPLL to improve phase noise */
	/* I2C	60	  0x08[01:00]		0x03   RW  RG_SSUSB_PLL_BC_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg4)
			  , A60810_RG_SSUSB_PLL_BC_U3_OFST, A60810_RG_SSUSB_PLL_BC_U3, 0x3);
	/* I2C	60	  0x08[12:10]		0x03   RW  RG_SSUSB_PLL_DIVEN_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg4)
			  , A60810_RG_SSUSB_PLL_DIVEN_U3_OFST, A60810_RG_SSUSB_PLL_DIVEN_U3, 0x3);
	/* I2C	60	  0x0C[03:00]		0x01   RW  RG_SSUSB_PLL_IC_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg5)
			  , A60810_RG_SSUSB_PLL_IC_U3_OFST, A60810_RG_SSUSB_PLL_IC_U3, 0x1);
	/* I2C	60	  0x0C[23:22]		0x01   RW  RG_SSUSB_PLL_BR_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg5)
			  , A60810_RG_SSUSB_PLL_BR_U3_OFST, A60810_RG_SSUSB_PLL_BR_U3, 0x1);
	/* I2C	60	  0x10[03:00]		0x01   RW  RG_SSUSB_PLL_IR_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg6)
			  , A60810_RG_SSUSB_PLL_IR_U3_OFST, A60810_RG_SSUSB_PLL_IR_U3, 0x1);
	/* I2C	60	  0x14[03:00]		0x0F   RW  RG_SSUSB_PLL_BP_U3 */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u3phya_da_reg_a->reg7)
/* //	, A60810_RG_SSUSB_PLL_BP_U3, A60810_RG_SSUSB_PLL_BP_U3, 0xF); */
			  , A60810_RG_SSUSB_PLL_BP_U3_OFST, A60810_RG_SSUSB_PLL_BP_U3, 0x0f);

	/* BANK 0x60 */
	/* force xtal pwd mode enable */
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_spllc_reg_a->u3d_xtalctl_2),
		A60810_RG_SSUSB_FORCE_XTAL_PWD_OFST, A60810_RG_SSUSB_FORCE_XTAL_PWD,
		0x1);
	/* force bias pwd mode enable */
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_spllc_reg_a->u3d_xtalctl_2),
		A60810_RG_SSUSB_FORCE_BIAS_PWD_OFST, A60810_RG_SSUSB_FORCE_BIAS_PWD,
		0x1);
	/* force xtal pwd mode off to work around xtal drv de */
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_spllc_reg_a->u3d_xtalctl_2),
		A60810_RG_SSUSB_XTAL_PWD_OFST, A60810_RG_SSUSB_XTAL_PWD, 0x0);
	/* force bias pwd mode off to work around xtal drv de */
	U3PhyWriteField32(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_spllc_reg_a->u3d_xtalctl_2),
		A60810_RG_SSUSB_BIAS_PWD_OFST, A60810_RG_SSUSB_BIAS_PWD, 0x0);

	/* ******** test chip settings *********** */
	/* BANK 0x00 */
	/* slew rate setting */
	U3PhyWriteField32(((phys_addr_t)(uintptr_t) &instance->info.a60810_u2phy_reg_a->usbphyacr5)
			  , A60810_RG_USB20_HSTX_SRCTRL_OFST, A60810_RG_USB20_HSTX_SRCTRL, 0x4);

	/* BANK 0x50 */

	/* Write Phase Scan Result */
	/* PIPE setting  BANK5 */
	/* PIPE drv = 2 */
	U3PhyWriteReg8(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_sifslv_chip_reg_a->gpio_ctla) + 2,
		0x10);
	/* PIPE phase */
	/* U3PhyWriteReg8(
	 * ((phys_addr_t)(uintptr_t)&instance->info.sifslv_chip_reg_a->gpio_ctla)+3, 0xdc);
	 */
	U3PhyWriteReg8(
		((phys_addr_t)(uintptr_t) &instance->info.a60810_sifslv_chip_reg_a->gpio_ctla) + 3,
		0x24);
#endif
	return 0;
}

/* --------------------------- PHY DRIVER API ------------------------------- */

static int fpga_phy_init(struct phy *phy)
{
	struct fpga_phy_instance *instance = phy_get_drvdata(phy);
	struct fpga_u3phy *u3phy = dev_get_drvdata(phy->dev.parent);

	switch (instance->chip_version) {
	case PHY_TEST_CHIP_A60931:
		#ifndef CONFIG_U3_PHY_GPIO_SUPPORT
		instance->info.a60931_u2phy_reg_a = (struct a60931_u2phy_reg_a *)0x0;
		instance->info.a60931_u3phyd_reg_a = (struct a60931_u3phyd_reg_a *)0x100000;
		instance->info.a60931_u3phyd_bank2_reg_a =
			(struct a60931_u3phyd_bank2_reg_a *)0x200000;
		instance->info.a60931_u3phya_reg_a = (struct a60931_u3phya_reg_a *)0x300000;
		instance->info.a60931_u3phya_da_reg_a =
			(struct a60931_u3phya_da_reg_a *)0x400000;
		instance->info.a60931_sifslv_chip_reg_a =
			(struct a60931_sifslv_chip_reg_a *)0x500000;
		instance->info.a60931_spllc_reg_a = (struct a60931_spllc_reg_a *)0x600000;
		instance->info.a60931_sifslv_fm_reg_a =
			(struct a60931_sifslv_fm_reg_a *)0xf00000;
		#endif

		a60931_u3phy_init(u3phy, instance);
		a60931_u3phy_set_pclk(u3phy, instance, instance->u3_pclk);
		break;
	case PHY_TEST_CHIP_A60810:
		#ifndef CONFIG_U3_PHY_GPIO_SUPPORT
		instance->info.a60810_u2phy_reg_a = (struct a60810_u2phy_reg_a *)0x0;
		instance->info.a60810_u3phyd_reg_a = (struct a60810_u3phyd_reg_a *)0x100000;
		instance->info.a60810_u3phyd_bank2_reg_a =
			(struct a60810_u3phyd_bank2_reg_a *)0x200000;
		instance->info.a60810_u3phya_reg_a = (struct a60810_u3phya_reg_a *)0x300000;
		instance->info.a60810_u3phya_da_reg_a =
			(struct a60810_u3phya_da_reg_a *)0x400000;
		instance->info.a60810_sifslv_chip_reg_a =
			(struct a60810_sifslv_chip_reg_a *)0x500000;
		instance->info.a60810_spllc_reg_a = (struct a60810_spllc_reg_a *)0x600000;
		instance->info.a60810_sifslv_fm_reg_a =
			(struct a60810_sifslv_fm_reg_a *)0xf00000;
		#endif

		a60810_u3phy_init(u3phy, instance);
		break;
	default:
		dev_info(u3phy->dev, "%s: incompatible PHY type:0x%x\n",
			__func__, instance->chip_version);
		return -EINVAL;
	}

	return 0;
}

int fpga_phy_set_pclk(struct phy *phy, int pclk)
{
	struct fpga_phy_instance *instance = phy_get_drvdata(phy);
	struct fpga_u3phy *u3phy = dev_get_drvdata(phy->dev.parent);

	if (pclk < 0)
		pclk = instance->u3_pclk;

	switch (instance->chip_version) {
	case PHY_TEST_CHIP_A60931:
		a60931_u3phy_set_pclk(u3phy, instance, pclk);
		break;
	case PHY_TEST_CHIP_A60810:
		/* nothing to do */
		break;
	default:
		dev_info(u3phy->dev, "%s: incompatible PHY type:0x%x\n",
			__func__, instance->chip_version);
		return -EINVAL;
	}

	return 0;
}

static struct phy *fpga_phy_xlate(struct device *dev,
		struct of_phandle_args *args)
{
	struct fpga_u3phy *u3phy = dev_get_drvdata(dev);
	struct fpga_phy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_info(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < u3phy->nphys; index++)
		if (phy_np == u3phy->phys[index]->phy->dev.of_node) {
			instance = u3phy->phys[index];
			break;
		}

	if (!instance) {
		dev_info(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];
	if (!(instance->type == PHY_TYPE_USB2 ||
				instance->type == PHY_TYPE_USB3)) {
		dev_info(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	return instance->phy;
}

static const struct phy_ops fpga_u3phy_ops = {
	.init		= fpga_phy_init,
	.owner		= THIS_MODULE,
};

static const struct of_device_id fpga_u3phy_id_table[] = {
	{ .compatible = "mediatek,fpga-u3phy", },
	{ },
};
MODULE_DEVICE_TABLE(of, fpga_u3phy_id_table);

static void dbg_write_reg(struct fpga_phy_instance *instance, const char *buf)
{
	struct device *dev = instance->dev;
	u32 i2c_addr = 0;
	u32 addr = 0;
	u32 value = 0;
	u32 old_val;
	u32 new_val;
	u32 param;

	param = sscanf(buf, "%*s 0x%x 0x%x 0x%x", &i2c_addr, &addr, &value);
	dev_info(dev, "params-%d (i2c_addr:%#x, addr:%#x, value:%#x)\n",
			param, i2c_addr, addr, value);
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	old_val = phy_readb(instance->i2c_base, i2c_addr, addr);
	phy_writeb(instance->i2c_base, i2c_addr, addr, value);
	new_val = phy_readb(instance->i2c_base, i2c_addr, addr);
#else
	old_val = U3PhyReadReg8(addr);
	U3PhyWriteReg8(addr, value);
	new_val = U3PhyReadReg8(addr);
#endif
	dev_info(dev, "0x%2.2x: 0x%2.2x --> 0x%2.2x\n",
		addr, old_val, new_val);

}

static void dbg_read_reg(struct fpga_phy_instance *instance, const char *buf)
{
	struct device *dev = instance->dev;
	u32 i2c_addr = 0;
	u32 addr = 0;
	u32 value = 0;
	u32 param;

	param = sscanf(buf, "%*s 0x%x 0x%x", &i2c_addr, &addr);
	dev_info(dev, "params-%d (i2c_addr: %#x, addr: %#x)\n",
			param, i2c_addr, addr);

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	value = phy_readb(instance->i2c_base, i2c_addr, addr);
#else
	value = U3PhyReadReg8(addr);
#endif
	dev_info(dev, "0x%2.2x: 0x%2.2x\n", addr, value);
}

static void dbg_set_pclk(struct fpga_phy_instance *instance, const char *buf)
{
	struct device *dev = instance->dev;
	int pclk;
	u32 param;

	param = sscanf(buf, "%*s 0x%x", &pclk);
	dev_info(dev, "params-%d (pclk: %#x)\n", param, pclk);
	fpga_phy_set_pclk(instance->phy, pclk);
}

static int phy_reg_show(struct seq_file *sf, void *unused)
{
	struct fpga_phy_instance *instance = sf->private;

	seq_printf(sf, "usage: %x\n(echo r[w] i2c_addr reg [value])(HEX)\n"
			"e.g. echo r 0x60 0xff > reg\n",
			instance->chip_version);

	return 0;
}

static int phy_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, phy_reg_show, inode->i_private);
}

static ssize_t phy_reg_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct fpga_phy_instance *instance = sf->private;
	char buf[128];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	switch (buf[0]) {
	case 'w':
		dbg_write_reg(instance, buf);
		break;
	case 'r':
		dbg_read_reg(instance, buf);
		break;
	case 'p':
		dbg_set_pclk(instance, buf);
		break;
	default:
		dev_info(instance->dev, "No such cmd\n");
	}

	return count;
}

static const struct file_operations phy_reg_fops = {
	.open = phy_reg_open,
	.write = phy_reg_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_debugfs_interface(struct fpga_u3phy *u3phy,
		struct fpga_phy_instance *instance)
{
	struct dentry *root;

	root = debugfs_create_dir(instance->name, u3phy->dbg_root);
	if (!root) {
		dev_info(u3phy->dev, "create debugfs root failed\n");
		return;
	}
	instance->dbg = root;

	debugfs_create_file("reg", 0644, root, instance, &phy_reg_fops);
}

static int fpga_u3phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct fpga_u3phy *u3phy;
	u32 ippc;
	int index;
	int ret;

	dev_info(dev, "%s\n", __func__);
	u3phy = devm_kzalloc(dev, sizeof(*u3phy), GFP_KERNEL);
	if (!u3phy)
		return -ENOMEM;

	u3phy->dbg_root = debugfs_create_dir("fpga_phy", NULL);
	if (!u3phy->dbg_root)
		return -ENODEV;

	u3phy->nphys = of_get_child_count(np);
	u3phy->phys = devm_kcalloc(dev, u3phy->nphys,
			sizeof(*u3phy->phys), GFP_KERNEL);
	if (!u3phy->phys)
		return -ENOMEM;

	u3phy->dev = dev;
	platform_set_drvdata(pdev, u3phy);

	ret = of_property_read_u32(np, "mediatek,ippc", &ippc);
	if (ret) {
		dev_info(dev, "Failed to parse ippc value\n");
		return ret;
	}
	u3phy->ippc_base = ioremap(ippc, SSUSB_IPPC_LEN);

	if (!u3phy->ippc_base) {
		dev_info(dev, "could not ioremap ippc regs\n");
		return -ENOMEM;
	}

#ifndef CONFIG_U3_PHY_GPIO_SUPPORT
	if (!of_property_read_u32(np, "fpga_i2c_physical_base",
			(u32 *) &i2c_physical_base))
		dev_info(dev, "%s, i2c_physical_base:%x (dtsi)\n"
			,  __func__, i2c_physical_base);
	else {
		dev_info(dev, "no fpga_i2c_physical_base defined in dts.\n");
		return -ENODEV;
	}
	i2c_base = ioremap(i2c_physical_base, 0x1000);
	if (!(i2c_base))
		dev_info(dev, "Can't remap I2C BASE\n");

	dev_info(dev, "I2C BASE=0x%lx, %x\n", (uintptr_t) (i2c_base), i2c_physical_base);
#endif

	index = 0;
	for_each_child_of_node(np, child_np) {
		struct fpga_phy_instance *instance;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);


		if (!instance) {
			ret = -ENOMEM;
			goto put_child;
		}

		u3phy->phys[index] = instance;

		phy = devm_phy_create(dev, child_np, &fpga_u3phy_ops);
		if (IS_ERR(phy)) {
			dev_info(dev, "failed to create phy\n");
			ret = PTR_ERR(phy);
			goto put_child;
		}

		of_property_read_u32(child_np, "port",
				&instance->port);
		of_property_read_u32(child_np, "pclk_phase",
				&instance->u3_pclk);
		of_property_read_u32(child_np, "chip-id",
				&instance->chip_id);

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		instance->i2c_base = u3phy->ippc_base +
			SSUSB_FPGA_I2C_PORT_OFFSET(instance->port);
#endif
		instance->phy = phy;
		instance->chip_version = ssusb_read_phy_version(u3phy, instance);
		if (instance->chip_version) {
			if (instance->chip_version != PHY_TEST_CHIP_NONAME)
				/* this chip has a correct chip version */
				instance->chip_id = instance->chip_version;
			else if (instance->chip_id != instance->chip_version) {
				dev_info(dev, "WARN: this test chip maybe uses a wrong chip id\n"
							"use dts chip_id:%x\n", instance->chip_id);
			}
		} else {
			dev_info(dev, "ERROR: maybe phy DTB not connected or powered?\n");
		}
		sprintf(instance->name, "%x", instance->chip_id);
		instance->index = index;
		instance->dev = dev;
		phy_set_drvdata(phy, instance);
		create_debugfs_interface(u3phy, instance);
		index++;
		if (instance->chip_version)
			dev_info(dev, "sub-board: %s, port:%d, u3_pclk: 0x%x\n",
				instance->name,	instance->port, instance->u3_pclk);
	}

	provider = devm_of_phy_provider_register(dev, fpga_phy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return ret;
}

static struct platform_driver fpga_u3phy_driver = {
	.probe		= fpga_u3phy_probe,
	.driver		= {
		.name	= "fpga-u3phy",
		.of_match_table = fpga_u3phy_id_table,
	},
};

module_platform_driver(fpga_u3phy_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_DESCRIPTION("MediaTek FPGA USB PHY driver");
MODULE_LICENSE("GPL v2");
