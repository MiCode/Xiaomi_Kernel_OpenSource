// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */


#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <linux/uidgid.h>
#include <tmp_bts.h>
#include <linux/slab.h>
#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
#include <linux/iio/consumer.h>
#endif

/*=============================================================*/

static int mtkts_lcm_debug_log;

#define mtkts_lcm_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_lcm_debug_log) {                \
		pr_notice("[Thermal/TZ/BTS]" fmt, ##args); \
	}                                   \
} while (0)


#define mtkts_lcm_printk(fmt, args...) \
pr_debug("[Thermal/TZ/BTS]" fmt, ##args)


struct iio_channel *thermistor_ch4;

struct BTS_TEMPERATURE {
	__s32 BTS_Temp;
	__s32 TemperatureR;
};

static int g_RAP_pull_up_R = BTS_RAP_PULL_UP_R;
static int g_TAP_over_critical_low = BTS_TAP_OVER_CRITICAL_LOW;
static int g_RAP_pull_up_voltage = BTS_RAP_PULL_UP_VOLTAGE;
static int g_RAP_ntc_table = BTS_RAP_NTC_TABLE;
/* BTS_TEMPERATURE LCM_Temperature_Table[] = {0}; */

static struct BTS_TEMPERATURE *LCM_Temperature_Table;
static int ntc_tbl_size;

/* AP_NTC_BL197 */
static struct BTS_TEMPERATURE LCM_Temperature_Table1[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};

/* AP_NTC_TSM_1 */
static struct BTS_TEMPERATURE LCM_Temperature_Table2[] = {
	{-40, 70603},		/* FIX_ME */
	{-35, 70603},		/* FIX_ME */
	{-30, 70603},		/* FIX_ME */
	{-25, 70603},		/* FIX_ME */
	{-20, 70603},
	{-15, 55183},
	{-10, 43499},
	{-5, 34569},
	{0, 27680},
	{5, 22316},
	{10, 18104},
	{15, 14773},
	{20, 12122},
	{25, 10000},		/* 10K */
	{30, 8294},
	{35, 6915},
	{40, 5795},
	{45, 4882},
	{50, 4133},
	{55, 3516},
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004}		/* FIX_ME */
};

/* AP_NTC_10_SEN_1 */
static struct BTS_TEMPERATURE LCM_Temperature_Table3[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};


/* AP_NTC_10(TSM0A103F34D1RZ) */
static struct BTS_TEMPERATURE LCM_Temperature_Table4[] = {
	{-40, 188500},
	{-35, 144290},
	{-30, 111330},
	{-25, 86560},
	{-20, 67790},
	{-15, 53460},
	{-10, 42450},
	{-5, 33930},
	{0, 27280},
	{5, 22070},
	{10, 17960},
	{15, 14700},
	{20, 12090},
	{25, 10000},		/* 10K */
	{30, 8310},
	{35, 6940},
	{40, 5830},
	{45, 4910},
	{50, 4160},
	{55, 3540},
	{60, 3020},
	{65, 2590},
	{70, 2230},
	{75, 1920},
	{80, 1670},
	{85, 1450},
	{90, 1270},
	{95, 1110},
	{100, 975},
	{105, 860},
	{110, 760},
	{115, 674},
	{120, 599},
	{125, 534}
};

/* AP_NTC_47 */
static struct BTS_TEMPERATURE LCM_Temperature_Table5[] = {
	{-40, 483954},		/* FIX_ME */
	{-35, 483954},		/* FIX_ME */
	{-30, 483954},		/* FIX_ME */
	{-25, 483954},		/* FIX_ME */
	{-20, 483954},
	{-15, 360850},
	{-10, 271697},
	{-5, 206463},
	{0, 158214},
	{5, 122259},
	{10, 95227},
	{15, 74730},
	{20, 59065},
	{25, 47000},		/* 47K */
	{30, 37643},
	{35, 30334},
	{40, 24591},
	{45, 20048},
	{50, 16433},
	{55, 13539},
	{60, 11210},
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210}		/* FIX_ME */
};


/* NTCG104EF104F(100K) */
static struct BTS_TEMPERATURE LCM_Temperature_Table6[] = {
	{-40, 4251000},
	{-35, 3005000},
	{-30, 2149000},
	{-25, 1554000},
	{-20, 1135000},
	{-15, 837800},
	{-10, 624100},
	{-5, 469100},
	{0, 355600},
	{5, 271800},
	{10, 209400},
	{15, 162500},
	{20, 127000},
	{25, 100000},		/* 100K */
	{30, 79230},
	{35, 63180},
	{40, 50680},
	{45, 40900},
	{50, 33190},
	{55, 27090},
	{60, 22220},
	{65, 18320},
	{70, 15180},
	{75, 12640},
	{80, 10580},
	{85, 8887},
	{90, 7500},
	{95, 6357},
	{100, 5410},
	{105, 4623},
	{110, 3965},
	{115, 3415},
	{120, 2951},
	{125, 2560}
};

