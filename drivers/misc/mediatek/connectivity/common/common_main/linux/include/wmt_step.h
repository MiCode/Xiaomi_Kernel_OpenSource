/*
 * Copyright (C) 2016 MediaTek Inc. *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _WMT_STEP_H_
#define _WMT_STEP_H_

#include <linux/list.h>

#include "osal.h"
#include "wmt_exp.h"
#include "wmt_core.h"

#define STEP_CONFIG_NAME "WMT_STEP.cfg"
#define STEP_VERSION 2

#define STEP_PERIODIC_DUMP_WORK_QUEUE "wmt_step_pd_wq"
#define STEP_PERIODIC_DUMP_THREAD "wmt_pd"

#define STEP_ACTION_NAME_EMI "_EMI"
#define STEP_ACTION_NAME_REGISTER "_REG"
#define STEP_ACTION_NAME_GPIO "GPIO"
#define STEP_ACTION_NAME_DISABLE_RESET "DRST"
#define STEP_ACTION_NAME_CHIP_RESET "_RST"
#define STEP_ACTION_NAME_KEEP_WAKEUP "WAK+"
#define STEP_ACTION_NAME_CANCEL_WAKEUP "WAK-"
#define STEP_ACTION_NAME_SHOW_STRING "SHOW"
#define STEP_ACTION_NAME_SLEEP "_SLP"
#define STEP_ACTION_NAME_CONDITION "COND"
#define STEP_ACTION_NAME_VALUE "_VAL"
#define STEP_ACTION_NAME_CONDITION_EMI "CEMI"
#define STEP_ACTION_NAME_CONDITION_REGISTER "CREG"

extern struct platform_device *g_pdev;

enum step_action_id {
	STEP_ACTION_INDEX_NO_DEFINE = 0,
	STEP_ACTION_INDEX_EMI = 1,
	STEP_ACTION_INDEX_REGISTER,
	STEP_ACTION_INDEX_GPIO,
	STEP_ACTION_INDEX_DISABLE_RESET,
	STEP_ACTION_INDEX_CHIP_RESET,
	STEP_ACTION_INDEX_KEEP_WAKEUP,
	STEP_ACTION_INDEX_CANCEL_WAKEUP,
	STEP_ACTION_INDEX_PERIODIC_DUMP,
	STEP_ACTION_INDEX_SHOW_STRING,
	STEP_ACTION_INDEX_SLEEP,
	STEP_ACTION_INDEX_CONDITION,
	STEP_ACTION_INDEX_VALUE,
	STEP_ACTION_INDEX_CONDITION_EMI,
	STEP_ACTION_INDEX_CONDITION_REGISTER,
	STEP_ACTION_INDEX_MAX,
};

enum step_trigger_point_id {
	STEP_TRIGGER_POINT_NO_DEFINE = 0,
	STEP_TRIGGER_POINT_COMMAND_TIMEOUT = 1,
	STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT,
	STEP_TRIGGER_POINT_BEFORE_CHIP_RESET,
	STEP_TRIGGER_POINT_AFTER_CHIP_RESET,
	STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_READ_THERMAL,
	STEP_TRIGGER_POINT_POWER_ON_START,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_GET_CONNSYS_ID,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_SEND_DOWNLOAD_PATCH,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_CONNSYS_RESET,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_SET_WIFI_LTE_COEX,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_BT_WIFI_CALIBRATION,
	STEP_TRIGGER_POINT_POWER_ON_END,
	STEP_TRIGGER_POINT_BEFORE_POWER_OFF,
	STEP_TRIGGER_POINT_WHEN_AP_SUSPEND,
	STEP_TRIGGER_POINT_WHEN_AP_RESUME,
	STEP_TRIGGER_POINT_POWER_OFF_HANDSHAKE,
	STEP_TRIGGER_POINT_BEFORE_RESTORE_CAL_RESULT,
	STEP_TRIGGER_POINT_AFTER_RESTORE_CAL_RESULT,
	STEP_TRIGGER_POINT_POWER_ON_AFTER_BT_WIFI_CALIBRATION,
	STEP_TRIGGER_POINT_AFTER_RESTORE_CAL_CMD,
	STEP_TRIGGER_POINT_WHEN_CLOCK_FAIL,
	STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_OFF,
	STEP_TRIGGER_POINT_MAX,
};

enum step_base_address_index {
	STEP_MCU_BASE_INDEX = 0,
	STEP_TOP_RGU_BASE_INDEX,
	STEP_INFRACFG_AO_BASE_INDEX,
	STEP_SPM_BASE_INDEX,
	STEP_MCU_CONN_HIF_ON_BASE_INDEX,
	STEP_MCU_TOP_MISC_OFF_BASE_INDEX,
	STEP_MCU_CFG_ON_BASE_INDEX,
	STEP_MCU_CIRQ_BASE_INDEX,
	STEP_MCU_TOP_MISC_ON_BASE_INDEX,
	STEP_BASE_ADDRESS_MAX,
};

enum step_register_base_id {
	STEP_REGISTER_PHYSICAL_ADDRESS = 0,
	STEP_REGISTER_CONN_MCU_CONFIG_BASE,
	STEP_REGISTER_AP_RGU_BASE,
	STEP_REGISTER_TOPCKGEN_BASE,
	STEP_REGISTER_SPM_BASE,
	STEP_REGISTER_HIF_ON_BASE,
	STEP_REGISTER_MISC_OFF_BASE,
	STEP_REGISTER_CFG_ON_BASE,
	STEP_CIRQ_BASE,
	STEP_MCU_TOP_MISC_ON_BASE,
	STEP_REGISTER_MAX,
};

enum step_condition_operator_id {
	STEP_OPERATOR_GREATER = 0,
	STEP_OPERATOR_GREATER_EQUAL,
	STEP_OPERATOR_LESS,
	STEP_OPERATOR_LESS_EQUAL,
	STEP_OPERATOR_EQUAL,
	STEP_OPERATOR_NOT_EQUAL,
	STEP_OPERATOR_AND,
	STEP_OPERATOR_OR,
	STEP_OPERATOR_MAX,
};

struct step_register_base_struct {
	unsigned long vir_addr;
	unsigned long long size;
};

struct step_action_list {
	struct list_head list;
};

struct step_pd_entry {
	bool is_enable;
	unsigned int expires_ms;
	struct step_action_list action_list;
	struct delayed_work pd_work;
	struct list_head list;
};

struct step_pd_struct {
	bool is_init;
	struct workqueue_struct *step_pd_wq;
	struct list_head pd_list;
};

struct step_action {
	struct list_head list;
	enum step_action_id action_id;
};

typedef int (*STEP_WRITE_ACT_TO_LIST) (struct step_action_list *, enum step_action_id, int, char **);
typedef void (*STEP_DO_EXTRA) (unsigned int, ...);

#define STEP_OUTPUT_LOG 0
#define STEP_OUTPUT_REGISTER 1

struct step_emi_info {
	bool is_write;
	unsigned int begin_offset;
	unsigned int end_offset;
	int value;
	unsigned int temp_reg_id;
	int output_mode;
	int mask;
};

struct step_emi_action {
	struct step_emi_info info;
	struct step_action base;
};

struct step_reigster_info {
	bool is_write;
	enum step_register_base_id address_type;
	unsigned long address;
	unsigned int offset;
	unsigned int times;
	unsigned int delay_time;
	int value;
	int mask;
	unsigned int temp_reg_id;
	int output_mode;
};

struct step_register_action {
	struct step_reigster_info info;
	struct step_action base;
};

struct step_gpio_action {
	bool is_write;
	unsigned int pin_symbol;
	struct step_action base;
};

struct step_disable_reset_action {
	struct step_action base;
};

struct step_chip_reset_action {
	struct step_action base;
};

struct step_keep_wakeup_action {
	struct step_action base;
};

struct step_cancel_wakeup_action {
	struct step_action base;
};

struct step_periodic_dump_action {
	struct step_pd_entry *pd_entry;
	struct step_action base;
};

struct step_show_string_action {
	char *content;
	struct step_action base;
};

struct step_sleep_action {
	unsigned int ms;
	struct step_action base;
};

#define STEP_CONDITION_RIGHT_REGISTER 0
#define STEP_CONDITION_RIGHT_VALUE 1
struct step_condition_action {
	unsigned int result_temp_reg_id;
	unsigned int l_temp_reg_id;
	unsigned int r_temp_reg_id;
	int value;
	int mode;
	enum step_condition_operator_id operator_id;
	struct step_action base;
};

struct step_value_action {
	unsigned int temp_reg_id;
	int value;
	struct step_action base;
};

struct step_condition_emi_action {
	unsigned int cond_reg_id;
	struct step_emi_info info;
	struct step_action base;
};

struct step_condition_register_action {
	unsigned int cond_reg_id;
	struct step_reigster_info info;
	struct step_action base;
};

#define list_entry_action(act_struct, ptr) \
	container_of(ptr, struct step_##act_struct##_action, base)

struct step_reg_addr_info {
	int address_type;
	unsigned long address;
};

struct step_target_act_list_info {
	enum step_trigger_point_id tp_id;
	struct step_action_list *p_target_list;
	struct step_pd_entry *p_pd_entry;
};

#define STEP_PARAMETER_SIZE 10
struct step_parse_line_data_param_info {
	int state;
	enum step_action_id act_id;
	char *act_params[STEP_PARAMETER_SIZE];
	int param_index;
};

typedef struct step_action *(*STEP_CREATE_ACTION) (int, char *[]);
typedef int (*STEP_DO_ACTIONS) (struct step_action *, STEP_DO_EXTRA);
typedef void (*STEP_REMOVE_ACTION) (struct step_action *);
struct step_action_contrl {
	STEP_CREATE_ACTION func_create_action;
	STEP_DO_ACTIONS func_do_action;
	STEP_REMOVE_ACTION func_remove_action;
};

#define STEP_REGISTER_BASE_SYMBOL '#'
#define STEP_TEMP_REGISTER_SYMBOL '$'

#define STEP_VALUE_INFO_UNKNOWN -1
#define STEP_VALUE_INFO_NUMBER 0
#define STEP_VALUE_INFO_SYMBOL_REG_BASE 1
#define STEP_VALUE_INFO_SYMBOL_TEMP_REG 2

#define STEP_TEMP_REGISTER_SIZE 10
struct step_env_struct {
	bool is_enable;
	bool is_keep_wakeup;
	struct step_action_list actions[STEP_TRIGGER_POINT_MAX];
	unsigned char __iomem *emi_base_addr;
	struct step_register_base_struct reg_base[STEP_REGISTER_MAX];
	struct step_pd_struct pd_struct;
	int temp_register[STEP_TEMP_REGISTER_SIZE];
	bool is_setup;
	struct rw_semaphore init_rwsem;
};

/********************************************************************************
 *              F U N C T I O N   D E C L A R A T I O N S
*********************************************************************************/
void wmt_step_init(void);
void wmt_step_deinit(void);
void wmt_step_do_actions(enum step_trigger_point_id tp_id);
void wmt_step_func_crtl_do_actions(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId);
void wmt_step_command_timeout_do_actions(char *reason);
#ifdef CFG_WMT_STEP
#define WMT_STEP_INIT_FUNC() wmt_step_init()
#define WMT_STEP_DEINIT_FUNC() wmt_step_deinit()
#define WMT_STEP_DO_ACTIONS_FUNC(tp) wmt_step_do_actions(tp)
#define WMT_STEP_FUNC_CTRL_DO_ACTIONS_FUNC(type, id) wmt_step_func_crtl_do_actions(type, id)
#define WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC(reason) wmt_step_command_timeout_do_actions(reason)
#else
#define WMT_STEP_INIT_FUNC()
#define WMT_STEP_DEINIT_FUNC()
#define WMT_STEP_DO_ACTIONS_FUNC(tp)
#define WMT_STEP_FUNC_CTRL_DO_ACTIONS_FUNC(type, id)
#define WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC(reason)
#endif

/********************************************************************************
 *              D E C L A R E   F O R   T E S T
*********************************************************************************/
int wmt_step_read_file(const char *file_name);
int wmt_step_parse_data(const char *in_buf, unsigned int size, STEP_WRITE_ACT_TO_LIST func_act_to_list);
int wmt_step_init_pd_env(void);
int wmt_step_deinit_pd_env(void);
struct step_pd_entry *wmt_step_get_periodic_dump_entry(unsigned int expires);
struct step_action *wmt_step_create_action(enum step_action_id act_id, int param_num, char *params[]);
int wmt_step_do_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_gpio_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_disable_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_chip_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_keep_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_cancel_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_periodic_dump_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_show_string_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_sleep_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_condition_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_value_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_condition_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_condition_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
void wmt_step_remove_action(struct step_action *p_act);
void wmt_step_print_version(void);

#endif /* end of _WMT_STEP_H_ */

