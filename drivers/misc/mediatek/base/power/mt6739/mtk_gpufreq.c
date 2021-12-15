/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include <linux/uaccess.h>

#include "mtk_gpufreq.h"
/*
* #include "mtk_static_power.h"
*/
#include "mt-plat/upmu_common.h"
#include "mt-plat/sync_write.h"
#include "mach/mtk_pmic_wrap.h"

#include "mtk_fhreg.h"
#include "mtk_freqhopping_drv.h"

static void __iomem *g_apmixed_base;

/* Change name of REG_MFGPLL_CON0 to REG_GPUPLL_CON0 in MT6739 */
#define GPUPLL_CON0 (REG_MFGPLL_CON0)
#define GPUPLL_CON1 (REG_MFGPLL_CON1)

/* Use VOLT_SET_READY for regulator operation debug*/
#define VOLT_SET_READY

/* support vcorefs_request_dvfs_opp, operation voltage by the API */
#define VCORE_DVFS_OPP_SUPPORT

/* TODO: check this! */
/* #include "mach/mt_static_power.h" */
#include "mach/mtk_thermal.h"
#include "mach/upmu_sw.h"
#include "mach/upmu_hw.h"
#include "mach/mtk_pbm.h"

#include <linux/regulator/consumer.h>
#include <mtk_vcorefs_manager.h>
#include "mtk_devinfo.h"

/* #define BRING_UP */

#define DRIVER_NOT_READY	-1

#ifndef MTK_UNREFERENCED_PARAMETER
#define MTK_UNREFERENCED_PARAMETER(param) ((void)(param))
#endif

/*
 * CONFIG
 */
/**************************************************
 * Define low battery voltage support
 ***************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT

/**************************************************
 * Define low battery volume support
 ***************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT

/**************************************************
 * Define oc support
 ***************************************************/
#define MT_GPUFREQ_OC_PROTECT

/**************************************************
 * GPU DVFS input boost feature
 ***************************************************/
#define MT_GPUFREQ_INPUT_BOOST

/***************************
 * Define for dynamic power table update
 ****************************/
#define MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE

#define STATIC_PWR_READY2USE

#ifdef STATIC_PWR_READY2USE
#include "mtk_common_static_power.h"
#endif

/***************************
 * Define for random test
 ****************************/
/* #define MT_GPU_DVFS_RANDOM_TEST */

/***************************
 * Define for SRAM debugging
 ****************************/
#ifdef CONFIG_MTK_RAM_CONSOLE
#define MT_GPUFREQ_AEE_RR_REC
#endif

/*
 * Define how to setup VPROC by
 * PMIC_WRAP
 * PMIC
 * external IC
 */
/* #define VPROC_SET_BY_PMIC_WRAP */
#define VPROC_SET_BY_PMIC
/* #define VPROC_SET_BY_EXTIC */

#ifdef VPROC_SET_BY_EXTIC
#include "fan53555.h"
#endif
/**************************************************
 * Define register write function
 ***************************************************/
#define mt_gpufreq_reg_write(val, addr)		mt_reg_sync_writel((val), ((void *)addr))

/***************************
 * Operate Point Definition
 ****************************/
#define GPUOP(khz, volt, vsram, idx)	   \
{						   \
	.gpufreq_khz = khz,	 \
	.gpufreq_volt = volt,	\
	.gpufreq_vsram = vsram,	  \
	.gpufreq_idx = idx,	  \
}

/*************
 * For MT6739
 *************/
#define VPROC 1
#define VSRAM 2

/**************************
 * GPU DVFS OPP table setting
 ***************************/

/* MT6739 Use VCORE DVFS and need on/off VCORE & frequency control only */
#define GPU_DVFS_FREQ0	 (450000)	/* KHz */
#define GPU_DVFS_FREQ1	 (400000)	/* KHz */
#define GPU_DVFS_FREQ2	 (350000)	/* KHz */
#define GPU_DVFS_FREQ3	 (290000)	/* KHz */
#define GPUFREQ_LAST_FREQ_LEVEL	(GPU_DVFS_FREQ3)

#define GPU_DVFS_VOLT0	 (115000)	/* mV x 100 */
#define GPU_DVFS_VOLT1	 (111875)	/* mV x 100 */
#define GPU_DVFS_VOLT2	 (108750)	/* mV x 100 */
#define GPU_DVFS_VOLT3	 (105000)	/* mV x 100 */

#define GPU_DVFS_VSRAM0	  (100000)	/* mV x 100 */
#define GPU_DVFS_VSRAM1	  (98750)	/* mV x 100 */

#define GPU_DVFS_MAX_FREQ	GPU_DVFS_FREQ0 /* KHz */

#define GPU_DVFS_PTPOD_DISABLE_VOLT	GPU_DVFS_VOLT1

#define UNIVPLL_FREQ	(416000) /* KHz */

/* MT6739TW Use VCORE DVFS and need on/off VCORE & frequency control only */
#define GPU_DVFS_FREQ0_6739TW	 (570000)	/* KHz */
#define GPU_DVFS_FREQ1_6739TW	 (482500)	/* KHz */
#define GPU_DVFS_FREQ2_6739TW	 (395000)	/* KHz */
#define GPU_DVFS_FREQ3_6739TW	 (290000)	/* KHz */
#define GPUFREQ_LAST_FREQ_LEVEL_6739TW	(GPU_DVFS_FREQ3_6739TW)

#define GPU_DVFS_VOLT0_6739TW	 (115000)	/* mV x 100 */
#define GPU_DVFS_VOLT1_6739TW	 (111875)	/* mV x 100 */
#define GPU_DVFS_VOLT2_6739TW	 (108750)	/* mV x 100 */
#define GPU_DVFS_VOLT3_6739TW	 (105000)	/* mV x 100 */

#define GPU_DVFS_VSRAM0_6739TW	  (90000)	/* mV x 100 */
#define GPU_DVFS_VSRAM1_6739TW	  (80000)	/* mV x 100 */

#define GPU_DVFS_PTPOD_DISABLE_VOLT_MT6739TW	GPU_DVFS_VOLT0_6739TW

/* MT6739WD(6731) Use VCORE DVFS and need on/off VCORE & frequency control only */
#define GPU_DVFS_FREQ0_6739WD	 (350000)	/* KHz */
#define GPU_DVFS_FREQ1_6739WD	 (290000)	/* KHz */
#define GPUFREQ_LAST_FREQ_LEVEL_6739WD	(GPU_DVFS_FREQ1_6739WD)

#define GPU_DVFS_VOLT0_6739WD	 (108750)	/* mV x 100 */
#define GPU_DVFS_VOLT1_6739WD	 (105000)	/* mV x 100 */

#define GPU_DVFS_VSRAM0_6739WD	  (90000)	/* mV x 100 */
#define GPU_DVFS_VSRAM1_6739WD	  (80000)	/* mV x 100 */

#define GPU_DVFS_PTPOD_DISABLE_VOLT_MT6739WD	GPU_DVFS_VOLT0_6739WD

/*****************************************
 * PMIC settle time (us), should not be changed
 ******************************************/
#define PMIC_MAX_VSRAM GPU_DVFS_VSRAM0
#define PMIC_MIN_VCORE GPU_DVFS_VOLT1
#define PMIC_MAX_VCORE GPU_DVFS_VOLT0

#define PMIC_MIN_VCORE_6739TW GPU_DVFS_VOLT1_6739TW
#define PMIC_MAX_VCORE_6739TW GPU_DVFS_VOLT0_6739TW

#define PMIC_MIN_VCORE_6739WD GPU_DVFS_VOLT1_6739WD
#define PMIC_MAX_VCORE_6739WD GPU_DVFS_VOLT0_6739WD

/* mt6739 us * 100 BEGIN */
#define DELAY_FACTOR 625
#define HALF_DELAY_FACTOR 312
#define BUCK_INIT_US 750
#define VPROC_ENABLE_TIME_US 240 /* spec is 220, but more time means more safety */
#define VSRAM_GPU_ENABLE_TIME_US 120 /* spec is 110, but more time means more safety */

/* mt6739 END */


#define PMIC_CMD_DELAY_TIME	 5
#define MIN_PMIC_SETTLE_TIME	25
#define PMIC_VOLT_UP_SETTLE_TIME(old_volt, new_volt)	\
	(((((new_volt) - (old_volt)) + 1250 - 1) / 1250) + PMIC_CMD_DELAY_TIME)
#define PMIC_VOLT_DOWN_SETTLE_TIME(old_volt, new_volt)	\
	(((((old_volt) - (new_volt)) * 2)  / 625) + PMIC_CMD_DELAY_TIME)
#define PMIC_VOLT_ON_OFF_DELAY_US	   (200)
#define INVALID_SLEW_RATE	(0)

/* efuse */
#define GPUFREQ_EFUSE_INDEX		(8)
#define EFUSE_MFG_SPD_BOND_SHIFT	(8)
#define EFUSE_MFG_SPD_BOND_MASK		(0xF)
#define EFUSE_FAB_INFO_TURBO_MASK	(0x300000)
/*
 * LOG
 */
#define TAG	 "[Power/gpufreq] "

#define gpufreq_pr_err(fmt, args...)	   \
	pr_err(TAG"[ERROR]"fmt, ##args)
#define gpufreq_pr_warn(fmt, args...) \
	pr_warn(TAG"[WARNING]"fmt, ##args)
#define gpufreq_info(fmt, args...)	\
	pr_info(TAG""fmt, ##args)
#define gpufreq_dbg(fmt, args...)	   \
	do {								\
		if (mt_gpufreq_debug)		   \
			pr_debug(TAG""fmt, ##args);	 \
	} while (0)
#define gpufreq_ver(fmt, args...)	   \
	do {								\
		if (mt_gpufreq_debug)		   \
			pr_debug(TAG""fmt, ##args);	\
	} while (0)


#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_gpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = NULL,
	.resume = NULL,
};
#endif

static sampler_func g_pFreqSampler;
static sampler_func g_pVoltSampler;

static gpufreq_power_limit_notify g_pGpufreq_power_limit_notify;
#ifdef MT_GPUFREQ_INPUT_BOOST
static gpufreq_input_boost_notify g_pGpufreq_input_boost_notify;
#endif
static gpufreq_ptpod_update_notify g_pGpufreq_ptpod_update_notify;

/* Efuse Device ID */
enum chip_ip_table {
	ID_MT6739 = 0,
	ID_MT6739TW,
	ID_MT6739WD,
};

static enum chip_ip_table device_id;

/***************************
 * GPU DVFS OPP Table
 ****************************/
/* Full Yield */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_mt6739[] = {
	GPUOP(GPU_DVFS_FREQ0, GPU_DVFS_VOLT0, GPU_DVFS_VSRAM0, 0),
	GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1, GPU_DVFS_VSRAM1, 1),
	GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT2, GPU_DVFS_VSRAM1, 2),
	GPUOP(GPU_DVFS_FREQ3, GPU_DVFS_VOLT3, GPU_DVFS_VSRAM1, 3),
};

static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_mt6739tw[] = {
	GPUOP(GPU_DVFS_FREQ0_6739TW, GPU_DVFS_VOLT0_6739TW, GPU_DVFS_VSRAM0_6739TW, 0),
	GPUOP(GPU_DVFS_FREQ1_6739TW, GPU_DVFS_VOLT1_6739TW, GPU_DVFS_VSRAM1_6739TW, 1),
	GPUOP(GPU_DVFS_FREQ2_6739TW, GPU_DVFS_VOLT2_6739TW, GPU_DVFS_VSRAM1_6739TW, 2),
	GPUOP(GPU_DVFS_FREQ3_6739TW, GPU_DVFS_VOLT3_6739TW, GPU_DVFS_VSRAM1_6739TW, 3),
};

static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_mt6739wd[] = {
	GPUOP(GPU_DVFS_FREQ0_6739WD, GPU_DVFS_VOLT0_6739WD, GPU_DVFS_VSRAM0_6739WD, 0),
	GPUOP(GPU_DVFS_FREQ1_6739WD, GPU_DVFS_VOLT1_6739WD, GPU_DVFS_VSRAM1_6739WD, 1),
};

/*
 * AEE (SRAM debug)
 */
#ifdef MT_GPUFREQ_AEE_RR_REC
enum gpu_dvfs_state {
	GPU_DVFS_IS_DOING_DVFS = 0,
	GPU_DVFS_IS_VPROC_ENABLED,
};

static void _mt_gpufreq_aee_init(void)
{
	aee_rr_rec_gpu_dvfs_vgpu(0xFF);
	aee_rr_rec_gpu_dvfs_oppidx(0xFF);
	aee_rr_rec_gpu_dvfs_status(0xFC);
}
#endif

/**************************
 * enable GPU DVFS count
 ***************************/
static int g_gpufreq_dvfs_disable_count;

static unsigned int g_cur_gpu_freq = GPU_DVFS_FREQ0;	/* initial value */
static unsigned int g_cur_gpu_volt = GPU_DVFS_VOLT0;	/* initial value */
static unsigned int g_cur_gpu_idx = 0xFF;
static unsigned int g_cur_gpu_OPPidx = 0xFF;
#ifdef SFCHG
static unsigned int g_vproc_sfchg_rrate = 3; /* just a default value */
static unsigned int g_vproc_sfchg_frate = 3; /* just a default value */
static unsigned int g_vsram_sfchg_rrate = 3; /* just a default value */
static unsigned int g_vsram_sfchg_frate = 3; /* just a default value */
#endif

static unsigned int g_cur_freq_init_keep;

static bool mt_gpufreq_ready;

/* In default settiing, freq_table[0] is max frequency, freq_table[num-1] is min frequency,*/
static unsigned int g_gpufreq_max_id;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id;
static unsigned int g_limited_min_id;

static bool mt_gpufreq_debug;
static bool mt_gpufreq_pause;
static bool mt_gpufreq_keep_max_frequency_state;
static bool mt_gpufreq_keep_opp_frequency_state;
static unsigned int mt_gpufreq_keep_opp_frequency;
static unsigned int mt_gpufreq_keep_opp_index;
static bool mt_gpufreq_fixed_freq_volt_state;
static unsigned int mt_gpufreq_fixed_frequency;
static unsigned int mt_gpufreq_fixed_voltage;

static unsigned int mt_gpufreq_volt_enable;

static unsigned int mt_gpufreq_volt_enable_state;
#ifdef MT_GPUFREQ_INPUT_BOOST
static unsigned int mt_gpufreq_input_boost_state = 1;
#endif
static bool g_limited_thermal_ignore_state;
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static bool g_limited_low_batt_volt_ignore_state;
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static bool g_limited_low_batt_volume_ignore_state;
#endif
#ifdef MT_GPUFREQ_OC_PROTECT
static bool g_limited_oc_ignore_state;
#endif

static bool mt_gpufreq_opp_max_frequency_state;
static unsigned int mt_gpufreq_opp_max_frequency;
static unsigned int mt_gpufreq_opp_max_index;

static unsigned int mt_gpufreq_dvfs_table_type;
static unsigned int mt_gpufreq_dvfs_table_cid;

/* static DEFINE_SPINLOCK(mt_gpufreq_lock); */
static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);

static unsigned int mt_gpufreqs_num;
static struct mt_gpufreq_table_info *mt_gpufreqs;
static struct mt_gpufreq_table_info *mt_gpufreqs_default;
static struct mt_gpufreq_power_table_info *mt_gpufreqs_power;
static struct mt_gpufreq_clk_t *mt_gpufreq_clk;
static struct mt_gpufreq_pmic_t *mt_gpufreq_pmic;


/* static struct mt_gpufreq_power_table_info *mt_gpufreqs_default_power; */

static bool mt_gpufreq_ptpod_disable;
static int mt_gpufreq_ptpod_disable_idx;

static void mt_gpufreq_clock_switch(unsigned int freq_new);
static void mt_gpufreq_volt_switch(unsigned int volt_old, unsigned int volt_new);
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new,
			   unsigned int volt_old, unsigned int volt_new);
