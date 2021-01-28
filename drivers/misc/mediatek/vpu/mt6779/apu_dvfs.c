// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <mt-plat/upmu_common.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#ifdef CONFIG_MTK_DEVINFO
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>
#endif
#include <mtk_qos_sram.h>
#include <linux/delay.h>
#include "vpu_reg.h"


//#include "governor.h"

#include <mt-plat/aee.h>
#include <spm/mtk_spm.h>

#include "apu_dvfs.h"
#include "vpu_cmn.h"

#include <linux/regulator/consumer.h>

#define CCF_GET_CKGEN_READY	(1)

static uint32_t g_efuse_ptpod;
static inline uint32_t get_devinfo_with_index(int idx)
{
	return g_efuse_ptpod;
}

/*regulator id*/
static struct regulator *vvpu_reg_id;
static struct regulator *vmdla_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;


static bool vvpu_DVFS_is_paused_by_ptpod;
static bool vmdla_DVFS_is_paused_by_ptpod;
static bool ready_for_ptpod_check;
static bool ptpod_enabled;


static bool vpu_opp_ready;
static bool mdla_opp_ready;
#define VPU_DVFS_OPP_MAX	  (16)
#define MDLA_DVFS_OPP_MAX	  (16)

static int apu_power_count;
static int vvpu_count;
static int vmdla_count;


#define VPU_DVFS_FREQ0	 (700000)	/* KHz */
#define VPU_DVFS_FREQ1	 (624000)	/* KHz */
#define VPU_DVFS_FREQ2	 (606000)	/* KHz */
#define VPU_DVFS_FREQ3	 (594000)	/* KHz */
#define VPU_DVFS_FREQ4	 (560000)	/* KHz */
#define VPU_DVFS_FREQ5	 (525000)	/* KHz */
#define VPU_DVFS_FREQ6	 (450000)	/* KHz */
#define VPU_DVFS_FREQ7	 (416000)	/* KHz */
#define VPU_DVFS_FREQ8	 (364000)	/* KHz */
#define VPU_DVFS_FREQ9	 (312000)	/* KHz */
#define VPU_DVFS_FREQ10	  (273000)	 /* KHz */
#define VPU_DVFS_FREQ11	  (208000)	 /* KHz */
#define VPU_DVFS_FREQ12	  (137000)	 /* KHz */
#define VPU_DVFS_FREQ13	  (104000)	 /* KHz */
#define VPU_DVFS_FREQ14	  (52000)	 /* KHz */
#define VPU_DVFS_FREQ15	  (26000)	 /* KHz */


#ifdef AGING_MARGIN
#define VPU_DVFS_VOLT0	 (81250)	/* mV x 100 */
#define VPU_DVFS_VOLT1	 (81250)	/* mV x 100 */
#define VPU_DVFS_VOLT2	 (81250)	/* mV x 100 */
#define VPU_DVFS_VOLT3	 (81250)	/* mV x 100 */
#define VPU_DVFS_VOLT4	 (81250)	/* mV x 100 */
#define VPU_DVFS_VOLT5	 (71250)	/* mV x 100 */
#define VPU_DVFS_VOLT6	 (71250)	/* mV x 100 */
#define VPU_DVFS_VOLT7	 (71250) /* mV x 100 */
#define VPU_DVFS_VOLT8	 (71250) /* mV x 100 */
#define VPU_DVFS_VOLT9	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT10	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT11	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT12	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT13	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT14	 (63750)	/* mV x 100 */
#define VPU_DVFS_VOLT15	 (63750)	/* mV x 100 */
#else
#define VPU_DVFS_VOLT0	 (82500)	/* mV x 100 */
#define VPU_DVFS_VOLT1	 (82500)	/* mV x 100 */
#define VPU_DVFS_VOLT2	 (82500)	/* mV x 100 */
#define VPU_DVFS_VOLT3	 (82500)	/* mV x 100 */
#define VPU_DVFS_VOLT4	 (82500)	/* mV x 100 */
#define VPU_DVFS_VOLT5	 (72500)	/* mV x 100 */
#define VPU_DVFS_VOLT6	 (72500)	/* mV x 100 */
#define VPU_DVFS_VOLT7	 (72500) /* mV x 100 */
#define VPU_DVFS_VOLT8	 (72500) /* mV x 100 */
#define VPU_DVFS_VOLT9	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT10	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT11	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT12	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT13	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT14	 (65000)	/* mV x 100 */
#define VPU_DVFS_VOLT15	 (65000)	/* mV x 100 */
#endif

#define MDLA_DVFS_FREQ0	 (788000)	/* KHz */
#define MDLA_DVFS_FREQ1	 (700000)	/* KHz */
#define MDLA_DVFS_FREQ2	 (624000)	/* KHz */
#define MDLA_DVFS_FREQ3	 (606000)	/* KHz */
#define MDLA_DVFS_FREQ4	 (594000)	/* KHz */
#define MDLA_DVFS_FREQ5	 (546000)	/* KHz */
#define MDLA_DVFS_FREQ6	 (525000)	/* KHz */
#define MDLA_DVFS_FREQ7	 (450000)	/* KHz */
#define MDLA_DVFS_FREQ8	 (416000)	/* KHz */
#define MDLA_DVFS_FREQ9	 (364000)	/* KHz */
#define MDLA_DVFS_FREQ10	  (312000)	 /* KHz */
#define MDLA_DVFS_FREQ11	  (273000)	 /* KHz */
#define MDLA_DVFS_FREQ12	  (208000)	 /* KHz */
#define MDLA_DVFS_FREQ13	  (137000)	 /* KHz */
#define MDLA_DVFS_FREQ14	  (52000)	 /* KHz */
#define MDLA_DVFS_FREQ15	  (26000)	 /* KHz */

#ifdef AGING_MARGIN
#define MDLA_DVFS_VOLT0	 (81500)	/* mV x 100 */
#define MDLA_DVFS_VOLT1	 (81500)	/* mV x 100 */
#define MDLA_DVFS_VOLT2	 (81500)	/* mV x 100 */
#define MDLA_DVFS_VOLT3	 (71500)	/* mV x 100 */
#define MDLA_DVFS_VOLT4	 (71500)	/* mV x 100 */
#define MDLA_DVFS_VOLT5	 (71500)	/* mV x 100 */
#define MDLA_DVFS_VOLT6	 (71500)	/* mV x 100 */
#define MDLA_DVFS_VOLT7	 (71500) /* mV x 100 */
#define MDLA_DVFS_VOLT8	 (71500) /* mV x 100 */
#define MDLA_DVFS_VOLT9	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT10	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT11	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT12	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT13	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT14	 (64000)	/* mV x 100 */
#define MDLA_DVFS_VOLT15	 (64000)	/* mV x 100 */
#else
#define MDLA_DVFS_VOLT0	 (82500)	/* mV x 100 */
#define MDLA_DVFS_VOLT1	 (82500)	/* mV x 100 */
#define MDLA_DVFS_VOLT2	 (82500)	/* mV x 100 */
#define MDLA_DVFS_VOLT3	 (72500)	/* mV x 100 */
#define MDLA_DVFS_VOLT4	 (72500)	/* mV x 100 */
#define MDLA_DVFS_VOLT5	 (72500)	/* mV x 100 */
#define MDLA_DVFS_VOLT6	 (72500)	/* mV x 100 */
#define MDLA_DVFS_VOLT7	 (72500) /* mV x 100 */
#define MDLA_DVFS_VOLT8	 (72500) /* mV x 100 */
#define MDLA_DVFS_VOLT9	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT10	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT11	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT12	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT13	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT14	 (65000)	/* mV x 100 */
#define MDLA_DVFS_VOLT15	 (65000)	/* mV x 100 */
#endif

#define VPUOP(khz, volt, idx) \
{ \
	.vpufreq_khz = khz, \
	.vpufreq_volt = volt, \
	.vpufreq_idx = idx, \
}
#define MDLAOP(khz, volt, idx) \
{ \
	.mdlafreq_khz = khz, \
	.mdlafreq_volt = volt, \
	.mdlafreq_idx = idx, \
}
#define VPU_PTP(ptp_count) \
{ \
	.vpu_ptp_count = ptp_count, \
}
#define MDLA_PTP(ptp_count) \
{ \
	.mdla_ptp_count = ptp_count, \
}


static struct vpu_opp_table_info vpu_opp_table_default[] = {
	VPUOP(VPU_DVFS_FREQ0, VPU_DVFS_VOLT0,  0),
	VPUOP(VPU_DVFS_FREQ1, VPU_DVFS_VOLT1,  1),
	VPUOP(VPU_DVFS_FREQ2, VPU_DVFS_VOLT2,  2),
	VPUOP(VPU_DVFS_FREQ3, VPU_DVFS_VOLT3,  3),
	VPUOP(VPU_DVFS_FREQ4, VPU_DVFS_VOLT4,  4),
	VPUOP(VPU_DVFS_FREQ5, VPU_DVFS_VOLT5,  5),
	VPUOP(VPU_DVFS_FREQ6, VPU_DVFS_VOLT6,  6),
	VPUOP(VPU_DVFS_FREQ7, VPU_DVFS_VOLT7,  7),
	VPUOP(VPU_DVFS_FREQ8, VPU_DVFS_VOLT8,  8),
	VPUOP(VPU_DVFS_FREQ9, VPU_DVFS_VOLT9,  9),
	VPUOP(VPU_DVFS_FREQ10, VPU_DVFS_VOLT10,  10),
	VPUOP(VPU_DVFS_FREQ11, VPU_DVFS_VOLT11,  11),
	VPUOP(VPU_DVFS_FREQ12, VPU_DVFS_VOLT12,  12),
	VPUOP(VPU_DVFS_FREQ13, VPU_DVFS_VOLT13,  13),
	VPUOP(VPU_DVFS_FREQ14, VPU_DVFS_VOLT14,  14),
	VPUOP(VPU_DVFS_FREQ15, VPU_DVFS_VOLT15,  15),
};

