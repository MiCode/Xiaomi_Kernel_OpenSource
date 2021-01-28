/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_secure_api.h>

#include <mtk_dcm_internal.h>

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

static short dcm_cpu_cluster_stat;

#ifdef CONFIG_HOTPLUG_CPU
#if 0 /* TODO check again */
static struct notifier_block dcm_hotplug_nb;
#endif
#endif

unsigned int all_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | DDRPHY_DCM_TYPE | EMI_DCM_TYPE
		| DRAMC_DCM_TYPE | BIG_CORE_DCM_TYPE);

#if defined(BUSDVT_ONLY_MD)
static unsigned int init_dcm_type = BUSDVT_DCM_TYPE;
#else
unsigned int init_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | BIG_CORE_DCM_TYPE);
#endif



#if defined(__KERNEL__) && defined(CONFIG_OF)
unsigned long  dcm_mp_cpusys_top_base;
unsigned long  dcm_cpccfg_reg_base;
unsigned long  dcm_infracfg_ao_base;
/* infra_cfg_ao_mem : can't change on-the-fly */
unsigned long  dcm_sub_infracfg_ao_mem_base;
unsigned long  dcm_infracfg_ao_mem_base;
unsigned long  dcm_infra_ao_bcrm_base;

/* emi */
unsigned long  dcm_emi_base;
unsigned long  dcm_sub_emi_base;

/* dramc */
/* ddrphy */
unsigned long  dcm_chn0_emi_base;
unsigned long  dcm_dramc_ch0_top5_base;
unsigned long  dcm_chn1_emi_base;
unsigned long  dcm_dramc_ch1_top5_base;
unsigned long  dcm_chn2_emi_base;
unsigned long  dcm_dramc_ch2_top5_base;
unsigned long  dcm_chn3_emi_base;
unsigned long  dcm_dramc_ch3_top5_base;

/* the DCMs that not used actually */
unsigned long  dcm_sspm_base;
unsigned long  dcm_audio_base;


#define INFRACFG_AO_NODE	"mediatek,infracfg_ao"
#define INFRACFG_AO_MEM_NODE	"mediatek,infracfg_ao_mem"
#define MCUCFG_NODE		"mediatek,mcucfg"
#define CPCCFG_NODE		"mediatek,mt6779-cpc"
#define PWRAP_NODE		"mediatek,pwrap"
#define DRAMC_NODE		"mediatek,dramc"


#define EMI_NODE		"mediatek,emi"
#define DCM_NODE "mediatek,mt6885-dcm"

#endif /* #if defined(__KERNEL__) && defined(CONFIG_OF) */

short is_dcm_bringup(void)
{
#ifdef DCM_BRINGUP
	dcm_pr_info("%s: skipped for bring up\n", __func__);
	return 1;
#else
	return 0;
#endif
}

#ifdef CONFIG_OF

struct DCM_BASE dcm_base_array[] = {
	DCM_BASE_INFO(dcm_infracfg_ao_base),
	DCM_BASE_INFO(dcm_infracfg_ao_mem_base),
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_emi_base),
	DCM_BASE_INFO(dcm_sub_emi_base),
	DCM_BASE_INFO(dcm_chn0_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch0_top5_base),
	DCM_BASE_INFO(dcm_chn1_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch1_top5_base),
	DCM_BASE_INFO(dcm_chn2_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch2_top5_base),
	DCM_BASE_INFO(dcm_chn3_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch3_top5_base),
	DCM_BASE_INFO(dcm_sub_infracfg_ao_mem_base),
	DCM_BASE_INFO(dcm_sspm_base),
	DCM_BASE_INFO(dcm_audio_base),
	DCM_BASE_INFO(dcm_mp_cpusys_top_base),
	DCM_BASE_INFO(dcm_cpccfg_reg_base),
};

#define DCM_COMMON_MAP
int mt_dcm_dts_map(void)
{
	struct device_node *node;
#ifdef DCM_COMMON_MAP
	unsigned int i;
	/* dcm */
	node = of_find_compatible_node(NULL, NULL, DCM_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DCM_NODE);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dcm_base_array); i++) {
		//*dcm_base_array[i].base= (unsigned long)of_iomap(node, i);
		*(dcm_base_array[i].base) = (unsigned long)of_iomap(node, i);

		if (!*(dcm_base_array[i].base)) {
			dcm_pr_info("error: cannot iomap base %s\n",
				dcm_base_array[i].name);
			return -1;
		}
	}
