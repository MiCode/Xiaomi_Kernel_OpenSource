#ifndef __CMDQ_PLATFORM_H__
#define __CMDQ_PLATFORM_H__

/* platform dependent utilities, format: cmdq_{util_type}_{name} */

#include "cmdq_def.h"
#include "cmdq_core.h"

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
void cmdq_core_fix_command_desc_scenario_for_user_space_request(cmdqCommandStruct *pCommand);
bool cmdq_core_is_request_from_user_space(const CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_is_disp_scenario(const CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_should_enable_prefetch(CMDQ_SCENARIO_ENUM scenario);
bool cmdq_core_should_profile(CMDQ_SCENARIO_ENUM scenario);
int cmdq_core_get_thread_index_from_scenario_and_secure_data(CMDQ_SCENARIO_ENUM scenario, const bool secure);
int cmdq_core_disp_thread_index_from_scenario(CMDQ_SCENARIO_ENUM scenario);
CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(CMDQ_SCENARIO_ENUM scenario);
int32_t cmdq_platform_get_thread_index_trigger_loop(void);
bool cmdq_platform_force_loop_irq_from_scenario(CMDQ_SCENARIO_ENUM scenario);
bool cmdq_platform_is_loop_scenario(CMDQ_SCENARIO_ENUM scenario, bool displayOnly);

/**
 * Module dependent
 *
 */
void cmdq_core_get_reg_id_from_hwflag(uint64_t hwflag,
				      CMDQ_DATA_REGISTER_ENUM *valueRegId,
				      CMDQ_DATA_REGISTER_ENUM *destRegId,
				      CMDQ_EVENT_ENUM *regAccessToken);
const char *cmdq_core_module_from_event_id(CMDQ_EVENT_ENUM event, uint32_t instA, uint32_t instB);
const char *cmdq_core_parse_module_from_reg_addr(uint32_t reg_addr);
const int32_t cmdq_core_can_module_entry_suspend(EngineStruct *engineList);

ssize_t cmdq_core_print_status_clock(char *buf);
void cmdq_core_print_status_seq_clock(struct seq_file *m);
void cmdq_core_enable_common_clock_locked_impl(bool enable);
void cmdq_core_enable_gce_clock_locked_impl(bool enable);

void cmdq_core_enable_cmdq_clock_locked_impl(bool enable, char *deviceName);

const char* cmdq_core_parse_error_module_by_hwflag_impl(struct TaskStruct *pTask);

/**
 * Debug
 *
 */

/* use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print */
#define CMDQ_FOREACH_STATUS_MODULE_PRINT(ACTION)\
    ACTION(CMDQ_ENG_ISP_IMGI,   ISP_IMGI) \
    ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0) \
    ACTION(CMDQ_ENG_MDP_RDMA1,  MDP_RDMA1) \
    ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0) \
    ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1) \
    ACTION(CMDQ_ENG_MDP_RSZ2,   MDP_RSZ2) \
    ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0) \
    ACTION(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1) \
    ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0) \
    ACTION(CMDQ_ENG_MDP_WROT1,  MDP_WROT1) \
    ACTION(CMDQ_ENG_MDP_WDMA,   MDP_WDMA)


void cmdq_core_dump_mmsys_config(void);
void cmdq_core_dump_clock_gating(void);
int cmdq_core_dump_smi(const int showSmiDump);
void cmdq_core_gpr_dump(void);
void cmdq_core_dump_secure_metadata(cmdqSecDataStruct *pSecData);

/**
 * Record usage
 *
 */
uint64_t cmdq_rec_flag_from_scenario(CMDQ_SCENARIO_ENUM scn);

/**
 * Test
 *
 */
void cmdq_test_setup(void);
void cmdq_test_cleanup(void);

#endif				/* __CMDQ_PLATFORM_H__ */
