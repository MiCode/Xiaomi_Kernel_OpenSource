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

#include <connectivity_build_in_adapter.h>
#include <linux/platform_device.h>

#include "osal.h"
#include "wmt_step.h"
#include "wmt_dev.h"
#include "wmt_plat.h"
#include "mtk_wcn_consys_hw.h"
#include "stp_core.h"
#include "wmt_lib.h"

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************/
static struct step_action *wmt_step_create_emi_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_register_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_gpio_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_disable_reset_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_chip_reset_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_keep_wakeup_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_cancel_wakeup_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_periodic_dump_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_show_string_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_sleep_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_condition_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_value_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_condition_emi_action(int param_num, char *params[]);
static struct step_action *wmt_step_create_condition_register_action(int param_num, char *params[]);

static void wmt_step_remove_emi_action(struct step_action *p_act);
static void wmt_step_remove_register_action(struct step_action *p_act);
static void wmt_step_remove_gpio_action(struct step_action *p_act);
static void wmt_step_remove_disable_reset_action(struct step_action *p_act);
static void wmt_step_remove_chip_reset_action(struct step_action *p_act);
static void wmt_step_remove_keep_wakeup_action(struct step_action *p_act);
static void wmt_step_remove_cancel_wakeup_action(struct step_action *p_act);
static void wmt_step_remove_periodic_dump_action(struct step_action *p_act);
static void wmt_step_remove_show_string_action(struct step_action *p_act);
static void wmt_step_remove_sleep_action(struct step_action *p_act);
static void wmt_step_remove_condition_action(struct step_action *p_act);
static void wmt_step_remove_value_action(struct step_action *p_act);
static void wmt_step_remove_condition_emi_action(struct step_action *p_act);
static void wmt_step_remove_condition_register_action(struct step_action *p_act);

static int wmt_step_access_line_state_init(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_tp(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_at(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_at_op(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_pd(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);

static int wmt_step_operator_result_greater(int l_val, int r_val);
static int wmt_step_operator_result_greater_equal(int l_val, int r_val);
static int wmt_step_operator_result_less(int l_val, int r_val);
static int wmt_step_operator_result_less_equal(int l_val, int r_val);
static int wmt_step_operator_result_equal(int l_val, int r_val);
static int wmt_step_operator_result_not_equal(int l_val, int r_val);
static int wmt_step_operator_result_and(int l_val, int r_val);
static int wmt_step_operator_result_or(int l_val, int r_val);

/*******************************************************************************
 *                           D E F I N E
********************************************************************************/
#define STEP_EMI_ACT_INT (int)(*(int *)STEP_ACTION_NAME_EMI)
#define STEP_REG_ACT_INT (int)(*(int *)STEP_ACTION_NAME_REGISTER)
#define STEP_GPIO_ACT_INT (int)(*(int *)STEP_ACTION_NAME_GPIO)
#define STEP_DISABLE_RESET_ACT_INT (int)(*(int *)STEP_ACTION_NAME_DISABLE_RESET)
#define STEP_CHIP_RESET_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CHIP_RESET)
#define STEP_KEEP_WAKEUP_ACT_INT (int)(*(int *)STEP_ACTION_NAME_KEEP_WAKEUP)
#define STEP_CANCEL_KEEP_WAKEUP_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CANCEL_WAKEUP)
#define STEP_SHOW_STRING_ACT_INT (int)(*(int *)STEP_ACTION_NAME_SHOW_STRING)
#define STEP_SLEEP_ACT_INT (int)(*(int *)STEP_ACTION_NAME_SLEEP)
#define STEP_CONDITION_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CONDITION)
#define STEP_VALUE_ACT_INT (int)(*(int *)STEP_ACTION_NAME_VALUE)
#define STEP_CONDITION_EMI_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CONDITION_EMI)
#define STEP_CONDITION_REG_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CONDITION_REGISTER)

#define STEP_PARSE_LINE_STATE_INIT 0
#define STEP_PARSE_LINE_STATE_TP 1
#define STEP_PARSE_LINE_STATE_AT 2
#define STEP_PARSE_LINE_STATE_AT_OP 3
#define STEP_PARSE_LINE_STATE_PD_START 4
#define STEP_PARSE_LINE_STATE_PD_END 5
/*******************************************************************************
 *                           P R I V A T E   D A T A
********************************************************************************/
struct step_env_struct g_step_env;

static const struct step_action_contrl wmt_step_action_map[] = {
	[STEP_ACTION_INDEX_EMI] = {
		wmt_step_create_emi_action,
		wmt_step_do_emi_action,
		wmt_step_remove_emi_action
	},
	[STEP_ACTION_INDEX_REGISTER] = {
		wmt_step_create_register_action,
		wmt_step_do_register_action,
		wmt_step_remove_register_action
	},
	[STEP_ACTION_INDEX_GPIO] = {
		wmt_step_create_gpio_action,
		wmt_step_do_gpio_action,
		wmt_step_remove_gpio_action
	},
	[STEP_ACTION_INDEX_DISABLE_RESET] = {
		wmt_step_create_disable_reset_action,
		wmt_step_do_disable_reset_action,
		wmt_step_remove_disable_reset_action
	},
	[STEP_ACTION_INDEX_CHIP_RESET] = {
		wmt_step_create_chip_reset_action,
		wmt_step_do_chip_reset_action,
		wmt_step_remove_chip_reset_action
	},
	[STEP_ACTION_INDEX_KEEP_WAKEUP] = {
		wmt_step_create_keep_wakeup_action,
		wmt_step_do_keep_wakeup_action,
		wmt_step_remove_keep_wakeup_action
	},
	[STEP_ACTION_INDEX_CANCEL_WAKEUP] = {
		wmt_step_create_cancel_wakeup_action,
		wmt_step_do_cancel_wakeup_action,
		wmt_step_remove_cancel_wakeup_action
	},
	[STEP_ACTION_INDEX_PERIODIC_DUMP] = {
		wmt_step_create_periodic_dump_action,
		wmt_step_do_periodic_dump_action,
		wmt_step_remove_periodic_dump_action,
	},
	[STEP_ACTION_INDEX_SHOW_STRING] = {
		wmt_step_create_show_string_action,
		wmt_step_do_show_string_action,
		wmt_step_remove_show_string_action
	},
	[STEP_ACTION_INDEX_SLEEP] = {
		wmt_step_create_sleep_action,
		wmt_step_do_sleep_action,
		wmt_step_remove_sleep_action
	},
	[STEP_ACTION_INDEX_CONDITION] = {
		wmt_step_create_condition_action,
		wmt_step_do_condition_action,
		wmt_step_remove_condition_action
	},
	[STEP_ACTION_INDEX_VALUE] = {
		wmt_step_create_value_action,
		wmt_step_do_value_action,
		wmt_step_remove_value_action
	},
	[STEP_ACTION_INDEX_CONDITION_EMI] = {
		wmt_step_create_condition_emi_action,
		wmt_step_do_condition_emi_action,
		wmt_step_remove_condition_emi_action
	},
	[STEP_ACTION_INDEX_CONDITION_REGISTER] = {
		wmt_step_create_condition_register_action,
		wmt_step_do_condition_register_action,
		wmt_step_remove_condition_register_action
	},
};

static const char * const STEP_TRIGGER_TIME_NAME[] = {
	[STEP_TRIGGER_POINT_COMMAND_TIMEOUT] =
		"[TP 1] When Command timeout",
	[STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT] =
		"[TP 2] When Firmware trigger assert",
	[STEP_TRIGGER_POINT_BEFORE_CHIP_RESET] =
		"[TP 3] Before Chip reset",
	[STEP_TRIGGER_POINT_AFTER_CHIP_RESET] =
		"[TP 4] After Chip reset",
	[STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON] =
		"[TP 5] Before Wifi function on",
	[STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF] =
		"[TP 6] Before Wifi function off",
	[STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON] =
		"[TP 7] Before BT function on",
	[STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF] =
		"[TP 8] Before BT function off",
	[STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON] =
		"[TP 9] Before FM function on",
	[STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF] =
		"[TP 10] Before FM function off",
	[STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON] =
		"[TP 11] Before GPS function on",
	[STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF] =
		"[TP 12] Before GPS function off",
	[STEP_TRIGGER_POINT_BEFORE_READ_THERMAL] =
		"[TP 13] Before read consys thermal",
	[STEP_TRIGGER_POINT_POWER_ON_START] =
		"[TP 14] Power on sequence(0): Start power on",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_GET_CONNSYS_ID] =
		"[TP 15] Power on sequence(1): Before can get connsys id",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_SEND_DOWNLOAD_PATCH] =
		"[TP 16] Power on sequence(2): Before send download patch",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_CONNSYS_RESET] =
		"[TP 17] Power on sequence(3): Before connsys reset (donwload patch)",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_SET_WIFI_LTE_COEX] =
		"[TP 18] Power on sequence(4): Before set wifi and lte coex",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_BT_WIFI_CALIBRATION] =
		"[TP 19] Power on sequence(5): Before set BT and Wifi calibration",
	[STEP_TRIGGER_POINT_POWER_ON_END] =
		"[TP 20] Power on sequence(6): End power on",
	[STEP_TRIGGER_POINT_BEFORE_POWER_OFF] =
		"[TP 21] Before WMT power off",
	[STEP_TRIGGER_POINT_WHEN_AP_SUSPEND] =
		"[TP 22] When AP suspend",
	[STEP_TRIGGER_POINT_WHEN_AP_RESUME] =
		"[TP 23] When AP resume",
	[STEP_TRIGGER_POINT_POWER_OFF_HANDSHAKE] =
		"[TP 24] When power off handshake",
	[STEP_TRIGGER_POINT_BEFORE_RESTORE_CAL_RESULT] =
		"[TP 25] Before restore calibration result",
	[STEP_TRIGGER_POINT_AFTER_RESTORE_CAL_RESULT] =
		"[TP 26] After restore calibration result",
	[STEP_TRIGGER_POINT_POWER_ON_AFTER_BT_WIFI_CALIBRATION] =
		"[TP 27] Power on sequence(5): After BT and Wi-Fi calibration",
	[STEP_TRIGGER_POINT_AFTER_RESTORE_CAL_CMD] =
		"[TP 28] After send calibration restore command",
	[STEP_TRIGGER_POINT_WHEN_CLOCK_FAIL] =
		"[TP 29] When clock fail",
	[STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_ON] =
		"[TP 30] Before GPSL5 function on",
	[STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_OFF] =
		"[TP 31] Before GPSL5 function off",
};