#else
	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", INFRACFG_AO_NODE);
		return -1;
	}
	dcm_infracfg_ao_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_infracfg_ao_base) {
		dcm_pr_info("error: cannot iomap %s\n", INFRACFG_AO_NODE);
		return -1;
	}

	/* infracfg_ao_mem */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_MEM_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n",
			INFRACFG_AO_MEM_NODE);
		return -1;
	}
	dcm_infracfg_ao_mem_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_infracfg_ao_base) {
		dcm_pr_info("error: cannot iomap %s\n", INFRACFG_AO_MEM_NODE);
		return -1;
	}

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", MCUCFG_NODE);
		return -1;
	}
	dcm_mcucfg_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_mcucfg_base) {
		dcm_pr_info("error: cannot iomap %s\n", MCUCFG_NODE);
		return -1;
	}
	dcm_mcucfg_base += 0x8000;

	/* cpccfg */
	node = of_find_compatible_node(NULL, NULL, CPCCFG_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", CPCCFG_NODE);
		return -1;
	}
	dcm_cpccfg_rg_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_cpccfg_rg_base) {
		dcm_pr_info("error: cannot iomap %s\n", CPCCFG_NODE);
		return -1;
	}

	/* dram related */
	node = of_find_compatible_node(NULL, NULL, DRAMC_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DRAMC_NODE);
		return -1;
	}
	/* dramc ch0*/
	dcm_dramc_ch0_top0_ao_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_dramc_ch0_top0_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch0\n");
		return -1;
	}
	/* dramc ch1*/
	dcm_dramc_ch1_top0_ao_base = (unsigned long)of_iomap(node, 1);
	if (!dcm_dramc_ch1_top0_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch1\n");
		return -1;
	}
	/* ddrphy ch0*/
	dcm_dramc_ch0_top5_ao_base = (unsigned long)of_iomap(node, 4);
	if (!dcm_dramc_ch0_top5_ao_base) {
		dcm_pr_info("error: cannot iomap ddrphy ch0\n");
		return -1;
	}
	/* ddrphy ch1*/
	dcm_dramc_ch1_top5_ao_base = (unsigned long)of_iomap(node, 5);
	if (!dcm_dramc_ch1_top5_ao_base) {
		dcm_pr_info("error: cannot iomap ddrphy ch1\n");
		return -1;
	}

	/* EMI related */
	node = of_find_compatible_node(NULL, NULL, EMI_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", EMI_NODE);
		return -1;
	}
	/* cen emi */
	dcm_emi_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_emi_base) {
		dcm_pr_info("error: cannot iomap CEN EMI\n");
		return -1;
	}
	/* emi ch0 */
	dcm_ch0_emi_base = (unsigned long)of_iomap(node, 2);
	if (!dcm_ch0_emi_base) {
		dcm_pr_info("error: cannot iomap CH0 EMI\n");
		return -1;
	}
#endif
	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #ifdef CONFIG_PM */




/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
/* TODO: Fix*/
int dcm_topckg(int on)
{
	return 0;
}

void dcm_infracfg_ao_emi_indiv(int on)
{
}

int dcm_infra_preset(int on)
{
	return 0;
}

bool dcm_infra_is_on(void)
{
	bool ret = true;

	ret &= dcm_infracfg_ao_aximem_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_aximem_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_bus_dcm_is_on();
	/*ret &= dcm_infracfg_ao_infra_conn_bus_dcm_is_on();*/
	ret &= dcm_infracfg_ao_infra_rx_p2p_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_module_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_infra_bus_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_peri_bus_dcm_is_on();

	return ret;
}

int dcm_infra(int on)
{
	/* INFRACFG_AO */
	/* Review this group with PIC*/
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	/*dcm_infracfg_ao_infra_conn_bus_dcm(on);*/
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infracfg_ao_peri_bus_dcm(on);
	dcm_infracfg_ao_peri_module_dcm(on);

	/* INFRACFG_AO_MEM */
	/* move to preloader */
	/* dcm_infracfg_ao_mem_dcm_emi_group(on); */

	/* INFRA_AO_BCRM */
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_peri_bus_dcm(on);
	/* SUB_INFRACFG_AO_MEM */

	/* move to preloader */
	/* dcm_sub_infracfg_ao_mem_dcm_emi_group(on); */

	return 0;
}

int dcm_peri(int on)
{
	return 0;
}

bool dcm_armcore_is_on(void)
{
	bool ret = true;

	ret &= dcm_mp_cpusys_top_bus_pll_div_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_2_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_3_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpu_pll_div_4_dcm_is_on();

	return ret;
}