static struct vpu_opp_table_info vpu_opp_table[] = {
	VPUOP(VPU_DVFS_FREQ0, VPU_DVFS_VOLT0,  0),
	VPUOP(VPU_DVFS_FREQ1, VPU_DVFS_VOLT1,  1),
	VPUOP(VPU_DVFS_FREQ2, VPU_DVFS_VOLT2,  2),
	VPUOP(VPU_DVFS_FREQ3, VPU_DVFS_VOLT3,  3),
	VPUOP(VPU_DVFS_FREQ4, VPU_DVFS_VOLT4,  4),
	VPUOP(VPU_DVFS_FREQ5, VPU_DVFS_VOLT5,  5),
	VPUOP(VPU_DVFS_FREQ6, VPU_DVFS_VOLT6,  6),
	VPUOP(VPU_DVFS_FREQ7, VPU_DVFS_VOLT7,  7),
	VPUOP(VPU_DVFS_FREQ8, VPU_DVFS_VOLT8,  8),
	VPUOP(VPU_DVFS_FREQ9, VPU_DVFS_VOLT9,  9),
	VPUOP(VPU_DVFS_FREQ10, VPU_DVFS_VOLT10,  10),
	VPUOP(VPU_DVFS_FREQ11, VPU_DVFS_VOLT11,  11),
	VPUOP(VPU_DVFS_FREQ12, VPU_DVFS_VOLT12,  12),
	VPUOP(VPU_DVFS_FREQ13, VPU_DVFS_VOLT13,  13),
	VPUOP(VPU_DVFS_FREQ14, VPU_DVFS_VOLT14,  14),
	VPUOP(VPU_DVFS_FREQ15, VPU_DVFS_VOLT15,  15),
};

static struct mdla_opp_table_info mdla_opp_table_default[] = {
	MDLAOP(MDLA_DVFS_FREQ0, MDLA_DVFS_VOLT0,  0),
	MDLAOP(MDLA_DVFS_FREQ1, MDLA_DVFS_VOLT1,  1),
	MDLAOP(MDLA_DVFS_FREQ2, MDLA_DVFS_VOLT2,  2),
	MDLAOP(MDLA_DVFS_FREQ3, MDLA_DVFS_VOLT3,  3),
	MDLAOP(MDLA_DVFS_FREQ4, MDLA_DVFS_VOLT4,  4),
	MDLAOP(MDLA_DVFS_FREQ5, MDLA_DVFS_VOLT5,  5),
	MDLAOP(MDLA_DVFS_FREQ6, MDLA_DVFS_VOLT6,  6),
	MDLAOP(MDLA_DVFS_FREQ7, MDLA_DVFS_VOLT7,  7),
	MDLAOP(MDLA_DVFS_FREQ8, MDLA_DVFS_VOLT8,  8),
	MDLAOP(MDLA_DVFS_FREQ9, MDLA_DVFS_VOLT9,  9),
	MDLAOP(MDLA_DVFS_FREQ10, MDLA_DVFS_VOLT10,  10),
	MDLAOP(MDLA_DVFS_FREQ11, MDLA_DVFS_VOLT11,  11),
	MDLAOP(MDLA_DVFS_FREQ12, MDLA_DVFS_VOLT12,  12),
	MDLAOP(MDLA_DVFS_FREQ13, MDLA_DVFS_VOLT13,  13),
	MDLAOP(MDLA_DVFS_FREQ14, MDLA_DVFS_VOLT14,  14),
	MDLAOP(MDLA_DVFS_FREQ15, MDLA_DVFS_VOLT15,  15),
};

static struct mdla_opp_table_info mdla_opp_table[] = {
	MDLAOP(MDLA_DVFS_FREQ0, MDLA_DVFS_VOLT0,  0),
	MDLAOP(MDLA_DVFS_FREQ1, MDLA_DVFS_VOLT1,  1),
	MDLAOP(MDLA_DVFS_FREQ2, MDLA_DVFS_VOLT2,  2),
	MDLAOP(MDLA_DVFS_FREQ3, MDLA_DVFS_VOLT3,  3),
	MDLAOP(MDLA_DVFS_FREQ4, MDLA_DVFS_VOLT4,  4),
	MDLAOP(MDLA_DVFS_FREQ5, MDLA_DVFS_VOLT5,  5),
	MDLAOP(MDLA_DVFS_FREQ6, MDLA_DVFS_VOLT6,  6),
	MDLAOP(MDLA_DVFS_FREQ7, MDLA_DVFS_VOLT7,  7),
	MDLAOP(MDLA_DVFS_FREQ8, MDLA_DVFS_VOLT8,  8),
	MDLAOP(MDLA_DVFS_FREQ9, MDLA_DVFS_VOLT9,  9),
	MDLAOP(MDLA_DVFS_FREQ10, MDLA_DVFS_VOLT10,  10),
	MDLAOP(MDLA_DVFS_FREQ11, MDLA_DVFS_VOLT11,  11),
	MDLAOP(MDLA_DVFS_FREQ12, MDLA_DVFS_VOLT12,  12),
	MDLAOP(MDLA_DVFS_FREQ13, MDLA_DVFS_VOLT13,  13),
	MDLAOP(MDLA_DVFS_FREQ14, MDLA_DVFS_VOLT14,  14),
	MDLAOP(MDLA_DVFS_FREQ15, MDLA_DVFS_VOLT15,  15),
};
static struct vpu_ptp_count_info vpu_ptp_count_table[] = {
	VPU_PTP(0),
	VPU_PTP(0),
	VPU_PTP(0),
	VPU_PTP(0),
};
static struct mdla_ptp_count_info mdla_ptp_count_table[] = {
	MDLA_PTP(0),
	MDLA_PTP(0),
	MDLA_PTP(0),
	MDLA_PTP(0),
};


//#define APU_CONN_BASE (0x19000000)

#define APU_CONN_QOS_CTRL0  (0x170)
#define APU_CONN_QOS_CTRL1  (0x174)
#define APU_CONN_QOS_CTRL2  (0x178)
#define APU_CONN_QA_CTRL0   (0x17C)
#define APU_CONN_QA_CTRL1   (0x180)
#define APU_CONN_QA_CTRL2   (0x184)
#define APU_CONN_QA_CTRL3   (0x188)
#define APU_CONN_QA_CTRL4   (0x18C)
#define APU_CONN_QA_CTRL5   (0x190)
#define APU_CONN_QA_CTRL6   (0x194)
#define APU_CONN_QA_CTRL7   (0x198)
#define APU_CONN_QA_CTRL8   (0x19C)
#define APU_CONN_QA_CTRL9   (0x1A0)
#define APU_CONN_QA_CTRL10  (0x1A4)
#define APU_CONN_QA_CTRL11  (0x1A8)
#define APU_CONN_QA_CTRL12  (0x1AC)
#define APU_CONN_QA_CTRL13  (0x1B0)
#define APU_CONN_QA_CTRL14  (0x1B4)
#define APU_CONN_QA_CTRL15  (0x1B8)
#define APU_CONN_QA_CTRL16  (0x1BC)
#define APU_CONN_QA_CTRL17  (0x1C0)
#define APU_CONN_QA_CTRL18  (0x1C4)
#define APU_CONN_QB_CTRL0   (0x1C8)
#define APU_CONN_QB_CTRL1   (0x1CC)
#define APU_CONN_QB_CTRL2   (0x1D0)
#define APU_CONN_QB_CTRL3   (0x1D4)
#define APU_CONN_QB_CTRL4   (0x1D8)
#define APU_CONN_QB_CTRL5   (0x1DC)
#define APU_CONN_QB_CTRL6   (0x1E0)
#define APU_CONN_QB_CTRL7   (0x1E4)
#define APU_CONN_QB_CTRL8   (0x1E8)
#define APU_CONN_QB_CTRL9   (0x1EC)
#define APU_CONN_QB_CTRL10  (0x1F0)
#define APU_CONN_QB_CTRL11  (0x1F4)
#define APU_CONN_QB_CTRL12  (0x1F8)
#define APU_CONN_QB_CTRL13  (0x1FC)
#define APU_CONN_QB_CTRL14  (0x200)
#define APU_CONN_QB_CTRL15  (0x204)
#define APU_CONN_QB_CTRL16  (0x208)
#define APU_CONN_QB_CTRL17  (0x20C)
#define APU_CONN_QB_CTRL18  (0x210)
#define APU_CONN_QC_CTRL0   (0x214)
#define APU_CONN_QC_CTRL1   (0x218)
#define APU_CONN_QC_CTRL2   (0x21C)
#define APU_CONN_QC_CTRL3   (0x220)
#define APU_CONN_QC_CTRL4   (0x224)
#define APU_CONN_QC_CTRL5   (0x228)
#define APU_CONN_QC_CTRL6   (0x22C)
#define APU_CONN_QC_CTRL7   (0x230)
#define APU_CONN_QC_CTRL8   (0x234)
#define APU_CONN_QC_CTRL9   (0x238)
#define APU_CONN_QC_CTRL10  (0x23C)
#define APU_CONN_QC_CTRL11  (0x240)
#define APU_CONN_QC_CTRL12  (0x244)
#define APU_CONN_QC_CTRL13  (0x248)
#define APU_CONN_QC_CTRL14  (0x24C)
#define APU_CONN_QC_CTRL15  (0x250)
#define APU_CONN_QC_CTRL16  (0x254)
#define APU_CONN_QC_CTRL17  (0x258)
#define APU_CONN_QC_CTRL18  (0x25C)
#define APU_CONN_QC1_CTRL0  (0x260)
#define APU_CONN_QC1_CTRL1  (0x264)
#define APU_CONN_QC1_CTRL2  (0x268)
#define APU_CONN_QC1_CTRL3  (0x26C)
#define APU_CONN_QC1_CTRL4  (0x270)
#define APU_CONN_QC1_CTRL5  (0x274)
#define APU_CONN_QC1_CTRL6  (0x278)
#define APU_CONN_QC1_CTRL7  (0x27C)
#define APU_CONN_QC1_CTRL8  (0x280)
#define APU_CONN_QC1_CTRL9  (0x284)
#define APU_CONN_QC1_CTRL10 (0x288)
#define APU_CONN_QC1_CTRL11 (0x28C)
#define APU_CONN_QC1_CTRL12 (0x290)
#define APU_CONN_QC1_CTRL13 (0x294)
#define APU_CONN_QC1_CTRL14 (0x298)
#define APU_CONN_QC1_CTRL15 (0x29C)
#define APU_CONN_QC1_CTRL16 (0x2A0)
#define APU_CONN_QC1_CTRL17 (0x2A4)
#define APU_CONN_QC1_CTRL18 (0x2A8)

