// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <mtk_sip_svc.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
/* #include <mt-plat/mtk_io.h> */
/* #include <mt-plat/sync_write.h> */
/* include <mt-plat/mtk_secure_api.h> */
#include "mt6789_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

static short dcm_cpu_cluster_stat;


unsigned int all_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | DDRPHY_DCM_TYPE | EMI_DCM_TYPE
		| DRAMC_DCM_TYPE | BIG_CORE_DCM_TYPE);

unsigned int init_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | BIG_CORE_DCM_TYPE);

#if defined(__KERNEL__) && defined(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_mcusys_par_wrap_base;
unsigned long dcm_mp_cpusys_top_base;
unsigned long dcm_cpccfg_reg_base;

#define DCM_NODE "mediatek,mt6789-dcm"

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

/* SMC call for mcusys_par_wrap_big_dcm initializations */
static int dcm_smc_call_control(int onoff, unsigned int mask);
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

int dcm_mcusys_acp(int on)
{
	return 0;
}

int dcm_mcusys_adb(int on)
{
	return 0;
}

int dcm_mcusys_bus(int on)
{
	return 0;
}

int dcm_mcusys_cbip(int on)
{
	return 0;
}

int dcm_mcusys_core(int on)
{
	return 0;
}

int dcm_mcusys_io(int on)
{
	return 0;
}

int dcm_mcusys_cpc_pbi(int on)
{
	return 0;
}

int dcm_mcusys_cpc_turbo(int on)
{
	return 0;
}

int dcm_mcusys_stall(int on)
{
	return 0;
}

int dcm_mcusys_apb(int on)
{
	return 0;
}

int dcm_vlp(int on)
{
	return 0;
}

int dcm_armcore(int on)
{

	return 0;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_core_stall_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_fcm_stall_dcm(on);
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
	// TODO: Modify the value of BIG_CORE_DCM_TYPE
	if (0) {
		dcm_mcusys_par_wrap_big_dcm(on);
		dcm_smc_call_control(on, BIG_CORE_DCM_TYPE);
	}

	return 0;
}

int dcm_stall_preset(int on)
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

	return 0;
}

int dcm_ddrphy(int on)
{
	return 0;
}

int dcm_emi(int on)
{
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

	REG_DUMP(MCUSYS_PAR_WRAP_STALL_DCM_CONF);
	REG_DUMP(MP_CPUSYS_TOP_BUS_PLLDIV_CFG);
	REG_DUMP(MP_CPUSYS_TOP_MCSIC_DCM0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4);
	REG_DUMP(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0);
	REG_DUMP(CPCCFG_REG_EMI_WFIFO);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG0);
	REG_DUMP(MP_CPUSYS_TOP_MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(MODULE_SW_CG_2_SET);
	REG_DUMP(MODULE_SW_CG_2_CLR);
	REG_DUMP(MODULE_SW_CG_2_STA);
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_10);
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
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_mcusys_par_wrap_base),
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
	 .typeid = MCUSYS_ACP_DCM_TYPE,
	 .name = "MCU_ACP_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_acp,
	 .current_state = MCUSYS_ACP_DCM_ON,
	 .default_state = MCUSYS_ACP_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_ADB_DCM_TYPE,
	 .name = "MCU_ADB_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_adb,
	 .current_state = MCUSYS_ADB_DCM_ON,
	 .default_state = MCUSYS_ADB_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_BUS_DCM_TYPE,
	 .name = "MCU_BUS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_bus,
	 .current_state = MCUSYS_BUS_DCM_ON,
	 .default_state = MCUSYS_BUS_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_CBIP_DCM_TYPE,
	 .name = "MCU_CBIP_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cbip,
	 .current_state = MCUSYS_CBIP_DCM_ON,
	 .default_state = MCUSYS_CBIP_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_CORE_DCM_TYPE,
	 .name = "MCU_CORE_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_core,
	 .current_state = MCUSYS_CORE_DCM_ON,
	 .default_state = MCUSYS_CORE_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_IO_DCM_TYPE,
	 .name = "MCU_IO_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_io,
	 .current_state = MCUSYS_IO_DCM_ON,
	 .default_state = MCUSYS_IO_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_CPC_PBI_DCM_TYPE,
	 .name = "MCU_CPC_PBI_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cpc_pbi,
	 .current_state = MCUSYS_CPC_PBI_DCM_ON,
	 .default_state = MCUSYS_CPC_PBI_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_CPC_TURBO_DCM_TYPE,
	 .name = "MCU_CPC_TURBO_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_cpc_turbo,
	 .current_state = MCUSYS_CPC_TURBO_DCM_ON,
	 .default_state = MCUSYS_CPC_TURBO_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_STALL_DCM_TYPE,
	 .name = "MCU_STALL_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_stall,
	 .current_state = MCUSYS_STALL_DCM_ON,
	 .default_state = MCUSYS_STALL_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = MCUSYS_APB_DCM_TYPE,
	 .name = "MCU_APB_DCM",
	 .func = (DCM_FUNC) dcm_mcusys_apb,
	 .current_state = MCUSYS_APB_DCM_ON,
	 .default_state = MCUSYS_APB_DCM_ON,
	 .disable_refcnt = 0,
	},
	{
	 .typeid = VLP_DCM_TYPE,
	 .name = "VLP_DCM",
	 .func = (DCM_FUNC) dcm_vlp,
	 .current_state = VLP_DCM_ON,
	 .default_state = VLP_DCM_ON,
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

void dcm_array_register(void)
{
	mt_dcm_array_register(dcm_array, &dcm_ops);
}

/*From DCM COMMON*/

#if IS_ENABLED(CONFIG_OF)
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
#endif /* #if IS_ENABLED(CONFIG_PM) */


void dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

static int dcm_smc_call_control(int onoff, unsigned int mask)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_DCM, onoff, mask, 0, 0, 0, 0, 0, &res);

	return 0;
}

static int __init mt6789_dcm_init(void)
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

static void __exit mt6789_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6789_dcm_init);
module_exit(mt6789_dcm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek DCM driver");
