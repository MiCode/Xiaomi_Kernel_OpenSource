/*
 * Copyright (C) 2017 MediaTek Inc.
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

static short dcm_cpu_cluster_stat;

unsigned int all_dcm_type = (ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE
				    | INFRA_DCM_TYPE
				    | EMI_DCM_TYPE | DRAMC_DCM_TYPE
				    );
unsigned int init_dcm_type = (ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE
				     | INFRA_DCM_TYPE
				     );

#if defined(__KERNEL__) && defined(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_topckgen_ao_base;
unsigned long dcm_mcucfg_base;
unsigned long dcm_mcucfg_phys_base;
unsigned long dcm_dramc_base;
unsigned long dcm_emi_base;
unsigned long dcm_chn0_emi_base;

#define INFRACFG_AO_NODE "mediatek,infracfg_ao"
#define TOPCKGEN_AO_NODE "mediatek,topckgen_ao"
#define MCUCFG_NODE "mediatek,mcucfg"
#define DRAMC_NODE "mediatek,dramc"
#define EMI_NODE "mediatek,emi"
#define CHN0_EMI_NODE "mediatek,chn_emi"
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
int mt_dcm_dts_map(void)
{
	struct device_node *node;
	struct resource r;

	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", INFRACFG_AO_NODE);
		return -1;
	}
	dcm_infracfg_ao_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_infracfg_ao_base) {
		dcm_pr_err("error: cannot iomap %s\n", INFRACFG_AO_NODE);
		return -1;
	}

	/* topckgen_ao */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_AO_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", TOPCKGEN_AO_NODE);
		return -1;
	}
	dcm_topckgen_ao_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_topckgen_ao_base) {
		dcm_pr_err("error: cannot iomap %s\n", TOPCKGEN_AO_NODE);
		return -1;
	}

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", MCUCFG_NODE);
		return -1;
	}
	if (of_address_to_resource(node, 0, &r)) {
		dcm_pr_err("error: cannot get phys addr %s\n", MCUCFG_NODE);
		return -1;
	}
	dcm_mcucfg_phys_base = r.start;
	dcm_mcucfg_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_mcucfg_base) {
		dcm_pr_err("error: cannot iomap %s\n", MCUCFG_NODE);
		return -1;
	}

	/* dram related */
	node = of_find_compatible_node(NULL, NULL, DRAMC_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", DRAMC_NODE);
		return -1;
	}
	dcm_dramc_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_dramc_base) {
		dcm_pr_err("error: cannot iomap %s\n", DRAMC_NODE);
		return -1;
	}

	/* emi */
	node = of_find_compatible_node(NULL, NULL, EMI_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", EMI_NODE);
		return -1;
	}
	dcm_emi_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_emi_base) {
		dcm_pr_err("error: cannot iomap %s\n", EMI_NODE);
		return -1;
	}

#if 0
	node = of_find_compatible_node(NULL, NULL, CHN0_EMI_NODE);
	if (!node) {
		dcm_pr_err("error: cannot find node %s\n", CHN0_EMI_NODE);
		return -1;
	}
	dcm_chn0_emi_base = (unsigned long)of_iomap(node, 0);
#else
	dcm_chn0_emi_base = (unsigned long)of_iomap(node, 1);
#endif
	if (!dcm_chn0_emi_base) {
		dcm_pr_err("error: cannot iomap %s\n", CHN0_EMI_NODE);
		return -1;
	}

	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #ifdef CONFIG_PM */

int dcm_set_stall_wr_del_sel(unsigned int mp0, unsigned int mp1)
{
	return 0;
}

int sync_dcm_set_cci_freq(unsigned int cci)
{
	return 0;
}

int sync_dcm_set_mp0_freq(unsigned int mp0)
{
	return 0;
}

int sync_dcm_set_mp1_freq(unsigned int mp1)
{
	return 0;
}

int sync_dcm_set_mp2_freq(unsigned int mp2)
{
	return 0;
}

/* unit of frequency is MHz */
int sync_dcm_set_cpu_freq(unsigned int cci, unsigned int mp0,
			  unsigned int mp1, unsigned int mp2)
{
	sync_dcm_set_cci_freq(cci);
	sync_dcm_set_mp0_freq(mp0);
	sync_dcm_set_mp1_freq(mp1);
	sync_dcm_set_mp2_freq(mp2);

	return 0;
}

int sync_dcm_set_cpu_div(unsigned int cci, unsigned int mp0,
			 unsigned int mp1, unsigned int mp2)
{
	return 0;
}

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
int dcm_topckg(int on)
{
	return 0;
}

