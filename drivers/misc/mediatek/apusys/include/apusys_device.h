/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEVICE_H__
#define __APUSYS_DEVICE_H__

#include <linux/dma-buf.h>


/* device type */
enum {
	APUSYS_DEVICE_NONE,

	APUSYS_DEVICE_SAMPLE,
	APUSYS_DEVICE_MDLA,
	APUSYS_DEVICE_VPU,
	APUSYS_DEVICE_EDMA,
	APUSYS_DEVICE_EDMA_LITE,
	APUSYS_DEVICE_MVPU,
	APUSYS_DEVICE_UP,
	APUSYS_DEVICE_LAST,
	APUSYS_DEVICE_RT = 32,
	APUSYS_DEVICE_SAMPLE_RT,
	APUSYS_DEVICE_MDLA_RT,
	APUSYS_DEVICE_VPU_RT,
	APUSYS_DEVICE_EDMA_LITE_RT,
	APUSYS_DEVICE_MVPU_RT,
	APUSYS_DEVICE_UP_RT,

	APUSYS_DEVICE_MAX = 64, //total support 64 different devices
};

/* device cmd type */
enum {
	APUSYS_CMD_POWERON,
	APUSYS_CMD_POWERDOWN,
	APUSYS_CMD_RESUME,
	APUSYS_CMD_SUSPEND,
	APUSYS_CMD_EXECUTE,
	APUSYS_CMD_PREEMPT,

	APUSYS_CMD_FIRMWARE,
	APUSYS_CMD_USER,
	APUSYS_CMD_VALIDATE,
	APUSYS_CMD_SESSION_CREATE,
	APUSYS_CMD_SESSION_DELETE,

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

/* handle definition for send_cmd */
struct apusys_power_hnd {
	uint32_t opp;
	int boost_val;

	uint32_t timeout;
};

struct apusys_cmdbuf {
	void *kva;
	uint32_t size;
};

struct apusys_cmd_valid_handle {
	void *session;
	uint32_t num_cmdbufs;
	struct apusys_cmdbuf *cmdbufs;
};

struct apusys_cmd_handle {
	struct apusys_cmdbuf *cmdbufs;
	uint32_t num_cmdbufs;

	uint64_t kid;
	uint32_t subcmd_idx;
	uint32_t boost;
	uint32_t ip_time;

	uint32_t multicore_total;
	int cluster_size;

	int (*context_callback)(int a, int b, uint8_t c);
	uint32_t vlm_ctx;
};

struct apusys_usercmd_hnd {
	uint64_t kva;
	uint32_t iova;
	uint32_t size;
};

/* device definition */
#define APUSYS_DEVICE_META_SIZE (32)

struct apusys_device {
	uint32_t dev_type;
	int idx;
	int preempt_type;
	uint8_t preempt_level;
	char meta_data[APUSYS_DEVICE_META_SIZE];

	void *private; // for hw driver to record private structure

	int (*send_cmd)(int type, void *hnd, struct apusys_device *dev);
};

/*
 * export function for hardware driver
 * @apusys_register_device
 *  Description:
 *    for drvier register device to resource management,
 *    caller should allocate struct apusys_device struct byself
 *    midware can schedule task and preempt by current requests status
 *    1. user should implement 6 callback function for midware
 *  Parameters:
 *    <dev>
 *      dev_type: apusys device type, define as APUSYS_DEVICE_CMD_E
 *      private: midware don't touch this,
 *        it's for user record some private data,
 *        midware will pass to driver when send_cmd()
 *      preempt_type: not use
 *      preempt_level: not use
 *      send_cmd: 6 callback user should implement
 *        1. poweron: poweron device with specific opp
 *          return:
 *            success: 0
 *            fail: error code
 *        2. poweroff: poweroff device
 *          return:
 *            success: 0
 *            fail: error code
 *        3. suspend: suspend device
 *        4. resume: resume device
 *        5. excute: execute command with device
 *          parameter:
 *          reutrn:
 *            success: cmd id
 *            fail: 0
 *        6. preempt: not support, preemption should trigger by rt dev
 *  return:
 *      success: 0
 *      fail: linux error code (negative value)
 */
int apusys_register_device(struct apusys_device *dev);
int apusys_unregister_device(struct apusys_device *dev);


int apusys_mem_get_by_iova(void *session, uint64_t iova);
void *apusys_mem_query_kva_by_sess(void *session, uint64_t iova);

uint64_t apusys_mem_query_kva(uint64_t iova);
uint64_t apusys_mem_query_iova(uint64_t kva);

int apusys_mem_flush_kva(void *kva, uint32_t size);
int apusys_mem_invalidate_kva(void *kva, uint32_t size);

#endif
