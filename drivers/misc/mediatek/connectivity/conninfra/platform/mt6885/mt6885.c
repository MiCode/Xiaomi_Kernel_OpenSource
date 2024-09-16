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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <mtk_clkbuf_ctl.h>

#include <connectivity_build_in_adapter.h>

#include "osal.h"
#include "conninfra.h"
#include "conninfra_conf.h"
#include "consys_hw.h"
#include "consys_reg_mng.h"
#include "consys_reg_util.h"
#include "mt6885.h"
#include "emi_mng.h"
#include "mt6885_emi.h"
#include "mt6885_consys_reg.h"
#include "mt6885_consys_reg_offset.h"
#include "mt6885_pos.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define CONSYS_PWR_SPM_CTRL 1
#define PLATFORM_SOC_CHIP 0x6885

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

static int consys_clk_get_from_dts(struct platform_device *pdev);
static int consys_clock_buffer_ctrl(unsigned int enable);
static unsigned int consys_soc_chipid_get(void);
static unsigned int consys_get_hw_ver(void);
static void consys_clock_fail_dump(void);
static int consys_thermal_query(void);
static int consys_power_state(void);
static int consys_bus_clock_ctrl(enum consys_drv_type, unsigned int, int);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
struct consys_hw_ops_struct g_consys_hw_ops = {
	/* load from dts */
	/* TODO: mtcmos should move to a independent module */
	.consys_plt_clk_get_from_dts = consys_clk_get_from_dts,

	/* clock */
	.consys_plt_clock_buffer_ctrl = consys_clock_buffer_ctrl,
	.consys_plt_co_clock_type = consys_co_clock_type,

	/* POS */
	.consys_plt_conninfra_on_power_ctrl = consys_conninfra_on_power_ctrl,
	.consys_plt_set_if_pinmux = consys_set_if_pinmux,

	.consys_plt_polling_consys_chipid = consys_polling_chipid,
	.consys_plt_d_die_cfg = connsys_d_die_cfg,
	.consys_plt_spi_master_cfg = connsys_spi_master_cfg,
	.consys_plt_a_die_cfg = connsys_a_die_cfg,
	.consys_plt_afe_wbg_cal = connsys_afe_wbg_cal,
	.consys_plt_subsys_pll_initial = connsys_subsys_pll_initial,
	.consys_plt_low_power_setting = connsys_low_power_setting,
	.consys_plt_soc_chipid_get = consys_soc_chipid_get,
	.consys_plt_conninfra_wakeup = consys_conninfra_wakeup,
	.consys_plt_conninfra_sleep = consys_conninfra_sleep,
	.consys_plt_is_rc_mode_enable = consys_is_rc_mode_enable,

	/* debug */
	.consys_plt_clock_fail_dump = consys_clock_fail_dump,
	.consys_plt_get_hw_ver = consys_get_hw_ver,

	.consys_plt_spi_read = consys_spi_read,
	.consys_plt_spi_write = consys_spi_write,
	.consys_plt_adie_top_ck_en_on = consys_adie_top_ck_en_on,
	.consys_plt_adie_top_ck_en_off = consys_adie_top_ck_en_off,
	.consys_plt_spi_clock_switch = consys_spi_clock_switch,
	.consys_plt_subsys_status_update = consys_subsys_status_update,

	.consys_plt_thermal_query = consys_thermal_query,
	.consys_plt_power_state = consys_power_state,
	.consys_plt_config_setup = consys_config_setup,
	.consys_plt_bus_clock_ctrl = consys_bus_clock_ctrl,
};


struct clk *clk_scp_conn_main;	/*ctrl conn_power_on/off */
struct consys_plat_thermal_data g_consys_plat_therm_data;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
struct consys_hw_ops_struct* get_consys_platform_ops(void)
{
	return &g_consys_hw_ops;
}

/* mtcmos contorl */
int consys_clk_get_from_dts(struct platform_device *pdev)
{
	clk_scp_conn_main = devm_clk_get(&pdev->dev, "conn");
	if (IS_ERR(clk_scp_conn_main)) {
		pr_err("[CCF]cannot get clk_scp_conn_main clock.\n");
		return PTR_ERR(clk_scp_conn_main);
	}
	pr_debug("[CCF]clk_scp_conn_main=%p\n", clk_scp_conn_main);

	return 0;
}

int consys_platform_spm_conn_ctrl(unsigned int enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(clk_scp_conn_main);
		if (ret) {
			pr_err("Turn on oonn_infra power fail. Ret=%d\n", ret);
			return -1;
		}
	} else {
		clk_disable_unprepare(clk_scp_conn_main);

	}

	return ret;
}

