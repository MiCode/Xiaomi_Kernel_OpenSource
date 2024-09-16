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

#ifndef _WMT_STEP_TEST_H_
#define _WMT_STEP_TEST_H_

#include "osal.h"
#include "wmt_step.h"

#define STEP_TEST_CONSYS_EMI_WMT_OFFSET 0x68000

#define TEST_FAIL -1
#define TEST_PASS 0
#define TEST_CHECK 1
#define STEP_TEST_ACTION_NUMBER 30

extern struct step_env_struct g_step_env;
extern struct platform_device *g_pdev;

struct step_test_report {
	unsigned int pass;
	unsigned int fail;
	unsigned int check;
};

struct step_test_check {
	unsigned int step_check_total;
	int step_check_test_tp_id[STEP_TEST_ACTION_NUMBER];
	int step_check_test_act_id[STEP_TEST_ACTION_NUMBER];
	char *step_check_params[STEP_TEST_ACTION_NUMBER][STEP_PARAMETER_SIZE];
	int step_check_params_num[STEP_TEST_ACTION_NUMBER];
	unsigned int step_check_index;
	int step_check_result;
	char *step_check_result_string;
	int step_check_result_value;
	unsigned int step_check_emi_offset[STEP_TEST_ACTION_NUMBER];
	SIZE_T step_check_register_addr;
	SIZE_T step_check_write_value;
	unsigned int step_test_mask;
	unsigned int step_recovery_value;
	int step_check_temp_register_id;
};

void wmt_step_test_all(void);
void wmt_step_test_read_file(struct step_test_report *p_report);
void wmt_step_test_parse_data(struct step_test_report *p_report);
void wmt_step_test_create_emi_action(struct step_test_report *p_report);
void wmt_step_test_create_cond_emi_action(struct step_test_report *p_report);
void wmt_step_test_create_register_action(struct step_test_report *p_report);
void wmt_step_test_create_cond_register_action(struct step_test_report *p_report);
void wmt_step_test_check_register_symbol(struct step_test_report *p_report);
void wmt_step_test_create_other_action(struct step_test_report *p_report);
void wmt_step_test_do_emi_action(struct step_test_report *p_report);
void wmt_step_test_do_cond_emi_action(struct step_test_report *p_report);
void wmt_step_test_do_register_action(struct step_test_report *p_report);
void wmt_step_test_do_cond_register_action(struct step_test_report *p_report);
void wmt_step_test_do_gpio_action(struct step_test_report *p_report);
void wmt_step_test_do_disable_reset_action(struct step_test_report *p_report);
void wmt_step_test_do_chip_reset_action(struct step_test_report *p_report);
void wmt_step_test_do_wakeup_action(struct step_test_report *p_report);
void wmt_step_test_create_periodic_dump(struct step_test_report *p_report);
void wmt_step_test_do_show_action(struct step_test_report *p_report);
void wmt_step_test_do_sleep_action(struct step_test_report *p_report);
void wmt_step_test_do_condition_action(struct step_test_report *p_report);
void wmt_step_test_do_value_action(struct step_test_report *p_report);

#endif /* end of _WMT_STEP_TEST_H_ */

