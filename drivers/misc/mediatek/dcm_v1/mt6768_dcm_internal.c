/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/io.h>
//#include <mt-plat/mtk_io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/mtk_secure_api.h>

#include <mt6768_dcm_internal.h>

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

static short dcm_cpu_cluster_stat;

#if 0 /* 6768 doesn't need */
#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
static struct notifier_block dcm_hotplug_nb;
#endif
#endif

unsigned int all_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE | DDRPHY_DCM_TYPE | EMI_DCM_TYPE
		| DRAMC_DCM_TYPE);
unsigned int init_dcm_type =
		(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | STALL_DCM_TYPE |
		INFRA_DCM_TYPE);

#if IS_ENABLED(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_infracfg_ao_base;
/*unsigned long dcm_pwrap_base;*/
unsigned long dcm_mcucfg_base;/*Todo: dts node missing*/
unsigned long dcm_cpccfg_rg_base;/*Todo: dts node missing*/

/* dramc */
unsigned long dcm_dramc_ch0_top0_ao_base;
unsigned long dcm_dramc_ch1_top0_ao_base;

/* ddrphy */
unsigned long dcm_dramc_ch0_top1_ao_base;
unsigned long dcm_dramc_ch1_top1_ao_base;

/* emi */
unsigned long dcm_ch0_emi_base;
unsigned long dcm_ch1_emi_base;
unsigned long dcm_emi_base;


#define INFRACFG_AO_NODE	"mediatek,infracfg_ao"
/*#define PWRAP_NODE		"mediatek,pwrap"*/
#define MCUCFG_NODE		"mediatek,mcucfg_mp0_counter"
/*#define CPCCFG_NODE		"mediatek,mt6768-cpc"*/
#define DRAMC_NODE		"mediatek,common-dramc"
#define EMI_CEN_NODE	"mediatek,mt6779-emicen"
#define EMI_CHN_NODE	"mediatek,mt6779-emichn"


#endif /* #if IS_ENABLED(__KERNEL__) && IS_ENABLED(CONFIG_OF) */

short is_dcm_bringup(void)
{
#ifdef DCM_BRINGUP
	dcm_pr_info("%s: skipped for bring up\n", __func__);
	return 1;
#else
	return 0;
#endif
}

#if IS_ENABLED(CONFIG_OF)
int mt_dcm_dts_map(void)
{
	struct device_node *node;

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
	dcm_cpccfg_rg_base = dcm_mcucfg_base;
	dcm_mcucfg_base += 0x8000;
	dcm_cpccfg_rg_base += 0xA800;

	/* dram related */
	node = of_find_compatible_node(NULL, NULL, DRAMC_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DRAMC_NODE);
		return -1;
	}
	/* dramc ch0*/
	dcm_dramc_ch0_top0_ao_base = (unsigned long)of_iomap(node, 4);
	if (!dcm_dramc_ch0_top0_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch0 t0\n");
		return -1;
	}
	/* dramc ch1*/
	dcm_dramc_ch1_top0_ao_base = (unsigned long)of_iomap(node, 5);
	if (!dcm_dramc_ch1_top0_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch1 t0\n");
		return -1;
	}
	/* ddrphy ch0*/
	dcm_dramc_ch0_top1_ao_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_dramc_ch0_top1_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch0 t1\n");
		return -1;
	}
	/* ddrphy ch1*/
	dcm_dramc_ch1_top1_ao_base = (unsigned long)of_iomap(node, 1);
	if (!dcm_dramc_ch1_top1_ao_base) {
		dcm_pr_info("error: cannot iomap dramc ch1 t1\n");
		return -1;
	}

	/* EMI CEN related */
	node = of_find_compatible_node(NULL, NULL, EMI_CEN_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", EMI_CEN_NODE);
		return -1;
	}
	/* cen emi */
	dcm_emi_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_emi_base) {
		dcm_pr_info("error: cannot iomap %s\n", EMI_CEN_NODE);
		return -1;
	}

	/* EMI CHN related */
	node = of_find_compatible_node(NULL, NULL, EMI_CHN_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", EMI_CHN_NODE);
		return -1;
	}
	/* chn emi */
	dcm_ch0_emi_base = (unsigned long)of_iomap(node, 0);
	if (!dcm_ch0_emi_base) {
		dcm_pr_info("error: cannot iomap emi ch0\n");
		return -1;
	}
	dcm_ch1_emi_base = (unsigned long)of_iomap(node, 1);
	if (!dcm_ch1_emi_base) {
		dcm_pr_info("error: cannot iomap emi ch1\n");
		return -1;
	}


	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #if IS_ENABLED(CONFIG_PM) */




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
	dcm_infracfg_ao_infra_bus(on);
	dcm_infracfg_ao_peri_bus(on);

	dcm_infracfg_ao_audio_bus(on);
	dcm_infracfg_ao_icusb_bus(on);

	return 0;
}