int consys_clock_buffer_ctrl(unsigned int enable)
{
	/* This function call didn't work now.
	 * clock buffer is HW controlled, not SW controlled.
	 * Keep this function call to update status.
	 */
	if (enable)
		KERNEL_clk_buf_ctrl(CLK_BUF_CONN, true);	/*open XO_WCN*/
	else
		KERNEL_clk_buf_ctrl(CLK_BUF_CONN, false);	/*close XO_WCN*/
	return 0;
}

int consys_co_clock_type(void)
{
	const struct conninfra_conf *conf;

	/* Default solution */
	conf = conninfra_conf_get_cfg();
	if (NULL == conf) {
		pr_err("[%s] Get conf fail", __func__);
		return -1;
	}
	/* TODO: for co-clock mode, there are two case: 26M and 52M. Need something to distinguish it. */
	if (conf->tcxo_gpio != 0)
		return CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO;
	else
		return CONNSYS_CLOCK_SCHEMATIC_26M_COTMS;
}

unsigned int consys_soc_chipid_get(void)
{
	return PLATFORM_SOC_CHIP;
}

unsigned int consys_get_hw_ver(void)
{
	return CONN_HW_VER;
}

void consys_clock_fail_dump(void)
{
	pr_info("[%s]", __func__);
}


void update_thermal_data(struct consys_plat_thermal_data* input)
{
	memcpy(&g_consys_plat_therm_data, input, sizeof(struct consys_plat_thermal_data));
	/* Special factor, not in POS */
	/* THERMCR1 [16:17]*/
	CONSYS_REG_WRITE(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERMCR1,
			(CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERMCR1) |
				(0x3 << 16)));

}

int calculate_thermal_temperature(int y)
{
	struct consys_plat_thermal_data *data = &g_consys_plat_therm_data;
	int t;
	int const_offset = 25;

	/*
	 *    MT6635 E1 : read 0x02C = 0x66358A00
	 *    MT6635 E2 : read 0x02C = 0x66358A10
	 *    MT6635 E3 : read 0x02C = 0x66358A11
	 */
	if (conn_hw_env.adie_hw_version == 0x66358A10 ||
		conn_hw_env.adie_hw_version == 0x66358A11)
		const_offset = 28;

	/* temperature = (y-b)*slope + (offset) */
	/* TODO: offset + 25 : this is only for E1, E2 is 28 */
	t = (y - (data->thermal_b == 0 ? 0x36 : data->thermal_b)) *
			((data->slop_molecule + 209) / 100) + (data->offset + const_offset);

	pr_info("y=[%d] b=[%d] constOffset=[%d] [%d] [%d] => t=[%d]\n",
			y, data->thermal_b, const_offset, data->slop_molecule, data->offset,
			t);

	return t;
}

int consys_thermal_query(void)
{
#define THERMAL_DUMP_NUM	11
#define LOG_TMP_BUF_SZ		256
#define TEMP_SIZE		13
	void __iomem *addr = NULL;
	int cal_val, res = 0;
	/* Base: 0x1800_2000, CONN_TOP_THERM_CTL */
	const unsigned int thermal_dump_crs[THERMAL_DUMP_NUM] = {
		0x00, 0x04, 0x08, 0x0c,
		0x10, 0x14, 0x18, 0x1c,
		0x20, 0x24, 0x28,
	};
	char tmp[TEMP_SIZE] = {'\0'};
	char tmp_buf[LOG_TMP_BUF_SZ] = {'\0'};
	unsigned int i;
	unsigned int efuse0, efuse1, efuse2, efuse3;

	addr = ioremap_nocache(CONN_GPT2_CTRL_BASE, 0x100);
	if (addr == NULL) {
		pr_err("GPT2_CTRL_BASE remap fail");
		return -1;
	}

	consys_adie_top_ck_en_on(CONNSYS_ADIE_CTL_HOST_CONNINFRA);

	/* Hold Semaphore, TODO: may not need this, because
		thermal cr seperate for different  */
	if (consys_sema_acquire_timeout(CONN_SEMA_THERMAL_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[THERM QRY] Require semaphore fail\n");
		consys_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_CONNINFRA);
		iounmap(addr);
		return -1;
	}

	/* therm cal en */
	CONSYS_REG_WRITE(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN,
			(CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN) |
				(0x1 << 19)));
	/* GPT2 En */
	CONSYS_REG_WRITE(addr + CONN_GPT2_CTRL_THERMAL_EN,
			(CONSYS_REG_READ(addr + CONN_GPT2_CTRL_THERMAL_EN) |
			0x1));

	/* thermal trigger */
	CONSYS_REG_WRITE(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN,
			(CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN) |
				(0x1 << 18)));
	udelay(500);
	/* get thermal value */
	cal_val = CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN);
	cal_val = (cal_val >> 8) & 0x7f;

	/* thermal debug dump */
	efuse0 = CONSYS_REG_READ(CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_0);
	efuse1 = CONSYS_REG_READ(CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_1);
	efuse2 = CONSYS_REG_READ(CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_2);
	efuse3 = CONSYS_REG_READ(CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_3);
	for (i = 0; i < THERMAL_DUMP_NUM; i++) {
		if (snprintf(
			tmp, TEMP_SIZE, "[0x%08x]",
			CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + thermal_dump_crs[i])) >= 0)
			strncat(tmp_buf, tmp, strlen(tmp));
	}
	pr_info("[%s] efuse:[0x%08x][0x%08x][0x%08x][0x%08x] thermal dump: %s",
		__func__, efuse0, efuse1, efuse2, efuse3, tmp_buf);

	res = calculate_thermal_temperature(cal_val);

	/* GPT2 disable, no effect on 6885 */
	CONSYS_REG_WRITE(addr + CONN_GPT2_CTRL_THERMAL_EN,
			(CONSYS_REG_READ(addr + CONN_GPT2_CTRL_THERMAL_EN) &
				~(0x1)));

	/* disable */
	CONSYS_REG_WRITE(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN,
			(CONSYS_REG_READ(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_CAL_EN) &
				~(0x1 << 19)));

	consys_sema_release(CONN_SEMA_THERMAL_INDEX);
	consys_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_CONNINFRA);

	iounmap(addr);

	return res;
}