unsigned long apu_syscfg_base;

static struct apu_dvfs *dvfs;
int vvpu_orig_opp;
int vmdla_orig_opp;
int vvpu0_cpe_result;
int vvpu1_cpe_result;
int vvpu2_cpe_result;
int vmdla0_cpe_result;
int vmdla1_cpe_result;
int vmdla2_cpe_result;


static DEFINE_MUTEX(vpu_opp_lock);
static DEFINE_MUTEX(mdla_opp_lock);
static DEFINE_MUTEX(apu_power_count_lock);
static DEFINE_MUTEX(power_check_lock);


static void get_vvpu_from_efuse(void);
static void get_vmdla_from_efuse(void);
static int vmdla_vbin(int opp);
static int vvpu_vbin(int opp);




static void dvfs_get_timestamp(char *p)
{
	int ret = 0;

	u64 sec = local_clock();
	u64 usec = do_div(sec, 1000000000);

	do_div(usec, 1000000);
	ret = sprintf(p, "%llu.%llu", sec, usec);

	if (ret < 0)
		LOG_ERR("%s sprintf fail\n", __func__);

}

void dump_opp_table(void)
{
	int i;

	LOG_DBG("%s start\n", __func__);
	for (i = 0; i < VPU_DVFS_OPP_MAX; i++) {
		LOG_INF("vpu opp:%d, vol:%d, freq:%d\n", i
			, vpu_opp_table[i].vpufreq_volt
		, vpu_opp_table[i].vpufreq_khz);
		LOG_INF("mdla opp:%d, vol:%d, freq:%d\n", i
			, mdla_opp_table[i].mdlafreq_volt
		, mdla_opp_table[i].mdlafreq_khz);
	}
	LOG_DBG("%s end\n", __func__);
}
void dump_ptp_count(void)
{
	int i;

	LOG_DBG("%s start\n", __func__);
	for (i = 0; i < 4; i++) {
		LOG_INF("vvpu id:%d, ptp cnt:%d\n", i
			, vpu_ptp_count_table[i].vpu_ptp_count);
	}
	for (i = 0; i < 4; i++) {
		LOG_INF("vmdla id:%d, ptp cnt:%d\n", i
			, mdla_ptp_count_table[i].mdla_ptp_count);
	}
	LOG_DBG("%s end\n", __func__);
}

