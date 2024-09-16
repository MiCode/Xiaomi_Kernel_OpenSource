/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include "osal.h"

#include "consys_hw.h"
#include "emi_mng.h"
#include "pmic_mng.h"
#include "consys_reg_mng.h"
#include "connsys_debug_utility.h"

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

static int mtk_conninfra_probe(struct platform_device *pdev);
static int mtk_conninfra_remove(struct platform_device *pdev);
static int mtk_conninfra_suspend(struct platform_device *pdev, pm_message_t state);
static int mtk_conninfra_resume(struct platform_device *pdev);

static int _consys_hw_conninfra_wakeup(void);
static void _consys_hw_conninfra_sleep(void);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

#ifdef CONFIG_OF
const struct of_device_id apconninfra_of_ids[] = {
	{.compatible = "mediatek,mt6885-consys",},
	{.compatible = "mediatek,mt6893-consys",},
	{}
};
#endif

static struct platform_driver mtk_conninfra_dev_drv = {
	.probe = mtk_conninfra_probe,
	.remove = mtk_conninfra_remove,
	.suspend = mtk_conninfra_suspend,
	.resume = mtk_conninfra_resume,
	.driver = {
		   .name = "mtk_conninfra",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = apconninfra_of_ids,
#endif
		   },
};


struct consys_hw_env conn_hw_env;

struct consys_hw_ops_struct *consys_hw_ops;
struct platform_device *g_pdev;

int g_conninfra_wakeup_ref_cnt;

struct work_struct ap_resume_work;

struct conninfra_dev_cb *g_conninfra_dev_cb;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
struct consys_hw_ops_struct* __weak get_consys_platform_ops(void)
{
	pr_err("Miss platform ops !!\n");
	return NULL;
}


struct platform_device *get_consys_device(void)
{
	return g_pdev;
}

int consys_hw_get_clock_schematic(void)
{
	if (consys_hw_ops->consys_plt_co_clock_type)
		return consys_hw_ops->consys_plt_co_clock_type();
	else
		pr_err("consys_hw_ops->consys_co_clock_type not supported\n");

	return -1;
}

unsigned int consys_hw_chipid_get(void)
{
	if (consys_hw_ops->consys_plt_soc_chipid_get)
		return consys_hw_ops->consys_plt_soc_chipid_get();
	else
		pr_err("consys_plt_soc_chipid_get not supported\n");

	return 0;
}

unsigned int consys_hw_get_hw_ver(void)
{
	if (consys_hw_ops->consys_plt_get_hw_ver)
		return consys_hw_ops->consys_plt_get_hw_ver();
	return 0;
}


int consys_hw_reg_readable(void)
{
	return consys_reg_mng_reg_readable();
}

int consys_hw_is_connsys_reg(phys_addr_t addr)
{
	return consys_reg_mng_is_connsys_reg(addr);
}

int consys_hw_is_bus_hang(void)
{
	return consys_reg_mng_is_bus_hang();
}

int consys_hw_dump_bus_status(void)
{
	return consys_reg_mng_dump_bus_status();
}

int consys_hw_dump_cpupcr(enum conn_dump_cpupcr_type dump_type, int times, unsigned long interval_us)
{
	return consys_reg_mng_dump_cpupcr(dump_type, times, interval_us);
}