static unsigned int _mt_gpufreq_get_cur_volt(void);
static unsigned int _mt_gpufreq_get_cur_freq(void);
static void _mt_gpufreq_kick_pbm(int enable);


#ifndef DISABLE_PBM_FEATURE
static bool g_limited_pbm_ignore_state;
static unsigned int mt_gpufreq_pbm_limited_gpu_power;	/* PBM limit power */
static unsigned int mt_gpufreq_pbm_limited_index;	/* Limited frequency index for PBM */
#define GPU_OFF_SETTLE_TIME_MS		(100)
struct delayed_work notify_pbm_gpuoff_work;
#endif

/* SSPM reserved memory */
/* #ifdef MTK_SSPM */
phys_addr_t gpu_fdvfs_phy_addr, gpu_fdvfs_virt_addr;
uint32_t gpu_dvfs_mem_size;

/* #endif */


int __attribute__ ((weak))
get_immediate_gpu_wrap(void)
{
	gpufreq_pr_err("get_immediate_gpu_wrap doesn't exist");
	return 0;
}

/*************************************************************************************
 * Get Post DIV value
 *
 * @breif
 * VCO needs proper post div value to get corresponding dds value to adjust PLL value
 * e.g: VCO range is 1.5GHz - 3.8GHz
 *	  required frequency is 900MHz, so pos div could be 2(1.8/2), 4(3.6/4), 8(X), 16(X)
 * It may have special requiremt by DE in different efuse value
 * e.g: In MT6757, efuse value(3'b001) ,VCO range is 1.5GHz - 3.8GHz
 *	  375MHz - 900MHz, It can only use post div 4, no pos div 2
 *
 * @param[in] freq: required frequency
 * @param[in] efuse: efuse value
 * return corresponding post div by freq and efuse
 **************************************************************************************/
static enum post_div_order_enum _get_post_div_order(unsigned int freq, unsigned int efuse)
{
	/* [MT6757]VCO range: 1.5G - 3.8GHz by div 1/2/4/8/16
	*
	*	PLL range: 125MHz - 3.8GHz
	*	Version(eFuse Value)			Info	 POSTDIV		   FOUT
	*	Free Version(efuse 3'b000)	 No limit  1/2/4/8/16  125MHz - 3800MHz
	*/
	enum post_div_order_enum post_div_order = POST_DIV4;

	post_div_order = POST_DIV4;

	gpufreq_dbg("@%s: freq = %d efuse = %d post_div_order = %d\n", __func__, freq, efuse, post_div_order);
	return post_div_order;
}


#ifdef MT_GPUFREQ_INPUT_BOOST
static struct task_struct *mt_gpufreq_up_task;

static int mt_gpufreq_input_boost_task(void *data)
{
	while (1) {
		gpufreq_dbg("@%s: begin\n", __func__);

		if (g_pGpufreq_input_boost_notify != NULL) {
			gpufreq_dbg("@%s: g_pGpufreq_input_boost_notify\n", __func__);
			g_pGpufreq_input_boost_notify(g_gpufreq_max_id);
		}

		gpufreq_dbg("@%s: end\n", __func__);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}


/*************************************************************************************
 * Input boost
 **************************************************************************************/
static void mt_gpufreq_input_event(struct input_handle *handle, unsigned int type,
				   unsigned int code, int value)
{
	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1)
		&& (mt_gpufreq_input_boost_state == 1)) {
		gpufreq_dbg("@%s: accept.\n", __func__);

		wake_up_process(mt_gpufreq_up_task);
	}
}

static int mt_gpufreq_input_connect(struct input_handler *handler,
					struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "gpufreq_ib";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void mt_gpufreq_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id mt_gpufreq_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler mt_gpufreq_input_handler = {
	.event = mt_gpufreq_input_event,
	.connect = mt_gpufreq_input_connect,
	.disconnect = mt_gpufreq_input_disconnect,
	.name = "gpufreq_ib",
	.id_table = mt_gpufreq_ids,
};
#endif /* mt_gpufreq_input_handler  */

/*
 * Power table calculation
 */
static void mt_gpufreq_power_calculation(unsigned int idx, unsigned int freq,
					  unsigned int volt, unsigned int temp)
{
#define GPU_ACT_REF_POWER				250	/* mW  */
#define GPU_ACT_REF_FREQ				420000	/* KHz */
#define GPU_ACT_REF_VOLT				115000	/* mV x 100 */
#define GPU_ACT_REF_POWER_MT6739W		350	/* mW  */
#define GPU_ACT_REF_FREQ_MT6739W		570000	/* KHz */
#define GPU_ACT_REF_VOLT_MT6739W		115000	/* mV x 100 */

	unsigned int p_total = 0, p_dynamic = 0, ref_freq = 0, ref_volt = 0;
	int p_leakage = 0;

	if (mt_gpufreq_dvfs_table_type == ID_MT6739TW) {
		p_dynamic = GPU_ACT_REF_POWER_MT6739W;
		ref_freq = GPU_ACT_REF_FREQ_MT6739W;
		ref_volt = GPU_ACT_REF_VOLT_MT6739W;
	} else {
		p_dynamic = GPU_ACT_REF_POWER;
		ref_freq = GPU_ACT_REF_FREQ;
		ref_volt = GPU_ACT_REF_VOLT;
	}

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) * ((volt * 100) / ref_volt) / (100 * 100 * 100);

#ifdef STATIC_PWR_READY2USE
	p_leakage =
		mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (!mt_gpufreq_volt_enable_state || p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif

	p_total = p_dynamic + p_leakage;

	gpufreq_ver("%d: p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
			idx, p_dynamic, p_leakage, p_total, temp);

	mt_gpufreqs_power[idx].gpufreq_power = p_total;
}

/**************************************
 * Random seed generated for test
 ***************************************/
#ifdef MT_GPU_DVFS_RANDOM_TEST
static int mt_gpufreq_idx_get(int num)
{
	int random = 0, mult = 0, idx;

	random = jiffies & 0xF;

	while (1) {
		if ((mult * num) >= random) {
			idx = (mult * num) - random;
			break;
		}
		mult++;
	}
	return idx;
}
#endif


/* Set frequency and voltage at driver probe function */
static void mt_gpufreq_set_initial(void)
{
	unsigned int cur_volt = 0, cur_freq = 0;
	/*int i = 0;*/

	mutex_lock(&mt_gpufreq_lock);

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_DOING_DVFS));
#endif
	gpufreq_dbg("[Figo] Into mt_gpufreq_set_initial");
	cur_volt = _mt_gpufreq_get_cur_volt();
	cur_freq = _mt_gpufreq_get_cur_freq();

#if 0
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (cur_volt >= mt_gpufreqs[i].gpufreq_volt) {
			mt_gpufreq_set(cur_freq, mt_gpufreqs[i].gpufreq_khz,
				   cur_volt, mt_gpufreqs[i].gpufreq_volt);
			g_cur_gpu_OPPidx = i;
			gpufreq_dbg("init_idx = %d\n", g_cur_gpu_OPPidx);
			_mt_gpufreq_kick_pbm(1);
			break;
		}
	}

	/* Not found, set to LPM */
	if (i == mt_gpufreqs_num) {
		gpufreq_pr_err
			("Set to LPM since GPU idx not found according to current Vcore = %d mV\n",
			 cur_volt / 100);
		g_cur_gpu_OPPidx = mt_gpufreqs_num - 1;
		mt_gpufreq_set(cur_freq, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_khz,
				   cur_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);
	}
#endif

	g_cur_gpu_OPPidx = 0;
	mt_gpufreq_set(cur_freq, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_khz,
				   cur_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);

	g_cur_gpu_freq = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_khz;
	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt;
	g_cur_gpu_idx = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_idx;

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_oppidx(g_cur_gpu_OPPidx);
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() & ~(1 << GPU_DVFS_IS_DOING_DVFS));
#endif

	mutex_unlock(&mt_gpufreq_lock);
}


#ifndef DISABLE_PBM_FEATURE
static void mt_gpufreq_notify_pbm_gpuoff(struct work_struct *work)
{
	mutex_lock(&mt_gpufreq_lock);
	if (!mt_gpufreq_volt_enable_state)
		_mt_gpufreq_kick_pbm(0);

	mutex_unlock(&mt_gpufreq_lock);
}
#endif


/* Set VPROC/VSRAM enable/disable when GPU clock be switched on/off */
unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	int ret = 0;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_dbg("[Figo][+]@%s: enable = %d, mt_gpufreq_volt_enable_state = %d\n",
		__func__, enable, mt_gpufreq_volt_enable_state);


#ifdef VPROC_SET_BY_PMIC
	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		ret = DRIVER_NOT_READY;
		goto SET_EXIT;
	}

	{
	#ifdef VCORE_DVFS_OPP_SUPPORT
		if (enable == BUCK_ON) {
			switch (g_cur_gpu_volt) {
			case GPU_DVFS_VOLT0:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_0);
				break;
			case GPU_DVFS_VOLT1:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_1);
				break;
			case GPU_DVFS_VOLT2:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_2);
				break;
			case GPU_DVFS_VOLT3:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_3);
				break;
			default:
				gpufreq_pr_warn("@%s: ummapped voltage! volt %d\n", __func__, g_cur_gpu_volt);
				break;
			}
		} else {
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_UNREQ);
		}
		if (ret)
			gpufreq_pr_warn("@%s: vcorefs_request_dvfs_opp fail! volt %d\n", __func__, g_cur_gpu_volt);
	#else
		gpufreq_dbg("@%s: VCORE_DVFS_OPP_SUPPORT not defined!, need to implement it by PMIC\n",
			__func__);
	#endif
	}
#endif

#ifndef DISABLE_PBM_FEATURE
	if (enable == 1) {
		if (delayed_work_pending(&notify_pbm_gpuoff_work))
			cancel_delayed_work(&notify_pbm_gpuoff_work);
		else
			_mt_gpufreq_kick_pbm(1);
	} else {
		schedule_delayed_work(&notify_pbm_gpuoff_work, msecs_to_jiffies(GPU_OFF_SETTLE_TIME_MS));
	}

#endif

	mt_gpufreq_volt_enable_state = enable;
SET_EXIT:
	gpufreq_dbg("[Figo][-]@%s: enable = %d, mt_gpufreq_volt_enable_state = %d\n",
		__func__, enable, mt_gpufreq_volt_enable_state);
	mutex_unlock(&mt_gpufreq_lock);

	return ret;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_enable_set);