void apu_get_power_info(void)
{
	int vvpu = 0;
	int vmdla = 0;
	int vcore = 0;
	int vsram = 0;
	int dsp_freq = 0;
	int dsp1_freq = 0;
	int dsp2_freq = 0;
	int dsp3_freq = 0;
	int ipuif_freq = 0;
#if CCF_GET_CKGEN_READY
	int temp_freq = 0;
#endif
	mutex_lock(&power_check_lock);

#if CCF_GET_CKGEN_READY
	dsp_freq = mt_get_ckgen_freq(10);
	if (dsp_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	dsp1_freq = mt_get_ckgen_freq(11);
	if (dsp1_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	dsp2_freq = mt_get_ckgen_freq(12);
	if (dsp2_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	dsp3_freq = mt_get_ckgen_freq(13);
	if (dsp3_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	ipuif_freq = mt_get_ckgen_freq(14);
	if (ipuif_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	check_vpu_clk_sts();
#endif
	if (vmdla_reg_id)
		vmdla = regulator_get_voltage(vmdla_reg_id);

	if (vvpu_reg_id)
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if (vsram_reg_id)
		vsram = regulator_get_voltage(vsram_reg_id);

	LOG_ERR("vvpu=%d, vmdla=%d, vcore=%d, vsram=%d\n",
		vvpu, vmdla, vcore, vsram);
	if (vvpu < 700000) {
		if	((dsp_freq >= 364000) || (ipuif_freq >= 364000)) {
			LOG_ERR("freq check fail\n");
			LOG_ERR("dsp_freq = %d\n", dsp_freq);
			LOG_ERR("dsp1_freq = %d\n", dsp1_freq);
			LOG_ERR("dsp2_freq = %d\n", dsp2_freq);
			LOG_ERR("dsp3_freq = %d\n", dsp3_freq);
			LOG_ERR("ipuif_freq = %d\n", ipuif_freq);
	LOG_ERR("vvpu=%d, vmdla=%d, vcore=%d\n", vvpu, vmdla, vcore);
	aee_kernel_warning("freq check", "%s: failed.", __func__);
			}
	}
	mutex_unlock(&power_check_lock);
}
EXPORT_SYMBOL(apu_get_power_info);

/************************************************
 * return current Vvpu voltage mV*100
 *************************************************/
bool vvpu_vmdla_vcore_checker(void)
{
	int ret = 0;
	int vvpu = 0;
	int vmdla = 0;
	int vcore = 0;
	int vvpu_vmdla_diff = 0;
	int vcore_vvpu_diff = 0;
	int vcore_vmdla_diff = 0;

	mutex_lock(&power_check_lock);

	vvpu_vmdla_diff = 825000 - 650000;
	vcore_vvpu_diff = 825000 - 650000;
	vcore_vmdla_diff = 825000 - 650000;
	if (vmdla_reg_id)
		vmdla = regulator_get_voltage(vmdla_reg_id);

	if (vvpu_reg_id)
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if ((vvpu < vmdla) && ((vmdla - vvpu) >= vvpu_vmdla_diff)
			&& (vvpu != 550000)) {
		ret = 1;
		LOG_ERR("vvpu_vmdla_diff fail\n");
	}
	if ((vvpu > vcore) && ((vvpu - vcore) >= vcore_vvpu_diff)) {
		ret = 1;
		LOG_ERR("vcore_vvpu_diff fail\n");
	}
	if ((vmdla > vcore) && ((vmdla - vcore) >= vcore_vmdla_diff)) {
		ret = 1;
		LOG_ERR("vcore_vmdla_diff fail\n");
	}
	LOG_DVFS("vvpu=%d, vmdla=%d, vcore=%d\n", vvpu, vmdla, vcore);
	LOG_DVFS("get vvpu=%d, vmdla=%d, vcore=%d\n",
	regulator_get_voltage(vvpu_reg_id),
	regulator_get_voltage(vmdla_reg_id),
	regulator_get_voltage(vcore_reg_id));
	if (ret) {
		LOG_INF("vvpuopp:%d, vmdlaopp:%d,\n",
				vvpu_orig_opp, vmdla_orig_opp);
		LOG_INF("vvpu=%d, vmdla=%d, vcore=%d\n", vvpu, vmdla, vcore);
		LOG_INF("get vvpu=%d, vmdla=%d, vcore=%d\n",
		regulator_get_voltage(vvpu_reg_id),
		regulator_get_voltage(vmdla_reg_id),
		regulator_get_voltage(vcore_reg_id));
		aee_kernel_warning("dvfs", "%s: failed.", __func__);
	}
	mutex_unlock(&power_check_lock);
	return ret;
}
EXPORT_SYMBOL(vvpu_vmdla_vcore_checker);

unsigned int vvpu_get_cur_volt(void)
{
	return (regulator_get_voltage(vvpu_reg_id)/10);
}
EXPORT_SYMBOL(vvpu_get_cur_volt);

unsigned int vmdla_get_cur_volt(void)
{
	return (regulator_get_voltage(vmdla_reg_id)/10);
}
EXPORT_SYMBOL(vmdla_get_cur_volt);

unsigned int vvpu_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	int i;			/* , idx; */

	mutex_lock(&vpu_opp_lock);

	for (i = 0; i < array_size; i++) {
		vpu_opp_table[i].vpufreq_volt = pmic_volt[i];
		LOG_DBG("%s opp:%d vol:%d", __func__, i, pmic_volt[i]);
		}
	dump_opp_table();

	vpu_opp_ready = true;
//
	mutex_unlock(&vpu_opp_lock);

	return 0;
}
EXPORT_SYMBOL(vvpu_update_volt);

unsigned int vmdla_update_volt(unsigned int pmic_volt[],
					unsigned int array_size)
{
	int i;

	mutex_lock(&mdla_opp_lock);

	for (i = 0; i < array_size; i++) {
		mdla_opp_table[i].mdlafreq_volt = pmic_volt[i];
		LOG_DBG("%s opp:%d vol:%d", __func__, i, pmic_volt[i]);
		}
	dump_opp_table();

	mdla_opp_ready = true;

	mutex_unlock(&mdla_opp_lock);

	return 0;
}
EXPORT_SYMBOL(vmdla_update_volt);
unsigned int vvpu_update_ptp_count(unsigned int ptp_count[],
					unsigned int array_size)
{
	int i;			/* , idx; */

	mutex_lock(&vpu_opp_lock);

	for (i = 0; i < array_size; i++) {
		vpu_ptp_count_table[i].vpu_ptp_count = ptp_count[i];
		LOG_INF("%s id:%d, ptp cnt:0x%x\n", __func__, i, ptp_count[i]);
		}
//
LOG_INF("[CPE]:VPU Det_Count: %d, %d, %d, %d\n",
vpu_ptp_count_table[0].vpu_ptp_count,
	vpu_ptp_count_table[1].vpu_ptp_count,
	vpu_ptp_count_table[2].vpu_ptp_count,
	vpu_ptp_count_table[3].vpu_ptp_count);

	get_vvpu_from_efuse();

	mutex_unlock(&vpu_opp_lock);

	return 0;
}
EXPORT_SYMBOL(vvpu_update_ptp_count);

unsigned int vmdla_update_ptp_count(unsigned int ptp_count[],
				unsigned int array_size)
{
	int i;			/* , idx; */

	mutex_lock(&mdla_opp_lock);

	for (i = 0; i < array_size; i++) {
		mdla_ptp_count_table[i].mdla_ptp_count = ptp_count[i];
		LOG_INF("%s id:%d, ptp cnt:0x%x\n", __func__, i, ptp_count[i]);
		}
//
LOG_INF("[CPE]:MDLA Det_Count: %d, %d, %d, %d\n",
mdla_ptp_count_table[0].mdla_ptp_count,
	mdla_ptp_count_table[1].mdla_ptp_count,
	mdla_ptp_count_table[2].mdla_ptp_count,
	mdla_ptp_count_table[3].mdla_ptp_count);

	get_vmdla_from_efuse();

	mutex_unlock(&mdla_opp_lock);



	return 0;
}
EXPORT_SYMBOL(vmdla_update_ptp_count);



void vvpu_restore_default_volt(void)
{
	int i;

	mutex_lock(&vpu_opp_lock);

	for (i = 0; i < VPU_DVFS_OPP_MAX; i++) {
		vpu_opp_table[i].vpufreq_volt =
			vpu_opp_table_default[i].vpufreq_volt;
	}
	dump_opp_table();

	mutex_unlock(&vpu_opp_lock);
}
EXPORT_SYMBOL(vvpu_restore_default_volt);

void vmdla_restore_default_volt(void)
{
	int i;

	mutex_lock(&mdla_opp_lock);

	for (i = 0; i < MDLA_DVFS_OPP_MAX; i++) {
		mdla_opp_table[i].mdlafreq_volt =
			mdla_opp_table_default[i].mdlafreq_volt;
	}
	dump_opp_table();

	mutex_unlock(&mdla_opp_lock);
}
EXPORT_SYMBOL(vmdla_restore_default_volt);

/* API : get frequency via OPP table index */
unsigned int vpu_get_freq_by_idx(unsigned int idx)
{
	if (idx < VPU_DVFS_OPP_MAX)
		return vpu_opp_table[idx].vpufreq_khz;
	else
		return 0;
}
EXPORT_SYMBOL(vpu_get_freq_by_idx);

/* API : get voltage via OPP table index */
unsigned int vpu_get_volt_by_idx(unsigned int idx)
{
	if (idx < VPU_DVFS_OPP_MAX)
		return vpu_opp_table[idx].vpufreq_volt;
	else
		return 0;
}
EXPORT_SYMBOL(vpu_get_volt_by_idx);

/* API : get frequency via OPP table index */
unsigned int mdla_get_freq_by_idx(unsigned int idx)
{
	if (idx < MDLA_DVFS_OPP_MAX)
		return mdla_opp_table[idx].mdlafreq_khz;
	else
		return 0;
}
EXPORT_SYMBOL(mdla_get_freq_by_idx);

/* API : get voltage via OPP table index */
unsigned int mdla_get_volt_by_idx(unsigned int idx)
{
	if (idx < MDLA_DVFS_OPP_MAX)
		return mdla_opp_table[idx].mdlafreq_volt;
	else
		return 0;
}
EXPORT_SYMBOL(mdla_get_volt_by_idx);

/*
 * API : disable DVFS for PTPOD initializing
 */
void vpu_disable_by_ptpod(void)
{
	int ret = 0;
	/* Pause VPU DVFS */
	vvpu_DVFS_is_paused_by_ptpod = true;
	/*fix vvpu to 0.8V*/
	LOG_DBG("%s\n", __func__);
	mutex_lock(&vpu_opp_lock);
	/*--Set voltage--*/
	ret = regulator_set_voltage(vvpu_reg_id,
				10*VVPU_PTPOD_FIX_VOLT,
				10*VVPU_DVFS_VOLT0);
	udelay(20);


	regulator_set_mode(vvpu_reg_id, REGULATOR_MODE_FAST);

	mutex_unlock(&vpu_opp_lock);
}

void ptpod_is_enabled(bool enable)
{
	/* Freerun VPU DVFS */
	ptpod_enabled = enable;
}
EXPORT_SYMBOL(ptpod_is_enabled);

/*
 * API : enable DVFS for PTPOD initializing
 */
void vpu_enable_by_ptpod(void)
{
	/* Freerun VPU DVFS */
	vvpu_DVFS_is_paused_by_ptpod = false;
}
EXPORT_SYMBOL(vpu_enable_by_ptpod);


bool get_vvpu_DVFS_is_paused_by_ptpod(void)
{
	/* Freerun VPU DVFS */
	return vvpu_DVFS_is_paused_by_ptpod;
}
EXPORT_SYMBOL(get_vvpu_DVFS_is_paused_by_ptpod);

/*
 * API : disable DVFS for PTPOD initializing
 */
void mdla_disable_by_ptpod(void)
{
	int ret = 0;

	/* Pause VPU DVFS */
	vmdla_DVFS_is_paused_by_ptpod = true;
	LOG_DBG("%s\n", __func__);
	mutex_lock(&mdla_opp_lock);
	/*--Set voltage--*/
	ret = regulator_set_voltage(vmdla_reg_id,
				10*VMDLA_PTPOD_FIX_VOLT,
				10*VMDLA_DVFS_VOLT0);
	udelay(500);

	regulator_set_mode(vmdla_reg_id, REGULATOR_MODE_FAST);
	vvpu_vmdla_vcore_checker();
	mutex_unlock(&mdla_opp_lock);
}


/*
 * API : enable DVFS for PTPOD initializing
 */
void mdla_enable_by_ptpod(void)
{
	int ret = 0;
	int mode = 0;

	/* Freerun VPU DVFS */
	vmdla_DVFS_is_paused_by_ptpod = false;

	mode = regulator_get_mode(vvpu_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("++vvpu_reg_id pwm mode\n");
	else
		LOG_INF("++vvpu_reg_id auto mode\n");

	regulator_set_mode(vvpu_reg_id, REGULATOR_MODE_NORMAL);
	udelay(100);

	mode = regulator_get_mode(vvpu_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("--vvpu_reg_id pwm mode\n");
	else
		LOG_INF("--vvpu_reg_id auto mode\n");

	mode = regulator_get_mode(vmdla_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("++vmdla_reg_id pwm mode\n");
	else
		LOG_INF("++vmdla_reg_id auto mode\n");

	regulator_set_mode(vmdla_reg_id, REGULATOR_MODE_NORMAL);
	udelay(100);

	mode = regulator_get_mode(vmdla_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("--vmdla_reg_id pwm mode\n");
	else
		LOG_INF("--vmdla_reg_id auto mode\n");

	ret = regulator_set_voltage(vvpu_reg_id,
			10*(vpu_opp_table[9].vpufreq_volt),
			850000);

	ret = regulator_set_voltage(vmdla_reg_id,
			10*(mdla_opp_table[9].mdlafreq_volt),
			850000);

	udelay(100);
	ret = vvpu_regulator_set_mode(true);
	udelay(100);
	LOG_DVFS("vvpu set normal mode ret=%d\n", ret);
	ret = vmdla_regulator_set_mode(true);
	udelay(100);
	LOG_DVFS("vmdla set normal mode ret=%d\n", ret);
	ret = vvpu_regulator_set_mode(false);
	udelay(100);
	LOG_DVFS("vvpu set sleep mode ret=%d\n", ret);
	ret = vmdla_regulator_set_mode(false);
	udelay(100);
	LOG_DVFS("vmdla set sleep mode ret=%d\n", ret);

	vvpu_vmdla_vcore_checker();
}
EXPORT_SYMBOL(mdla_enable_by_ptpod);

bool get_vmdla_DVFS_is_paused_by_ptpod(void)
{
	/* Freerun MDLA DVFS */
	return vmdla_DVFS_is_paused_by_ptpod;
}
EXPORT_SYMBOL(get_vmdla_DVFS_is_paused_by_ptpod);

bool get_ready_for_ptpod_check(void)
{
	/* Freerun MDLA DVFS */
	return ready_for_ptpod_check;
}
EXPORT_SYMBOL(get_ready_for_ptpod_check);



int vvpu_regulator_set_mode(bool enable)
{
	int ret = 0;

	if (!vvpu_reg_id) {
		LOG_INF("vvpu_reg_id not ready\n");
		return ret;
	}
	if (vvpu_DVFS_is_paused_by_ptpod) {
		LOG_INF("vvpu dvfs lock\n");
		return ret;
	}
	mutex_lock(&vpu_opp_lock);
	LOG_DVFS("vvpu_reg enable:%d, count:%d\n", enable, vvpu_count);
	if (enable) {
		if (vvpu_count == 0) {
			ret = regulator_set_mode(vvpu_reg_id,
				REGULATOR_MODE_NORMAL);
			}
			vvpu_count++;
	} else {
		if (vvpu_count == 1) {
			ret = regulator_set_mode(vvpu_reg_id,
				REGULATOR_MODE_IDLE);
			vvpu_count = 0;
		} else if (vvpu_count > 1)
			vvpu_count--;
	}
	mutex_unlock(&vpu_opp_lock);
	return ret;
}
EXPORT_SYMBOL(vvpu_regulator_set_mode);
int vmdla_regulator_set_mode(bool enable)
{
	int ret = 0;

	if (!vmdla_reg_id) {
		LOG_INF("vmdla_reg_id not ready\n");
		return ret;
	}
	if (vmdla_DVFS_is_paused_by_ptpod) {
		LOG_INF("vmdla dvfs lock\n");
		return ret;
	}
	mutex_lock(&mdla_opp_lock);
	LOG_DVFS("vmdla_reg enable:%d, count:%d\n", enable, vmdla_count);
	if (enable) {
		if (vmdla_count == 0) {
			ret = regulator_set_voltage(vmdla_reg_id,
				10*(mdla_opp_table[9].mdlafreq_volt),
				850000);
			}
			vmdla_count++;
	} else {
		if (vmdla_count == 1) {
			ret = regulator_set_voltage(vmdla_reg_id,
				550000,
				850000);
			vmdla_count = 0;
		} else if (vmdla_count > 1)
			vmdla_count--;
	}
	mutex_unlock(&mdla_opp_lock);
	return ret;
}
EXPORT_SYMBOL(vmdla_regulator_set_mode);

int apu_power_count_enable(bool enable, int user)
{
	mutex_lock(&apu_power_count_lock);
	if (enable) {
		apu_power_count |= user;
		qos_sram_write(APU_CLK, 1);
	} else {
		apu_power_count &= ~user;
	}
	mutex_unlock(&apu_power_count_lock);
	LOG_DVFS("apu_power_count %d", apu_power_count);
	return apu_power_count;
}
EXPORT_SYMBOL(apu_power_count_enable);

int apu_shut_down(void)
{
	int ret = 0;
	int bw_nord = 0;

	mutex_lock(&apu_power_count_lock);
	if (apu_power_count != 0) {
		mutex_unlock(&apu_power_count_lock);
		return 0;
	}
	qos_sram_write(APU_CLK, 0);
	while (bw_nord == 0) {
		bw_nord = qos_sram_read(APU_BW_NORD);
		udelay(500);
		LOG_DVFS("wait SSPM bw_nord");
	}
	mutex_unlock(&apu_power_count_lock);
	return ret;
}
EXPORT_SYMBOL(apu_shut_down);

int vpu_get_hw_vvpu_opp(int core)
{
	int opp_value = 0;
	int get_vvpu_value = 0;
	int vvpu_opp_0;
	int vvpu_opp_1;
	int vvpu_opp_2;
	int vvpu_opp_0_vol;
	int vvpu_opp_1_vol;
	int vvpu_opp_2_vol;
	//index63:PTPOD 0x11C105B4
	vvpu_opp_0 = (get_devinfo_with_index(63) & (0x7<<15))>>15;
	vvpu_opp_1 = (get_devinfo_with_index(63) & (0x7<<12))>>12;
	vvpu_opp_2 = (get_devinfo_with_index(63) & (0x7<<9))>>9;
#ifdef AGING_MARGIN
	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = (812500 - 2500);
		else
			vvpu_opp_0_vol = 812500;
	} else
		vvpu_opp_0_vol = 812500;

	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = (712500 - 2500);
		else
			vvpu_opp_1_vol = 712500;
	} else
		vvpu_opp_1_vol = 712500;

	vvpu_opp_2_vol = 637500;
#else
	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = 800000;
		else
			vvpu_opp_0_vol = 825000;
	} else {
		vvpu_opp_0_vol = 825000;
	}
	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = 700000;
		else
			vvpu_opp_1_vol = 725000;
	} else
		vvpu_opp_1_vol = 725000;

	vvpu_opp_2_vol = 650000;
#endif

	get_vvpu_value = (int)regulator_get_voltage(vvpu_reg_id);
	if (get_vvpu_value >= vvpu_opp_0_vol)
		opp_value = 0;
	else if (get_vvpu_value > vvpu_opp_1_vol)
		opp_value = 0;
	else if (get_vvpu_value > vvpu_opp_2_vol)
		opp_value = 1;
	else
		opp_value = 2;
		LOG_DVFS("[vpu_%d] vvpu(%d->%d)\n",
			core, get_vvpu_value, opp_value);
	return opp_value;

}
EXPORT_SYMBOL(vpu_get_hw_vvpu_opp);

int mdla_get_hw_vmdla_opp(int core)
{
	int opp_value = 0;
	int get_vmdla_value = 0;
	int vmdla_opp_0;
	int vmdla_opp_1;
	int vmdla_opp_2;
	int vmdla_opp_0_vol;
	int vmdla_opp_1_vol;
	int vmdla_opp_2_vol;
	//index63:PTPOD 0x11C105B4
	vmdla_opp_0 =  (get_devinfo_with_index(63) & (0x7<<24))>>24;
	vmdla_opp_1 =  (get_devinfo_with_index(63) & (0x7<<21))>>21;
	vmdla_opp_2 =  (get_devinfo_with_index(63) & (0x7<<18))>>18;
#ifdef AGING_MARGIN
	if (vmdla0_cpe_result == 1) {
		if ((vmdla_opp_0 <= 7) && (vmdla_opp_0 >= 3))
			vmdla_opp_0_vol = (815000 - 25000);
		else
			vmdla_opp_0_vol = 815000;
	} else
		vmdla_opp_0_vol = 815000;

	if (vmdla1_cpe_result == 1) {
		if ((vmdla_opp_1 <= 7) && (vmdla_opp_1 >= 3))
			vmdla_opp_1_vol = (715000 - 25000);
		else
			vmdla_opp_1_vol = 715000;
	} else
		vmdla_opp_1_vol = 715000;

	vmdla_opp_2_vol = 640000;
#else
	if (vmdla0_cpe_result == 1) {
		if ((vmdla_opp_0 <= 7) && (vmdla_opp_0 >= 3))
			vmdla_opp_0_vol = 800000;
		else
			vmdla_opp_0_vol = 825000;
	} else {
		vmdla_opp_0_vol = 825000;
	}
	if (vmdla1_cpe_result == 1) {
		if ((vmdla_opp_1 <= 7) && (vmdla_opp_1 >= 3))
			vmdla_opp_1_vol = 700000;
		else
			vmdla_opp_1_vol = 725000;
	} else
		vmdla_opp_1_vol = 725000;

	vmdla_opp_2_vol = 650000;
#endif
	get_vmdla_value = (int)regulator_get_voltage(vmdla_reg_id);
	if (get_vmdla_value >= vmdla_opp_0_vol)
		opp_value = 0;
	else if (get_vmdla_value > vmdla_opp_1_vol)
		opp_value = 0;
	else if (get_vmdla_value > vmdla_opp_2_vol)
		opp_value = 1;
	else
		opp_value = 2;

	LOG_DVFS("[mdla_%d] vmdla(%d->%d)\n",
		core, get_vmdla_value, opp_value);

	return opp_value;
}

void init_cycle(unsigned int *reg)
{
	//2'b00: 0.63ms2'b01: 1.26ms2'b10: 1.89ms2'b11: 2.52ms
	*reg &= ~(1 << 22);//qa_int_cyc[22:21]
	*reg |= (1 << 21);
}
void enable_bw(unsigned int *reg)
{
	//2'b00: 0.63ms2'b01: 1.26ms2'b10: 1.89ms2'b11: 2.52ms
	*reg |= ((1 << 23) | (1 << 7)); //qa_bw_int_en,qa_int_bw
}

void enable_apu_bw(unsigned int core)
{
	unsigned int reg = 0;

	if (core == 0) {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QA_CTRL0);
		init_cycle(&reg);
		enable_bw(&reg);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QA_CTRL0, reg);
	} else if (core == 1) {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QB_CTRL0);
		init_cycle(&reg);
		enable_bw(&reg);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QB_CTRL0, reg);
	} else {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QC_CTRL0);
		init_cycle(&reg);
		enable_bw(&reg);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QC_CTRL0, reg);
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QC1_CTRL0);
		init_cycle(&reg);
		enable_bw(&reg);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QC1_CTRL0, reg);
	}
}
EXPORT_SYMBOL(enable_apu_bw);