int consys_hw_pwr_off(unsigned int curr_status, unsigned int off_radio)
{
	unsigned int next_status = curr_status & ~(0x1 << off_radio);
	int ret = 0;

	if (next_status == 0) {
		pr_info("Last pwoer off: %d\n", off_radio);
		pr_info("Power off CONNSYS PART 1\n");
		if (consys_hw_ops->consys_plt_conninfra_on_power_ctrl)
			consys_hw_ops->consys_plt_conninfra_on_power_ctrl(0);
		pr_info("Power off CONNSYS PART 2\n");
		if (consys_hw_ops->consys_plt_set_if_pinmux)
			consys_hw_ops->consys_plt_set_if_pinmux(0);
		if (consys_hw_ops->consys_plt_clock_buffer_ctrl)
			consys_hw_ops->consys_plt_clock_buffer_ctrl(0);
		ret = pmic_mng_common_power_ctrl(0);
		pr_info("Power off a-die power, ret=%d\n", ret);
	} else {
		pr_info("[%s] Part 0: only subsys (%d) off (curr_status=0x%x, next_status = 0x%x)\n",
			__func__, off_radio, curr_status, next_status);
		ret = _consys_hw_conninfra_wakeup();
		if (consys_hw_ops->consys_plt_subsys_status_update)
			consys_hw_ops->consys_plt_subsys_status_update(false, off_radio);
		if (consys_hw_ops->consys_plt_spi_master_cfg)
			consys_hw_ops->consys_plt_spi_master_cfg(next_status);
		if (consys_hw_ops->consys_plt_low_power_setting)
			consys_hw_ops->consys_plt_low_power_setting(curr_status, next_status);
		if (ret == 0)
			_consys_hw_conninfra_sleep();
	}

	return 0;
}

int consys_hw_pwr_on(unsigned int curr_status, unsigned int on_radio)
{
	int ret;
	unsigned int next_status = (curr_status | (0x1 << on_radio));

	/* first power on */
	if (curr_status == 0) {
		/* POS PART 1:
		 * Set PMIC to turn on the power that AFE WBG circuit in D-die,
		 * OSC or crystal component, and A-die need.
		 */
		ret = pmic_mng_common_power_ctrl(1);
		if (consys_hw_ops->consys_plt_clock_buffer_ctrl)
			consys_hw_ops->consys_plt_clock_buffer_ctrl(1);

		/* POS PART 2:
		 * 1. Pinmux setting
		 * 2. Turn on MTCMOS
		 * 3. Enable AXI bus (AP2CONN slpprot)
		 */
		if (consys_hw_ops->consys_plt_set_if_pinmux)
			consys_hw_ops->consys_plt_set_if_pinmux(1);

		if (consys_hw_ops->consys_plt_conninfra_on_power_ctrl)
			consys_hw_ops->consys_plt_conninfra_on_power_ctrl(1);

		if (consys_hw_ops->consys_plt_polling_consys_chipid)
			ret = consys_hw_ops->consys_plt_polling_consys_chipid();

		/* POS PART 3:
		 * 1. Set connsys EMI mapping
		 * 2. d_die_cfg
		 * 3. spi_master_cfg
		 * 4. a_die_cfg
		 * 5. afe_wbg_cal
		 * 6. patch default value
		 * 7. CONN_INFRA low power setting (srcclken wait time, mtcmos HW ctl...)
		 */
		emi_mng_set_remapping_reg();
		if (consys_hw_ops->consys_plt_d_die_cfg)
			consys_hw_ops->consys_plt_d_die_cfg();
		if (consys_hw_ops->consys_plt_spi_master_cfg)
			consys_hw_ops->consys_plt_spi_master_cfg(next_status);
		if (consys_hw_ops->consys_plt_a_die_cfg)
			consys_hw_ops->consys_plt_a_die_cfg();
		if (consys_hw_ops->consys_plt_afe_wbg_cal)
			consys_hw_ops->consys_plt_afe_wbg_cal();
		if (consys_hw_ops->consys_plt_subsys_pll_initial)
			consys_hw_ops->consys_plt_subsys_pll_initial();
		/* Record SW status on shared sysram */
		if (consys_hw_ops->consys_plt_subsys_status_update)
			consys_hw_ops->consys_plt_subsys_status_update(true, on_radio);
		if (consys_hw_ops->consys_plt_low_power_setting)
			consys_hw_ops->consys_plt_low_power_setting(curr_status, next_status);
	} else {
		ret = _consys_hw_conninfra_wakeup();
		/* Record SW status on shared sysram */
		if (consys_hw_ops->consys_plt_subsys_status_update)
			consys_hw_ops->consys_plt_subsys_status_update(true, on_radio);
		if (consys_hw_ops->consys_plt_spi_master_cfg)
			consys_hw_ops->consys_plt_spi_master_cfg(next_status);
		if (consys_hw_ops->consys_plt_low_power_setting)
			consys_hw_ops->consys_plt_low_power_setting(curr_status, next_status);

		if (ret == 0)
			_consys_hw_conninfra_sleep();
	}
	return 0;
}

