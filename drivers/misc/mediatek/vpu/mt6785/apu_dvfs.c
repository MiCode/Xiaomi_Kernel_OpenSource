/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <mt-plat/upmu_common.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#ifdef ENABLE_EARA_QOS
#include <mtk_qos_sram.h>
#endif
#include <linux/delay.h>
#include "vpu_reg.h"


//#include "governor.h"

#include <mt-plat/aee.h>
#include <spm/mtk_spm.h>

#include "apu_dvfs.h"
#include "vpu_cmn.h"

#include <linux/regulator/consumer.h>
#include <mtk_devinfo.h>

/*regulator id*/
static struct regulator *vvpu_reg_id;
static struct regulator *vcore_reg_id;


static bool vvpu_DVFS_is_paused_by_ptpod;
static bool ready_for_ptpod_check;


static bool vpu_opp_ready;
#define VPU_DVFS_OPP_MAX	  (16)

static int apu_power_count;
static int vvpu_count;

#define VPU_DVFS_FREQ0	 (750000)	/* KHz */
#define VPU_DVFS_FREQ1	 (700000)	/* KHz */
#define VPU_DVFS_FREQ2	 (624000)	/* KHz */
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
#define VPU_DVFS_VOLT0	 (83750)	/* mV x 100 */
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
#define VPU_DVFS_VOLT0	 (85000)	/* mV x 100 */
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
#define VPU_CT_DIFF	 (25000)

#define VPUOP(khz, volt, idx) \
{ \
	.vpufreq_khz = khz, \
	.vpufreq_volt = volt, \
	.vpufreq_idx = idx, \
}