void enable_apu_latency(unsigned int core)
{
	unsigned int reg = 0;

	//qa_avl_timer_prd_shf=2    [11:9]
	//qa_avl_timer_prd=9 [8:1]
	//qa_lt_int_en=1  [0]
	//Unit = 9.884* 2^TIMER_PRD_SHF us
	//(TIMER_PRD<<TIMER_PRD_SHF) * Unit = (9<<2)*9.884*2^2 = 1.422ms
	//APU_CONN_QOS_CTRL1
	//qa_aw_sel [11], qb_aw_sel[8], qc_aw_sel[5],   qc1_aw_sel[2]
	if (core == 0) {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QA_CTRL3);
		reg &= ~((1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) |
			(1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) |
				(1 << 2) | (1 << 1) | 1);
		reg |= ((2 << 9) | (9 << 1) | (1 << 0));
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QA_CTRL3, reg);

		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1);
		reg |= (1 << 11);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1, reg);

	} else if (core == 1) {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QB_CTRL3);
		reg &= ~((1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) |
			(1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) |
				(1 << 2) | (1 << 1) | 1);
		reg |= ((2 << 9) | (9 << 1) | (1 << 0));
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QB_CTRL3, reg);
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1);
		reg |= (1 << 8);
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1, reg);
	} else {
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QC_CTRL3);
		reg &= ~((1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) |
			(1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) |
				(1 << 2) | (1 << 1) | 1);
		reg |= ((2 << 9) | (9 << 1) | (1 << 0));
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QC_CTRL3, reg);
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QC1_CTRL3);
		reg &= ~((1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) |
			(1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) |
				(1 << 2) | (1 << 1) | 1);
		reg |= ((2 << 9) | (9 << 1) | (1 << 0));
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QC1_CTRL3, reg);
		reg = vpu_read_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1);
		reg |= ((1 << 5) | (1 << 2));
		vpu_write_reg32(apu_syscfg_base, APU_CONN_QOS_CTRL1, reg);
	}
}
EXPORT_SYMBOL(enable_apu_latency);


