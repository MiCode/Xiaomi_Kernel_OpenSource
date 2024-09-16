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

#include <linux/platform_device.h>
#include "osal.h"
#include "wmt_step_test.h"
#include "wmt_step.h"
#include "wmt_exp.h"
#include "wmt_lib.h"
#include "mtk_wcn_consys_hw.h"
#include "stp_core.h"
#include "wmt_dbg.h"

struct step_test_check g_step_test_check;

void wmt_step_test_clear_parameter(char *params[])
{
	int i = 0;

	for (i = 0; i < STEP_PARAMETER_SIZE; i++)
		params[i] = NULL;
}

#define index_of_array(current_addr, base_addr, type) \
	(((unsigned long)current_addr - (unsigned long)base_addr) / sizeof(type))

int wmt_step_test_check_write_tp(struct step_action_list *p_act_list, enum step_action_id act_id,
	int param_num, char *params[])
{
	int index = g_step_test_check.step_check_index;
	int i;
	int tp_id;

	if (g_step_test_check.step_check_result == TEST_FAIL)
		return 0;

	if (index < 0)
		return 0;

	g_step_test_check.step_check_index++;

	if (g_step_test_check.step_check_test_tp_id[index] != -1) {
		tp_id = index_of_array(p_act_list, &g_step_env.actions, struct step_action_list);
		if (tp_id != g_step_test_check.step_check_test_tp_id[index]) {
			g_step_test_check.step_check_result = TEST_FAIL;
			WMT_ERR_FUNC("STEP test failed: tp_id %d: expect %d(%d)\n", tp_id,
				g_step_test_check.step_check_test_tp_id[index], index);
			return 0;
		}
	}

	if (act_id != g_step_test_check.step_check_test_act_id[index]) {
		g_step_test_check.step_check_result = TEST_FAIL;
		WMT_ERR_FUNC("STEP test failed: act_id %d: expect %d(%d)\n", act_id,
			g_step_test_check.step_check_test_tp_id[index], index);
		return 0;
	}

	if (param_num != g_step_test_check.step_check_params_num[index]) {
		g_step_test_check.step_check_result = TEST_FAIL;
		WMT_ERR_FUNC("STEP test failed: param num %d: expect %d(%d)\n", param_num,
			g_step_test_check.step_check_params_num[index], index);
		return 0;
	}

	for (i = 0; i < STEP_PARAMETER_SIZE; i++) {
		if (osal_strcmp(g_step_test_check.step_check_params[index][0], "") == 0)
			break;

		if (params[0] == NULL || osal_strcmp(params[0], g_step_test_check.step_check_params[index][0]) != 0) {
			g_step_test_check.step_check_result = TEST_FAIL;
			WMT_ERR_FUNC("STEP test failed: params[%d] %s: expect %s(%d)\n", i, params[0],
				g_step_test_check.step_check_params[index][0], index);
			return 0;
		}
	}

	g_step_test_check.step_check_result = TEST_PASS;

	return 0;
}