static const int wmt_step_func_ctrl_id[WMTDRV_TYPE_MAX][2] = {
	[WMTDRV_TYPE_BT] = {
		STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON
	},
	[WMTDRV_TYPE_FM] = {
		STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON
	},
	[WMTDRV_TYPE_GPS] = {
		STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON
	},
	[WMTDRV_TYPE_WIFI] = {
		STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON
	},
	[WMTDRV_TYPE_GPSL5] = {
		STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_GPSL5_FUNC_ON
	},
};

typedef int (*STEP_LINE_STATE) (char *,
	struct step_target_act_list_info *, struct step_parse_line_data_param_info *);
static const STEP_LINE_STATE wmt_step_line_state_action_map[] = {
	[STEP_PARSE_LINE_STATE_INIT] = wmt_step_access_line_state_init,
	[STEP_PARSE_LINE_STATE_TP] = wmt_step_access_line_state_tp,
	[STEP_PARSE_LINE_STATE_AT] = wmt_step_access_line_state_at,
	[STEP_PARSE_LINE_STATE_AT_OP] = wmt_step_access_line_state_at_op,
	[STEP_PARSE_LINE_STATE_PD_START] = wmt_step_access_line_state_pd,
};

typedef int (*STEP_OPERATOR_RESULT) (int, int);
static const STEP_OPERATOR_RESULT wmt_step_operator_result_map[] = {
	[STEP_OPERATOR_GREATER] = wmt_step_operator_result_greater,
	[STEP_OPERATOR_GREATER_EQUAL] = wmt_step_operator_result_greater_equal,
	[STEP_OPERATOR_LESS] = wmt_step_operator_result_less,
	[STEP_OPERATOR_LESS_EQUAL] = wmt_step_operator_result_less_equal,
	[STEP_OPERATOR_EQUAL] = wmt_step_operator_result_equal,
	[STEP_OPERATOR_NOT_EQUAL] = wmt_step_operator_result_not_equal,
	[STEP_OPERATOR_AND] = wmt_step_operator_result_and,
	[STEP_OPERATOR_OR] = wmt_step_operator_result_or,
};

/*******************************************************************************
 *                      I N T E R N A L   F U N C T I O N S
********************************************************************************/
#ifdef CFG_WMT_STEP
static void wmt_step_init_list(void)
{
	unsigned int i = 0;

	for (i = 0; i < STEP_TRIGGER_POINT_MAX; i++)
		INIT_LIST_HEAD(&(g_step_env.actions[i].list));
}
#endif

static unsigned char __iomem *wmt_step_get_emi_base_address(void)
{
	if (g_step_env.emi_base_addr == NULL) {
		if (gConEmiPhyBase)
			g_step_env.emi_base_addr = ioremap_nocache(gConEmiPhyBase, gConEmiSize);
	}

	return g_step_env.emi_base_addr;
}

static void _wmt_step_init_register_base_size(struct device_node *node, int index, int step_index, unsigned long addr)
{
	int flag;

	if (step_index < 0)
		return;

	g_step_env.reg_base[step_index].vir_addr = addr;
	if (addr != 0)
		of_get_address(node, index, &(g_step_env.reg_base[step_index].size), &flag);
}

static void wmt_step_init_register_base_size(void)
{
	struct device_node *node = NULL;

	/* If you need to change the register index, address. Please update STEP Config comment */
	if (g_pdev != NULL) {
		node = g_pdev->dev.of_node;
		_wmt_step_init_register_base_size(node, STEP_MCU_BASE_INDEX,
			STEP_REGISTER_CONN_MCU_CONFIG_BASE, conn_reg.mcu_base);
		_wmt_step_init_register_base_size(node, STEP_TOP_RGU_BASE_INDEX,
			STEP_REGISTER_AP_RGU_BASE, conn_reg.ap_rgu_base);
		_wmt_step_init_register_base_size(node, STEP_INFRACFG_AO_BASE_INDEX,
			STEP_REGISTER_TOPCKGEN_BASE, conn_reg.topckgen_base);
		_wmt_step_init_register_base_size(node, STEP_SPM_BASE_INDEX,
			STEP_REGISTER_SPM_BASE, conn_reg.spm_base);
		_wmt_step_init_register_base_size(node, STEP_MCU_CONN_HIF_ON_BASE_INDEX,
			STEP_REGISTER_HIF_ON_BASE, conn_reg.mcu_conn_hif_on_base);
		_wmt_step_init_register_base_size(node, STEP_MCU_TOP_MISC_OFF_BASE_INDEX,
			STEP_REGISTER_MISC_OFF_BASE, conn_reg.mcu_top_misc_off_base);
		_wmt_step_init_register_base_size(node, STEP_MCU_CFG_ON_BASE_INDEX,
			STEP_REGISTER_CFG_ON_BASE, conn_reg.mcu_cfg_on_base);
		_wmt_step_init_register_base_size(node, STEP_MCU_CIRQ_BASE_INDEX,
			STEP_CIRQ_BASE, conn_reg.mcu_cirq_base);
		_wmt_step_init_register_base_size(node, STEP_MCU_TOP_MISC_ON_BASE_INDEX,
			STEP_MCU_TOP_MISC_ON_BASE, conn_reg.mcu_top_misc_on_base);
	}
}

static void wmt_step_clear_action_list(struct step_action_list *action_list)
{
	struct step_action *p_act = NULL, *p_act_next = NULL;

	list_for_each_entry_safe(p_act, p_act_next, &(action_list->list), list) {
		list_del_init(&p_act->list);
		wmt_step_remove_action(p_act);
	}
}

static void wmt_step_clear_list(void)
{
	unsigned int i = 0;

	for (i = 0; i < STEP_TRIGGER_POINT_MAX; i++)
		wmt_step_clear_action_list(&g_step_env.actions[i]);
}

static void wmt_step_unioremap_emi(void)
{
	if (g_step_env.emi_base_addr != NULL) {
		iounmap(g_step_env.emi_base_addr);
		g_step_env.emi_base_addr = NULL;
	}
}

static unsigned char *mtk_step_get_emi_virt_addr(unsigned char *emi_base_addr, unsigned int offset)
{
	unsigned char *p_virtual_addr = NULL;

	if (offset > gConEmiSize) {
		WMT_ERR_FUNC("STEP failed: offset size %d over MAX size(%llu)\n", offset,
			gConEmiSize);
		return NULL;
	}
	p_virtual_addr = emi_base_addr + offset;

	return p_virtual_addr;
}

static int wmt_step_get_cfg(const char *p_patch_name, osal_firmware **pp_patch)
{
	osal_firmware *fw = NULL;

	*pp_patch = NULL;
	if (request_firmware((const struct firmware **)&fw, p_patch_name, NULL) != 0) {
		release_firmware(fw);
		return -1;
	}

	WMT_DBG_FUNC("Load step cfg %s ok!!\n", p_patch_name);
	*pp_patch = fw;

	return 0;
}

static void wmt_step_sleep_or_delay(int ms)
{
	/* msleep < 20ms can sleep for up to 20ms */
	if (ms < 20)
		udelay(ms * 1000);
	else
		osal_sleep_ms(ms);
}

static unsigned char wmt_step_to_upper(char str)
{
	if ((str >= 'a') && (str <= 'z'))
		return str + ('A' - 'a');
	else
		return str;
}

static void wmt_step_string_to_upper(char *tok)
{
	for (; *tok != '\0'; tok++)
		*tok = wmt_step_to_upper(*tok);
}

static int wmt_step_get_int_from_four_char(char *str)
{
	unsigned char char_array[4];
	int i;

	for (i = 0; i < 4; i++) {
		if (*(str + i) == '\0')
			return -1;

		char_array[i] = wmt_step_to_upper(*(str + i));
	}

	return *(int *)char_array;
}

static enum step_trigger_point_id wmt_step_parse_tp_id(char *str)
{
	long tp_id = STEP_TRIGGER_POINT_NO_DEFINE;

	if (osal_strtol(str, 10, &tp_id)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n", str);
		return STEP_TRIGGER_POINT_NO_DEFINE;
	}
	if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX)
		return STEP_TRIGGER_POINT_NO_DEFINE;

	return (enum step_trigger_point_id)tp_id;
}

static int wmt_step_parse_pd_expires(PINT8 ptr)
{
	long expires_ms;

	if (osal_strtol(ptr, 0, &expires_ms))
		return -1;

	return (int)expires_ms;
}

static int wmt_step_parse_act_id(char *str)
{
	int str_to_int = STEP_ACTION_INDEX_NO_DEFINE;

	if (str == NULL)
		return STEP_ACTION_INDEX_NO_DEFINE;

	str_to_int = wmt_step_get_int_from_four_char(str);
	if (str_to_int == STEP_EMI_ACT_INT)
		return STEP_ACTION_INDEX_EMI;
	else if (str_to_int == STEP_REG_ACT_INT)
		return STEP_ACTION_INDEX_REGISTER;
	else if (str_to_int == STEP_GPIO_ACT_INT)
		return STEP_ACTION_INDEX_GPIO;
	else if (str_to_int == STEP_DISABLE_RESET_ACT_INT)
		return STEP_ACTION_INDEX_DISABLE_RESET;
	else if (str_to_int == STEP_CHIP_RESET_ACT_INT)
		return STEP_ACTION_INDEX_CHIP_RESET;
	else if (str_to_int == STEP_KEEP_WAKEUP_ACT_INT)
		return STEP_ACTION_INDEX_KEEP_WAKEUP;
	else if (str_to_int == STEP_CANCEL_KEEP_WAKEUP_ACT_INT)
		return STEP_ACTION_INDEX_CANCEL_WAKEUP;
	else if (str_to_int == STEP_SHOW_STRING_ACT_INT)
		return STEP_ACTION_INDEX_SHOW_STRING;
	else if (str_to_int == STEP_SLEEP_ACT_INT)
		return STEP_ACTION_INDEX_SLEEP;
	else if (str_to_int == STEP_CONDITION_ACT_INT)
		return STEP_ACTION_INDEX_CONDITION;
	else if (str_to_int == STEP_VALUE_ACT_INT)
		return STEP_ACTION_INDEX_VALUE;
	else if (str_to_int == STEP_CONDITION_EMI_ACT_INT)
		return STEP_ACTION_INDEX_CONDITION_EMI;
	else if (str_to_int == STEP_CONDITION_REG_ACT_INT)
		return STEP_ACTION_INDEX_CONDITION_REGISTER;
	else
		return STEP_ACTION_INDEX_NO_DEFINE;

}

