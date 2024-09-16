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

#define pr_fmt(fmt) "conninfra_test@(%s:%d) " fmt, __func__, __LINE__

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "conninfra_test.h"
#include "osal.h"

#include "conninfra.h"
#include "conninfra_core.h"
#include "consys_reg_mng.h"

#include "connsyslog_test.h"
#include "conf_test.h"
#include "cal_test.h"
#include "msg_evt_test.h"
#include "chip_rst_test.h"
#include "mailbox_test.h"
#include "coredump_test.h"
#include "consys_hw.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define CONNINFRA_TEST_PROCNAME "driver/conninfra_test"

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

static ssize_t conninfra_test_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static ssize_t conninfra_test_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

static int core_tc(int par1, int par2, int par3);
static int conf_tc(int par1, int par2, int par3);
static int cal_tc(int par1, int par2, int par3);
static int msg_evt_tc(int par1, int par2, int par3);
static int chip_rst_tc(int par1, int par2, int par3);
static int mailbox_tc(int par1, int par2, int par3);
static int emi_tc(int par1, int par2, int par3);
static int log_tc(int par1, int par2, int par3);
static int thermal_tc(int par1, int par2, int par3);
static int bus_hang_tc(int par1, int par2, int par3);
static int dump_tc(int par1, int par2, int par3);
static int is_bus_hang_tc(int par1, int par2, int par3);
static int ap_resume_tc(int par1, int par2, int par3);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static struct proc_dir_entry *gConninfraTestEntry;

