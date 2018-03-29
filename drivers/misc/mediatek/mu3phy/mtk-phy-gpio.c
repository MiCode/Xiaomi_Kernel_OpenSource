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

#include "mtk-phy.h"

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT

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

#ifdef USB_ELBRUS
#define PHY_I2C_BASE      (i2c_base)
#else
#define PHY_I2C_BASE      (i2c1_base)
#endif

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
