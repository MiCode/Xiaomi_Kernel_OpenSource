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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/sched.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#else
#include <mach/mt_reg_base.h>
#endif
#include "mach/ddr_info.h"

static struct ddr_info_driver ddr_info_driver = {
	.driver = {
		   .name = "ddr_info",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
	.id_table = NULL,
};

#define DDR_REG_PASSWORD	8173
#define DDR_REG_MAX			0x690
#define DDR_EMI_MAX			0x5F8
#define TTY_LENGTH_MAX	256
#define DDR_TTY_DEBUG
#ifdef DDR_TTY_DEBUG
char buff_tty[TTY_LENGTH_MAX] = { 0 };

void tty_output(const char *fmt, ...)
{
	int size_data;
	va_list args;
	struct tty_struct *tty = NULL;

	tty = current->signal->tty;
	va_start(args, fmt);
	size_data = vsnprintf(buff_tty, sizeof(buff_tty), fmt, args);
	va_end(args);

	if (tty == NULL) {	/* no tty ,send log to uart */
		ddr_info_dbg("%s", buff_tty);
	} else {
		tty->driver->ops->write(tty, buff_tty, size_data);
		ddr_info_dbg("%s", buff_tty);	/* write log to uart too */
	}

}
#endif

/**************************************************************************
*STATIC FUNCTION
**************************************************************************/
#ifdef CONFIG_OF
static int ddr_reg_of_iomap(struct ddr_reg_base *reg_base)
{
	struct device_node *node = NULL;
	/*IO remap */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-ddrphy");
	if (node) {
		reg_base->ddrphy0_base = of_iomap(node, 0);
		ddr_info_warn("DDRPHY0 ADDRESS %p\n", reg_base->ddrphy0_base);
	} else {
		ddr_info_warn("can't find DDRPHY0 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dramco");
	if (node) {
		reg_base->dramc0_base = of_iomap(node, 0);
		ddr_info_warn("DRAMC0 ADDRESS %p,\n", reg_base->dramc0_base);
	} else {
		ddr_info_warn("can't find DRAMC0 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc0_nao");
	if (node) {
		reg_base->dramc0_nao_base = of_iomap(node, 0);
		ddr_info_warn("DRAMC0_NAO ADDRESS %p,\n", reg_base->dramc0_nao_base);
	} else {
		ddr_info_warn("can't find DRAMC0_NAO compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-ddrphy1");
	if (node) {
		reg_base->ddrphy1_base = of_iomap(node, 0);
		ddr_info_warn("DDRPHY1 ADDRESS %p\n", reg_base->ddrphy1_base);
	} else {
		ddr_info_warn("can't find DDRPHY1 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-dramc1");
	if (node) {
		reg_base->dramc1_base = of_iomap(node, 0);
		ddr_info_warn("DRAMC1 ADDRESS %p,\n", reg_base->dramc1_base);
	} else {
		ddr_info_warn("can't find DRAMC1 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,dramc1_nao");
	if (node) {
		reg_base->dramc1_nao_base = of_iomap(node, 0);
		ddr_info_warn("DRAMC1_NA1 ADDRESS %p,\n", reg_base->dramc1_nao_base);
	} else {
		ddr_info_warn("can't find DRAMC1_NA1 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
	if (node) {
		reg_base->ccif0_base = of_iomap(node, 0);
		ddr_info_warn("CCIF0 ADDRESS %p,\n", reg_base->ccif0_base);
	} else {
		ddr_info_warn("can't find CCIF0 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-emi");
	if (node) {
		reg_base->emi_base = of_iomap(node, 0);
		ddr_info_warn("EMI ADDRESS %p,\n", reg_base->emi_base);
	} else {
		ddr_info_warn("can't find EMI compatible node\n");
		return -1;
	}

	return 0;
}
#endif

static ssize_t ddr_add_show(struct device_driver *driver, char *buf)
{
	char *p = buf;
#ifdef CONFIG_OF
	p += sprintf(p, "ddrphy0_base      %p\n", ddr_info_driver.reg_base.ddrphy0_base);
	p += sprintf(p, "dramc0_base       %p\n", ddr_info_driver.reg_base.dramc0_base);
	p += sprintf(p, "dramc0_nao_base   %p\n", ddr_info_driver.reg_base.dramc0_nao_base);
	p += sprintf(p, "ddrphy1_base      %p\n", ddr_info_driver.reg_base.ddrphy1_base);
	p += sprintf(p, "dramc1_base       %p\n", ddr_info_driver.reg_base.dramc1_base);
	p += sprintf(p, "dramc1_nao_base   %p\n", ddr_info_driver.reg_base.dramc1_nao_base);
	p += sprintf(p, "emi_base          %p\n", ddr_info_driver.reg_base.emi_base);
#endif
	return (p-buf);
}

DRIVER_ATTR(ddr_info_add, 0444, ddr_add_show, NULL);

static ssize_t ddr_drv_info_show(struct device_driver *driver, char *buf)
{
	char *p = buf;
#ifdef CONFIG_OF
	unsigned int u4value0_1, u4value0_2, u4value1_1, u4value1_2;

	u4value0_1 = readl(ddr_info_driver.reg_base.dramc0_base + 0x0b8);
	u4value1_1 = readl(ddr_info_driver.reg_base.dramc1_base + 0x0b8);
	u4value0_2 = readl(ddr_info_driver.reg_base.ddrphy0_base + 0x0bc);
	u4value1_2 = readl(ddr_info_driver.reg_base.ddrphy1_base + 0x0bc);

	p += sprintf(p, "A : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value0_2 & 0xFF00FF00);
	p += sprintf(p, "    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value0_1 & 0xFF00FF00);
	p += sprintf(p, "B : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value1_2 & 0xFF00FF00);
	p += sprintf(p, "    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value1_1 & 0xFF00FF00);
#endif
	return (p-buf);
}

DRIVER_ATTR(ddr_info_drv, 0444, ddr_drv_info_show, NULL);

static ssize_t ddr_drv_show(struct device_driver *driver, char *buf)
{
	char *p = buf;

#ifdef CONFIG_OF
	unsigned int u4value0_1, u4value0_2, u4value1_1, u4value1_2;

	u4value0_1 = readl(ddr_info_driver.reg_base.dramc0_base + 0x0b8);
	u4value1_1 = readl(ddr_info_driver.reg_base.dramc1_base + 0x0b8);
	u4value0_2 = readl(ddr_info_driver.reg_base.ddrphy0_base + 0x0bc);
	u4value1_2 = readl(ddr_info_driver.reg_base.ddrphy1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_nao_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_nao_base + 0x0bc);

	p += sprintf(p, "read: cat  ddr_drv\n");
	p += sprintf(p, "write:echo ddr_drv channel drv value\n");
	p += sprintf(p, "ext : echo ddr_drv 0 0 0x88\n");
	p += sprintf(p, "      echo ddr_drv 1 1 0xaa\n");
	p += sprintf(p, "channel : A channel -> 0, B channel -> 1\n");
	p += sprintf(p, "drv     : CLK -> 0, CA -> 1, DQS -> 2, DQ -> 3\n");
	p += sprintf(p, "value   : 0x00 ~ 0xFF(ext :0x66, 0x88, 0xaa, 0xff...)\n");
	p += sprintf(p, "\n");
	p += sprintf(p, "A : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value0_2 & 0xFF00FF00);
	p += sprintf(p, "    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value0_1 & 0xFF00FF00);
	p += sprintf(p, "B : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value1_2 & 0xFF00FF00);
	p += sprintf(p, "    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value1_1 & 0xFF00FF00);
#endif
	return (p-buf);
}

static ssize_t ddr_drv_store(struct device_driver *driver, const char *buf, size_t count)
{				/* format: channel : drv : value */
	char *p = (char *)buf;

#ifdef CONFIG_OF
	int res = 0x00;
	unsigned int u4update = 0x00;
	unsigned int u4value0, u4value1, u4value2;
	unsigned int u4value0_1, u4value0_2, u4value1_1, u4value1_2;

	u4value0 = 0x00;
	u4value1 = 0x00;
	u4value2 = 0x00;
	u4value0_1 = readl(ddr_info_driver.reg_base.dramc0_base + 0x0b8);
	u4value1_1 = readl(ddr_info_driver.reg_base.dramc1_base + 0x0b8);
	u4value0_2 = readl(ddr_info_driver.reg_base.ddrphy0_base + 0x0bc);
	u4value1_2 = readl(ddr_info_driver.reg_base.ddrphy1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_nao_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_nao_base + 0x0bc);

#ifdef	DDR_TTY_DEBUG
	/* tty_output("count = %d , arg =%s\n",count,p); */
#endif
	if (0 == strncmp(p, "ddr_drv ", 8)) {
		p = p + 8;
	} else {
#ifdef	DDR_TTY_DEBUG
		tty_output("[ddr_info]cmd is not support...please try : cat ddr_drv\n");
#endif
		return count;
	}

	res = sscanf(p, "%d %d %x", &u4value0, &u4value1, &u4value2);
	if (res == (-1))
		return count;
#ifdef	DDR_TTY_DEBUG
	/* tty_output(" %d , %d, 0x%x\n",u4value0,u4value1,u4value2); */
#endif

	switch (u4value0) {
	case 0:		/* A drv */
		u4update = 0x01;
		if (u4value1 == 0x00) {	/* CLK */
			u4value0_2 = (u4value0_2 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x01) {	/* CA */
			u4value0_2 = (u4value0_2 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x02) {	/* DQS */
			u4value0_1 = (u4value0_1 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x03) {	/* DQ */
			u4value0_1 = (u4value0_1 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x04) {	/* all : CLK + CA + DQS + DQ */
			u4value0_2 = (u4value0_2 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
			u4value0_1 = (u4value0_1 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
		} else {
			u4update = 0x00;
		}
		break;
	case 1:		/* B drv */
		u4update = 0x01;
		if (u4value1 == 0x00) {	/* CLK */
			u4value1_2 = (u4value1_2 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x01) {	/* CA */
			u4value1_2 = (u4value1_2 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x02) {	/* DQS */
			u4value1_1 = (u4value1_1 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x03) {	/* DQ */
			u4value1_1 = (u4value1_1 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x04) {	/* all : CLK + CA + DQS + DQ */
			u4value1_2 = (u4value1_2 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
			u4value1_1 = (u4value1_1 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
		} else {
			u4update = 0x00;
		}
		break;
	case 2:		/* A + B drv */
		u4update = 0x01;
		if (u4value1 == 0x00) {	/* CLK */
			u4value0_2 = (u4value0_2 & 0x00FFFFFF) + (u4value2 << 24);
			u4value1_2 = (u4value1_2 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x01) {	/* CA */
			u4value0_2 = (u4value0_2 & 0xFFFF00FF) + (u4value2 << 8);
			u4value1_2 = (u4value1_2 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x02) {	/* DQS */
			u4value0_1 = (u4value0_1 & 0x00FFFFFF) + (u4value2 << 24);
			u4value1_1 = (u4value1_1 & 0x00FFFFFF) + (u4value2 << 24);
		} else if (u4value1 == 0x03) {	/* DQ */
			u4value0_1 = (u4value0_1 & 0xFFFF00FF) + (u4value2 << 8);
			u4value1_1 = (u4value1_1 & 0xFFFF00FF) + (u4value2 << 8);
		} else if (u4value1 == 0x04) {	/* all : CLK + CA + DQS + DQ */
			u4value0_2 = (u4value0_2 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
			u4value0_1 = (u4value0_1 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
			u4value1_2 = (u4value1_2 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
			u4value1_1 = (u4value1_1 & 0x00FF00FF) + (u4value2 << 24) + (u4value2 << 8);
		} else {
			u4update = 0x00;
		}
		break;
	default:
		u4update = 0x00;
		break;
	}

	if (u4update) {
		writel(u4value0_1, ddr_info_driver.reg_base.dramc0_base + 0x0b8);
		writel(u4value1_1, ddr_info_driver.reg_base.dramc1_base + 0x0b8);
		writel(u4value0_2, ddr_info_driver.reg_base.ddrphy0_base + 0x0bc);
		writel(u4value1_2, ddr_info_driver.reg_base.ddrphy1_base + 0x0bc);
		writel(u4value0_2, ddr_info_driver.reg_base.dramc0_base + 0x0bc);
		writel(u4value1_2, ddr_info_driver.reg_base.dramc1_base + 0x0bc);
		writel(u4value0_2, ddr_info_driver.reg_base.dramc0_nao_base + 0x0bc);
		writel(u4value1_2, ddr_info_driver.reg_base.dramc1_nao_base + 0x0bc);
	}
#ifdef	DDR_TTY_DEBUG
	u4value0_1 = readl(ddr_info_driver.reg_base.dramc0_base + 0x0b8);
	u4value1_1 = readl(ddr_info_driver.reg_base.dramc1_base + 0x0b8);
	u4value0_2 = readl(ddr_info_driver.reg_base.ddrphy0_base + 0x0bc);
	u4value1_2 = readl(ddr_info_driver.reg_base.ddrphy1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_base + 0x0bc);
	u4value0_2 |= readl(ddr_info_driver.reg_base.dramc0_nao_base + 0x0bc);
	u4value1_2 |= readl(ddr_info_driver.reg_base.dramc1_nao_base + 0x0bc);

	tty_output("A : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value0_2 & 0xFF00FF00);
	tty_output("    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value0_1 & 0xFF00FF00);
	tty_output("B : CLK[31:24] CA[15: 8] drv: 0x%08X\n", u4value1_2 & 0xFF00FF00);
	tty_output("    DQS[31:24] DQ[15: 8] drv: 0x%08X\n", u4value1_1 & 0xFF00FF00);
#endif

#endif
	return count;
}

DRIVER_ATTR(ddr_drv, 0664, ddr_drv_show, ddr_drv_store);

static unsigned int ddr_reg_readA(unsigned int offset)
{
	unsigned int u4Regvalue = 0x00;
	unsigned int u4Regvalue0_0, u4Regvalue0_1, u4Regvalue0_2;

	u4Regvalue0_0 = readl(ddr_info_driver.reg_base.dramc0_base + offset);
	u4Regvalue0_1 = readl(ddr_info_driver.reg_base.ddrphy0_base + offset);
	u4Regvalue0_2 = readl(ddr_info_driver.reg_base.dramc0_nao_base + offset);
	u4Regvalue = u4Regvalue0_0 | u4Regvalue0_1 | u4Regvalue0_2;

	return u4Regvalue;
}

static unsigned int ddr_reg_writeA(unsigned int offset, unsigned int value)
{
	writel(value, ddr_info_driver.reg_base.dramc0_base + offset);
	writel(value, ddr_info_driver.reg_base.ddrphy0_base + offset);
	writel(value, ddr_info_driver.reg_base.dramc0_nao_base + offset);

	return 0;
}

static unsigned int ddr_reg_readB(unsigned int offset)
{
	unsigned int u4Regvalue = 0x00;
	unsigned int u4Regvalue0_0, u4Regvalue0_1, u4Regvalue0_2;

	u4Regvalue0_0 = readl(ddr_info_driver.reg_base.dramc1_base + offset);
	u4Regvalue0_1 = readl(ddr_info_driver.reg_base.ddrphy1_base + offset);
	u4Regvalue0_2 = readl(ddr_info_driver.reg_base.dramc1_nao_base + offset);
	u4Regvalue = u4Regvalue0_0 | u4Regvalue0_1 | u4Regvalue0_2;

	return u4Regvalue;
}

static unsigned int ddr_reg_writeB(unsigned int offset, unsigned int value)
{
	writel(value, ddr_info_driver.reg_base.dramc1_base + offset);
	writel(value, ddr_info_driver.reg_base.ddrphy1_base + offset);
	writel(value, ddr_info_driver.reg_base.dramc1_nao_base + offset);

	return 0;
}

static ssize_t ddr_reg_show(struct device_driver *driver, char *buf)
{
	char *p = buf;

	p += sprintf(p, "read : echo regr channel offset\n");
	p += sprintf(p, "write: echo regw channel offset value\n");
	p += sprintf(p, "ext  : echo regr 0 0x110\n");
	p += sprintf(p, "       echo regw 0 0x110 0x55aa\n");
	p += sprintf(p, "channel : A channel -> 0, B channel -> 1\n");
	p += sprintf(p, "offset  : 0x00 ~ 0x690\n");
	p += sprintf(p, "value   : 0x00 ~ 0xffffffff\n");
	return (p-buf);
}

static ssize_t ddr_reg_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
#ifdef CONFIG_OF
	int res = 0x00;
	unsigned int u4Regvalue = 0x00;
	unsigned int u4password = 0x00;
	unsigned int u4value00, u4value01, u4value02;
	unsigned int u4Regvalue0_0, u4Regvalue0_1, u4Regvalue0_2;
	unsigned int u4Regvalue1_0, u4Regvalue1_1, u4Regvalue1_2;

	u4value00 = 0x00;
	u4value01 = 0x00;
	u4value02 = 0x00;
	u4Regvalue0_0 = 0x00;
	u4Regvalue0_1 = 0x00;
	u4Regvalue0_2 = 0x00;
	u4Regvalue1_0 = 0x00;
	u4Regvalue1_1 = 0x00;
	u4Regvalue1_2 = 0x00;

#ifdef	DDR_TTY_DEBUG
	/* tty_output("count0 = %d , arg =%s\n",count,p); */
#endif
	if (0 == strncmp(p, "regr ", 5)) {	/* read reg */
		p = p + 5;
		res = sscanf(p, "%d %x", &u4value00, &u4value01);	/* channel : offset */
		if (res == (-1))
			goto error;
#ifdef	DDR_TTY_DEBUG
		/* tty_output("%d, 0x%x, 0x%x, %d\n",u4value00, u4value01, u4value02, u4password); */
#endif

		if (u4value01 > DDR_REG_MAX)
			goto error;
		u4value01 = u4value01 & 0xFFFFFFFC;
		if (u4value00 == 0x00) {	/* A */
			u4Regvalue = ddr_reg_readA(u4value01);
#ifdef	DDR_TTY_DEBUG
			tty_output("[A channel]addr 0x%x : value 0x%x\n", u4value01, u4Regvalue);
#endif
		} else if (u4value00 == 0x01) {	/* B */
			u4Regvalue = ddr_reg_readB(u4value01);
#ifdef	DDR_TTY_DEBUG
			tty_output("[B channel]addr 0x%x : value 0x%x\n", u4value01, u4Regvalue);
#endif
		} else if (u4value00 == 0x02) {	/* A + B */
			u4Regvalue = ddr_reg_readA(u4value01);
#ifdef	DDR_TTY_DEBUG
			tty_output("[A channel]addr 0x%x : value 0x%x\n", u4value01, u4Regvalue);
#endif

			u4Regvalue = ddr_reg_readB(u4value01);
#ifdef	DDR_TTY_DEBUG
			tty_output("[B channel]addr 0x%x : value 0x%x\n", u4value01, u4Regvalue);
#endif
		} else {
			goto error;
		}
	} else if (0 == strncmp(p, "regw ", 5)) {	/* write reg */
		p = p + 5;
		/* channel : offset : value : password */
		res = sscanf(p, "%d %x %x %d", &u4value00, &u4value01, &u4value02, &u4password);
		if (res == (-1))
			goto error;
#ifdef	DDR_TTY_DEBUG
		/* tty_output("%d, 0x%x, 0x%x, %d\n",u4value00, u4value01, u4value02, u4password); */
#endif

		if (u4value01 > DDR_REG_MAX)
			goto error;
		u4value01 = u4value01 & 0xFFFFFFFC;
		if (u4password == DDR_REG_PASSWORD) {
			u4Regvalue = u4value02;
			if (u4value00 == 0x00) {	/* A */
				ddr_reg_writeA(u4value01, u4Regvalue);
#ifdef	DDR_TTY_DEBUG
				tty_output("[A channel]addr 0x%x : value 0x%x\n", u4value01,
					   u4Regvalue);
#endif
			} else if (u4value00 == 0x01) {	/* B */
				ddr_reg_writeB(u4value01, u4Regvalue);
#ifdef	DDR_TTY_DEBUG
				tty_output("[B channel]addr 0x%x : value 0x%x\n", u4value01,
					   u4Regvalue);
#endif
			} else if (u4value00 == 0x02) {	/* A + B */
				ddr_reg_writeA(u4value01, u4Regvalue);
				ddr_reg_writeB(u4value01, u4Regvalue);
#ifdef	DDR_TTY_DEBUG
				tty_output("[A channel]addr 0x%x : value 0x%x\n", u4value01,
					   ddr_reg_readA(u4value01));
				tty_output("[B channel]addr 0x%x : value 0x%x\n", u4value01,
					   ddr_reg_readB(u4value01));
#endif
			} else {
				goto error;
			}
		} else {
			goto error;
		}
	} else if (0 == strncmp(p, "emir ", 5)) {	/* read emi */
		p = p + 5;
		res = kstrtoint(p, 0, &u4value00);	/* offset */
		if (res == (-1))
			goto error;
#ifdef	DDR_TTY_DEBUG
		/* tty_output("0x%x, 0x%x, 0x%x, %d\n",u4value00, u4value01, u4value02, u4password); */
#endif

		if (u4value00 > DDR_EMI_MAX)
			goto error;
		u4value00 = u4value00 & 0xFFFFFFFC;
		u4Regvalue = readl(ddr_info_driver.reg_base.emi_base + u4value00);
#ifdef	DDR_TTY_DEBUG
		tty_output("[emi]addr 0x%x : value 0x%x\n", u4value00, u4Regvalue);
#endif
	} else if (0 == strncmp(p, "emiw ", 5)) {	/* write emi */
		p = p + 5;
		res = sscanf(p, "%x %x %d", &u4value00, &u4value01, &u4password);	/* offset : value : password */
		if (res == (-1))
			goto error;
#ifdef	DDR_TTY_DEBUG
		/* tty_output("0x%x, 0x%x, 0x%x, %d\n",u4value00, u4value01, u4value02, u4password); */
#endif

		if (u4value00 > DDR_EMI_MAX)
			goto error;
		u4value00 = u4value00 & 0xFFFFFFFC;
		if (u4password == DDR_REG_PASSWORD) {
			u4Regvalue = u4value01;
			writel(u4Regvalue, ddr_info_driver.reg_base.emi_base + u4value00);
#ifdef	DDR_TTY_DEBUG
			u4Regvalue = readl(ddr_info_driver.reg_base.emi_base + u4value00);
			tty_output("[emi]addr 0x%x : value 0x%x\n", u4value00, u4Regvalue);
#endif
		} else {
			goto error;
		}
	} else {		/* others not support */
error:
#ifdef	DDR_TTY_DEBUG
		tty_output("[ddr_info]cmd is not support...please try : cat ddr_reg\n");
#endif
		return count;
	}

#endif
	return count;
}

DRIVER_ATTR(ddr_reg, 0664, ddr_reg_show, ddr_reg_store);

static int __init ddr_info_init(void)
{
	int ret = 0;

	ddr_info_dbg("module init\n");

#ifdef CONFIG_OF
	ddr_reg_of_iomap(&ddr_info_driver.reg_base);
#else
	ddr_info_driver.reg_base.ddrphy0_base = DDRPHY_BASE;
	ddr_info_driver.reg_base.dramc0_base = DRAMC0_BASE;
	ddr_info_driver.reg_base.dramc0_nao_base = DRAMC_NAO_BASE;
	ddr_info_driver.reg_base.ccif0_base = APMIXEDSYS_BASE;

	ddr_info_driver.reg_base.ddrphy1_base = DDRPHY1_BASE;
	ddr_info_driver.reg_base.dramc1_base = DRAMC1_BASE;
	ddr_info_driver.reg_base.dramc1_nao_base = DRAMC1_NAO_BASE;
	ddr_info_driver.reg_base.emi_base = EMI_BASE;
#endif

	/* register driver and create sysfs files */
	ret = driver_register(&ddr_info_driver.driver);
	if (ret) {
		ddr_info_warn("fail to register ddr_info driver\n");
		return -1;
	}

	ret = driver_create_file(&ddr_info_driver.driver, &driver_attr_ddr_info_add);
	if (ret) {
		ddr_info_warn("fail to create ddr_info_add sysfs file\n");
		driver_unregister(&ddr_info_driver.driver);
		return -1;
	}

	ret = driver_create_file(&ddr_info_driver.driver, &driver_attr_ddr_info_drv);
	if (ret) {
		ddr_info_warn("fail to create ddr_info_drv sysfs file\n");
		driver_unregister(&ddr_info_driver.driver);
		return -1;
	}

	ret = driver_create_file(&ddr_info_driver.driver, &driver_attr_ddr_drv);
	if (ret) {
		ddr_info_warn("fail to create ddr_drv sysfs file\n");
		driver_unregister(&ddr_info_driver.driver);
		return -1;
	}

	ret = driver_create_file(&ddr_info_driver.driver, &driver_attr_ddr_reg);
	if (ret) {
		ddr_info_warn("fail to create ddr_reg sysfs file\n");
		driver_unregister(&ddr_info_driver.driver);
		return -1;
	}

	return 0;
}

static void __exit ddr_info_exit(void)
{
	ddr_info_dbg("module exit\n");
	driver_unregister(&ddr_info_driver.driver);
}
module_init(ddr_info_init);
module_exit(ddr_info_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek DDR info");
MODULE_AUTHOR("Quanwen Tan <quanwen.tan@mediatek.com>");
