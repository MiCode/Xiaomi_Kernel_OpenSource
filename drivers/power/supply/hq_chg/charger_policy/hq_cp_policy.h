// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */
#ifndef __LINUX_HUAQIN_CHARGER_PUMP_POLICY_H__
#define __LINUX_HUAQIN_CHARGER_PUMP_POLICY_H__

#include "../charger_class/hq_adapter_class.h"
#include "../charger_class/hq_charger_class.h"
#include "../charger_class/hq_cp_class.h"
#include "../common/hq_voter.h"
#include "hq_jeita.h"

#define ADAPTER_DEVICE_MAX                    5
#define CHARGERPUMP_POLICY_LOOP_TIME          300// ms
#define CHARGERPUMP_POLICY_EXIT_THREAD_TIME   -1
#define CHARGERPUMP_POLICY_REQUEST_SAFE9V     9000
#define CHARGERPUMP_POLICY_FIRST_REQUEST_CURR 1000
#define CHARGERPUMP_POLICY_MAIN_CHG_CURR      100
#define CHARGERPUMP_POLICY_RECOVER_CNT        10
#define CHARGERPUMP_POLICY_ENABLE_RETRY_CNT   30
#define CHARGERPUMP_POLICY_END_CHG_CNT        30
#define CHARGERPUMP_POLICY_HIGH_SOC           95
#define CHARGERPUMP_POLICY_FFC_CV_OFFSET      10

enum thermal_level {
	THERMAL_COLD,
	THERMAL_VERY_COOL,
	THERMAL_COOL,
	THERMAL_NORMAL,
	THERMAL_WARM,
	THERMAL_VERY_WARM,
	THERMAL_HOT,
	THERMAL_MAX,
};

enum state_machine {
	PM_STATE_CHECK_DEV = 0,
	PM_STATE_INIT,
	PM_STATE_MEASURE_RCABLE,
	PM_STATE_ENABLE_CHARGERPUMP,
	PM_STATE_CHARGERPUMP_CC_CV,
	PM_STATE_CHARGERPUMP_EXIT,
};

enum policy_state {
	POLICY_NO_START = 0,
	POLICY_NO_SUPPORT,
	POLICY_RUNNING,
	POLICY_STOP,
};

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_CHARGERPUMP_DONE,
	PM_ALGO_RET_CP_LMT_CURR,
	PM_ALGO_RET_INPUT_SUSPEND,
	PM_ALGO_RET_SOFT_RESET,
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	PM_ALGO_RET_FG_I2C_ERROR,
#endif
};

enum {
	CHARGE_PD_INVALID = 0,
	CHARGE_PD_ACTIVE,
	CHARGE_PD_PPS_ACTIVE,
};

struct chg_info {
	int vbat;
	int ibat;
	int last_ibat;
	int vbus;
	int ibus;
	int m_ibus;
	int s_ibus;
	int tbat;
	uint32_t ffc_cv;
	uint32_t total_fcc;
	int main_chg_disable;
	uint8_t tdie;
	uint8_t tadapter;
	bool mainchg_chging;
	bool m_cp_chging;
	bool s_cp_chging;
	bool adapter_soft_reset;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int soc;
#endif
};

struct chargerpump_policy_config {
	uint32_t warm_cv;
	uint32_t cv_offset;
	uint32_t cc;
	uint32_t exit_cc;
	uint32_t min_vbat;
	uint32_t step_volt;
	uint32_t warm_temp;


	uint32_t max_request_volt;
	uint32_t min_request_volt;
	uint32_t min_request_curr;
	uint32_t max_request_curr;

	uint32_t max_vbat;
	uint32_t max_ibat;

	bool s_cp_enable;
};

struct chargerpump_policy {
	struct device *dev;
	struct chargerpump_policy_config cfg;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	bool run_thread;

	enum state_machine sm;
	enum policy_state state;

	struct timer_list policy_timer;

	char *adapter_name[ADAPTER_DEVICE_MAX];
	struct adapter_dev *adapter;
	struct charger_dev *charger;
	struct chargerpump_dev *master_cp;
	struct chargerpump_dev *slave_cp;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	struct fuel_gauge_dev *fuel_gauge;
#endif

	struct adapter_cap cap;
	uint8_t cap_nr;

	struct chg_info info;

	uint32_t next_time;
	bool recover;
	uint8_t recover_cnt;

	uint32_t request_volt;
	uint32_t request_curr;

	bool fast_request;
	bool cp_charge_done;

	struct mutex state_lock;
	struct mutex running_lock;
	struct mutex access_lock;

	struct power_supply *fg_psy;
	struct votable *total_fcc_votable;
	struct votable *main_chg_disable_votable;
	struct votable *main_fcc_votable;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	bool switch2_1_enable;
	bool switch2_1_single_enable;
	bool switch1_1_enable;
	bool switch1_1_single_enable;
	bool switch_mode;
	int div_rate;
	int m_cp_mode;
	int s_cp_mode;
	int initial_vbus_flag;
#endif
};

int chargerpump_policy_start(struct chargerpump_policy *policy);
int chargerpump_policy_stop(struct chargerpump_policy *policy);
bool chargerpump_policy_check_adapter_cap(struct chargerpump_policy *policy,
							struct adapter_cap *cap);

#endif /* __LINUX_HUAQIN_CHARGER_PUMP_POLICY_H__ */