/* Set VPROC enable/disable when GPU clock be switched on/off */
unsigned int mt_gpufreq_voltage_lpm_set(unsigned int enable_lpm)
{
	int ret = 0;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_dbg("[Figo][+]@%s: enable_lpm = %d, mt_gpufreq_volt_enable_state = %d\n",
		__func__, enable_lpm, mt_gpufreq_volt_enable_state);

	if (mt_gpufreq_ready == false) {
		gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);
		ret = DRIVER_NOT_READY;
		goto SET_LPM_EXIT;
	}

	{
	#ifdef VCORE_DVFS_OPP_SUPPORT
		if (enable_lpm) {
			if (!mt_gpufreq_ptpod_disable)
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_UNREQ);
		} else {
			switch (g_cur_gpu_volt) {
			case GPU_DVFS_VOLT0:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_0);
				break;
			case GPU_DVFS_VOLT1:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_1);
				break;
			case GPU_DVFS_VOLT2:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_2);
				break;
			case GPU_DVFS_VOLT3:
				ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_3);
				break;
			default:
				gpufreq_pr_warn("@%s: ummapped voltage! volt %d\n", __func__, g_cur_gpu_volt);
				break;
			}
		}
		if (ret)
			gpufreq_pr_warn("@%s: vcorefs_request_dvfs_opp fail! volt %d\n", __func__, g_cur_gpu_volt);
	#else
		gpufreq_dbg("@%s: VCORE_DVFS_OPP_SUPPORT not defined!, need to implement it by PMIC\n",
			__func__);
	#endif
	}

	if (enable_lpm)
		mt_gpufreq_volt_enable_state = 0;
	else
		mt_gpufreq_volt_enable_state = 1;

SET_LPM_EXIT:
	gpufreq_dbg("[Figo][-]@%s: enable_lpm = %d, mt_gpufreq_volt_enable_state = %d\n",
		__func__, enable_lpm, mt_gpufreq_volt_enable_state);
	mutex_unlock(&mt_gpufreq_lock);

	return ret;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_lpm_set);

/************************************************
 * DVFS enable API for PTPOD
 *************************************************/

void mt_gpufreq_enable_by_ptpod(void)
{
#ifdef VOLT_SET_READY
	gpufreq_dbg("[Figo] Power off by mt_gpufreq_enable_by_ptpod\n");
	mt_gpufreq_voltage_enable_set(0);
#endif

	mt_gpufreq_ptpod_disable = false;
	gpufreq_info("mt_gpufreq enabled by ptpod\n");
}
EXPORT_SYMBOL(mt_gpufreq_enable_by_ptpod);

/************************************************
 * DVFS disable API for PTPOD
 *************************************************/
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0, target_idx = 0;

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	mt_gpufreq_ptpod_disable = true;
	mt_gpufreq_voltage_lpm_set(0);
	gpufreq_info("mt_gpufreq disabled by ptpod\n");

	for (i = 0; i < mt_gpufreqs_num; i++) {
		target_idx = i;
		if (mt_gpufreqs_default[i].gpufreq_volt <= GPU_DVFS_PTPOD_DISABLE_VOLT)
			break;
	}

	mt_gpufreq_ptpod_disable_idx = target_idx;

#ifdef VOLT_SET_READY
	gpufreq_dbg("[Figo] Power on by mt_gpufreq_disable_by_ptpod\n");
	mt_gpufreq_voltage_enable_set(1);
#endif
	mt_gpufreq_target(target_idx);
}
EXPORT_SYMBOL(mt_gpufreq_disable_by_ptpod);

/************************************************
 * API to switch back default voltage setting for GPU PTPOD disabled
 *************************************************/
void mt_gpufreq_restore_default_volt(void)
{
	int i;

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	mutex_lock(&mt_gpufreq_lock);

	for (i = 0; i < mt_gpufreqs_num; i++) {
		mt_gpufreqs[i].gpufreq_volt = mt_gpufreqs_default[i].gpufreq_volt;
		gpufreq_dbg("@%s: mt_gpufreqs[%d].gpufreq_volt = %x\n", __func__, i,
				mt_gpufreqs[i].gpufreq_volt);
	}

    /* Use VCORE for power Control */
	mt_gpufreq_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);

	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt;

	mutex_unlock(&mt_gpufreq_lock);
}
EXPORT_SYMBOL(mt_gpufreq_restore_default_volt);

/* Set voltage because PTP-OD modified voltage table by PMIC wrapper */
unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	int i;			/* , idx; */
	/* unsigned long flags; */

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return DRIVER_NOT_READY;
	}

	mutex_lock(&mt_gpufreq_lock);

	for (i = 0; i < array_size; i++) {
		mt_gpufreqs[i].gpufreq_volt = pmic_volt[i];
		gpufreq_dbg("@%s: mt_gpufreqs[%d].gpufreq_volt = %d\n", __func__, i,
				mt_gpufreqs[i].gpufreq_volt);
	}

    /* Use VCORE for power Control */
	mt_gpufreq_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt);

	g_cur_gpu_volt = mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_volt;
	if (g_pGpufreq_ptpod_update_notify != NULL)
		g_pGpufreq_ptpod_update_notify();
	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_update_volt);

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return mt_gpufreqs_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return DRIVER_NOT_READY;
	}

	if (idx < mt_gpufreqs_num) {
		gpufreq_dbg("@%s: idx = %d, frequency= %d\n", __func__, idx,
				mt_gpufreqs[idx].gpufreq_khz);
		return mt_gpufreqs[idx].gpufreq_khz;
	}

	gpufreq_dbg("@%s: idx = %d, NOT found! return 0!\n", __func__, idx);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

int mt_gpufreq_get_idx_by_target_freq(unsigned int target_freq)
{
	int i = mt_gpufreqs_num - 1;

	gpufreq_dbg("@%s: freq = %u\n", __func__, target_freq);
	while (i >= 0) {
		if (mt_gpufreqs[i--].gpufreq_khz >= target_freq)
			goto TARGET_EXIT;
	}

TARGET_EXIT:
	gpufreq_pr_err("@%s: idx = %u\n", __func__, i+1);
	return i+1;
}

unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return DRIVER_NOT_READY;
	}

	if (idx < mt_gpufreqs_num) {
		gpufreq_dbg("@%s: idx = %d, voltage= %d\n", __func__, idx,
				mt_gpufreqs[idx].gpufreq_volt);
		return mt_gpufreqs[idx].gpufreq_volt;
	}

	gpufreq_dbg("@%s: idx = %d, NOT found! return 0!\n", __func__, idx);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_volt_by_idx);

#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
static void mt_update_gpufreqs_power_table(void)
{
	int i = 0;
	int temp = 0;
	unsigned int freq = 0, volt = 0;

	if (mt_gpufreq_ready == false) {
		gpufreq_pr_warn("@%s: GPU DVFS not ready\n", __func__);
		return;
	}

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

	gpufreq_dbg("@%s, temp = %d\n", __func__, temp);

	mutex_lock(&mt_gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < mt_gpufreqs_num; i++) {
			freq = mt_gpufreqs_power[i].gpufreq_khz;
			volt = mt_gpufreqs_power[i].gpufreq_volt;

			mt_gpufreq_power_calculation(i, freq, volt, temp);

			gpufreq_ver("update mt_gpufreqs_power[%d].gpufreq_khz = %d\n", i,
					mt_gpufreqs_power[i].gpufreq_khz);
			gpufreq_ver("update mt_gpufreqs_power[%d].gpufreq_volt = %d\n", i,
					mt_gpufreqs_power[i].gpufreq_volt);
			gpufreq_ver("update mt_gpufreqs_power[%d].gpufreq_power = %d\n", i,
					mt_gpufreqs_power[i].gpufreq_power);
		}
	} else
		gpufreq_pr_err("@%s: temp < 0 or temp > 125, NOT update power table!\n", __func__);

	mutex_unlock(&mt_gpufreq_lock);
}
#endif

static void mt_setup_gpufreqs_power_table(int num)
{
	int i = 0, temp = 0;

	mt_gpufreqs_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);
	if (mt_gpufreqs_power == NULL)
		return;
#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

	gpufreq_dbg("@%s: temp = %d\n", __func__, temp);

	if ((temp < -20) || (temp > 125)) {
		gpufreq_dbg("@%s: temp < 0 or temp > 125!\n", __func__);
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		/* fill-in freq and volt in power table */
		mt_gpufreqs_power[i].gpufreq_khz = mt_gpufreqs[i].gpufreq_khz;
		mt_gpufreqs_power[i].gpufreq_volt = mt_gpufreqs[i].gpufreq_volt;

		mt_gpufreq_power_calculation(i,
						 mt_gpufreqs_power[i].gpufreq_khz,
						 mt_gpufreqs_power[i].gpufreq_volt,
						 temp);

		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_khz = %u\n", i,
				 mt_gpufreqs_power[i].gpufreq_khz);
		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_volt = %u\n", i,
				 mt_gpufreqs_power[i].gpufreq_volt);
		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_power = %u\n", i,
				 mt_gpufreqs_power[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_power, num);
#endif
}

/***********************************************
 * register frequency table to gpufreq subsystem
 ************************************************/
static int mt_setup_gpufreqs_table(struct mt_gpufreq_table_info *freqs, int num)
{
	int i = 0;

	mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	mt_gpufreqs_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	if (mt_gpufreqs == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs[i].gpufreq_vsram = freqs[i].gpufreq_vsram;
		mt_gpufreqs[i].gpufreq_idx = freqs[i].gpufreq_idx;

		mt_gpufreqs_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs_default[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs_default[i].gpufreq_vsram = freqs[i].gpufreq_vsram;
		mt_gpufreqs_default[i].gpufreq_idx = freqs[i].gpufreq_idx;

		gpufreq_dbg("freqs[%d].gpufreq_khz = %u\n", i, freqs[i].gpufreq_khz);
		gpufreq_dbg("freqs[%d].gpufreq_volt = %u\n", i, freqs[i].gpufreq_volt);
		gpufreq_dbg("freqs[%d].gpufreq_vsram = %u\n", i, freqs[i].gpufreq_vsram);
		gpufreq_dbg("freqs[%d].gpufreq_idx = %u\n", i, freqs[i].gpufreq_idx);
	}

	mt_gpufreqs_num = num;

	g_limited_max_id = 0;
	g_limited_min_id = mt_gpufreqs_num - 1;

	gpufreq_info("@%s: g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", __func__, g_cur_gpu_freq,
			 g_cur_gpu_volt);

	mt_setup_gpufreqs_power_table(num);

	return 0;
}

/**************************************
 * check if maximum frequency is needed
 ***************************************/
static int mt_gpufreq_keep_max_freq(unsigned int freq_old, unsigned int freq_new)
{
	if (mt_gpufreq_keep_max_frequency_state == true)
		return 1;

	return 0;
}

/*****************************
 * set GPU DVFS status
 ******************************/
int mt_gpufreq_state_set(int enabled)
{
	if (enabled) {
		if (!mt_gpufreq_pause) {
			gpufreq_dbg("gpufreq already enabled\n");
			return 0;
		}

		/*****************
		 * enable GPU DVFS
		 ******************/
		g_gpufreq_dvfs_disable_count--;
		gpufreq_dbg("enable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
				g_gpufreq_dvfs_disable_count);

		/***********************************************
		 * enable DVFS if no any module still disable it
		 ************************************************/
		if (g_gpufreq_dvfs_disable_count <= 0)
			mt_gpufreq_pause = false;
		else
			gpufreq_pr_warn("someone still disable gpufreq, cannot enable it\n");
	} else {
		/******************
		 * disable GPU DVFS
		 *******************/
		g_gpufreq_dvfs_disable_count++;
		gpufreq_dbg("disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
				g_gpufreq_dvfs_disable_count);

		if (mt_gpufreq_pause) {
			gpufreq_dbg("gpufreq already disabled\n");
			return 0;
		}

		mt_gpufreq_pause = true;
	}

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_state_set);


static unsigned int mt_gpufreq_dds_calc(unsigned int freq_khz, enum post_div_order_enum post_div_order)
{
	unsigned int dds = 0;

	gpufreq_dbg("@%s: request freq = %d, div_order = %d\n",
			__func__, freq_khz, post_div_order);
	/*
	 * Below freq range is Austin @ mt6739 only:
	 *
	 *	dds formula is consistent with Elbrus
	 *
	 */
	if ((freq_khz >= GPUFREQ_LAST_FREQ_LEVEL) && (freq_khz <= GPU_DVFS_FREQ0_6739TW)) {
		dds = (((freq_khz / TO_MHz_HEAD * (1 << post_div_order)) << DDS_SHIFT)
			/ GPUPLL_FIN + ROUNDING_VALUE) / TO_MHz_TAIL;
	} else {
		gpufreq_pr_err("@%s: target freq_khz(%d) out of range!\n", __func__, freq_khz);
		/* WARN_ON(); */
	}

	return dds;
}


static void mt_gpufreq_clock_switch_transient(unsigned int freq_new,  enum post_div_order_enum post_div_order)
{
	unsigned int cur_volt;
	unsigned int cur_freq;
	unsigned int dds;

	/*	GPUPLL_CON1: 31: MMPLL_SDM_PCW_CHG
	*			  26-24: GPUPLL_POSDIV
	*				  000: /1
	*				  001: /2
	*				  010: /4
	*				  011: /8
	*				  100: /18
	*			  21-0:MMPLL_SDM_PCW
	*/

	cur_volt = _mt_gpufreq_get_cur_volt();
	cur_freq = _mt_gpufreq_get_cur_freq();
	dds = mt_gpufreq_dds_calc(freq_new, post_div_order);
	gpufreq_dbg("@%s: request GPU dds = 0x%x, cur_volt = %d, cur_freq = %d\n",
			__func__, dds, cur_volt, cur_freq);

	gpufreq_dbg("@%s: request GPUPLL_CON1 = 0x%x\n",
			__func__, DRV_Reg32(GPUPLL_CON1));

	/* Step2. Modify gpupll_ck */
	mt_dfs_general_pll(FH_PLL3, dds);

}

/* static void _mt_gpufreq_set_cur_freq(unsigned int freq_new) */
static void mt_gpufreq_clock_switch(unsigned int freq_new)
{
	unsigned int mfgpll;

	if (mt_gpufreq_dvfs_table_cid == 0)
		mt_gpufreq_clock_switch_transient(freq_new, _get_post_div_order(freq_new, mt_gpufreq_dvfs_table_cid));
	else
		gpufreq_dbg("@%s: efuse number type(%d)\n", __func__, mt_gpufreq_dvfs_table_cid);

	mfgpll = DRV_Reg32(GPUPLL_CON1);
	gpufreq_dbg("@%s: freq_new = %d, mfgpll = 0x%x\n", __func__, freq_new, mfgpll);
	if (g_pFreqSampler != NULL)
		g_pFreqSampler(freq_new);

}

#if 0
static int mt_gpufreq_get_idx_by_target_volt(unsigned int target_volt)
{
	int i = mt_gpufreqs_num - 1;

	while (i >= 0) {
		if (mt_gpufreqs[i--].gpufreq_volt >= target_volt)
			goto TARGET_EXIT;
	}

TARGET_EXIT:
	return i+1;
}
#endif

#ifndef VCORE_DVFS_OPP_SUPPORT
static unsigned int _calculate_vproc_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_reg;
	unsigned int sfchg_rate_vproc;

	/* [MT6739] RG_LDO_VSRAM_PROC_SFCHG_RRATE (rising rate) and RG_LDO_VSRAM_PROC_SFCHG_FRATE (falling rate)
	*	4:	0.19us
	*	8:	0.34us
	*	11:	0.46us
	*	17:	0.69us
	*	23:	0.92us
	*	25:	1us
	*/
	if (isRising) {
		pmic_read_interface(MT6356_BUCK_VPROC_CFG0, &sfchg_rate_reg,
		PMIC_RG_BUCK_VPROC_SFCHG_RRATE_MASK, PMIC_RG_BUCK_VPROC_SFCHG_RRATE_SHIFT);
	} else {
		pmic_read_interface(MT6356_BUCK_VPROC_CFG0, &sfchg_rate_reg,
		PMIC_RG_BUCK_VPROC_SFCHG_FRATE_MASK, PMIC_RG_BUCK_VPROC_SFCHG_FRATE_SHIFT);
	}

	sfchg_rate_vproc = 1;

	gpufreq_dbg("@%s: isRising(%d), sfchg_rate_vproc = %d, sfchg_rate_reg = %x\n",
		__func__, isRising, sfchg_rate_vproc, sfchg_rate_reg);

	return sfchg_rate_vproc;
}
static unsigned int _calculate_vsram_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_reg;
	unsigned int sfchg_rate_vsram;

	/* [MT6739] RG_LDO_VSRAM_GPU_SFCHG_RRATE and RG_LDO_VSRAM_GPU_SFCHG_FRATE
	*	4:	0.19us
	*	8:	0.34us
	*	11:	0.46us
	*	17:	0.69us
	*	23:	0.92us
	*	25:	1us
	*/

	if (isRising) {
		pmic_read_interface(MT6356_LDO_VSRAM_GPU_CFG0, &sfchg_rate_reg,
		PMIC_RG_LDO_VSRAM_GPU_SFCHG_RRATE_MASK, PMIC_RG_LDO_VSRAM_GPU_SFCHG_RRATE_SHIFT);
		sfchg_rate_vsram = 1;
	} else {
		pmic_read_interface(MT6356_LDO_VSRAM_GPU_CFG0, &sfchg_rate_reg,
		PMIC_RG_LDO_VSRAM_GPU_SFCHG_FRATE_MASK, PMIC_RG_LDO_VSRAM_GPU_SFCHG_FRATE_SHIFT);
		sfchg_rate_vsram = 1;
	}

	gpufreq_dbg("@%s: isRising(%d), sfchg_rate_vsram = %d, sfchg_rate_reg = %x\n",
		__func__, isRising, sfchg_rate_vsram, sfchg_rate_reg);

	return sfchg_rate_vsram;
}
#endif


