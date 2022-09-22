/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_THERMAL_PLATFORM_H
#define _MTK_THERMAL_PLATFORM_H

#include <linux/thermal.h>

extern
int mtk_thermal_get_cpu_info(int *nocores, int **cpufreq, int **cpuloading);

extern
int mtk_thermal_get_gpu_info(int *nocores, int **gpufreq, int **gpuloading);

extern
int mtk_thermal_get_batt_info
(int *batt_voltage, int *batt_current, int *batt_temp);

extern
int mtk_thermal_get_extra_info(int *no_extra_attr,
		char ***attr_names, int **attr_values, char ***attr_unit);

extern
int mtk_thermal_force_get_batt_temp(void);


enum {
	MTK_THERMAL_SCEN_CALL = 0x1
};

extern
unsigned int mtk_thermal_set_user_scenarios(unsigned int mask);

extern
unsigned int mtk_thermal_clear_user_scenarios(unsigned int mask);

extern int mtk_thermal_platform_init(void);
extern void mtk_thermal_platform_exit(void);

extern void mtk_cooler_shutdown_exit(void);
extern int mtk_cooler_shutdown_init(void);

extern int mtk_cooler_backlight_init(void);
extern void mtk_cooler_backlight_exit(void);

extern void mtk_cooler_kshutdown_exit(void);
extern int mtk_cooler_kshutdown_init(void);
extern void mtk_cooler_cam_exit(void);
extern int mtk_cooler_cam_init(void);
extern void mtk_thermal_pm_exit(void);
extern int mtk_thermal_pm_init(void);

extern void mt6358tsbuck1_exit(void);
extern int mt6358tsbuck1_init(void);

extern void mt6358tsbuck2_exit(void);
extern int mt6358tsbuck2_init(void);
extern void mt6358tsbuck3_exit(void);
extern int mt6358tsbuck3_init(void);
extern int mtktsbattery_init(void);
extern void mtktsbattery_exit(void);
extern void mtkts_bts_exit(void);
extern int mtkts_bts_init(void);
extern void mtkts_btsmdpa_exit(void);
extern int mtkts_btsmdpa_init(void);
extern void tscpu_exit(void);
extern int tscpu_init(void);
extern void mtktspa_exit(void);
extern int mtktspa_init(void);
extern void mtk_mdm_txpwr_exit(void);
extern int mtk_mdm_txpwr_init(void);
extern void mtktscharger_exit(void);
extern int mtktscharger_init(void);
extern void wmt_tm_deinit(void);
extern int wmt_tm_init(void);
extern void tsallts_exit(void);
extern int tsallts_init(void);
extern int mtk_imgs_init(void);
extern void mtk_imgs_exit(void);
extern void mtkts_dctm_exit(void);
extern int mtkts_dctm_init(void);
extern void mtktspmic_exit(void);
extern int mtktspmic_init(void);
extern void mtkts_bif_exit(void);
extern int mtkts_bif_init(void);

//for thermal cooler
extern int ta_init(void);
extern void mtk_cooler_mutt_exit(void);
extern int mtk_cooler_mutt_init(void);
extern void mtk_cooler_bcct_exit(void);
extern int mtk_cooler_bcct_init(void);
#if IS_ENABLED(CONFIG_MTK_GAUGE_VERSION)
extern int  mtkcooler_bcct_late_init(void);
#endif
extern void mtk_cooler_atm_exit(void);
extern int mtk_cooler_atm_init(void);
extern void mtk_cooler_dtm_exit(void);
extern int mtk_cooler_dtm_init(void);
extern void mtk_cooler_sysrst_exit(void);
extern int mtk_cooler_sysrst_init(void);
extern int mtk_cooler_VR_FPS_init(void);
extern void mtk_cooler_VR_FPS_exit(void);

#if IS_ENABLED(CONFIG_MTK_SMART_BATTERY)
/* global variable from battery driver... */
extern kal_bool gFG_Is_Charging;
#endif

