/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CMDQ_PLATFORM_H__
#define __CMDQ_PLATFORM_H__

/* platform dependent utilities, format: cmdq_{util_type}_{name} */

#include "cmdq_def.h"
#include "cmdq_core.h"
#include "cmdq_platform_idv.h"

/*
 * GCE capability
 */
const bool cmdq_core_support_sync_non_suspendable(void);
const bool cmdq_core_support_wait_and_receive_event_in_same_tick(void);

/* get LSB for subsys encoding in argA (range: 0 - 31)*/
const uint32_t cmdq_core_get_subsys_LSB_in_argA(void);

/* HW thread related */
const bool cmdq_core_is_a_secure_thread(const int32_t thread);
const bool cmdq_core_is_valid_notify_thread_for_secure_path(const int32_t thread);
/**
 * Scenario related
 *
 */
bool cmdq_core_is_request_from_user_space(const enum CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_is_disp_scenario(const enum CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_should_enable_prefetch(enum CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_should_profile(enum CMDQ_SCENARIO_ENUM scenario);
int cmdq_core_get_thread_index_from_scenario_and_secure_data(enum CMDQ_SCENARIO_ENUM scenario,
							     const bool secure);
int cmdq_core_disp_thread_index_from_scenario(enum CMDQ_SCENARIO_ENUM scenario);
enum CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(enum CMDQ_SCENARIO_ENUM
								   scenario);

/**
 * Module dependent
 *
 */
void cmdq_core_get_reg_id_from_hwflag(uint64_t hwflag, enum CMDQ_DATA_REGISTER_ENUM *valueRegId,
				      enum CMDQ_DATA_REGISTER_ENUM *destRegId,
				      enum CMDQ_EVENT_ENUM *regAccessToken);
const char *cmdq_core_module_from_event_id(enum CMDQ_EVENT_ENUM event, uint32_t instA,
					   uint32_t instB);
const char *cmdq_core_parse_module_from_reg_addr(uint32_t reg_addr);
const int32_t cmdq_core_can_module_entry_suspend(struct EngineStruct *engineList);

ssize_t cmdq_core_print_status_clock(char *buf);
void cmdq_core_print_status_seq_clock(struct seq_file *m);
void cmdq_core_enable_common_clock_locked_impl(bool enable);
void cmdq_core_enable_gce_clock_locked_impl(bool enable);
void cmdq_core_enable_cmdq_clock_locked_impl(bool enable, char *deviceName);

const char *cmdq_core_parse_error_module_by_hwflag_impl(struct TaskStruct *pTask);
/**
 * Debug
 *
 */


void cmdq_core_dump_clock_gating(void);
int cmdq_core_dump_smi(const int showSmiDump);
void cmdq_core_gpr_dump(void);
void cmdq_core_dump_secure_metadata(cmdqSecDataStruct *pSecData);
ssize_t cmdq_core_print_event(char *buf);
void cmdq_core_print_event_seq(struct seq_file *m);

/**
 * Record usage
 *
 */
uint64_t cmdq_rec_flag_from_scenario(enum CMDQ_SCENARIO_ENUM scn);

/**
 * Test
 *
 */
void cmdq_test_setup(void);
void cmdq_test_cleanup(void);

#endif				/* #ifndef __CMDQ_PLATFORM_H__ */