static void mt_gpufreq_volt_switch(unsigned int volt_old, unsigned int volt_new)
{
#ifdef VCORE_DVFS_OPP_SUPPORT
	if (volt_new != volt_old) {
		int ret = 0;

		switch (volt_new) {
		case GPU_DVFS_VOLT0:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_0);
			break;
		case GPU_DVFS_VOLT1:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_1);
			break;
		case GPU_DVFS_VOLT2:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_2);
			break;
		case GPU_DVFS_VOLT3:
			ret = vcorefs_request_dvfs_opp(KIR_GPU, OPP_3);
			break;
		default:
			gpufreq_pr_warn("@%s: ummapped voltage! volt %d\n", __func__, volt_new);
			break;
		}
		if (ret)
			gpufreq_pr_warn("@%s: vcorefs_request_dvfs_opp fail! volt %d\n", __func__, volt_new);
	}
#else
	gpufreq_dbg("@%s: VCORE_DVFS_OPP_SUPPORT not defined!, need to implement it by PMIC\n",
		__func__);
#endif
}

static unsigned int _mt_gpufreq_get_cur_freq(void)
{
	unsigned long mfgpll = 0;
	unsigned int post_div_order = 0;
	unsigned int freq_khz = 0;
	unsigned long dds;

	mfgpll = DRV_Reg32(GPUPLL_CON1);
	post_div_order = (mfgpll & (0x7 << POST_DIV_SHIFT)) >> POST_DIV_SHIFT;

	/*
	 *	become dds
	 */
	dds = mfgpll & (0x3FFFFF);

	freq_khz = (((dds * TO_MHz_TAIL + ROUNDING_VALUE)*GPUPLL_FIN) >> DDS_SHIFT)
		/ (1 << post_div_order) *  TO_MHz_HEAD;

		if (freq_khz < DIV4_MIN_FREQ || freq_khz > DIV4_MAX_FREQ)
			gpufreq_pr_err("Invalid out-of-bound freq %d KHz, mfgpll value = 0x%lx\n",
				freq_khz, mfgpll);
		else
			gpufreq_dbg("mfgpll = 0x%lx, freq = %d KHz, div_order = %d\n",
				mfgpll, freq_khz, post_div_order);

	return freq_khz;		/* KHz */
}

static unsigned int _mt_gpufreq_get_cur_volt(void)
{
	unsigned int gpu_volt = 0;
	{
		gpu_volt = regulator_get_voltage(mt_gpufreq_pmic->reg_vcore) / 10;
		gpufreq_dbg("gpu_dvfs_get_cur_volt:[PMIC] volt = %d\n", gpu_volt);
	}

	return gpu_volt;
}

static void _mt_gpufreq_kick_pbm(int enable)
{
#ifndef DISABLE_PBM_FEATURE
	static unsigned int power;
	static unsigned int cur_volt;
	int i;
	int tmp_idx = -1;
	unsigned int found = 0;
	unsigned int cur_freq = _mt_gpufreq_get_cur_freq();

	cur_volt = _mt_gpufreq_get_cur_volt();

	if (enable) {
		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs_power[i].gpufreq_khz == cur_freq) {
				/* record idx since current voltage may not in DVFS table */
				tmp_idx = i;

				if (mt_gpufreqs_power[i].gpufreq_volt == cur_volt) {
					power = mt_gpufreqs_power[i].gpufreq_power;
					found = 1;
					kicker_pbm_by_gpu(true, power, cur_volt / 100);
					gpufreq_dbg
						("@%s: request GPU power = %d, cur_volt = %d, cur_freq = %d\n",
						 __func__, power, cur_volt / 100, cur_freq);
					return;
				}
			}
		}

		if (!found) {
			gpufreq_dbg("@%s: tmp_idx = %d\n", __func__, tmp_idx);

			if (tmp_idx != -1 && tmp_idx < mt_gpufreqs_num) {
				/* use freq to found corresponding power budget */
				power = mt_gpufreqs_power[tmp_idx].gpufreq_power;
				kicker_pbm_by_gpu(true, power, cur_volt / 100);
				gpufreq_dbg
					("@%s: request GPU power = %d, cur_volt = %d, cur_freq = %d\n",
					 __func__, power, cur_volt / 100, cur_freq);
			} else {
				gpufreq_pr_warn("@%s: Cannot found request power in power table!\n",
						 __func__);
				gpufreq_pr_warn("cur_freq = %dKHz, cur_volt = %dmV\n", cur_freq,
						 cur_volt / 100);
			}
		}
	} else {
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
	}
#endif
}

/*****************************************
 * frequency ramp up and ramp down handler
 ******************************************/
/***********************************************************
 * [note]
 * 1. frequency ramp up need to wait voltage settle
 * 2. frequency ramp down do not need to wait voltage settle
 ************************************************************/
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new,
			   unsigned int volt_old, unsigned int volt_new)
{

	gpufreq_dbg("[Figo][+]@%s: Freq: %d ---> %d, Volt: %d ---> %d\n",
		__func__, freq_old, freq_new, volt_old, volt_new);
	if (freq_new > freq_old) {
		mt_gpufreq_volt_switch(volt_old, volt_new);
		mt_gpufreq_clock_switch(freq_new);
	} else {
		mt_gpufreq_clock_switch(freq_new);
		mt_gpufreq_volt_switch(volt_old, volt_new);
	}
	gpufreq_dbg("[Figo][-]@%s: Freq: %d ---> %d, Volt: %d ---> %d\n",
		__func__, freq_old, freq_new, volt_old, volt_new);
	gpufreq_dbg("[Figo][Status]@%s: freq:%d, volt:%d\n",
		__func__, mt_get_ckgen_freq(7), _mt_gpufreq_get_cur_volt());

	g_cur_gpu_freq = freq_new;
	g_cur_gpu_volt = volt_new;

	_mt_gpufreq_kick_pbm(1);
}

/**********************************
 * gpufreq target callback function
 ***********************************/
/*************************************************
 * [note]
 * 1. handle frequency change request
 * 2. call mt_gpufreq_set to set target frequency
 **************************************************/
unsigned int mt_gpufreq_target(unsigned int idx)
{
	/* unsigned long flags; */
	unsigned int target_freq, target_volt, target_idx, target_OPPidx;

#ifdef MT_GPUFREQ_PERFORMANCE_TEST
	return 0;
#endif

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_pause == true) {
		gpufreq_pr_warn("GPU DVFS pause!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return DRIVER_NOT_READY;
	}

	if (mt_gpufreq_ready == false) {
		gpufreq_pr_warn("GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return DRIVER_NOT_READY;
	}

	if (mt_gpufreq_volt_enable_state == 0) {
		gpufreq_dbg("mt_gpufreq_volt_enable_state == 0! return\n");
		mutex_unlock(&mt_gpufreq_lock);
		return DRIVER_NOT_READY;
	}
#ifdef MT_GPU_DVFS_RANDOM_TEST
	idx = mt_gpufreq_idx_get(5);
	gpufreq_dbg("@%s: random test index is %d !\n", __func__, idx);
#endif

	if (idx > (mt_gpufreqs_num - 1)) {
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_pr_err("@%s: idx out of range! idx = %d\n", __func__, idx);
		return -1;
	}

	/**********************************
	 * look up for the target GPU OPP
	 ***********************************/
	target_freq = mt_gpufreqs[idx].gpufreq_khz;
	target_volt = mt_gpufreqs[idx].gpufreq_volt;
	target_idx = mt_gpufreqs[idx].gpufreq_idx;
	target_OPPidx = idx;

	gpufreq_dbg("@%s: begin, receive freq: %d, OPPidx: %d\n", __func__, target_freq,
			target_OPPidx);

	/**********************************
	 * Check if need to keep max frequency
	 ***********************************/
	if (mt_gpufreq_keep_max_freq(g_cur_gpu_freq, target_freq)) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		target_idx = mt_gpufreqs[g_gpufreq_max_id].gpufreq_idx;
		target_OPPidx = g_gpufreq_max_id;
		gpufreq_dbg("Keep MAX frequency %d !\n", target_freq);
	}

	/************************************************
	 * If /proc command keep opp frequency.
	 *************************************************/
	if (mt_gpufreq_keep_opp_frequency_state == true) {
		target_freq = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz;
		target_volt = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_idx;
		target_OPPidx = mt_gpufreq_keep_opp_index;
		gpufreq_dbg("Keep opp! opp frequency %d, opp voltage %d, opp idx %d\n", target_freq,
				target_volt, target_OPPidx);
	}

	/************************************************
	 * If /proc command fix the frequency.
	 *************************************************/
	if (mt_gpufreq_fixed_freq_volt_state == true) {
		target_freq = mt_gpufreq_fixed_frequency;
		target_volt = mt_gpufreq_fixed_voltage;
		target_idx = 0;
		target_OPPidx = 0;
		gpufreq_dbg("Fixed! fixed frequency %d, fixed voltage %d\n", target_freq,
				target_volt);
	}

	/************************************************
	 * If /proc command keep opp max frequency.
	 *************************************************/
	if (mt_gpufreq_opp_max_frequency_state == true) {
		if (target_freq > mt_gpufreq_opp_max_frequency) {
			target_freq = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;
			target_volt = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt;
			target_idx = mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_idx;
			target_OPPidx = mt_gpufreq_opp_max_index;

			gpufreq_dbg
				("opp max freq! opp max frequency %d, opp max voltage %d, opp max idx %d\n",
				 target_freq, target_volt, target_OPPidx);
		}
	}

	/************************************************
	 * PBM limit
	 *************************************************/
#ifndef DISABLE_PBM_FEATURE
	if (mt_gpufreq_pbm_limited_index != 0) {
		if (target_freq > mt_gpufreqs[mt_gpufreq_pbm_limited_index].gpufreq_khz) {
			/*********************************************
			* target_freq > limited_freq, need to adjust
			**********************************************/
			target_freq = mt_gpufreqs[mt_gpufreq_pbm_limited_index].gpufreq_khz;
			target_volt = mt_gpufreqs[mt_gpufreq_pbm_limited_index].gpufreq_volt;
			target_OPPidx = mt_gpufreq_pbm_limited_index;
			gpufreq_dbg("Limit! Thermal/Power limit gpu frequency %d\n",
					mt_gpufreqs[mt_gpufreq_pbm_limited_index].gpufreq_khz);
		}
	}
#endif

	/************************************************
	 * Thermal/Power limit
	 *************************************************/
	if (g_limited_max_id != 0) {
		if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz) {
			/*********************************************
			 * target_freq > limited_freq, need to adjust
			 **********************************************/
			target_freq = mt_gpufreqs[g_limited_max_id].gpufreq_khz;
			target_volt = mt_gpufreqs[g_limited_max_id].gpufreq_volt;
			target_idx = mt_gpufreqs[g_limited_max_id].gpufreq_idx;
			target_OPPidx = g_limited_max_id;
			gpufreq_info("Limit! Thermal/Power limit gpu frequency %d\n",
					 mt_gpufreqs[g_limited_max_id].gpufreq_khz);
		}
	}

	/************************************************
	 * DVFS keep at max freq when PTPOD initial
	 *************************************************/
	if (mt_gpufreq_ptpod_disable == true) {
#if 1
		target_freq = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_khz;
		target_volt = GPU_DVFS_PTPOD_DISABLE_VOLT;
		target_idx = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_idx;
		target_OPPidx = mt_gpufreq_ptpod_disable_idx;
		gpufreq_dbg("PTPOD disable dvfs, mt_gpufreq_ptpod_disable_idx = %d\n",
				mt_gpufreq_ptpod_disable_idx);
#else
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_dbg("PTPOD disable dvfs, return\n");
		return 0;
#endif
	}

	/************************************************
	 * target frequency == current frequency, skip it
	 *************************************************/
	if (g_cur_gpu_freq == target_freq && g_cur_gpu_volt == target_volt) {
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_dbg("GPU frequency from %d KHz to %d KHz (skipped) due to same frequency\n",
				g_cur_gpu_freq, target_freq);
		return 0;
	}

	gpufreq_dbg("GPU current frequency %d KHz, target frequency %d KHz\n", g_cur_gpu_freq,
			target_freq);

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() | (1 << GPU_DVFS_IS_DOING_DVFS));
	aee_rr_rec_gpu_dvfs_oppidx(target_OPPidx);