int dcm_armcore(int mode)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_2_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_3_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_4_dcm(mode);

	return 0;
}

bool dcm_mcusys_is_on(void)
{
	bool ret = true;

	ret &= dcm_mp_cpusys_top_adb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_apb_dcm_is_on();
	ret &= dcm_mp_cpusys_top_cpubiu_dcm_is_on();
	ret &= dcm_mp_cpusys_top_misc_dcm_is_on();
	ret &= dcm_mp_cpusys_top_mp0_qdcm_is_on();
	ret &= dcm_cpccfg_reg_emi_wfifo_is_on();
	ret &= dcm_mp_cpusys_top_last_cor_idle_dcm_is_on();

	return ret;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);

	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);

	/* CPCCFG_REG */
	dcm_cpccfg_reg_emi_wfifo(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);

	return 0;
}

int dcm_mcusys_preset(int on)
{
	return 0;
}

int dcm_big_core_preset(void)
{
	return 0;
}

int dcm_big_core(int on)
{
	return 0;
}

int dcm_stall_preset(int on)
{
	return 0;
}

bool dcm_stall_is_on(void)
{
	bool ret = true;

	ret &= dcm_mp_cpusys_top_core_stall_dcm_is_on();
	ret &= dcm_mp_cpusys_top_fcm_stall_dcm_is_on();

	return ret;
}

int dcm_stall(int on)
{
	dcm_mp_cpusys_top_core_stall_dcm(on);
	dcm_mp_cpusys_top_fcm_stall_dcm(on);

	return 0;
}

int dcm_gic_sync(int on)
{
	return 0;
}

int dcm_last_core(int on)
{
	return 0;
}

int dcm_rgu(int on)
{
	return 0;
}

int dcm_dramc_ao(int on)
{
	return 0;
}

int dcm_ddrphy(int on)
{
	dcm_dramc_ch0_top5_ddrphy(on);
	dcm_dramc_ch1_top5_ddrphy(on);
	dcm_dramc_ch2_top5_ddrphy(on);
	dcm_dramc_ch3_top5_ddrphy(on);

	return 0;
}

int dcm_emi(int on)
{
	dcm_chn0_emi_chn_emi_dcm(on);
	dcm_chn1_emi_chn_emi_dcm(on);
	dcm_chn2_emi_chn_emi_dcm(on);
	dcm_chn3_emi_chn_emi_dcm(on);
	dcm_emi_emi_dcm(on);
	dcm_sub_emi_emi_dcm(on);

	return 0;
}

int dcm_lpdma(int on)
{
	return 0;
}

int dcm_pwrap(int on)
{
	return 0;
}

int dcm_mcsi_preset(int on)
{
	return 0;
}

int dcm_mcsi(int on)
{
	return 0;
}

int dcm_busdvt(int on)
{
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	dcm_infracfg_ao_infra_conn_bus_dcm(on);
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infracfg_ao_peri_bus_dcm(on);
	dcm_infracfg_ao_peri_module_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_peri_bus_dcm(on);

	dcm_dramc_ch0_top5_ddrphy(on);
	dcm_dramc_ch1_top5_ddrphy(on);
	/* move to preloader */
	/*dcm_infracfg_ao_mem_dcm_emi_group(on);*/
	/*dcm_sub_infracfg_ao_mem_dcm_emi_group(on);*/

	return 0;
}

