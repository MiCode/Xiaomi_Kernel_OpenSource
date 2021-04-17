/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __THERMAL_INTERFACE_H__
#define __THERMAL_INTERFACE_H__

#define CPU_TEMP_OFFSET             (0)
#define CPU_HEADROOM_OFFSET         (0x20)
#define CPU_HEADROOM_RATIO_OFFSET   (0x40)
#define CPU_PREDICT_TEMP_OFFSET     (0x60)
#define AP_NTC_HEADROOM_OFFSET      (0x80)
#define TTJ_OFFSET                 (0x100)
#define POWER_BUDGET_OFFSET        (0x110)
#define CPU_MIN_OPP_HINT_OFFSET    (0x120)

struct headroom_info {
    int temp;
    int predict_temp;
    int headroom;
    int ratio;
};
enum headroom_id {
	/* SoC Tj */
	SOC_CPU0,
	SOC_CPU1,
	SOC_CPU2,
	SOC_CPU3,
	SOC_CPU4,
	SOC_CPU5,
	SOC_CPU6,
	SOC_CPU7,
	/* PCB */
	PCB_AP,

	NR_HEADROOM_ID
};

extern void update_ap_ntc_headroom(int temp, int polling_interval);
extern int get_thermal_headroom(enum headroom_id id);
extern int set_cpu_min_opp(int gear, int opp);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern void __iomem * thermal_csram_base;
#else
void __iomem * thermal_csram_base;
#endif
#endif