int wmt_step_test_check_create_emi(struct step_emi_action *p_emi_act, int check_params[],
	char *err_result)
{
	struct step_emi_info *p_emi_info = NULL;
	int result = TEST_FAIL;

	p_emi_info = &p_emi_act->info;
	if (p_emi_act->base.action_id != STEP_ACTION_INDEX_EMI) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_emi_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_emi_info->output_mode == STEP_OUTPUT_LOG) {
		if (p_emi_info->is_write != check_params[0] ||
			p_emi_info->begin_offset != check_params[1] ||
			p_emi_info->end_offset != check_params[2]) {
			WMT_ERR_FUNC("%s, C1 emi log params: %d, %d, %d\n",
				err_result, p_emi_info->is_write, p_emi_info->begin_offset,
				p_emi_info->end_offset);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else if (p_emi_info->output_mode == STEP_OUTPUT_REGISTER) {
		if (p_emi_info->is_write != check_params[0] ||
			p_emi_info->begin_offset != check_params[1] ||
			p_emi_info->mask != check_params[2] ||
			p_emi_info->temp_reg_id != check_params[3]) {
			WMT_ERR_FUNC("%s, C2 emi reg params: %d, %d, %d, %d\n",
				err_result, p_emi_info->is_write, p_emi_info->begin_offset,
				p_emi_info->mask, p_emi_info->temp_reg_id);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else {
		result = TEST_FAIL;
	}

	return result;
}

int wmt_step_test_check_create_condition_emi(struct step_condition_emi_action *p_cond_emi_act, int check_params[],
	char *err_result)
{
	struct step_emi_info *p_emi_info = NULL;
	int result = TEST_FAIL;

	p_emi_info = &p_cond_emi_act->info;
	if (p_cond_emi_act->base.action_id != STEP_ACTION_INDEX_CONDITION_EMI) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_cond_emi_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_emi_info->output_mode == STEP_OUTPUT_LOG) {
		if (p_cond_emi_act->cond_reg_id != check_params[0] ||
			p_emi_info->is_write != check_params[1] ||
			p_emi_info->begin_offset != check_params[2] ||
			p_emi_info->end_offset != check_params[3]) {
			WMT_ERR_FUNC("%s, C1 emi log params: %d, %d, %d, %d\n",
				err_result, p_cond_emi_act->cond_reg_id, p_emi_info->is_write,
				p_emi_info->begin_offset, p_emi_info->end_offset);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else if (p_emi_info->output_mode == STEP_OUTPUT_REGISTER) {
		if (p_cond_emi_act->cond_reg_id != check_params[0] ||
			p_emi_info->is_write != check_params[1] ||
			p_emi_info->begin_offset != check_params[2] ||
			p_emi_info->mask != check_params[3] ||
			p_emi_info->temp_reg_id != check_params[4]) {
			WMT_ERR_FUNC("%s, C2 emi reg params: %d, %d, %d, %d, %d\n",
				err_result, p_cond_emi_act->cond_reg_id, p_emi_info->is_write,
				p_emi_info->begin_offset, p_emi_info->mask, p_emi_info->temp_reg_id);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else {
		result = TEST_FAIL;
	}

	return result;
}

int wmt_step_test_check_create_read_reg(struct step_reigster_info *p_reg_info,
	int check_params[], char *err_result, int check_index)
{
	int result = TEST_FAIL;

	if (p_reg_info->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		if (p_reg_info->address != check_params[check_index + 1] ||
			p_reg_info->offset != check_params[check_index + 2]) {
			WMT_ERR_FUNC(
				"%s, C1 reg params: %d, 0x%08x, %d\n",
				err_result, p_reg_info->is_write, (unsigned int)p_reg_info->address,
				p_reg_info->offset);
			return TEST_FAIL;
		}
	} else {
		if (p_reg_info->address_type != check_params[check_index + 1] ||
			p_reg_info->offset != check_params[check_index + 2]) {
			WMT_ERR_FUNC(
				"%s, C2 reg params: %d, 0x%08x, %d\n",
				err_result, p_reg_info->is_write, (unsigned int)p_reg_info->address,
				p_reg_info->offset);
			return TEST_FAIL;
		}
	}

	if (p_reg_info->output_mode == STEP_OUTPUT_LOG) {
		if (p_reg_info->times != check_params[check_index + 3] ||
			p_reg_info->delay_time != check_params[check_index + 4]) {
			WMT_ERR_FUNC(
				"%s, C3 reg params: %d, 0x%08x, %d, %d, %d\n",
				err_result, p_reg_info->is_write, (unsigned int)p_reg_info->address,
				p_reg_info->offset, p_reg_info->times, p_reg_info->delay_time);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else if (p_reg_info->output_mode == STEP_OUTPUT_REGISTER) {
		if (p_reg_info->mask != check_params[check_index + 3] ||
			p_reg_info->temp_reg_id != check_params[check_index + 4]) {
			WMT_ERR_FUNC(
				"%s, C4 reg params: %d, 0x%08x, %d, %d, %d\n",
				err_result, p_reg_info->is_write, (unsigned int)p_reg_info->address,
				p_reg_info->offset, p_reg_info->mask, p_reg_info->temp_reg_id);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else {
		result = TEST_FAIL;
	}

	return result;
}

int wmt_step_test_check_create_write_reg(struct step_reigster_info *p_reg_info,
	int check_params[], char *err_result, int check_index)
{
	int result = TEST_FAIL;

	if (p_reg_info->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		if (p_reg_info->address != check_params[check_index + 1] ||
			p_reg_info->offset != check_params[check_index + 2] ||
			p_reg_info->value != check_params[check_index + 3]) {
			WMT_ERR_FUNC(
				"%s, C1 reg params: %d, 0x%08x, %d, %d\n",
				err_result, p_reg_info->is_write, (unsigned int)p_reg_info->address,
				p_reg_info->offset, p_reg_info->value);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	} else {
		if (p_reg_info->address_type != check_params[check_index + 1] ||
			p_reg_info->offset != check_params[check_index + 2] ||
			p_reg_info->value != check_params[check_index + 3]) {
			WMT_ERR_FUNC(
				"%s, C2 reg params: %d, %d, %d, %d\n",
				err_result, p_reg_info->is_write, p_reg_info->address_type,
				p_reg_info->offset, p_reg_info->value);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	}

	return result;
}

int wmt_step_test_check_create_reg(struct step_register_action *p_reg_act, int check_params[],
	char *err_result)
{
	struct step_reigster_info *p_reg_info = NULL;
	int result = TEST_FAIL;

	p_reg_info = &p_reg_act->info;
	if (p_reg_act->base.action_id != STEP_ACTION_INDEX_REGISTER) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_reg_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_reg_info->is_write != check_params[0]) {
		WMT_ERR_FUNC("%s, write failed: %d expect(%d)", err_result, p_reg_info->is_write, check_params[0]);
		result = TEST_FAIL;
	} else {
		if (p_reg_info->is_write == 0)
			result = wmt_step_test_check_create_read_reg(p_reg_info, check_params, err_result, 0);
		else
			result = wmt_step_test_check_create_write_reg(p_reg_info, check_params, err_result, 0);
	}

	return result;
}

int wmt_step_test_check_create_condition_reg(struct step_condition_register_action *p_cond_reg_act, int check_params[],
	char *err_result)
{
	struct step_reigster_info *p_reg_info = NULL;
	int result = TEST_FAIL;

	p_reg_info = &p_cond_reg_act->info;
	if (p_cond_reg_act->base.action_id != STEP_ACTION_INDEX_CONDITION_REGISTER) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_cond_reg_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_cond_reg_act->cond_reg_id != check_params[0]) {
		WMT_ERR_FUNC("%s, reg id failed: %d expect(%d)", err_result,
			p_cond_reg_act->cond_reg_id, check_params[0]);
		result = TEST_FAIL;
	} else if (p_reg_info->is_write != check_params[1]) {
		WMT_ERR_FUNC("%s, write failed: %d expect(%d)", err_result,
			p_reg_info->is_write, check_params[1]);
		result = TEST_FAIL;
	} else {
		if (p_reg_info->is_write == 0)
			result = wmt_step_test_check_create_read_reg(p_reg_info, check_params, err_result, 1);
		else
			result = wmt_step_test_check_create_write_reg(p_reg_info, check_params, err_result, 1);
	}

	return result;
}

int wmt_step_test_check_create_gpio(struct step_gpio_action *p_gpio_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_gpio_act->base.action_id != STEP_ACTION_INDEX_GPIO) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_gpio_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_gpio_act->is_write != check_params[0]) {
		WMT_ERR_FUNC("%s, write failed: %d", err_result, p_gpio_act->is_write);
		result = TEST_FAIL;
	} else {
		if (p_gpio_act->pin_symbol != check_params[1]) {
			WMT_ERR_FUNC("%s, gpio params: %d, %d\n",
			err_result, p_gpio_act->is_write, p_gpio_act->pin_symbol);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	}

	return result;
}

int wmt_step_test_check_create_drst(struct step_disable_reset_action *p_drst_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_drst_act->base.action_id != STEP_ACTION_INDEX_DISABLE_RESET) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_drst_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_crst(struct step_chip_reset_action *p_crst_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_crst_act->base.action_id != STEP_ACTION_INDEX_CHIP_RESET) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_crst_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_keep_wakeup(struct step_keep_wakeup_action *p_kwak_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_kwak_act->base.action_id != STEP_ACTION_INDEX_KEEP_WAKEUP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_kwak_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_cancel_wakeup(struct step_cancel_wakeup_action *p_cwak_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_cwak_act->base.action_id != STEP_ACTION_INDEX_CANCEL_WAKEUP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_cwak_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_periodic_dump(struct step_periodic_dump_action *p_pd_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_pd_act->base.action_id != STEP_ACTION_INDEX_PERIODIC_DUMP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_pd_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_show_string(struct step_show_string_action *p_show_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_show_act->base.action_id != STEP_ACTION_INDEX_SHOW_STRING) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_show_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_sleep(struct step_sleep_action *p_sleep_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_sleep_act->base.action_id != STEP_ACTION_INDEX_SLEEP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_sleep_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_sleep_act->ms != check_params[0]) {
		WMT_ERR_FUNC("%s, param failed: %d expect(%d)", err_result, p_sleep_act->ms, check_params[0]);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_condition(struct step_condition_action *p_cond_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_cond_act->base.action_id != STEP_ACTION_INDEX_CONDITION) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_cond_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_cond_act->result_temp_reg_id != check_params[0] ||
		p_cond_act->l_temp_reg_id != check_params[1] ||
		p_cond_act->operator_id != check_params[2]) {
		WMT_ERR_FUNC("%s, C1 param failed: %d %d %d %d expect(%d %d %d %d)",
			err_result, p_cond_act->result_temp_reg_id, p_cond_act->l_temp_reg_id,
			p_cond_act->operator_id, p_cond_act->r_temp_reg_id,
			check_params[0], check_params[1], check_params[2], check_params[3]);
		result = TEST_FAIL;
	} else {
		if (p_cond_act->mode == STEP_CONDITION_RIGHT_REGISTER && p_cond_act->r_temp_reg_id != check_params[3]) {
			WMT_ERR_FUNC("%s, C2 param failed: %d %d %d %d expect(%d %d %d %d)",
			err_result, p_cond_act->result_temp_reg_id, p_cond_act->l_temp_reg_id,
			p_cond_act->operator_id, p_cond_act->r_temp_reg_id,
			check_params[0], check_params[1], check_params[2], check_params[3]);
			result = TEST_FAIL;
		} else if (p_cond_act->mode == STEP_CONDITION_RIGHT_VALUE && p_cond_act->value != check_params[3]) {
			WMT_ERR_FUNC("%s, C3 param failed: %d %d %d %d expect(%d %d %d %d)",
			err_result, p_cond_act->result_temp_reg_id, p_cond_act->l_temp_reg_id,
			p_cond_act->operator_id, p_cond_act->value,
			check_params[0], check_params[1], check_params[2], check_params[3]);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	}

	return result;
}

int wmt_step_test_check_create_value(struct step_value_action *p_val_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_val_act->base.action_id != STEP_ACTION_INDEX_VALUE) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_val_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_val_act->temp_reg_id != check_params[0] ||
		p_val_act->value != check_params[1]) {
		WMT_ERR_FUNC("%s, param failed: %d %d expect(%d %d)",
			err_result, p_val_act->temp_reg_id, p_val_act->value,
			check_params[0], check_params[1]);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

void wmt_step_test_check_emi_act(unsigned int len, ...)
{
	unsigned int offset;
	unsigned int check_result;
	unsigned int value;
	PUINT8 p_virtual_addr = NULL;
	va_list args;

	if (g_step_test_check.step_check_result == TEST_FAIL)
		return;

	offset = g_step_test_check.step_check_emi_offset[g_step_test_check.step_check_index];
	p_virtual_addr = wmt_plat_get_emi_virt_add(offset);
	if (!p_virtual_addr) {
		g_step_test_check.step_check_result = TEST_FAIL;
		WMT_ERR_FUNC("STEP test failed: p_virtual_addr offset(%d) is null", offset);
		return;
	}
	check_result = CONSYS_REG_READ(p_virtual_addr);

	va_start(args, len);
	value = va_arg(args, unsigned int);
	va_end(args);

	if (check_result == value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP test failed: Value is %d, expect %d", value, check_result);
		g_step_test_check.step_check_result = TEST_FAIL;
		return;
	}

	if (g_step_test_check.step_check_temp_register_id != -1) {
		if (g_step_env.temp_register[g_step_test_check.step_check_temp_register_id] !=
			(check_result & g_step_test_check.step_test_mask)) {
			WMT_ERR_FUNC("STEP test failed: Register id(%d) value is %d, expect %d mask 0x%08x",
				g_step_test_check.step_check_temp_register_id,
				g_step_env.temp_register[g_step_test_check.step_check_temp_register_id],
				check_result, g_step_test_check.step_test_mask);
			g_step_test_check.step_check_result = TEST_FAIL;
		}
	}

	g_step_test_check.step_check_index++;
}

void wmt_step_test_check_reg_read_act(unsigned int len, ...)
{
	unsigned int check_result;
	unsigned int value;
	va_list args;

	if (g_step_test_check.step_check_result == TEST_FAIL)
		return;

	va_start(args, len);
	value = va_arg(args, unsigned int);
	check_result = CONSYS_REG_READ(g_step_test_check.step_check_register_addr);

	if (check_result == value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP test failed: Value is %d, expect %d(0x%08x)", value, check_result,
			(unsigned int)g_step_test_check.step_check_register_addr);
		g_step_test_check.step_check_result = TEST_FAIL;
	}

	if (g_step_test_check.step_check_temp_register_id != -1) {
		if (g_step_env.temp_register[g_step_test_check.step_check_temp_register_id] !=
			(check_result & g_step_test_check.step_test_mask)) {
			WMT_ERR_FUNC("STEP test failed: Register id(%d) value is %d, expect %d",
				g_step_test_check.step_check_temp_register_id,
				g_step_env.temp_register[g_step_test_check.step_check_temp_register_id],
				check_result);
			g_step_test_check.step_check_result = TEST_FAIL;
		}
	}

	va_end(args);
}

void wmt_step_test_check_reg_write_act(unsigned int len, ...)
{
	unsigned int value;
	va_list args;
	unsigned int mask = g_step_test_check.step_test_mask;

	va_start(args, len);
	value = va_arg(args, unsigned int);

	if (value == 0xdeadfeed) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else if (mask == 0xFFFFFFFF) {
		if (g_step_test_check.step_check_write_value == value) {
			g_step_test_check.step_check_result = TEST_PASS;
		} else {
			WMT_ERR_FUNC("STEP test failed: Value is %d, expect %zu", value,
				g_step_test_check.step_check_write_value);
			g_step_test_check.step_check_result = TEST_FAIL;
		}
	} else {
		if ((mask & value) != (mask & g_step_test_check.step_check_write_value)) {
			WMT_ERR_FUNC("STEP test failed: Overrite:%d, expect:%zu origin %d mask %d",
				value,
				g_step_test_check.step_check_write_value,
				g_step_test_check.step_recovery_value,
				mask);
			g_step_test_check.step_check_result = TEST_FAIL;
		} else if ((~mask & value) != (~mask & g_step_test_check.step_recovery_value)) {
			WMT_ERR_FUNC("STEP test failed: No change:%d, expect:%zu origin %d mask %d",
				value,
				g_step_test_check.step_check_write_value,
				g_step_test_check.step_recovery_value,
				mask);
			g_step_test_check.step_check_result = TEST_FAIL;
		} else {
			g_step_test_check.step_check_result = TEST_PASS;
		}
	}

	va_end(args);
}

void wmt_step_test_check_show_act(unsigned int len, ...)
{
	char *content = NULL;
	va_list args;

	va_start(args, len);
	content = va_arg(args, char*);
	if (content == NULL || g_step_test_check.step_check_result_string == NULL) {
		WMT_ERR_FUNC("STEP test failed: content is NULL");
		g_step_test_check.step_check_result = TEST_FAIL;
	} else if (osal_strcmp(content, g_step_test_check.step_check_result_string) == 0) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP test failed: content(%s), expect(%s)",
			content, g_step_test_check.step_check_result_string);
		g_step_test_check.step_check_result = TEST_FAIL;
	}
	va_end(args);
}

void wmt_step_test_check_condition_act(unsigned int len, ...)
{
	int value;
	va_list args;

	va_start(args, len);
	value = va_arg(args, int);
	if (value == g_step_test_check.step_check_result_value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP test failed: value(%d), expect(%d)",
			value, g_step_test_check.step_check_result_value);
		g_step_test_check.step_check_result = TEST_FAIL;
	}
	va_end(args);
}

void wmt_step_test_check_value_act(unsigned int len, ...)
{
	int value;
	va_list args;

	va_start(args, len);
	value = va_arg(args, int);
	if (value == g_step_test_check.step_check_result_value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP test failed: value(%d), expect(%d)",
			value, g_step_test_check.step_check_result_value);
		g_step_test_check.step_check_result = TEST_FAIL;
	}
	va_end(args);
}

void wmt_step_test_clear_check_data(void)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < STEP_TEST_ACTION_NUMBER; i++) {
		g_step_test_check.step_check_test_tp_id[i] = 0;
		g_step_test_check.step_check_test_act_id[i] = 0;
		g_step_test_check.step_check_params_num[i] = 0;
		g_step_test_check.step_check_emi_offset[i] = 0;
		for (j = 0; j < STEP_PARAMETER_SIZE; j++)
			g_step_test_check.step_check_params[i][j] = "";
	}

	g_step_test_check.step_check_total = 0;
	g_step_test_check.step_check_index = 0;
	g_step_test_check.step_check_result = TEST_PASS;
	g_step_test_check.step_check_register_addr = 0;
	g_step_test_check.step_test_mask = 0xFFFFFFFF;
	g_step_test_check.step_recovery_value = 0;
	g_step_test_check.step_check_result_value = 0;
	g_step_test_check.step_check_temp_register_id = -1;
}

void wmt_step_test_clear_temp_register(void)
{
	int i;

	for (i = 0; i < STEP_TEMP_REGISTER_SIZE; i++)
		g_step_env.temp_register[i] = 0;
}

#define STEP_CAN_WRITE_UNKNOWN 0
#define STEP_CAN_WRITE_YES 1
#define STEP_CAN_WRITE_NO 2
int wmt_step_test_is_can_write(SIZE_T addr, unsigned int mask)
{
	unsigned int before, after;
	int ret = STEP_CAN_WRITE_UNKNOWN;

	before = CONSYS_REG_READ(addr);
	CONSYS_REG_WRITE_MASK(addr, 0xFFFFFFFF, mask);
	after = CONSYS_REG_READ(addr);
	if ((after & mask) != (0xFFFFFFFF & mask))
		ret = STEP_CAN_WRITE_NO;

	CONSYS_REG_WRITE_MASK(addr, 0x0, mask);
	after = CONSYS_REG_READ(addr);
	if ((after & mask) != (0x0 & mask))
		ret = STEP_CAN_WRITE_NO;

	CONSYS_REG_WRITE_MASK(addr, before, mask);
	if (ret != STEP_CAN_WRITE_NO)
		ret = STEP_CAN_WRITE_YES;

	return ret;
}

int wmt_step_test_find_can_write_register(SIZE_T addr, int max, unsigned int mask)
{
	int i;
	int write_able;

	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		return -1;
	}

	for (i = 0x0; i < max; i += 0x4) {
		write_able = wmt_step_test_is_can_write(addr + i, mask);
		if (write_able == STEP_CAN_WRITE_YES) {
			ENABLE_PSM_MONITOR();
			return i;
		}
	}
	ENABLE_PSM_MONITOR();

	return -1;
}

void wmt_step_test_update_result(int result, struct step_test_report *p_report, char *err_result)
{
	if (result != TEST_FAIL) {
		p_report->pass++;
	} else {
		WMT_ERR_FUNC("%s", err_result);
		p_report->fail++;
	}
}

void wmt_step_test_update_result_report(struct step_test_report *p_dest_report,
	struct step_test_report *p_src_report)
{
	p_dest_report->pass += p_src_report->pass;
	p_dest_report->fail += p_src_report->fail;
	p_dest_report->check += p_src_report->check;
}

void wmt_step_test_show_result_report(char *test_name, struct step_test_report *p_report, int sec_begin, int usec_begin,
	int sec_end, int usec_end)
{
	unsigned int total = 0;
	unsigned int pass = 0;
	unsigned int fail = 0;
	unsigned int check = 0;
	int sec = 0;
	int usec = 0;

	pass = p_report->pass;
	fail = p_report->fail;
	check = p_report->check;

	if (usec_end >= usec_begin) {
		sec = sec_end - sec_begin;
		usec = usec_end - usec_begin;
	} else {
		sec = sec_end - sec_begin - 1;
		usec = usec_end - usec_begin + 1000000;
	}

	total = pass + fail + check;
	WMT_INFO_FUNC("%s Total: %d, PASS: %d, FAIL: %d, CHECK: %d, Spend Time: %d.%.6d\n",
		test_name, total, pass, fail, check, sec, usec);
}

void __wmt_step_test_parse_data(const char *buf, struct step_test_report *p_report, char *err_result)
{
	wmt_step_parse_data(buf, osal_strlen((char *)buf), wmt_step_test_check_write_tp);
	if (g_step_test_check.step_check_total != g_step_test_check.step_check_index) {
		WMT_ERR_FUNC("STEP test failed: index %d: expect total %d\n", g_step_test_check.step_check_index,
				g_step_test_check.step_check_total);
		g_step_test_check.step_check_result = TEST_FAIL;
	}
	wmt_step_test_update_result(g_step_test_check.step_check_result, p_report, err_result);
}

void __wmt_step_test_create_action(enum step_action_id act_id, int param_num, char *params[], int result_of_action,
	int check_params[], struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;
	int result = TEST_FAIL;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		switch (p_act->action_id) {
		case STEP_ACTION_INDEX_EMI:
			{
				struct step_emi_action *p_emi_act = NULL;

				p_emi_act = list_entry_action(emi, p_act);
				result = wmt_step_test_check_create_emi(p_emi_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_REGISTER:
			{
				struct step_register_action *p_reg_act = NULL;

				p_reg_act = list_entry_action(register, p_act);
				result = wmt_step_test_check_create_reg(p_reg_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_GPIO:
			{
				struct step_gpio_action *p_gpio_act = NULL;

				p_gpio_act = list_entry_action(gpio, p_act);
				result = wmt_step_test_check_create_gpio(p_gpio_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_DISABLE_RESET:
			{
				struct step_disable_reset_action *p_drst_act = NULL;

				p_drst_act = list_entry_action(disable_reset, p_act);
				result = wmt_step_test_check_create_drst(p_drst_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CHIP_RESET:
			{
				struct step_chip_reset_action *p_crst_act = NULL;

				p_crst_act = list_entry_action(chip_reset, p_act);
				result = wmt_step_test_check_create_crst(p_crst_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_KEEP_WAKEUP:
			{
				struct step_keep_wakeup_action *p_kwak_act = NULL;

				p_kwak_act = list_entry_action(keep_wakeup, p_act);
				result = wmt_step_test_check_create_keep_wakeup(p_kwak_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CANCEL_WAKEUP:
			{
				struct step_cancel_wakeup_action *p_cwak_act = NULL;

				p_cwak_act = list_entry_action(cancel_wakeup, p_act);
				result = wmt_step_test_check_create_cancel_wakeup(p_cwak_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_PERIODIC_DUMP:
			{
				struct step_periodic_dump_action *p_pd_act = NULL;

				p_pd_act = list_entry_action(periodic_dump, p_act);
				result = wmt_step_test_check_create_periodic_dump(p_pd_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_SHOW_STRING:
			{
				struct step_show_string_action *p_show_act = NULL;

				p_show_act = list_entry_action(show_string, p_act);
				result = wmt_step_test_check_create_show_string(p_show_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_SLEEP:
			{
				struct step_sleep_action *p_sleep_act = NULL;

				p_sleep_act = list_entry_action(sleep, p_act);
				result = wmt_step_test_check_create_sleep(p_sleep_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CONDITION:
			{
				struct step_condition_action *p_cond_act = NULL;

				p_cond_act = list_entry_action(condition, p_act);
				result = wmt_step_test_check_create_condition(p_cond_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_VALUE:
			{
				struct step_value_action *p_val_act = NULL;

				p_val_act = list_entry_action(value, p_act);
				result = wmt_step_test_check_create_value(p_val_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CONDITION_EMI:
			{
				struct step_condition_emi_action *p_cond_emi_act = NULL;

				p_cond_emi_act = list_entry_action(condition_emi, p_act);
				result = wmt_step_test_check_create_condition_emi(p_cond_emi_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_CONDITION_REGISTER:
			{
				struct step_condition_register_action *p_cond_reg_act = NULL;

				p_cond_reg_act = list_entry_action(condition_register, p_act);
				result = wmt_step_test_check_create_condition_reg(p_cond_reg_act, check_params,
					err_result);
			}
			break;
		default:
			result = TEST_FAIL;
			break;
		}

		if (result_of_action == -1)
			result = TEST_FAIL;

		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1)
			result = TEST_PASS;
		else
			result = TEST_FAIL;
	}
	wmt_step_test_update_result(result, p_report, err_result);
}

void __wmt_step_test_do_emi_action(enum step_action_id act_id, int param_num, char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (wmt_step_do_emi_action(p_act, wmt_step_test_check_emi_act) ==
			result_of_action) {
			if (g_step_test_check.step_check_total != g_step_test_check.step_check_index) {
				p_report->fail++;
				WMT_ERR_FUNC("%s, index %d: expect total %d\n", err_result,
					g_step_test_check.step_check_index, g_step_test_check.step_check_total);
			} else if (g_step_test_check.step_check_result == TEST_PASS) {
				p_report->pass++;
			} else {
				p_report->fail++;
				WMT_ERR_FUNC("%s\n", err_result);
			}
		} else {
			WMT_ERR_FUNC("%s, expect result is %d\n", err_result, result_of_action);
			p_report->fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1) {
			p_report->pass++;
		} else {
			p_report->fail++;
			WMT_ERR_FUNC("%s, Create failed\n", err_result);
		}
	}
}

void __wmt_step_test_do_cond_emi_action(enum step_action_id act_id, int param_num, char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (wmt_step_do_condition_emi_action(p_act, wmt_step_test_check_emi_act) ==
			result_of_action) {
			if (g_step_test_check.step_check_total != g_step_test_check.step_check_index) {
				p_report->fail++;
				WMT_ERR_FUNC("%s, index %d: expect total %d\n", err_result,
					g_step_test_check.step_check_index,	g_step_test_check.step_check_total);
			} else if (g_step_test_check.step_check_result == TEST_PASS) {
				p_report->pass++;
			} else {
				p_report->fail++;
				WMT_ERR_FUNC("%s\n", err_result);
			}
		} else {
			WMT_ERR_FUNC("%s, expect result is %d\n", err_result, result_of_action);
			p_report->fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1) {
			p_report->pass++;
		} else {
			p_report->fail++;
			WMT_ERR_FUNC("%s, Create failed\n", err_result);
		}
	}
}


void __wmt_step_test_do_register_action(enum step_action_id act_id, int param_num, char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;
	int result;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (osal_strcmp(params[0], "1") == 0) {
			/* Write register test */
			if (g_step_test_check.step_check_register_addr != 0) {
				g_step_test_check.step_recovery_value =
					CONSYS_REG_READ(g_step_test_check.step_check_register_addr);
			}
			result = wmt_step_do_register_action(p_act, wmt_step_test_check_reg_write_act);
			if (result == result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else if (result == -2) {
				p_report->check++;
				WMT_INFO_FUNC("STEP check: %s, no clock is ok?\n", err_result);
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result,
					result_of_action);
				p_report->fail++;
			}
			if (g_step_test_check.step_check_register_addr != 0) {
				CONSYS_REG_WRITE(g_step_test_check.step_check_register_addr,
					g_step_test_check.step_recovery_value);
			}
		} else {
			/* Read register test */
			g_step_test_check.step_check_result = TEST_PASS;
			result = wmt_step_do_register_action(p_act, wmt_step_test_check_reg_read_act);
			if (result == result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else if (result == -2) {
				p_report->check++;
				WMT_INFO_FUNC("STEP check: %s, no clock is ok?\n", err_result);
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result,
					result_of_action);
				p_report->fail++;
			}
		}
		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1) {
			p_report->pass++;
		} else {
			p_report->fail++;
			WMT_ERR_FUNC("%s, Create failed\n", err_result);
		}
	}
}

void __wmt_step_test_do_cond_register_action(enum step_action_id act_id, int param_num,
	char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;
	int result;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (osal_strcmp(params[1], "1") == 0) {
			/* Write register test */
			if (g_step_test_check.step_check_register_addr != 0) {
				g_step_test_check.step_recovery_value =
					CONSYS_REG_READ(g_step_test_check.step_check_register_addr);
			}

			result = wmt_step_do_condition_register_action(p_act, wmt_step_test_check_reg_write_act);
			if (result == result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else if (result == -2) {
				p_report->check++;
				WMT_INFO_FUNC("STEP check: %s, no clock is ok?\n", err_result);
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result, result_of_action);
				p_report->fail++;
			}
			if (g_step_test_check.step_check_register_addr != 0) {
				CONSYS_REG_WRITE(g_step_test_check.step_check_register_addr,
					g_step_test_check.step_recovery_value);
			}
		} else {
			/* Read register test */
			g_step_test_check.step_check_result = TEST_PASS;
			result = wmt_step_do_condition_register_action(p_act, wmt_step_test_check_reg_read_act);
			if (result == result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else if (result == -2) {
				p_report->check++;
				WMT_INFO_FUNC("STEP check: %s, no clock is ok?\n", err_result);
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result, result_of_action);
				p_report->fail++;
			}
		}
		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1) {
			p_report->pass++;
		} else {
			p_report->fail++;
			WMT_ERR_FUNC("%s, Create failed\n", err_result);
		}
	}
}

void wmt_step_test_all(void)
{
	struct step_test_report report = {0, 0, 0};
	bool is_enable_step = g_step_env.is_enable;
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	report.pass = 0;
	report.fail = 0;
	report.check = 0;

	WMT_INFO_FUNC("STEP test: All start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	g_step_env.is_enable = 0;
	wmt_step_test_read_file(&report);
	g_step_env.is_enable = 1;
	wmt_step_test_create_emi_action(&report);
	wmt_step_test_create_cond_emi_action(&report);
	wmt_step_test_create_register_action(&report);
	wmt_step_test_create_cond_register_action(&report);
	wmt_step_test_check_register_symbol(&report);
	wmt_step_test_create_other_action(&report);
	wmt_step_test_parse_data(&report);
	wmt_step_test_do_emi_action(&report);
	wmt_step_test_do_cond_emi_action(&report);
	wmt_step_test_do_register_action(&report);
	wmt_step_test_do_cond_register_action(&report);
	wmt_step_test_do_gpio_action(&report);
	wmt_step_test_do_wakeup_action(&report);
	wmt_step_test_create_periodic_dump(&report);
	wmt_step_test_do_show_action(&report);
	wmt_step_test_do_sleep_action(&report);
	wmt_step_test_do_condition_action(&report);
	wmt_step_test_do_value_action(&report);

	wmt_step_test_do_chip_reset_action(&report);
	g_step_env.is_enable = is_enable_step;

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: All result",
		&report, sec_begin, usec_begin, sec_end, usec_end);
}

void wmt_step_test_read_file(struct step_test_report *p_report)
{
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Read file start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/********************************
	 ******** Test case 1 ***********
	 ******* Wrong file name ********
	 ********************************/
	if (-1 == wmt_step_read_file("wmt_failed.cfg")) {
		temp_report.pass++;
	} else {
		WMT_ERR_FUNC("STEP test failed: (Read file TC-1) expect no file\n");
		temp_report.fail++;
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Read file result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_parse_data(struct step_test_report *p_report)
{
	char *buf = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Parse data start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/********************************
	 ******** Test case 1 ***********
	 ******** Normal case ***********
	 ********************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[AT] WAK+\r\n"
		"[AT] WAK-\r\n"
		"[AT] _REG 1 0x08 30 0xFF00FF00\r\n"
		"[TP 3] Before Chip reset\r\n"
		"[AT] SHOW Hello_World\r\n"
		"[AT] _SLP 1000\r\n"
		"[AT] COND $1 $2 == $3\r\n"
		"[AT] COND $1 $2 == 16\r\n"
		"[AT] _VAL $0 0x66\r\n"
		"[TP 4] After Chip reset\r\n"
		"[AT] _EMI 0 0x50 0xFFFFFFFF $1\r\n"
		"[AT] _REG 0 0x08 0xFFFFFFFF $1\r\n"
		"[AT] CEMI $2 0 0x50 0xFFFFFFFF $1\r\n"
		"[AT] CREG $2 0 0x08 0xFFFFFFFF $1\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 19;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_SHOW_STRING;
	g_step_test_check.step_check_test_tp_id[11] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[11] = STEP_ACTION_INDEX_SLEEP;
	g_step_test_check.step_check_test_tp_id[12] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[12] = STEP_ACTION_INDEX_CONDITION;
	g_step_test_check.step_check_test_tp_id[13] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[13] = STEP_ACTION_INDEX_CONDITION;
	g_step_test_check.step_check_test_tp_id[14] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[14] = STEP_ACTION_INDEX_VALUE;
	g_step_test_check.step_check_test_tp_id[15] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[15] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[16] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[16] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[17] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[17] = STEP_ACTION_INDEX_CONDITION_EMI;
	g_step_test_check.step_check_test_tp_id[18] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[18] = STEP_ACTION_INDEX_CONDITION_REGISTER;

	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params_num[1] = 4;
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[4][2] = "30";
	g_step_test_check.step_check_params_num[4] = 3;
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params_num[5] = 2;
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "6";
	g_step_test_check.step_check_params[6][2] = "3";
	g_step_test_check.step_check_params_num[6] = 3;
	g_step_test_check.step_check_params[9][0] = "1";
	g_step_test_check.step_check_params[9][1] = "0x08";
	g_step_test_check.step_check_params[9][2] = "30";
	g_step_test_check.step_check_params[9][3] = "0xFF00FF00";
	g_step_test_check.step_check_params_num[9] = 4;
	g_step_test_check.step_check_params[10][0] = "Hello_World";
	g_step_test_check.step_check_params_num[10] = 1;
	g_step_test_check.step_check_params[11][0] = "1000";
	g_step_test_check.step_check_params_num[11] = 1;
	g_step_test_check.step_check_params[12][0] = "$1";
	g_step_test_check.step_check_params[12][1] = "$2";
	g_step_test_check.step_check_params[12][2] = "==";
	g_step_test_check.step_check_params[12][3] = "$3";
	g_step_test_check.step_check_params_num[12] = 4;
	g_step_test_check.step_check_params[13][0] = "$1";
	g_step_test_check.step_check_params[13][1] = "$2";
	g_step_test_check.step_check_params[13][2] = "==";
	g_step_test_check.step_check_params[13][3] = "16";
	g_step_test_check.step_check_params_num[13] = 4;
	g_step_test_check.step_check_params[14][0] = "$0";
	g_step_test_check.step_check_params[14][1] = "0x66";
	g_step_test_check.step_check_params_num[14] = 2;
	g_step_test_check.step_check_params[15][0] = "0";
	g_step_test_check.step_check_params[15][1] = "0x50";
	g_step_test_check.step_check_params[15][2] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[15][3] = "$1";
	g_step_test_check.step_check_params_num[15] = 4;
	g_step_test_check.step_check_params[16][0] = "0";
	g_step_test_check.step_check_params[16][1] = "0x08";
	g_step_test_check.step_check_params[16][2] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[16][3] = "$1";
	g_step_test_check.step_check_params_num[16] = 4;
	g_step_test_check.step_check_params[17][0] = "$2";
	g_step_test_check.step_check_params[17][1] = "0";
	g_step_test_check.step_check_params[17][2] = "0x50";
	g_step_test_check.step_check_params[17][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[17][4] = "$1";
	g_step_test_check.step_check_params_num[17] = 5;
	g_step_test_check.step_check_params[18][0] = "$2";
	g_step_test_check.step_check_params[18][1] = "0";
	g_step_test_check.step_check_params[18][2] = "0x08";
	g_step_test_check.step_check_params[18][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[18][4] = "$1";
	g_step_test_check.step_check_params_num[18] = 5;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-1) Normal case\n");

	/*********************************
	 ******** Test case 2 ************
	 ** Normal case with some space **
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI   0  0x50   0x9c \r\n"
		"[AT]    _REG 0   0x08 5   2\r\n"
		"[AT]  DRST\r\n"
		"[AT]    _RST\r\n"
		"  [PD+] 2\r\n"
		"    [AT] _EMI 0 0x50 0x9c\r\n"
		"     [PD-] \r\n"
		" [TP 2] When Firmware trigger assert\r\n"
		"[AT]    _REG   1   0x08  30\r\n"
		"[AT]    GPIO  0  8\r\n"
		" [AT]  GPIO   1  6   3\r\n"
		"  [AT]      WAK+\r\n"
		"  [AT]   WAK-\r\n"
		"  [PD+]     5\r\n"
		"       [AT]    _EMI 0     0x50 0x9c\r\n"
		"  [PD-]   \r\n"
		"[TP 3] Before Chip reset\r\n"
		" [AT]    SHOW    Hello\r\n"
		"[AT]    _SLP     1000\r\n"
		"       [AT]   COND   $1    $2    ==     $3\r\n"
		"       [AT] _VAL   $0    0x66\r\n"
		"[TP 4] After Chip reset\r\n"
		"[AT]   CEMI $2    0 0x50     0xFFFFFFFF    $1\r\n"
		"   [AT]   CREG   $2 0    0x08    0xFFFFFFFF    $1\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 19;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = -1;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_test_tp_id[11] = -1;
	g_step_test_check.step_check_test_act_id[11] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[12] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[12] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[13] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[13] = STEP_ACTION_INDEX_SHOW_STRING;
	g_step_test_check.step_check_test_tp_id[14] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[14] = STEP_ACTION_INDEX_SLEEP;
	g_step_test_check.step_check_test_tp_id[15] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[15] = STEP_ACTION_INDEX_CONDITION;
	g_step_test_check.step_check_test_tp_id[16] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[16] = STEP_ACTION_INDEX_VALUE;
	g_step_test_check.step_check_test_tp_id[17] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[17] = STEP_ACTION_INDEX_CONDITION_EMI;
	g_step_test_check.step_check_test_tp_id[18] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[18] = STEP_ACTION_INDEX_CONDITION_REGISTER;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params_num[1] = 4;
	g_step_test_check.step_check_params[4][0] = "0";
	g_step_test_check.step_check_params[4][1] = "0x50";
	g_step_test_check.step_check_params[4][2] = "0x9c";
	g_step_test_check.step_check_params_num[4] = 3;
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "0x08";
	g_step_test_check.step_check_params[6][2] = "30";
	g_step_test_check.step_check_params_num[6] = 3;
	g_step_test_check.step_check_params[7][0] = "0";
	g_step_test_check.step_check_params[7][1] = "8";
	g_step_test_check.step_check_params_num[7] = 2;
	g_step_test_check.step_check_params[8][0] = "1";
	g_step_test_check.step_check_params[8][1] = "6";
	g_step_test_check.step_check_params[8][2] = "3";
	g_step_test_check.step_check_params_num[8] = 3;
	g_step_test_check.step_check_params[11][0] = "0";
	g_step_test_check.step_check_params[11][1] = "0x50";
	g_step_test_check.step_check_params[11][2] = "0x9c";
	g_step_test_check.step_check_params_num[11] = 3;
	g_step_test_check.step_check_params[13][0] = "Hello";
	g_step_test_check.step_check_params_num[13] = 1;
	g_step_test_check.step_check_params[14][0] = "1000";
	g_step_test_check.step_check_params_num[14] = 1;
	g_step_test_check.step_check_params[15][0] = "$1";
	g_step_test_check.step_check_params[15][1] = "$2";
	g_step_test_check.step_check_params[15][2] = "==";
	g_step_test_check.step_check_params[15][3] = "$3";
	g_step_test_check.step_check_params_num[15] = 4;
	g_step_test_check.step_check_params[16][0] = "$0";
	g_step_test_check.step_check_params[16][1] = "0x66";
	g_step_test_check.step_check_params_num[16] = 2;
	g_step_test_check.step_check_params[17][0] = "$2";
	g_step_test_check.step_check_params[17][1] = "0";
	g_step_test_check.step_check_params[17][2] = "0x50";
	g_step_test_check.step_check_params[17][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[17][4] = "$1";
	g_step_test_check.step_check_params_num[17] = 5;
	g_step_test_check.step_check_params[18][0] = "$2";
	g_step_test_check.step_check_params[18][1] = "0";
	g_step_test_check.step_check_params[18][2] = "0x08";
	g_step_test_check.step_check_params[18][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[18][4] = "$1";
	g_step_test_check.step_check_params_num[18] = 5;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-2) Normal case with some space\n");

	/***************************************************
	 ****************** Test case 3 ********************
	 ** Failed case not enough parameter (Can parser) **
	 ***************************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0x50 0x9c\r\n"
		"[AT] _REG 0 5 2\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08\r\n"
		"[AT] GPIO 0\r\n"
		"[AT] GPIO 6 3\r\n"
		"[TP 3] Before Chip reset\r\n"
		"[AT] SHOW\r\n"
		"[AT] _SLP\r\n"
		"[AT] COND $1 $2 >\r\n"
		"[AT] _VAL 0x66\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 11;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_SHOW_STRING;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_SLEEP;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_CONDITION;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_VALUE;

	g_step_test_check.step_check_params[0][0] = "0x50";
	g_step_test_check.step_check_params[0][1] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 2;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "5";
	g_step_test_check.step_check_params[1][2] = "2";
	g_step_test_check.step_check_params_num[1] = 3;
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params_num[4] = 2;
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params_num[5] = 1;
	g_step_test_check.step_check_params[6][0] = "6";
	g_step_test_check.step_check_params[6][1] = "3";
	g_step_test_check.step_check_params_num[6] = 2;
	g_step_test_check.step_check_params[9][0] = "$1";
	g_step_test_check.step_check_params[9][1] = "$2";
	g_step_test_check.step_check_params[9][2] = ">";
	g_step_test_check.step_check_params_num[9] = 3;
	g_step_test_check.step_check_params[10][0] = "0x66";
	g_step_test_check.step_check_params_num[10] = 1;

	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-3) Not enough parameter\n");

	/***************************************************
	 ****************** Test case 4 ********************
	 ************** Upcase and lowercase ***************
	 ***************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[Tp 1] When Command timeout\r\n"
		"[aT] _emi 0 0x50 0x9c\r\n"
		"[At] _reg 0 0x08 5 2\r\n"
		"[AT] drst\r\n"
		"[at] _rst\r\n"
		"[tP 2] When Firmware trigger assert\r\n"
		"[at] _reg 1 0x08 30\r\n"
		"[at] gpio 0 8\r\n"
		"[AT] gpio 1 6 3\r\n"
		"[AT] wak+\r\n"
		"[AT] wak--\r\n"
		"[AT] _reg 1 0x08 30 0xFF00FF00\r\n"
		"[pd+] 5\r\n"
		"[At] gpio 0 8\r\n"
		"[pd-]\r\n"
		"[tp 3] Before Chip reset\r\n"
		"[AT] show Hello_World\r\n"
		"[AT] _slp 1000\r\n"
		"[AT] cond $1 $2 == $3\r\n"
		"[AT] _val $0 0x66\r\n"
		"[TP 4] After Chip reset\r\n"
		"[AT] cemi $2 0 0x50 0xFFFFFFFF $1\r\n"
		"[at] creg $2 0 0x08 0xffffffff $1\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 18;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[10] = -1;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[11] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[11] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[12] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[12] = STEP_ACTION_INDEX_SHOW_STRING;
	g_step_test_check.step_check_test_tp_id[13] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[13] = STEP_ACTION_INDEX_SLEEP;
	g_step_test_check.step_check_test_tp_id[14] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[14] = STEP_ACTION_INDEX_CONDITION;
	g_step_test_check.step_check_test_tp_id[15] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[15] = STEP_ACTION_INDEX_VALUE;
	g_step_test_check.step_check_test_tp_id[16] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[16] = STEP_ACTION_INDEX_CONDITION_EMI;
	g_step_test_check.step_check_test_tp_id[17] = STEP_TRIGGER_POINT_AFTER_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[17] = STEP_ACTION_INDEX_CONDITION_REGISTER;

	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params_num[1] = 4;
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[4][2] = "30";
	g_step_test_check.step_check_params_num[4] = 3;
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params_num[5] = 2;
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "6";
	g_step_test_check.step_check_params[6][2] = "3";
	g_step_test_check.step_check_params_num[6] = 3;
	g_step_test_check.step_check_params[9][0] = "1";
	g_step_test_check.step_check_params[9][1] = "0x08";
	g_step_test_check.step_check_params[9][2] = "30";
	g_step_test_check.step_check_params[9][3] = "0xFF00FF00";
	g_step_test_check.step_check_params_num[9] = 4;
	g_step_test_check.step_check_params[10][0] = "0";
	g_step_test_check.step_check_params[10][1] = "8";
	g_step_test_check.step_check_params_num[10] = 2;
	g_step_test_check.step_check_params[12][0] = "Hello_World";
	g_step_test_check.step_check_params_num[12] = 1;
	g_step_test_check.step_check_params[13][0] = "1000";
	g_step_test_check.step_check_params_num[13] = 1;
	g_step_test_check.step_check_params[14][0] = "$1";
	g_step_test_check.step_check_params[14][1] = "$2";
	g_step_test_check.step_check_params[14][2] = "==";
	g_step_test_check.step_check_params[14][3] = "$3";
	g_step_test_check.step_check_params_num[14] = 4;
	g_step_test_check.step_check_params[15][0] = "$0";
	g_step_test_check.step_check_params[15][1] = "0x66";
	g_step_test_check.step_check_params_num[15] = 2;
	g_step_test_check.step_check_params[16][0] = "$2";
	g_step_test_check.step_check_params[16][1] = "0";
	g_step_test_check.step_check_params[16][2] = "0x50";
	g_step_test_check.step_check_params[16][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[16][4] = "$1";
	g_step_test_check.step_check_params_num[16] = 5;
	g_step_test_check.step_check_params[17][0] = "$2";
	g_step_test_check.step_check_params[17][1] = "0";
	g_step_test_check.step_check_params[17][2] = "0x08";
	g_step_test_check.step_check_params[17][3] = "0xFFFFFFFF";
	g_step_test_check.step_check_params[17][4] = "$1";
	g_step_test_check.step_check_params_num[17] = 5;

	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-4) Upcase and lowercase\n");

	/*************************************************
	 ****************** Test case 5 ******************
	 ************** TP sequence switch ***************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[tp 3] Before Chip reset\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 7;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_params[0][0] = "1";
	g_step_test_check.step_check_params[0][1] = "0x08";
	g_step_test_check.step_check_params[0][2] = "30";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "8";
	g_step_test_check.step_check_params_num[1] = 2;
	g_step_test_check.step_check_params[2][0] = "1";
	g_step_test_check.step_check_params[2][1] = "6";
	g_step_test_check.step_check_params[2][2] = "3";
	g_step_test_check.step_check_params_num[2] = 3;
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "0x50";
	g_step_test_check.step_check_params[5][2] = "0x9c";
	g_step_test_check.step_check_params_num[5] = 3;
	g_step_test_check.step_check_params[6][0] = "0";
	g_step_test_check.step_check_params[6][1] = "0x08";
	g_step_test_check.step_check_params[6][2] = "5";
	g_step_test_check.step_check_params[6][3] = "2";
	g_step_test_check.step_check_params_num[6] = 4;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-5) TP sequence switch\n");

	/*********************************
	 ********* Test case 6 ***********
	 ********* More comment **********
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c // show emi 0x50~0x9c\r\n"
		"// show cregister\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"// Do some action\r\n"
		"[AT] DRST // just do it\r\n"
		"[AT] _RST // is ok?\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[PD+] 5 // pd start\r\n"
		"[AT] GPIO 0 8 // just do it\r\n"
		"// Do some action\r\n"
		"[PD-] // pd ned\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 9;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = -1;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params_num[1] = 4;
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[4][2] = "30";
	g_step_test_check.step_check_params_num[4] = 3;
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params_num[5] = 2;
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "6";
	g_step_test_check.step_check_params[6][2] = "3";
	g_step_test_check.step_check_params_num[6] = 3;
	g_step_test_check.step_check_params[7][0] = "0";
	g_step_test_check.step_check_params[7][1] = "8";
	g_step_test_check.step_check_params_num[7] = 2;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-6) More comment\n");

	/*********************************
	 ********* Test case 7 ***********
	 ********* Wrong format **********
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP adfacdadf]When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[TP1]When Command timeout\r\n"
		"[AT] DRST\r\n"
		"[TP-1]When Command timeout\r\n"
		"[AT] _RST\r\n"
		"[T P 2] When Firmware trigger assert\r\n"
		"[AT] WAK+\r\n"
		"[TP 2 When Firmware trigger assert\r\n"
		"[AT] WAK+\r\n"
		"[PD+]\r\n"
		"[PD-]\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT]_REG 1 0x08 30\r\n"
		"[A  T] GPIO 0 8\r\n"
		"[ AT ] GPIO 1 6 3\r\n"
		"AT GPIO 0 8\r\n"
		"[AT WAK+\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 0;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-7) Wrong format\n");

	/********************************
	 ******** Test case 8 ***********
	 ******* Periodic dump **********
	 ********************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[PD+] 5\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"[PD-]\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[PD+] 3\r\n"
		"[AT] GPIO 0 8\r\n"
		"[PD-]\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[AT] WAK+\r\n"
		"[AT] WAK-\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 11;
	g_step_test_check.step_check_test_tp_id[0] = -1;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = -1;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[6] = -1;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params_num[0] = 3;
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params_num[1] = 4;
	g_step_test_check.step_check_params[5][0] = "1";
	g_step_test_check.step_check_params[5][1] = "0x08";
	g_step_test_check.step_check_params[5][2] = "30";
	g_step_test_check.step_check_params_num[5] = 3;
	g_step_test_check.step_check_params[6][0] = "0";
	g_step_test_check.step_check_params[6][1] = "8";
	g_step_test_check.step_check_params_num[6] = 2;
	g_step_test_check.step_check_params[8][0] = "1";
	g_step_test_check.step_check_params[8][1] = "6";
	g_step_test_check.step_check_params[8][2] = "3";
	g_step_test_check.step_check_params_num[8] = 3;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-8) Periodic dump\n");

	/*********************************
	 ******** Test case 9 ************
	 *** Boundary: Much parameter ****
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0x10 0x11 0x12 0x13\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_params[0][0] = "0x1";
	g_step_test_check.step_check_params[0][1] = "0x2";
	g_step_test_check.step_check_params[0][2] = "0x3";
	g_step_test_check.step_check_params[0][3] = "0x4";
	g_step_test_check.step_check_params[0][4] = "0x5";
	g_step_test_check.step_check_params[0][5] = "0x6";
	g_step_test_check.step_check_params[0][6] = "0x7";
	g_step_test_check.step_check_params[0][7] = "0x8";
	g_step_test_check.step_check_params[0][8] = "0x9";
	g_step_test_check.step_check_params[0][9] = "0x10";
	g_step_test_check.step_check_params_num[0] = 10;

	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP test case failed: (Parse data TC-9) Boundary: Much parameter\n");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Parse data result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_emi_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num = 0;

	WMT_INFO_FUNC("STEP test: Create EMI action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_EMI;

	/*****************************
	 ******** Test case 1 ********
	 **** EMI create for read ****
	 *****************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x50";
	params[2] = "0x9c";
	param_num = 3;
	check_params[0] = 0;
	check_params[1] = 0x50;
	check_params[2] = 0x9c;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-1) EMI create");

	/************************************
	 ********** Test case 2 ************
	 **** EMI create fail less param ****
	 ************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x50";
	param_num = 2;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-2) EMI create fail");

	/*************************************************
	 **************** Test case 3 *******************
	 ********** Save emi to temp register ************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x50";
	params[2] = "0x00000030";
	params[3] = "$3";
	param_num = 4;
	check_params[0] = 0;
	check_params[1] = 0x50;
	check_params[2] = 0x00000030;
	check_params[3] = 3;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-3) Save emi to temp register");

	/*************************************************
	 **************** Test case 4 *******************
	 ** Boundary: Save emi to wrong temp register ****
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x50";
	params[2] = "0x00000030";
	params[3] = "$30";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-4) Boundary: Save emi to wrong temp register");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create EMI action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_cond_emi_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num = 0;

	WMT_INFO_FUNC("STEP test: Create condition EMI action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CONDITION_EMI;

	/*************************************************
	 **************** Test case 1 *******************
	 ************ Condition emi create ***************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "0";
	params[2] = "0x50";
	params[3] = "0x70";
	param_num = 4;
	check_params[0] = 1;
	check_params[1] = 0;
	check_params[2] = 0x50;
	check_params[3] = 0x70;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-1) Condition emi create");

	/*************************************************
	 **************** Test case 2 *******************
	 ****** Save condition emi to temp register ******
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$2";
	params[1] = "0";
	params[2] = "0x50";
	params[3] = "0x00000030";
	params[4] = "$3";
	param_num = 5;
	check_params[0] = 2;
	check_params[1] = 0;
	check_params[2] = 0x50;
	check_params[3] = 0x00000030;
	check_params[4] = 3;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-2) Save condition emi to temp register");

	/***********************************************************
	 ******************** Test case 3 *************************
	 ** Boundary: Save condition emi to wrong temp register ****
	 ***********************************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "0";
	params[2] = "0x50";
	params[3] = "0x00000030";
	params[4] = "$30";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-3) Boundary: Boundary: Save condition emi to wrong temp register");

	/***********************************************************
	 ******************** Test case 4 *************************
	 ** Boundary: Save condition emi is wrong temp register ****
	 ***********************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$30";
	params[1] = "0";
	params[2] = "0x50";
	params[3] = "0x00000030";
	params[4] = "$1";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-4) Boundary: Boundary: Save condition emi is wrong temp register");

	/*************************************************
	 **************** Test case 5 *******************
	 ******* Condition emi create less params ********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "0";
	params[2] = "0x50";
	param_num = 3;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-5) Condition emi create less params");

	/*************************************************
	 **************** Test case 6 *******************
	 ******* Condition emi create wrong symbol *******
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "0";
	params[2] = "0x50";
	params[3] = "0x00000030";
	params[4] = "$1";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-6) Condition emi create wrong symbol");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create condition EMI action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_register_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num = 0;

	WMT_INFO_FUNC("STEP test: Create Register action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_REGISTER;

	/****************************************
	 ************ Test case 1 ***************
	 **** REGISTER(Addr) create for read ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "2";
	params[4] = "10";
	param_num = 5;
	check_params[0] = 0;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 2;
	check_params[4] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-1) REG create read");

	/*****************************************
	 ************ Test case 2 ****************
	 **** REGISTER(Addr) create for write ****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "15";
	param_num = 4;
	check_params[0] = 1;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 15;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-2) REG create write");

	/******************************************
	 ************** Test case 3 ***************
	 ******* Boundary: read wrong symbol ******
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#10000";
	params[2] = "0x204";
	params[3] = "1";
	params[4] = "0";
	param_num = 5;
	check_params[0] = 0;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 2;
	check_params[4] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-3) Boundary: read wrong symbol");

	/****************************************************
	 **************** Test case 4 **********************
	 **** REGISTER(Addr) create read fail less param ****
	 ****************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	param_num = 3;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-4) REG create read fail");

	/*****************************************************
	 ************ Test case 5 ***************************
	 **** REGISTER(Addr) create write fail less param ****
	 *****************************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	param_num = 3;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-5) REG create write fail");

	/*****************************************
	 ************ Test case 6 ****************
	 ** REGISTER(Addr) create for write bit ***
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "15";
	params[4] = "0xFF00FF00";
	param_num = 5;
	check_params[0] = 1;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 15;
	check_params[4] = 0xFF00FF00;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-6) REG create write");

	/*********************************************************
	 ******************** Test case 7 ***********************
	 **** REGISTER(Addr) create for read to temp register ****
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "0x00000030";
	params[4] = "$5";
	param_num = 5;
	check_params[0] = 0;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 0x00000030;
	check_params[4] = 5;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-7) REGISTER(Addr) create for read to temp register");

	/*********************************************************
	 ******************** Test case 8 ***********************
	 *** REGISTER(Symbol) create for read to temp register ***
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x9c";
	params[3] = "0x00000030";
	params[4] = "$7";
	param_num = 5;
	check_params[0] = 0;
	check_params[1] = 1;
	check_params[2] = 0x9c;
	check_params[3] = 0x00000030;
	check_params[4] = 7;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-8) REGISTER(Symbol) create for read to temp register");

	/*********************************************************
	 ******************** Test case 9 ***********************
	 ********** REGISTER(Symbol) create for read *************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x9c";
	params[3] = "1";
	params[4] = "10";
	param_num = 5;
	check_params[0] = 0;
	check_params[1] = 1;
	check_params[2] = 0x9c;
	check_params[3] = 1;
	check_params[4] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-9) REGISTER(Symbol) create for read");

	/*********************************************************
	 ******************** Test case 10 ***********************
	 ************ REGISTER(Addr) less parameter **************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "0x555";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-10) REGISTER(Addr) less parameter");

	/*********************************************************
	 ******************** Test case 11 ***********************
	 ************ REGISTER(Symbol) less parameter **************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x9c";
	params[3] = "0x555";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-11) REGISTER(Symbol) less parameter");

	/**********************************************************
	 *********************** Test case 12 *********************
	 ** Boundary: REGISTER(Addr) read to worng temp register **
	 **********************************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "0x00000030";
	params[4] = "$35";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-12) Boundary: REGISTER(Addr) read to worng temp registe");

	/************************************************************
	 *********************** Test case 13 ***********************
	 ** Boundary: REGISTER(Symbol) read to worng temp register **
	 ************************************************************/
	WMT_INFO_FUNC("STEP test: TC 13\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x9c";
	params[3] = "0x00000030";
	params[4] = "$35";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-13) Boundary: REGISTER(Symbol) read to worng temp registe");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create register action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_cond_register_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num = 0;

	WMT_INFO_FUNC("STEP test: Create condition Register action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CONDITION_REGISTER;

	/****************************************
	 ************ Test case 1 ***************
	 **** COND_REG(Addr) create for read ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$5";
	params[1] = "0";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "2";
	params[5] = "10";
	param_num = 6;
	check_params[0] = 5;
	check_params[1] = 0;
	check_params[2] = 0x124dfad;
	check_params[3] = 0x9c;
	check_params[4] = 2;
	check_params[5] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-1) COND_REG(Addr) create for read");

	/*****************************************
	 ************ Test case 2 ****************
	 **** COND_REG(Addr) create for write ****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$7";
	params[1] = "1";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "15";
	param_num = 5;
	check_params[0] = 7;
	check_params[1] = 1;
	check_params[2] = 0x124dfad;
	check_params[3] = 0x9c;
	check_params[4] = 15;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-2) COND_REG(Addr) create write");

	/******************************************
	 ************** Test case 3 ***************
	 ******* Boundary: read wrong symbol ******
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$2";
	params[1] = "0";
	params[2] = "#10000";
	params[3] = "0x204";
	params[4] = "1";
	params[5] = "0";
	param_num = 6;
	check_params[0] = 2;
	check_params[1] = 0;
	check_params[2] = 0x124dfad;
	check_params[3] = 0x9c;
	check_params[4] = 2;
	check_params[5] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-3) Boundary: read wrong symbol");

	/****************************************************
	 **************** Test case 4 **********************
	 **** COND_REG(Addr) create read fail less param ****
	 ****************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$3";
	params[1] = "0";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-4) COND_REG create read fail");

	/*****************************************************
	 ************ Test case 5 ***************************
	 **** COND_REG(Addr) create write fail less param ****
	 *****************************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$4";
	params[1] = "1";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-5) COND_REG create write fail");

	/*****************************************
	 ************ Test case 6 ****************
	 ** COND_REG(Addr) create for write bit ***
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$9";
	params[1] = "1";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "15";
	params[5] = "0xFF00FF00";
	param_num = 6;
	check_params[0] = 9;
	check_params[1] = 1;
	check_params[2] = 0x124dfad;
	check_params[3] = 0x9c;
	check_params[4] = 15;
	check_params[5] = 0xFF00FF00;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-6) COND_REG create write");

	/*********************************************************
	 ******************** Test case 7 ***********************
	 **** COND_REG(Addr) create for read to temp register ****
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$9";
	params[1] = "0";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "0x00000030";
	params[5] = "$5";
	param_num = 6;
	check_params[0] = 9;
	check_params[1] = 0;
	check_params[2] = 0x124dfad;
	check_params[3] = 0x9c;
	check_params[4] = 0x00000030;
	check_params[5] = 5;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-7) COND_REG(Addr) create for read to temp register");

	/*********************************************************
	 ******************** Test case 8 ***********************
	 *** COND_REG(Symbol) create for read to temp register ***
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "0x00000030";
	params[5] = "$7";
	param_num = 6;
	check_params[0] = 1;
	check_params[1] = 0;
	check_params[2] = 1;
	check_params[3] = 0x9c;
	check_params[4] = 0x00000030;
	check_params[5] = 7;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-8) COND_REG(Symbol) create for read to temp register");

	/*********************************************************
	 ******************** Test case 9 ***********************
	 ********** COND_REG(Symbol) create for read *************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$2";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "1";
	params[5] = "10";
	param_num = 6;
	check_params[0] = 2;
	check_params[1] = 0;
	check_params[2] = 1;
	check_params[3] = 0x9c;
	check_params[4] = 1;
	check_params[5] = 10;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-9) COND_REG(Symbol) create for read");

	/*********************************************************
	 ******************** Test case 10 ***********************
	 ************ COND_REG(Addr) less parameter **************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$3";
	params[1] = "0";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "0x555";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-10) COND_REG(Addr) less parameter");

	/*********************************************************
	 ******************** Test case 11 ***********************
	 ************ COND_REG(Symbol) less parameter **************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$4";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "0x555";
	param_num = 5;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-11) COND_REG(Symbol) less parameter");

	/**********************************************************
	 *********************** Test case 12 *********************
	 ** Boundary: COND_REG(Addr) read to worng temp register **
	 **********************************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$5";
	params[1] = "0";
	params[2] = "0x124dfad";
	params[3] = "0x9c";
	params[4] = "0x00000030";
	params[5] = "$35";
	param_num = 6;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-12) Boundary: COND_REG(Addr) read to worng temp registe");

	/************************************************************
	 *********************** Test case 13 ***********************
	 ** Boundary: COND_REG(Symbol) read to worng temp register **
	 ************************************************************/
	WMT_INFO_FUNC("STEP test: TC 13\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$6";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "0x00000030";
	params[5] = "$35";
	param_num = 6;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-13) Boundary: COND_REG(Symbol) read to worng temp registe");

	/*********************************************************
	 ******************** Test case 14 ***********************
	 ************* COND_REG(Symbol) worng symbol *************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 14\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "8";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "1";
	params[5] = "10";
	param_num = 6;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-14) Boundary: COND_REG(Symbol) worng symbol");

	/*********************************************************
	 ******************** Test case 15 ***********************
	 ********* COND_REG(Symbol) worng temp register id *******
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 15\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "$88";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x9c";
	params[4] = "1";
	params[5] = "10";
	param_num = 6;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-15) Boundary: COND_REG(Symbol) read to worng temp registe");


	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create condition register action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

int wmt_step_test_get_symbol_num(void)
{
	int len = 0;
	struct device_node *node = NULL;

	if (g_pdev != NULL) {
		node = g_pdev->dev.of_node;
		if (node) {
			of_get_property(node, "reg", &len);
			len /= (of_n_addr_cells(node) + of_n_size_cells(node));
			len /= (sizeof(int));
		} else {
			WMT_ERR_FUNC("STEP test failed: node null");
			return -1;
		}
	} else {
		WMT_ERR_FUNC("STEP test failed: gdev null");
		return -1;
	}

	return len;
}

void wmt_step_test_check_register_symbol(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num = 0;
	int i = 0;
	int symbol_num = wmt_step_test_get_symbol_num();
	unsigned char buf[4];

	WMT_INFO_FUNC("STEP test: Check Register symbol start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_REGISTER;
	/*********************************************************
	 ******************** Test case 1 ***********************
	 ********** REGISTER(Symbol) create for read *************
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	if (symbol_num < 0) {
		temp_report.fail++;
	} else {
		if (symbol_num >= STEP_REGISTER_MAX) {
			symbol_num = STEP_REGISTER_MAX - 1;
		}

		for (i = 1; i <= symbol_num; i++) {
			wmt_step_test_clear_parameter(params);
			params[0] = "0";
			if (snprintf(buf, 4, "#%d", i) < 0)
				WMT_INFO_FUNC("[%s::%d] snprintf buf fail\n", __func__, __LINE__);
			else
				params[1] = buf;
			params[2] = "0x9c";
			params[3] = "1";
			params[4] = "10";
			param_num = 5;
			check_params[0] = 0;
			check_params[1] = i;
			check_params[2] = 0x9c;
			check_params[3] = 1;
			check_params[4] = 10;
			__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
				"STEP test case failed: (Check Register symbol TC-1) REGISTER(Symbol) create for read");
		}
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Check Register symbol result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_other_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	int check_params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	struct step_pd_entry fack_pd_entry;
	int param_num = 0;

	WMT_INFO_FUNC("STEP test: Create other action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/******************************************
	 ************ Test case 1 *****************
	 ********** GPIO create for read **********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_GPIO;
	params[0] = "0";
	params[1] = "8";
	param_num = 2;
	check_params[0] = 0;
	check_params[1] = 8;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-1) GPIO create read");

	/*****************************************
	 ************ Test case 2 ****************
	 ********* DISABLE REST create ***********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_DISABLE_RESET;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-2) DISABLE REST");

	/*****************************************
	 ************ Test case 3 ****************
	 ********** CHIP REST create *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CHIP_RESET;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-3) CHIP REST");

	/*****************************************
	 ************ Test case 4 ****************
	 ******** Keep Wakeup create *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_KEEP_WAKEUP;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-4) Keep wakeup");

	/*****************************************
	 ************ Test case 5 ****************
	 ***** Cancel keep wakeup create *********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-5) Cancel keep wakeup");

	/*************************************************
	 **************** Test case 6 *******************
	 ********** GPIO create fail less param **********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_GPIO;
	params[0] = "0";
	param_num = 1;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-6) GPIO create fail");

	/*************************************************
	 **************** Test case 7 *******************
	 ************** Periodic dump create *************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_PERIODIC_DUMP;
	params[0] = (PINT8)&fack_pd_entry;
	param_num = 1;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-7) Periodic dump create fail");

	/*************************************************
	 **************** Test case 8 *******************
	 ****** Periodic dump create fail no param *******
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_PERIODIC_DUMP;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-8) Periodic dump create fail");

	/*************************************************
	 **************** Test case 9 *******************
	 **************** Show create ********************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_SHOW_STRING;
	params[0] = "Hello";
	param_num = 1;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-9) Show create");

	/*************************************************
	 **************** Test case 10 *******************
	 ******** Show create failed no param ************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_SHOW_STRING;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-10) Show create failed no param");

	/*************************************************
	 **************** Test case 11 *******************
	 **************** Sleep create *******************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_SLEEP;
	params[0] = "1000";
	param_num = 1;
	check_params[0] = 1000;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-11) Sleep create");

	/*************************************************
	 **************** Test case 12 *******************
	 ********* Sleep create failed no param **********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_SLEEP;
	param_num = 0;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-12) Sleep create failed no param");

	/*************************************************
	 **************** Test case 13 *******************
	 ************** Condition create *****************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 13\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "==";
	params[3] = "$2";
	param_num = 4;
	check_params[0] = 0;
	check_params[1] = 1;
	check_params[2] = STEP_OPERATOR_EQUAL;
	check_params[3] = 2;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-13) Condition create");

	/*************************************************
	 **************** Test case 14 *******************
	 *********** Condition create value **************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 14\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "==";
	params[3] = "16";
	param_num = 4;
	check_params[0] = 0;
	check_params[1] = 1;
	check_params[2] = STEP_OPERATOR_EQUAL;
	check_params[3] = 16;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-14) Condition create");

	/*************************************************
	 **************** Test case 15 *******************
	 ****** Condition create failed less param *******
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 15\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "$2";
	param_num = 3;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-15) Condition create failed less param");

	/*************************************************
	 **************** Test case 16 *******************
	 ******** Condition create failed no value********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 16\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "==";
	param_num = 1;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-16) Condition create failed no value");

	/*************************************************
	 **************** Test case 17 *******************
	 * Boundary: Condition create failed over reg id *
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 17\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$25";
	params[1] = "$1";
	params[2] = "==";
	params[3] = "$2";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-17) Boundary: Condition create failed over reg id");

	/*************************************************
	 **************** Test case 18 *******************
	 * Boundary: Condition create failed over reg id *
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 18\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "==";
	params[3] = "$20";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-18) Boundary: Condition create failed over reg id");

	/*************************************************
	 **************** Test case 19 *******************
	 ******** Condition create failed operator********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 19\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CONDITION;
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "&";
	params[3] = "$2";
	param_num = 4;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-19) Condition create failed operator");

	/*************************************************
	 **************** Test case 20 *******************
	 **************** Value create *******************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 20\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_VALUE;
	params[0] = "$0";
	params[1] = "0x123";
	param_num = 2;
	check_params[0] = 0;
	check_params[1] = 0x123;
	__wmt_step_test_create_action(act_id, param_num, params, 0, check_params, &temp_report,
		"STEP test case failed: (Create action TC-20) Condition create");

	/*************************************************
	 **************** Test case 21 *******************
	 ******* Value create failed wrong order *********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 21\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_VALUE;
	params[0] = "0x123";
	params[1] = "$1";
	param_num = 2;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-21) Value create failed wrong order");

	/*************************************************
	 **************** Test case 22 *******************
	 ********* Value create failed no value **********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 22\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_VALUE;
	params[0] = "$1";
	param_num = 1;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-22) Value create failed no value");

	/*************************************************
	 **************** Test case 23 *******************
	 *** Boundary: Value create failed over reg id ***
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 23\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_VALUE;
	params[0] = "$25";
	params[1] = "0x123";
	param_num = 2;
	__wmt_step_test_create_action(act_id, param_num, params, -1, check_params, &temp_report,
		"STEP test case failed: (Create action TC-23) Boundary: Value create failed over reg id");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create other action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

int wmt_step_test_get_emi_wmt_offset(unsigned char buf[], int offset)
{
	P_CONSYS_EMI_ADDR_INFO emi_phy_addr;

	emi_phy_addr = mtk_wcn_consys_soc_get_emi_phy_add();
	if (emi_phy_addr != NULL) {
		if (snprintf(buf, 11, "0x%08x", ((unsigned int)emi_phy_addr->emi_core_dump_offset + offset)) < 0) {
			WMT_INFO_FUNC("[%s::%d] snprintf buf fail\n", __func__, __LINE__);
			return -1;
		}
	} else {
		WMT_ERR_FUNC("STEP test failed: emi_phy_addr is NULL\n");
		return -1;
	}

	return 0;
}

void wmt_step_test_do_emi_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf_begin[11];
	unsigned char buf_end[11];
	int param_num;

	WMT_INFO_FUNC("STEP test: Do EMI action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_EMI;

	if (wmt_step_test_get_emi_wmt_offset(buf_begin, 0x0) != 0) {
		temp_report.fail++;
		osal_gettimeofday(&sec_end, &usec_end);
		wmt_step_test_show_result_report("STEP result: Do EMI action result",
			&temp_report, sec_begin, usec_begin, sec_end, usec_end);
		wmt_step_test_update_result_report(p_report, &temp_report);
		return;
	}

	/*****************************************
	 ************ Test case 1 ****************
	 ********** EMI dump 32 bit **************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x44);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x48);
	params[2] = buf_end;
	param_num = 3;
	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_emi_offset[0] = 0x44;
	__wmt_step_test_do_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-1) dump 32bit");

	/*****************************************
	 ************ Test case 2 ****************
	 ****** EMI dump check for address *******
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x24);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x44);
	params[2] = buf_end;
	param_num = 3;
	g_step_test_check.step_check_total = 8;
	g_step_test_check.step_check_emi_offset[0] = 0x24;
	g_step_test_check.step_check_emi_offset[1] = 0x28;
	g_step_test_check.step_check_emi_offset[2] = 0x2c;
	g_step_test_check.step_check_emi_offset[3] = 0x30;
	g_step_test_check.step_check_emi_offset[4] = 0x34;
	g_step_test_check.step_check_emi_offset[5] = 0x38;
	g_step_test_check.step_check_emi_offset[6] = 0x3c;
	g_step_test_check.step_check_emi_offset[7] = 0x40;
	__wmt_step_test_do_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-2) more address");

	/*****************************************
	 ************ Test case 3 ****************
	 **** EMI dump begin larger than end *****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x20);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[2] = buf_end;
	param_num = 3;
	g_step_test_check.step_check_total = 6;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	g_step_test_check.step_check_emi_offset[2] = 0x10;
	g_step_test_check.step_check_emi_offset[3] = 0x14;
	g_step_test_check.step_check_emi_offset[4] = 0x18;
	g_step_test_check.step_check_emi_offset[5] = 0x1c;
	__wmt_step_test_do_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-3) begin larger than end");

	/****************************************
	 ************ Test case 4 ***************
	 ******** EMI only support read *********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x20);
	params[2] = buf_end;
	param_num = 3;
	__wmt_step_test_do_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do EMI action TC-4) only support read");

	/****************************************
	 ************ Test case 5 ***************
	 ********* EMI dump not 32bit ***********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x0e);
	params[2] = buf_end;
	param_num = 3;
	g_step_test_check.step_check_total = 2;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	__wmt_step_test_do_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-5) not 32bit");

	/*****************************************
	 ************ Test case 6 ****************
	 ***** EMI dump over emi max size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, (gConEmiSize + 0x08));
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, (gConEmiSize + 0x0e));
	params[2] = buf_end;
	param_num = 3;
	__wmt_step_test_do_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do EMI action TC-6) over emi max size");

	/*****************************************
	 ************ Test case 7 ****************
	 ************* page fault ****************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x02);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[2] = buf_end;
	param_num = 3;
	__wmt_step_test_do_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do EMI action TC-7) page fault");

	/*****************************************
	 ************ Test case 8 ****************
	 ********** save to temp reg *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[1] = buf_begin;
	params[2] = "0x0F0F0F0F";
	params[3] = "$1";
	param_num = 4;
	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_test_mask = 0x0F0F0F0F;
	g_step_test_check.step_check_temp_register_id = 1;
	__wmt_step_test_do_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-8) save to temp reg");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do EMI action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_cond_emi_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf_begin[11];
	unsigned char buf_end[11];
	int param_num;

	WMT_INFO_FUNC("STEP test: Do condition EMI action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CONDITION_EMI;
	/*****************************************
	 ************ Test case 1 ****************
	 ********** EMI dump 32 bit **************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x44);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x48);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[0] = 1;

	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_emi_offset[0] = 0x44;
	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-1) dump 32bit");

	/*****************************************
	 ************ Test case 2 ****************
	 ****** EMI dump check for address *******
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$1";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x24);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x44);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[1] = 1;

	g_step_test_check.step_check_total = 8;
	g_step_test_check.step_check_emi_offset[0] = 0x24;
	g_step_test_check.step_check_emi_offset[1] = 0x28;
	g_step_test_check.step_check_emi_offset[2] = 0x2c;
	g_step_test_check.step_check_emi_offset[3] = 0x30;
	g_step_test_check.step_check_emi_offset[4] = 0x34;
	g_step_test_check.step_check_emi_offset[5] = 0x38;
	g_step_test_check.step_check_emi_offset[6] = 0x3c;
	g_step_test_check.step_check_emi_offset[7] = 0x40;
	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-2) more address");

	/*****************************************
	 ************ Test case 3 ****************
	 **** EMI dump begin larger than end *****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x20);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[0] = 15;

	g_step_test_check.step_check_total = 6;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	g_step_test_check.step_check_emi_offset[2] = 0x10;
	g_step_test_check.step_check_emi_offset[3] = 0x14;
	g_step_test_check.step_check_emi_offset[4] = 0x18;
	g_step_test_check.step_check_emi_offset[5] = 0x1c;
	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-3) begin larger than end");

	/****************************************
	 ************ Test case 4 ***************
	 ******** EMI only support read *********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$1";
	params[1] = "1";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x20);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[1] = 1;

	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-4) only support read");

	/****************************************
	 ************ Test case 5 ***************
	 ********* EMI dump not 32bit ***********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x0e);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[0] = 1;

	g_step_test_check.step_check_total = 2;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-5) not 32bit");

	/*****************************************
	 ************ Test case 6 ****************
	 ***** EMI dump over emi max size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$9";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, (gConEmiSize + 0x08));
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, (gConEmiSize + 0x0e));
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[9] = 1;

	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-6) over emi max size");

	/*****************************************
	 ************ Test case 7 ****************
	 ************* page fault ****************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x02);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[0] = 1;

	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-7) page fault");

	/*****************************************
	 ************ Test case 8 ****************
	 ********** save to temp reg *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[2] = buf_begin;
	params[3] = "0x0F0F0F0F";
	params[4] = "$1";
	param_num = 5;
	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_test_mask = 0x0F0F0F0F;
	g_step_test_check.step_check_temp_register_id = 1;
	g_step_env.temp_register[0] = 1;
	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do EMI action TC-8) save to temp reg");


	/*****************************************
	 ************ Test case 9 ****************
	 ******** condition invalid **************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[2] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x0e);
	params[3] = buf_end;
	param_num = 4;
	g_step_env.temp_register[0] = 0;

	__wmt_step_test_do_cond_emi_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do COND EMI action TC-9) condition invalid");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do condition EMI action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

int wmt_step_test_get_reg_base_phy_addr(unsigned char buf[], unsigned int index)
{
	struct device_node *node = NULL;
	struct resource res;
	int ret;

	if (g_pdev != NULL) {
		node = g_pdev->dev.of_node;
		if (node) {
			ret = of_address_to_resource(node, index, &res);
			if (ret) {
				WMT_ERR_FUNC("STEP test failed: of_address_to_resource null");
				return ret;
			}
		} else {
			WMT_ERR_FUNC("STEP test failed: node null");
			return -1;
		}
	} else {
		WMT_ERR_FUNC("STEP test failed: gdev null");
		return -1;
	}
	snprintf(buf, 11, "0x%08x", ((unsigned int)res.start));

	return 0;
}

void wmt_step_test_do_register_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf[11];
	int param_num;
	int can_write_offset = 0;
	unsigned char can_write_offset_char[11];

	WMT_INFO_FUNC("STEP test: Do register action start\n");

	can_write_offset =
		 wmt_step_test_find_can_write_register(conn_reg.mcu_base, 0x200, 0x0000000F);
	if (can_write_offset == -1) {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test: Do register action init can_write_offset failed\n");
		return;
	}
	if (snprintf(can_write_offset_char, 11, "0x%08x", can_write_offset) < 0)
		WMT_INFO_FUNC("[%s::%d] snprintf can_write_offset_char fail\n", __func__, __LINE__);

	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_REGISTER;
	/****************************************
	 ************ Test case 1 ***************
	 ******** REG read MCU chip id **********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x08";
	params[3] = "1";
	params[4] = "0";
	param_num = 5;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-1) MCU chip id");

	/****************************************
	 ************ Test case 2 ***************
	 *** REG read cpucpr 5 times / 3ms ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x160";
	params[3] = "5";
	params[4] = "3";
	param_num = 5;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-2) cpucpr 5 times / 3ms");

	/**********************************************
	 *************** Test case 3 ******************
	 ** REG read MCU chip id by physical address **
	 **********************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x08";
		params[3] = "1";
		params[4] = "0";
		param_num = 5;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-3) MCU chip id by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/*********************************************************
	 ********************* Test case 4 ***********************
	 ** REG read cpucpr 5 times / 3ms by physical address **
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x160";
		params[3] = "5";
		params[4] = "3";
		param_num = 5;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-4) cpucpr 5 times / 3ms by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/*****************************************
	 ************* Test case 5 ***************
	 ******** REG read over base size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x11204";
	params[3] = "1";
	params[4] = "0";
	param_num = 5;
	__wmt_step_test_do_register_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do register action TC-5) Over size");

	/******************************************
	 ************** Test case 6 ***************
	 ***** REG read over base size by phy *****
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x204";
		params[3] = "1";
		params[4] = "0";
		param_num = 5;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x204);
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-6) Over size by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 7 ***************
	 *************** REG write ****************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "#1";
	params[2] = can_write_offset_char;
	params[3] = "0x2";
	param_num = 4;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
	g_step_test_check.step_check_write_value = 0x2;
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-7) REG write");

	/******************************************
	 ************** Test case 8 ***************
	 *********** REG write by phy *************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = can_write_offset_char;
		params[3] = "0x7";
		param_num = 4;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
		g_step_test_check.step_check_write_value = 0x7;
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-8) REG write by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 9 ***************
	 ************* REG write bit **************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	params[1] = "#1";
	params[2] = can_write_offset_char;
	params[3] = "0x321";
	params[4] = "0x00F";
	param_num = 5;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
	g_step_test_check.step_check_write_value = 0x001;
	g_step_test_check.step_test_mask = 0x00F;
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-9) REG write bit");

	/******************************************
	 ************** Test case 10 **************
	 ********* REG write bit by phy ***********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "1";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = can_write_offset_char;
		params[3] = "0x32f";
		params[4] = "0x002";
		param_num = 5;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
		g_step_test_check.step_check_write_value = 0x002;
		g_step_test_check.step_test_mask = 0x002;
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-10) REG write bit by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 11 **************
	 ********* REG read to temp reg ***********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x08";
	params[3] = "0x0F0F0F0F";
	params[4] = "$2";
	param_num = 5;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
	g_step_test_check.step_test_mask = 0x0F0F0F0F;
	g_step_test_check.step_check_temp_register_id = 2;
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-11) REG read to temp reg");

	/******************************************
	 ************** Test case 12 **************
	 ******* REG read phy to temp reg *********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x08";
		params[3] = "0x0F0F0F0F";
		params[4] = "$3";
		param_num = 5;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
		g_step_test_check.step_test_mask = 0x0F0F0F0F;
		g_step_test_check.step_check_temp_register_id = 3;
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-12) REG read phy to temp reg");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do register action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_cond_register_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf[11];
	int param_num;
	int can_write_offset = 0;
	unsigned char can_write_offset_char[11];

	WMT_INFO_FUNC("STEP test: Do condition register action start\n");

	can_write_offset =
		 wmt_step_test_find_can_write_register(conn_reg.mcu_base, 0x200, 0x0000000F);
	if (can_write_offset == -1) {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test: Do register action init can_write_offset failed\n");
		return;
	}
	if (snprintf(can_write_offset_char, 11, "0x%08x", can_write_offset) < 0)
		WMT_INFO_FUNC("[%s::%d] snprintf can_write_offset_char fail\n", __func__, __LINE__);

	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CONDITION_REGISTER;
	/****************************************
	 ************ Test case 1 ***************
	 ******** REG read MCU chip id **********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x08";
	params[4] = "1";
	params[5] = "0";
	param_num = 6;
	g_step_env.temp_register[0] = 1;

	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
	__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do cond register action TC-1) MCU chip id");

	/****************************************
	 ************ Test case 2 ***************
	 *** REG read cpucpr 5 times / 3ms ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$1";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x160";
	params[4] = "5";
	params[5] = "3";
	param_num = 6;
	g_step_env.temp_register[1] = 1;

	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
	__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do cond register action TC-2) cpucpr 5 times / 3ms");

	/**********************************************
	 *************** Test case 3 ******************
	 ** REG read MCU chip id by physical address **
	 **********************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$2";
	params[1] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = "0x08";
		params[4] = "1";
		params[5] = "0";
		param_num = 6;
		g_step_env.temp_register[2] = 1;

		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
		__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do cond register action TC-3) MCU chip id by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/*********************************************************
	 ********************* Test case 4 ***********************
	 ** REG read cpucpr 5 times / 3ms by physical address **
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$3";
	params[1] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = "0x160";
		params[4] = "5";
		params[5] = "3";
		param_num = 6;
		g_step_env.temp_register[3] = 11;

		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
		__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do cond register action TC-4) cpucpr 5 times / 3ms by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/*****************************************
	 ************* Test case 5 ***************
	 ******** REG read over base size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$4";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x11204";
	params[4] = "1";
	params[5] = "0";
	param_num = 6;
	g_step_env.temp_register[4] = 10;

	__wmt_step_test_do_cond_register_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do cond register action TC-5) Over size");

	/******************************************
	 ************** Test case 6 ***************
	 ***** REG read over base size by phy *****
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$5";
	params[1] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = "0x204";
		params[4] = "1";
		params[5] = "0";
		param_num = 6;
		g_step_env.temp_register[5] = 1;

		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x204);
		__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do cond register action TC-6) Over size by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 7 ***************
	 *************** REG write ****************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$6";
	params[1] = "1";
	params[2] = "#1";
	params[3] = can_write_offset_char;
	params[4] = "0x2";
	param_num = 5;
	g_step_env.temp_register[6] = 1;

	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
	g_step_test_check.step_check_write_value = 0x2;
	__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do cond register action TC-7) REG write");

	/******************************************
	 ************** Test case 8 ***************
	 *********** REG write by phy *************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$7";
	params[1] = "1";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = can_write_offset_char;
		params[4] = "0x7";
		param_num = 5;
		g_step_env.temp_register[7] = 1;

		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
		g_step_test_check.step_check_write_value = 0x7;
		__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do cond register action TC-8) REG write by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 9 ***************
	 ************* REG write bit **************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$8";
	params[1] = "1";
	params[2] = "#1";
	params[3] = can_write_offset_char;
	params[4] = "0x321";
	params[5] = "0x00F";
	param_num = 6;
	g_step_env.temp_register[8] = 1;

	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
	g_step_test_check.step_check_write_value = 0x001;
	g_step_test_check.step_test_mask = 0x00F;
	__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do cond register action TC-9) REG write bit");

	/******************************************
	 ************** Test case 10 **************
	 ********* REG write bit by phy ***********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$9";
	params[1] = "1";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = can_write_offset_char;
		params[4] = "0x32f";
		params[5] = "0x002";
		param_num = 6;
		g_step_env.temp_register[9] = 1;

		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + can_write_offset);
		g_step_test_check.step_check_write_value = 0x002;
		g_step_test_check.step_test_mask = 0x002;
		__wmt_step_test_do_cond_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do cond register action TC-10) REG write bit by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 11 **************
	 ********* REG read to temp reg ***********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$8";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x08";
	params[4] = "0x0F0F0F0F";
	params[5] = "$2";
	param_num = 6;
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
	g_step_test_check.step_test_mask = 0x0F0F0F0F;
	g_step_test_check.step_check_temp_register_id = 2;
	g_step_env.temp_register[8] = 1;
	__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
		"STEP test case failed: (Do register action TC-11) REG read to temp reg");

	/******************************************
	 ************** Test case 12 **************
	 ******* REG read phy to temp reg *********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$8";
	params[1] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[2] = buf;
		params[3] = "0x08";
		params[4] = "0x0F0F0F0F";
		params[5] = "$3";
		param_num = 6;
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
		g_step_test_check.step_test_mask = 0x0F0F0F0F;
		g_step_test_check.step_check_temp_register_id = 3;
		g_step_env.temp_register[8] = 1;
		__wmt_step_test_do_register_action(act_id, param_num, params, 0, &temp_report,
			"STEP test case failed: (Do register action TC-12) REG read phy to temp reg");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP test case failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 13 **************
	 ************* condition invalid **********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 13\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$8";
	params[1] = "1";
	params[2] = "#1";
	params[3] = "0x160";
	params[4] = "0x123";
	params[5] = "0xF00";
	param_num = 6;
	g_step_env.temp_register[8] = 0;

	__wmt_step_test_do_cond_register_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do cond register action TC-13) condition invalid");

	/******************************************
	 ************** Test case 14 **************
	 ********** condition invalid write *******
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 14\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$6";
	params[1] = "1";
	params[2] = "#1";
	params[3] = "0x110";
	params[4] = "0x200";
	param_num = 5;
	g_step_env.temp_register[6] = 0;

	__wmt_step_test_do_cond_register_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do cond register action TC-14) condition invalid write");

	/******************************************
	 ************** Test case 15 **************
	 ********** condition invalid read *******
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 15\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	wmt_step_test_clear_temp_register();
	params[0] = "$0";
	params[1] = "0";
	params[2] = "#1";
	params[3] = "0x08";
	params[4] = "1";
	params[5] = "0";
	param_num = 6;
	g_step_env.temp_register[0] = 0;

	__wmt_step_test_do_cond_register_action(act_id, param_num, params, -1, &temp_report,
		"STEP test case failed: (Do cond register action TC-15) REG write");


	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do condition register action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}


void wmt_step_test_do_gpio_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do GPIO action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_GPIO;
	/****************************************
	 ************* Test case 1 **************
	 ************* GPIO read #8 *************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "0";
	params[1] = "8";
	param_num = 2;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (wmt_step_do_gpio_action(p_act, NULL) == 0) {
			WMT_INFO_FUNC("STEP check: Do gpio action TC-1(Read #8): search(8: )");
			temp_report.check++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: (Do gpio action TC-1) Read #8\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: (Do gpio action TC-1) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do GPIO action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_chip_reset_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do chip reset action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CHIP_RESET;
	/****************************************
	 ************* Test case 1 **************
	 ************* chip reset ***************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	param_num = 0;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		if (wmt_step_do_chip_reset_action(p_act, NULL) == 0) {
			WMT_INFO_FUNC("STEP check: Do chip reset TC-1(chip reset): Trigger AEE");
			temp_report.check++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: (Do chip reset action TC-1) chip reset\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: (Do chip reset action TC-1) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do chip reset action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_wakeup_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do wakeup action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/****************************************
	 ************* Test case 1 **************
	 ***** Wakeup then read/write reg *******
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	act_id = STEP_ACTION_INDEX_KEEP_WAKEUP;
	param_num = 0;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_keep_wakeup_action(p_act, NULL);
		wmt_step_test_do_register_action(&temp_report);
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: (Do wakeup) Create failed\n");
	}

	act_id = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	param_num = 0;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_cancel_wakeup_action(p_act, NULL);
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: (Do cancel wakeup) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do wakeup action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_show_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do show action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_SHOW_STRING;
	/****************************************
	 ************* Test case 1 **************
	 ********** Show Hello world ************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	params[0] = "Hello_World";
	param_num = 1;

	g_step_test_check.step_check_result_string = "Hello_World";
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_show_string_action(p_act, wmt_step_test_check_show_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Show Hello world)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Show Hello world) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do show action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_sleep_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int check_sec_b, check_sec_e;
	int check_usec_b, check_usec_e;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do sleep action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_SLEEP;
	/****************************************
	 ************* Test case 1 **************
	 *************** Sleep 1s ***************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	params[0] = "1000";
	param_num = 1;

	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		osal_gettimeofday(&check_sec_b, &check_usec_b);
		wmt_step_do_sleep_action(p_act, NULL);
		osal_gettimeofday(&check_sec_e, &check_usec_e);
		if (check_sec_e > check_sec_b) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Sleep 1s), begin(%d.%d) end(%d.%d)\n",
				check_sec_b, check_usec_b, check_sec_e, check_usec_e);
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Sleep 1s) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do sleep action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_condition_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do condition action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_CONDITION;
	/****************************************
	 ************* Test case 1 **************
	 *********** Condition equal ************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "==";
	params[3] = "$2";
	param_num = 4;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-1(equal)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-1(equal) Create failed\n");
	}

	/****************************************
	 ************* Test case 2 **************
	 ********** Condition greater ***********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$0";
	params[1] = "$1";
	params[2] = ">";
	params[3] = "$2";
	param_num = 4;
	g_step_env.temp_register[1] = 0;
	g_step_env.temp_register[2] = 1;

	g_step_test_check.step_check_result_value = 0;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-2(greater)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-2(greater) Create failed\n");
	}

	/****************************************
	 ************* Test case 3 **************
	 ******* Condition greater equal ********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$0";
	params[1] = "$1";
	params[2] = ">=";
	params[3] = "$2";
	param_num = 4;
	g_step_env.temp_register[1] = 2;
	g_step_env.temp_register[2] = 2;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-3(greater equal)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-3(greater equal) Create failed\n");
	}

	/****************************************
	 ************* Test case 4 **************
	 ************ Condition less ************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "<";
	params[3] = "$2";
	param_num = 4;
	g_step_env.temp_register[1] = 10;
	g_step_env.temp_register[2] = 0;

	g_step_test_check.step_check_result_value = 0;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-4(less)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-4(less) Create failed\n");
	}

	/****************************************
	 ************* Test case 5 **************
	 ********* Condition less equal *********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$0";
	params[1] = "$1";
	params[2] = "<=";
	params[3] = "$2";
	param_num = 4;
	g_step_env.temp_register[1] = 0;
	g_step_env.temp_register[2] = 10;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-5(less equal)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-5(less equal) Create failed\n");
	}

	/****************************************
	 ************* Test case 6 **************
	 ********* Condition not equal **********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "!=";
	params[3] = "$3";
	param_num = 4;
	g_step_env.temp_register[2] = 3;
	g_step_env.temp_register[3] = 3;

	g_step_test_check.step_check_result_value = 0;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-6(not equal)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-6(not equal) Create failed\n");
	}

	/****************************************
	 ************* Test case 7 **************
	 ************ Condition and *************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "&&";
	params[3] = "$3";
	param_num = 4;
	g_step_env.temp_register[2] = 0x10;
	g_step_env.temp_register[3] = 0x00;

	g_step_test_check.step_check_result_value = 0;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-7(and)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-7(and) Create failed\n");
	}

	/****************************************
	 ************* Test case 8 **************
	 ************* Condition or *************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "||";
	params[3] = "$3";
	param_num = 4;
	g_step_env.temp_register[2] = 0x10;
	g_step_env.temp_register[3] = 0x00;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-8(or)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-8(or) Create failed\n");
	}

	/****************************************
	 ************* Test case 9 **************
	 ****** Condition not equal value *******
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "!=";
	params[3] = "99";
	param_num = 4;
	g_step_env.temp_register[2] = 99;

	g_step_test_check.step_check_result_value = 0;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-9(not equal value)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-9(not equal value) Create failed\n");
	}

	/****************************************
	 ************* Test case 10 *************
	 ********* Condition equal value ********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "==";
	params[3] = "18";
	param_num = 4;
	g_step_env.temp_register[2] = 18;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-10(equal value)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-10(equal value) Create failed\n");
	}

	/****************************************
	 ************* Test case 11 *************
	 ****** Condition equal value (HEX) *****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$1";
	params[1] = "$2";
	params[2] = "==";
	params[3] = "0x18";
	param_num = 4;
	g_step_env.temp_register[2] = 24;

	g_step_test_check.step_check_result_value = 1;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_condition_action(p_act, wmt_step_test_check_condition_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do condition TC-11(equal value HEX)\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do condition TC-11(equal value HEX) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do condition action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_value_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_PARAMETER_SIZE];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	int param_num;

	WMT_INFO_FUNC("STEP test: Do value action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	act_id = STEP_ACTION_INDEX_VALUE;
	/****************************************
	 ************* Test case 1 **************
	 ******* Save value to register *********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_temp_register();
	wmt_step_test_clear_parameter(params);
	params[0] = "$2";
	params[1] = "0x66";
	param_num = 2;

	g_step_test_check.step_check_result_value = 0x66;
	p_act = wmt_step_create_action(act_id, param_num, params);
	if (p_act != NULL) {
		wmt_step_do_value_action(p_act, wmt_step_test_check_value_act);
		if (g_step_test_check.step_check_result == TEST_PASS) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Save value to register)");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	} else {
		temp_report.fail++;
		WMT_ERR_FUNC("STEP test case failed: Do show TC-1(Save value to register) Create failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Do value action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_periodic_dump(struct step_test_report *p_report)
{
	int expires_ms;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	struct step_pd_entry *p_current = NULL;
	bool is_thread_run_for_test = 0;

	WMT_INFO_FUNC("STEP test: Create periodic dump start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);

	if (g_step_env.pd_struct.step_pd_wq == NULL) {
		if (wmt_step_init_pd_env() != 0) {
			WMT_ERR_FUNC("STEP test case failed: Start thread failed\n");
			return;
		}
		is_thread_run_for_test = 1;
	}

	/****************************************
	 ************* Test case 1 **************
	 *************** Normal *****************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	expires_ms = 5;
	p_current = wmt_step_get_periodic_dump_entry(expires_ms);
	if (p_current == NULL) {
		WMT_ERR_FUNC("STEP test case failed: (Create periodic dump TC-1) No entry\n");
		temp_report.fail++;
	} else {
		if (p_current->expires_ms == expires_ms) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP test case failed: (Create periodic dump TC-1) Currect %d not %d\n",
				p_current->expires_ms, expires_ms);
			temp_report.fail++;
		}
		list_del_init(&p_current->list);
		kfree(p_current);
	}

	if (is_thread_run_for_test == 1)
		wmt_step_deinit_pd_env();

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP result: Create periodic dump result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