struct DCM dcm_array[NR_DCM_TYPE] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .func_is_on = (DCM_FUNC_IS_ON) dcm_armcore_is_on,
	 .current_state = ARMCORE_DCM_MODE1,
	 .default_state = ARMCORE_DCM_MODE1,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .func_is_on = (DCM_FUNC_IS_ON) dcm_mcusys_is_on,
	 .current_state = MCUSYS_DCM_ON,
	 .default_state = MCUSYS_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 .func_is_on = (DCM_FUNC_IS_ON) dcm_infra_is_on,
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .func_is_on = NULL,
	 .current_state = PERI_DCM_ON,
	 .default_state = PERI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .func_is_on = NULL,
	 .current_state = EMI_DCM_ON,
	 .default_state = EMI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = DRAMC_DCM_TYPE,
	 .name = "DRAMC_DCM",
	 .func = (DCM_FUNC) dcm_dramc_ao,
	 .func_is_on = NULL,
	 .current_state = DRAMC_AO_DCM_ON,
	 .default_state = DRAMC_AO_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = DDRPHY_DCM_TYPE,
	 .name = "DDRPHY_DCM",
	 .func = (DCM_FUNC) dcm_ddrphy,
	 //.func_is_on = NULL,
	 .current_state = DDRPHY_DCM_ON,
	 .default_state = DDRPHY_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = STALL_DCM_TYPE,
	 .name = "STALL_DCM",
	 .func = (DCM_FUNC) dcm_stall,
	 .func_is_on = (DCM_FUNC_IS_ON) dcm_stall_is_on,
	 .current_state = STALL_DCM_ON,
	 .default_state = STALL_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = BIG_CORE_DCM_TYPE,
	 .name = "BIG_CORE_DCM",
	 .func = (DCM_FUNC) dcm_big_core,
	 .func_is_on = NULL,
	 .current_state = BIG_CORE_DCM_ON,
	 .default_state = BIG_CORE_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = GIC_SYNC_DCM_TYPE,
	 .name = "GIC_SYNC_DCM",
	 .func = (DCM_FUNC) dcm_gic_sync,
	 .func_is_on = NULL,
	 .current_state = GIC_SYNC_DCM_ON,
	 .default_state = GIC_SYNC_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = LAST_CORE_DCM_TYPE,
	 .name = "LAST_CORE_DCM",
	 .func = (DCM_FUNC) dcm_last_core,
	 .func_is_on = NULL,
	 .current_state = LAST_CORE_DCM_ON,
	 .default_state = LAST_CORE_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = RGU_DCM_TYPE,
	 .name = "RGU_CORE_DCM",
	 .func = (DCM_FUNC) dcm_rgu,
	 .func_is_on = NULL,
	 .current_state = RGU_DCM_ON,
	 .default_state = RGU_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = TOPCKG_DCM_TYPE,
	 .name = "TOPCKG_DCM",
	 .func = (DCM_FUNC) dcm_topckg,
	 .func_is_on = NULL,
	 .current_state = TOPCKG_DCM_ON,
	 .default_state = TOPCKG_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = LPDMA_DCM_TYPE,
	 .name = "LPDMA_DCM",
	 .func = (DCM_FUNC) dcm_lpdma,
	 .func_is_on = NULL,
	 .current_state = LPDMA_DCM_ON,
	 .default_state = LPDMA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCSI_DCM_TYPE,
	 .name = "MCSI_DCM",
	 .func = (DCM_FUNC) dcm_mcsi,
	 .func_is_on = NULL,
	 .current_state = MCSI_DCM_ON,
	 .default_state = MCSI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = BUSDVT_DCM_TYPE,
	 .name = "MCSI_DCM",
	 .func = (DCM_FUNC) dcm_busdvt,
	 .func_is_on = NULL,
	 .current_state = BUSDVT_DCM_ON,
	 .default_state = BUSDVT_DCM_ON,
	 .disable_refcnt = 0,
	 },
};

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	/* mcusys */
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG2);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG3);
	REG_DUMP(MP_CPUSYS_TOP_CPU_PLLDIV_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_BUS_PLLDIV_CFG);
	REG_DUMP(MP_CPUSYS_TOP_MCSIC_DCM0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0);
	REG_DUMP(CPCCFG_REG_EMI_WFIFO);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG7);


	/* infra_ao */
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(MODULE_SW_CG_2_SET);
	REG_DUMP(MODULE_SW_CG_2_CLR);
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);

	/* sub infro*/
	REG_DUMP(SUB_INFRA_EMI_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_DCM_CFG0);
	REG_DUMP(SUB_INFRA_EMI_DCM_CFG1);
	REG_DUMP(SUB_INFRA_EMI_DCM_CFG2);
	REG_DUMP(SUB_INFRA_EMI_M0M1_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_M0M1_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_M2M5_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_M2M5_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_M3_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_M3_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_M4_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_M4_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_M6M7_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_M6M7_IDLE_BIT_EN_1);
	REG_DUMP(SUB_INFRA_EMI_VPU_IDLE_BIT_EN_0);
	REG_DUMP(SUB_INFRA_EMI_VPU_IDLE_BIT_EN_1);

	/* infra emi */
	REG_DUMP(INFRA_EMI_DCM_CFG0);
	REG_DUMP(INFRA_EMI_DCM_CFG1);
	REG_DUMP(INFRA_EMI_DCM_CFG2);
	REG_DUMP(TOP_CK_ANCHOR_CFG);
	REG_DUMP(INFRA_EMI_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_M0M1_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_M0M1_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_M0M1_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_M0M1_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_M2M5_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_M2M5_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_M2M5_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_M2M5_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_M3_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_M3_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_M3_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_M3_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_M4_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_M4_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_M4_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_M4_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_M6M7_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_M6M7_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_M6M7_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_M6M7_IDLE_BIT_EN_3);
	REG_DUMP(INFRA_EMI_SRAM_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_EMI_SRAM_IDLE_BIT_EN_1);
	REG_DUMP(INFRA_EMI_SRAM_IDLE_BIT_EN_2);
	REG_DUMP(INFRA_EMI_SRAM_IDLE_BIT_EN_3);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_13);


	/* emi */
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONN);
	REG_DUMP(EMI_THRO_CTRL0);
	REG_DUMP(SUB_EMI_EMI_CONM);
	REG_DUMP(SUB_EMI_EMI_CONN);
	REG_DUMP(SUB_EMI_EMI_THRO_CTRL0);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);


	/* dramc */
	/* ddrphy */
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_DQSG_RETRY1);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_APB);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CTRL2);
	REG_DUMP(CHN1_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_DQSG_RETRY1);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_APB);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CTRL2);
	REG_DUMP(CHN2_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH2_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH2_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH2_TOP5_MISC_DQSG_RETRY1);
	REG_DUMP(DRAMC_CH2_TOP5_MISC_APB);
	REG_DUMP(DRAMC_CH2_TOP5_MISC_CTRL2);
	REG_DUMP(CHN3_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH3_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH3_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH3_TOP5_MISC_DQSG_RETRY1);
	REG_DUMP(DRAMC_CH3_TOP5_MISC_APB);
	REG_DUMP(DRAMC_CH3_TOP5_MISC_CTRL2);

}