int dcm_peri(int on)
{
	return 0;
}

int dcm_armcore(int mode)
{
	dcm_mp_cpusys_top_bus_pll_div_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_0_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_1_dcm(mode);
	dcm_mp_cpusys_top_cpu_pll_div_2_dcm(mode);

	return 0;
}

int dcm_mcusys(int on)
{
	dcm_mp_cpusys_top_adb_dcm(on);
	dcm_mp_cpusys_top_apb_dcm(on);
	dcm_mp_cpusys_top_cpubiu_dbg_cg(on);
	dcm_mp_cpusys_top_cpubiu_dcm(on);
	dcm_mp_cpusys_top_misc_dcm(on);
	dcm_mp_cpusys_top_mp0_qdcm(on);
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
	dcm_dramc_ch0_top1_dramc_dcm(on);
	dcm_dramc_ch1_top1_dramc_dcm(on);
	return 0;
}

int dcm_ddrphy(int on)
{
	dcm_dramc_ch0_top0_ddrphy(on);
	dcm_dramc_ch1_top0_ddrphy(on);
	return 0;
}

int dcm_emi(int on)
{
	dcm_emi_emi_dcm(on);
	dcm_chn0_emi_emi_dcm(on);
	dcm_chn1_emi_emi_dcm(on);
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

void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");
	REG_DUMP(CPU_PLLDIV_CFG0);
	REG_DUMP(CPU_PLLDIV_CFG1);
	REG_DUMP(CPU_PLLDIV_CFG2);
	REG_DUMP(BUS_PLLDIV_CFG);
	REG_DUMP(MCSI_CFG2);
	REG_DUMP(MCSI_DCM0);
	REG_DUMP(MP_ADB_DCM_CFG4);
	REG_DUMP(MP_MISC_DCM_CFG0);
	REG_DUMP(MCUSYS_DCM_CFG0);
	REG_DUMP(EMI_WFIFO);
	REG_DUMP(MP0_DCM_CFG0);
	REG_DUMP(MP0_DCM_CFG7);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(MEM_DCM_CTRL);
	REG_DUMP(DFS_MEM_DCM_CTRL);
	/*REG_DUMP(PMIC_WRAP_DCM_EN);*/
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONN);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH0_TOP0_MISC_CTRL3);
	REG_DUMP(DRAMC_CH0_TOP1_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH0_TOP1_CLKAR);
	REG_DUMP(CHN0_EMI_CHN_EMI_CONB);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CG_CTRL0);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CG_CTRL2);
	REG_DUMP(DRAMC_CH1_TOP0_MISC_CTRL3);
	REG_DUMP(DRAMC_CH1_TOP1_DRAMC_PD_CTRL);
	REG_DUMP(DRAMC_CH1_TOP1_CLKAR);
	REG_DUMP(CHN1_EMI_CHN_EMI_CONB);

}

#if 0 /* 6768 doesn't need */
#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
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
#endif /* #if IS_ENABLED(CONFIG_HOTPLUG_CPU) */
#endif

void dcm_set_hotplug_nb(void)
{
#if 0 /* 6768 doesn't need */
#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
	dcm_hotplug_nb = (struct notifier_block) {
		.notifier_call	= dcm_hotplug_nc,
		.priority	= INT_MIN + 2,
			/* NOTE: make sure this is < CPU DVFS */
	};

	if (register_cpu_notifier(&dcm_hotplug_nb))
		dcm_pr_info("[%s]: fail to register_cpu_notifier\n", __func__);
#endif /* #if IS_ENABLED(CONFIG_HOTPLUG_CPU) */
#endif
}

short dcm_get_cpu_cluster_stat(void)
{
	return dcm_cpu_cluster_stat;
}