static struct step_action_list *wmt_step_get_tp_list(int tp_id)
{
	if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX) {
		WMT_ERR_FUNC("STEP failed: Write action to tp_id: %d\n", tp_id);
		return NULL;
	}

	return &g_step_env.actions[tp_id];
}

#define STEP_PARSE_LINE_RET_CONTINUE 0
#define STEP_PARSE_LINE_RET_BREAK 1

static void wmt_step_set_line_state(int *p_state, int value)
{
	*p_state = value;
}

static int wmt_step_access_line_state_init(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	wmt_step_string_to_upper(tok);
	if (osal_strcmp(tok, "[TP") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_TP);
		return STEP_PARSE_LINE_RET_CONTINUE;
	}

	if (p_parse_info->tp_id == STEP_TRIGGER_POINT_NO_DEFINE) {
		WMT_ERR_FUNC("STEP failed: Set trigger point first: %s\n", tok);
		return STEP_PARSE_LINE_RET_BREAK;
	}

	if (osal_strcmp(tok, "[PD+]") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_PD_START);
		return STEP_PARSE_LINE_RET_CONTINUE;
	}

	if (osal_strcmp(tok, "[PD-]") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_PD_END);
		return STEP_PARSE_LINE_RET_BREAK;
	}

	if (osal_strcmp(tok, "[AT]") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_AT);
		return STEP_PARSE_LINE_RET_CONTINUE;
	}

	return STEP_PARSE_LINE_RET_BREAK;
}

static int wmt_step_access_line_state_tp(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	char *pch = NULL;

	if (p_parse_info->p_pd_entry != NULL) {
		WMT_ERR_FUNC("STEP failed: Please add [PD-] after [PD+], tok = %s\n", tok);
		p_parse_info->p_pd_entry = NULL;
	}

	pch = osal_strchr(tok, ']');
	if (pch == NULL) {
		WMT_ERR_FUNC("STEP failed: Trigger point format is wrong: %s\n", tok);
	} else {
		*pch = '\0';
		p_parse_info->tp_id = wmt_step_parse_tp_id(tok);
		p_parse_info->p_target_list = wmt_step_get_tp_list(p_parse_info->tp_id);

		if (p_parse_info->tp_id == STEP_TRIGGER_POINT_NO_DEFINE)
			WMT_ERR_FUNC("STEP failed: Trigger point no define: %s\n", tok);
	}

	return STEP_PARSE_LINE_RET_BREAK;
}

static int wmt_step_access_line_state_at(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	p_parse_line_info->act_id = wmt_step_parse_act_id(tok);
	if (p_parse_line_info->act_id == STEP_ACTION_INDEX_NO_DEFINE) {
		WMT_ERR_FUNC("STEP failed: Action no define: %s\n", tok);
		return STEP_PARSE_LINE_RET_BREAK;
	}
	wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_AT_OP);

	return STEP_PARSE_LINE_RET_CONTINUE;
}

static int wmt_step_access_line_state_at_op(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	if (p_parse_line_info->param_index >= 0)
		p_parse_line_info->act_params[p_parse_line_info->param_index] = tok;
	(p_parse_line_info->param_index)++;

	if (p_parse_line_info->param_index >= STEP_PARAMETER_SIZE) {
		WMT_ERR_FUNC("STEP failed: Param too much");
		return STEP_PARSE_LINE_RET_BREAK;
	}

	return STEP_PARSE_LINE_RET_CONTINUE;
}

static int wmt_step_access_line_state_pd(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	int pd_ms = -1;

	pd_ms = wmt_step_parse_pd_expires(tok);
	if (pd_ms == -1)
		WMT_ERR_FUNC("STEP failed: PD ms failed %s\n", tok);

	if (p_parse_info->p_pd_entry != NULL)
		WMT_ERR_FUNC("STEP failed: Please add [PD-] after [PD+], tok = %s\n", tok);

	p_parse_info->p_pd_entry = wmt_step_get_periodic_dump_entry(pd_ms);
	if (p_parse_info->p_pd_entry == NULL)
		WMT_ERR_FUNC("STEP failed: p_pd_entry create fail\n");
	else
		p_parse_info->p_target_list = &(p_parse_info->p_pd_entry->action_list);

	return STEP_PARSE_LINE_RET_BREAK;
}

static void wmt_step_parse_line_data(char *line, struct step_target_act_list_info *p_parse_info,
	STEP_WRITE_ACT_TO_LIST func_act_to_list)
{
	char *tok;
	int line_ret = STEP_PARSE_LINE_RET_BREAK;
	struct step_parse_line_data_param_info parse_line_info;

	parse_line_info.param_index = 0;
	parse_line_info.act_id = STEP_ACTION_INDEX_NO_DEFINE;
	parse_line_info.state = STEP_PARSE_LINE_STATE_INIT;

	while ((tok = osal_strsep(&line, " \t")) != NULL) {
		if (*tok == '\0')
			continue;
		if (osal_strcmp(tok, "//") == 0)
			break;

		if (wmt_step_line_state_action_map[parse_line_info.state] != NULL) {
			line_ret = wmt_step_line_state_action_map[parse_line_info.state] (tok,
				p_parse_info, &parse_line_info);
		}

		if (line_ret == STEP_PARSE_LINE_RET_CONTINUE)
			continue;
		else
			break;
	}

	if (parse_line_info.state == STEP_PARSE_LINE_STATE_AT_OP) {
		func_act_to_list(p_parse_info->p_target_list,
			parse_line_info.act_id, parse_line_info.param_index, parse_line_info.act_params);
	} else if (parse_line_info.state == STEP_PARSE_LINE_STATE_PD_END) {
		p_parse_info->p_target_list = wmt_step_get_tp_list(p_parse_info->tp_id);
		if (p_parse_info->p_pd_entry != NULL) {
			parse_line_info.act_params[0] = (PINT8)p_parse_info->p_pd_entry;
			func_act_to_list(p_parse_info->p_target_list,
				STEP_ACTION_INDEX_PERIODIC_DUMP, parse_line_info.param_index,
				parse_line_info.act_params);
			p_parse_info->p_pd_entry = NULL;
		}
	}
}

static void _wmt_step_do_actions(struct step_action_list *action_list)
{
	struct step_action *p_act = NULL, *p_act_next = NULL;

	list_for_each_entry_safe(p_act, p_act_next, &action_list->list, list) {
		if (p_act->action_id <= STEP_ACTION_INDEX_NO_DEFINE || p_act->action_id >= STEP_ACTION_INDEX_MAX) {
			WMT_ERR_FUNC("STEP failed: Wrong action id %d\n", (int)p_act->action_id);
			continue;
		}

		if (wmt_step_action_map[p_act->action_id].func_do_action != NULL)
			wmt_step_action_map[p_act->action_id].func_do_action(p_act, NULL);
		else
			WMT_ERR_FUNC("STEP failed: Action is NULL\n");
	}
}

static void wmt_step_start_work(struct step_pd_entry *p_entry)
{
	unsigned int timeout;
	int result = 0;

	if (!g_step_env.pd_struct.step_pd_wq) {
		WMT_ERR_FUNC("STEP failed: step wq doesn't run\n");
		result = -1;
	}

	if (p_entry == NULL) {
		WMT_ERR_FUNC("STEP failed: entry is null\n");
		result = -1;
	}

	if (result == 0) {
		timeout = p_entry->expires_ms;
		queue_delayed_work(g_step_env.pd_struct.step_pd_wq, &p_entry->pd_work, timeout);
	}
}

static void wmt_step_pd_work(struct work_struct *work)
{
	struct step_pd_entry *p_entry = NULL;
	struct delayed_work *delayed_work = NULL;
	int result = 0;

	if (down_read_trylock(&g_step_env.init_rwsem)) {
		if (!g_step_env.is_enable) {
			WMT_ERR_FUNC("STEP failed: step doesn`t enable\n");
			result = -1;
		}

		delayed_work = to_delayed_work(work);
		if (delayed_work == NULL) {
			WMT_ERR_FUNC("STEP failed: work is NULL\n");
			result = -1;
		}

		if (result == 0) {
			p_entry = container_of(delayed_work, struct step_pd_entry, pd_work);

			WMT_INFO_FUNC("STEP show: Periodic dump: %d ms\n", p_entry->expires_ms);
			_wmt_step_do_actions(&p_entry->action_list);
			wmt_step_start_work(p_entry);
		}

		up_read(&g_step_env.init_rwsem);
	}
}

static struct step_pd_entry *wmt_step_create_periodic_dump_entry(unsigned int expires)
{
	struct step_pd_entry *p_pd_entry = NULL;

	p_pd_entry = kzalloc(sizeof(struct step_pd_entry), GFP_KERNEL);
	if (p_pd_entry == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc fail\n");
		return NULL;
	}
	p_pd_entry->expires_ms = expires;

	INIT_DELAYED_WORK(&p_pd_entry->pd_work, wmt_step_pd_work);
	INIT_LIST_HEAD(&(p_pd_entry->action_list.list));

	return p_pd_entry;
}

static void wmt_step_print_trigger_time(enum step_trigger_point_id tp_id, char *reason)
{
	const char *p_trigger_name = NULL;

	if (tp_id < 0)
		return;

	p_trigger_name = STEP_TRIGGER_TIME_NAME[tp_id];
	if (reason != NULL)
		WMT_INFO_FUNC("STEP show: Trigger point: %s reason: %s\n", p_trigger_name, reason);
	else
		WMT_INFO_FUNC("STEP show: Trigger point: %s\n", p_trigger_name);
}

static VOID wmt_step_do_actions_from_tp(enum step_trigger_point_id tp_id, char *reason)
{
	int result = 0;

	if (down_read_trylock(&g_step_env.init_rwsem)) {
		if (g_step_env.is_enable == 0)
			result = -1;

		if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX) {
			WMT_ERR_FUNC("STEP failed: Do actions from tp_id: %d\n", tp_id);
			result = -1;
		} else if (list_empty(&g_step_env.actions[tp_id].list)) {
			result = -1;
		}

		if (result == 0) {
			wmt_step_print_trigger_time(tp_id, reason);
			_wmt_step_do_actions(&g_step_env.actions[tp_id]);
		}

		up_read(&g_step_env.init_rwsem);
	}
}