static int vvpu_vbin(int opp)
{
	int vbin = 0;
	int result = 0;
	int vvpu_opp = 0;
	int pass_crit = 300000;
	int i = 0;

	for (i = 0; i < 4; i++) {
		if (vpu_ptp_count_table[i].vpu_ptp_count == 0) {
			LOG_INF("vpu ptp count 0\n");
			result = 0;
			return result;
		}
	}

	//index63:PTPOD 0x11C105B4
	if (opp == 0) {
		vbin = (-1365) * vpu_ptp_count_table[1].vpu_ptp_count +
				(1758) * vpu_ptp_count_table[3].vpu_ptp_count +
				(1443) * vpu_ptp_count_table[0].vpu_ptp_count +
				(2465) * vpu_ptp_count_table[2].vpu_ptp_count
				-2579054;
		vvpu_opp = (get_devinfo_with_index(63) & (0x7<<15))>>15;
		pass_crit = 270000;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;
	} else if (opp == 1) {
		vbin = (-1290) * vpu_ptp_count_table[1].vpu_ptp_count +
				(1925) * vpu_ptp_count_table[3].vpu_ptp_count +
				(1346) * vpu_ptp_count_table[0].vpu_ptp_count +
				(1525) * vpu_ptp_count_table[2].vpu_ptp_count
				-2060363;

		vvpu_opp = (get_devinfo_with_index(63) & (0x7<<12))>>12;
		pass_crit = 290000;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;

	} else if (opp == 2) {
		vbin = (-5328) * vpu_ptp_count_table[1].vpu_ptp_count +
				(2391) * vpu_ptp_count_table[3].vpu_ptp_count +
				(9801) * vpu_ptp_count_table[0].vpu_ptp_count +
				(-2551) * vpu_ptp_count_table[2].vpu_ptp_count
				-1502260;

		vvpu_opp = (get_devinfo_with_index(63) & (0x7<<9))>>9;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;
	}
LOG_INF("[CPE]:VPU_OPP=%d,VPU_BIN=%d,CPE_VBIN=%d,Criteria=%d,Result=%d\n",
	opp, vvpu_opp, vbin, pass_crit, result);
	return result;
}
static int vmdla_vbin(int opp)
{
	int vbin = 0;
	int result = 0;
	int vmdla_opp = 0;
	int pass_crit = 300000;
	int i = 0;

	//index63:PTPOD 0x11C105B4
	//vmdla_opp_0 =  (get_devinfo_with_index(63) & (0x7<<24))>>24;
	//vmdla_opp_1 =  (get_devinfo_with_index(63) & (0x7<<21))>>21;
	//vmdla_opp_2 =  (get_devinfo_with_index(63) & (0x7<<18))>>18;

	for (i = 0; i < 4; i++) {
		if (mdla_ptp_count_table[i].mdla_ptp_count == 0) {
			LOG_INF("mdla ptp count 0\n");
			result = 0;
			return result;
		}
	}


	if (opp == 0) {
		vbin =
		(-1365) * mdla_ptp_count_table[1].mdla_ptp_count +
		(1758) * mdla_ptp_count_table[3].mdla_ptp_count +
		(1443) * mdla_ptp_count_table[0].mdla_ptp_count +
		(2465) * mdla_ptp_count_table[2].mdla_ptp_count
		-2579054;
		vmdla_opp = (get_devinfo_with_index(63) & (0x7<<24))>>24;
		pass_crit = 270000;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;
	} else if (opp == 1) {
		vbin =
		(-1290) * mdla_ptp_count_table[1].mdla_ptp_count +
		(1925) * mdla_ptp_count_table[3].mdla_ptp_count +
		(1346) * mdla_ptp_count_table[0].mdla_ptp_count +
		(1525) * mdla_ptp_count_table[2].mdla_ptp_count
		-2060363;

		vmdla_opp = (get_devinfo_with_index(63) & (0x7<<21))>>21;
		pass_crit = 290000;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;

	} else if (opp == 2) {
		vbin =
		(-5328) * mdla_ptp_count_table[1].mdla_ptp_count +
		(2391) * mdla_ptp_count_table[3].mdla_ptp_count +
		(9801) * mdla_ptp_count_table[0].mdla_ptp_count +
		(-2551) * mdla_ptp_count_table[2].mdla_ptp_count
		-1502260;

		vmdla_opp = (get_devinfo_with_index(63) & (0x7<<18))>>18;
		if (vbin < pass_crit)
			result = 0;
		else
			result = 1;
	}

LOG_INF("[CPE]:MDLA_OPP=%d,BIN=%d,CPE_VBIN=%d,Criteria=%d,Result=%d\n",
		opp, vmdla_opp, vbin, pass_crit, result);
	return result;
}


int apu_dvfs_dump_info(void)
{
	int mode = 0;
	int i = 4;

	mode = regulator_get_mode(vvpu_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("++vvpu_reg_id pwm mode\n");
	else
		LOG_INF("++vvpu_reg_id auto mode\n");


	mode = regulator_get_mode(vmdla_reg_id);
	if (mode == REGULATOR_MODE_FAST)
		LOG_INF("++vmdla_reg_id pwm mode\n");
	else
		LOG_INF("++vmdla_reg_id auto mode\n");

	vvpu_vmdla_vcore_checker();

	for (i = 0; i < 4; i++) {
		LOG_INF("id:%d, vpu ptp cnt:0x%x\n",
			i, vpu_ptp_count_table[i].vpu_ptp_count);
	}
	for (i = 0; i < 4; i++) {
		LOG_INF("id:%d, mdla ptp cnt:0x%x\n",
			i, mdla_ptp_count_table[i].mdla_ptp_count);
	}

	LOG_INF("vpu dvfs lock:%d, mdla dvfs lock:%d\n",
		vvpu_DVFS_is_paused_by_ptpod, vmdla_DVFS_is_paused_by_ptpod);
	LOG_INF("vpu cpe0:%d, cpe1:%d, cpe2:%d\n",
		vvpu0_cpe_result, vvpu1_cpe_result, vvpu2_cpe_result);
	LOG_INF("mdla cpe0:%d, cpe1:%d, cpe2:%d\n",
		vmdla0_cpe_result, vmdla1_cpe_result, vmdla2_cpe_result);
	vvpu_vbin(0);
	vvpu_vbin(1);
	vvpu_vbin(2);
	vmdla_vbin(0);
	vmdla_vbin(1);
	vmdla_vbin(2);
	dump_opp_table();
	return 0;
}
EXPORT_SYMBOL(apu_dvfs_dump_info);

static void get_vvpu_from_efuse(void)
{
	int vvpu_opp_0;
	int vvpu_opp_1;
	int vvpu_opp_2;
	int vvpu_opp_0_vol;
	int vvpu_opp_1_vol;
	int vvpu_opp_2_vol;
//index63:PTPOD 0x11C105B4
	vvpu_opp_0 = (get_devinfo_with_index(63) & (0x7<<15))>>15;
	vvpu_opp_1 = (get_devinfo_with_index(63) & (0x7<<12))>>12;
	vvpu_opp_2 = (get_devinfo_with_index(63) & (0x7<<9))>>9;
	vvpu0_cpe_result = vvpu_vbin(0);
	vvpu1_cpe_result = vvpu_vbin(1);
	vvpu2_cpe_result = vvpu_vbin(2);
#ifdef AGING_MARGIN
	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = (81250 - 2500);
		else
			vvpu_opp_0_vol = 81250;
	} else
		vvpu_opp_0_vol = 81250;

	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = (71250 - 2500);
		else
			vvpu_opp_1_vol = 71250;
	} else
		vvpu_opp_1_vol = 71250;


			vvpu_opp_2_vol = 63750;
