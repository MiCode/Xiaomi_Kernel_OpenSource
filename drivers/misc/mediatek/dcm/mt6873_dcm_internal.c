// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include "mt6873_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

static short dcm_cpu_cluster_stat;


unsigned int all_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | DDRPHY_DCM_TYPE | EMI_DCM_TYPE
		| DRAMC_DCM_TYPE);
unsigned int init_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE);

#if defined(__KERNEL__) && defined(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_infracfg_ao_mem_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_emi_base;
unsigned long dcm_dramc_ch0_top0_base;
unsigned long dcm_chn0_emi_base;
unsigned long dcm_dramc_ch0_top5_base;
unsigned long dcm_dramc_ch1_top0_base;
unsigned long dcm_dramc_ch1_top5_base;
unsigned long dcm_sspm_base;
unsigned long dcm_audio_base;
unsigned long dcm_mp_cpusys_top_base;
unsigned long dcm_cpccfg_reg_base;

#define DCM_NODE "mediatek,mt6873-dcm"

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

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
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

int dcm_infra(int on)
{
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	dcm_infracfg_ao_infra_conn_bus_dcm(on);
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infracfg_ao_peri_bus_dcm(on);
	dcm_infracfg_ao_peri_module_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_peri_bus_dcm(on);

	return 0;
}

int dcm_peri(int on)
{
	return 0;
}

int dcm_armcore(int on)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(on);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(on);

	return 0;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_last_cor_idle_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
	dcm_cpccfg_reg_emi_wfifo(on);

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
	dcm_dramc_ch0_top0_ddrphy(on);
	dcm_dramc_ch0_top5_ddrphy(on);
	dcm_dramc_ch1_top0_ddrphy(on);
	dcm_dramc_ch1_top5_ddrphy(on);

	return 0;
}

int dcm_emi(int on)
{
	dcm_emi_emi_dcm(on);
	dcm_chn0_emi_chn_emi_dcm(on);

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

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");
	/* MCUSYS reg */
	REG_DUMP(MP_ADB_DCM_CFG0);
	REG_DUMP(MP_ADB_DCM_CFG4);
	REG_DUMP(MCUSYS_DCM_CFG0);
	REG_DUMP(MP_MISC_DCM_CFG0);
	REG_DUMP(MP0_DCM_CFG0);
	REG_DUMP(MCSI_DCM0);
	REG_DUMP(BUS_PLLDIV_CFG);
	REG_DUMP(EMI_WFIFO);

	/* ARMCORE reg */
	REG_DUMP(CPU_PLLDIV_CFG0);
	REG_DUMP(CPU_PLLDIV_CFG1);

	/* STALL reg */
	REG_DUMP(MP0_DCM_CFG7);

	/* INFRA_AO reg */
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(MODULE_SW_CG_2_SET);
	REG_DUMP(MODULE_SW_CG_2_CLR);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(TOP_CK_ANCHOR_CFG);

	/* INFRA_AO_MEM reg */

	/* SUB INFRA reg */

	/* INFRA EMI reg */
	REG_DUMP(INFRA_EMI_DCM_CFG0);
	REG_DUMP(INFRA_EMI_DCM_CFG1);
	REG_DUMP(INFRA_EMI_DCM_CFG3);

	/* emi */
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONN);
	REG_DUMP(EMI_THRO_CTRL0);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);

	/* dramc */

	/* ddrphy */
	REG_DUMP(DRAMC_CH0_TOP0_DDRCOMMON0);
	REG_DUMP(DRAMC_CH0_TOP0_TEST2_A0);
	REG_DUMP(DRAMC_CH0_TOP0_TEST2_A3);
	REG_DUMP(DRAMC_CH0_TOP0_DUMMY_RD);
	REG_DUMP(DRAMC_CH0_TOP0_SREF_DPD_CTRL);
	REG_DUMP(DRAMC_CH0_TOP0_ACTIMING_CTRL);
	REG_DUMP(DRAMC_CH0_TOP0_ZQ_SET0);
	REG_DUMP(DRAMC_CH0_TOP0_TX_TRACKING_SET0);
	REG_DUMP(DRAMC_CH0_TOP0_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH0_TOP0_DCM_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP0_DVFS_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP0_CMD_DEC_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP0_TX_CG_SET0);
	REG_DUMP(DRAMC_CH0_TOP0_RX_CG_SET0);
	REG_DUMP(DRAMC_CH0_TOP0_MISCTL0);
	REG_DUMP(DRAMC_CH0_TOP0_CLKAR);
	REG_DUMP(DRAMC_CH0_TOP0_SCSMCTRL_CG);
	REG_DUMP(DRAMC_CH0_TOP0_SHU_APHY_TX_PICG_CTRL);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CG_CTRL5);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_DUTYSCAN1);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CTRL3);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_CTRL4);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_RX_AUTOK_CFG0);
	REG_DUMP(DRAMC_CH0_TOP5_SHU_B0_DQ7);
	REG_DUMP(DRAMC_CH0_TOP5_SHU_B0_DQ8);
	REG_DUMP(DRAMC_CH0_TOP5_SHU_B1_DQ7);
	REG_DUMP(DRAMC_CH0_TOP5_SHU_B1_DQ8);
	REG_DUMP(DRAMC_CH0_TOP5_SHU_CA_CMD8);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_SHU_RX_CG_CTRL);
	REG_DUMP(DRAMC_CH0_TOP5_MISC_SHU_CG_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_DDRCOMMON0);
	REG_DUMP(DRAMC_CH1_TOP0_TEST2_A0);
	REG_DUMP(DRAMC_CH1_TOP0_TEST2_A3);
	REG_DUMP(DRAMC_CH1_TOP0_DUMMY_RD);
	REG_DUMP(DRAMC_CH1_TOP0_SREF_DPD_CTRL);
	REG_DUMP(DRAMC_CH1_TOP0_ACTIMING_CTRL);
	REG_DUMP(DRAMC_CH1_TOP0_ZQ_SET0);
	REG_DUMP(DRAMC_CH1_TOP0_TX_TRACKING_SET0);
	REG_DUMP(DRAMC_CH1_TOP0_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH1_TOP0_DCM_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_DVFS_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_CMD_DEC_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_TX_CG_SET0);
	REG_DUMP(DRAMC_CH1_TOP0_RX_CG_SET0);
	REG_DUMP(DRAMC_CH1_TOP0_MISCTL0);
	REG_DUMP(DRAMC_CH1_TOP0_CLKAR);
	REG_DUMP(DRAMC_CH1_TOP0_SCSMCTRL_CG);
	REG_DUMP(DRAMC_CH1_TOP0_SHU_APHY_TX_PICG_CTRL);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CG_CTRL5);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_DUTYSCAN1);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CTRL3);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_CTRL4);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_RX_AUTOK_CFG0);
	REG_DUMP(DRAMC_CH1_TOP5_SHU_B0_DQ7);
	REG_DUMP(DRAMC_CH1_TOP5_SHU_B0_DQ8);
	REG_DUMP(DRAMC_CH1_TOP5_SHU_B1_DQ7);
	REG_DUMP(DRAMC_CH1_TOP5_SHU_B1_DQ8);
	REG_DUMP(DRAMC_CH1_TOP5_SHU_CA_CMD8);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_SHU_RX_CG_CTRL);
	REG_DUMP(DRAMC_CH1_TOP5_MISC_SHU_CG_CTRL0);
}