static int wmt_step_write_action(struct step_action_list *p_list, enum step_action_id act_id,
	int param_num, char *params[])
{
	struct step_action *p_action = NULL;

	if (p_list == NULL) {
		WMT_ERR_FUNC("STEP failed: p_list is null\n");
		return -1;
	}

	p_action = wmt_step_create_action(act_id, param_num, params);
	if (p_action != NULL) {
		list_add_tail(&(p_action->list), &(p_list->list));
		return 0;
	}

	return -1;
}

static int wmt_step_parse_number_with_symbol(char *ptr, long *value)
{
	int ret = STEP_VALUE_INFO_UNKNOWN;

	if (*ptr == '#') {
		if (osal_strtol(ptr + 1, 10, value)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n", ptr);
			ret = STEP_VALUE_INFO_UNKNOWN;
		} else {
			ret = STEP_VALUE_INFO_SYMBOL_REG_BASE;
		}
	} else if (*ptr == '$') {
		if (osal_strtol(ptr + 1, 10, value)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n", ptr);
			ret = STEP_VALUE_INFO_UNKNOWN;
		} else {
			ret = STEP_VALUE_INFO_SYMBOL_TEMP_REG;
		}
	} else {
		if (osal_strtol(ptr, 0, value)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n", ptr);
			ret = STEP_VALUE_INFO_UNKNOWN;
		} else {
			ret = STEP_VALUE_INFO_NUMBER;
		}
	}

	return ret;
}

static int wmt_step_parse_register_address(struct step_reg_addr_info *p_reg_addr, char *ptr, long offset)
{
	unsigned long res;
	unsigned int symbol;
	int num_sym;

	num_sym = wmt_step_parse_number_with_symbol(ptr, &res);
	if (num_sym == STEP_VALUE_INFO_SYMBOL_REG_BASE) {
		symbol = (unsigned int) res;
		if (symbol <= STEP_REGISTER_PHYSICAL_ADDRESS || symbol >= STEP_REGISTER_MAX) {
			WMT_ERR_FUNC("STEP failed: No support the base %s\n", ptr);
			return -1;
		}
		res = g_step_env.reg_base[symbol].vir_addr;

		if (res == 0) {
			WMT_ERR_FUNC("STEP failed: No support the base %s is 0\n", ptr);
			return -1;
		}

		if (offset >= g_step_env.reg_base[symbol].size) {
			WMT_ERR_FUNC("STEP failed: symbol(%d), offset(%d) over max size(%llu)\n",
				symbol, (int) offset, g_step_env.reg_base[symbol].size);
			return -1;
		}

		p_reg_addr->address = res;
		p_reg_addr->address_type = symbol;
	} else if (num_sym == STEP_VALUE_INFO_NUMBER) {
		p_reg_addr->address = res;
		p_reg_addr->address_type = STEP_REGISTER_PHYSICAL_ADDRESS;
	} else {
		WMT_ERR_FUNC("STEP failed: number with symbol parse fail %s\n", ptr);
		return -1;
	}

	return 0;
}

static unsigned int wmt_step_parse_temp_register_id(char *ptr)
{
	unsigned long res;
	int num_sym;

	num_sym = wmt_step_parse_number_with_symbol(ptr, &res);

	if (num_sym == STEP_VALUE_INFO_SYMBOL_TEMP_REG)
		return res;
	else
		return STEP_TEMP_REGISTER_SIZE;
}

static enum step_condition_operator_id wmt_step_parse_operator_id(char *ptr)
{
	if (osal_strcmp(ptr, ">") == 0)
		return STEP_OPERATOR_GREATER;
	else if (osal_strcmp(ptr, ">=") == 0)
		return STEP_OPERATOR_GREATER_EQUAL;
	else if (osal_strcmp(ptr, "<") == 0)
		return STEP_OPERATOR_LESS;
	else if (osal_strcmp(ptr, "<=") == 0)
		return STEP_OPERATOR_LESS_EQUAL;
	else if (osal_strcmp(ptr, "==") == 0)
		return STEP_OPERATOR_EQUAL;
	else if (osal_strcmp(ptr, "!=") == 0)
		return STEP_OPERATOR_NOT_EQUAL;
	else if (osal_strcmp(ptr, "&&") == 0)
		return STEP_OPERATOR_AND;
	else if (osal_strcmp(ptr, "||") == 0)
		return STEP_OPERATOR_OR;

	return STEP_OPERATOR_MAX;
}

static int wmt_step_operator_result_greater(int l_val, int r_val)
{
	return (l_val > r_val);
}

static int wmt_step_operator_result_greater_equal(int l_val, int r_val)
{
	return (l_val >= r_val);
}

static int wmt_step_operator_result_less(int l_val, int r_val)
{
	return (l_val < r_val);
}

static int wmt_step_operator_result_less_equal(int l_val, int r_val)
{
	return (l_val <= r_val);
}

static int wmt_step_operator_result_equal(int l_val, int r_val)
{
	return (l_val == r_val);
}

static int wmt_step_operator_result_not_equal(int l_val, int r_val)
{
	return (l_val != r_val);
}

static int wmt_step_operator_result_and(int l_val, int r_val)
{
	return (l_val && r_val);
}

static int wmt_step_operator_result_or(int l_val, int r_val)
{
	return (l_val || r_val);
}

static char *wmt_step_save_params_msg(int num, char *params[], char *buf, int buf_size)
{
	int i, len, temp;

	for (i = 0, len = 0; i < num; i++) {
		if (params[i] == NULL)
			break;

		temp = osal_strlen(params[i]) + 1;

		if ((len + temp) >= (buf_size - 1))
			break;

		len += temp;
		osal_strncat(buf, params[i], temp);
		osal_strncat(buf, " ", 1);
	}
	osal_strncat(buf, "\0", 1);

	return buf;
}

static void wmt_step_create_emi_output_log(struct step_emi_info *p_emi_info, int write,
	unsigned int begin, unsigned int end)
{
	p_emi_info->is_write = write;
	p_emi_info->begin_offset = begin;
	p_emi_info->end_offset = end;
	p_emi_info->output_mode = STEP_OUTPUT_LOG;
}

static void wmt_step_create_emi_output_register(struct step_emi_info *p_emi_info, int write,
	unsigned int begin, unsigned int mask, unsigned int reg_id)
{
	p_emi_info->is_write = write;
	p_emi_info->begin_offset = begin;
	p_emi_info->end_offset = begin + 0x4;
	p_emi_info->mask = mask;
	p_emi_info->temp_reg_id = reg_id;
	p_emi_info->output_mode = STEP_OUTPUT_REGISTER;
}

static int _wmt_step_create_emi_action(struct step_emi_info *p_emi_info, int param_num, char *params[])
{
	long write, begin, end;
	unsigned int reg_id;
	char buf[128] = "";
	long mask = 0xFFFFFFFF;

	if (param_num < 3) {
		WMT_ERR_FUNC("STEP failed: Init EMI to log param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return -1;
	}

	if (osal_strtol(params[0], 0, &write) ||
		osal_strtol(params[1], 0, &begin)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return -1;
	}

	if (*params[(param_num - 1)] == STEP_TEMP_REGISTER_SYMBOL) {
		reg_id = wmt_step_parse_temp_register_id(params[(param_num - 1)]);
		if (reg_id >= STEP_PARAMETER_SIZE) {
			WMT_ERR_FUNC("STEP failed: register id failed: %s\n",
				wmt_step_save_params_msg(param_num, params, buf, 128));
			return -1;
		}

		if (param_num > 3) {
			if (osal_strtol(params[2], 0, &mask)) {
				WMT_ERR_FUNC("STEP failed: str to value %s\n",
					wmt_step_save_params_msg(param_num, params, buf, 128));
				return -1;
			}
		}

		wmt_step_create_emi_output_register(p_emi_info, write, begin, mask, reg_id);
	} else {
		if (osal_strtol(params[2], 0, &end)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n",
				wmt_step_save_params_msg(param_num, params, buf, 128));
			return -1;
		}
		wmt_step_create_emi_output_log(p_emi_info, write, begin, end);
	}

	return 0;
}

/*
 * Support:
 * _EMI | R(0) | Begin offset | End offset
 * _EMI | R(0) | Begin offset | mask | Output temp register ID ($)
 * _EMI | R(0) | Begin offset | Output temp register ID ($)
 */
static struct step_action *wmt_step_create_emi_action(int param_num, char *params[])
{
	struct step_emi_action *p_emi_act = NULL;
	struct step_emi_info *p_emi_info = NULL;
	int ret;

	p_emi_act = kzalloc(sizeof(struct step_emi_action), GFP_KERNEL);
	if (p_emi_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc emi fail\n");
		return NULL;
	}

	p_emi_info = &p_emi_act->info;
	ret = _wmt_step_create_emi_action(p_emi_info, param_num, params);

	if (ret != 0) {
		kfree(p_emi_act);
		return NULL;
	}

	return &(p_emi_act->base);
}

/*
 * Support:
 * CEMI | Check temp register ID (#) | R(0) | Begin offset | End offset
 * CEMI | Check temp register ID (#) | R(0) | Begin offset | mask | Output temp register ID ($)
 * CEMI | Check temp register ID (#) | R(0) | Begin offset | Output temp register ID ($)
 */
static struct step_action *wmt_step_create_condition_emi_action(int param_num, char *params[])
{
	struct step_condition_emi_action *p_cond_emi_act = NULL;
	struct step_emi_info *p_emi_info = NULL;
	unsigned int reg_id;
	char buf[128] = "";
	int ret;

	if (param_num < 1) {
		WMT_ERR_FUNC("STEP failed: EMI no params\n");
		return NULL;
	}