int consys_hw_wifi_power_ctl(unsigned int enable)
{
	return pmic_mng_wifi_power_ctrl(enable);
}

int consys_hw_bt_power_ctl(unsigned int enable)
{
	return pmic_mng_bt_power_ctrl(enable);
}

int consys_hw_gps_power_ctl(unsigned int enable)
{
	return pmic_mng_gps_power_ctrl(enable);
}

int consys_hw_fm_power_ctl(unsigned int enable)
{
	return pmic_mng_fm_power_ctrl(enable);
}


int consys_hw_therm_query(int *temp_ptr)
{
	int ret = 0;

	/* wake/sleep conninfra */
	if (consys_hw_ops && consys_hw_ops->consys_plt_thermal_query) {
		ret = _consys_hw_conninfra_wakeup();
		if (ret)
			return CONNINFRA_ERR_WAKEUP_FAIL;
		*temp_ptr = consys_hw_ops->consys_plt_thermal_query();
		_consys_hw_conninfra_sleep();
	} else
		ret = -1;

	return ret;
}

void consys_hw_clock_fail_dump(void)
{
	if (consys_hw_ops && consys_hw_ops->consys_plt_clock_fail_dump)
		consys_hw_ops->consys_plt_clock_fail_dump();
}


int consys_hw_dump_power_state(void)
{
	if (consys_hw_ops && consys_hw_ops->consys_plt_power_state)
		consys_hw_ops->consys_plt_power_state();
	return 0;
}

int consys_hw_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data)
{
	if (consys_hw_ops->consys_plt_spi_read)
		return consys_hw_ops->consys_plt_spi_read(subsystem, addr, data);
	return -1;
}

int consys_hw_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data)
{
	if (consys_hw_ops->consys_plt_spi_write)
		return consys_hw_ops->consys_plt_spi_write(subsystem, addr, data);
	return -1;
}

int consys_hw_adie_top_ck_en_on(enum consys_adie_ctl_type type)
{
	if (consys_hw_ops->consys_plt_adie_top_ck_en_on)
		return consys_hw_ops->consys_plt_adie_top_ck_en_on(type);
	return -1;
}

int consys_hw_adie_top_ck_en_off(enum consys_adie_ctl_type type)
{
	if (consys_hw_ops->consys_plt_adie_top_ck_en_off)
		return consys_hw_ops->consys_plt_adie_top_ck_en_off(type);
	return -1;
}


static int _consys_hw_conninfra_wakeup(void)
{
	int ref = g_conninfra_wakeup_ref_cnt;
	bool wakeup = false, ret;

	if (consys_hw_ops->consys_plt_conninfra_wakeup) {
		if (g_conninfra_wakeup_ref_cnt == 0)  {
			ret = consys_hw_ops->consys_plt_conninfra_wakeup();
			if (ret) {
				pr_err("wakeup fail!! ret=[%d]", ret);
				return ret;
			}
			wakeup = true;
		}
		g_conninfra_wakeup_ref_cnt++;
	}

	pr_info("conninfra_wakeup refcnt=[%d]->[%d] %s",
			ref, g_conninfra_wakeup_ref_cnt, (wakeup ? "wakeup!!" : ""));
	return 0;
}

static void _consys_hw_conninfra_sleep(void)
{
	int ref = g_conninfra_wakeup_ref_cnt;
	bool sleep = false;

	if (consys_hw_ops->consys_plt_conninfra_sleep &&
		--g_conninfra_wakeup_ref_cnt == 0) {
		sleep = true;
		consys_hw_ops->consys_plt_conninfra_sleep();
	}
	pr_info("conninfra_sleep refcnt=[%d]->[%d] %s",
			ref, g_conninfra_wakeup_ref_cnt, (sleep ? "sleep!!" : ""));
}