#endif

	/******************************
	 * set to the target frequency
	 *******************************/
	mt_gpufreq_set(g_cur_gpu_freq, target_freq, g_cur_gpu_volt, target_volt);

	g_cur_gpu_idx = target_idx;
	g_cur_gpu_OPPidx = target_OPPidx;

#ifdef MT_GPUFREQ_AEE_RR_REC
	aee_rr_rec_gpu_dvfs_status(aee_rr_curr_gpu_dvfs_status() & ~(1 << GPU_DVFS_IS_DOING_DVFS));
#endif

	mutex_unlock(&mt_gpufreq_lock);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_target);


/********************************************
 *	   POWER LIMIT RELATED
 ********************************************/
enum {
	IDX_THERMAL_LIMITED,
	IDX_LOW_BATT_VOLT_LIMITED,
	IDX_LOW_BATT_VOLUME_LIMITED,
	IDX_OC_LIMITED,

	NR_IDX_POWER_LIMITED,
};

/* NO need to throttle when OC */
#ifdef MT_GPUFREQ_OC_PROTECT
static unsigned int mt_gpufreq_oc_level;

#define MT_GPUFREQ_OC_LIMIT_FREQ_1		GPUFREQ_LAST_FREQ_LEVEL		/* < 485 MHz */
#define MT_GPUFREQ_OC_LIMIT_FREQ_1_6739TW	GPUFREQ_LAST_FREQ_LEVEL_6739TW	/* 530 MHz */
static unsigned int mt_gpufreq_oc_limited_index_0;	/* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_oc_limited_index_1;
static unsigned int mt_gpufreq_oc_limited_index;	/* Limited frequency index for oc */
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static unsigned int mt_gpufreq_low_battery_volume;

#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1	 GPU_DVFS_FREQ0
static unsigned int mt_gpufreq_low_bat_volume_limited_index_0;	/* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_low_bat_volume_limited_index_1;
static unsigned int mt_gpufreq_low_batt_volume_limited_index;	/* Limited frequency index for low battery volume */
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static unsigned int mt_gpufreq_low_battery_level;

#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1		GPU_DVFS_FREQ0	/* no need to throttle when LV1 */
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2		GPUFREQ_LAST_FREQ_LEVEL	/* 485 MHz */
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2_6739TW	GPUFREQ_LAST_FREQ_LEVEL_6739TW	/* 530 MHz */
static unsigned int mt_gpufreq_low_bat_volt_limited_index_0;	/* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_low_bat_volt_limited_index_1;
static unsigned int mt_gpufreq_low_bat_volt_limited_index_2;
static unsigned int mt_gpufreq_low_batt_volt_limited_index;	/* Limited frequency index for low battery voltage */
#endif

static unsigned int mt_gpufreq_thermal_limited_gpu_power;	/* thermal limit power */
static unsigned int mt_gpufreq_prev_thermal_limited_freq;	/* thermal limited freq */
/* limit frequency index array */
static unsigned int mt_gpufreq_power_limited_index_array[NR_IDX_POWER_LIMITED] = { 0 };
bool mt_gpufreq_power_limited_ignore[NR_IDX_POWER_LIMITED] = { false };

int mt_gpufreq_get_cur_ceiling_idx(void)
{
	int limited_id = 0;
	int i;

	for (i = 0; i < NR_IDX_POWER_LIMITED; i++) {
		limited_id = mt_gpufreq_power_limited_index_array[i];

		if (limited_id > g_limited_max_id) {
			if (mt_gpufreq_power_limited_ignore[i] == false)
				g_limited_max_id = limited_id;
		}
	}

	limited_id = g_limited_max_id;
#ifndef DISABLE_PBM_FEATURE
	if (mt_gpufreq_pbm_limited_index > limited_id)
		limited_id = mt_gpufreq_pbm_limited_index;
#endif
	return limited_id;
}

/************************************************
 * frequency adjust interface for thermal protect
 *************************************************/
/******************************************************
 * parameter: target power
 *******************************************************/
static int mt_gpufreq_power_throttle_protect(void)
{
	int ret = 0;
	int i = 0;
	unsigned int limited_index = 0;

	/* Check lowest frequency in all limitation */
	for (i = 0; i < NR_IDX_POWER_LIMITED; i++) {
		if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index == 0)
			limited_index = mt_gpufreq_power_limited_index_array[i];
		else if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index != 0) {
			if (mt_gpufreq_power_limited_index_array[i] > limited_index)
				limited_index = mt_gpufreq_power_limited_index_array[i];
		}
	}

	g_limited_max_id = limited_index;

	if (g_pGpufreq_power_limit_notify != NULL)
		g_pGpufreq_power_limit_notify(g_limited_max_id);

	return ret;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/************************************************
 * GPU frequency adjust interface for oc protect
 *************************************************/
static void mt_gpufreq_oc_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);

	mt_gpufreq_power_limited_index_array[IDX_OC_LIMITED] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_oc_callback(BATTERY_OC_LEVEL oc_level)
{
	gpufreq_dbg("@%s: oc_level = %d\n", __func__, oc_level);

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	if (g_limited_oc_ignore_state == true) {
		gpufreq_info("@%s: g_limited_oc_ignore_state == true!\n", __func__);
		return;
	}

	mt_gpufreq_oc_level = oc_level;

	/* BATTERY_OC_LEVEL_1: >= 5.5A	*/
	if (oc_level == BATTERY_OC_LEVEL_1) {
		if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_1) {
			mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_1;
			mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_1);	/* Limit GPU 396.5Mhz */
		}
	}
	/* unlimit gpu */
	else {
		if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_0) {
			mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_0;
			mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_0);	/* Unlimit */
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/************************************************
 * GPU frequency adjust interface for low bat_volume protect
 *************************************************/
static void mt_gpufreq_low_batt_volume_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);

	mt_gpufreq_power_limited_index_array[IDX_LOW_BATT_VOLUME_LIMITED] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_low_batt_volume_callback(BATTERY_PERCENT_LEVEL low_battery_volume)
{
	gpufreq_dbg("@%s: low_battery_volume = %d\n", __func__, low_battery_volume);

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	if (g_limited_low_batt_volume_ignore_state == true) {
		gpufreq_info("@%s: g_limited_low_batt_volume_ignore_state == true!\n", __func__);
		return;
	}

	mt_gpufreq_low_battery_volume = low_battery_volume;

	/* LOW_BATTERY_VOLUME_1: <= 15%, LOW_BATTERY_VOLUME_0: >15% */
	if (low_battery_volume == BATTERY_PERCENT_LEVEL_1) {
		if (mt_gpufreq_low_batt_volume_limited_index !=
			mt_gpufreq_low_bat_volume_limited_index_1) {
			mt_gpufreq_low_batt_volume_limited_index =
				mt_gpufreq_low_bat_volume_limited_index_1;

			/* Unlimited */
			mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_1);
		}
	}
	/* unlimit gpu */
	else {
		if (mt_gpufreq_low_batt_volume_limited_index !=
			mt_gpufreq_low_bat_volume_limited_index_0) {
			mt_gpufreq_low_batt_volume_limited_index =
				mt_gpufreq_low_bat_volume_limited_index_0;
			mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_0);	/* Unlimit */
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/************************************************
 * GPU frequency adjust interface for low bat_volt protect
 *************************************************/
static void mt_gpufreq_low_batt_volt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);
	mt_gpufreq_power_limited_index_array[IDX_LOW_BATT_VOLT_LIMITED] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

/******************************************************
 * parameter: low_battery_level
 *******************************************************/
void mt_gpufreq_low_batt_volt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	gpufreq_dbg("@%s: low_battery_level = %d\n", __func__, low_battery_level);

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		return;
	}

	if (g_limited_low_batt_volt_ignore_state == true) {
		gpufreq_info("@%s: g_limited_low_batt_volt_ignore_state == true!\n", __func__);
		return;
	}

	mt_gpufreq_low_battery_level = low_battery_level;

	/* is_low_battery=1:need limit HW, is_low_battery=0:no limit */
	/* 3.25V HW issue int and is_low_battery=1,
	 * 3.0V HW issue int and is_low_battery=2,
	 * 3.5V HW issue int and is_low_battery=0
	 */

	/* no need to throttle when LV1 */
#if 0
	if (low_battery_level == LOW_BATTERY_LEVEL_1) {
		if (mt_gpufreq_low_batt_volt_limited_index !=
			mt_gpufreq_low_bat_volt_limited_index_1) {
			mt_gpufreq_low_batt_volt_limited_index =
				mt_gpufreq_low_bat_volt_limited_index_1;
			/* Limit GPU 416Mhz */
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_1);
		}
	} else
#endif
	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		if (mt_gpufreq_low_batt_volt_limited_index !=
			mt_gpufreq_low_bat_volt_limited_index_2) {
			mt_gpufreq_low_batt_volt_limited_index =
				mt_gpufreq_low_bat_volt_limited_index_2;
			/* Limit GPU 400Mhz */
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_2);
		}
	} else {		/* unlimit gpu */
		if (mt_gpufreq_low_batt_volt_limited_index !=
			mt_gpufreq_low_bat_volt_limited_index_0) {
			mt_gpufreq_low_batt_volt_limited_index =
				mt_gpufreq_low_bat_volt_limited_index_0;
			/* Unlimit */
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_0);
		}
	}
}
#endif

/************************************************
 * frequency adjust interface for thermal protect
 *************************************************/
/******************************************************
 * parameter: target power
 *******************************************************/
static unsigned int _mt_gpufreq_get_limited_freq(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;
	unsigned int found = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs_power[i].gpufreq_power <= limited_power) {
			limited_freq = mt_gpufreqs_power[i].gpufreq_khz;
			found = 1;
			break;
		}
	}

	/* not found */
	if (!found)
		limited_freq = mt_gpufreqs_power[mt_gpufreqs_num - 1].gpufreq_khz;

	gpufreq_dbg("@%s: limited_freq = %d\n", __func__, limited_freq);

	return limited_freq;
}

void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;

	mutex_lock(&mt_gpufreq_power_lock);

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (mt_gpufreqs_num == 0) {
		gpufreq_pr_warn("@%s: mt_gpufreqs_num == 0!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (g_limited_thermal_ignore_state == true) {
		gpufreq_info("@%s: g_limited_thermal_ignore_state == true!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	mt_gpufreq_thermal_limited_gpu_power = limited_power;

#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
	mt_update_gpufreqs_power_table();
#endif

	if (limited_power == 0)
		mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED] = 0;
	else {
		limited_freq = _mt_gpufreq_get_limited_freq(limited_power);

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
				mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED] = i;
				break;
			}
		}
	}

	if (mt_gpufreq_prev_thermal_limited_freq != limited_freq) {
		mt_gpufreq_prev_thermal_limited_freq = limited_freq;
		mt_gpufreq_power_throttle_protect();
		if (limited_freq < GPU_DVFS_FREQ0)
			gpufreq_info("@%s: p %u f %u i %u\n", __func__, limited_power, limited_freq,
				mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED]);
	}

	mutex_unlock(&mt_gpufreq_power_lock);
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);