	reg_id = wmt_step_parse_temp_register_id(params[0]);
	if (reg_id >= STEP_PARAMETER_SIZE) {
		WMT_ERR_FUNC("STEP failed: condition register id failed: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	p_cond_emi_act = kzalloc(sizeof(struct step_condition_emi_action), GFP_KERNEL);
	if (p_cond_emi_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc condition emi fail\n");
		return NULL;
	}

	p_emi_info = &p_cond_emi_act->info;
	p_cond_emi_act->cond_reg_id = reg_id;
	ret = _wmt_step_create_emi_action(p_emi_info, param_num - 1, &params[1]);

	if (ret != 0) {
		kfree(p_cond_emi_act);
		return NULL;
	}

	return &(p_cond_emi_act->base);
}

static void wmt_step_create_read_register_output_register(struct step_reigster_info *p_reg_info,
	struct step_reg_addr_info reg_addr_info, unsigned int offset, int mask, unsigned int reg_id)
{
	p_reg_info->is_write = 0;
	p_reg_info->address_type = reg_addr_info.address_type;
	p_reg_info->address = reg_addr_info.address;
	p_reg_info->offset = offset;
	p_reg_info->times = 1;
	p_reg_info->delay_time = 0;
	p_reg_info->mask = mask;
	p_reg_info->temp_reg_id = reg_id;
	p_reg_info->output_mode = STEP_OUTPUT_REGISTER;
}

static void wmt_step_create_read_register_output_log(struct step_reigster_info *p_reg_info,
	struct step_reg_addr_info reg_addr_info, unsigned int offset, unsigned int times, unsigned int delay_time)
{
	p_reg_info->is_write = 0;
	p_reg_info->address_type = reg_addr_info.address_type;
	p_reg_info->address = reg_addr_info.address;
	p_reg_info->offset = offset;
	p_reg_info->times = times;
	p_reg_info->delay_time = delay_time;
	p_reg_info->output_mode = STEP_OUTPUT_LOG;
}

static void wmt_step_create_write_register_action(struct step_reigster_info *p_reg_info,
	struct step_reg_addr_info reg_addr_info, unsigned int offset, int value, int mask)
{
	p_reg_info->is_write = 1;
	p_reg_info->address_type = reg_addr_info.address_type;
	p_reg_info->address = reg_addr_info.address;
	p_reg_info->offset = offset;
	p_reg_info->value = value;
	p_reg_info->mask = mask;
}

static int _wmt_step_create_register_action(struct step_reigster_info *p_reg_info,
	int param_num, char *params[])
{
	long write;
	struct step_reg_addr_info reg_addr_info;
	long offset, value;
	unsigned int reg_id = 0;
	char buf[128] = "";
	long mask = 0xFFFFFFFF;
	long times = 1;
	long delay_time = 0;

	if (param_num < 4) {
		WMT_ERR_FUNC("STEP failed: Register no params\n");
		return -1;
	}

	if (osal_strtol(params[0], 0, &write)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			params[0]);
		return -1;
	}

	if (osal_strtol(params[2], 0, &offset)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return -1;
	}

	if (wmt_step_parse_register_address(&reg_addr_info, params[1], offset) == -1) {
		WMT_ERR_FUNC("STEP failed: init write register symbol: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return -1;
	}

	if (write == 0) {
		if (*params[(param_num - 1)] == STEP_TEMP_REGISTER_SYMBOL) {
			reg_id = wmt_step_parse_temp_register_id(params[(param_num - 1)]);
			if (reg_id >= STEP_PARAMETER_SIZE) {
				WMT_ERR_FUNC("STEP failed: register id failed: %s\n",
					wmt_step_save_params_msg(param_num, params, buf, 128));
				return -1;
			}

			if (param_num > 4) {
				if (osal_strtol(params[3], 0, &mask)) {
					WMT_ERR_FUNC("STEP failed: str to value %s\n",
						wmt_step_save_params_msg(param_num, params, buf, 128));
					return -1;
				}
			}

			wmt_step_create_read_register_output_register(p_reg_info, reg_addr_info, offset, mask, reg_id);
		} else {
			if (param_num < 5 ||
				osal_strtol(params[3], 0, &times) ||
				osal_strtol(params[4], 0, &delay_time)) {
				WMT_ERR_FUNC("STEP failed: str to value %s\n",
					wmt_step_save_params_msg(param_num, params, buf, 128));
				return -1;
			}

			wmt_step_create_read_register_output_log(p_reg_info, reg_addr_info, offset, times, delay_time);
		}
	} else {
		if (osal_strtol(params[3], 0, &value)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n",
				wmt_step_save_params_msg(param_num, params, buf, 128));
			return -1;
		}

		if (param_num > 4) {
			if (osal_strtol(params[4], 0, &mask)) {
				WMT_ERR_FUNC("STEP failed: str to value %s\n",
					wmt_step_save_params_msg(param_num, params, buf, 128));
				return -1;
			}
		}

		wmt_step_create_write_register_action(p_reg_info, reg_addr_info, offset, value, mask);
	}

	return 0;
}

/*
 * Support:
 * _REG | R(0) | Pre-define base address ID | offset | times | delay time(ms)
 * _REG | R(0) | AP Physical address        | offset | times | delay time(ms)
 * _REG | R(0) | Pre-define base address ID | offset | mask  | Output temp register ID ($)
 * _REG | R(0) | AP Physical address        | offset | mask  | Output temp register ID ($)
 * _REG | R(0) | Pre-define base address ID | offset | Output temp register ID ($)
 * _REG | R(0) | AP Physical address        | offset | Output temp register ID ($)
 * _REG | W(1) | AP Physical address        | offset | value
 * _REG | W(1) | AP Physical address        | offset | value
 * _REG | W(1) | AP Physical address        | offset | value | mask
 * _REG | W(1) | AP Physical address        | offset | value | mask
 */
static struct step_action *wmt_step_create_register_action(int param_num, char *params[])
{
	struct step_register_action *p_reg_act = NULL;
	struct step_reigster_info *p_reg_info = NULL;
	int ret;

	p_reg_act = kzalloc(sizeof(struct step_register_action), GFP_KERNEL);
	if (p_reg_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc register fail\n");
		return NULL;
	}

	p_reg_info = &p_reg_act->info;
	ret = _wmt_step_create_register_action(p_reg_info, param_num, params);

	if (ret != 0) {
		kfree(p_reg_act);
		return NULL;
	}

	return &(p_reg_act->base);
}

/*
 * Support:
 * CREG | Check temp register ID (#) | R(0) | Pre-define base address ID | offset | times | delay time(ms)
 * CREG | Check temp register ID (#) | R(0) | AP Physical address        | offset | times | delay time(ms)
 * CREG | Check temp register ID (#) | R(0) | Pre-define base address ID | offset | mask  | Output temp register ID ($)
 * CREG | Check temp register ID (#) | R(0) | AP Physical address        | offset | mask  | Output temp register ID ($)
 * CREG | Check temp register ID (#) | R(0) | Pre-define base address ID | offset | Output temp register ID ($)
 * CREG | Check temp register ID (#) | R(0) | AP Physical address        | offset | Output temp register ID ($)
 * CREG | Check temp register ID (#) | W(1) | AP Physical address        | offset | value
 * CREG | Check temp register ID (#) | W(1) | AP Physical address        | offset | value
 * CREG | Check temp register ID (#) | W(1) | AP Physical address        | offset | value | mask
 * CREG | Check temp register ID (#) | W(1) | AP Physical address        | offset | value | mask
 */
static struct step_action *wmt_step_create_condition_register_action(int param_num, char *params[])
{
	struct step_condition_register_action *p_cond_reg_act = NULL;
	struct step_reigster_info *p_reg_info = NULL;
	unsigned int reg_id;
	char buf[128] = "";
	int ret;