#define VPU_PTP(ptp_count) \
{ \
	.vpu_ptp_count = ptp_count, \
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

static struct vpu_ptp_count_info vpu_ptp_count_table[] = {
	VPU_PTP(0),
	VPU_PTP(0),
	VPU_PTP(0),
	VPU_PTP(0),
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
int vvpu0_cpe_result;
int vvpu1_cpe_result;
int vvpu2_cpe_result;


static DEFINE_MUTEX(vpu_opp_lock);
static DEFINE_MUTEX(apu_power_count_lock);
static DEFINE_MUTEX(power_check_lock);

static void get_vvpu_from_efuse(void);
static int vvpu_vbin(int opp);

void dump_opp_table(void)
{
	int i;

	LOG_DBG("%s start\n", __func__);
	for (i = 0; i < VPU_DVFS_OPP_MAX; i++) {
		LOG_INF("vpu opp:%d, vol:%d, freq:%d\n", i
			, vpu_opp_table[i].vpufreq_volt
		, vpu_opp_table[i].vpufreq_khz);
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
	LOG_DBG("%s end\n", __func__);
}
void apu_get_power_info(void)
{
	int vvpu = 0;
	int vcore = 0;
	int dsp_freq = 0;
	int dsp1_freq = 0;
	int dsp2_freq = 0;
	int ipuif_freq = 0;
	int temp_freq = 0;
	mutex_lock(&power_check_lock);

	dsp_freq = mt_get_ckgen_freq(10);
	if (dsp_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	dsp1_freq = mt_get_ckgen_freq(11);
	if (dsp1_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	dsp2_freq = mt_get_ckgen_freq(12);
	if (dsp2_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);
	ipuif_freq = mt_get_ckgen_freq(14);
	if (ipuif_freq == 0)
		temp_freq = mt_get_ckgen_freq(1);

	if (vvpu_reg_id)
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);
	LOG_DVFS("dsp_freq = %d\n", dsp_freq);
	LOG_DVFS("dsp1_freq = %d\n", dsp1_freq);
	LOG_DVFS("dsp2_freq = %d\n", dsp2_freq);
	LOG_DVFS("ipuif_freq = %d\n", ipuif_freq);
	LOG_DVFS("vvpu=%d, vcore=%d\n", vvpu, vcore);
	if (vvpu < 700000) {
		if	((dsp_freq >= 364000) || (ipuif_freq >= 364000)) {
			LOG_INF("freq check fail\n");
			LOG_INF("dsp_freq = %d\n", dsp_freq);
			LOG_INF("dsp1_freq = %d\n", dsp1_freq);
			LOG_INF("dsp2_freq = %d\n", dsp2_freq);
			LOG_INF("ipuif_freq = %d\n", ipuif_freq);
	LOG_INF("vvpu=%d, vcore=%d\n", vvpu, vcore);
	aee_kernel_warning("freq check", "%s: failed.", __func__);
			}
	}
	mutex_unlock(&power_check_lock);
}
EXPORT_SYMBOL(apu_get_power_info);

/************************************************
 * return current Vvpu voltage mV*100
 *************************************************/

unsigned int vvpu_get_cur_volt(void)
{
	return (regulator_get_voltage(vvpu_reg_id)/10);
}
EXPORT_SYMBOL(vvpu_get_cur_volt);

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

bool get_ready_for_ptpod_check(void)
{
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
			ret = regulator_set_voltage(vvpu_reg_id,
				10*(vpu_opp_table[9].vpufreq_volt),
				900000);
			}
			vvpu_count++;
	} else {
		if (vvpu_count == 1) {
			ret = regulator_set_voltage(vvpu_reg_id,
				550000,
				900000);
			vvpu_count = 0;
			} else if (vvpu_count > 1)
				vvpu_count--;
	}
	mutex_unlock(&vpu_opp_lock);
	return ret;
}
EXPORT_SYMBOL(vvpu_regulator_set_mode);


#ifdef ENABLE_EARA_QOS
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
#endif

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
	int vvpu_opp_3_vol;
	//index63:PTPOD 0x11C105B4
	vvpu_opp_0 = (get_devinfo_with_index(63) & (0x7<<15))>>15;
	vvpu_opp_1 = (get_devinfo_with_index(63) & (0x7<<12))>>12;
	vvpu_opp_2 = (get_devinfo_with_index(63) & (0x7<<9))>>9;

	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = (10 * VPU_DVFS_VOLT0 - VPU_CT_DIFF);
		else
			vvpu_opp_0_vol = 10 * VPU_DVFS_VOLT0;
	} else
		vvpu_opp_0_vol = 10 * VPU_DVFS_VOLT0;

	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = (10 * VPU_DVFS_VOLT1 - VPU_CT_DIFF);
		else
			vvpu_opp_1_vol = 10 * VPU_DVFS_VOLT1;
	} else
		vvpu_opp_1_vol = 10 * VPU_DVFS_VOLT1;

	if (vvpu2_cpe_result == 1) {
		if ((vvpu_opp_2 <= 7) && (vvpu_opp_2 >= 3))
			vvpu_opp_2_vol = (10 * VPU_DVFS_VOLT5 - VPU_CT_DIFF);
		else
			vvpu_opp_2_vol = 10 * VPU_DVFS_VOLT5;
	} else
		vvpu_opp_2_vol = 10 * VPU_DVFS_VOLT5;

	vvpu_opp_3_vol = 10 * VPU_DVFS_VOLT9;

	get_vvpu_value = (int)regulator_get_voltage(vvpu_reg_id);
	if (get_vvpu_value >= vvpu_opp_0_vol)
		opp_value = 0;
	else if (get_vvpu_value > vvpu_opp_1_vol)
		opp_value = 0;
	else if (get_vvpu_value > vvpu_opp_2_vol)
		opp_value = 1;
	else if (get_vvpu_value > vvpu_opp_3_vol)
		opp_value = 2;
	else
		opp_value = 3;
		LOG_DVFS("[vpu_%d] vvpu(%d->%d)\n",
			core, get_vvpu_value, opp_value);
	return opp_value;

}
EXPORT_SYMBOL(vpu_get_hw_vvpu_opp);



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