#else
	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = 80000;
		else
			vvpu_opp_0_vol = 82500;
	} else
		vvpu_opp_0_vol = 82500;

	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = 70000;
		else
			vvpu_opp_1_vol = 72500;
	} else
		vvpu_opp_1_vol = 72500;


			vvpu_opp_2_vol = 65000;
#endif
	vpu_opp_table[0].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[1].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[2].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[3].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[4].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[5].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[6].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[7].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[8].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[9].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[10].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[11].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[12].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[13].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[14].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[15].vpufreq_volt = vvpu_opp_2_vol;

}
static void get_vvpu_efuse(void)
{
	int vvpu_opp_0;
	int vvpu_opp_1;
	int vvpu_opp_2;
//index63:PTPOD 0x11C105B4
	vvpu_opp_0 = (get_devinfo_with_index(63) & (0x7<<15))>>15;
	vvpu_opp_1 = (get_devinfo_with_index(63) & (0x7<<12))>>12;
	vvpu_opp_2 = (get_devinfo_with_index(63) & (0x7<<9))>>9;
	LOG_DVFS("vvpu_opp_0 %d, vvpu_opp_1 %d, vvpu_opp_2 %d\n",
		vvpu_opp_0, vvpu_opp_1, vvpu_opp_2);
}
static void get_vmdla_efuse(void)
{
	int vmdla_opp_0;
	int vmdla_opp_1;
	int vmdla_opp_2;
	//index63:PTPOD 0x11C105B4
	vmdla_opp_0 =  (get_devinfo_with_index(63) & (0x7<<24))>>24;
	vmdla_opp_1 =  (get_devinfo_with_index(63) & (0x7<<21))>>21;
	vmdla_opp_2 =  (get_devinfo_with_index(63) & (0x7<<18))>>18;
	LOG_DVFS("vmdla_opp_0 %d, vmdla_opp_1 %d, vmdla_opp_2 %d\n",
		vmdla_opp_0, vmdla_opp_1, vmdla_opp_2);
}

static void get_vmdla_from_efuse(void)
{
	int vmdla_opp_0;
	int vmdla_opp_1;
	int vmdla_opp_2;
	int vmdla_opp_0_vol;
	int vmdla_opp_1_vol;
	int vmdla_opp_2_vol;

	//index63:PTPOD 0x11C105B4
	vmdla_opp_0 =  (get_devinfo_with_index(63) & (0x7<<24))>>24;
	vmdla_opp_1 =  (get_devinfo_with_index(63) & (0x7<<21))>>21;
	vmdla_opp_2 =  (get_devinfo_with_index(63) & (0x7<<18))>>18;
	vmdla0_cpe_result = vmdla_vbin(0);
	vmdla1_cpe_result = vmdla_vbin(1);
	vmdla2_cpe_result = vmdla_vbin(2);
#ifdef AGING_MARGIN
	if (vmdla0_cpe_result == 1) {
		if ((vmdla_opp_0 <= 7) && (vmdla_opp_0 >= 3))
			vmdla_opp_0_vol = (81500 - 2500);
		else
			vmdla_opp_0_vol = 81500;
	} else
		vmdla_opp_0_vol = 81500;

	if (vmdla1_cpe_result == 1) {
		if ((vmdla_opp_1 <= 7) && (vmdla_opp_1 >= 3))
			vmdla_opp_1_vol = (71500 - 2500);
		else
			vmdla_opp_1_vol = 71500;
	} else
		vmdla_opp_1_vol = 71500;

	vmdla_opp_2_vol = 64000;
#else

	if (vmdla0_cpe_result == 1) {
		if ((vmdla_opp_0 <= 7) && (vmdla_opp_0 >= 3))
			vmdla_opp_0_vol = 80000;
		else
			vmdla_opp_0_vol = 82500;
	} else
		vmdla_opp_0_vol = 82500;

	if (vmdla1_cpe_result == 1) {
		if ((vmdla_opp_1 <= 7) && (vmdla_opp_1 >= 3))
			vmdla_opp_1_vol = 70000;
		else
			vmdla_opp_1_vol = 72500;
	} else
		vmdla_opp_1_vol = 72500;

	vmdla_opp_2_vol = 65000;
#endif
	mdla_opp_table[0].mdlafreq_volt = vmdla_opp_0_vol;
	mdla_opp_table[1].mdlafreq_volt = vmdla_opp_0_vol;
	mdla_opp_table[2].mdlafreq_volt = vmdla_opp_0_vol;
	mdla_opp_table[3].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[4].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[5].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[6].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[7].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[8].mdlafreq_volt = vmdla_opp_1_vol;
	mdla_opp_table[9].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[10].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[11].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[12].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[13].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[14].mdlafreq_volt = vmdla_opp_2_vol;
	mdla_opp_table[15].mdlafreq_volt = vmdla_opp_2_vol;
}

static int commit_data(int type, int data)
{
	int ret = 0;
	int level = 16, opp = 16;
	int settle_time = 0;

	switch (type) {

	case MTK_PM_QOS_VVPU_OPP:
		mutex_lock(&vpu_opp_lock);
		if (get_vvpu_DVFS_is_paused_by_ptpod())
			LOG_INF("PM_QOS_VVPU_OPP paused by ptpod %d\n",
									data);
		else {
			LOG_DVFS("%s PM_QOS_VVPU_OPP %d\n", __func__, data);
/*settle time*/
		if (data > vvpu_orig_opp) {
			if (data - vvpu_orig_opp == 1)
				settle_time = 14;
			if (data - vvpu_orig_opp == 2)
				settle_time = 24;
		} else if (data < vvpu_orig_opp) {
			if (vvpu_orig_opp - data == 1)
				settle_time = 10;
			if (vvpu_orig_opp - data == 2)
				settle_time = 18;
		} else
			settle_time = 0;

		vvpu_orig_opp = data;

		/*--Set voltage--*/
		if (data == 0) {
			LOG_DBG("set_voltage %d\n",
10*(vpu_opp_table[0].vpufreq_volt));
			ret = regulator_set_voltage(vvpu_reg_id,
				10*(vpu_opp_table[0].vpufreq_volt),
				850000);
		} else if (data == 1) {
			LOG_DBG("set_voltage %d\n",
				10*(vpu_opp_table[5].vpufreq_volt));
			ret = regulator_set_voltage(vvpu_reg_id,
				10*(vpu_opp_table[5].vpufreq_volt),
				850000);
		} else {
			LOG_DBG("set_voltage %d\n",
				10*(vpu_opp_table[9].vpufreq_volt));
			ret = regulator_set_voltage(vvpu_reg_id,
				10*(vpu_opp_table[9].vpufreq_volt),
			850000);
		}
		if (ret)
			LOG_ERR("regulator_set_voltage vvpu_reg_id failed\n");
		}
		udelay(settle_time);
		mutex_unlock(&vpu_opp_lock);
		break;
	case MTK_PM_QOS_VMDLA_OPP:
		mutex_lock(&mdla_opp_lock);
		if (get_vmdla_DVFS_is_paused_by_ptpod())
			LOG_INF("PM_QOS_VMDLA_OPP paused by ptpod %d\n",
									data);
		else {
			LOG_DVFS("%s PM_QOS_VMDLA_OPP %d\n", __func__,
									data);
			/*settle time*/
			if (data > vmdla_orig_opp) {
				if (data - vmdla_orig_opp == 1)
					settle_time = 37;
				if (data - vmdla_orig_opp == 2)
					settle_time = 41;
			} else if (data < vmdla_orig_opp) {
				if (vmdla_orig_opp - data == 1)
					settle_time = 36;
				if (vmdla_orig_opp - data == 2)
					settle_time = 42;
			} else
				settle_time = 0;

			vmdla_orig_opp = data;

		/*--Set voltage--*/
		if (data == 0) {
			ret = regulator_set_voltage(vmdla_reg_id,
			10*(mdla_opp_table[0].mdlafreq_volt),
			850000);
			LOG_DBG("set vmdla %d, ret %d\n",
			10*(mdla_opp_table[0].mdlafreq_volt), ret);
		} else if (data == 1) {
			ret = regulator_set_voltage(vmdla_reg_id,
			10*(mdla_opp_table[3].mdlafreq_volt),
			850000);
			LOG_DBG("set vmdla %d, ret %d\n",
			10*(mdla_opp_table[3].mdlafreq_volt), ret);
		} else {
			ret = regulator_set_voltage(vmdla_reg_id,
			10*(mdla_opp_table[9].mdlafreq_volt),
			850000);
			LOG_DBG("set vmdla %d, ret %d\n",
			10*(mdla_opp_table[9].mdlafreq_volt), ret);
		}

		}
		udelay(settle_time);
		mutex_unlock(&mdla_opp_lock);
		break;

	default:
		LOG_DBG("unsupported type of commit data\n");
		break;
	}
	vvpu_vmdla_vcore_checker();

	get_vvpu_efuse();
	get_vmdla_efuse();

	if (ret < 0) {
		pr_info("%s: type: 0x%x, data: 0x%x, opp: %d, level: %d\n",
				__func__, type, data, opp, level);
		apu_dvfs_dump_reg(NULL);
		aee_kernel_warning("dvfs", "%s: failed.", __func__);
	}


	return ret;
}