#ifdef CONFIG_HOTPLUG_CPU
#if 0 /* TODO check again */
static int dcm_hotplug_nc(struct notifier_block *self,
					 unsigned long action, void *hcpu)
{
	unsigned int cpu = (long)hcpu;
	struct cpumask cpuhp_cpumask;
	struct cpumask cpu_online_cpumask;

	switch (action) {
	case CPU_ONLINE:
		arch_get_cluster_cpus(&cpuhp_cpumask, arch_get_cluster_id(cpu));
		cpumask_and(&cpu_online_cpumask,
			&cpuhp_cpumask, cpu_online_mask);
		if (cpumask_weight(&cpu_online_cpumask) == 1) {
			switch (cpu / 4) {
			case 0:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, LL CPU_ONLINE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat |= DCM_CPU_CLUSTER_LL;
				break;
			case 1:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, L CPU_ONLINE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat |= DCM_CPU_CLUSTER_L;
				break;
			case 2:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, B CPU_ONLINE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat |= DCM_CPU_CLUSTER_B;
				break;
			default:
				break;
			}
		}
		break;
	case CPU_DOWN_PREPARE:
		arch_get_cluster_cpus(&cpuhp_cpumask, arch_get_cluster_id(cpu));
		cpumask_and(&cpu_online_cpumask,
			&cpuhp_cpumask, cpu_online_mask);
		if (cpumask_weight(&cpu_online_cpumask) == 1) {
			switch (cpu / 4) {
			case 0:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, LL CPU_DOWN_PREPARE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat &= ~DCM_CPU_CLUSTER_LL;
				break;
			case 1:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, L CPU_DOWN_PREPARE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat &= ~DCM_CPU_CLUSTER_L;
				break;
			case 2:
				dcm_pr_dbg(
				"%s: act=0x%lx, cpu=%u, B CPU_DOWN_PREPARE\n",
				__func__, action, cpu);
				dcm_cpu_cluster_stat &= ~DCM_CPU_CLUSTER_B;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
#endif
#endif /* #ifdef CONFIG_HOTPLUG_CPU */

void dcm_set_hotplug_nb(void)
{
#ifdef CONFIG_HOTPLUG_CPU
#if 0 /* TODO check again */
	dcm_hotplug_nb = (struct notifier_block) {
		.notifier_call	= dcm_hotplug_nc,
		.priority	= INT_MIN + 2,
			/* NOTE: make sure this is < CPU DVFS */
	};

	if (register_cpu_notifier(&dcm_hotplug_nb))
		dcm_pr_info("[%s]: fail to register_cpu_notifier\n", __func__);
#endif
#endif /* #ifdef CONFIG_HOTPLUG_CPU */
}

int dcm_smc_get_cnt(int type_id)
{
	return dcm_smc_read_cnt(type_id);
}

void dcm_smc_msg_send(unsigned int msg)
{
	dcm_smc_msg(msg);
}

short dcm_get_cpu_cluster_stat(void)
{
	return dcm_cpu_cluster_stat;
}