/* NCP15WF104F03RC(100K) */
static struct BTS_TEMPERATURE LCM_Temperature_Table7[] = {
	{-40, 4397119},
	{-35, 3088599},
	{-30, 2197225},
	{-25, 1581881},
	{-20, 1151037},
	{-15, 846579},
	{-10, 628988},
	{-5, 471632},
	{0, 357012},
	{5, 272500},
	{10, 209710},
	{15, 162651},
	{20, 127080},
	{25, 100000},		/* 100K */
	{30, 79222},
	{35, 63167},
#if defined(APPLY_PRECISE_NTC_TABLE)
	{40, 50677},
	{41, 48528},
	{42, 46482},
	{43, 44533},
	{44, 42675},
	{45, 40904},
	{46, 39213},
	{47, 37601},
	{48, 36063},
	{49, 34595},
	{50, 33195},
	{51, 31859},
	{52, 30584},
	{53, 29366},
	{54, 28203},
	{55, 27091},
	{56, 26028},
	{57, 25013},
	{58, 24042},
	{59, 23113},
	{60, 22224},
	{61, 21374},
	{62, 20560},
	{63, 19782},
	{64, 19036},
	{65, 18322},
	{66, 17640},
	{67, 16986},
	{68, 16360},
	{69, 15759},
	{70, 15184},
	{71, 14631},
	{72, 14100},
	{73, 13591},
	{74, 13103},
	{75, 12635},
	{76, 12187},
	{77, 11756},
	{78, 11343},
	{79, 10946},
	{80, 10565},
	{81, 10199},
	{82,  9847},
	{83,  9509},
	{84,  9184},
	{85,  8872},
	{86,  8572},
	{87,  8283},
	{88,  8005},
	{89,  7738},
	{90,  7481},
#else
	{40, 50677},
	{45, 40904},
	{50, 33195},
	{55, 27091},
	{60, 22224},
	{65, 18323},
	{70, 15184},
	{75, 12635},
	{80, 10566},
	{85, 8873},
	{90, 7481},
#endif
	{95, 6337},
	{100, 5384},
	{105, 4594},
	{110, 3934},
	{115, 3380},
	{120, 2916},
	{125, 2522}
};


/* convert register to temperature  */
static __s16 mtkts_lcm_thermistor_conver_temp(__s32 Res)
{
	int i = 0;
	int asize = 0;
	__s32 RES1 = 0, RES2 = 0;
	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;

	asize = (ntc_tbl_size / sizeof(struct BTS_TEMPERATURE));

	/* mtkts_lcm_dprintk("mtkts_lcm_thermistor_conver_temp() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= LCM_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
	} else if (Res <= LCM_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
	} else {
		RES1 = LCM_Temperature_Table[0].TemperatureR;
		TMP1 = LCM_Temperature_Table[0].BTS_Temp;
		/* mtkts_lcm_dprintk("%d : RES1 = %d,TMP1 = %d\n",
		 * __LINE__,RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >= LCM_Temperature_Table[i].TemperatureR) {
				RES2 = LCM_Temperature_Table[i].TemperatureR;
				TMP2 = LCM_Temperature_Table[i].BTS_Temp;
				/* mtkts_lcm_dprintk("%d :i=%d, RES2 = %d,
				 * TMP2 = %d\n",__LINE__,i,RES2,TMP2);
				 */
				break;
			}
			RES1 = LCM_Temperature_Table[i].TemperatureR;
			TMP1 = LCM_Temperature_Table[i].BTS_Temp;
			/* mtkts_lcm_dprintk("%d :i=%d, RES1 = %d,TMP1 = %d\n",
			 * __LINE__,i,RES1,TMP1);
			 */
		}

		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2))
								/ (RES1 - RES2);
	}


	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static __s16 mtk_ts_lcm_volt_to_temp(__u32 dwVolt)
{
	__s32 TRes;
	__u64 dwVCriAP = 0;
	__s32 BTS_TMP = -100;
	__u64 dwVCriAP2 = 0;
	/* SW workaround-----------------------------------------------------
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
	 *		(TAP_OVER_CRITICAL_LOW + 39000);
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
	 *		(TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
	 */

	dwVCriAP = ((__u64)g_TAP_over_critical_low *
		(__u64)g_RAP_pull_up_voltage);
	dwVCriAP2 = (g_TAP_over_critical_low + g_RAP_pull_up_R);
	do_div(dwVCriAP, dwVCriAP2);


	if (dwVolt > ((__u32)dwVCriAP)) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (g_RAP_pull_up_R * dwVolt) /
					(g_RAP_pull_up_voltage - dwVolt);
	}
	/* ------------------------------------------------------------------ */

	/* convert register to temperature */
	BTS_TMP = mtkts_lcm_thermistor_conver_temp(TRes);

	return BTS_TMP;
}