/* for thermal to update power budget */
unsigned int mt_gpufreq_get_max_power(void)
{
	if (!mt_gpufreqs_power)
		return 0;
	else
		return mt_gpufreqs_power[0].gpufreq_power;
}

/* for thermal to update power budget */
unsigned int mt_gpufreq_get_min_power(void)
{
	if (!mt_gpufreqs_power)
		return 0;
	else
		return mt_gpufreqs_power[mt_gpufreqs_num - 1].gpufreq_power;
}

void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
#ifndef DISABLE_PBM_FEATURE
	int i = 0;
	unsigned int limited_freq = 0;

	mutex_lock(&mt_gpufreq_power_lock);

	if (mt_gpufreq_ready == false) {
		/*gpufreq_pr_warn("@%s: GPU DVFS not ready!\n", __func__);*/
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (mt_gpufreqs_num == 0) {
		gpufreq_pr_warn("@%s: mt_gpufreqs_num == 0!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (g_limited_pbm_ignore_state == true) {
		gpufreq_info("@%s: g_limited_pbm_ignorepbm_ignore_state == true!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == mt_gpufreq_pbm_limited_gpu_power) {
		gpufreq_dbg("@%s: limited_power(%d mW) not changed, skip it!\n",
				__func__, limited_power);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	mt_gpufreq_pbm_limited_gpu_power = limited_power;

	gpufreq_dbg("@%s: limited_power = %d\n", __func__, limited_power);

#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
	mt_update_gpufreqs_power_table();	/* TODO: need to check overhead? */
#endif

	if (limited_power == 0)
		mt_gpufreq_pbm_limited_index = 0;
	else {
		limited_freq = _mt_gpufreq_get_limited_freq(limited_power);

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
				mt_gpufreq_pbm_limited_index = i;
				break;
			}
		}
	}

	gpufreq_dbg("PBM limit frequency upper bound to id = %d\n", mt_gpufreq_pbm_limited_index);

	if (g_pGpufreq_power_limit_notify != NULL)
		g_pGpufreq_power_limit_notify(mt_gpufreq_pbm_limited_index);

	mutex_unlock(&mt_gpufreq_power_lock);
#endif
}

unsigned int mt_gpufreq_get_leakage_mw(void)
{
#ifndef DISABLE_PBM_FEATURE
	int temp = 0;
#ifdef STATIC_PWR_READY2USE
	unsigned int cur_vcore = _mt_gpufreq_get_cur_volt() / 100;
	int leak_power;
#endif

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

#ifdef STATIC_PWR_READY2USE
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, cur_vcore, temp);
	if (mt_gpufreq_volt_enable_state && leak_power > 0)
		return leak_power;
	else
		return 0;
#else
	return 130;
#endif

#else /* DISABLE_PBM_FEATURE */
	return 0;
#endif
}

/************************************************
 * return current GPU thermal limit index
 *************************************************/
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	gpufreq_dbg("current GPU thermal limit index is %d\n", g_limited_max_id);
	return g_limited_max_id;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_index);

/************************************************
 * return current GPU thermal limit frequency
 *************************************************/
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	gpufreq_dbg("current GPU thermal limit freq is %d MHz\n",
			mt_gpufreqs[g_limited_max_id].gpufreq_khz / 1000);
	return mt_gpufreqs[g_limited_max_id].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/************************************************
 * return current GPU frequency index
 *************************************************/
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	gpufreq_dbg("current GPU frequency OPP index is %d\n", g_cur_gpu_OPPidx);
	return g_cur_gpu_OPPidx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/************************************************
 * return current GPU frequency
 *************************************************/
unsigned int mt_gpufreq_get_cur_freq(void)
{
	gpufreq_dbg("current GPU frequency is %d MHz\n", g_cur_gpu_freq / 1000);
	return g_cur_gpu_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/************************************************
 * return current GPU voltage
 *************************************************/
unsigned int mt_gpufreq_get_cur_volt(void)
{
#if 0
	return g_cur_gpu_volt;
#else
	return _mt_gpufreq_get_cur_volt();
#endif
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/************************************************
 * register / unregister GPU input boost notifiction CB
 *************************************************/
void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	g_pGpufreq_input_boost_notify = pCB;
#endif
}
EXPORT_SYMBOL(mt_gpufreq_input_boost_notify_registerCB);

/************************************************
 * register / unregister GPU power limit notifiction CB
 *************************************************/
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	g_pGpufreq_power_limit_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

/************************************************
 * register / unregister ptpod update GPU volt CB
 *************************************************/
void mt_gpufreq_update_volt_registerCB(gpufreq_ptpod_update_notify pCB)
{
	g_pGpufreq_ptpod_update_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_update_volt_registerCB);

/************************************************
 * register / unregister set GPU freq CB
 *************************************************/
void mt_gpufreq_setfreq_registerCB(sampler_func pCB)
{
	g_pFreqSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setfreq_registerCB);

/************************************************
 * register / unregister set GPU volt CB
 *************************************************/
void mt_gpufreq_setvolt_registerCB(sampler_func pCB)
{
	g_pVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setvolt_registerCB);

#ifdef CONFIG_HAS_EARLYSUSPEND
/*********************************
 * early suspend callback function
 **********************************/
void mt_gpufreq_early_suspend(struct early_suspend *h)
{
	/* mt_gpufreq_state_set(0); */

}

/*******************************
 * late resume callback function
 ********************************/
void mt_gpufreq_late_resume(struct early_suspend *h)
{
	/* mt_gpufreq_check_freq_and_set_pll(); */

	/* mt_gpufreq_state_set(1); */
}
#endif

static int mt_gpufreq_pm_restore_early(struct device *dev)
{
	int i = 0;
	int found = 0;

	g_cur_gpu_freq = _mt_gpufreq_get_cur_freq();

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (g_cur_gpu_freq == mt_gpufreqs[i].gpufreq_khz) {
			g_cur_gpu_idx = mt_gpufreqs[i].gpufreq_idx;
			g_cur_gpu_volt = mt_gpufreqs[i].gpufreq_volt;
			g_cur_gpu_OPPidx = i;
			found = 1;
			gpufreq_dbg("match g_cur_gpu_OPPidx: %d\n", g_cur_gpu_OPPidx);
			break;
		}
	}

	if (found == 0) {
		g_cur_gpu_idx = mt_gpufreqs[0].gpufreq_idx;
		g_cur_gpu_volt = mt_gpufreqs[0].gpufreq_volt;
		g_cur_gpu_OPPidx = 0;
		gpufreq_pr_err("gpu freq not found, set parameter to max freq\n");
	}

	gpufreq_dbg("GPU freq SW/HW: %d/%d\n", g_cur_gpu_freq, _mt_gpufreq_get_cur_freq());
	gpufreq_dbg("g_cur_gpu_OPPidx: %d\n", g_cur_gpu_OPPidx);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_gpufreq_of_match[] = {
	{.compatible = "mediatek,mt6739-gpufreq",},
	{ /* sentinel */ },
};
#endif
MODULE_DEVICE_TABLE(of, mt_gpufreq_of_match);

void mt_gpufreq_set_loading(unsigned int gpu_loading)
{
}

#define TURBO_EFUSE (29)
#define SEG_EFUSE (30)
static int mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *apmixed_node;
	int i = 0;
	int mt_gpufreq_low_bat_volt_limit_freq_2;
	int mt_gpufreq_oc_limit_freq_1;
#ifdef MT_GPUFREQ_INPUT_BOOST
	int rc;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
#endif

#if defined(CONFIG_OF)
	struct device_node *node;

	node = of_find_matching_node(NULL, mt_gpufreq_of_match);
	if (!node)
		gpufreq_pr_err("@%s: find GPU node failed\n", __func__);

	mt_gpufreq_clk = kzalloc(sizeof(struct mt_gpufreq_clk_t), GFP_KERNEL);
	if (mt_gpufreq_clk == NULL)
		return -ENOMEM;

	/* MT6739 segment code[5]{1b0} + FAB turbo[21:20]{0x3} */
	if (((get_devinfo_with_index(TURBO_EFUSE) & EFUSE_FAB_INFO_TURBO_MASK) == EFUSE_FAB_INFO_TURBO_MASK) &&
		(get_devinfo_with_index(SEG_EFUSE) == 0x80 || get_devinfo_with_index(SEG_EFUSE) == 0x88 ||
		get_devinfo_with_index(SEG_EFUSE) == 0x90 || get_devinfo_with_index(SEG_EFUSE) == 0x08 ||
		get_devinfo_with_index(SEG_EFUSE) == 0x00 || get_devinfo_with_index(SEG_EFUSE) == 0x40 ||
		get_devinfo_with_index(SEG_EFUSE) == 0x48 || get_devinfo_with_index(SEG_EFUSE) == 0xC8 ||
		get_devinfo_with_index(SEG_EFUSE) == 0xC0 || get_devinfo_with_index(SEG_EFUSE) == 0xD0)) {
		gpufreq_info("@%s: I am 6739TW seg_code(%x), fab_turbo(%x)\n", __func__,
			get_devinfo_with_index(SEG_EFUSE), get_devinfo_with_index(TURBO_EFUSE));
		device_id = ID_MT6739TW;
	} else if ((get_devinfo_with_index(SEG_EFUSE) == 0xB8 || get_devinfo_with_index(SEG_EFUSE) == 0x38)) {
		gpufreq_info("@%s: I am 6739WD(6731) seg_code(%x), fab_turbo(%x)\n", __func__,
			get_devinfo_with_index(SEG_EFUSE), get_devinfo_with_index(TURBO_EFUSE));
		device_id = ID_MT6739WD;
	} else {
		gpufreq_info("@%s: I am 6739 seg_code(%x), fab_turbo(%x)\n", __func__,
			get_devinfo_with_index(SEG_EFUSE), get_devinfo_with_index(TURBO_EFUSE));
		device_id = ID_MT6739;
	}

	/* alloc PMIC regulator */
	{
		mt_gpufreq_pmic = kzalloc(sizeof(struct mt_gpufreq_pmic_t), GFP_KERNEL);
		mt_gpufreq_pmic->reg_vcore = regulator_get(&pdev->dev, "vcore");
		if (IS_ERR(mt_gpufreq_pmic->reg_vcore)) {
			dev_err(&pdev->dev, "cannot get vcore\n");
			return PTR_ERR(mt_gpufreq_pmic->reg_vcore);
		}
	}

#endif

	mt_gpufreq_dvfs_table_type = device_id;

#ifdef CONFIG_HAS_EARLYSUSPEND
	mt_gpufreq_early_suspend_handler.suspend = mt_gpufreq_early_suspend;
	mt_gpufreq_early_suspend_handler.resume = mt_gpufreq_late_resume;
	register_early_suspend(&mt_gpufreq_early_suspend_handler);
#endif


	/**********************
	 * Initial leackage power usage
	 ***********************/
#ifdef STATIC_PWR_READY2USE
	mt_spower_init();
#endif

	/**********************
	 * Initial SRAM debugging ptr
	 ***********************/
#ifdef MT_GPUFREQ_AEE_RR_REC
	_mt_gpufreq_aee_init();
#endif

	/**********************
	 * setup gpufreq table
	 ***********************/
	gpufreq_info("setup gpufreqs table\n");

	if (mt_gpufreq_dvfs_table_type == ID_MT6739)
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_mt6739,
					ARRAY_SIZE(mt_gpufreq_opp_tbl_mt6739));
	else if (mt_gpufreq_dvfs_table_type == ID_MT6739TW)
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_mt6739tw,
					ARRAY_SIZE(mt_gpufreq_opp_tbl_mt6739tw));
	else if (mt_gpufreq_dvfs_table_type == ID_MT6739WD)
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_mt6739wd,
					ARRAY_SIZE(mt_gpufreq_opp_tbl_mt6739wd));
	else
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_mt6739,
					ARRAY_SIZE(mt_gpufreq_opp_tbl_mt6739));
	gpufreq_dbg("[Figo] OPP Table Set Up Done (%d)", mt_gpufreq_dvfs_table_type);

	/**********************
	 * setup PMIC init value
	 ***********************/
	gpufreq_dbg("[Figo] Skip regulator init");
	mt_gpufreq_volt_enable_state = 1;
	gpufreq_dbg("[Figo] power init done");

	/* init PLL
	 *	to get PLL register
	 */

	/* Init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		gpufreq_pr_err("Error, APMIXED iomap failed");
		return -ENOENT;
	}
	g_cur_freq_init_keep = g_cur_gpu_freq;

	/**********************
	 * setup initial frequency
	 ***********************/
	gpufreq_dbg("[Figo] setup initial frequency");
	mt_gpufreq_set_initial();

	gpufreq_info("GPU current frequency = %dKHz\n", _mt_gpufreq_get_cur_freq());
	gpufreq_info("Current Vproc = %dmV\n", _mt_gpufreq_get_cur_volt() / 100);
	gpufreq_info("g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", g_cur_gpu_freq, g_cur_gpu_volt);
	gpufreq_info("g_cur_gpu_idx = %d, g_cur_gpu_OPPidx = %d\n", g_cur_gpu_idx,
			 g_cur_gpu_OPPidx);

	mt_gpufreq_ready = true;