int consys_hw_force_conninfra_wakeup(void)
{
	return _consys_hw_conninfra_wakeup();
}

int consys_hw_force_conninfra_sleep(void)
{
	_consys_hw_conninfra_sleep();
	return 0;
}

int consys_hw_spi_clock_switch(enum connsys_spi_speed_type type)
{
	if (consys_hw_ops->consys_plt_spi_clock_switch)
		return consys_hw_ops->consys_plt_spi_clock_switch(type);
	return -1;
}

void consys_hw_config_setup(void)
{
	if (consys_hw_ops->consys_plt_config_setup)
		consys_hw_ops->consys_plt_config_setup();
}

int consys_hw_pmic_event_cb(unsigned int id, unsigned int event)
{
	pmic_mng_event_cb(id, event);
	return 0;
}

int consys_hw_bus_clock_ctrl(enum consys_drv_type drv_type, unsigned int bus_clock, int status)
{
	if (consys_hw_ops->consys_plt_bus_clock_ctrl)
		return consys_hw_ops->consys_plt_bus_clock_ctrl(drv_type, bus_clock, status);
	else
		return -1;
}

int mtk_conninfra_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct consys_emi_addr_info* emi_info = NULL;

	/* Read device node */
	if (consys_reg_mng_init(pdev) != 0) {
		pr_err("consys_plt_read_reg_from_dts fail");
		return -1;
	}

	if (consys_hw_ops->consys_plt_clk_get_from_dts)
		consys_hw_ops->consys_plt_clk_get_from_dts(pdev);
	else {
		pr_err("consys_plt_clk_get_from_dtsfail");
		return -2;
	}

	/* emi mng init */
	ret = emi_mng_init();
	if (ret) {
		pr_err("emi_mng init fail, %d\n", ret);
		return -3;
	}

	ret = pmic_mng_init(pdev, g_conninfra_dev_cb);
	if (ret) {
		pr_err("pmic_mng init fail, %d\n", ret);
		return -4;
	}

	/* Setup connsys log emi base */
	emi_info = emi_mng_get_phy_addr();
	if (emi_info) {
		connsys_dedicated_log_path_apsoc_init((phys_addr_t)emi_info->emi_ap_phy_addr);
	} else {
		pr_err("Connsys log didn't init because EMI is invalid\n");
	}

	if (pdev)
		g_pdev = pdev;

	return 0;
}

int mtk_conninfra_remove(struct platform_device *pdev)
{
	if (g_pdev)
		g_pdev = NULL;

	return 0;
}

int mtk_conninfra_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* suspend callback is in atomic context */
	return 0;
}

int mtk_conninfra_resume(struct platform_device *pdev)
{
	/* suspend callback is in atomic context */
	schedule_work(&ap_resume_work);
	return 0;
}


static void consys_hw_ap_resume_handler(struct work_struct *work)
{
	if (g_conninfra_dev_cb && g_conninfra_dev_cb->conninfra_resume_cb)
		(*g_conninfra_dev_cb->conninfra_resume_cb)();
}


int consys_hw_init(struct conninfra_dev_cb *dev_cb)
{
	int iRet = 0;

	if (consys_hw_ops == NULL)
		consys_hw_ops = get_consys_platform_ops();

	g_conninfra_dev_cb = dev_cb;

	iRet = platform_driver_register(&mtk_conninfra_dev_drv);
	if (iRet)
		pr_err("Conninfra platform driver registered failed(%d)\n", iRet);

	INIT_WORK(&ap_resume_work, consys_hw_ap_resume_handler);
	pr_info("[consys_hw_init] result [%d]\n", iRet);

	return iRet;
}

int consys_hw_deinit(void)
{
	platform_driver_unregister(&mtk_conninfra_dev_drv);
	g_conninfra_dev_cb = NULL;
	return 0;
}