	if (param_num < 0) {
		WMT_ERR_FUNC("STEP failed: Init EMI param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	reg_id = wmt_step_parse_temp_register_id(params[0]);
	if (reg_id >= STEP_PARAMETER_SIZE) {
		WMT_ERR_FUNC("STEP failed: condition register id failed: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	p_cond_reg_act = kzalloc(sizeof(struct step_condition_register_action), GFP_KERNEL);
	if (p_cond_reg_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc condition register fail\n");
		return NULL;
	}

	p_reg_info = &p_cond_reg_act->info;
	p_cond_reg_act->cond_reg_id = reg_id;
	ret = _wmt_step_create_register_action(p_reg_info, param_num - 1, &params[1]);

	if (ret != 0) {
		kfree(p_cond_reg_act);
		return NULL;
	}

	return &(p_cond_reg_act->base);
}

/*
 * Support:
 * GPIO | R(0) | Pin number
 */
static struct step_action *wmt_step_create_gpio_action(int param_num, char *params[])
{
	struct step_gpio_action *p_gpio_act = NULL;
	long write, symbol;
	char buf[128] = "";

	if (param_num != 2) {
		WMT_ERR_FUNC("STEP failed: init gpio param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	if (osal_strtol(params[0], 0, &write) ||
		osal_strtol(params[1], 0, &symbol)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	p_gpio_act = kzalloc(sizeof(struct step_gpio_action), GFP_KERNEL);
	if (p_gpio_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc gpio fail\n");
		return NULL;
	}
	p_gpio_act->is_write = write;
	p_gpio_act->pin_symbol = symbol;

	return &(p_gpio_act->base);
}

/*
 * Support:
 * DRST
 */
static struct step_action *wmt_step_create_disable_reset_action(int param_num, char *params[])
{
	struct step_disable_reset_action *p_drst_act = NULL;

	p_drst_act = kzalloc(sizeof(struct step_disable_reset_action), GFP_KERNEL);
	if (p_drst_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc disalbe reset fail\n");
		return NULL;
	}

	return &(p_drst_act->base);
}

/*
 * Support:
 * _RST
 */
static struct step_action *wmt_step_create_chip_reset_action(int param_num, char *params[])
{
	struct step_chip_reset_action *p_crst_act = NULL;

	p_crst_act = kzalloc(sizeof(struct step_chip_reset_action), GFP_KERNEL);
	if (p_crst_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc chip reset fail\n");
		return NULL;
	}

	return &(p_crst_act->base);
}

/*
 * Support:
 * WAK+
 */
static struct step_action *wmt_step_create_keep_wakeup_action(int param_num, char *params[])
{
	struct step_keep_wakeup_action *p_kwak_act = NULL;

	p_kwak_act = kzalloc(sizeof(struct step_keep_wakeup_action), GFP_KERNEL);
	if (p_kwak_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc keep wakeup fail\n");
		return NULL;
	}

	return &(p_kwak_act->base);
}

/*
 * Support:
 * WAK-
 */
static struct step_action *wmt_step_create_cancel_wakeup_action(int param_num, char *params[])
{
	struct step_cancel_wakeup_action *p_cwak_act = NULL;

	p_cwak_act = kzalloc(sizeof(struct step_cancel_wakeup_action), GFP_KERNEL);
	if (p_cwak_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc cancel wakeup fail\n");
		return NULL;
	}

	return &(p_cwak_act->base);
}

/*
 * Support:
 * [PD+] | ms
 */
static struct step_action *wmt_step_create_periodic_dump_action(int param_num, char *params[])
{
	struct step_periodic_dump_action *p_pd_act = NULL;

	if (params[0] == NULL) {
		WMT_ERR_FUNC("STEP failed: param null\n");
		return NULL;
	}

	p_pd_act = kzalloc(sizeof(struct step_periodic_dump_action), GFP_KERNEL);
	if (p_pd_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc fail\n");
		return NULL;
	}

	p_pd_act->pd_entry = (struct step_pd_entry *)params[0];
	return &(p_pd_act->base);
}

/*
 * Support:
 * SHOW | Message (no space)
 */
static struct step_action *wmt_step_create_show_string_action(int param_num, char *params[])
{
	struct step_show_string_action *p_show_act = NULL;
	char buf[128] = "";

	if (param_num != 1) {
		WMT_ERR_FUNC("STEP failed: init show param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	p_show_act = kzalloc(sizeof(struct step_show_string_action), GFP_KERNEL);
	if (p_show_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc show string fail\n");
		return NULL;
	}

	p_show_act->content = kzalloc((osal_strlen(params[0]) + 1), GFP_KERNEL);
	if (p_show_act->content == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc show string content fail\n");
		kfree(p_show_act);
		return NULL;
	}
	osal_memcpy(p_show_act->content, params[0], osal_strlen(params[0]));
	return &(p_show_act->base);
}

/*
 * Support:
 * _SLP | time (ms)
 */
static struct step_action *wmt_step_create_sleep_action(int param_num, char *params[])
{
	struct step_sleep_action *p_sleep_act = NULL;
	long ms;
	char buf[128] = "";

	if (param_num != 1) {
		WMT_ERR_FUNC("STEP failed: init sleep param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	if (osal_strtol(params[0], 0, &ms)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			params[0]);
		return NULL;
	}

	p_sleep_act = kzalloc(sizeof(struct step_sleep_action), GFP_KERNEL);
	if (p_sleep_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc sleep fail\n");
		return NULL;
	}
	p_sleep_act->ms = ms;

	return &(p_sleep_act->base);
}

/*
 * Support:
 * COND | Check temp register ID ($) | Left temp register ID ($) | Operator | Right temp register ID ($)
 * COND | Check temp register ID ($) | Left temp register ID ($) | Operator | value
 */
static struct step_action *wmt_step_create_condition_action(int param_num, char *params[])
{
	struct step_condition_action *p_cond_act = NULL;
	unsigned int res_reg_id, l_reg_id, r_reg_id = 0;
	long value = 0;
	int mode;
	enum step_condition_operator_id op_id;
	char buf[128] = "";

	if (param_num != 4) {
		WMT_ERR_FUNC("STEP failed: init sleep param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	res_reg_id = wmt_step_parse_temp_register_id(params[0]);
	l_reg_id = wmt_step_parse_temp_register_id(params[1]);
	if (res_reg_id >= STEP_PARAMETER_SIZE || l_reg_id >= STEP_PARAMETER_SIZE) {
		WMT_ERR_FUNC("STEP failed: register id failed: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	op_id = wmt_step_parse_operator_id(params[2]);
	if (op_id >= STEP_OPERATOR_MAX) {
		WMT_ERR_FUNC("STEP failed: operator id failed: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	if (*params[(param_num - 1)] == STEP_TEMP_REGISTER_SYMBOL) {
		r_reg_id = wmt_step_parse_temp_register_id(params[3]);
		mode = STEP_CONDITION_RIGHT_REGISTER;
		if (r_reg_id >= STEP_PARAMETER_SIZE) {
			WMT_ERR_FUNC("STEP failed: register id failed: %s\n",
				wmt_step_save_params_msg(param_num, params, buf, 128));
			return NULL;
		}
	} else {
		if (osal_strtol(params[3], 0, &value)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n",
				wmt_step_save_params_msg(param_num, params, buf, 128));
			return NULL;
		}
		mode = STEP_CONDITION_RIGHT_VALUE;
	}

	p_cond_act = kzalloc(sizeof(struct step_condition_action), GFP_KERNEL);
	if (p_cond_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc condition fail\n");
		return NULL;
	}

	p_cond_act->result_temp_reg_id = res_reg_id;
	p_cond_act->l_temp_reg_id = l_reg_id;
	p_cond_act->operator_id = op_id;
	p_cond_act->r_temp_reg_id = r_reg_id;
	p_cond_act->value = value;
	p_cond_act->mode = mode;

	return &(p_cond_act->base);
}

/*
 * Support:
 * _VAL | Save temp register ID ($) | Value
 */
static struct step_action *wmt_step_create_value_action(int param_num, char *params[])
{
	struct step_value_action *p_val_act = NULL;
	unsigned int reg_id;
	long value;
	char buf[128] = "";

	if (param_num != 2) {
		WMT_ERR_FUNC("STEP failed: init sleep param(%d): %s\n", param_num,
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	reg_id = wmt_step_parse_temp_register_id(params[0]);
	if (reg_id >= STEP_PARAMETER_SIZE) {
		WMT_ERR_FUNC("STEP failed: register id failed: %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	if (osal_strtol(params[1], 0, &value)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			wmt_step_save_params_msg(param_num, params, buf, 128));
		return NULL;
	}

	p_val_act = kzalloc(sizeof(struct step_value_action), GFP_KERNEL);
	if (p_val_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc value fail\n");
		return NULL;
	}

	p_val_act->temp_reg_id = reg_id;
	p_val_act->value = value;

	return &(p_val_act->base);
}

static int wmt_step_do_write_register_action(struct step_reigster_info *p_reg_info,
	STEP_DO_EXTRA func_do_extra)
{
	phys_addr_t phy_addr;
	void __iomem *p_addr = NULL;
	SIZE_T vir_addr;

	if (p_reg_info->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_info->address + p_reg_info->offset;
		if (phy_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: phy_addr(0x%08x) page failed\n",
					(unsigned int)phy_addr);
			return -1;
		}

		p_addr = ioremap_nocache(phy_addr, 0x4);
		if (p_addr) {
			CONSYS_REG_WRITE_MASK((unsigned int *)p_addr, p_reg_info->value, p_reg_info->mask);
			WMT_INFO_FUNC(
				"STEP show: reg write Phy addr(0x%08x): 0x%08x\n",
				(unsigned int)phy_addr, CONSYS_REG_READ(p_addr));
			if (func_do_extra != NULL)
				func_do_extra(1, CONSYS_REG_READ(p_addr));
			iounmap(p_addr);
		} else {
			WMT_ERR_FUNC("STEP failed: ioremap(0x%08x) is NULL\n",
					(unsigned int)phy_addr);
			return -1;
		}
	} else {
		vir_addr = p_reg_info->address + p_reg_info->offset;
		if (vir_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: vir_addr(0x%08x) page failed\n",
					(unsigned int)vir_addr);
			return -1;
		}

		CONSYS_REG_WRITE_MASK((unsigned int *)vir_addr, p_reg_info->value, p_reg_info->mask);
		WMT_INFO_FUNC(
			"STEP show: reg write (symbol, offset)(%d, 0x%08x): 0x%08x\n",
			p_reg_info->address_type, p_reg_info->offset,
			CONSYS_REG_READ(vir_addr));
		if (func_do_extra != NULL)
			func_do_extra(1, CONSYS_REG_READ(vir_addr));
	}

	return 0;
}

static void _wmt_step_do_read_register_action(struct step_reigster_info *p_reg_info,
	STEP_DO_EXTRA func_do_extra, char *info, int value)
{
	int i;

	for (i = 0; i < p_reg_info->times; i++) {
		if (i > 0)
			wmt_step_sleep_or_delay(p_reg_info->delay_time);

		if (p_reg_info->output_mode == STEP_OUTPUT_REGISTER)
			g_step_env.temp_register[p_reg_info->temp_reg_id] = value & p_reg_info->mask;
		else
			WMT_INFO_FUNC("%s", info);

		if (func_do_extra != NULL)
			func_do_extra(1, value);
	}
}

static int wmt_step_do_read_register_action(struct step_reigster_info *p_reg_info,
	STEP_DO_EXTRA func_do_extra)
{
#define WMT_STEP_REGISTER_ACTION_BUF_LEN 128
	phys_addr_t phy_addr;
	void __iomem *p_addr = NULL;
	SIZE_T vir_addr;
	char buf[WMT_STEP_REGISTER_ACTION_BUF_LEN];

	if (p_reg_info->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_info->address + p_reg_info->offset;
		if (phy_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: phy_addr(0x%08x) page failed\n",
					(unsigned int)phy_addr);
			return -1;
		}

		p_addr = ioremap_nocache(phy_addr, 0x4);
		if (p_addr) {
			if (snprintf(buf, WMT_STEP_REGISTER_ACTION_BUF_LEN,
				"STEP show: reg read Phy addr(0x%08x): 0x%08x\n",
				(unsigned int)phy_addr, CONSYS_REG_READ(p_addr)) < 0)
				WMT_INFO_FUNC("snprintf buf fail\n");
			else
				_wmt_step_do_read_register_action(p_reg_info, func_do_extra, buf,
					CONSYS_REG_READ(p_addr));
			iounmap(p_addr);
		} else {
			WMT_ERR_FUNC("STEP failed: ioremap(0x%08x) is NULL\n",
					(unsigned int)phy_addr);
			return -1;
		}
	} else {
		vir_addr = p_reg_info->address + p_reg_info->offset;
		if (vir_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: vir_addr(0x%08x) page failed\n",
					(unsigned int)vir_addr);
			return -1;
		}

		if (snprintf(buf, WMT_STEP_REGISTER_ACTION_BUF_LEN,
			"STEP show: reg read (symbol, offset)(%d, 0x%08x): 0x%08x\n",
			p_reg_info->address_type, p_reg_info->offset,
			CONSYS_REG_READ(vir_addr)) < 0)
			WMT_INFO_FUNC("snprintf buf fail\n");
		else
			_wmt_step_do_read_register_action(p_reg_info, func_do_extra, buf,
				CONSYS_REG_READ(vir_addr));
	}

	return 0;
}

static void wmt_step_remove_emi_action(struct step_action *p_act)
{
	struct step_emi_action *p_emi_act = NULL;

	p_emi_act = list_entry_action(emi, p_act);
	kfree(p_emi_act);
}

static void wmt_step_remove_register_action(struct step_action *p_act)
{
	struct step_register_action *p_reg_act = NULL;

	p_reg_act = list_entry_action(register, p_act);
	kfree(p_reg_act);
}

static void wmt_step_remove_gpio_action(struct step_action *p_act)
{
	struct step_gpio_action *p_gpio_act = NULL;

	p_gpio_act = list_entry_action(gpio, p_act);
	kfree(p_gpio_act);
}

static void wmt_step_remove_disable_reset_action(struct step_action *p_act)
{
	struct step_disable_reset_action *p_drst = NULL;

	p_drst = list_entry_action(disable_reset, p_act);
	kfree(p_drst);
}

static void wmt_step_remove_chip_reset_action(struct step_action *p_act)
{
	struct step_chip_reset_action *p_crst = NULL;

	p_crst = list_entry_action(chip_reset, p_act);
	kfree(p_crst);
}

static void wmt_step_remove_keep_wakeup_action(struct step_action *p_act)
{
	struct step_keep_wakeup_action *p_kwak = NULL;

	p_kwak = list_entry_action(keep_wakeup, p_act);
	kfree(p_kwak);
}

static void wmt_step_remove_cancel_wakeup_action(struct step_action *p_act)
{
	struct step_cancel_wakeup_action *p_cwak = NULL;

	p_cwak = list_entry_action(cancel_wakeup, p_act);
	kfree(p_cwak);
}

static void wmt_step_remove_periodic_dump_action(struct step_action *p_act)
{
	struct step_periodic_dump_action *p_pd = NULL;

	p_pd = list_entry_action(periodic_dump, p_act);
	kfree(p_pd);
}

static void wmt_step_remove_show_string_action(struct step_action *p_act)
{
	struct step_show_string_action *p_show = NULL;

	p_show = list_entry_action(show_string, p_act);
	if (p_show->content != NULL)
		kfree(p_show->content);

	kfree(p_show);
}

static void wmt_step_remove_sleep_action(struct step_action *p_act)
{
	struct step_sleep_action *p_sleep = NULL;

	p_sleep = list_entry_action(sleep, p_act);
	kfree(p_sleep);
}

static void wmt_step_remove_condition_action(struct step_action *p_act)
{
	struct step_condition_action *p_cond = NULL;

	p_cond = list_entry_action(condition, p_act);
	kfree(p_cond);
}

static void wmt_step_remove_value_action(struct step_action *p_act)
{
	struct step_value_action *p_val = NULL;

	p_val = list_entry_action(value, p_act);
	kfree(p_val);
}

static void wmt_step_remove_condition_emi_action(struct step_action *p_act)
{
	struct step_condition_emi_action *p_cond_emi_act = NULL;

	p_cond_emi_act = list_entry_action(condition_emi, p_act);
	kfree(p_cond_emi_act);
}

static void wmt_step_remove_condition_register_action(struct step_action *p_act)
{
	struct step_condition_register_action *p_cond_reg_act = NULL;

	p_cond_reg_act = list_entry_action(condition_register, p_act);
	kfree(p_cond_reg_act);
}

static int _wmt_step_do_emi_action(struct step_emi_info *p_emi_info, STEP_DO_EXTRA func_do_extra)
{
	unsigned char *p_emi_begin_addr = NULL, *p_emi_end_addr = NULL;
	unsigned char __iomem *emi_base_addr = NULL;
	unsigned int dis = 0, temp = 0, i = 0;

	if (p_emi_info->is_write != 0) {
		WMT_ERR_FUNC("STEP failed: Only support dump EMI region\n");
		return -1;
	}

	if (p_emi_info->begin_offset > p_emi_info->end_offset) {
		temp = p_emi_info->begin_offset;
		p_emi_info->begin_offset = p_emi_info->end_offset;
		p_emi_info->end_offset = temp;
	}
	dis = p_emi_info->end_offset - p_emi_info->begin_offset;

	emi_base_addr = wmt_step_get_emi_base_address();
	if (emi_base_addr == NULL) {
		WMT_ERR_FUNC("STEP failed: EMI base address is NULL\n");
		return -1;
	}

	if (p_emi_info->begin_offset & 0x3) {
		WMT_ERR_FUNC("STEP failed: begin offset(0x%08x) page failed\n",
			p_emi_info->begin_offset);
		return -1;
	}

	p_emi_begin_addr = mtk_step_get_emi_virt_addr(emi_base_addr, p_emi_info->begin_offset);
	p_emi_end_addr = mtk_step_get_emi_virt_addr(emi_base_addr, p_emi_info->end_offset);
	if (!p_emi_begin_addr) {
		WMT_ERR_FUNC("STEP failed: Get NULL begin virtual address 0x%08x\n",
			p_emi_info->begin_offset);
		return -1;
	}

	if (!p_emi_end_addr) {
		WMT_ERR_FUNC("STEP failed: Get NULL end virtual address 0x%08x\n",
			p_emi_info->end_offset);
		return -1;
	}

	for (i = 0; i < dis; i += 0x4) {
		if (p_emi_info->output_mode == STEP_OUTPUT_REGISTER) {
			g_step_env.temp_register[p_emi_info->temp_reg_id] =
				(CONSYS_REG_READ(p_emi_begin_addr + i) & p_emi_info->mask);
		} else {
			WMT_INFO_FUNC("STEP show: EMI action, Phy address(0x%08x): 0x%08x\n",
				(unsigned int) (gConEmiPhyBase + p_emi_info->begin_offset + i),
				CONSYS_REG_READ(p_emi_begin_addr + i));
		}

		if (func_do_extra != NULL)
			func_do_extra(1, CONSYS_REG_READ(p_emi_begin_addr + i));
	}

	return 0;
}

static bool wmt_step_reg_readable(struct step_reigster_info *p_reg_info)
{
	phys_addr_t phy_addr;
	SIZE_T vir_addr;

	if (p_reg_info->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_info->address + p_reg_info->offset;
		if (mtk_consys_is_connsys_reg(phy_addr))
			return wmt_lib_reg_readable();
		else
			return 1;

	} else {
		if (p_reg_info->address_type == STEP_REGISTER_CONN_MCU_CONFIG_BASE ||
		    p_reg_info->address_type == STEP_REGISTER_MISC_OFF_BASE ||
		    p_reg_info->address_type == STEP_REGISTER_CFG_ON_BASE ||
		    p_reg_info->address_type == STEP_REGISTER_HIF_ON_BASE ||
		    p_reg_info->address_type == STEP_MCU_TOP_MISC_ON_BASE ||
		    p_reg_info->address_type == STEP_CIRQ_BASE) {
			vir_addr = p_reg_info->address + p_reg_info->offset;
			return wmt_lib_reg_readable_by_addr(vir_addr);
		}
	}

	return 1;
}

int _wmt_step_do_register_action(struct step_reigster_info *p_reg_info, STEP_DO_EXTRA func_do_extra)
{
	int ret = 0;
	bool is_wakeup = g_step_env.is_keep_wakeup;

	if (is_wakeup == 1) {
		if (DISABLE_PSM_MONITOR())
			WMT_ERR_FUNC("STEP failed: Wake up, continue to show register\n");
	}

	if (wmt_lib_power_lock_trylock() == 0) {
		WMT_INFO_FUNC("STEP failed: can't get lock\n");
		if (is_wakeup == 1)
			ENABLE_PSM_MONITOR();
		return -1;
	}

	if (!wmt_step_reg_readable(p_reg_info)) {
		wmt_lib_power_lock_release();
		WMT_ERR_FUNC("STEP failed: register cant read (No clock)\n");
		if (is_wakeup == 1)
			ENABLE_PSM_MONITOR();

		return -2;
	}

	if (p_reg_info->is_write == 1)
		ret = wmt_step_do_write_register_action(p_reg_info, func_do_extra);
	else
		ret = wmt_step_do_read_register_action(p_reg_info, func_do_extra);
	wmt_lib_power_lock_release();

	if (is_wakeup == 1)
		ENABLE_PSM_MONITOR();

	return ret;
}

void wmt_step_setup(void)
{
	if (!g_step_env.is_setup) {
		g_step_env.is_setup = true;
		init_rwsem(&g_step_env.init_rwsem);
		wmt_step_init_register_base_size();
	}
}

/*******************************************************************************
 *              I N T E R N A L   F U N C T I O N S   W I T H   U T
********************************************************************************/
int wmt_step_do_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_emi_action *p_emi_act = NULL;
	struct step_emi_info *p_emi_info = NULL;

	p_emi_act = list_entry_action(emi, p_act);
	p_emi_info = &p_emi_act->info;
	return _wmt_step_do_emi_action(p_emi_info, func_do_extra);
}

int wmt_step_do_condition_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_condition_emi_action *p_cond_emi_act = NULL;
	struct step_emi_info *p_emi_info = NULL;

	p_cond_emi_act = list_entry_action(condition_emi, p_act);
	p_emi_info = &p_cond_emi_act->info;

	if (g_step_env.temp_register[p_cond_emi_act->cond_reg_id] == 0) {
		WMT_INFO_FUNC("STEP show: Dont do emi, condition %c%d is %d\n",
			STEP_TEMP_REGISTER_SYMBOL, p_emi_info->temp_reg_id,
			g_step_env.temp_register[p_cond_emi_act->cond_reg_id]);
		return -1;
	}

	return _wmt_step_do_emi_action(p_emi_info, func_do_extra);
}

int wmt_step_do_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_register_action *p_reg_act = NULL;
	struct step_reigster_info *p_reg_info = NULL;

	p_reg_act = list_entry_action(register, p_act);
	p_reg_info = &p_reg_act->info;

	return _wmt_step_do_register_action(p_reg_info, func_do_extra);
}

int wmt_step_do_condition_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_condition_register_action *p_cond_reg_act = NULL;
	struct step_reigster_info *p_reg_info = NULL;

	p_cond_reg_act = list_entry_action(condition_register, p_act);
	p_reg_info = &p_cond_reg_act->info;

	if (g_step_env.temp_register[p_cond_reg_act->cond_reg_id] == 0) {
		WMT_INFO_FUNC("STEP show: Dont do register, condition %c%d is %d\n",
			STEP_TEMP_REGISTER_SYMBOL, p_reg_info->temp_reg_id,
			g_step_env.temp_register[p_cond_reg_act->cond_reg_id]);
		return -1;
	}

	return _wmt_step_do_register_action(p_reg_info, func_do_extra);
}

int wmt_step_do_gpio_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_gpio_action *p_gpio_act = NULL;

	p_gpio_act = list_entry_action(gpio, p_act);
	if (p_gpio_act->is_write == 1) {
		WMT_ERR_FUNC("STEP failed: Only support dump GPIO\n");
		return -1;
	}

#ifdef KERNEL_gpio_dump_regs_range
	KERNEL_gpio_dump_regs_range(p_gpio_act->pin_symbol, p_gpio_act->pin_symbol);
#else
	WMT_INFO_FUNC("STEP show: No support gpio dump\n");
#endif
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_disable_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do disable reset\n");
	mtk_wcn_stp_set_auto_rst(0);
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_chip_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do chip reset\n");
	mtk_wcn_wmt_do_reset(WMTDRV_TYPE_WMT);
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_keep_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do keep wake up\n");
	g_step_env.is_keep_wakeup = 1;
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_cancel_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do cancel keep wake up\n");
	g_step_env.is_keep_wakeup = 0;
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_periodic_dump_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_periodic_dump_action *p_pd_act = NULL;

	p_pd_act = list_entry_action(periodic_dump, p_act);
	if (p_pd_act->pd_entry->is_enable == 0) {
		WMT_INFO_FUNC("STEP show: Start periodic dump(%d ms)\n",
			p_pd_act->pd_entry->expires_ms);
		wmt_step_start_work(p_pd_act->pd_entry);
		p_pd_act->pd_entry->is_enable = 1;
	}

	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_show_string_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_show_string_action *p_show_act = NULL;

	p_show_act = list_entry_action(show_string, p_act);

	WMT_INFO_FUNC("STEP show: %s\n", p_show_act->content);

	if (func_do_extra != NULL)
		func_do_extra(1, p_show_act->content);

	return 0;
}

int wmt_step_do_sleep_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_sleep_action *p_sleep_act = NULL;

	p_sleep_act = list_entry_action(sleep, p_act);

	wmt_step_sleep_or_delay(p_sleep_act->ms);

	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_condition_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_condition_action *p_cond_act = NULL;
	int result, l_val, r_val;

	p_cond_act = list_entry_action(condition, p_act);

	l_val = g_step_env.temp_register[p_cond_act->l_temp_reg_id];

	if (p_cond_act->mode == STEP_CONDITION_RIGHT_REGISTER)
		r_val = g_step_env.temp_register[p_cond_act->r_temp_reg_id];
	else
		r_val = p_cond_act->value;

	if ((p_cond_act->operator_id >= 0) && wmt_step_operator_result_map[p_cond_act->operator_id]) {
		result = wmt_step_operator_result_map[p_cond_act->operator_id] (l_val, r_val);
		g_step_env.temp_register[p_cond_act->result_temp_reg_id] = result;

		WMT_INFO_FUNC("STEP show: Condition %d(%c%d) op %d(%c%d) => %d(%c%d)\n",
			l_val, STEP_TEMP_REGISTER_SYMBOL, p_cond_act->l_temp_reg_id,
			r_val, STEP_TEMP_REGISTER_SYMBOL, p_cond_act->r_temp_reg_id,
			result, STEP_TEMP_REGISTER_SYMBOL, p_cond_act->result_temp_reg_id);
	} else {
		WMT_ERR_FUNC("STEP failed: operator no define id: %d\n", p_cond_act->operator_id);
	}

	if (func_do_extra != NULL)
		func_do_extra(1, g_step_env.temp_register[p_cond_act->result_temp_reg_id]);

	return 0;
}

int wmt_step_do_value_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_value_action *p_val_act = NULL;

	p_val_act = list_entry_action(value, p_act);

	g_step_env.temp_register[p_val_act->temp_reg_id] = p_val_act->value;

	if (func_do_extra != NULL)
		func_do_extra(1, g_step_env.temp_register[p_val_act->temp_reg_id]);

	return 0;
}

struct step_action *wmt_step_create_action(enum step_action_id act_id, int param_num, char *params[])
{
	struct step_action *p_act = NULL;

	if (act_id <= STEP_ACTION_INDEX_NO_DEFINE || act_id >= STEP_ACTION_INDEX_MAX) {
		WMT_ERR_FUNC("STEP failed: Create action id: %d\n", act_id);
		return NULL;
	}

	if (wmt_step_action_map[act_id].func_create_action != NULL)
		p_act = wmt_step_action_map[act_id].func_create_action(param_num, params);
	else
		WMT_ERR_FUNC("STEP failed: Create no define id: %d\n", act_id);

	if (p_act != NULL)
		p_act->action_id = act_id;

	return p_act;
}

int wmt_step_init_pd_env(void)
{
	g_step_env.pd_struct.step_pd_wq = create_workqueue(STEP_PERIODIC_DUMP_WORK_QUEUE);
	if (!g_step_env.pd_struct.step_pd_wq) {
		WMT_ERR_FUNC("create_workqueue fail\n");
		return -1;
	}
	INIT_LIST_HEAD(&g_step_env.pd_struct.pd_list);

	return 0;
}

int wmt_step_deinit_pd_env(void)
{
	struct step_pd_entry *p_current = NULL;
	struct step_pd_entry *p_next = NULL;

	if (!g_step_env.pd_struct.step_pd_wq)
		return -1;

	list_for_each_entry_safe(p_current, p_next, &g_step_env.pd_struct.pd_list, list) {
		cancel_delayed_work(&p_current->pd_work);
		wmt_step_clear_action_list(&p_current->action_list);
	}
	destroy_workqueue(g_step_env.pd_struct.step_pd_wq);

	return 0;
}

struct step_pd_entry *wmt_step_get_periodic_dump_entry(unsigned int expires)
{
	struct step_pd_entry *p_current = NULL;

	if (expires <= 0)
		return NULL;

	if (!g_step_env.pd_struct.step_pd_wq) {
		if (wmt_step_init_pd_env() != 0)
			return NULL;
	}

	p_current = wmt_step_create_periodic_dump_entry(expires);
	if (p_current == NULL)
		return NULL;
	list_add_tail(&(p_current->list), &(g_step_env.pd_struct.pd_list));

	return p_current;
}

int wmt_step_parse_data(const char *in_buf, unsigned int size,
	STEP_WRITE_ACT_TO_LIST func_act_to_list)
{
	struct step_target_act_list_info parse_info;
	char *buf = NULL, *tmp_buf = NULL;
	char *line = NULL;

	buf = osal_malloc(size + 1);
	if (!buf) {
		WMT_ERR_FUNC("STEP failed: Buf malloc\n");
		return -1;
	}

	osal_memcpy(buf, (char *)in_buf, size);
	buf[size] = '\0';

	parse_info.tp_id = STEP_TRIGGER_POINT_NO_DEFINE;
	parse_info.p_target_list = NULL;
	parse_info.p_pd_entry = NULL;

	tmp_buf = buf;
	while ((line = osal_strsep(&tmp_buf, "\r\n")) != NULL)
		wmt_step_parse_line_data(line, &parse_info, func_act_to_list);

	osal_free(buf);

	return 0;
}


int wmt_step_read_file(const char *file_name)
{
	int ret = -1;
	const osal_firmware *p_step_cfg = NULL;

	if (g_step_env.is_enable == 1)
		return 0;

	if (0 == wmt_step_get_cfg(file_name, (osal_firmware **) &p_step_cfg)) {
		if (0 == wmt_step_parse_data((const char *)p_step_cfg->data, p_step_cfg->size,
			wmt_step_write_action)) {
			ret = 0;
		} else {
			ret = -1;
		}

		wmt_dev_patch_put((osal_firmware **) &p_step_cfg);
		return ret;
	}

	WMT_INFO_FUNC("STEP read file, %s is not exist\n", file_name);

	return ret;
}

void wmt_step_remove_action(struct step_action *p_act)
{
	if (p_act != NULL) {
		if (p_act->action_id <= STEP_ACTION_INDEX_NO_DEFINE || p_act->action_id >= STEP_ACTION_INDEX_MAX) {
			WMT_ERR_FUNC("STEP failed: Wrong action id %d\n", (int)p_act->action_id);
			return;
		}

		if (wmt_step_action_map[p_act->action_id].func_remove_action != NULL)
			wmt_step_action_map[p_act->action_id].func_remove_action(p_act);
	} else {
		WMT_ERR_FUNC("STEP failed: Action is NULL\n");
	}
}

void wmt_step_print_version(void)
{
	WMT_INFO_FUNC("STEP version: %d\n", STEP_VERSION);
}

/*******************************************************************************
 *                      E X T E R N A L   F U N C T I O N S
********************************************************************************/
void wmt_step_init(void)
{
#ifdef CFG_WMT_STEP
	wmt_step_setup();
	wmt_step_init_list();
	if (wmt_step_read_file(STEP_CONFIG_NAME) == 0) {
		wmt_step_print_version();
		down_write(&g_step_env.init_rwsem);
		g_step_env.is_enable = 1;
		up_write(&g_step_env.init_rwsem);
	}
#endif
}

void wmt_step_deinit(void)
{
	down_write(&g_step_env.init_rwsem);
	g_step_env.is_enable = 0;
	up_write(&g_step_env.init_rwsem);
	wmt_step_clear_list();
	wmt_step_unioremap_emi();
	wmt_step_deinit_pd_env();
}

void wmt_step_do_actions(enum step_trigger_point_id tp_id)
{
	wmt_step_do_actions_from_tp(tp_id, NULL);
}

void wmt_step_func_crtl_do_actions(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId)
{
	enum step_trigger_point_id tp_id = STEP_TRIGGER_POINT_NO_DEFINE;

	if (type < WMTDRV_TYPE_BT || type >= WMTDRV_TYPE_MAX) {
		WMT_ERR_FUNC("STEP failed: Do actions from type: %d\n", type);
		return;
	}

	switch (opId) {
	case WMT_OPID_FUNC_OFF:
		tp_id = wmt_step_func_ctrl_id[type][0];
		break;
	case WMT_OPID_FUNC_ON:
		tp_id = wmt_step_func_ctrl_id[type][1];
		break;
	default:
		break;
	}

	if (tp_id != STEP_TRIGGER_POINT_NO_DEFINE) {
		/* default value is 0*/
		wmt_step_do_actions(tp_id);
	}
}

void wmt_step_command_timeout_do_actions(char *reason)
{
	wmt_step_do_actions_from_tp(STEP_TRIGGER_POINT_COMMAND_TIMEOUT, reason);
}