#ifdef MT_GPUFREQ_INPUT_BOOST
	mt_gpufreq_up_task =
		kthread_create(mt_gpufreq_input_boost_task, NULL, "mt_gpufreq_input_boost_task");
	if (IS_ERR(mt_gpufreq_up_task))
		return PTR_ERR(mt_gpufreq_up_task);

	sched_setscheduler_nocheck(mt_gpufreq_up_task, SCHED_FIFO, &param);
	get_task_struct(mt_gpufreq_up_task);

	rc = input_register_handler(&mt_gpufreq_input_handler);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1) {
			mt_gpufreq_low_bat_volt_limited_index_1 = i;
			break;
		}
	}

    /* limit to the lowest frequency */
	mt_gpufreq_low_bat_volt_limit_freq_2 = GPUFREQ_LAST_FREQ_LEVEL;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == mt_gpufreq_low_bat_volt_limit_freq_2) {
			mt_gpufreq_low_bat_volt_limited_index_2 = i;
			break;
		}
	}

	register_low_battery_notify(&mt_gpufreq_low_batt_volt_callback, LOW_BATTERY_PRIO_GPU);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1) {
			mt_gpufreq_low_bat_volume_limited_index_1 = i;
			break;
		}
	}

	register_battery_percent_notify(&mt_gpufreq_low_batt_volume_callback,
					BATTERY_PERCENT_PRIO_GPU);
#endif

#ifdef MT_GPUFREQ_OC_PROTECT
	mt_gpufreq_oc_limit_freq_1 = GPUFREQ_LAST_FREQ_LEVEL;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == mt_gpufreq_oc_limit_freq_1) {
			mt_gpufreq_oc_limited_index_1 = i;
			break;
		}
	}

	register_battery_oc_notify(&mt_gpufreq_oc_callback, BATTERY_OC_PRIO_GPU);
#endif

#ifndef DISABLE_PBM_FEATURE
	INIT_DEFERRABLE_WORK(&notify_pbm_gpuoff_work, mt_gpufreq_notify_pbm_gpuoff);
#endif

	return 0;
}

/***************************************
 * this function should never be called
 ****************************************/
static int mt_gpufreq_pdrv_remove(struct platform_device *pdev)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	input_unregister_handler(&mt_gpufreq_input_handler);

	kthread_stop(mt_gpufreq_up_task);
	put_task_struct(mt_gpufreq_up_task);
#endif

	return 0;
}

static const struct dev_pm_ops mt_gpufreq_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.restore_early = mt_gpufreq_pm_restore_early,
};
#if 0
struct platform_device mt_gpufreq_pdev = {
	.name = "mt-gpufreq",
	.id = -1,
};
#endif
static struct platform_driver mt_gpufreq_pdrv = {
	.probe = mt_gpufreq_pdrv_probe,
	.remove = mt_gpufreq_pdrv_remove,
	.driver = {
		   .name = "gpufreq",
		   .pm = &mt_gpufreq_pm_ops,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_gpufreq_of_match,
#endif
		   },
};


#ifdef CONFIG_PROC_FS
/* #if 0 */
/*
 * PROC
 */

/***************************
 * show current debug status
 ****************************/
static int mt_gpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_debug)
		seq_puts(m, "gpufreq debug enabled\n");
	else
		seq_puts(m, "gpufreq debug disabled\n");

	return 0;
}

/***********************
 * enable debug message
 ************************/
static ssize_t mt_gpufreq_debug_proc_write(struct file *file, const char __user *buffer,
					   size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &debug) == 0) {
		if (debug == 0)
			mt_gpufreq_debug = 0;
		else if (debug == 1)
			mt_gpufreq_debug = 1;
		else
			gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/****************************
 * show current limited by low batt volume
 *****************************/
static int mt_gpufreq_limited_oc_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_oc_ignore_state = %d\n", g_limited_max_id,
		   g_limited_oc_ignore_state);
	return 0;
}

/**********************************
 * limited for low batt volume protect
 ***********************************/
static ssize_t mt_gpufreq_limited_oc_ignore_proc_write(struct file *file,
							   const char __user *buffer, size_t count,
							   loff_t *data)
{
	char desc[32];
	unsigned int len = 0;
	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &ignore) == 0) {
		if (ignore == 1)
			g_limited_oc_ignore_state = true;
		else if (ignore == 0)
			g_limited_oc_ignore_state = false;
		else
			gpufreq_pr_warn
				("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");

	mt_gpufreq_power_limited_ignore[IDX_OC_LIMITED] = g_limited_oc_ignore_state;
	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/****************************
 * show current limited by low batt volume
 *****************************/
static int mt_gpufreq_limited_low_batt_volume_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volume_ignore_state = %d\n",
		   g_limited_max_id, g_limited_low_batt_volume_ignore_state);
	return 0;
}

/**********************************
 * limited for low batt volume protect
 ***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volume_ignore_proc_write(struct file *file,
									const char __user *buffer,
									size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &ignore) == 0) {
		if (ignore == 1)
			g_limited_low_batt_volume_ignore_state = true;
		else if (ignore == 0)
			g_limited_low_batt_volume_ignore_state = false;
		else
			gpufreq_pr_warn
				("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");

	mt_gpufreq_power_limited_ignore[IDX_LOW_BATT_VOLUME_LIMITED] = g_limited_low_batt_volume_ignore_state;
	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/****************************
 * show current limited by low batt volt
 *****************************/
static int mt_gpufreq_limited_low_batt_volt_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volt_ignore_state = %d\n",
		   g_limited_max_id, g_limited_low_batt_volt_ignore_state);

	return 0;
}

/**********************************
 * limited for low batt volt protect
 ***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volt_ignore_proc_write(struct file *file,
								  const char __user *buffer,
								  size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &ignore) == 0) {
		if (ignore == 1)
			g_limited_low_batt_volt_ignore_state = true;
		else if (ignore == 0)
			g_limited_low_batt_volt_ignore_state = false;
		else
			gpufreq_pr_warn
				("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");

	mt_gpufreq_power_limited_ignore[IDX_LOW_BATT_VOLT_LIMITED] = g_limited_low_batt_volt_ignore_state;
	return count;
}
#endif

/****************************
 * show current limited by thermal
 *****************************/
static int mt_gpufreq_limited_thermal_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_thermal_ignore_state = %d\n",
		   g_limited_max_id, g_limited_thermal_ignore_state);

	return 0;
}

/**********************************
 * limited for thermal protect
 ***********************************/
static ssize_t mt_gpufreq_limited_thermal_ignore_proc_write(struct file *file,
								const char __user *buffer,
								size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &ignore) == 0) {
		if (ignore == 1)
			g_limited_thermal_ignore_state = true;
		else if (ignore == 0)
			g_limited_thermal_ignore_state = false;
		else
			gpufreq_pr_warn
				("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");

	mt_gpufreq_power_limited_ignore[IDX_THERMAL_LIMITED] = g_limited_thermal_ignore_state;
	return count;
}

#ifndef DISABLE_PBM_FEATURE
/****************************
 * show current limited by PBM
 *****************************/
static int mt_gpufreq_limited_pbm_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "mt_gpufreq_pbm_limited_index = %d, g_limited_pbm_ignore_state = %d\n",
		mt_gpufreq_pbm_limited_index, g_limited_pbm_ignore_state);

	return 0;
}

/**********************************
 * limited for low batt volume protect
 ***********************************/
static ssize_t mt_gpufreq_limited_pbm_ignore_proc_write(struct file *file,
							const char __user *buffer, size_t count,
							loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &ignore) == 0) {
		if (ignore == 1) {
			g_limited_pbm_ignore_state = true;
		} else if (ignore == 0)
			g_limited_pbm_ignore_state = false;
		else
			gpufreq_pr_warn
				("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: not ignore, 1: ignore]\n");

	return count;
}
#endif

/****************************
 * show current limited power
 *****************************/
static int mt_gpufreq_limited_power_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, limit frequency = %d\n",
		   g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);
	return 0;
}

/**********************************
 * limited power for thermal protect
 ***********************************/
static ssize_t mt_gpufreq_limited_power_proc_write(struct file *file,
						   const char __user *buffer,
						   size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &power) == 0)
		mt_gpufreq_thermal_protect(power);
	else
		gpufreq_pr_warn("bad argument!! please provide the maximum limited power\n");

	return count;
}

/****************************
 * show current limited power by PBM
 *****************************/
#ifndef DISABLE_PBM_FEATURE
static int mt_gpufreq_limited_by_pbm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pbm_limited_power = %d, limit index = %d\n",
		   mt_gpufreq_pbm_limited_gpu_power, mt_gpufreq_pbm_limited_index);

	return 0;
}

/**********************************
 * limited power for thermal protect
 ***********************************/
static ssize_t mt_gpufreq_limited_by_pbm_proc_write(struct file *file, const char __user *buffer,
							size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	unsigned int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtouint(desc, 0, &power) == 0)
		mt_gpufreq_set_power_limit_by_pbm(power);
	else
		gpufreq_pr_warn("bad argument!! please provide the maximum limited power\n");

	return count;
}
#endif

/******************************
 * show current GPU DVFS stauts
 *******************************/
static int mt_gpufreq_state_proc_show(struct seq_file *m, void *v)
{
	if (!mt_gpufreq_pause)
		seq_puts(m, "GPU DVFS enabled\n");
	else
		seq_puts(m, "GPU DVFS disabled\n");

	return 0;
}

/****************************************
 * set GPU DVFS stauts by sysfs interface
 *****************************************/
static ssize_t mt_gpufreq_state_proc_write(struct file *file,
					   const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int enabled = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &enabled) == 0) {
		if (enabled == 1) {
			mt_gpufreq_keep_max_frequency_state = false;
			mt_gpufreq_state_set(1);
		} else if (enabled == 0) {
			/* Keep MAX frequency when GPU DVFS disabled. */
			mt_gpufreq_keep_max_frequency_state = true;
#ifdef VOLT_SET_READY
			gpufreq_dbg("[Figo] Power on by mt_gpufreq_state_proc_write\n");
			mt_gpufreq_voltage_enable_set(1);
#endif
			mt_gpufreq_target(g_gpufreq_max_id);
			mt_gpufreq_state_set(0);
		} else
			gpufreq_pr_warn("bad argument!! argument should be \"1\" or \"0\"\n");
	} else
		gpufreq_pr_warn("bad argument!! argument should be \"1\" or \"0\"\n");

	return count;
}

/********************
 * show GPU OPP table
 *********************/
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		seq_printf(m, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
		seq_printf(m, "idx = %d\n", mt_gpufreqs[i].gpufreq_idx);
	}

	return 0;
}

/********************
 * show GPU power table
 *********************/
static int mt_gpufreq_power_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_khz = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_khz);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_volt = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_volt);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_power = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_power);
	}
	return 0;
}

/***************************
 * show current specific frequency status
 ****************************/
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_keep_opp_frequency_state) {
		seq_puts(m, "gpufreq keep opp frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq keep opp frequency disabled\n");
	return 0;
}

/***********************
 * enable specific frequency
 ************************/
static ssize_t mt_gpufreq_opp_freq_proc_write(struct file *file, const char __user *buffer,
						  size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int fixed_freq = 0;
	int i = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';
	if (kstrtoint(desc, 0, &fixed_freq) == 0) {
		if (fixed_freq == 0) {
			mt_gpufreq_keep_opp_frequency_state = false;
		} else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (fixed_freq == mt_gpufreqs[i].gpufreq_khz) {
					mt_gpufreq_keep_opp_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1) {
				mt_gpufreq_keep_opp_frequency_state = true;
				mt_gpufreq_keep_opp_frequency = fixed_freq;

#ifdef VOLT_SET_READY
			gpufreq_dbg("[Figo] Power on by mt_gpufreq_opp_freq_proc_write\n");
			mt_gpufreq_voltage_enable_set(1);
#endif
			mt_gpufreq_target(mt_gpufreq_keep_opp_index);
			}

		}
	} else
	gpufreq_pr_warn("bad argument!! please provide the fixed frequency\n");

	return count;
}

/***************************
 * show current specific frequency status
 ****************************/
static int mt_gpufreq_opp_max_freq_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_opp_max_frequency_state) {
		seq_puts(m, "gpufreq opp max frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq opp max frequency disabled\n");

	return 0;
}

/***********************
 * enable specific frequency
 ************************/
static ssize_t mt_gpufreq_opp_max_freq_proc_write(struct file *file, const char __user *buffer,
						  size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int max_freq = 0;
	int i = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &max_freq) == 0) {
		if (max_freq == 0) {
			mt_gpufreq_opp_max_frequency_state = false;
		} else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (mt_gpufreqs[i].gpufreq_khz <= max_freq) {
					mt_gpufreq_opp_max_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1) {
				mt_gpufreq_opp_max_frequency_state = true;
				mt_gpufreq_opp_max_frequency =
					mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;
#ifdef VOLT_SET_READY
				gpufreq_dbg("[Figo] Power on by mt_gpufreq_opp_max_freq_proc_write\n");
				mt_gpufreq_voltage_enable_set(1);
#endif
				mt_gpufreq_target(mt_gpufreq_opp_max_index);
			}
		}
	} else
	gpufreq_pr_warn("bad argument!! please provide the maximum limited frequency\n");

	return count;
}

/********************
 * show variable dump
 *********************/