static int get_hw_lcm_temp(void)
{
	int val = 0;
	int ret = 0, output;

	ret = iio_read_channel_processed(thermistor_ch4, &val);
	if (ret < 0) {
		mtkts_lcm_printk("Busy/Timeout, IIO ch read failed %d\n", ret);
		return ret;
	}

	/* NOT need to do the conversion "val * 1500 / 4096" */
	/* iio_read_channel_processed can get mV immediately */
	ret = val;


	ret = ret*1800/4096;//82's ADC power 
	mtkts_lcm_dprintk("APtery output mV = %d\n", ret);
	output = mtk_ts_lcm_volt_to_temp(ret);
	mtkts_lcm_dprintk("LCM output temperature = %d\n", output);
	return output;
}

static DEFINE_MUTEX(LCM_lock);

int mtkts_lcm_get_hw_temp(void)
{
	int t_ret = 0;
		
	mutex_lock(&LCM_lock);

	/* get HW AP temp (TSAP) */
	/* cat /sys/class/power_supply/AP/AP_temp */
	t_ret = get_hw_lcm_temp();
	mtkts_lcm_printk("T_AP_LCM=%d\n", t_ret);

	mutex_unlock(&LCM_lock);

	return t_ret;
}

void mtkts_lcm_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		LCM_Temperature_Table = LCM_Temperature_Table1;
		ntc_tbl_size = sizeof(LCM_Temperature_Table1);
		break;
	case 2:		/* AP_NTC_TSM_1 */
		LCM_Temperature_Table = LCM_Temperature_Table2;
		ntc_tbl_size = sizeof(LCM_Temperature_Table2);
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		LCM_Temperature_Table = LCM_Temperature_Table3;
		ntc_tbl_size = sizeof(LCM_Temperature_Table3);
		break;
	case 4:		/* AP_NTC_10 */
		LCM_Temperature_Table = LCM_Temperature_Table4;
		ntc_tbl_size = sizeof(LCM_Temperature_Table4);
		break;
	case 5:		/* AP_NTC_47 */
		LCM_Temperature_Table = LCM_Temperature_Table5;
		ntc_tbl_size = sizeof(LCM_Temperature_Table5);
		break;
	case 6:		/* NTCG104EF104F */
		LCM_Temperature_Table = LCM_Temperature_Table6;
		ntc_tbl_size = sizeof(LCM_Temperature_Table6);
		break;
	case 7:		/* NCP15WF104F03RC */
		LCM_Temperature_Table = LCM_Temperature_Table7;
		ntc_tbl_size = sizeof(LCM_Temperature_Table7);
		break;
	default:		/* AP_NTC_10 */
		LCM_Temperature_Table = LCM_Temperature_Table4;
		ntc_tbl_size = sizeof(LCM_Temperature_Table4);
		break;
	}

	pr_notice("[Thermal/TZ/BTS] %s table_num=%d\n", __func__, table_num);

}

static int mtkts_lcm_probe(struct platform_device *pdev)
{
	int err = 0;
	int ret = 0;

	mtkts_lcm_dprintk("[%s]\n", __func__);


	if (!pdev->dev.of_node) {
		mtkts_lcm_printk("[%s] Only DT based supported\n",
			__func__);
		return -ENODEV;
	}

	thermistor_ch4 = devm_kzalloc(&pdev->dev, sizeof(*thermistor_ch4),
		GFP_KERNEL);
	if (!thermistor_ch4)
		return -ENOMEM;


	thermistor_ch4 = iio_channel_get(&pdev->dev, "thermistor-ch4");
	ret = IS_ERR(thermistor_ch4);
	if (ret) {
		mtkts_lcm_printk("[%s] fail to get auxadc iio ch0: %d\n",
			__func__, ret);
		return ret;
	}

	/* setup default table */
	mtkts_lcm_prepare_table(g_RAP_ntc_table);

	ret = mtkts_lcm_get_hw_temp();
	pr_err("T_AP_LCM=%d\n", ret);

	return err;
}

#ifdef CONFIG_OF
const struct of_device_id mt_thermistor_of_match5[2] = {
	{.compatible = "mediatek,mtboard-thermistor5",},
	{},
};
#endif

#define THERMAL_THERMISTOR_NAME    "mtboard-thermistor5"
static struct platform_driver mtk_thermal_lcm_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = mtkts_lcm_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = THERMAL_THERMISTOR_NAME,
#ifdef CONFIG_OF
		.of_match_table = mt_thermistor_of_match5,
#endif
	},
};

static int __init mtkts_lcm_init(void)
{

	int err = 0;

	mtkts_lcm_dprintk("[%s]\n", __func__);


	err = platform_driver_register(&mtk_thermal_lcm_driver);
	if (err) {
		mtkts_lcm_printk("thermal driver callback register failed.\n");
		return err;
	}

	return 0;
}

static void __exit mtkts_lcm_exit(void)
{
	mtkts_lcm_dprintk("[%s]\n", __func__);
	platform_driver_unregister(&mtk_thermal_lcm_driver);
}

module_init(mtkts_lcm_init);
module_exit(mtkts_lcm_exit);