int dcm_infra(int on)
{
	/* dcm_infracfg_ao_dcm_dfs_mem_ctrl(on); */
	/* dcm_infracfg_ao_dcm_mem_ctrl(on); */
	dcm_infracfg_ao_dcm_infra_bus(on);
	dcm_infracfg_ao_dcm_peri_bus(on);
	dcm_infracfg_ao_dcm_top_p2p_rx_ck(on);

	return 0;
}

int dcm_peri(int on)
{
	return 0;
}

int dcm_armcore(int mode)
{
	dcm_topckgen_ao_mcu_armpll_ca7ll(mode);

	return 0;
}

int dcm_mcusys(int on)
{
	dcm_mcucfg_bus_clock_dcm(on);
	dcm_mcucfg_bus_fabric_dcm(on);
	dcm_mcucfg_l2_shared_dcm(on);
	dcm_mcucfg_mcu_misc_dcm(on);

	return 0;
}

int dcm_big_core(int on)
{
	return 0;
}

int dcm_stall_preset(void)
{
	return 0;
}

int dcm_stall(int on)
{
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
	dcm_dramc_dramc_dcm(on);

	return 0;
}

int dcm_ddrphy(int on)
{
	return 0;
}

int dcm_emi(int on)
{
	dcm_emi_dcm_emi_group(on);
	dcm_chn0_emi_dcm_emi_group(on);

	return 0;
}

int dcm_lpdma(int on)
{
	return 0;
}

struct DCM dcm_array[NR_DCM_TYPE] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .current_state = ARMCORE_DCM_MODE1,
	 .default_state = ARMCORE_DCM_MODE1,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .current_state = MCUSYS_DCM_ON,
	 .default_state = MCUSYS_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 /*.preset_func = (DCM_PRESET_FUNC) dcm_infra_preset,*/
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 /*.preset_func = (DCM_PRESET_FUNC) dcm_peri_preset,*/
	 .current_state = PERI_DCM_ON,
	 .default_state = PERI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .current_state = EMI_DCM_ON,
	 .default_state = EMI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = DRAMC_DCM_TYPE,
	 .name = "DRAMC_DCM",
	 .func = (DCM_FUNC) dcm_dramc_ao,
	 .current_state = DRAMC_AO_DCM_ON,
	 .default_state = DRAMC_AO_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = DDRPHY_DCM_TYPE,
	 .name = "DDRPHY_DCM",
	 .func = (DCM_FUNC) dcm_ddrphy,
	 .current_state = DDRPHY_DCM_ON,
	 .default_state = DDRPHY_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = STALL_DCM_TYPE,
	 .name = "STALL_DCM",
	 .func = (DCM_FUNC) dcm_stall,
	 .preset_func = (DCM_PRESET_FUNC) dcm_stall_preset,
	 .current_state = STALL_DCM_ON,
	 .default_state = STALL_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = BIG_CORE_DCM_TYPE,
	 .name = "BIG_CORE_DCM",
	 .func = (DCM_FUNC) dcm_big_core,
	 .current_state = BIG_CORE_DCM_ON,
	 .default_state = BIG_CORE_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = GIC_SYNC_DCM_TYPE,
	 .name = "GIC_SYNC_DCM",
	 .func = (DCM_FUNC) dcm_gic_sync,
	 .current_state = GIC_SYNC_DCM_ON,
	 .default_state = GIC_SYNC_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = LAST_CORE_DCM_TYPE,
	 .name = "LAST_CORE_DCM",
	 .func = (DCM_FUNC) dcm_last_core,
	 .current_state = LAST_CORE_DCM_ON,
	 .default_state = LAST_CORE_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = RGU_DCM_TYPE,
	 .name = "RGU_DCM",
	 .func = (DCM_FUNC) dcm_rgu,
	 .current_state = RGU_DCM_ON,
	 .default_state = RGU_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = TOPCKG_DCM_TYPE,
	 .name = "TOPCKG_DCM",
	 .func = (DCM_FUNC) dcm_topckg,
	 .current_state = TOPCKG_DCM_ON,
	 .default_state = TOPCKG_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = LPDMA_DCM_TYPE,
	 .name = "LPDMA_DCM",
	 .func = (DCM_FUNC) dcm_lpdma,
	 .current_state = LPDMA_DCM_ON,
	 .default_state = LPDMA_DCM_ON,
	 .disable_refcnt = 0,
	 },
};

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(MEM_DCM_CTRL);
	REG_DUMP(DFS_MEM_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(INFRA_TOPCKGEN_DCMCTL);
	REG_DUMP(DRAMC_DRAMC_PD_CTRL);
	REG_DUMP(L2C_SRAM_CTRL);
	REG_DUMP(CCI_CLK_CTRL);
	REG_DUMP(BUS_FABRIC_DCM_CTRL);
	REG_DUMP(MCU_MISC_DCM_CTRL);
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONN);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);
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