static const CONNINFRA_TEST_FUNC conninfra_test_func[] = {
	[0x01] = core_tc,
	[0x02] = conf_tc,
	[0x03] = msg_evt_tc,
	[0x04] = chip_rst_tc,
	[0x05] = cal_tc,
	[0x06] = mailbox_tc,
	[0x07] = emi_tc,
	[0x08] = log_tc,
	[0x09] = thermal_tc,
	[0x0a] = bus_hang_tc,
	[0x0b] = dump_tc,
	[0x0c] = is_bus_hang_tc,
	[0x0d] = ap_resume_tc,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

int core_tc_pwr_on(void)
{
	int iret = 0;

	pr_info("Power on test start");
	iret = conninfra_core_power_on(CONNDRV_TYPE_BT);
	pr_info("BT power on %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_on(CONNDRV_TYPE_FM);
	pr_info("FM power on %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_on(CONNDRV_TYPE_GPS);
	pr_info("GPS power on %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_on(CONNDRV_TYPE_WIFI);
	pr_info("Wi-Fi power on %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(200);

	return iret;
}

int core_tc_pwr_off(void)
{
	int iret = 0;

	iret = conninfra_core_power_off(CONNDRV_TYPE_WIFI);
	pr_info("Wi-Fi power off %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_off(CONNDRV_TYPE_GPS);
	pr_info("GPS power off %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_off(CONNDRV_TYPE_BT);
	pr_info("BT power off %s (result = %d)", iret? "fail" : "pass", iret);
	osal_sleep_ms(100);
	iret = conninfra_core_power_off(CONNDRV_TYPE_FM);
	pr_info("FM power off %s (result = %d)", iret? "fail" : "pass", iret);

	return iret;
}


int core_tc(int par1, int par2, int par3)
{
	int iret = 0;
	char* driver_name[CONNDRV_TYPE_MAX] = {
		"BT",
		"FM",
		"GPS",
		"Wi-Fi",
	};

	if (par2 == 0) {
		iret = core_tc_pwr_on();
		iret = core_tc_pwr_off();
	} else if (par2 == 1) {
		if (par3 == 15) {
			iret = core_tc_pwr_on();
		} else if (par3 >= CONNDRV_TYPE_BT && par3 <= CONNDRV_TYPE_WIFI) {
			pr_info("Power on %s test start\n", driver_name[par3]);
			iret = conninfra_core_power_on(par3);
			pr_info("Power on %s test, return = %d\n", driver_name[par3], iret);
		} else {
			pr_info("No support parameter\n");
		}
	} else if (par2 == 2) {
		if (par3 == 15) {
			iret = core_tc_pwr_off();
		} else if (par3 >= CONNDRV_TYPE_BT && par3 <= CONNDRV_TYPE_WIFI) {
			pr_info("Power off %s test start\n", driver_name[par3]);
			iret = conninfra_core_power_off(par3);
			pr_info("Power off %s test, return = %d\n", driver_name[par3], iret);
		} else {
			pr_info("No support parameter\n");
		}
	} else if (par2 == 3) {
		if (par3 == 1) {
			iret = conninfra_core_adie_top_ck_en_on(CONNSYS_ADIE_CTL_FW_WIFI);
			pr_info(
				"Turn on adie top ck en (ret=%d), please check 0x1805_2830[6] should be 1\n",
				iret);
		} else if (par3 == 0) {
			iret = conninfra_core_adie_top_ck_en_off(CONNSYS_ADIE_CTL_FW_WIFI);
			pr_info(
				"Turn off adie top ck en (ret=%d), please check 0x1805_2830[6] should be 1\n",
				iret);
		}
	}
	//pr_info("core_tc %s (result = %d)", iret? "fail" : "pass", iret);
	return 0;
}

static int conf_tc(int par1, int par2, int par3)
{
	return conninfra_conf_test();
}

static int msg_evt_tc(int par1, int par2, int par3)
{
	return msg_evt_test();
}

static int chip_rst_tc(int par1, int par2, int par3)
{
	pr_info("test start");
	return chip_rst_test();
}

static int cal_tc(int par1, int par2, int par3)
{
	pr_info("test start");
	return calibration_test();
}


static int mailbox_tc(int par1, int par2, int par3)
{
	return mailbox_test();
}

static int emi_tc(int par1, int par2, int par3)
{
	unsigned int addr = 0;
	unsigned int size = 0;
	int ret = 0;

	pr_info("[%s] start", __func__);
	conninfra_get_phy_addr(&addr, &size);
	if (addr == 0 || size == 0) {
		pr_err("[%s] fail! addr=[0x%x] size=[%u]", __func__, addr, size);
		ret = -1;
	} else
		pr_info("[%s] pass. addr=[0x%x] size=[%u]", __func__, addr, size);

	pr_info("[%s] end", __func__);

	return ret;
}

static int thermal_tc(int par1, int par2, int par3)
{
	int ret, temp;

	ret = core_tc_pwr_on();
	if (ret) {
		pr_err("pwr on fail");
		return -1;
	}
	ret = conninfra_core_thermal_query(&temp);
	pr_info("[%s] thermal res=[%d][%d]", __func__, ret, temp);

	return ret;
}

static int log_tc(int par1, int par2, int par3)
{
	/* 0: initial state
	 * 1: log has been init.
	 */
	static int log_status = 0;
	int ret = 0;

	if (par2 == 0) {
		if (log_status != 0) {
			pr_info("log has been init.\n");
			return 0;
		}
		/* init */
		ret = connlog_test_init();
		if (ret)
			pr_err("FW log init fail! ret=%d\n", ret);
		else {
			log_status = 1;
			pr_info("FW log init finish. Check result on EMI.\n");
		}
	} else if (par2 == 1) {
		/* add fake log */
		/* read log */
		connlog_test_read();
	} else if (par2 == 2) {
		/* deinit */
		if (log_status == 0) {
			pr_info("log didn't init\n");
			return 0;
		}
		ret = connlog_test_deinit();
		if (ret)
			pr_err("FW log deinit fail! ret=%d\n", ret);
		else
			log_status = 0;
	}
	return ret;
}


static int bus_hang_tc(int par1, int par2, int par3)
{
	int r;
	r = conninfra_core_is_bus_hang();

	pr_info("[%s] r=[%d]\n", __func__, r);
	return 0;
}

static int dump_tc(int par1, int par2, int par3)
{
	return coredump_test(par1, par2, par3);
}

static int is_bus_hang_tc(int par1, int par2, int par3)
{
	int r;

	r = consys_reg_mng_reg_readable();
	pr_info("[%s] r=[%d]", __func__, r);
	r = consys_reg_mng_is_bus_hang();
	pr_info("[%s] r=[%d]", __func__, r);
	return 0;
}

static int ap_resume_tc(int par1, int par2, int par3)
{
	return 0;
}

ssize_t conninfra_test_read(struct file *filp, char __user *buf,
				size_t count, loff_t *f_pos)
{
	return 0;
}

ssize_t conninfra_test_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	char *pDelimiter = " \t";
	int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	long res = 0;
	static bool test_enabled = false;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= osal_sizeof(buf)) {
		pr_err("input handling fail!\n");
		len = osal_sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = osal_strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x = (int)res;
	} else {
		x = 0;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		y = (int)res;
		pr_info("y = 0x%08x\n\r", y);
	} else {
		y = 3000;
		/*efuse, register read write default value */
		if (0x11 == x || 0x12 == x || 0x13 == x)
			y = 0x80000000;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		z = (int)res;
	} else {
		z = 10;
		/*efuse, register read write default value */
		if (0x11 == x || 0x12 == x || 0x13 == x)
			z = 0xffffffff;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	/* For eng and userdebug load, have to enable wmt_dbg by
	 * writing 0xDB9DB9 to * "/proc/driver/wmt_dbg" to avoid
	 * some malicious use
	 */
	if (x == 0xDB9DB9) {
		test_enabled = true;
		return len;
	}

	if (!test_enabled)
		return 0;

	if (osal_array_size(conninfra_test_func) > x &&
		NULL != conninfra_test_func[x])
		(*conninfra_test_func[x]) (x, y, z);
	else
		pr_warn("no handler defined for command id(0x%08x)\n\r", x);

	return len;

}


int conninfra_test_setup(void)
{
	static const struct file_operations conninfra_test_fops = {
		.owner = THIS_MODULE,
		.read = conninfra_test_read,
		.write = conninfra_test_write,
	};
	int i_ret = 0;

	gConninfraTestEntry = proc_create(CONNINFRA_TEST_PROCNAME,
					0664, NULL, &conninfra_test_fops);
	if (gConninfraTestEntry == NULL) {
		pr_err("Unable to create / wmt_aee proc entry\n\r");
		i_ret = -1;
	}

	return i_ret;
}

int conninfra_test_remove(void)
{
	if (gConninfraTestEntry != NULL)
		proc_remove(gConninfraTestEntry);
	return 0;
}