int consys_power_state(void)
{
	const char* osc_str[] = {
		"fm ", "gps ", "bgf ", "wf ", "ap2conn ", "conn_thm ", "conn_pta ", "conn_infra_bus "
	};
	char buf[256] = {'\0'};
	unsigned int r = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_2);
	unsigned int i, buf_len = 0, str_len;

	for (i = 0; i < 8; i++) {
		str_len = strlen(osc_str[i]);
		if ((r & (0x1 << (18 + i))) > 0 && (buf_len + str_len < 256)) {
			strncat(buf, osc_str[i], str_len);
			buf_len += str_len;
		}
	}
	pr_info("[%s] [0x%x] %s", __func__, r, buf);
	return 0;
}

int consys_bus_clock_ctrl(enum consys_drv_type drv_type, unsigned int bus_clock, int status)
{
	static unsigned int conninfra_bus_clock_wpll_state = 0;
	static unsigned int conninfra_bus_clock_bpll_state = 0;
	unsigned int wpll_state = conninfra_bus_clock_wpll_state;
	unsigned int bpll_state = conninfra_bus_clock_bpll_state;
	bool wpll_switch = false, bpll_switch = false;
	int check;

	if (status) {
		/* Turn on */
		/* Enable BPLL */
		if (bus_clock & CONNINFRA_BUS_CLOCK_BPLL) {

			if (conninfra_bus_clock_bpll_state == 0) {
				CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 21));
				udelay(30);
				bpll_switch = true;
			}
			conninfra_bus_clock_bpll_state |= (0x1 << drv_type);
		}
		/* Enable WPLL */
		if (bus_clock & CONNINFRA_BUS_CLOCK_WPLL) {
			if (conninfra_bus_clock_wpll_state == 0) {
				CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 20));
				udelay(50);
				wpll_switch = true;
			}
			conninfra_bus_clock_wpll_state |= (0x1 << drv_type);
		}
		pr_info("drv=[%d] conninfra_bus_clock_wpll=[%u]->[%u] %s conninfra_bus_clock_bpll=[%u]->[%u] %s",
			drv_type,
			wpll_state, conninfra_bus_clock_wpll_state, (wpll_switch ? "enable" : ""),
			bpll_state, conninfra_bus_clock_bpll_state, (bpll_switch ? "enable" : ""));
	} else {
		/* Turn off */
		/* Turn off WPLL */
		if (bus_clock & CONNINFRA_BUS_CLOCK_WPLL) {
			conninfra_bus_clock_wpll_state &= ~(0x1<<drv_type);
			if (conninfra_bus_clock_wpll_state == 0) {
				CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 20));
				wpll_switch = true;
			}
		}
		/* Turn off BPLL */
		if (bus_clock & CONNINFRA_BUS_CLOCK_BPLL) {
			conninfra_bus_clock_bpll_state &= ~(0x1<<drv_type);
			if (conninfra_bus_clock_bpll_state == 0) {
				CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 21));
				bpll_switch = true;
			}
		}
		pr_info("drv=[%d] conninfra_bus_clock_wpll=[%u]->[%u] %s conninfra_bus_clock_bpll=[%u]->[%u] %s",
			drv_type,
			wpll_state, conninfra_bus_clock_wpll_state, (wpll_switch ? "disable" : ""),
			bpll_state, conninfra_bus_clock_bpll_state, (bpll_switch ? "disable" : ""));
		if (consys_reg_mng_reg_readable() == 0) {
			check = consys_reg_mng_is_bus_hang();
			pr_info("[%s] not readable, bus hang check=[%d]", __func__, check);
		}
	}
	return 0;
}