int apu_dvfs_dump_info(void)
{
	int mode = 0;
	int i = 4;

	mode = regulator_get_mode(vvpu_reg_id);
		if (mode == REGULATOR_MODE_FAST)
			LOG_INF("++vvpu_reg_id pwm mode\n");
		else
			LOG_INF("++vvpu_reg_id auto mode\n");

	for (i = 0; i < 4; i++) {
		LOG_INF("id:%d, vpu ptp cnt:0x%x\n",
			i, vpu_ptp_count_table[i].vpu_ptp_count);
	}


	LOG_INF("vpu dvfs lock:%d\n",
		vvpu_DVFS_is_paused_by_ptpod);
	LOG_INF("vpu cpe0:%d, cpe1:%d, cpe2:%d\n",
		vvpu0_cpe_result, vvpu1_cpe_result, vvpu2_cpe_result);
	vvpu_vbin(0);
	vvpu_vbin(1);
	vvpu_vbin(2);
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
	int vvpu_opp_3_vol;
//index63:PTPOD 0x11C105B4
	vvpu_opp_0 = (get_devinfo_with_index(63) & (0x7<<15))>>15;
	vvpu_opp_1 = (get_devinfo_with_index(63) & (0x7<<12))>>12;
	vvpu_opp_2 = (get_devinfo_with_index(63) & (0x7<<9))>>9;
	vvpu0_cpe_result = vvpu_vbin(0);
	vvpu1_cpe_result = vvpu_vbin(1);
	vvpu2_cpe_result = vvpu_vbin(2);
	if (vvpu0_cpe_result == 1) {
		if ((vvpu_opp_0 <= 7) && (vvpu_opp_0 >= 3))
			vvpu_opp_0_vol = (10 * VPU_DVFS_VOLT0 - VPU_CT_DIFF);
		else
			vvpu_opp_0_vol = 10 * VPU_DVFS_VOLT0;
	} else
		vvpu_opp_0_vol = 10 * VPU_DVFS_VOLT0;

	if (vvpu1_cpe_result == 1) {
		if ((vvpu_opp_1 <= 7) && (vvpu_opp_1 >= 3))
			vvpu_opp_1_vol = (10 * VPU_DVFS_VOLT1 - VPU_CT_DIFF);
		else
			vvpu_opp_1_vol = 10 * VPU_DVFS_VOLT1;
	} else
		vvpu_opp_1_vol = 10 * VPU_DVFS_VOLT1;

	if (vvpu2_cpe_result == 1) {
		if ((vvpu_opp_2 <= 7) && (vvpu_opp_2 >= 3))
			vvpu_opp_2_vol = (10 * VPU_DVFS_VOLT5 - VPU_CT_DIFF);
		else
			vvpu_opp_2_vol = 10 * VPU_DVFS_VOLT5;
	} else
		vvpu_opp_2_vol = 10 * VPU_DVFS_VOLT5;


			vvpu_opp_3_vol = 10 * VPU_DVFS_VOLT9;
	vpu_opp_table[0].vpufreq_volt = vvpu_opp_0_vol;
	vpu_opp_table[1].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[2].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[3].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[4].vpufreq_volt = vvpu_opp_1_vol;
	vpu_opp_table[5].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[6].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[7].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[8].vpufreq_volt = vvpu_opp_2_vol;
	vpu_opp_table[9].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[10].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[11].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[12].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[13].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[14].vpufreq_volt = vvpu_opp_3_vol;
	vpu_opp_table[15].vpufreq_volt = vvpu_opp_3_vol;

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


static int commit_data(int type, int data)
{
	int ret = 0;
	int level = 16, opp = 16;
	int settle_time = 0;

	switch (type) {

	case PM_QOS_VVPU_OPP:
		mutex_lock(&vpu_opp_lock);
		if (get_vvpu_DVFS_is_paused_by_ptpod()) {
			LOG_INF("PM_QOS_VVPU_OPP paused by ptpod %d\n", data);
		} else {
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
					900000);
			} else if (data == 1) {
				LOG_DBG("set_voltage %d\n",
					10*(vpu_opp_table[1].vpufreq_volt));
				ret = regulator_set_voltage(vvpu_reg_id,
					10*(vpu_opp_table[1].vpufreq_volt),
					900000);
			} else if (data == 2) {
				LOG_DBG("set_voltage %d\n",
					10*(vpu_opp_table[5].vpufreq_volt));
				ret = regulator_set_voltage(vvpu_reg_id,
					10*(vpu_opp_table[5].vpufreq_volt),
					900000);
			} else {
				LOG_DBG("set_voltage %d\n",
					10*(vpu_opp_table[9].vpufreq_volt));
				ret = regulator_set_voltage(vvpu_reg_id,
					10*(vpu_opp_table[9].vpufreq_volt),
				900000);
			}
			if (ret < 0)
				LOG_ERR("rg_set_voltage vvpu_r_id failed\n");
		}
		udelay(settle_time);
		mutex_unlock(&vpu_opp_lock);
		break;


	default:
		LOG_DBG("unsupported type of commit data\n");
		break;
	}

	get_vvpu_efuse();
	if (ret < 0) {
		pr_info("%s: type: 0x%x, data: 0x%x, opp: %d, level: %d\n",
				__func__, type, data, opp, level);
		apu_get_power_info();
		apu_dvfs_dump_reg(NULL);
		aee_kernel_warning("dvfs", "%s: failed.", __func__);
	}


	return ret;
}

