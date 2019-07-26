/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_DEVICE_H__
#define __APUSYS_DEVICE_H__

/* device type */
enum {
	APUSYS_DEVICE_NONE,
	APUSYS_DEVICE_SAMPLE,

	APUSYS_DEVICE_MDLA,
	APUSYS_DEVICE_VPU,
	APUSYS_DEVICE_EDMA,
	APUSYS_DEVICE_WAIT, // subgraph mean wait event

	APUSYS_DEVICE_MAX,
};

/* device cmd type */
enum {
	APUSYS_CMD_POWERON,
	APUSYS_CMD_POWERDOWN,
	APUSYS_CMD_RESUME,
	APUSYS_CMD_SUSPEND,
	APUSYS_CMD_EXECUTE,
	APUSYS_CMD_PREEMPT,

	APUSYS_CMD_FIRMWARE, // setup firmware

	APUSYS_CMD_MAX,
};

/* device preempt type */
enum {
	// device don't support preemption
	APUSYS_PREEMPT_NONE,
	// midware should resend preempted command after preemption call
	APUSYS_PREEMPT_RESEND,
	// midware don't need to resend cmd, and wait preempted cmd completed
	APUSYS_PREEMPT_WAITCOMPLETED,

	APUSYS_PREEMPT_MAX,
};

enum {
	APUSYS_FIRMWARE_UNLOAD,
	APUSYS_FIRMWARE_LOAD,
};

/* handle definition for send_cmd */
struct apusys_power_hnd {
	uint32_t opp;
	int boost_val;
};

struct apusys_pmu_info {
	uint64_t kva;
	uint32_t iova;
	uint32_t size;
};

/* cmd handle */
struct apusys_cmd_hnd {
	uint64_t kva;
	uint32_t iova;
	uint32_t size;

	uint64_t pmu_kva;

	int boost_val;
};

struct apusys_firmware_hnd {
	void *kva;
	uint32_t size;

	int load;
};

struct apusys_preempt_hnd {
	struct apusys_cmd_hnd *new_cmd;
	struct apusys_cmd_hnd *old_cmd; //TODO
};

/* device definition */
struct apusys_device {
	int dev_type;
	int preempt_type;
	uint8_t preempt_level;

	void *private; // for hw driver to record private structure

	int (*send_cmd)(int type, void *hnd, struct apusys_device *dev);
};

/*
 * export function for hardware driver
 * @apusys_register_device
 *  Description:
 *      for drvier register device to resource management,
 *      caller should allocate struct apusys_device struct byself
 *      midware can schedule task and preempt by current requests status
 *      1. user should implement 6 callback function for midware
 *  Parameters:
 *      <dev>
 *          dev_type: apusys device type, define as APUSYS_DEVICE_CMD_E
 *          private: midware don't touch this,
 *              it's for user record some private data,
 *              midware will pass to driver when send_cmd()
 *          preempt_type:
 *          preempt_level:
 *          send_cmd: 6 callback user should implement
 *              1. poweron: poweron device with specific opp
 *                  return:
 *                      success: 0
 *                   fail: error code
 *              2. poweroff: poweroff device
 *                  return:
 *                      success: 0
 *                      fail: error code
 *              3. suspend: suspend device (TODO)
 *              4. resume: resume device (TODO)
 *              5. excute: execute command with device
 *                  parameter:
 *                  reutrn:
 *                      success: cmd id
 *                      fail: 0
 *             6. preempt: preempt device and run new command
 *  return:
 *      success: 0
 *      fail: linux error code (negative value)
 */
extern int apusys_register_device(struct apusys_device *dev);
extern int apusys_unregister_device(struct apusys_device *dev);

#endif