static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	seq_puts(m, "DVFS_GPU [legacy]\n");

	seq_printf(m, "g_cur_gpu_freq = %d, g_cur_gpu_volt = %d\n", mt_gpufreq_get_cur_freq(),
		   mt_gpufreq_get_cur_volt());
	seq_printf(m, "g_cur_gpu_idx = %d, g_cur_gpu_OPPidx = %d\n", g_cur_gpu_idx,
		   g_cur_gpu_OPPidx);
	seq_printf(m, "g_limited_max_id = %d\n", g_limited_max_id);

	for (i = 0; i < NR_IDX_POWER_LIMITED; i++)
		seq_printf(m, "mt_gpufreq_power_limited_index_array[%d] = %d\n", i,
			   mt_gpufreq_power_limited_index_array[i]);

	seq_printf(m, "_mt_gpufreq_get_cur_freq = %d, mt_get_ckgen_freq = %d KHz\n",
		_mt_gpufreq_get_cur_freq(), mt_get_ckgen_freq(7));
	seq_printf(m, "mt_gpufreq_volt_enable_state = %d\n", mt_gpufreq_volt_enable_state);
	seq_printf(m, "mt_gpufreq_dvfs_table_type = %d\n", mt_gpufreq_dvfs_table_type);
	seq_printf(m, "mt_gpufreq_ptpod_disable_idx = %d\n", mt_gpufreq_ptpod_disable_idx);

	return 0;
}

/***************************
 * show current voltage enable status
 ****************************/
static int mt_gpufreq_volt_enable_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_volt_enable)
		seq_puts(m, "gpufreq voltage enabled\n");
	else
		seq_puts(m, "gpufreq voltage disabled\n");

	return 0;
}

/***********************
 * enable specific frequency
 ************************/
static ssize_t mt_gpufreq_volt_enable_proc_write(struct file *file, const char __user *buffer,
						 size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &enable) == 0) {
		if (enable == 0) {
#ifdef VOLT_SET_READY
			gpufreq_dbg("[Figo] Power off by mt_gpufreq_state_proc_write\n");
			mt_gpufreq_voltage_enable_set(0);
#endif
			mt_gpufreq_volt_enable = false;
		} else if (enable == 1) {
#ifdef VOLT_SET_READY
			gpufreq_dbg("[Figo] Power on by mt_gpufreq_state_proc_write\n");
			mt_gpufreq_voltage_enable_set(1);
#endif
			mt_gpufreq_volt_enable = true;
		} else
			gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	return count;
}

/***************************
 * show current specific frequency status
 ****************************/
static int mt_gpufreq_fixed_freq_volt_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_fixed_freq_volt_state) {
		seq_puts(m, "gpufreq fixed frequency enabled\n");
		seq_printf(m, "fixed frequency = %d\n", mt_gpufreq_fixed_frequency);
		seq_printf(m, "fixed voltage = %d\n", mt_gpufreq_fixed_voltage);
	} else
		seq_puts(m, "gpufreq fixed frequency disabled\n");
	return 0;
}

/***********************
 * enable specific frequency
 ************************/
static void _mt_gpufreq_fixed_freq(int fixed_freq)
{
	unsigned int last_freq_level = 0;

	last_freq_level = GPUFREQ_LAST_FREQ_LEVEL;

	/* freq (KHz) */
	if ((fixed_freq >= GPUFREQ_LAST_FREQ_LEVEL)
		&& (fixed_freq <= GPU_DVFS_FREQ0)) {
		gpufreq_dbg("@ %s, mt_gpufreq_clock_switch1 fix frq = %d, fix volt = %d, volt = %d\n",
			__func__, mt_gpufreq_fixed_frequency, mt_gpufreq_fixed_voltage, g_cur_gpu_volt);
		mt_gpufreq_fixed_freq_volt_state = true;
		mt_gpufreq_fixed_frequency = fixed_freq;
		mt_gpufreq_fixed_voltage = g_cur_gpu_volt;
#ifdef VOLT_SET_READY
		gpufreq_dbg("[Figo] Power on by _mt_gpufreq_fixed_freq\n");
		mt_gpufreq_voltage_enable_set(1);
#endif
		gpufreq_dbg("@ %s, mt_gpufreq_clock_switch2 fix frq = %d, fix volt = %d, volt = %d\n",
			__func__, mt_gpufreq_fixed_frequency, mt_gpufreq_fixed_voltage, g_cur_gpu_volt);
		mt_gpufreq_clock_switch(mt_gpufreq_fixed_frequency);
		g_cur_gpu_freq = mt_gpufreq_fixed_frequency;
	}
}

static void _mt_gpufreq_fixed_volt(int fixed_volt)
{
	unsigned int max_volt = 0;
	unsigned int min_volt = 0;


	if (device_id == ID_MT6739) {
		max_volt = PMIC_MAX_VCORE;
		min_volt = PMIC_MIN_VCORE;
	} else if (device_id == ID_MT6739TW) {
		max_volt = PMIC_MAX_VCORE_6739TW;
		min_volt = PMIC_MIN_VCORE_6739TW;
	} else if (device_id == ID_MT6739WD) {
		max_volt = PMIC_MAX_VCORE_6739WD;
		min_volt = PMIC_MIN_VCORE_6739WD;
	} else {
		max_volt = PMIC_MAX_VCORE;
		min_volt = PMIC_MAX_VCORE;
	}

	/* volt (mV) */
#ifdef VPROC_SET_BY_PMIC
	if (fixed_volt >= (min_volt / 100) &&
		fixed_volt <= (max_volt / 100)) {
#endif
		gpufreq_dbg("@ %s, mt_gpufreq_volt_switch1 fix frq = %d, fix volt = %d, volt = %d\n",
			__func__, mt_gpufreq_fixed_frequency, mt_gpufreq_fixed_voltage, g_cur_gpu_volt);
		mt_gpufreq_fixed_freq_volt_state = true;
		mt_gpufreq_fixed_frequency = g_cur_gpu_freq;
		mt_gpufreq_fixed_voltage = fixed_volt * 100;
#ifdef VOLT_SET_READY
		gpufreq_dbg("[Figo] Power on by _mt_gpufreq_fixed_volt\n");
		mt_gpufreq_voltage_enable_set(1);
#endif
		gpufreq_dbg("@ %s, mt_gpufreq_volt_switch2 fix frq = %d, fix volt = %d, volt = %d\n",
			__func__, mt_gpufreq_fixed_frequency, mt_gpufreq_fixed_voltage, g_cur_gpu_volt);
		mt_gpufreq_volt_switch(g_cur_gpu_volt, mt_gpufreq_fixed_voltage);
		g_cur_gpu_volt = mt_gpufreq_fixed_voltage;
	}
}

static ssize_t mt_gpufreq_fixed_freq_volt_proc_write(struct file *file, const char __user *buffer,
							 size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;

	int fixed_freq = 0;
	int fixed_volt = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		if ((fixed_freq == 0) && (fixed_volt == 0)) {
			mt_gpufreq_fixed_freq_volt_state = false;
			mt_gpufreq_fixed_frequency = 0;
			mt_gpufreq_fixed_voltage = 0;
		} else {
			g_cur_gpu_freq = _mt_gpufreq_get_cur_freq();
			if (fixed_freq > g_cur_gpu_freq) {
				_mt_gpufreq_fixed_volt(fixed_volt);
				_mt_gpufreq_fixed_freq(fixed_freq);
			} else {
				_mt_gpufreq_fixed_freq(fixed_freq);
				_mt_gpufreq_fixed_volt(fixed_volt);
			}
		}
	} else
		gpufreq_pr_warn("bad argument!! should be [enable fixed_freq fixed_volt]\n");

	return count;
}

#ifdef MT_GPUFREQ_INPUT_BOOST
/*****************************
 * show current input boost status
 ******************************/
static int mt_gpufreq_input_boost_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_input_boost_state == 1)
		seq_puts(m, "gpufreq input boost is enabled\n");
	else
		seq_puts(m, "gpufreq input boost is disabled\n");

	return 0;
}

/***************************
 * enable/disable input boost
 ****************************/
static ssize_t mt_gpufreq_input_boost_proc_write(struct file *file, const char __user *buffer,
						 size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &debug) == 0) {
		if (debug == 0)
			mt_gpufreq_input_boost_state = 0;
		else if (debug == 1)
			mt_gpufreq_input_boost_state = 1;
		else
			gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}

/***************************
 * show lowpower frequency opp enable status
 ****************************/
static int mt_gpufreq_lpt_enable_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "not implemented\n");

	return 0;
}

/***********************
 * enable lowpower frequency opp
 ************************/
static ssize_t mt_gpufreq_lpt_enable_proc_write(struct file *file, const char __user *buffer,
						 size_t count, loff_t *data)
{

	gpufreq_pr_warn("not implemented\n");
#if 0
	char desc[32];
	unsigned int len = 0;

	int enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 0, &enable) == 0) {
		if (enable == 0)
			mt_gpufreq_low_power_test_enable = false;
		else if (enable == 1)
			mt_gpufreq_low_power_test_enable = true;
		else
			gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_pr_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
#endif

	return count;
}

#endif

#define PROC_FOPS_RW(name)							\
	static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {		\
	.owner		  = THIS_MODULE,				\
	.open		   = mt_ ## name ## _proc_open,	\
	.read		   = seq_read,					\
	.llseek		 = seq_lseek,					\
	.release		= single_release,				\
	.write		  = mt_ ## name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)							\
	static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {		\
	.owner		  = THIS_MODULE,				\
	.open		   = mt_ ## name ## _proc_open,	\
	.read		   = seq_read,					\
	.llseek		 = seq_lseek,					\
	.release		= single_release,				\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RW(gpufreq_debug);
PROC_FOPS_RW(gpufreq_limited_power);
#ifdef MT_GPUFREQ_OC_PROTECT
PROC_FOPS_RW(gpufreq_limited_oc_ignore);
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
PROC_FOPS_RW(gpufreq_limited_low_batt_volume_ignore);
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
PROC_FOPS_RW(gpufreq_limited_low_batt_volt_ignore);
#endif
PROC_FOPS_RW(gpufreq_limited_thermal_ignore);
#ifndef DISABLE_PBM_FEATURE
PROC_FOPS_RW(gpufreq_limited_pbm_ignore);
PROC_FOPS_RW(gpufreq_limited_by_pbm);
#endif
PROC_FOPS_RW(gpufreq_state);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RO(gpufreq_power_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RW(gpufreq_opp_max_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_volt_enable);
PROC_FOPS_RW(gpufreq_fixed_freq_volt);
#ifdef MT_GPUFREQ_INPUT_BOOST
PROC_FOPS_RW(gpufreq_input_boost);
#endif
PROC_FOPS_RW(gpufreq_lpt_enable);

static int mt_gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_debug),
		PROC_ENTRY(gpufreq_limited_power),
#ifdef MT_GPUFREQ_OC_PROTECT
		PROC_ENTRY(gpufreq_limited_oc_ignore),
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
		PROC_ENTRY(gpufreq_limited_low_batt_volume_ignore),
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
		PROC_ENTRY(gpufreq_limited_low_batt_volt_ignore),
#endif
		PROC_ENTRY(gpufreq_limited_thermal_ignore),
#ifndef DISABLE_PBM_FEATURE
		PROC_ENTRY(gpufreq_limited_pbm_ignore),
		PROC_ENTRY(gpufreq_limited_by_pbm),
#endif
		PROC_ENTRY(gpufreq_state),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_power_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_opp_max_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_volt_enable),
		PROC_ENTRY(gpufreq_fixed_freq_volt),
#ifdef MT_GPUFREQ_INPUT_BOOST
		PROC_ENTRY(gpufreq_input_boost),
#endif
		PROC_ENTRY(gpufreq_lpt_enable),
	};


	dir = proc_mkdir("gpufreq", NULL);

	if (!dir) {
		gpufreq_pr_err("fail to create /proc/gpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
			(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			gpufreq_pr_err("@%s: create /proc/gpufreq/%s failed\n", __func__,
					entries[i].name);
	}

	return 0;
}
#endif				/* CONFIG_PROC_FS */

/**********************************
 * mediatek gpufreq initialization
 ***********************************/
 #if 1
static int __init mt_gpufreq_init(void)
{
	int ret = 0;

#ifdef BRING_UP
	/* Skip driver init in bring up stage */
	return 0;
#endif

	gpufreq_info("@%s\n", __func__);

#ifdef CONFIG_PROC_FS

	/* init proc */
	if (mt_gpufreq_create_procfs())
		goto out;

#endif				/* CONFIG_PROC_FS */

	/* register platform device/driver */
#if !defined(CONFIG_OF)
	ret = platform_device_register(&mt_gpufreq_pdev);
	if (ret) {
		gpufreq_pr_err("fail to register gpufreq device @ %s()\n", __func__);
		goto out;
	}
#endif
	ret = platform_driver_register(&mt_gpufreq_pdrv);
	if (ret) {
		gpufreq_pr_err("fail to register gpufreq driver @ %s()\n", __func__);
#if !defined(CONFIG_OF)
		platform_device_unregister(&mt_gpufreq_pdev);
#endif
	}

out:
	return ret;
}
#endif
static void __exit mt_gpufreq_exit(void)
{
	platform_driver_unregister(&mt_gpufreq_pdrv);
#if !defined(CONFIG_OF)
	platform_device_unregister(&mt_gpufreq_pdev);
#endif
}

module_init(mt_gpufreq_init);
module_exit(mt_gpufreq_exit);

MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");


