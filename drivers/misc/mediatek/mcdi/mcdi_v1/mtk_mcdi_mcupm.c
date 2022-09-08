// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <stdbool.h>
#include <mtk_mcdi.h>
#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_mcupm.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_governor.h>
//#include <mt-plat/mtk_secure_api.h>

#ifdef MCDI_MCUPM_INTF

void __iomem *mcdi_mcupm_base;
void __iomem *mcdi_mcupm_sram_base;
bool mcupm_sram_is_ready;
static int mcupm_fw_load_success = -1;

/*
 * Use mcupm_fw_load_success to verify mcupm fw is ready or not
 * 0 means load fail
 * 1 means load success
 */
int get_mcupmfw_load_info(void)
{
	return mcupm_fw_load_success;
}

void set_mcupmfw_load_info(int value)
{
	/* limit the value to 0 or 1 */
	value %= 2;
	mcupm_fw_load_success = value;
}

/* notify mcupm the residency of current idle cpu */
/* set cpu residency into MCUPM_CFGREG_MPx_CPUx_RES */
void mcdi_cluster_counter_set_cpu_residency(int cpu)
{
#if MCUPM_CLUSTER_COUNTER_EN
	unsigned int i;

	if (cpu_is_invalid(cpu))
		return;

	i = cluster_idx_get(cpu);
	/* DVT test */
	/* target_residency = 0xfffff; */
	mcdi_write(mcdi_mcupm_base + MCUPM_CFGREG_MP_CPU0_RES[i] + (cpu * 4),
			get_mcdi_cpu_next_timer_us(cpu));
#endif
}

/*
 * set cluster threshold to MCUPM_CFGREG_MPx_SLEEP_TH and
 * enable cluster counter
 */
static void mcdi_enable_mcupm_cluster_counter(void)
{
#if MCUPM_CLUSTER_COUNTER_EN
	int i = 0;
	int idx = 0;
	unsigned int thre = 0;
	struct cpuidle_driver *tbl = NULL;
	unsigned int state = MCDI_STATE_CLUSTER_OFF;
	struct cpuidle_state *s = NULL;

	for (i = 0; i < NF_CPU; i++) {
		if (tbl == mcdi_state_tbl_get(i))
			continue;
		idx = cluster_idx_get(i);
		tbl = mcdi_state_tbl_get(i);
		s = &tbl->states[state];
		thre = s->target_residency;
		mcdi_write((mcdi_mcupm_base + MCUPM_CFGREG_MP_SLEEP_TH[idx]),
			(thre | (CLUSTER_COUNTER_ENABLE << 31)));
		pr_info("[mcdi] [%s] cls_thre(%d)=0x%08x\n", __func__, idx,
			mcdi_read(mcdi_mcupm_base +
				  MCUPM_CFGREG_MP_SLEEP_TH[idx]));
	}
#endif
}

void mcdi_mcupm_init(void)
{
	mcdi_enable_mcupm_cluster_counter();
}

bool mcdi_mcupm_sram_is_ready(void)
{
	return mcupm_sram_is_ready;
}

#else

void mcdi_cluster_counter_set_cpu_residency(int cpu) {}
void mcdi_mcupm_init(void) {}
bool mcdi_mcupm_sram_is_ready(void) { return true; }

#endif /* #ifdef MCDI_MCUPM_INTF */
