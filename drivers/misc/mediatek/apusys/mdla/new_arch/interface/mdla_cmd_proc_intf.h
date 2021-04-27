/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_PROC_INTF_H__
#define __MDLA_CMD_PROC_INTF_H__

#include <common/mdla_device.h>

/* MDLA HW v1.0 */
int mdla_cmd_run_sync_v1_0(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v1_0(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

/* MDLA HW v1.5 and v1.7 */
int mdla_cmd_run_sync_v1_x(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v1_x(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

/* MDLA HW v1.5 and v1.7 with SW preemption */
int mdla_cmd_run_sync_v1_x_sched(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v1_x_sched(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

/* MDLA HW v2.0 */
int mdla_cmd_run_sync_v2_0(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v2_0(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

/* MDLA HW v2.0 with HW preemption */
int mdla_cmd_run_sync_v2_0_hw_sched(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v2_0_hw_sched(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

/* MDLA HW v2.0 with SW preemption */
int mdla_cmd_run_sync_v2_0_sw_sched(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				uint32_t priority);
int mdla_cmd_ut_run_sync_v2_0_sw_sched(void *run_cmd, void *wait_cmd,
				struct mdla_dev *mdla_info);

#endif /* __MDLA_CMD_PROC_INTF_H__ */

