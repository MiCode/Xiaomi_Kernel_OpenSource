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
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__

#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include "conninfra.h"
#include "emi_mng.h"
#include "conninfra_core.h"
#include "consys_hw.h"

#include <linux/ratelimit.h>

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

#define CONNINFRA_RST_RATE_LIMIT 0

#if CONNINFRA_RST_RATE_LIMIT
DEFINE_RATELIMIT_STATE(g_rs, HZ, 1);

#define DUMP_LOG() if (__ratelimit(&g_rs)) \
			pr_info("rst is ongoing")

#else
#define DUMP_LOG()
#endif

struct conninfra_rst_data {
	struct work_struct rst_worker;
	enum consys_drv_type drv;
	char *reason;
};

struct conninfra_rst_data rst_data;

int conninfra_get_clock_schematic(void)
{
	return consys_hw_get_clock_schematic();
}
EXPORT_SYMBOL(conninfra_get_clock_schematic);

void conninfra_get_phy_addr(unsigned int *addr, unsigned int *size)
{
	phys_addr_t base;

	conninfra_get_emi_phy_addr(CONNSYS_EMI_FW, &base, size);
	if (addr)
		*addr = (unsigned int)base;
	return;
}
EXPORT_SYMBOL(conninfra_get_phy_addr);

void conninfra_get_emi_phy_addr(enum connsys_emi_type type, phys_addr_t* base, unsigned int *size)
{
	struct consys_emi_addr_info* addr_info = emi_mng_get_phy_addr();

	switch (type) {
		case CONNSYS_EMI_FW:
			if (base)
				*base = addr_info->emi_ap_phy_addr;
			if (size)
				*size = addr_info->emi_size;
			break;
		case CONNSYS_EMI_MCIF:
			if (base)
				*base = addr_info->md_emi_phy_addr;
			if (size)
				*size = addr_info->md_emi_size;
			break;
		default:
			pr_err("Wrong EMI type: %d\n", type);
			if (base)
				*base = 0x0;
			if (size)
				*size = 0;
	}
}
EXPORT_SYMBOL(conninfra_get_emi_phy_addr);

int conninfra_pwr_on(enum consys_drv_type drv_type)
{
	pr_info("[%s] drv=[%d]", __func__, drv_type);
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}

#if ENABLE_PRE_CAL_BLOCKING_CHECK
	conninfra_core_pre_cal_blocking();
#endif

	return conninfra_core_power_on(drv_type);
}
EXPORT_SYMBOL(conninfra_pwr_on);

int conninfra_pwr_off(enum consys_drv_type drv_type)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}

#if ENABLE_PRE_CAL_BLOCKING_CHECK
	conninfra_core_pre_cal_blocking();
#endif

	return conninfra_core_power_off(drv_type);
}
EXPORT_SYMBOL(conninfra_pwr_off);


int conninfra_reg_readable(void)
{
	return conninfra_core_reg_readable();
}
EXPORT_SYMBOL(conninfra_reg_readable);


int conninfra_reg_readable_no_lock(void)
{
	return conninfra_core_reg_readable_no_lock();
}
EXPORT_SYMBOL(conninfra_reg_readable_no_lock);

int conninfra_is_bus_hang(void)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}
	return conninfra_core_is_bus_hang();
}
EXPORT_SYMBOL(conninfra_is_bus_hang);

int conninfra_trigger_whole_chip_rst(enum consys_drv_type who, char *reason)
{
	/* use schedule worker to trigger ??? */
	/* so that function can be returned immediately */
	int r;

	r = conninfra_core_lock_rst();
	if (r >= CHIP_RST_START) {
		/* reset is ongoing */
		pr_warn("[%s] r=[%d] chip rst is ongoing\n", __func__, r);
		return 1;
	}
	pr_info("[%s] rst lock [%d] [%d] reason=%s", __func__, r, who, reason);

	conninfra_core_trg_chip_rst(who, reason);

	return 0;
}
EXPORT_SYMBOL(conninfra_trigger_whole_chip_rst);

int conninfra_sub_drv_ops_register(enum consys_drv_type type,
				struct sub_drv_ops_cb *cb)
{
	/* type validation */
	if (type < 0 || type >= CONNDRV_TYPE_MAX) {
		pr_err("[%s] incorrect drv type [%d]", __func__, type);
		return -EINVAL;
	}
	pr_info("[%s] ----", __func__);
	conninfra_core_subsys_ops_reg(type, cb);
	return 0;
}
EXPORT_SYMBOL(conninfra_sub_drv_ops_register);

int conninfra_sub_drv_ops_unregister(enum consys_drv_type type)
{
	/* type validation */
	if (type < 0 || type >= CONNDRV_TYPE_MAX) {
		pr_err("[%s] incorrect drv type [%d]", __func__, type);
		return -EINVAL;
	}
	pr_info("[%s] ----", __func__);
	conninfra_core_subsys_ops_unreg(type);
	return 0;
}
EXPORT_SYMBOL(conninfra_sub_drv_ops_unregister);


int conninfra_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}
	if (subsystem >= SYS_SPI_MAX) {
		pr_err("[%s] wrong subsys %d", __func__, subsystem);
		return -EINVAL;
	}
	conninfra_core_spi_read(subsystem, addr, data);
	return 0;
}
EXPORT_SYMBOL(conninfra_spi_read);

int conninfra_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}

	if (subsystem >= SYS_SPI_MAX) {
		pr_err("[%s] wrong subsys %d", __func__, subsystem);
		return -EINVAL;
	}
	conninfra_core_spi_write(subsystem, addr, data);
	return 0;
}
EXPORT_SYMBOL(conninfra_spi_write);

int conninfra_adie_top_ck_en_on(enum consys_adie_ctl_type type)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}

	return conninfra_core_adie_top_ck_en_on(type);
}
EXPORT_SYMBOL(conninfra_adie_top_ck_en_on);

int conninfra_adie_top_ck_en_off(enum consys_adie_ctl_type type)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return CONNINFRA_ERR_RST_ONGOING;
	}

	return conninfra_core_adie_top_ck_en_off(type);
}
EXPORT_SYMBOL(conninfra_adie_top_ck_en_off);

int conninfra_spi_clock_switch(enum connsys_spi_speed_type type)
{
	return conninfra_core_spi_clock_switch(type);
}
EXPORT_SYMBOL(conninfra_spi_clock_switch);

void conninfra_config_setup(void)
{
	if (conninfra_core_is_rst_locking()) {
		DUMP_LOG();
		return;
	}

	conninfra_core_config_setup();
}
EXPORT_SYMBOL(conninfra_config_setup);

int conninfra_bus_clock_ctrl(enum consys_drv_type drv_type, unsigned int bus_clock, int status)
{
	return conninfra_core_bus_clock_ctrl(drv_type, bus_clock, status);
}
EXPORT_SYMBOL(conninfra_bus_clock_ctrl);