extern int force_get_tbat(void);


/* --- TA daemon --- */
enum ta_daemon_crtl_cmd_to_kernel {
	TA_DAEMON_CMD_GET_INIT_FLAG = 0,
	TA_DAEMON_CMD_SET_DAEMON_PID,
	TA_DAEMON_CMD_NOTIFY_DAEMON,
	TA_DAEMON_CMD_NOTIFY_DAEMON_CATMINIT,
	TA_DAEMON_CMD_SET_TTJ,
	TA_DAEMON_CMD_GET_TPCB,
	TA_DAEMON_CMD_GET_TI,
	TA_DAEMON_CMD_GET_DCTM_DRCCFG,
	TA_DAEMON_CMD_GET_DTCM,
	TA_DAEMON_CMD_GET_TSCPU,

	TA_DAEMON_CMD_TO_KERNEL_NUMBER
}; /*must sync userspace/kernel: TA_DAEMON_CTRL_CMD_FROM_USER*/

#define TAD_NL_MSG_T_HDR_LEN 12
#define TAD_NL_MSG_MAX_LEN 512

struct tad_nl_msg_t {
	unsigned int tad_cmd;
	unsigned int tad_data_len;
	unsigned int tad_ret_data_len;
	char tad_data[TAD_NL_MSG_MAX_LEN];
};

enum {
	TA_CATMPLUS = 1,
	TA_CONTINUOUS = 2,
	TA_CATMPLUS_TTJ = 3,
	TA_DCTM_DRC_CFG = 4,
	TA_DCTM_DRC_RST = 5,
	TA_DCTM_TTJ = 6
};
/* --- TA daemon --- */

/* --- cATM parameters --- */
struct cATM_params_t {
	int CATM_ON;
	int K_TT;
	int K_SUM_TT_LOW;
	int K_SUM_TT_HIGH;
	int MIN_SUM_TT;
	int MAX_SUM_TT;
	int MIN_TTJ;
	int CATMP_STEADY_TTJ_DELTA;
};
struct continuetm_params_t {
	int STEADY_TARGET_TJ;
	int MAX_TARGET_TJ;
	int TRIP_TPCB;
	int STEADY_TARGET_TPCB;
};


struct CATM_T {
	struct cATM_params_t t_catm_par;
	struct continuetm_params_t t_continuetm_par;
};
extern struct CATM_T thermal_atm_t;
/* --- cATM parameters --- */

/* --- SPA parameters --- */
struct spa_Tpolicy_info {
	int min_cpu_power[3];
	int min_gpu_power[3];
	int steady_target_tj;
	int steady_exit_tj;
};

struct spa_system_info {
	int cpu_Tj;
	int Tpcb;
	int OPP_power;
	unsigned int fg_app_pid;
	unsigned int avg_fps;
	int WIFI_UL_Tput;
	int MD_UL_Tput;
	int chg_current_limit;
	int input_current_limit;
	int camera_on;
	int game_mode;
};

struct SPA_T {
	struct spa_Tpolicy_info t_spa_Tpolicy_info;
	struct spa_system_info t_spa_system_info;
};

extern struct SPA_T thermal_spa_t;
/* --- SPA parameters --- */

/* --- DCTM parameters --- */
struct DRC_params_t {
	int tamb;
	int init_r1_r2;
	int init_r1c1;
	int dtskin_thres;
	int drc_bound;
	int ttskin;
	int ave_window;
	int drc_ave_window;
	int ttj_limit;
};

struct DCTM_T {
	struct DRC_params_t t_drc_par;
};

extern struct DCTM_T thermal_dctm_t;
/* --- DCTM parameters --- */

int wakeup_ta_algo(int flow_state);
int ta_get_ttj(void);

extern int mtk_thermal_get_tpcb_target(void);
extern int tsatm_thermal_get_catm_type(void);
extern int tsdctm_thermal_get_ttj_on(void);

#endif/* _MTK_THERMAL_PLATFORM_H */