void get_default(unsigned int *type, int *state)
{
#ifndef DCM_DEFAULT_ALL_OFF
	/** enable all dcm **/
	*type = init_dcm_type;
	*state = DCM_DEFAULT;
#else /* DCM_DEFAULT_ALL_OFF */
	*type = all_dcm_type;
	*state = DCM_OFF;
#endif /* #ifndef DCM_DEFAULT_ALL_OFF */
}

void get_init_type(unsigned int *type)
{
	*type = init_dcm_type;
}
void get_all_type(unsigned int *type)
{
	*type = all_dcm_type;
}
void get_init_by_k_type(unsigned int *type)
{
#ifdef ENABLE_DCM_IN_LK
	*type = INIT_DCM_TYPE_BY_K;
#else
	*type = init_dcm_type;
#endif
}

struct DCM_OPS dcm_ops = {
	.dump_regs = (DCM_FUNC_VOID_VOID) dcm_dump_regs,
	.get_default = (DCM_FUNC_VOID_UINTR_INTR) get_default,
	.get_init_type = (DCM_FUNC_VOID_UINTR) get_init_type,
	.get_all_type = (DCM_FUNC_VOID_UINTR) get_all_type,
	.get_init_by_k_type = (DCM_FUNC_VOID_UINTR) get_init_by_k_type,
};

struct DCM_BASE dcm_base_array[] = {
	DCM_BASE_INFO(dcm_infracfg_ao_base),
	DCM_BASE_INFO(dcm_infracfg_ao_mem_base),
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch0_top0_base),
	DCM_BASE_INFO(dcm_chn0_emi_base),
	DCM_BASE_INFO(dcm_dramc_ch0_top5_base),
	DCM_BASE_INFO(dcm_dramc_ch1_top0_base),
	DCM_BASE_INFO(dcm_dramc_ch1_top5_base),
	DCM_BASE_INFO(dcm_sspm_base),
	DCM_BASE_INFO(dcm_audio_base),
	DCM_BASE_INFO(dcm_mp_cpusys_top_base),
	DCM_BASE_INFO(dcm_cpccfg_reg_base),
};

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
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
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
	 .name = "RGU_CORE_DCM",
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
	{
	 .typeid = MCSI_DCM_TYPE,
	 .name = "MCSI_DCM",
	 .func = (DCM_FUNC) dcm_mcsi,
	 .current_state = MCSI_DCM_ON,
	 .default_state = MCSI_DCM_ON,
	 .disable_refcnt = 0,
	 },
};

void dcm_set_hotplug_nb(void) {}

short dcm_get_cpu_cluster_stat(void)
{
	return dcm_cpu_cluster_stat;
}

/**/
void dcm_array_register(void)
{
	mt_dcm_array_register(dcm_array, &dcm_ops);
}

/*From DCM COMMON*/

#ifdef CONFIG_OF
int mt_dcm_dts_map(void)
{
	struct device_node *node;
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
	/* infracfg_ao */
	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #ifdef CONFIG_PM */


void dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

static int __init mt6873_dcm_init(void)
{
	int ret = 0;

	if (is_dcm_bringup())
		return 0;

	if (is_dcm_initialized())
		return 0;

	if (mt_dcm_dts_map()) {
		dcm_pr_notice("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	dcm_array_register();

	ret = mt_dcm_common_init();

	return ret;
}

static void __exit mt6873_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6873_dcm_init);
module_exit(mt6873_dcm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek DCM driver");