static void get_pm_qos_info(char *p)
{
	p += sprintf(p, "%-24s: 0x%x\n",
			"PM_QOS_VVPU_OPP",
			pm_qos_request(PM_QOS_VVPU_OPP));

}

char *apu_dvfs_dump_reg(char *ptr)
{
	char buf[1024];

	get_pm_qos_info(buf);
	if (ptr)
		ptr += sprintf(ptr, "%s\n", buf);
	else
		pr_info("%s\n", buf);

	return ptr;
}

static int pm_qos_vvpu_opp_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	commit_data(PM_QOS_VVPU_OPP, l);

	return NOTIFY_OK;
}



static void pm_qos_notifier_register(void)
{

	dvfs->pm_qos_vvpu_opp_nb.notifier_call =
		pm_qos_vvpu_opp_notify;
	pm_qos_add_notifier(PM_QOS_VVPU_OPP,
			&dvfs->pm_qos_vvpu_opp_nb);
}

static int apu_dvfs_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device_node *ipu_conn_node = NULL;

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

	/*--Get regulator handle--*/
		vvpu_reg_id = regulator_get(&pdev->dev, "VMDLA");
		if (!vvpu_reg_id)
			LOG_ERR("regulator_get vvpu_reg_id failed\n");
		vcore_reg_id = regulator_get(&pdev->dev, "vcore");
		if (!vcore_reg_id)
			LOG_ERR("regulator_get vcore_reg_id failed\n");

	ready_for_ptpod_check = false;
	/*--enable regulator--*/
	ret = regulator_enable(vvpu_reg_id);
	udelay(200);//slew rate:rising10mV/us
	if (ret)
		LOG_ERR("regulator_enable vvpu_reg_id failed\n");

	ret = apu_dvfs_add_interface(&pdev->dev);

	if (ret)
		return ret;

	pm_qos_notifier_register();
	vvpu_count = 0;
	apu_power_count = 0;
		ret = regulator_set_voltage(vvpu_reg_id,
		10*(vpu_opp_table[9].vpufreq_volt),
		900000);

		udelay(100);
		ret = vvpu_regulator_set_mode(true);
		udelay(100);
		LOG_DVFS("vvpu set normal mode ret=%d\n", ret);
		ret = vvpu_regulator_set_mode(false);
		udelay(100);
		LOG_DVFS("vvpu set sleep mode ret=%d\n", ret);


	LOG_DVFS("%s: init done\n", __func__);

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