static void get_pm_qos_info(char *p, size_t n)
{
	char timestamp[20];
	int start, remain;

	dvfs_get_timestamp(timestamp);
	// Fix Coverity defect, don't call sprintf
	remain = n;
	start = snprintf(p, remain, "%-24s: 0x%x\n",
			"PM_QOS_VVPU_OPP",
			mtk_pm_qos_request(MTK_PM_QOS_VVPU_OPP));
	if (start >= 0 && start < remain) {
		p += start;
		remain -= start;
		start = snprintf(p, remain, "%-24s: 0x%x\n",
			"PM_QOS_VMDLA_OPP",
			mtk_pm_qos_request(MTK_PM_QOS_VMDLA_OPP));
	}
	if (start >= 0 && start < remain) {
		p += start;
		remain -= start;
		start = snprintf(p, remain, "%-24s: %s\n",
			"Current Timestamp", timestamp);
	}
	if (start >= 0 && start < remain) {
		p += start;
		remain -= start;
		start = snprintf(p, remain, "%-24s: %s\n",
			"Force Start Timestamp", dvfs->force_start);
	}
	if (start >= 0 && start < remain) {
		p += start;
		remain -= start;
		start = snprintf(p, remain, "%-24s: %s\n",
			"Force End Timestamp", dvfs->force_end);
	}
	if (start < 0)
		LOG_ERR("[%s] snprintf failed!\n", __func__);
}

char *apu_dvfs_dump_reg(char *ptr)
{
	char buf[1024];

	get_pm_qos_info(buf, 1024);
	// Fix Coverity defect, don't call sprintf
	if (ptr)
		ptr += snprintf(ptr, 1024, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	return ptr;
}

static int pm_qos_vvpu_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(MTK_PM_QOS_VVPU_OPP, l);

	return NOTIFY_OK;
}
static int pm_qos_vmdla_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(MTK_PM_QOS_VMDLA_OPP, l);

	return NOTIFY_OK;
}


static void pm_qos_notifier_register(void)
{

	dvfs->pm_qos_vvpu_opp_nb.notifier_call =
		pm_qos_vvpu_opp_notify;
	mtk_pm_qos_add_notifier(MTK_PM_QOS_VVPU_OPP,
			&dvfs->pm_qos_vvpu_opp_nb);

	dvfs->pm_qos_vmdla_opp_nb.notifier_call =
		pm_qos_vmdla_opp_notify;
	mtk_pm_qos_add_notifier(MTK_PM_QOS_VMDLA_OPP,
			&dvfs->pm_qos_vmdla_opp_nb);
}

#ifdef CONFIG_MTK_DEVINFO
static int get_nvmem_cell_efuse(struct device *dev)
{
	struct nvmem_cell *cell;
	uint32_t *buf = NULL;

	cell = nvmem_cell_get(dev, "efuse_ptpod");
	if (IS_ERR(cell)) {
		LOG_ERR("[%s] nvmem_cell_get fail\n", __func__);
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);
		return -1;
	}

	/* Add null checking for Coverity */
	if (cell != NULL) {
		buf = (uint32_t *)nvmem_cell_read(cell, NULL);
		nvmem_cell_put(cell);

		if (IS_ERR(buf)) {
			LOG_ERR("[%s] nvmem_cell_read fail\n", __func__);
			return PTR_ERR(buf);
		}

		if (buf != NULL) {
			g_efuse_ptpod = *buf;
			kfree(buf);
		}
	} else
		LOG_ERR("[%s] cell was null\n", __func__);

	return 0;
}
#endif // CONFIG_MTK_DEVINFO

static int apu_dvfs_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device_node *ipu_conn_node = NULL;
	struct device_node *ct_node = NULL;
	u32 ct_flag = 0;

#ifdef CONFIG_MTK_DEVINFO
	get_nvmem_cell_efuse(&pdev->dev);
#endif
	dvfs = devm_kzalloc(&pdev->dev, sizeof(*dvfs), GFP_KERNEL);
	if (!dvfs)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	LOG_DBG("%s\n", __func__);

	dvfs->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfs->regs))
		return PTR_ERR(dvfs->regs);
	platform_set_drvdata(pdev, dvfs);

	dvfs->dvfs_node = of_find_compatible_node(
		NULL, NULL, "mediatek,apu_dvfs");

	ipu_conn_node = of_find_compatible_node(NULL, NULL,
					"mediatek,ipu_conn");

	apu_syscfg_base =
			(unsigned long) of_iomap(ipu_conn_node, 0);


	/*enable Vvpu Vmdla*/
	/*--Get regulator handle--*/
	vvpu_reg_id = regulator_get(&pdev->dev, "vpu");
	if (!vvpu_reg_id)
		LOG_ERR("regulator_get vvpu_reg_id failed\n");
	vmdla_reg_id = regulator_get(&pdev->dev, "VMDLA");
	if (!vmdla_reg_id)
		LOG_ERR("regulator_get vmdla_reg_id failed\n");
	vcore_reg_id = regulator_get(&pdev->dev, "vcore");
	if (!vcore_reg_id)
		LOG_ERR("regulator_get vcore_reg_id failed\n");
	vsram_reg_id = regulator_get(&pdev->dev, "vsram_others");
	if (!vsram_reg_id)
		LOG_ERR("regulator_get vsram_reg_id failed\n");

	ready_for_ptpod_check = false;
	/*--enable regulator--*/
	ret = regulator_enable(vvpu_reg_id);
	udelay(200);//slew rate:rising10mV/us
	if (ret)
		LOG_ERR("regulator_enable vvpu_reg_id failed\n");

	ret = regulator_enable(vmdla_reg_id);
	udelay(200);
	if (ret)
		LOG_ERR("regulator_enable vmdla_reg_id failed\n");

	ret = apu_dvfs_add_interface(&pdev->dev);

	if (ret)
		return ret;

	pm_qos_notifier_register();
	vvpu_count = 0;
	vmdla_count = 0;
	apu_power_count = 0;
	ct_node = of_find_compatible_node(NULL, NULL, "mediatek,eem_fsm");
	ret = of_property_read_u32(ct_node, "eem-ct", &ct_flag);
	if (ret)
		LOG_ERR("cant find eem-ct node\n");
	if (ct_flag && ptpod_enabled) {
		vpu_disable_by_ptpod();
		mdla_disable_by_ptpod();
		ready_for_ptpod_check = true;
	} else {
		LOG_INF("EEM corner tighten close by DT\n");
		ret = regulator_set_voltage(vvpu_reg_id,
		10*(vpu_opp_table[9].vpufreq_volt),
		850000);

		ret = regulator_set_voltage(vmdla_reg_id,
		10*(mdla_opp_table[9].mdlafreq_volt),
		850000);

		udelay(100);
		ret = vvpu_regulator_set_mode(true);
		udelay(100);
		LOG_DVFS("vvpu set normal mode ret=%d\n", ret);
		ret = vmdla_regulator_set_mode(true);
		udelay(100);
		LOG_DVFS("vmdla set normal mode ret=%d\n", ret);
		ret = vvpu_regulator_set_mode(false);
		udelay(100);
		LOG_DVFS("vvpu set sleep mode ret=%d\n", ret);
		ret = vmdla_regulator_set_mode(false);
		udelay(100);
		LOG_DVFS("vmdla set sleep mode ret=%d\n", ret);

		vvpu_vmdla_vcore_checker();
	}

	LOG_DVFS("%s: init done\n", __func__);

	apu_get_power_info();

	return 0;
}

static int apu_dvfs_remove(struct platform_device *pdev)
{
	apu_dvfs_remove_interface(&pdev->dev);
	return 0;
}

static const struct of_device_id apu_dvfs_of_match[] = {
	{ .compatible = "mediatek,apu_dvfs" },
	{ },
};

MODULE_DEVICE_TABLE(of, apu_dvfs_of_match);

static __maybe_unused int apu_dvfs_suspend(struct device *dev)
{
	return 0;
}

static __maybe_unused int apu_dvfs_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(apu_dvfs_pm, apu_dvfs_suspend,
			 apu_dvfs_resume);

static struct platform_driver apu_dvfs_driver = {
	.probe	= apu_dvfs_probe,
	.remove	= apu_dvfs_remove,
	.driver = {
		.name = "apu_dvfs",
		.pm	= &apu_dvfs_pm,
		.of_match_table = apu_dvfs_of_match,
	},
};

static int __init apu_dvfs_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&apu_dvfs_driver);
	return ret;
}
late_initcall(apu_dvfs_init)

static void __exit apu_dvfs_exit(void)
{
	//int ret = 0;

	platform_driver_unregister(&apu_dvfs_driver);
}
module_exit(apu_dvfs_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("apu dvfs driver");
