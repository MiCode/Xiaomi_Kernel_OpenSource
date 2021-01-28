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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/atomic.h>
#include <linux/mailbox_controller.h>
#include <linux/sched/clock.h>
#include <linux/iopoll.h>

#include "cmdq_record_private.h"
#include "cmdq_reg.h"
#include "cmdq_virtual.h"
#include "mdp_common.h"
#include "mdp_cmdq_device.h"

#ifdef CMDQ_CONFIG_SMI
#include "smi_public.h"
#endif


#define CMDQ_TEST

#ifdef CMDQ_TEST

#define CMDQ_TESTCASE_PARAMETER_MAX		4
#define CMDQ_MONITOR_EVENT_MAX			10

#if 0
#define CMDQ_TEST_MMSYS_DUMMY_OFFSET (0x40C)

#define CMDQ_TEST_MMSYS_DUMMY_PA (0x14000000 + CMDQ_TEST_MMSYS_DUMMY_OFFSET)
#define CMDQ_TEST_MMSYS_DUMMY_VA (cmdq_mdp_get_module_base_VA_MMSYS_CONFIG() + \
	CMDQ_TEST_MMSYS_DUMMY_OFFSET)
#endif

#define CMDQ_TEST_MMSYS_DUMMY_PA	CMDQ_THR_SPR3_PA(3)
#define CMDQ_TEST_MMSYS_DUMMY_VA	CMDQ_THR_SPR3(3)

#define CMDQ_TEST_GCE_DUMMY_PA CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_1)
#define CMDQ_TEST_GCE_DUMMY_VA CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_1)

/* test configuration */
static DEFINE_MUTEX(gCmdqTestProcLock);

enum CMDQ_TEST_TYPE_ENUM {
	CMDQ_TEST_TYPE_NORMAL = 0,
	CMDQ_TEST_TYPE_SECURE = 1,
	CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP = 4,
	CMDQ_TEST_TYPE_FEATURE_CONFIG = 6,
	CMDQ_TEST_TYPE_MMSYS_PERFORMANCE = 7,

	CMDQ_TEST_TYPE_MAX	/* ALWAYS keep at the end */
};

static s64 gCmdqTestConfig[CMDQ_MONITOR_EVENT_MAX];
static bool gCmdqTestSecure;
#ifdef CMDQ_TEST_PROC
static struct proc_dir_entry *gCmdqTestProcEntry;
#endif

#define CMDQ_TEST_FAIL(string, args...) \
do { \
	if (1) { \
		pr_notice("[CMDQ][ERR]TEST FAIL: "string, ##args); \
	} \
} while (0)

/* wrapper of cmdq_pkt_flush_async_ex */
static s32 _test_flush_async(struct cmdqRecStruct *handle)
{
	return cmdq_pkt_flush_async_ex(handle, NULL, 0, false);
}

/* call flush or mdp flush by check scenario */
static s32 _test_flush_task(struct cmdqRecStruct *handle)
{
	s32 status;

	if (cmdq_get_func()->isDynamic(handle->scenario)) {
		if (!handle->finalized)
			cmdq_op_finalize_command(handle, false);
		/* go mdp path dispatch and wait */
		status = cmdq_mdp_flush_async_impl(handle);
		if (status < 0)
			CMDQ_ERR("flush failed handle:0x%p\n", handle);
		else
			status = cmdq_mdp_wait(handle, NULL);
	} else {
		/* use static thread flush */
		status = cmdq_task_flush(handle);
	}

	return status;
}

/* call async flush or async mdp flush by check scenario */
static s32 _test_flush_task_async(struct cmdqRecStruct *handle)
{
	s32 status;

	if (cmdq_get_func()->isDynamic(handle->scenario)) {
		/* go mdp path dispatch and wait */
		status = cmdq_mdp_flush_async_impl(handle);
		if (status < 0)
			CMDQ_ERR("flush failed handle:0x%p\n", handle);
	} else {
		/* use static thread flush */
		status = cmdq_pkt_flush_async_ex(handle, NULL, 0, false);
	}

	return status;
}

static s32 _test_wait_task(struct cmdqRecStruct *handle)
{
	s32 status;

	if (cmdq_get_func()->isDynamic(handle->scenario))
		status = cmdq_mdp_wait(handle, NULL);
	else
		status = cmdq_pkt_wait_flush_ex_result(handle);

	return status;
}



#if 0
static s32 _test_submit_sync(struct cmdqRecStruct *handle, bool ignore_timeout)
{
#if 0
	struct cmdqCommandStruct desc = { 0 };
	struct task_private private = {
		.node_private_data = NULL,
		.internal = true,
		.ignore_timeout = ignore_timeout,
	};
	s32 status;

	status = cmdq_op_finalize_command(handle, false);
	if (status < 0)
		return status;

	CMDQ_MSG(
		"Submit task scenario:%d priority:%d engine:0x%llx buffer:0x%p size:%zu\n",
		handle->scenario, handle->pkt->priority, handle->engineFlag,
		handle->pkt->va_base, handle->pkt->cmd_buf_size);

	desc.scenario = handle->scenario;
	desc.priority = handle->pkt->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pkt->va_base;
	desc.blockSize = handle->pkt->cmd_buf_size;
	desc.privateData = (cmdqU32Ptr_t)(unsigned long)&private;
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* replace instuction position */
	cmdq_setup_replace_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTask(&desc, &handle->ext);
#else
	return 0;
#endif

}

s32 _test_backup_instructions(struct cmdqRecStruct *task,
	s32 **instructions_out)
{
	s32 *insts_buffer = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 buffer_count = 0;

	insts_buffer = vzalloc(task->bufferSize);
	if (!insts_buffer)
		return -ENOMEM;

	list_for_each_entry(cmd_buffer, &task->cmd_buffer_list, listEntry) {
		u32 buf_size = list_is_last(&cmd_buffer->listEntry,
			&task->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - task->buf_available_size :
			CMDQ_CMD_BUFFER_SIZE;

		memcpy(insts_buffer + CMDQ_CMD_BUFFER_SIZE / sizeof(s32) *
			buffer_count, cmd_buffer->pVABase, buf_size);
		buffer_count++;
	}

	*instructions_out = insts_buffer;

	return 0;
}

void _test_free_backup_instructions(s32 **instructions_out)
{
	if (*instructions_out)
		vfree(*instructions_out);
	*instructions_out = NULL;
}
#endif

static void testcase_scenario(void)
{
	struct cmdqRecStruct *handle;
	s32 ret;
	int i = 0;

	CMDQ_LOG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	for (i = 0; i < CMDQ_MAX_SCENARIO_COUNT; i++) {
		if (cmdq_mdp_is_request_from_user_space(i) ||
			i == CMDQ_SCENARIO_TIMER_LOOP)
			continue;

		cmdq_task_create((enum CMDQ_SCENARIO_ENUM)i, &handle);
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, false);
		ret = _test_flush_task(handle);
		if (ret < 0) {
			CMDQ_TEST_FAIL("scenario fail:%d ret:%d\n",
				i, ret);
		}

		cmdq_task_destroy(handle);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static struct timer_list test_timer;
static bool test_timer_stop;

static void _testcase_sync_token_timer_func(unsigned long data)
{
	/* trigger sync event */
	CMDQ_MSG("trigger event:0x%08lx\n", (1L << 16) | data);
	cmdqCoreSetEvent(data);
}

static void _testcase_sync_token_timer_loop_func(unsigned long data)
{
	/* trigger sync event */
	CMDQ_MSG("trigger event:0x%08lx\n", (1L << 16) | data);
	cmdqCoreSetEvent(data);

	if (test_timer_stop) {
		del_timer(&test_timer);
		return;
	}

	/* repeate timeout until user delete it */
	mod_timer(&test_timer, jiffies + msecs_to_jiffies(10));
}

static void testcase_sync_token(void)
{
	struct cmdqRecStruct *hRec;
	s32 ret = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hRec);

	do {
		cmdq_task_reset(hRec);
		cmdq_task_set_secure(hRec, gCmdqTestSecure);

		/* setup timer to trigger sync token */
		setup_timer(&test_timer, &_testcase_sync_token_timer_func,
			CMDQ_SYNC_TOKEN_USER_0);
		mod_timer(&test_timer, jiffies + msecs_to_jiffies(1000));

		/* wait for sync token */
		cmdq_op_wait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdq_task_flush(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
		del_timer(&test_timer);
	} while (0);

	CMDQ_MSG("%s, timeout case\n", __func__);
	/*  */
	/* test for timeout */
	/*  */
	do {
		cmdq_task_reset(hRec);
		cmdq_task_set_secure(hRec, gCmdqTestSecure);

		/* wait for sync token */
		cmdq_op_wait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdq_task_flush(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
	} while (0);

	cmdq_task_destroy(hRec);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_errors(void)
{
	struct cmdqRecStruct *handle;
	s32 ret;
	const u8 UNKNOWN_OP = 0x50;

	ret = 0;
	do {
		/* SW timeout */
		CMDQ_LOG("=============== INIFINITE Wait ===============\n");

		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

		/* turn on ALL engine flag to test dump */
		for (ret = 0; ret < CMDQ_MAX_ENGINE_COUNT; ++ret)
			handle->engineFlag |= 1LL << ret;

		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_op_wait(handle, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_flush(handle);
		cmdq_core_reset_first_dump();
#if 0
		CMDQ_LOG("=============== INIFINITE JUMP ===============\n");

		/* HW timeout */
		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_op_wait(handle, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_append_command(handle, CMDQ_CODE_EOC, 0, 1, 0, 0);
		/* JUMP to connect tasks */
		cmdq_append_command(handle, CMDQ_CODE_JUMP, 0, 8, 0, 0);
		ret = _test_flush_async(handle);
		msleep_interruptible(500);
		ret = cmdq_pkt_wait_flush_ex_result(handle);
		cmdq_core_reset_first_dump();
#endif

		CMDQ_LOG("=============== POLL INIFINITE ===============\n");

		CMDQ_MSG("testReg: %lx\n", CMDQ_TEST_GCE_DUMMY_VA);

		CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0x0);
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_op_poll(handle, CMDQ_TEST_GCE_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdq_task_flush(handle);
		cmdq_core_reset_first_dump();

		CMDQ_LOG("=============== INVALID INSTR ===============\n");

		/* invalid instruction */
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_append_command(handle, CMDQ_CODE_JUMP, -1, 0, 0, 0);
		cmdq_task_flush(handle);
		cmdq_core_reset_first_dump();

		CMDQ_LOG("======= INVALID INSTR: UNKNOWN OP(0x%x) =======\n",
			UNKNOWN_OP);

		/* invalid instruction is asserted when unknown OP */
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_pkt_append_command(handle->pkt, 0, 0, 0, 0, 0, 0, 0,
			UNKNOWN_OP);
		cmdq_task_flush(handle);
		cmdq_core_reset_first_dump();
	} while (0);

	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static s32 finishCallback(unsigned long data)
{
	CMDQ_LOG("callback() with data=0x%08lx\n", data);
	return 0;
}

static void testcase_fire_and_forget(void)
{
	struct cmdqRecStruct *hReqA, *hReqB;

	CMDQ_LOG("%s\n", __func__);
	do {
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReqA);
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReqB);
		cmdq_task_reset(hReqA);
		cmdq_task_reset(hReqB);
		cmdq_task_set_secure(hReqA, gCmdqTestSecure);
		cmdq_task_set_secure(hReqB, gCmdqTestSecure);

		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdq_task_flush_async(hReqA);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdq_task_flush_async_callback(hReqB, finishCallback, 443);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
	} while (0);

	cmdq_task_destroy(hReqA);
	cmdq_task_destroy(hReqB);

	CMDQ_LOG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_request(void)
{
	struct cmdqRecStruct *hReqA, *hReqB;
	s32 ret = 0;

	CMDQ_LOG("%s\n", __func__);

	/* setup timer to trigger sync token */
	setup_timer(&timer_reqA, &_testcase_sync_token_timer_func,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(1000));

	setup_timer(&timer_reqB, &_testcase_sync_token_timer_func,
		CMDQ_SYNC_TOKEN_USER_1);
	/* mod_timer(&timer_reqB, jiffies + msecs_to_jiffies(1300)); */

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_1);

	do {
		cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hReqA);
		cmdq_task_reset(hReqA);
		cmdq_task_set_secure(hReqA, gCmdqTestSecure);
		cmdq_op_wait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1, 0, 0);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8, 0, 0);

		cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hReqB);
		cmdq_task_reset(hReqB);
		cmdq_task_set_secure(hReqB, gCmdqTestSecure);
		cmdq_op_wait(hReqB, CMDQ_SYNC_TOKEN_USER_1);
		cmdq_append_command(hReqB, CMDQ_CODE_EOC, 0, 1, 0, 0);
		cmdq_append_command(hReqB, CMDQ_CODE_JUMP, 0, 8, 0, 0);

		ret = _test_flush_async(hReqA);
		ret = _test_flush_async(hReqB);

		CMDQ_MSG("%s start wait sleep========\n", __func__);
		msleep_interruptible(500);

		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdq_pkt_wait_flush_ex_result(hReqA);

		CMDQ_MSG("%s start wait B, this should timeout========\n",
			__func__);
		ret = cmdq_pkt_wait_flush_ex_result(hReqB);
		CMDQ_MSG("%s wait B get %d ========\n", __func__, ret);

	} while (0);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_1);

	cmdq_task_destroy(hReqA);
	cmdq_task_destroy(hReqB);

	del_timer(&timer_reqA);
	del_timer(&timer_reqB);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_multiple_async_request(void)
{
#define TEST_REQ_COUNT_ASYNC 9
	struct cmdqRecStruct *hReq[TEST_REQ_COUNT_ASYNC] = { 0 };
	s32 ret = 0;
	int i;

	CMDQ_LOG("%s\n", __func__);

	test_timer_stop = false;
	setup_timer(&test_timer, &_testcase_sync_token_timer_loop_func,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&test_timer, jiffies + msecs_to_jiffies(10));

	/* Queue multiple async request */
	/* to test dynamic task allocation */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < TEST_REQ_COUNT_ASYNC; i++) {
		ret = cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReq[i]);
		if (ret < 0) {
			CMDQ_ERR("%s cmdq_task_create failed:%d i:%d\n ",
				__func__, ret, i);
			continue;
		}

		cmdq_task_reset(hReq[i]);
		cmdq_task_set_secure(hReq[i], gCmdqTestSecure);

		/* specify engine flag in order to dispatch all tasks to the
		 * same HW thread
		 */
		hReq[i]->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

		cmdq_op_wait(hReq[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(hReq[i], false);

		/* higher priority for later tasks */
		hReq[i]->pkt->priority = i;

		_test_flush_async(hReq[i]);

		CMDQ_LOG("create pkt:%2d 0x%p done thread:%d\n",
			i, hReq[i]->pkt, hReq[i]->thread);
	}

	/* release token and wait them */
	for (i = 0; i < TEST_REQ_COUNT_ASYNC; i++) {

		if (hReq[i] == NULL) {
			CMDQ_ERR("%s handle[%d] is NULL\n", __func__, i);
			continue;
		}

		msleep_interruptible(100);

		CMDQ_LOG("======== wait task[%2d] 0x%p ========\n",
			i, hReq[i]);
		ret = cmdq_pkt_wait_flush_ex_result(hReq[i]);
		cmdq_task_destroy(hReq[i]);
	}

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	test_timer_stop = true;
	del_timer(&test_timer);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_async_request_partial_engine(void)
{
	s32 ret = 0;
	int i;
	enum CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_DEBUG, };

	struct cmdqRecStruct *handles[ARRAY_SIZE(scn)] = { 0 };
	struct timer_list *timers;

	timers = kmalloc_array(ARRAY_SIZE(scn), sizeof(struct timer_list),
		GFP_ATOMIC);
	if (!timers)
		return;

	CMDQ_LOG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < ARRAY_SIZE(scn); i++) {
		setup_timer(&timers[i], &_testcase_sync_token_timer_func,
			    CMDQ_SYNC_TOKEN_USER_0 + i);
		mod_timer(&timers[i], jiffies +
			msecs_to_jiffies(50 * (1 + i)));
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0 + i);

		cmdq_task_create(scn[i], &handles[i]);
		cmdq_task_reset(handles[i]);
		cmdq_task_set_secure(handles[i], false);
		cmdq_op_wait(handles[i], CMDQ_SYNC_TOKEN_USER_0 + i);
		cmdq_op_finalize_command(handles[i], false);

		CMDQ_LOG("TEST: SUBMIT scneario:%d thread:%d\n",
			scn[i], handles[i]->thread);
		ret = _test_flush_task_async(handles[i]);
		if (ret) {
			CMDQ_LOG("[warn] handle:%d flush failed, ret:%d\n",
				i, ret);
			cmdq_task_destroy(handles[i]);
			handles[i] = NULL;
			continue;
		}
	}

	/* wait for task completion */
	for (i = 0; i < ARRAY_SIZE(scn); i++) {
		if (!handles[i])
			continue;
		ret = _test_wait_task(handles[i]);
		cmdq_task_destroy(handles[i]);
	}

	/* clear token */
	for (i = 0; i < ARRAY_SIZE(scn); i++) {
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0 + i);
		del_timer(&timers[i]);
	}

	if (timers != NULL) {
		kfree(timers);
		timers = NULL;
	}

	CMDQ_LOG("%s END\n", __func__);

}

static void _testcase_unlock_all_event_timer_func(unsigned long data)
{
	u32 token = 0;

	CMDQ_LOG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger events\n");
	for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token) {
		/* 3 threads waiting, so update 3 times */
		cmdqCoreSetEvent(token);
		msleep_interruptible(10);
		cmdqCoreSetEvent(token);
		msleep_interruptible(10);
		cmdqCoreSetEvent(token);
	}
}

static void testcase_sync_token_threaded(void)
{
	enum CMDQ_SCENARIO_ENUM scn[] = {
		CMDQ_SCENARIO_PRIMARY_DISP,	/* high prio */
		CMDQ_SCENARIO_TRIGGER_LOOP	/* normal prio */
	};
	s32 ret = 0;
	int i = 0;
	u32 token = 0;
	struct timer_list eventTimer[ARRAY_SIZE(scn)];
	struct cmdqRecStruct *handles[ARRAY_SIZE(scn)] = { 0 };

	CMDQ_LOG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < ARRAY_SIZE(scn); i++) {
		setup_timer(&eventTimer[i],
			&_testcase_unlock_all_event_timer_func, 0);
		mod_timer(&eventTimer[i], jiffies + msecs_to_jiffies(500));

		/*  */
		/* 3 threads, all wait & clear 511 events */
		/*  */
		cmdq_task_create(scn[i], &handles[i]);
		cmdq_task_reset(handles[i]);
		cmdq_task_set_secure(handles[i], false);
		for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token)
			cmdq_op_wait(handles[i], (enum cmdq_event) token);

		cmdq_op_finalize_command(handles[i], false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_flush_async(handles[i]);
	}


	/* wait for task completion */
	msleep_interruptible(1000);
	for (i = 0; i < ARRAY_SIZE(scn); i++)
		ret = cmdq_pkt_wait_flush_ex_result(handles[i]);

	/* clear token */
	for (i = 0; i < ARRAY_SIZE(scn); ++i) {
		cmdq_task_destroy(handles[i]);
		del_timer(&eventTimer[i]);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static struct timer_list g_loopTimer;
static int g_loopIter;
static struct cmdqRecStruct *hLoopReq;

static void _testcase_loop_timer_func(unsigned long data)
{
	cmdqCoreSetEvent(data);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	g_loopIter++;
}

static void testcase_loop(void)
{
	int status = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoopReq);
	cmdq_task_reset(hLoopReq);
	cmdq_task_set_secure(hLoopReq, false);
	cmdq_op_wait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	/* should success */
	status = cmdq_task_start_loop(hLoopReq);

	/* should fail because already started */
	CMDQ_LOG("%s start loop thread:%d handle:0x%p pkt:0x%p\n",
		__func__, hLoopReq->thread,
		hLoopReq, hLoopReq->pkt);
	status = cmdq_task_start_loop(hLoopReq);

	cmdq_pkt_dump_command(hLoopReq);

	/* WAIT */
	while (g_loopIter < 20)
		msleep_interruptible(2000);

	msleep_interruptible(2000);

	CMDQ_LOG("%s stop timer\n", __func__);
	cmdq_task_destroy(hLoopReq);
	del_timer(&g_loopTimer);

	CMDQ_LOG("%s end\n", __func__);
}

#if 0
static unsigned long gLoopCount;
static void _testcase_trigger_func(unsigned long data)
{
	/* trigger sync event */
	CMDQ_MSG("_testcase_trigger_func");
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_1);

	if (test_timer_stop) {
		del_timer(&test_timer);
		return;
	}

	/* start again */
	mod_timer(&test_timer, jiffies + msecs_to_jiffies(1000));
	gLoopCount++;
}

static void leave_loop_func(struct work_struct *w)
{
	CMDQ_MSG("%s: cancel loop");
	cmdq_task_stop_loop(hLoopConfig);
	hLoopConfig = NULL;
}

DECLARE_WORK(leave_loop, leave_loop_func);

s32 my_irq_callback(unsigned long data)
{
	CMDQ_MSG("%s data=%d\n", __func__, data);

	++gLoopCount;

	switch (data) {
	case 1:
		if (gLoopCount < 20)
			return 0;
		else
			return -1;
		break;
	case 2:
		if (gLoopCount > 40) {
			/* insert stopping cal */
			schedule_work(&leave_loop);
		}
		break;
	}
	return 0;
}
#endif

static void testcase_prefetch_scenarios(void)
{
	/* make sure both prefetch and non-prefetch cases */
	/* handle 248+ instructions properly */
	struct cmdqRecStruct *hConfig;
	s32 ret = 0;
	int index = 0, scn = 0;
	const int INSTRUCTION_COUNT = 500;

	CMDQ_LOG("%s\n", __func__);

	/* make sure each scenario runs properly with 248+ commands */
	for (scn = 0; scn < CMDQ_MAX_SCENARIO_COUNT; ++scn) {
		if (cmdq_mdp_is_request_from_user_space(scn) ||
			scn == CMDQ_SCENARIO_TIMER_LOOP)
			continue;

		CMDQ_MSG("%s scenario:%d\n", __func__, scn);
		cmdq_task_create((enum CMDQ_SCENARIO_ENUM) scn, &hConfig);
		cmdq_task_reset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < INSTRUCTION_COUNT; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1,
				0, 0);

		ret = _test_flush_task(hConfig);
		cmdq_task_destroy(hConfig);
	}

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_clkmgr_impl(enum CMDQ_ENG_ENUM engine,
	char *name, const unsigned long testWriteReg, const u32 testWriteValue,
	const unsigned long testReadReg, const bool verifyWriteResult)
{
/* clkmgr is not available on FPGA */
#ifndef CONFIG_FPGA_EARLY_PORTING
	u32 value = 0;

	CMDQ_MSG("====== %s:%s ======\n", __func__, name);
	CMDQ_VERBOSE("clk engine:%d name:%s\n", engine, name);
	CMDQ_VERBOSE(
		"write reg(0x%lx) to 0x%08x read reg(0x%lx) verify write result:%d\n",
		testWriteReg, testWriteValue, testReadReg, verifyWriteResult);

	if ((testWriteReg & 0xFFFFF000) == 0 ||
		(testReadReg & 0xFFFFF000) == 0) {
		CMDQ_TEST_FAIL("%s: invalid write reg:%08lx read reg:%08lx\n",
			name, testWriteReg, testReadReg);
		return;
	}

	/* turn on CLK, function should work */
	CMDQ_MSG("enable_clock\n");
	/* Turn on MDP engines */
#ifdef CMDQ_CONFIG_SMI
	smi_bus_prepare_enable(SMI_LARB0, "CMDQ");
#endif
	cmdq_mdp_get_func()->enableMdpClock(true, engine);

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (verifyWriteResult && testWriteValue != value) {
		CMDQ_TEST_FAIL("%s: when enable clock reg(0x%lx) = 0x%08x\n",
			name, testReadReg, value);
		/* BUG(); */
	}

	/* turn off CLK, function should not work and access register should
	 * not cause hang
	 */
	CMDQ_MSG("disable_clock\n");
	/* Turn on MDP engines */
#ifdef CMDQ_CONFIG_SMI
	smi_bus_disable_unprepare(SMI_LARB0, "CMDQ");
#endif
	cmdq_mdp_get_func()->enableMdpClock(false, engine);


	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (value != 0) {
		CMDQ_TEST_FAIL("%s: when disable clock reg(0x%lx) = 0x%08x\n",
			name, testReadReg, value);
		/* BUG(); */
	}
#endif
}

static void testcase_dram_access(void)
{
	struct cmdqRecStruct *handle = NULL;
	dma_addr_t result_pa, dst_pa;
	u8 code, sop;
	u16 arg_a, arg_b, arg_c;
	u32 *result_va, pa;
	unsigned long long data64;

	CMDQ_LOG("%s\n", __func__);

	result_va = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		sizeof(u32) * 2, &result_pa, GFP_KERNEL);
	if (!result_va)
		return;

	/* set up intput */
	result_va[0] = 0xdeaddead;	/* this is read-from */
	result_va[1] = 0xffffffff;	/* this is write-to */

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* READ from DRAME: register to read from
	 * note that we force convert to physical reg address.
	 * if it is already physical address, it won't be affected
	 * (at least on this platform)
	 */
	CMDQ_MSG("reg pa:%pa size:%zu\n",
		&result_pa, handle->pkt->cmd_buf_size);

	/* Move &(regResults[0]) to CMDQ_DATA_REG_DEBUG_DST */
	pa = (u32)CMDQ_PHYS_TO_AREG(result_pa);
	arg_c = pa & 0xffff;
	arg_b = pa >> 16;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	arg_a = ((result_pa >> 32) & 0xffff);
#else
	arg_a = 0;
#endif
	sop = CMDQ_DATA_REG_DEBUG_DST & 0x1f;
	code = CMDQ_CODE_MOVE;
	cmdq_pkt_append_command(handle->pkt, arg_c, arg_b, arg_a,
		sop, 0, 0, 1, code);

	/* WRITE to DRAME:
	 * from src_addr(CMDQ_DATA_REG_DEBUG_DST) to external RAM
	 * (regResults[1])
	 */

	/* Read data from *CMDQ_DATA_REG_DEBUG_DST to CMDQ_DATA_REG_DEBUG */
	arg_c = CMDQ_DATA_REG_DEBUG;
	arg_b = 0;
	arg_a = 0;
	sop = CMDQ_DATA_REG_DEBUG_DST;
	code = CMDQ_CODE_READ;
	cmdq_pkt_append_command(handle->pkt, arg_c, arg_b, arg_a,
		sop, 0, 1, 1, code);

	/* Load dst_addr to GPR: Move &(regResults[1]) to
	 * CMDQ_DATA_REG_DEBUG_DST
	 */
	dst_pa = result_pa + 4;	/* note regResults is a u32 array */
	arg_c = (u16)dst_pa;
	arg_b = (u16)(dst_pa >> 16);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	arg_a =	((dst_pa >> 32) & 0xffff);
#else
	arg_a = 0;
#endif
	sop = CMDQ_DATA_REG_DEBUG_DST & 0x1f;
	code = CMDQ_CODE_MOVE;
	cmdq_pkt_append_command(handle->pkt, arg_c, arg_b, arg_a,
		sop, 0, 0, 1, code);

	/* Write from CMDQ_DATA_REG_DEBUG to *CMDQ_DATA_REG_DEBUG_DST */
	arg_c = CMDQ_DATA_REG_DEBUG;
	arg_b = 0;
	arg_a = 0;
	sop = CMDQ_DATA_REG_DEBUG_DST & 0x1f;
	code = CMDQ_CODE_WRITE;
	cmdq_pkt_append_command(handle->pkt, arg_c, arg_b, arg_a,
		sop, 0, 1, 1, code);

	cmdq_task_flush(handle);
	cmdq_pkt_dump_command(handle);
	cmdq_task_destroy(handle);

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_DEBUG_DST);

	if (result_va[1] != result_va[0]) {
		/* Test DRAM access fail */
		CMDQ_TEST_FAIL(
			"results:0x%08x 0x%08x reg debug:0x%08x reg debug dst:0x%llx\n",
			result_va[0], result_va[1],
			CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)),
			data64);
	} else {
		/* Test DRAM access success */
		CMDQ_MSG(
			"success results:0x%08x 0x%08x reg debug:0x%08x reg debug dst:0x%llx\n",
			result_va[0], result_va[1],
			CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)),
			data64);
	}

	cmdq_core_free_hw_buffer(cmdq_dev_get(), 2 * sizeof(u32), result_va,
		result_pa);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_long_command(void)
{
	int i;
	struct cmdqRecStruct *handle = NULL;
	u32 data;
	u32 pattern = 0x0;

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	/* build a 64K instruction buffer */
	for (i = 0; i < 64 * 1024 / 8; i++) {
		pattern = i;
		cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, pattern, ~0);
	}
	CMDQ_LOG("handle:0x%p buf size:%zu size:%zu avail:%zu\n",
		handle, handle->pkt->cmd_buf_size, handle->pkt->buf_size,
		handle->pkt->avail_buf_size);
	_test_flush_task(handle);
	CMDQ_LOG("handle:0x%p buf size:%zu size:%zu avail:%zu\n",
		handle, handle->pkt->cmd_buf_size, handle->pkt->buf_size,
		handle->pkt->avail_buf_size);

	/* verify data */
	do {
		if (gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n",
				__func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
		if (pattern != data) {
			CMDQ_TEST_FAIL(
				"reg value is 0x%08x not pattern 0x%08x\n",
				data, pattern);
			cmdq_core_dump_handle_buffer(handle->pkt, "INFO");
		}
	} while (0);

	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_perisys_apb(void)
{
	/* write value to PERISYS register */
	/* we use MSDC debug to test: */
	/* write SEL, read OUT. */

	const u32 MSDC_SW_DBG_OUT_OFFSET = 0xa4;
	const u32 AUDIO_AFE_I2S_CON3_OFFSET = 0x4c;
	const u32 UAR0_OFFSET = 0xbc;

	const phys_addr_t MSDC_PA_START = cmdq_dev_get_reference_PA("msdc0",
		0);
	const phys_addr_t AUDIO_TOP_CONF0_PA = cmdq_dev_get_reference_PA(
		"audio", 0);
	const phys_addr_t UART0_PA_BASE = cmdq_dev_get_reference_PA("uart0",
		0);
	const unsigned long MSDC_SW_DBG_SEL_PA = MSDC_PA_START + 0xa0;
	const unsigned long MSDC_SW_DBG_OUT_PA = MSDC_PA_START +
		MSDC_SW_DBG_OUT_OFFSET;
	const phys_addr_t AUDIO_PA = AUDIO_TOP_CONF0_PA +
		AUDIO_AFE_I2S_CON3_OFFSET;
	const phys_addr_t UART0_PA = UART0_PA_BASE + UAR0_OFFSET;

	const unsigned long MSDC_VA_BASE =
		cmdq_dev_alloc_reference_VA_by_name("msdc0");
	const unsigned long AUDIO_VA_BASE =
		cmdq_dev_alloc_reference_VA_by_name("audio");
	const unsigned long UART0_VA_BASE =
		cmdq_dev_alloc_reference_VA_by_name("uart0");
	const unsigned long MSDC_SW_DBG_OUT = MSDC_VA_BASE +
		MSDC_SW_DBG_OUT_OFFSET;
	const unsigned long AUDIO_VA = AUDIO_VA_BASE +
		AUDIO_AFE_I2S_CON3_OFFSET;
	const unsigned long UAR0_BUS_VA = UART0_VA_BASE + UAR0_OFFSET;

	const u32 write_pattern = 0xaabbccdd;
	struct cmdqRecStruct *handle = NULL;
	u32 data = 0;
	u32 dataRead = 0;

	CMDQ_LOG("%s\n", __func__);
	CMDQ_LOG("MSDC_VA_BASE:  VA:0x%lx, PA:%pa\n", MSDC_VA_BASE,
		&MSDC_PA_START);
	CMDQ_LOG("AUDIO_VA_BASE: VA:0x%lx, PA:%pa\n", AUDIO_VA_BASE,
		&AUDIO_TOP_CONF0_PA);
	CMDQ_LOG("UART0: VA:0x%lx, PA:%pa\n", UART0_VA_BASE, &UART0_PA_BASE);

	if (!MSDC_PA_START || !AUDIO_TOP_CONF0_PA || !UART0_PA_BASE) {
		CMDQ_TEST_FAIL("msdc or audio node does not porting.\n");
		return;
	}

	if (cmdq_core_subsys_from_phys_addr(MSDC_PA_START) < 0)
		cmdq_core_set_addon_subsys(MSDC_PA_START & 0xffff0000, 99,
		0xffff0000);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_write_reg(handle, MSDC_SW_DBG_SEL_PA, 1, ~0);
	cmdq_pkt_dump_command(handle);

	cmdq_task_flush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(MSDC_SW_DBG_OUT);
	if (data != ~0) {
		/* MSDC_SW_DBG_OUT would not same as sel setting */
		CMDQ_MSG("write 0xFFFFFFFF to MSDC_SW_DBG_OUT = 0x%08x=====\n",
			data);
		CMDQ_MSG("MSDC_SW_DBG_OUT: PA(%pa) VA(0x%lx) =====\n",
			&MSDC_SW_DBG_OUT_PA, MSDC_SW_DBG_OUT);
	}

	/* test read from AP_DMA_GLOBAL_SLOW_DOWN to CMDQ GPR */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_read_to_data_register(handle, MSDC_SW_DBG_OUT_PA,
		CMDQ_DATA_REG_PQ_COLOR);
	cmdq_task_flush(handle);

	/* verify data */
	dataRead = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != dataRead || data == 0) {
		/* test fail */
		CMDQ_TEST_FAIL(
			"CMDQ_DATA_REG_PQ_COLOR:0x%08x should be:0x%08x\n",
			dataRead, data);
		CMDQ_ERR("MSDC_SW_DBG_OUT: PA(%pa) VA(0x%lx) =====\n",
			&MSDC_SW_DBG_OUT_PA, MSDC_SW_DBG_OUT);
	} else {
		/* also log success */
		CMDQ_LOG("TEST SUCCESS: MSDC_SW_DBG_OUT 0x%08x == 0x%08x\n",
			dataRead, data);
	}

	if (cmdq_core_subsys_from_phys_addr(AUDIO_PA) < 0)
		cmdq_core_set_addon_subsys(AUDIO_PA & 0xffff0000, 99,
			0xffff0000);

	CMDQ_REG_SET32(AUDIO_VA, write_pattern);
	data = CMDQ_REG_GET32(AUDIO_VA);
	if (data != write_pattern) {
		CMDQ_TEST_FAIL("write 0x%08x to AUDIO_VA result:0x%08x\n",
			write_pattern, data);
		CMDQ_ERR("AUDIO_PA: PA(%pa) VA(0x%lx) =====\n",
			&AUDIO_PA, AUDIO_VA);
	} else {
		CMDQ_LOG(
			"TEST SUCCESS: write 0x%08x to AUDIO_VA = 0x%08x=====\n",
			write_pattern, data);
	}
	CMDQ_REG_SET32(AUDIO_VA, 0);
	data = CMDQ_REG_GET32(AUDIO_VA);
	CMDQ_LOG("Before AUDIO_VA = 0x%08x=====\n", data);
	cmdq_task_reset(handle);
	cmdq_op_write_reg(handle, AUDIO_PA, write_pattern, ~0);
	cmdq_pkt_dump_command(handle);
	cmdq_task_flush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(AUDIO_VA);
	CMDQ_LOG("after AUDIO_VA = 0x%08x=====\n", data);
	if (data != write_pattern) {
		/* test fail */
		CMDQ_TEST_FAIL("AUDIO_VA:0x%08x should be:0x%08x\n",
			data, write_pattern);
		CMDQ_ERR("AUDIO_VA: PA(%pa) VA(0x%lx) =====\n",
			&AUDIO_PA, AUDIO_VA);
	} else {
		/* also log success */
		CMDQ_LOG("TEST SUCCESS: AUDIO_VA 0x%08x == 0x%08x\n",
			data, write_pattern);
	}

	if (cmdq_core_subsys_from_phys_addr(UART0_PA_BASE) < 0)
		cmdq_core_set_addon_subsys(UART0_PA_BASE & 0xffff0000, 99,
			0xffff0000);

	CMDQ_REG_SET32(UAR0_BUS_VA, 1);
	data = CMDQ_REG_GET32(UAR0_BUS_VA);
	if ((data & 0x1) != 1) {
		CMDQ_TEST_FAIL("CPU: write 0x1 to UAR0_BUS_VA = 0x%08x=====\n",
			data);
		CMDQ_ERR("CPU: UAR0_BUS_VA: PA_BASE(%pa) VA(0x%lx) =====\n",
			&UART0_PA_BASE, UAR0_BUS_VA);
	} else {
		/* also log success */
		CMDQ_LOG("TEST SUCCESS: UAR0_BUS_VA = 0x%08x\n", data);
	}
	CMDQ_REG_SET32(UAR0_BUS_VA, 0);
	data = CMDQ_REG_GET32(UAR0_BUS_VA);
	CMDQ_LOG("Before UAR0_BUS_VA = 0x%08x=====\n", data);

	cmdq_task_reset(handle);
	cmdq_op_write_reg(handle, UART0_PA, 0x1, ~0);
	cmdq_pkt_dump_command(handle);
	cmdq_task_flush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(UAR0_BUS_VA);
	CMDQ_LOG("after UAR0_BUS_VA = 0x%08x=====\n", data);
	if ((data & 0x1) != 1) {
		/* test fail */
		CMDQ_TEST_FAIL("UAR0_BUS_VA:0x%08x should be:0x1\n", data);
		CMDQ_ERR("UAR0_BUS_VA: PA_BASE(%pa) VA(0x%lx) =====\n",
			&UART0_PA_BASE, UAR0_BUS_VA);
	} else {
		/* also log success */
		CMDQ_LOG("TEST SUCCESS: UAR0_BUS_VA = 0x%08x\n", data);
	}

	cmdq_task_destroy(handle);

	/* release registers map */
	cmdq_dev_free_module_base_VA(MSDC_VA_BASE);
	cmdq_dev_free_module_base_VA(AUDIO_VA_BASE);
	cmdq_dev_free_module_base_VA(UART0_VA_BASE);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_write_address(void)
{
	dma_addr_t pa = 0;
	u32 value = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdqCoreAllocWriteAddress(3, &pa, CMDQ_CLT_UNKN, NULL);
	CMDQ_LOG("ALLOC:%pa\n", &pa);
	value = cmdqCoreReadWriteAddress(pa);
	CMDQ_LOG("value 0:0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 1);
	CMDQ_LOG("value 1:0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 2);
	CMDQ_LOG("value 2:0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 3);
	CMDQ_LOG("value 3:0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 4);
	CMDQ_LOG("value 4:0x%08x\n", value);

	value = cmdqCoreReadWriteAddress(pa + (4 * 20));
	CMDQ_LOG("value 80:0x%08x\n", value);

	/* free invalid start address fist to verify error handle */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:0, it's a error case\n");
	cmdqCoreFreeWriteAddress(0, CMDQ_CLT_UNKN);

	/* ok case */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:%pa it's a ok case\n", &pa);
	cmdqCoreFreeWriteAddress(pa, CMDQ_CLT_UNKN);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_write_from_data_reg(void)
{
	struct cmdqRecStruct *handle = NULL;
	u32 value;
	const u32 PATTERN = 0xFFFFDEAD;
	const u32 srcGprId = CMDQ_DATA_REG_DEBUG;
	u32 dstRegPA;
	unsigned long dummy_va;

	CMDQ_LOG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dstRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* clean dst register value */
	CMDQ_REG_SET32((void *)dummy_va, 0x0);

	/* init GPR as value 0xFFFFDEAD */
	CMDQ_REG_SET32(CMDQ_GPR_R32(srcGprId), PATTERN);
	value = CMDQ_REG_GET32(CMDQ_GPR_R32(srcGprId));
	if (value != PATTERN) {
		CMDQ_ERR(
			"init CMDQ_DATA_REG_DEBUG to 0x%08x failed, value:0x%08x\n",
			PATTERN, value);
	}

	/* write GPR data reg to hw register */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_from_data_register(handle, srcGprId, dstRegPA);
	cmdq_task_flush(handle);

	cmdq_pkt_dump_command(handle);

	cmdq_task_destroy(handle);

	/* verify */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		CMDQ_ERR("%s failed, dstReg value is not 0x%08x value:0x%08x\n",
			__func__, PATTERN, value);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_read_to_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	u32 data;
	unsigned long long data64;

	CMDQ_LOG("%s\n", __func__);

	/* init GPR 64 */
	CMDQ_REG_SET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST,
		0x1234567890ABCDEFULL);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);
	/* R4 */
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xbeefbeef);
	/* R5 */
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0x0);

	cmdq_get_func()->dumpGPR();

	/* [read 64 bit test] move data from GPR to GPR_Px: COLOR to
	 * COLOR_DST (64 bit)
	 */
#if 1
	cmdq_op_read_to_data_register(handle,
		CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
		CMDQ_DATA_REG_PQ_COLOR_DST);
#else
	/* 64 bit behavior of Read OP depends APB bus implementation
	 * (CMDQ uses APB to access HW register, use AXI to access DRAM)
	 * from DE's suggestion,
	 * 1. for read HW register case, it's better to separate 1 x 64 bit
	 * length read to 2 x 32 bit length read
	 * 2. for GPRx each assignment case, it's better performance to use
	 * MOVE op to read GPR_x1 to GPR_x2
	 */

	/* when Read 64 length failed, try to use move to clear up if APB
	 * issue
	 */
	const u32 srcDataReg = CMDQ_DATA_REG_PQ_COLOR;
	const u32 dstDataReg = CMDQ_DATA_REG_PQ_COLOR_DST;
	/* arg_a, 22 bit 1: arg_b is GPR */
	/* arg_a, 23 bit 1: arg_a is GPR */
	cmdq_append_command(handle, CMDQ_CODE_RAW,
		(CMDQ_CODE_MOVE << 24) | (dstDataReg << 16) | (4 << 21) |
		(2 << 21), srcDataReg);
#endif

	/* [read 32 bit test] move data from register value to
	 * GPR_Rx: MM_DUMMY_REG to COLOR(32 bit)
	 */
	cmdq_op_read_to_data_register(handle, CMDQ_TEST_GCE_DUMMY_PA,
		CMDQ_DATA_REG_PQ_COLOR);

	cmdq_task_flush(handle);
	cmdq_pkt_dump_command(handle);
	cmdq_task_destroy(handle);

	cmdq_get_func()->dumpGPR();

	/* verify data */
	data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != 0xdeaddead) {
		/* Print error status */
		CMDQ_ERR(
			"[Read 32 bit from GPR_Rx]TEST FAIL: PQ reg value is 0x%08x\n",
			data);
	}

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST);
	if (data64 != 0xbeefbeef) {
		CMDQ_ERR(
			"[Read 64 bit from GPR_Px]TEST FAIL: PQ_DST reg value is 0x%llx\n",
			data64);
	}

	CMDQ_LOG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_write_reg_from_slot(void)
{
	const u32 PATTEN = 0xBCBCBCBC;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	u32 value = 0;
	long long value64 = 0LL;

	const enum cmdq_gpr_reg dstRegId = CMDQ_DATA_REG_DEBUG;
	const enum cmdq_gpr_reg srcRegId = CMDQ_DATA_REG_DEBUG_DST;

	CMDQ_LOG("%s\n", __func__);

	/* init */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(dstRegId), 0xdeaddead);
	CMDQ_REG_SET64_GPR_PX(srcRegId, 0xdeaddeaddeaddead);

	cmdq_alloc_mem(&hSlot, 1);
	cmdq_cpu_write_mem(hSlot, 0, PATTEN);
	cmdq_cpu_read_mem(hSlot, 0, &value);
	if (value != PATTEN) {
		/* Print error status */
		CMDQ_ERR("%s, slot init failed\n", __func__);
	}

	/* Create cmdqRec */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* Reset command buffer */
	cmdq_task_reset(handle);

	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* Insert commands to write register with slot's value */
	cmdq_op_read_mem_to_reg(handle, hSlot, 0, CMDQ_TEST_GCE_DUMMY_PA);

	/* Execute commands */
	cmdq_task_flush(handle);

	/* debug dump command instructions */
	cmdq_pkt_dump_command(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdq_task_destroy(handle);

	/* verify */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTEN) {
		/* Print error status */
		CMDQ_ERR("%s failed, value:0x%x\n", __func__, value);
	}

	value = CMDQ_REG_GET32(CMDQ_GPR_R32(dstRegId));
	value64 = CMDQ_REG_GET64_GPR_PX(srcRegId);
	CMDQ_LOG("srcGPR(%x):0x%llx\n", srcRegId, value64);
	CMDQ_LOG("dstGPR(%x):0x%08x\n", dstRegId, value);

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_LOG("%s END\n", __func__);

	return;

}

static void testcase_backup_reg_to_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	int i;
	u32 value = 0;

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	/* Create cmdqRec */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	/* Create Slot */
	cmdq_alloc_mem(&hSlot, 5);

	for (i = 0; i < 5; ++i)
		cmdq_cpu_write_mem(hSlot, i, i);

	for (i = 0; i < 5; i++) {
		cmdq_cpu_read_mem(hSlot, i, &value);
		if (value != i) {
			/* Print error status */
			CMDQ_ERR(
				"testcase_cmdqBackupWriteSlot FAILED!!!!!\n");
		}
		CMDQ_LOG("testcase_cmdqBackupWriteSlot OK!!!!!\n");
	}

	/* Reset command buffer */
	cmdq_task_reset(handle);

	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* Insert commands to backup registers */
	for (i = 0; i < 5; ++i)
		cmdq_op_read_reg_to_mem(handle, hSlot, i,
			CMDQ_TEST_GCE_DUMMY_PA);

	/* Execute commands */
	cmdq_task_flush(handle);

	/* debug dump command instructions */
	cmdq_pkt_dump_command(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdq_task_destroy(handle);

	/* verify data by reading it back from slot */
	for (i = 0; i < 5; i++) {
		cmdq_cpu_read_mem(hSlot, i, &value);
		CMDQ_LOG("backup slot %d = 0x%08x\n", i, value);

		if (value != 0xdeaddead) {
			/* content error */
			CMDQ_ERR("content error!!!!!!!!!!!!!!!!!!!!\n");
		}
	}

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_LOG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_update_value_to_slot(void)
{
	s32 i;
	u32 value;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	const u32 PATTERNS[] = {
		0xDEAD0000, 0xDEAD0001, 0xDEAD0002, 0xDEAD0003, 0xDEAD0004
	};

	CMDQ_LOG("%s\n", __func__);

	/* Create Slot */
	cmdq_alloc_mem(&hSlot, 5);

	/*use CMDQ to update slot value */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	for (i = 0; i < 5; ++i)
		cmdq_op_write_mem(handle, hSlot, i, PATTERNS[i]);

	cmdq_task_flush(handle);
	cmdq_pkt_dump_command(handle);
	cmdq_task_destroy(handle);

	/* CPU verify value by reading it back from slot  */
	for (i = 0; i < 5; i++) {
		cmdq_cpu_read_mem(hSlot, i, &value);

		if (PATTERNS[i] != value) {
			CMDQ_ERR(
				"slot[%d] = 0x%08x...content error! It should be 0x%08x\n",
				i, value, PATTERNS[i]);
		} else {
			CMDQ_LOG("slot[%d] = 0x%08x\n", i, value);
		}
	}

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_poll_run(u32 poll_value, u32 poll_mask,
	bool use_mmsys_dummy)
{
	struct cmdqRecStruct *handle = NULL;
	u32 value = 0;
	u32 dstRegPA;
	unsigned long dummy_va;

	if (gCmdqTestSecure || use_mmsys_dummy) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dstRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	}

	CMDQ_LOG("%s\n", __func__);
	CMDQ_LOG("poll value is 0x%08x\n", poll_value);
	CMDQ_LOG("poll mask is 0x%08x\n", poll_mask);
	CMDQ_LOG("use_mmsys_dummy is %u\n", use_mmsys_dummy);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	cmdq_op_poll(handle, dstRegPA, poll_value, poll_mask);

	cmdq_op_finalize_command(handle, false);
	_test_flush_async(handle);
	cmdq_pkt_dump_command(handle);

	/* Set MMSYS dummy register value after clock is on */
	CMDQ_REG_SET32(dummy_va, poll_value);
	value = CMDQ_REG_GET32(dummy_va);
	CMDQ_LOG("target value is 0x%08x\n", value);

	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_poll(void)
{
	CMDQ_LOG("%s\n", __func__);

	testcase_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, false);
	testcase_poll_run(0xdada1818, 0xFFFFFFFF, false);
	testcase_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, false);
	testcase_poll_run(0x00001818, 0xFFFFFFFF, false);
#ifndef CONFIG_FPGA_EARLY_PORTING
	testcase_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, true);
	testcase_poll_run(0xdada1818, 0xFFFFFFFF, true);
	testcase_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, true);
	testcase_poll_run(0x00001818, 0xFFFFFFFF, true);
#endif

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_write_with_mask(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	const u32 MASK = (1 << 16);
	const u32 EXPECT_RESULT = PATTERN & MASK;
	u32 value = 0;
	unsigned long dummy_va, dummy_pa;

	CMDQ_LOG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0x0 */
	CMDQ_REG_SET32((void *)dummy_va, 0x0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, MASK);
	cmdq_task_flush(handle);
	cmdq_pkt_dump_command(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != EXPECT_RESULT) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, EXPECT_RESULT);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_cross_4k_buffer(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	unsigned long dummy_va, dummy_pa;
	u32 i;

	CMDQ_LOG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32((void *)dummy_va, ~0);
	CMDQ_LOG("set reg=%lx to val=%x\n", dummy_va,
		CMDQ_REG_GET32((void *)dummy_va));

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	for (i = 0; i < 500; i++)
		cmdq_op_write_reg(handle, dummy_pa, PATTERN, ~0);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_write(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	unsigned long dummy_va, dummy_pa;

	CMDQ_LOG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32((void *)dummy_va, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, ~0);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_prefetch(void)
{
	struct cmdqRecStruct *handle = NULL;
	int i;
	u32 value = 0;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16); /* 0xDEADDEAD; */
	const u32 testRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	const u32 REP_COUNT = 500;

	CMDQ_LOG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* No prefetch. */
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdq_op_write_reg(handle, testRegPA, PATTERN, ~0);

	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	msleep_interruptible(1000);

	/* use prefetch */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdq_op_write_reg(handle, testRegPA, PATTERN, ~0);

	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	msleep_interruptible(1000);

	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_backup_register(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	int ret = 0;
	u32 regAddr[3] = { CMDQ_TEST_GCE_DUMMY_PA,
		CMDQ_THR_CURR_ADDR_PA(5),
		CMDQ_THR_END_ADDR_PA(5)
	};
	u32 regValue[3] = { 0 };

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xAAAAAAAA);
	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(5), 0xBBBBBBBB);
	CMDQ_REG_SET32(CMDQ_THR_END_ADDR(5), 0xCCCCCCCC);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	ret = cmdq_task_flush_and_read_register(handle, 3, regAddr, regValue);
	cmdq_task_destroy(handle);

	if (regValue[0] != 0xAAAAAAAA) {
		/* Print error status */
		CMDQ_ERR("regValue[0] is 0x%08x wrong!\n", regValue[0]);
	}
	if (regValue[1] != 0xBBBBBBBB) {
		/* Print error status */
		CMDQ_ERR("regValue[1] is 0x%08x wrong!\n", regValue[1]);
	}
	if (regValue[2] != 0xCCCCCCCC) {
		/* Print error status */
		CMDQ_ERR("regValue[2] is 0x%08x wrong!\n", regValue[2]);
	}

	CMDQ_LOG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_get_result(void)
{
	struct cmdqRecStruct *handle = NULL;
	int ret = 0;
	struct cmdqCommandStruct desc = { 0 };
	struct cmdq_pkt_buffer *buf;
	void *desc_buf;

	int registers[1] = { CMDQ_TEST_GCE_DUMMY_PA };
	int result[1] = { 0 };

	CMDQ_LOG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	/* because it has COLOR0 HW flag */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* insert dummy commands */
	cmdq_op_finalize_command(handle, false);

	desc_buf = kzalloc(handle->pkt->cmd_buf_size, GFP_KERNEL);
	if (!desc_buf) {
		CMDQ_TEST_FAIL("fail to allocat desc buf size:%zu\n",
			handle->pkt->cmd_buf_size);
		cmdq_task_destroy(handle);
		return;
	}

	/* init desc attributes after finalize command to ensure correct
	 * size and buffer addr
	 */
	desc.scenario = handle->scenario;
	desc.priority = handle->pkt->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t)(unsigned long)desc_buf;
	desc.blockSize = handle->pkt->cmd_buf_size;
	buf = list_first_entry(&handle->pkt->buf, typeof(*buf), list_entry);
	memcpy(desc_buf, buf->va_base, handle->pkt->cmd_buf_size);

	desc.regRequest.count = 1;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t)(unsigned long)registers;
	desc.regValue.count = 1;
	desc.regValue.regValues = (cmdqU32Ptr_t)(unsigned long)result;

	desc.secData.is_secure = handle->secData.is_secure;
	desc.secData.addrMetadataCount = 0;
	desc.secData.addrMetadataMaxCount = 0;
	desc.secData.waitCookie = 0;
	desc.secData.resetExecCnt = false;

	cmdq_task_destroy(handle);
	handle = NULL;

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	CMDQ_LOG("flush mdp desc va:0x%p size:%u\n",
		(void *)(unsigned long)desc.pVABase, desc.blockSize);

	ret = cmdq_mdp_flush_async(&desc, false, &handle);
	if (ret)
		CMDQ_ERR("handle=%p flush failed, ret=%d\n", handle, ret);

	if (handle) {
		cmdq_pkt_dump_command(handle);
		ret = cmdq_mdp_wait(handle, &desc.regValue);
		if (ret)
			CMDQ_ERR(
				"handle=%p wait failed, ret=%d\n",
				handle, ret);
	}

	if (CMDQ_U32_PTR(desc.regValue.regValues)[0] != 0xdeaddead) {
		CMDQ_ERR("TEST FAIL: reg value is 0x%08x wait ret:%d\n",
			 CMDQ_U32_PTR(desc.regValue.regValues)[0], ret);
	}

	cmdq_task_destroy(handle);
	kfree(desc_buf);

	CMDQ_LOG("%s END\n", __func__);
}

static int _testcase_simplest_command_loop_submit(
	const u32 loop, enum CMDQ_SCENARIO_ENUM scenario,
	const long long engineFlag, const bool isSecureTask)
{
	struct cmdqRecStruct *handle = NULL;
	s32 i;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(scenario, &handle);
	for (i = 0; i < loop; i++) {
		CMDQ_MSG(
			"pid:%d flush:%4d, engineFlag:0x%llx isSecureTask:%d\n",
			current->pid, i, engineFlag, isSecureTask);
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, isSecureTask);
		handle->engineFlag = engineFlag;
		_test_flush_task(handle);
	}
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);

	return 0;
}

/* threadfn: int (*threadfn)(void *data) */
static int _testcase_thread_dispatch(void *data)
{
	long long engineFlag;

	engineFlag = *((long long *)data);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG_MDP,
		engineFlag, false);

	return 0;
}

static void testcase_thread_dispatch(void)
{
	char threadName[20];
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const long long engineFlag1 = (0x1LL << CMDQ_ENG_MDP_RSZ0) |
		(0x1LL << CMDQ_ENG_MDP_CAMIN);
	const long long engineFlag2 = (0x1LL << CMDQ_ENG_MDP_RDMA0) |
		(0x1LL << CMDQ_ENG_MDP_WROT0);
	int len;

	CMDQ_LOG("%s\n", __func__);
	CMDQ_MSG(
		"=============== 2 THREAD with different engines ===============\n");

	len = sprintf(threadName, "cmdqKTHR_%llx", engineFlag1);
	if (len >= 20)
		pr_debug("%s:%d len:%d threadName:%s\n",
			__func__, __LINE__, len, threadName);

	pKThread1 = kthread_run(_testcase_thread_dispatch,
		(void *)(&engineFlag1), threadName);
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	len = sprintf(threadName, "cmdqKTHR_%llx", engineFlag2);
	if (len >= 20)
		pr_debug("%s:%d len:%d threadName:%s\n",
			__func__, __LINE__, len, threadName);

	pKThread2 = kthread_run(_testcase_thread_dispatch,
		(void *)(&engineFlag2), threadName);
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG_MDP,
		engineFlag1, false);
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG_MDP,
		engineFlag2, false);

	CMDQ_LOG("%s END\n", __func__);
}

static int _testcase_full_thread_array(void *data)
{
	/* this testcase will be passed only when cmdqSecDr support async
	 * config mode because
	 * never execute event setting till IWC back to NWd
	 */
#define MAX_FULL_THREAD_TASK_COUNT 50

	struct cmdqRecStruct *handle[MAX_FULL_THREAD_TASK_COUNT] = {0};
	s32 i;

	/* clearn event first */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < MAX_FULL_THREAD_TASK_COUNT; i++) {
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle[i]);

		/* specify engine flag in order to dispatch all tasks
		 * to the same HW thread
		 */
		handle[i]->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);

		cmdq_task_reset(handle[i]);
		cmdq_task_set_secure(handle[i], gCmdqTestSecure);
		cmdq_op_wait_no_clear(handle[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(handle[i], false);

		CMDQ_LOG("pid:%d flush:%6d\n", current->pid, i);

		if (i == 40) {
			CMDQ_LOG("set token:%d to 1\n",
				CMDQ_SYNC_TOKEN_USER_0);
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		}

		_test_flush_async(handle[i]);
	}

	for (i = 0; i < MAX_FULL_THREAD_TASK_COUNT; i++) {
		cmdq_pkt_wait_flush_ex_result(handle[i]);
		cmdq_task_destroy(handle[i]);
	}

	return 0;
}

static void testcase_full_thread_array(void)
{
	char threadName[20];
	struct task_struct *pKThread;
	int len;

	CMDQ_LOG("%s\n", __func__);

	len = sprintf(threadName, "cmdqKTHR");
	if (len >= 20)
		pr_debug("%s:%d len:%d threadName:%s\n",
			__func__, __LINE__, len, threadName);

	pKThread = kthread_run(_testcase_full_thread_array, NULL, threadName);
	if (IS_ERR(pKThread)) {
		/* create thread failed */
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
	}

	msleep_interruptible(5 * 1000);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_module_full_dump(void)
{
	struct cmdqRecStruct *handle = NULL;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();
	s32 status;

	CMDQ_LOG("%s\n", __func__);

	/* enable full dump */
	if (!alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* clean SW token to invoke SW timeout latter */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* turn on ALL except DISP engine flag to test dump */
	handle->engineFlag = CMDQ_ENG_MDP_GROUP_BITS;

	CMDQ_LOG("%s, engine:0x%llx it's a timeout case\n",
		__func__, handle->engineFlag);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	status = cmdq_mdp_flush_async_impl(handle);
	if (status < 0)
		CMDQ_ERR("flush failed handle:0x%p\n", handle);
	else
		status = cmdq_mdp_wait(handle, NULL);

	/* disable full dump */
	if (!alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_LOG("%s END\n", __func__);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#include "cmdqsectl_api.h"
s32 cmdq_sec_submit_to_secure_world_async_unlocked(u32 iwcCommand,
	struct cmdqRecStruct *handle, s32 thread,
	CmdqSecFillIwcCB iwcFillCB, void *data);
#endif

void testcase_secure_basic(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	s32 status = 0;

	CMDQ_LOG("%s\n", __func__);

	do {

		CMDQ_MSG("=========== Hello cmdqSecTl ===========\n ");
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
			CMD_CMDQ_TL_TEST_HELLO_TL, NULL,
			CMDQ_INVALID_THREAD, NULL, NULL);
		if (status < 0) {
			/* entry cmdqSecTL failed */
			CMDQ_ERR("entry cmdqSecTL failed, status:%d\n",
				status);
		}

		CMDQ_MSG("=========== Hello cmdqSecDr ===========\n ");
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
			CMD_CMDQ_TL_TEST_DUMMY, NULL,
			CMDQ_INVALID_THREAD, NULL, NULL);
		if (status < 0) {
			/* entry cmdqSecDr failed */
			CMDQ_ERR("entry cmdqSecDr failed, status:%d\n",
				status);
		}
	} while (0);

	CMDQ_LOG("%s END\n", __func__);
#endif
}

void testcase_submit_after_error_happened(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 pollingVal = 0x00003001;

	CMDQ_LOG("%s\n", __func__);
	CMDQ_MSG("=========== timeout case ===========\n");

	/* let poll INIFINITE */
	/* CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, pollingVal); */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	cmdq_op_poll(handle, CMDQ_TEST_GCE_DUMMY_PA, pollingVal, ~0);
	cmdq_task_flush(handle);

	CMDQ_MSG("=========== okay case ===========\n");
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0,
		gCmdqTestSecure);

	/* clear up */
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_write_stress_test(void)
{
	s32 loop;

	CMDQ_LOG("%s\n", __func__);

	loop = 1;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0,
		gCmdqTestSecure);

	loop = 100;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0,
		gCmdqTestSecure);

	CMDQ_LOG("%s END\n", __func__);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static int _testcase_concurrency(void *data)
{
	u32 securePath;

	securePath = *((u32 *) data);

	CMDQ_MSG("start secure(%d) path\n", securePath);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG,
		(0x1 << CMDQ_ENG_MDP_RSZ0), securePath);

	return 0;
}
#endif

static void testcase_concurrency_for_normal_path_and_secure_path(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const u32 securePath[2] = { 0, 1 };

	CMDQ_LOG("%s\n", __func__);

	pKThread1 = kthread_run(_testcase_concurrency,
		(void *)(&securePath[0]), "cmdqNormal");
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create cmdqNormal failed\n");
		return;
	}

	pKThread2 = kthread_run(_testcase_concurrency,
		(void *)(&securePath[1]), "cmdqSecure");
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create cmdqSecure failed\n");
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0x0,
		false);
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0x0,
		true);

	CMDQ_LOG("%s END\n", __func__);

	return;
#endif
}

static void testcase_nonsuspend_irq(void)
{
	struct cmdqRecStruct *handle, *handle2;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdq_task_reset(handle2);
	cmdq_task_set_secure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	/* force GCE to wait in second command before EOC */
	cmdq_op_wait(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle2, false);

	_test_flush_async(handle);
	_test_flush_async(handle2);

	msleep_interruptible(500);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* test code: use to trigger GCE continue test command,
	 * put in cmdq_core::handleIRQ to test
	 */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_MSG("IRQ: After set user sw token\n");

	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_pkt_wait_flush_ex_result(handle2);
	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_module_full_mdp_engine(void)
{
	struct cmdqRecStruct *handle = NULL;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();
	s32 status;

	CMDQ_LOG("%s\n", __func__);

	/* enable full dump */
	if (!alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* turn on ALL except DISP engine flag to test clock operation */
	handle->engineFlag = CMDQ_ENG_MDP_GROUP_BITS;

	CMDQ_LOG("%s, engine:0x%llx it's a engine clock test case\n",
		 __func__, handle->engineFlag);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_finalize_command(handle, false);

	status = cmdq_mdp_flush_async_impl(handle);
	if (status < 0)
		CMDQ_ERR("flush failed handle:0x%p\n", handle);
	else
		status = cmdq_mdp_wait(handle, NULL);

	/* disable full dump */
	if (!alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_trigger_engine_dispatch_check(void)
{
	struct cmdqRecStruct *handle, *handle2, *hTrigger;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	u32 loopIndex = 0;

	CMDQ_LOG("%s\n", __func__);

	/* Create first task and run without wait */
	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_finalize_command(handle, false);
	_test_flush_async(handle);

	/* Create trigger loop */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
	cmdq_task_reset(hTrigger);
	cmdq_op_wait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_start_loop(hTrigger);
	/* Sleep to let trigger loop run fow a while */
	CMDQ_MSG("%s before start sleep and trigger token\n", __func__);
	for (loopIndex = 0; loopIndex < 10; loopIndex++) {
		msleep_interruptible(500);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		CMDQ_MSG("%s after sleep 5000 and send (%d)\n",
			__func__, loopIndex);
	}

	/* Create second task and should run well */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdq_task_reset(handle2);
	cmdq_task_set_secure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle2, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush(handle2);
	cmdq_task_destroy(handle2);

	/* Call wait to release first task */
	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);
	cmdq_task_destroy(hTrigger);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_complicated_engine_thread(void)
{
#define TASK_COUNT 6
	struct cmdqRecStruct *handle[TASK_COUNT] = { 0 };
	u64 engineFlag[TASK_COUNT] = { 0 };
	u32 taskIndex = 0;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* config engine flag for test */
	engineFlag[0] = (1LL << CMDQ_ENG_MDP_RDMA0);
	engineFlag[1] = (1LL << CMDQ_ENG_MDP_RDMA0) |
		(1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[2] = (1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[3] = (1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[4] = (1LL << CMDQ_ENG_MDP_RDMA0) |
		(1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[5] = (1LL << CMDQ_ENG_MDP_TDSHP0) |
		(1LL << CMDQ_ENG_MDP_RSZ0);

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		/* Create task and run with wait */
		cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle[taskIndex]);
		cmdq_task_reset(handle[taskIndex]);
		cmdq_task_set_secure(handle[taskIndex], gCmdqTestSecure);
		handle[taskIndex]->engineFlag = engineFlag[taskIndex];
		cmdq_op_wait(handle[taskIndex], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(handle[taskIndex], false);
		_test_flush_task_async(handle[taskIndex]);
	}

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		/* Call wait to release task */
		_test_wait_task(handle[taskIndex]);
		cmdq_task_destroy(handle[taskIndex]);
		msleep_interruptible(1000);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_append_task_verify(void)
{
	struct cmdqRecStruct *handle, *handle2;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	u32 loopIndex = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle2);
	for (loopIndex = 0; loopIndex < 2; loopIndex++) {
		/* clear token */
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
		/* clear dummy register */
		CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

		/* Create first task and run with wait */
		/* use CMDQ to set to PATTERN */
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(handle, false);

		/* Create second task and should run well */
		cmdq_task_reset(handle2);
		cmdq_task_set_secure(handle2, gCmdqTestSecure);
		cmdq_op_write_reg(handle2, CMDQ_TEST_GCE_DUMMY_PA, PATTERN,
			~0);
		cmdq_op_finalize_command(handle2, false);

		_test_flush_async(handle);
		_test_flush_async(handle2);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		/* Call wait to release first task */
		cmdq_pkt_wait_flush_ex_result(handle);
		cmdq_pkt_wait_flush_ex_result(handle2);

		/* value check */
		value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
		if (value != PATTERN) {
			/* test fail */
			CMDQ_ERR(
				"TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
				value, PATTERN);
		}
	}

	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_manual_suspend_resume_test(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush_async(handle);

	/* Manual suspend and resume */
	cmdq_core_suspend();
	cmdq_core_resume_notifier();

	_test_flush_async(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	/* Call wait to release second task */
	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_timeout_wait_early_test(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	_test_flush_async(handle);

	cmdq_task_flush(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	/* Call wait to release first task */
	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_timeout_reorder_test(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);
	handle->pkt->priority = 0;
	cmdq_task_flush_async(handle);
	handle->pkt->priority = 2;
	cmdq_task_flush_async(handle);
	handle->pkt->priority = 4;
	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_error_irq_for_secure(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	unsigned long dummy_va, dummy_pa;

	CMDQ_LOG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32((void *)dummy_va, ~0);
	CMDQ_LOG("set reg=%lx to val=%x\n", dummy_va,
		CMDQ_REG_GET32((void *)dummy_va));

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, ~0);
	cmdq_append_command(handle, CMDQ_CODE_JUMP, 0, 8, 0, 0);
	cmdq_append_command(handle, CMDQ_CODE_JUMP, 0, 8, 0, 0);
	cmdq_pkt_append_command(handle->pkt, 0, 0xdead, 0xddaa,
		0, 0, 0, 0, 0x87);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_error_irq(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	const u8 UNKNOWN_OP = 0x50;

	CMDQ_LOG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* wait and block instruction */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush_async(handle);

	/* invalid instruction */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_append_command(handle, CMDQ_CODE_JUMP, -1, 0, 0, 0);
	cmdq_pkt_dump_command(handle);
	cmdq_task_flush_async(handle);

	/* Normal command */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);

	/* invalid instruction is asserted when unknown OP */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_pkt_append_command(handle->pkt, 0, 0, 0, 0, 0, 0, 0,
		UNKNOWN_OP);
	cmdq_task_flush_async(handle);

	/* use CMDQ to set to PATTERN */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_op_finalize_command(handle, false);
	_test_flush_async(handle);

	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

#if 0
static void testcase_open_buffer_dump(s32 scenario, s32 bufferSize)
{
	CMDQ_LOG("%s\n", __func__);

	CMDQ_MSG("[TESTCASE]CONFIG: bufferSize:%d scenario:%d\n",
		bufferSize, scenario);
	cmdq_core_set_command_buffer_dump(scenario, bufferSize);

	CMDQ_LOG("%s END\n", __func__);
}
#endif

static void testcase_check_dts_correctness(void)
{
	CMDQ_LOG("%s\n", __func__);

	cmdq_dev_test_dts_correctness();

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_acquire_resource(
	enum cmdq_event resourceEvent, bool acquireExpected)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	s32 acquireResult;

	CMDQ_LOG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	acquireResult = cmdq_resource_acquire_and_write(handle, resourceEvent,
		CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	if (acquireResult < 0) {
		/* Do error handle for acquire resource fail */
		if (acquireExpected) {
			/* print error message */
			CMDQ_ERR(
				"Acquire resource fail: it's not expected!\n");
		} else {
			/* print message */
			CMDQ_LOG("Acquire resource fail: it's expected!\n");
		}
	} else {
		if (!acquireExpected) {
			/* print error message */
			CMDQ_ERR(
				"Acquire resource success: it's not expected!\n");
		} else {
			/* print message */
			CMDQ_LOG("Acquire resource success: it's expected!\n");
		}
	}
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN && acquireExpected) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	CMDQ_LOG("%s END\n", __func__);
}

static s32 testcase_res_release_cb(enum cmdq_event resourceEvent)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_LOG("%s\n", __func__);
	/* Flush release command immedately with wait MUTEX event */

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	/* simulate display need to wait single */
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	/* simulate release resource via write register */
	cmdq_resource_release_and_write(handle, resourceEvent,
		CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
	return 0;
}

static s32 testcase_res_available_cb(enum cmdq_event resourceEvent)
{
	CMDQ_LOG("%s\n", __func__);
	testcase_acquire_resource(resourceEvent, true);
	CMDQ_LOG("%s END\n", __func__);
	return 0;
}

static void testcase_notify_and_delay_submit(u32 delayTimeMS)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	u32 value = 0;
	const u64 engineFlag = (1LL << CMDQ_ENG_MDP_WROT0);
	const enum cmdq_event resourceEvent = CMDQ_SYNC_RESOURCE_WROT0;
	u32 contDelay;

	CMDQ_LOG("%s\n", __func__);

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_mdp_set_resource_callback(resourceEvent,
		testcase_res_available_cb, testcase_res_release_cb);

	testcase_acquire_resource(resourceEvent, true);

	/* notify and delay time*/
	if (delayTimeMS > 0) {
		CMDQ_MSG("Before delay for acquire\n");
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("Before lock and delay\n");
		cmdq_mdp_lock_resource(engineFlag, true);
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("After lock and delay\n");
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = engineFlag;
	cmdq_op_wait_no_clear(handle, resourceEvent);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	msleep_interruptible(2000);

	/* Delay and continue sent */
	for (contDelay = 300; contDelay < CMDQ_DELAY_RELEASE_RESOURCE_MS*1.2;
		contDelay += 300) {
		CMDQ_MSG("Before delay and flush\n");
		msleep_interruptible(contDelay);
		CMDQ_MSG("After delay\n");
		cmdq_task_flush(handle);
		CMDQ_MSG("After flush\n");
	}
	/* Simulate DISP acquire fail case, acquire immediate after flush MDP */
	cmdq_task_flush_async(handle);
	testcase_acquire_resource(resourceEvent, false);

	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x not 0x%08x\n",
			value, PATTERN);
	}

	cmdq_mdp_set_resource_callback(resourceEvent, NULL, NULL);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_specific_bus_MMSYS(void)
{
	u32 i;
	const u32 loop = 1000;
	const u32 pattern = (1 << 0) | (1 << 2) | (1 << 16);
	u32 mmsys_register;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle slot_handle = 0;
	u32 start_time = 0, end_time = 0, duration_time = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 2);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_op_backup_TPR(handle, slot_handle, 0);
	for (i = 0; i < loop; i++) {
		mmsys_register = CMDQ_TEST_MMSYS_DUMMY_PA + (i%2)*0x4;
		if (i%11 == 10)
			cmdq_op_read_to_data_register(handle, mmsys_register,
				CMDQ_DATA_REG_2D_SHARPNESS_0);
		else
			cmdq_op_write_reg(handle, mmsys_register, pattern, ~0);
	}

	cmdq_op_backup_TPR(handle, slot_handle, 1);
	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &start_time);
	cmdq_cpu_read_mem(slot_handle, 1, &end_time);
	duration_time = (end_time - start_time) * 76;
	CMDQ_LOG("duration time, %u, ns\n", duration_time);

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

void cmdq_track_task(const struct cmdqRecStruct *handle)
{
	CMDQ_LOG("track_task: engine:0x%08llx\n", handle->engineFlag);
}

static void testcase_track_task_cb(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_LOG("%s\n", __func__);
	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP, cmdq_track_task);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);
	cmdq_task_flush(handle);

	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP, NULL);
	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_while_test_mmsys_bus(void)
{
	s32 i;
	const u32 loop = 5000;

	CMDQ_LOG("%s\n", __func__);

	for (i = 0; i < loop; i++) {
		testcase_specific_bus_MMSYS();
		msleep_interruptible(100);
	}

	CMDQ_LOG("%s END\n", __func__);
}

struct thread_set_event_config {
	enum cmdq_event event;
	u32 sleep_ms;
	bool loop;
};

static int testcase_set_gce_event(void *data)
{
	struct thread_set_event_config config;

	CMDQ_LOG("%s\n", __func__);

	config = *((struct thread_set_event_config *) data);
	do {
		if (kthread_should_stop())
			break;

		if (config.sleep_ms > 10)
			msleep_interruptible(config.sleep_ms);
		else
			mdelay(config.sleep_ms);
		cmdqCoreSetEvent(config.event);
	} while (config.loop);

	CMDQ_LOG("%s END\n", __func__);

	return 0;
}

static int testcase_cpu_config_non_mmsys(void *data)
{
	CMDQ_LOG("%s\n", __func__);

	while (1) {
		if (kthread_should_stop())
			break;

		/* set to 0xFFFFFFFF */
		CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_JPEG), ~0);
		/* udelay(1); */
	}

	CMDQ_LOG("%s END\n", __func__);

	return 0;
}

static int testcase_cpu_config_mmsys(void *data)
{
	unsigned long mmsys_register;

	CMDQ_LOG("%s\n", __func__);

	cmdq_mdp_get_func()->mdpEnableCommonClock(true);

	while (1) {
		if (kthread_should_stop())
			break;

		mmsys_register = CMDQ_TEST_MMSYS_DUMMY_VA + 0x4;
		/* set to 0xFFFFFFFF */
		CMDQ_REG_SET32(mmsys_register, ~0);
		/* udelay(1); */
	}

	CMDQ_LOG("%s END\n", __func__);

	cmdq_mdp_get_func()->mdpEnableCommonClock(false);

	return 0;
}

#define CMDQ_TEST_MAX_THREAD	(32)
struct task_struct *set_event_config_th;
struct task_struct *busy_mmsys_config_th[CMDQ_TEST_MAX_THREAD] = {NULL};
struct task_struct *busy_non_mmsys_config_th[CMDQ_TEST_MAX_THREAD] = {NULL};

static void testcase_run_set_gce_event(void *data)
{
	set_event_config_th = kthread_run(testcase_set_gce_event, data,
		"set_cmdq_event_loop");
	if (IS_ERR(set_event_config_th)) {
		/* print error log */
		CMDQ_LOG("%s, init kthread_run failed!\n", __func__);
		set_event_config_th = NULL;
	}
}

static void testcase_stop_set_gce_event(void)
{
	if (set_event_config_th == NULL)
		return;

	kthread_stop(set_event_config_th);
	set_event_config_th = NULL;
}

static void testcase_run_busy_non_mmsys_config_loop(void)
{
	u32 i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		busy_non_mmsys_config_th[i] = kthread_run(
			testcase_cpu_config_non_mmsys, NULL,
			"busy_config_non-mm");
		if (IS_ERR(busy_non_mmsys_config_th[i])) {
			/* print error log */
			CMDQ_LOG("%s, thread id:%d init kthread_run failed!\n",
				__func__, i);
			busy_non_mmsys_config_th[i] = NULL;
		}
	}
}

static void testcase_stop_busy_non_mmsys_config_loop(void)
{
	u32 i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		if (busy_non_mmsys_config_th[i] == NULL)
			continue;

		kthread_stop(busy_non_mmsys_config_th[i]);
		busy_non_mmsys_config_th[i] = NULL;
	}
}

static void testcase_run_busy_mmsys_config_loop(void)
{
	u32 i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		busy_mmsys_config_th[i] = kthread_run(
			testcase_cpu_config_mmsys, NULL, "busy_config_mm");
		if (IS_ERR(busy_mmsys_config_th[i])) {
			/* print error log */
			CMDQ_LOG("%s, thread id:%d init kthread_run failed!\n",
				__func__, i);
			busy_mmsys_config_th[i] = NULL;
		}
	}
}

static void testcase_stop_busy_mmsys_config_loop(void)
{
	u32 i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		if (busy_mmsys_config_th[i] == NULL)
			continue;

		kthread_stop(busy_mmsys_config_th[i]);
		busy_mmsys_config_th[i] = NULL;
	}
}

static void testcase_read_with_mask(void)
{
	struct cmdqRecStruct *handle;
	cmdqBackupSlotHandle slot_handle = 0;
	CMDQ_VARIABLE arg_read = CMDQ_TASK_CPR_INITIAL_VALUE;
	u32 read_value = 0x00FADE00;
	u32 read_mask[2] = {0x00FF0000, 0x0000FF00};
	u32 backup_read_value = 0;
	u32 loop = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 1);
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, read_value);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	for (loop = 0; loop < ARRAY_SIZE(read_mask); loop++) {
		cmdq_task_reset(handle);
		cmdq_op_read_reg(handle, CMDQ_TEST_GCE_DUMMY_PA,
			&arg_read, read_mask[loop]);
		cmdq_op_backup_CPR(handle, arg_read, slot_handle, 0);
		cmdq_task_flush(handle);

		cmdq_cpu_read_mem(slot_handle, 0, &backup_read_value);

		if (backup_read_value != (read_value & read_mask[loop])) {
			CMDQ_TEST_FAIL(
				"read value with mask error:0x%08x expect:0x%08x\n",
				backup_read_value,
				read_value & read_mask[loop]);
		}
	}

	cmdq_free_mem(slot_handle);
	cmdq_task_destroy(handle);
	CMDQ_LOG("%s END\n", __func__);
}

/*
 * Test Efficient Polling
 * 1. Polling basic function should work
 * 2. Polling should not block low priority thread
 */
static void testcase_efficient_polling(void)
{
	struct cmdqRecStruct *h_poll, *h_low;
	u32 poll_value = 0x00FADE00, poll_mask = 0x00FF0000;
	s32 result = 0;

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* create low priority thread to simulate block case*/
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &h_low);
	cmdq_task_reset(h_low);
	cmdq_op_wait(h_low, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(h_low, false);

	/* create polling thread with trigger loop priority */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &h_poll);
	cmdq_task_reset(h_poll);
	cmdq_op_poll(h_poll, CMDQ_TEST_GCE_DUMMY_PA, poll_value, poll_mask);
	cmdq_op_finalize_command(h_poll, false);

	_test_flush_async(h_low);
	msleep_interruptible(500);
	_test_flush_async(h_poll);

	cmdq_pkt_dump_command(h_low);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	result = cmdq_pkt_wait_flush_ex_result(h_low);
	if (result < 0)
		CMDQ_TEST_FAIL(
			"Low priority thread is blocked by polling. (%d)\n",
		result);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, poll_value);
	result = cmdq_pkt_wait_flush_ex_result(h_poll);

	if (result < 0)
		CMDQ_TEST_FAIL(
			"Poll ability does not execute successfully. (%d)\n",
			result);

	cmdq_task_destroy(h_low);
	cmdq_task_destroy(h_poll);
	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_mmsys_performance(s32 test_id)
{
	struct thread_set_event_config config = {
		.event = CMDQ_SYNC_TOKEN_USER_0,
		.loop = true, .sleep_ms = 150};

	switch (test_id) {
	case 0:
		/* test GCE config only in bus idle situation */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	case 1:
		/* test GCE config only when
		 * CPU busy configure MMSYS situation
		 */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_run_busy_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_busy_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	case 2:
		/* test GCE config only when
		 * CPU busy configure non-MMSYS situation
		 */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_run_busy_non_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_busy_non_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	default:
		CMDQ_LOG(
			"[TESTCASE] mmsys performance testcase Not Found: test_id:%d\n",
			test_id);
		break;
	}
}

void _testcase_boundary_mem_inst(u32 inst_num)
{
	int i;
	struct cmdqRecStruct *handle = NULL;
	u32 data;
	u32 pattern = 0x0;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);
	if (CMDQ_REG_GET32(MMSYS_DUMMY_REG) != 0xdeaddead)
		CMDQ_ERR("%s verify pattern register fail:0x%08x\n",
			__func__, (u32)CMDQ_REG_GET32(MMSYS_DUMMY_REG));

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/* Build a buffer with N instructions. */
	CMDQ_MSG("%s record inst count:%u size:%u\n",
		__func__, inst_num, (u32)(inst_num * CMDQ_INST_SIZE));
	for (i = 0; i < inst_num; i++) {
		pattern = i;
		cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pattern, ~0);
	}

	cmdq_op_finalize_command(handle, false);
	_test_flush_async(handle);
	cmdq_pkt_dump_command(handle);

	cmdq_pkt_wait_flush_ex_result(handle);
	cmdq_task_destroy(handle);

	/* verify data */
	do {
		if (gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n",
				__func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
		if (pattern != data) {
			CMDQ_ERR(
				"TEST FAIL: reg value is 0x%08x not pattern 0x%08x\n",
				data, pattern);
		}
	} while (0);
}

void testcase_boundary_mem(void)
{
	u32 inst_num = 0;
	u32 base_inst_num = 0;
	u32 buffer_num = 0;

	CMDQ_LOG("%s\n", __func__);

	/* test cross page from 1 to 3 cases */
	for (buffer_num = 1; buffer_num < 4; buffer_num++) {
		base_inst_num = buffer_num * CMDQ_CMD_BUFFER_SIZE /
			CMDQ_INST_SIZE;

		/*
		 * We check 0~4 cases.
		 * Case 0: 3 inst (OP+EOC+JUMP) in last buffer
		 * Case 1: 2 inst (EOC+JUMP) in last buffer
		 * Case 2: last buffer empty, EOC+JUMP at end of previous buf
		 * Case 3: EOC+JUMP+Blank at end of last buffer
		 * Case 4: EOC+JUMP+2 Blank at end of last buffer
		 */
		for (inst_num = 0; inst_num < 5; inst_num++)
			_testcase_boundary_mem_inst(base_inst_num - inst_num);
	}

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_boundary_mem_param(void)
{
	u32 base_inst_num = 0;
	u32 buffer_num = (u32)gCmdqTestConfig[2];
	u32 inst_num = (u32)gCmdqTestConfig[3];

	CMDQ_LOG("%s\n", __func__);

	base_inst_num = buffer_num * CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;
	_testcase_boundary_mem_inst(base_inst_num - inst_num);

	CMDQ_LOG("%s END\n", __func__);
}

s32 _testcase_secure_handle(u32 secHandle, enum CMDQ_SCENARIO_ENUM scenario)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdqRecStruct *hReqMDP;
	const u32 PATTERN_MDP = (1 << 0) | (1 << 2) | (1 << 16);
	s32 status;

	cmdq_task_create(scenario, &hReqMDP);
	cmdq_task_reset(hReqMDP);
	cmdq_task_set_secure(hReqMDP, true);

	/* specify use MDP engine */
	hReqMDP->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0) |
		(1LL << CMDQ_ENG_MDP_WROT0);

	/* enable secure test */
	cmdq_task_secure_enable_dapc(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));
	cmdq_task_secure_enable_port_security(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));

	/* record command */
	cmdq_op_write_reg(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_MDP, ~0);
	cmdq_op_write_reg_secure(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA,
		CMDQ_SAM_H_2_MVA, secHandle, 0xf000, 0x100, 0);
	cmdq_append_command(hReqMDP, CMDQ_CODE_EOC, 0, 1, 0, 0);
	cmdq_append_command(hReqMDP, CMDQ_CODE_JUMP, 0, 8, 0, 0);

	status = cmdq_task_flush(hReqMDP);
	cmdq_task_destroy(hReqMDP);

	CMDQ_LOG("%s END\n", __func__);

	return status;
#else
	return 0;
#endif
}

void testcase_invalid_handle(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	s32 status;

	CMDQ_LOG("%s\n", __func__);

	/* In this case we use an invalid secure handle to check
	 * error handling
	 */
	status = _testcase_secure_handle(0xdeaddead, CMDQ_SCENARIO_SUB_DISP);
	if (status >= 0)
		CMDQ_ERR(
			"TEST FAIL: should not success with invalid handle, status:%d\n",
			status);

	/* Handle 0 will make SW do not translate to PA. */
	status = _testcase_secure_handle(0x0, CMDQ_SCENARIO_DEBUG);
	if (status >= 0)
		CMDQ_ERR(
			"TEST FAIL: should not success with handle 0, status:%d\n",
			status);

	CMDQ_LOG("%s END\n", __func__);
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
#endif
}

void _testcase_set_event(struct cmdqRecStruct *task, s32 thread)
{
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
}

void testcase_engineflag_conflict_dump(void)
{
	struct cmdqRecStruct *handle, *handle2;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle);
	handle->engineFlag = CMDQ_ENG_MDP_RDMA0 | CMDQ_ENG_MDP_RSZ0;
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle2);
	handle->engineFlag = CMDQ_ENG_MDP_RDMA0 | CMDQ_ENG_MDP_WROT0;
	cmdq_op_wait_no_clear(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle2, false);

	cmdq_mdp_flush_async_impl(handle);
	cmdq_mdp_flush_async_impl(handle2);

	/* After wait we should get conflict dump in log without crash. */
	msleep_interruptible(50);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_mdp_wait(handle, NULL);
	cmdq_mdp_wait(handle2, NULL);

	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_end_behavior(bool test_prefetch, u32 dummy_size)
{
	cmdqBackupSlotHandle slot_handle = 0;
	u32 *cmd_end;
	s32 loop = 0;
	u32 thread = 1;
	u32 read_result = 0;
	u32 cmd_size, last_cmd;
	u32 *va_base;
	dma_addr_t pa_base, pa;

	CMDQ_LOG("%s START with test_prefetch:%d\n", __func__, test_prefetch);

	/* create command */
	cmdq_alloc_mem(&slot_handle, 1);
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);
	va_base = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		CMDQ_CMD_BUFFER_SIZE, &pa_base, GFP_KERNEL);
	if (!va_base)
		return;
	cmd_end = va_base;
	cmd_end[1] = (CMDQ_CODE_MOVE << 24) |
		((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	cmd_end[0] = slot_handle;
	cmd_end[3] = (CMDQ_CODE_WRITE << 24) |
		((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	cmd_end[2] = 1;
	for (loop = 2; loop < dummy_size+2; loop++) {
		cmd_end[loop*2+1] = (CMDQ_CODE_WRITE << 24) |
			((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
		cmd_end[loop*2] = 1;
	}
	cmd_end[loop*2+1] = (CMDQ_CODE_WFE << 24) | CMDQ_SYNC_TOKEN_GPR_SET_4;
	cmd_end[loop*2] = ((0 << 31) | (1 << 15) | 1);
	loop++;
	cmd_end[loop*2+1] = (CMDQ_CODE_WRITE << 24) |
		((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	cmd_end[loop*2] = 2;
	last_cmd = loop;
	loop++;
	cmd_size = loop * CMDQ_INST_SIZE;
	CMDQ_LOG("verify END behavior with command size:%d\n", cmd_size);

	/* start command with 1st END position */
	CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);
	while (0x1 == (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread)))) {
		if (loop > CMDQ_MAX_LOOP_COUNT)
			CMDQ_ERR("Reset HW thread %d failed\n", thread);
		loop++;
	}
	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), 0);
	CMDQ_REG_SET32(CMDQ_THR_PREFETCH(thread), 0x1);
	if (test_prefetch)
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
			CMDQ_REG_SHIFT_ADDR(pa_base + cmd_size));
	else {
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
			CMDQ_REG_SHIFT_ADDR(
			pa_base + cmd_size - CMDQ_INST_SIZE));
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);
	}
	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
		CMDQ_REG_SHIFT_ADDR(pa_base));
	pa = cmdq_core_get_pc(thread);
	CMDQ_LOG("Step0: PC:%pa\n", &pa);
	CMDQ_REG_SET32(CMDQ_THR_CFG(thread), 0x6);
	CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread), 0x011);
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);

	msleep_interruptible(50);
	/* check if GCE stop in expected position */
	cmdq_cpu_read_mem(slot_handle, 0, &read_result);
	pa = cmdq_core_get_pc(thread);
	CMDQ_LOG("Step1: PC:%pa result:%d\n", &pa, read_result);
	if (read_result != 1)
		CMDQ_TEST_FAIL("%s read result fail: result:%d\n",
			__func__, read_result);

	/* adjust command and reset END addr */
	cmd_end[last_cmd*2] = 3;
	CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
		CMDQ_REG_SHIFT_ADDR(pa_base + cmd_size));
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);
	/* check if GCE read correct instruction */
	cmdq_cpu_read_mem(slot_handle, 0, &read_result);
	pa = cmdq_core_get_pc(thread);
	CMDQ_LOG("Step2: PC:%pa result:%d\n", &pa, read_result);
	if ((!test_prefetch && read_result != 3) ||
		(test_prefetch && read_result != 2))
		CMDQ_TEST_FAIL("%s read result fail: result:%d\n",
			__func__, read_result);

	/* stop GCE thread */
	pa = cmdq_core_get_pc(thread);
	CMDQ_LOG("Step3: PC:%pa\n", &pa);
	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);
	if ((0x01 & CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread))) == 0)
		CMDQ_LOG("[WARNING] thread %d suspend not effective\n",
			thread);

	loop = 0;
	while ((CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread)) & 0x2) == 0x0) {
		if (loop > CMDQ_MAX_LOOP_COUNT)
			CMDQ_ERR("Suspend HW thread %d failed\n", thread);
		loop++;
	}
	loop = 0;
	CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);
	while (0x1 == (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread)))) {
		if (loop > CMDQ_MAX_LOOP_COUNT)
			CMDQ_ERR("Reset HW thread %d failed\n", thread);
		loop++;
	}
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x00);
	cmdq_free_mem(slot_handle);
	cmdq_core_free_hw_buffer(cmdq_dev_get(), CMDQ_CMD_BUFFER_SIZE,
		va_base, pa_base);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_end_addr_conflict(void)
{
	struct cmdqRecStruct *loop_handle, *submit_handle;
	u32 index;
	u32 test_thread = cmdq_get_func()->dispThread(
		CMDQ_SCENARIO_DEBUG_PREFETCH);
	u32 test_thread_end = CMDQ_THR_FIX_END_ADDR(test_thread);

	/* build trigger loop to write END addr value */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &loop_handle);
	cmdq_task_reset(loop_handle);
	cmdq_task_set_secure(loop_handle, gCmdqTestSecure);
	for (index = 0; index < 48; index++)
		cmdq_op_write_reg(loop_handle, CMDQ_TEST_GCE_DUMMY_PA,
			test_thread_end, ~0);

	cmdq_task_start_loop(loop_handle);

	/* repeatly submit task to catch error */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &submit_handle);
	for (index = 0; index < 48; index++) {
		cmdq_task_reset(submit_handle);
		cmdq_task_set_secure(submit_handle, gCmdqTestSecure);
		cmdq_op_write_reg(submit_handle, CMDQ_TEST_GCE_DUMMY_PA,
			test_thread_end, ~0);
		cmdq_task_flush(submit_handle);
		CMDQ_LOG("Flush test round #%u\n", index);
		msleep_interruptible(500);
	}

	cmdq_task_stop_loop(loop_handle);
	cmdq_task_destroy(loop_handle);
	cmdq_task_destroy(submit_handle);
}

void testcase_verify_timer(void)
{
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle slot_handle = 0;
	u32 start_time = 0, end_time = 0;
	const u32 tpr_mask = ~0;

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TPR_MASK, tpr_mask);

	cmdq_alloc_mem(&slot_handle, 1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	cmdq_op_backup_TPR(handle, slot_handle, 0);

	cmdq_task_flush(handle);
	cmdq_cpu_read_mem(slot_handle, 0, &start_time);

	msleep_interruptible(10);

	cmdq_task_flush(handle);
	cmdq_cpu_read_mem(slot_handle, 0, &end_time);

	if (start_time != end_time) {
		CMDQ_LOG("TEST SUCCESS: start:%u end:%u dur:%u mask:0x%08x\n",
			start_time, end_time, end_time - start_time, tpr_mask);
	} else {
		CMDQ_TEST_FAIL("start:%u end:%u dur:%u mask:0x%08x\n",
			start_time, end_time, end_time - start_time, tpr_mask);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_remove_by_file_node(void)
{
	struct cmdqRecStruct *handle[2], *handle_conflict;
	u64 engines[2] = {
		1LL << CMDQ_ENG_MDP_RSZ0,
		1LL << CMDQ_ENG_MDP_WROT0,
	};
	const u64 node = 0xfffffffcdead0000;
	s32 status;

	CMDQ_LOG("%s\n", __func__);

	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle[0]);
	cmdq_task_set_secure(handle[0], gCmdqTestSecure);
	cmdq_task_set_engine(handle[0], engines[0]);
	cmdq_op_wait(handle[0], CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle[0], false);
	status = _test_flush_task_async(handle[0]);
	if (status < 0) {
		CMDQ_TEST_FAIL("%s flush handle 0 fail:%d\n", __func__, status);
		return;
	}

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle[1]);
	cmdq_task_set_secure(handle[1], gCmdqTestSecure);
	cmdq_task_set_engine(handle[1], engines[1]);
	cmdq_op_wait(handle[1], CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle[1], false);
	status = _test_flush_task_async(handle[1]);
	if (status < 0) {
		CMDQ_TEST_FAIL("%s flush handle 1 fail:%d\n", __func__, status);
		return;
	}

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_MDP, &handle_conflict);
	cmdq_task_set_secure(handle_conflict, gCmdqTestSecure);
	cmdq_task_set_engine(handle_conflict, engines[0] | engines[1]);
	cmdq_op_wait(handle_conflict, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle_conflict, false);
	status = _test_flush_task_async(handle_conflict);
	if (status < 0) {
		CMDQ_TEST_FAIL("%s flush handle conflict fail:%d\n",
			__func__, status);
		return;
	}

	handle[0]->node_private = (void *)(unsigned long)node;
	handle[1]->node_private = (void *)(unsigned long)node;
	handle_conflict->node_private = (void *)(unsigned long)node;

	/* all task should be remove and no crash */
	cmdq_mdp_release_task_by_file_node((void *)(unsigned long)node);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_cmdq_trigger_devapc(void)
{
	struct cmdqRecStruct *handle = NULL;
	u32 PATTERN = 0xdeadabcd;
	u32 dummy_pa = 0x14009000;

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, ~0);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_timeout_error(void)
{
	struct cmdqRecStruct *handle;

	CMDQ_LOG("%s\n", __func__);

	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush(handle);
	cmdq_core_reset_first_dump();

	CMDQ_LOG("%s end\n", __func__);
}

/* CMDQ driver stress test */

enum ENGINE_POLICY_ENUM {
	CMDQ_TESTCASE_ENGINE_NOT_SET,
	CMDQ_TESTCASE_ENGINE_SAME,
	CMDQ_TESTCASE_ENGINE_RANDOM,
};

enum WAIT_POLICY_ENUM {
	CMDQ_TESTCASE_WAITOP_NOT_SET,
	CMDQ_TESTCASE_WAITOP_ALWAYS,
	CMDQ_TESTCASE_WAITOP_RANDOM,
	CMDQ_TESTCASE_WAITOP_RANDOM_NOTIMEOUT,
	CMDQ_TESTCASE_WAITOP_BEFORE_END,
};

enum BRANCH_POLICY_ENUM {
	CMDQ_TESTCASE_BRANCH_NONE = 0,
	CMDQ_TESTCASE_BRANCH_CONTINUE,
	CMDQ_TESTCASE_BRANCH_BREAK,
	CMDQ_TESTCASE_BRANCH_MAX,
};

enum CONDITION_TEST_POLICY_ENUM {
	CMDQ_TESTCASE_CONDITION_NONE,
	CMDQ_TESTCASE_CONDITION_RANDOM,
	CMDQ_TESTCASE_CONDITION_RANDOM_MORE,
};

enum ROUND_LOOP_TIME_POLICY_ENUM {
	CMDQ_TESTCASE_LOOP_FAST = 4,
	CMDQ_TESTCASE_LOOP_MEDIUM = 16,
	CMDQ_TESTCASE_LOOP_SLOW = 60,
};

enum POLL_POLICY_ENUM {
	CMDQ_TESTCASE_POLL_NONE,
	CMDQ_TESTCASE_POLL_PASS,
	CMDQ_TESTCASE_POLL_ALL,
};

enum TRIGGER_THREAD_POLICY_ENUM {
	CMDQ_TESTCASE_TRIGGER_RANDOM = 0,
	CMDQ_TESTCASE_TRIGGER_FAST = 4,
	CMDQ_TESTCASE_TRIGGER_MEDIUM = 16,
	CMDQ_TESTCASE_TRIGGER_SLOW = 80,
};

enum SECURE_POLICY_ENUM {
	CMDQ_TESTCASE_NO_SECURE,
	CMDQ_TESTCASE_SECURE_RANDOM,
};

struct stress_policy {
	enum ENGINE_POLICY_ENUM engines_policy;
	enum WAIT_POLICY_ENUM wait_policy;
	enum CONDITION_TEST_POLICY_ENUM condition_policy;
	enum ROUND_LOOP_TIME_POLICY_ENUM loop_policy;
	enum POLL_POLICY_ENUM poll_policy;
	enum TRIGGER_THREAD_POLICY_ENUM trigger_policy;
	enum SECURE_POLICY_ENUM secure_policy;
};

struct thread_param {
	struct completion cmplt;
	atomic_t stop;
	u32 run_count;
	bool multi_task;
	struct stress_policy policy;
};

struct random_data {
	struct work_struct release_work;
	struct thread_param *thread;
	struct cmdqRecStruct *handle;
	enum CMDQ_SCENARIO_ENUM scenario;
	atomic_t *ref_count;
	cmdqBackupSlotHandle slot;
	u32 round;
	u32 *slot_expect_values;
	u32 slot_reserve_count;
	u32 slot_count;
	u32 last_write;
	u32 mask;
	u32 inst_count;
	u32 wait_count;
	unsigned long dummy_reg_pa;
	unsigned long dummy_reg_va;
	u32 expect_result;
	CMDQ_VARIABLE var_result;
	bool may_wait;
	u32 condition_count;
	u32 poll_count;
};

/* trigger thread only set these bits */
#define CMDQ_TEST_POLL_BIT 0xFFFFFFF

/* secure task command buffer is limited */
#define CMDQ_MAX_SECURE_INST_COUNT (0x7F00 / CMDQ_INST_SIZE)

bool _is_boundary_offset(u32 offset)
{
	u32 offset_idx = offset / CMDQ_INST_SIZE;
	u32 buffer_inst_count = CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;
	u32 offset_idx_mod = ((offset_idx + (offset_idx /
		(buffer_inst_count - 1))) % buffer_inst_count);

	return (offset_idx_mod == 0 || offset_idx_mod == 1);
}

static bool _append_op_read_mem_to_reg(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	u32 slot_idx = 0;

	slot_idx = (get_random_int() % data->slot_count) +
		data->slot_reserve_count;
	cmdq_op_read_mem_to_reg(handle, data->slot, slot_idx,
		data->dummy_reg_pa);
	data->last_write = data->slot_expect_values[slot_idx];
	return true;
}

static bool _append_op_read_reg_to_mem(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	u32 slot_idx = 0;

	if (data->thread->multi_task)
		return true;

	slot_idx = (get_random_int() % data->slot_count) +
		data->slot_reserve_count;
	cmdq_op_read_reg_to_mem(handle, data->slot, slot_idx,
		data->dummy_reg_pa);
	data->slot_expect_values[slot_idx] = data->last_write;
	return true;
}

static bool _append_op_write_reg(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	u32 random_value = get_random_int();
	bool use_mask = get_random_int() % 10;

	if (use_mask) {
		data->mask = get_random_int();
		random_value = (data->last_write & ~data->mask) |
			(random_value & data->mask);
	} else {
		data->mask = ~0;
	}
	cmdq_op_write_reg(handle, data->dummy_reg_pa, data->last_write,
		data->mask);
	data->last_write = random_value;
	return true;
}

static bool _append_op_wait(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	const u32 max_wait_count = 2;
	const u32 max_wait_bound_count = 5;
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};
	unsigned long token;

	if (!data->may_wait || data->wait_count > max_wait_bound_count)
		return true;

	/* we save few chance to insert wait in boundary case */
	if (data->wait_count > max_wait_count &&
		handle->pkt->cmd_buf_size >= CMDQ_CMD_BUFFER_SIZE &&
		!_is_boundary_offset(handle->pkt->cmd_buf_size))
		return true;

	token = tokens[get_random_int() % 2];
	data->wait_count++;
	if (get_random_int() % 8)
		cmdq_op_wait_no_clear(handle, token);
	else
		cmdq_op_wait(handle, token);
	return true;
}

static bool _append_op_poll(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	const unsigned long dummy_poll_pa =
		CMDQ_GPR_R32_PA(CMDQ_GET_GPR_PX2RX_LOW(
		CMDQ_DATA_REG_2D_SHARPNESS_0_DST));
	const u32 max_poll_count = 2;
	const u32 min_poll_instruction = 18;
	u32 poll_bit = 0;
	u32 size_before = handle->pkt->cmd_buf_size;

	if (data->thread->policy.poll_policy == CMDQ_TESTCASE_POLL_NONE ||
		limit_size < CMDQ_INST_SIZE * min_poll_instruction ||
		data->poll_count >= max_poll_count)
		return true;

	CMDQ_MSG("%s limit:%u block size:%zu\n",
		__func__, limit_size, handle->pkt->cmd_buf_size);

	data->poll_count++;
	if (data->thread->policy.poll_policy == CMDQ_TESTCASE_POLL_PASS)
		poll_bit = 1 << (get_random_int() % 28);
	else
		poll_bit = 1 << (get_random_int() % 32);
	cmdq_op_write_reg(handle, dummy_poll_pa, 0, poll_bit);
	size_before = handle->pkt->cmd_buf_size;
	cmdq_op_poll(handle, dummy_poll_pa, poll_bit, poll_bit);

	CMDQ_LOG(
		"%s round:%u poll:0x%08x count:%u block size:%zu op size:%zu scenario:%d\n",
		__func__, data->round, poll_bit, data->poll_count,
		handle->pkt->cmd_buf_size,
		handle->pkt->cmd_buf_size - size_before,
		handle->scenario);

	return true;
}

static bool _append_op_add(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	cmdq_op_add(handle, &data->var_result, data->var_result, 1);
	data->expect_result++;
	return true;
}

bool _append_op_while(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size, u32 *op_result)
{
	/*
	 * Pattern of generate op:
	 * while (variable < condition_limit)
	 *     variable = variable + 1  (1~N times)
	 *     break/continue/empty
	 *     variable = variable + 1  (0~N times)
	 * end while
	 */

	u32 expect_result = 0;
	u32 condition_limit, pre_op_count, post_op_count, i;
	u32 remain_op_count = limit_size / CMDQ_INST_SIZE;
	enum BRANCH_POLICY_ENUM branch_op;

	if (data->expect_result >= 0xFFFF)
		return true;

	/* condition value accept max 16 bit value */
	condition_limit = get_random_int() % (0xFFFF - data->expect_result);
	pre_op_count = get_random_int() % remain_op_count + 1;
	post_op_count = remain_op_count > pre_op_count ?
		get_random_int() % (remain_op_count - pre_op_count) : 0;
	branch_op = get_random_int() % (u32)CMDQ_TESTCASE_BRANCH_MAX;

	if (limit_size > 0xFFFF || pre_op_count > 0xFFFF ||
		post_op_count > 0xFFFF) {
		CMDQ_ERR(
			"%s start size:%zu limit:%u pre/post:%u/%u op:%u current:%u condition:%u\n",
			__func__, handle->pkt->cmd_buf_size, limit_size,
			pre_op_count, post_op_count, branch_op,
			data->expect_result, condition_limit);
		return false;
	}

	CMDQ_MSG(
		"%s start size:%zu limit:%u pre/post:%u/%u op:%u current:%u condition:%u\n",
		__func__, handle->pkt->cmd_buf_size, limit_size,
		pre_op_count, post_op_count, branch_op,
		data->expect_result, condition_limit);

	cmdq_op_while(handle, data->var_result, CMDQ_LESS_THAN,
		condition_limit + data->expect_result);
	for (i = 0; i < pre_op_count; i++)
		cmdq_op_add(handle, &data->var_result, data->var_result, 1);

	if (data->expect_result < data->expect_result + condition_limit) {
		switch (branch_op) {
		case CMDQ_TESTCASE_BRANCH_CONTINUE:
			cmdq_op_continue(handle);
			while (expect_result < condition_limit)
				expect_result += pre_op_count;
			break;
		case CMDQ_TESTCASE_BRANCH_BREAK:
			cmdq_op_break(handle);
			expect_result = pre_op_count;
			break;
		case CMDQ_TESTCASE_BRANCH_NONE:
		default:
			while (expect_result < condition_limit)
				expect_result += pre_op_count + post_op_count;
			break;
		}
	} else {
		/* content of while loop would not run */
		expect_result = 0;
	}

	for (i = 0; i < post_op_count; i++)
		cmdq_op_add(handle, &data->var_result, data->var_result, 1);
	cmdq_op_end_while(handle);

	*op_result = expect_result;
	data->condition_count += 1;

	CMDQ_MSG("%s result:%u size:%zu\n",
		__func__, expect_result, handle->pkt->cmd_buf_size);
	return true;
}

bool _test_is_condition_match(enum CMDQ_CONDITION_ENUM condition_op,
	u32 arg_a, u32 arb_b)
{
	bool match;

	switch (condition_op) {
	case CMDQ_EQUAL:
		match = (arg_a == arb_b);
		break;
	case CMDQ_NOT_EQUAL:
		match = (arg_a != arb_b);
		break;
	case CMDQ_GREATER_THAN_AND_EQUAL:
		match = (arg_a >= arb_b);
		break;
	case CMDQ_LESS_THAN_AND_EQUAL:
		match = (arg_a <= arb_b);
		break;
	case CMDQ_GREATER_THAN:
		match = (arg_a > arb_b);
		break;
	case CMDQ_LESS_THAN:
		match = (arg_a < arb_b);
		break;
	default:
		CMDQ_TEST_FAIL("%s cannot recognize IF condition:%u\n",
			__func__, (s32)condition_op);
		match = false;
		break;
	}

	return match;
}

static bool _append_op_if(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size, u32 *op_result)
{
	/*
	 * Pattern of generate op:
	 * if (variable op condition_limit)
	 *     variable = variable + 1  (1~N times)
	 * else if (variable op condition_limit)  (0~N times)
	 *     variable = variable + 1  (1~N times)
	 * else/empty
	 *     variable = variable + 1  (1~N times)
	 * end if
	 */

	const u32 min_op_count = 2;
	const u32 reserve_op_count = 2;
	u32 current_result = 0;
	u32 ramain_op_count = limit_size / CMDQ_INST_SIZE;
	u32 op_if_counter = 0;
	bool already_matched = false;
	bool terminate = false;
	enum IF_BRANCH_OP_ENUM {
		BRANCH_NONE,
		BRANCH_ELSEIF,
		BRANCH_ELSE,
		BRANCH_MAX
	};

	if (data->expect_result >= 0xFFFF)
		return true;

	CMDQ_MSG("%s start size:%zu limit:%u\n",
		__func__, handle->pkt->cmd_buf_size, limit_size);

	while (ramain_op_count > min_op_count + reserve_op_count &&
		!terminate && data->expect_result + current_result < 0xFFFF) {
		u32 logic_op_count = 0;
		u32 condition_limit, i;
		enum IF_BRANCH_OP_ENUM branch_op;
		enum CMDQ_CONDITION_ENUM condition_op;

		/* reserve for if/add+1 */
		ramain_op_count -= reserve_op_count;

		/* condition value accept max 16 bit value */
		condition_limit = get_random_int() % (0xFFFF - current_result -
			data->expect_result);
		condition_op = get_random_int() % (u32)CMDQ_CONDITION_MAX;

		if (op_if_counter == 0) {
			cmdq_op_if(handle, data->var_result, condition_op,
				condition_limit);
			branch_op = BRANCH_MAX;
		} else {
			branch_op = get_random_int() % (u32)BRANCH_MAX;

			switch (branch_op) {
			case BRANCH_ELSEIF:
				cmdq_op_else_if(handle, data->var_result,
					condition_op, condition_limit);
				break;
			case BRANCH_ELSE:
				cmdq_op_else(handle);
				break;
			case BRANCH_NONE:
			default:
				terminate = true;
				break;
			}
		}

		if (terminate)
			break;

		logic_op_count = get_random_int() % ramain_op_count + 1;
		for (i = 0; i < logic_op_count; i++)
			cmdq_op_add(handle, &data->var_result,
				data->var_result, 1);
		ramain_op_count -= logic_op_count;

		if (!already_matched) {
			if (branch_op != BRANCH_ELSE) {
				already_matched = _test_is_condition_match(
					condition_op, data->expect_result,
					condition_limit);
			} else {
				already_matched = true;
			}

			if (already_matched)
				current_result += logic_op_count;
		}

		if (branch_op == BRANCH_ELSE)
			terminate = true;

		CMDQ_MSG(
			"%s if:%u remaind:%u logic:%u condition:%u branch:%u limit:%u matched:%u\n",
			__func__, op_if_counter, ramain_op_count,
			logic_op_count, (u32)condition_op, (s32)branch_op,
			condition_limit, already_matched);
		op_if_counter++;
	}

	if (op_if_counter > 0) {
		cmdq_op_end_if(handle);
		data->condition_count += 1;
	}

	*op_result = current_result;

	CMDQ_MSG("%s result:%u size:%zu\n", __func__, current_result,
		handle->pkt->cmd_buf_size);

	return true;
}

static bool _append_op_condition(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	typedef bool(*append_op_func)(struct cmdqRecStruct *handle,
		struct random_data *data, u32 limit_size, u32 *result);
	const append_op_func op_funcs[] = {
		_append_op_while,
		_append_op_if,
	};
	const u32 min_op_count = 4;
	u32 op_result = 0;
	u32 limit_op_count;

	CMDQ_MSG("%s start limit:%u\n", __func__, limit_size);

	if (data->thread->policy.condition_policy ==
		CMDQ_TESTCASE_CONDITION_NONE)
		return true;

	if (data->thread->policy.condition_policy ==
		CMDQ_TESTCASE_CONDITION_RANDOM &&
		data->condition_count >= 1)
		return true;

	/* make op count 4~N */
	limit_op_count = get_random_int() % (limit_size /
		CMDQ_INST_SIZE - min_op_count);
	limit_op_count += min_op_count;

	if (!op_funcs[get_random_int() % ARRAY_SIZE(op_funcs)](handle, data,
		limit_op_count * CMDQ_INST_SIZE, &op_result))
		return false;
	data->expect_result += op_result;

	CMDQ_MSG("%s op result:%u expect:%u\n",
		__func__, op_result, limit_size);

	return true;
}

static bool _append_random_instructions(struct cmdqRecStruct *handle,
	struct random_data *random_context, u32 limit_size)
{
	typedef bool(*append_op_func)(struct cmdqRecStruct *handle,
		struct random_data *data, u32 limit_size);
	const append_op_func op_funcs[] = {
		_append_op_read_mem_to_reg,
		_append_op_read_reg_to_mem,
		_append_op_write_reg,
		_append_op_wait,
		_append_op_poll,

		/* v3 instructions */
		_append_op_add,
		_append_op_condition,
	};
	const u32 min_condition_op_count = 4;

	while (handle->pkt->cmd_buf_size < limit_size) {
		u32 total_func = ARRAY_SIZE(op_funcs);
		s32 op_idx;

		if (limit_size - handle->pkt->cmd_buf_size <=
			CMDQ_INST_SIZE * min_condition_op_count)
			total_func--;
		op_idx = get_random_int() % total_func;
		if (!op_funcs[op_idx](handle, random_context,
			limit_size - handle->pkt->cmd_buf_size))
			return false;
	}

	return true;
}

bool _stress_is_ignore_timeout(struct stress_policy *policy)
{
	bool ignore = false;

	/* exclude some case that timeout is expected */
	if (policy->wait_policy != CMDQ_TESTCASE_WAITOP_NOT_SET) {
		/* insert wait op into testcase will trigger timeout */
		ignore = true;
	} else if (policy->engines_policy ==
		CMDQ_TESTCASE_ENGINE_RANDOM) {
		/* random engine flag may cause task never able to run,
		 * thus timedout
		 */
		ignore = true;
	} else if (policy->poll_policy != CMDQ_TESTCASE_POLL_NONE) {
		/* timeout due to poll fail */
		ignore = true;
	}

	return ignore;
}

static void _dump_stress_task_result(s32 status,
	struct random_data *random_context)
{
	u32 result_val = 0, i = 0;
	bool error_happen = false;

	do {
		if (status == -ETIMEDOUT) {
			error_happen = !_stress_is_ignore_timeout(
				&random_context->thread->policy);
			if (!error_happen) {
				CMDQ_LOG(
					"Timeout handle:0x%p round:%u skip compare ...\n",
					random_context->handle,
					random_context->round);
			}

			break;
		} else if (status == -EINVAL &&
			random_context->handle->thread == CMDQ_INVALID_THREAD &&
			!cmdq_get_func()->isDynamic(
			random_context->handle->scenario)) {
			CMDQ_LOG(
				"Fail to run handle:0x%p round:%u scenario:%d thread:%d skip compare\n",
				random_context->handle,
				random_context->round,
				random_context->handle->scenario,
				random_context->handle->thread);
		} else if (status < 0) {
			error_happen = true;
			break;
		}

		/* register may write by other task, thus only check in
		 * multi-task case
		 */
		if (!random_context->thread->multi_task) {
			result_val = CMDQ_REG_GET32(
				random_context->dummy_reg_va);
			if (result_val != random_context->last_write) {
				CMDQ_TEST_FAIL(
					"Reg value does not match:0x%08x to 0x%08x round:%u\n",
					result_val, random_context->last_write,
					random_context->round);
				error_happen = true;
			}
		}

		for (i = random_context->slot_reserve_count;
			i < random_context->slot_reserve_count +
				random_context->slot_count; i++) {
			cmdq_cpu_read_mem(random_context->slot, i,
				&result_val);
			if (result_val !=
				random_context->slot_expect_values[i]) {
				CMDQ_TEST_FAIL(
					"Slot %u value does not match expect:0x%08x reg 0x%08x round:%u\n",
					i,
					random_context->slot_expect_values[i],
					result_val, random_context->round);
				error_happen = true;
			}
		}

		cmdq_cpu_read_mem(random_context->slot, 1, &result_val);
		if (result_val != random_context->expect_result) {
			CMDQ_TEST_FAIL(
				"Counter value not match expect:0x%08x mem:0x%08x round:%u\n",
				random_context->expect_result, result_val,
				random_context->round);
			error_happen = true;
		}
	} while (0);

	if (error_happen) {
		char long_msg[CMDQ_LONGSTRING_MAX];
		u32 msg_offset;
		s32 msg_max_size;
		s32 poll_value = CMDQ_REG_GET32(CMDQ_GPR_R32(
			CMDQ_GET_GPR_PX2RX_LOW(
			CMDQ_DATA_REG_2D_SHARPNESS_0_DST)));

		atomic_set(&random_context->thread->stop, 1);

		cmdq_long_string_init(true, long_msg, &msg_offset,
			&msg_max_size);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			"handle:0x%p round:%u size:%zu wait:%d:%d multi:%d engine:%d",
			random_context->handle,
			random_context->round,
			random_context->handle->pkt->cmd_buf_size,
			random_context->may_wait,
			random_context->wait_count,
			random_context->thread->multi_task,
			random_context->thread->policy.engines_policy);
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			" condition:%d conditions:%u status:%d poll:%d reg:0x%08x count:%u exec:%d\n",
			random_context->thread->policy.condition_policy,
			random_context->condition_count,
			status,
			random_context->thread->policy.poll_policy,
			poll_value,
			random_context->poll_count,
			atomic_read(&random_context->handle->exec));
		CMDQ_TEST_FAIL("%s", long_msg);

		/* wait other threads stop print messages */
		msleep_interruptible(10);
		cmdq_pkt_dump_command(random_context->handle);
	}
}

static void _testcase_stress_release_work(struct work_struct *work_item)
{
	struct random_data *random_context = (struct random_data *)container_of(
		work_item, struct random_data, release_work);
	s32 status = 0;

	do {
		if (!random_context->handle) {
			CMDQ_TEST_FAIL("Handle not exists, round:%u\n",
				random_context->round);
			break;
		}

		status = _test_wait_task(random_context->handle);
		_dump_stress_task_result(status, random_context);
	} while (0);

	cmdq_task_destroy(random_context->handle);
	cmdq_free_mem(random_context->slot);
	atomic_dec(random_context->ref_count);
	kfree(random_context->slot_expect_values);
	kfree(random_context);
}

void _testcase_stress_submit_release_work(struct work_struct *work_item)
{
	struct random_data *random_context = (struct random_data *)container_of(
		work_item, struct random_data, release_work);
	s32 status = 0;

	do {
		status = _test_flush_task(random_context->handle);
		_dump_stress_task_result(status, random_context);
	} while (0);

	cmdq_task_destroy(random_context->handle);
	cmdq_free_mem(random_context->slot);
	atomic_dec(random_context->ref_count);
	kfree(random_context->slot_expect_values);
	kfree(random_context);
}

void _testcase_on_exec_suspend(struct cmdqRecStruct *task, s32 thread)
{
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};

	cmdqCoreSetEvent(tokens[get_random_int() % ARRAY_SIZE(tokens)]);
}

#define MAX_RELEASE_QUEUE 4
static const u32 stress_engines[] = {CMDQ_ENG_MDP_CAMIN, CMDQ_ENG_MDP_RDMA0,
	CMDQ_ENG_MDP_RDMA1, CMDQ_ENG_MDP_WROT0, CMDQ_ENG_MDP_WROT1};

static const enum CMDQ_SCENARIO_ENUM stress_scene[] = {
	CMDQ_SCENARIO_DEBUG_MDP, CMDQ_SCENARIO_DEBUG_MDP,
	CMDQ_SCENARIO_DEBUG_MDP, CMDQ_SCENARIO_DEBUG,};

static s32 _testcase_gen_task_thread_each(void *data, u32 task_count,
	atomic_t *task_ref_count, u32 engine_sel, bool ignore_timeout,
	struct workqueue_struct *release_queues[])
{
	struct thread_param *thread_data = (struct thread_param *)data;
	const unsigned long dummy_reg_va = CMDQ_TEST_GCE_DUMMY_VA;
	const unsigned long dummy_reg_pa = CMDQ_TEST_GCE_DUMMY_PA;
	const u32 inst_count_pattern[] = {
		1, 2, 3, 4, 5, 6, 7, 8,
		32, 64, 128, 256, 512, 1024};
	const u32 reserve_slot_count = 2;
	const u32 max_slot_count = 4;
	const u32 total_slot = reserve_slot_count + max_slot_count;
	const u32 max_buffer_count = 4;
	const u32 *curr_eng = stress_engines;
	u32 engine_cnt = 0;

	struct random_data *random_context = NULL;
	struct cmdqRecStruct *handle = NULL;
	u32 i = 0;
	s32 status = 0;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	bool is_secure = (get_random_int() % 20) == 0;
#endif

	random_context = kzalloc(sizeof(*random_context), GFP_KERNEL);
	random_context->thread = thread_data;
	random_context->round = task_count;
	random_context->dummy_reg_pa = dummy_reg_pa;
	random_context->dummy_reg_va = dummy_reg_va;
	random_context->ref_count = task_ref_count;
	random_context->slot_reserve_count = reserve_slot_count;
	random_context->slot_count = max_slot_count;
	random_context->slot_expect_values = kcalloc(total_slot,
		sizeof(*random_context->slot_expect_values), GFP_KERNEL);
	random_context->scenario =
		stress_scene[get_random_int() % ARRAY_SIZE(stress_scene)];

	if (cmdq_get_func()->isDynamic(random_context->scenario)) {
		curr_eng = stress_engines;
		engine_cnt = ARRAY_SIZE(stress_engines);
	}

	cmdq_alloc_mem(&random_context->slot, total_slot);
	for (i = 0; i < reserve_slot_count; i++) {
		/*
		 * slot 0: Final dummy register value read back.
		 * slot 1: Logic add counter
		 */
		cmdq_cpu_write_mem(random_context->slot, i, 0);
		random_context->slot_expect_values[i] = 0;
	}
	for (i = reserve_slot_count; i < total_slot; i++) {
		cmdq_cpu_write_mem(random_context->slot, i, i);
		random_context->slot_expect_values[i] = i;
	}

	/* if final result is 0xff000000, this task never run */
	cmdq_cpu_write_mem(random_context->slot, 0, 0xff000000);

	cmdq_task_create(random_context->scenario, &handle);
	cmdq_task_reset(handle);
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdq_task_set_secure(handle, is_secure);
#endif
	random_context->handle = handle;

	/* variable for final result */
	random_context->var_result = 0;

	switch (thread_data->policy.wait_policy) {
	case CMDQ_TESTCASE_WAITOP_NOT_SET:
	case CMDQ_TESTCASE_WAITOP_BEFORE_END:
		random_context->may_wait = false;
		break;
	case CMDQ_TESTCASE_WAITOP_ALWAYS:
		random_context->may_wait = true;
		break;
	case CMDQ_TESTCASE_WAITOP_RANDOM:
	case CMDQ_TESTCASE_WAITOP_RANDOM_NOTIMEOUT:
		/* give 1/10 chance the task has wait instructions */
		random_context->may_wait = get_random_int() % 10;
		break;
	default:
		random_context->may_wait = get_random_int() % 10;
		break;
	}

	for (i = 0; i < (get_random_int() % max_buffer_count) + 1; i++) {
		u32 seed = get_random_int() % ARRAY_SIZE(inst_count_pattern);

		random_context->inst_count += inst_count_pattern[seed];
	}

#ifdef CMDQ_SECURE_PATH_SUPPORT
	/* decide if this task can be secure */
	if (thread_data->policy.secure_policy == CMDQ_TESTCASE_SECURE_RANDOM &&
		random_context->inst_count < CMDQ_MAX_SECURE_INST_COUNT) {
		random_context->dummy_reg_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
		random_context->dummy_reg_va = CMDQ_TEST_MMSYS_DUMMY_VA;
	}
#endif

	/* set engine flag and priority */
	handle->pkt->priority = get_random_int() % 16;
	if (thread_data->policy.engines_policy ==
		CMDQ_TESTCASE_ENGINE_RANDOM) {
		u64 rand_eng = 0;

		engine_sel = get_random_int() %
			((1 << engine_cnt) - 1);
		for (i = 0; i < engine_cnt; i++) {
			if (((1 << i) & engine_sel) != 0)
				rand_eng = rand_eng | (1 << curr_eng[i]);
		}
		cmdq_task_set_engine(handle, rand_eng);
	} else {
		cmdq_task_set_engine(handle, engine_sel);
	}

	if (!thread_data->multi_task) {
		/* clear the reg first */
		cmdq_op_write_reg(handle, dummy_reg_pa, 0, ~0);
	}

	cmdq_op_assign(handle, &random_context->var_result, 0);

	/* append random instructions */
	if (random_context->inst_count > 4) {
		if (!_append_random_instructions(handle, random_context,
			(random_context->inst_count - 4) *
				CMDQ_INST_SIZE)) {
			CMDQ_ERR(
				"%s error during append random instructions\n",
				__func__);
			atomic_set(&thread_data->stop, 1);
			cmdq_free_mem(random_context->slot);
			kfree(random_context->slot_expect_values);
			kfree(random_context);
			return -EINVAL;
		}
	}

	if (!thread_data->multi_task) {
		cmdqRecBackupRegisterToSlot(handle,
			random_context->slot, 0, dummy_reg_pa);
		random_context->slot_expect_values[0] =
			random_context->last_write;
	}

	/* Note: backup to slot 1 which is reserved for register
	 * final value
	 */
	cmdq_op_backup_CPR(handle, random_context->var_result,
		random_context->slot, 1);

	if (thread_data->policy.wait_policy ==
		CMDQ_TESTCASE_WAITOP_BEFORE_END) {
		/* make sure task wait before eoc */
		cmdq_op_clear_event(handle, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
		/* do not queue too much tasks in thread */
		if ((u32)atomic_read(task_ref_count) >= 2)
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	}

	if (get_random_int() % 2) {
		status = cmdq_op_finalize_command(handle, false);
		if (status < 0) {
			CMDQ_ERR("Fail to finalize round:%u status:%d\n",
				task_count, status);
			cmdq_free_mem(random_context->slot);
			kfree(random_context->slot_expect_values);
			kfree(random_context);
			return -EINVAL;
		}

		/* async submit case, with contains more info
		 * during release
		 */
		status = _test_flush_task_async(handle);
		if (status < 0) {
			CMDQ_ERR("Fail to submit round:%u status:%d\n",
				task_count, status);
			cmdq_free_mem(random_context->slot);
			kfree(random_context->slot_expect_values);
			kfree(random_context);
			return -EINVAL;
		}

		INIT_WORK(&random_context->release_work,
			_testcase_stress_release_work);
		atomic_inc(task_ref_count);

		CMDQ_LOG(
			"Round:%u handle:0x%p pkt:0x%p thread:%d size:%zu ref:%u scenario:%d flag:0x%llx start async\n",
			task_count, random_context->handle,
			random_context->handle->pkt,
			random_context->handle->thread,
			random_context->handle->pkt->cmd_buf_size,
			(u32)atomic_read(task_ref_count),
			random_context->handle->scenario,
			random_context->handle->engineFlag);
	} else {
		/* sync flush case, will blocking worker */
		INIT_WORK(&random_context->release_work,
			_testcase_stress_submit_release_work);
		atomic_inc(task_ref_count);

		CMDQ_LOG(
			"Round:%u handle:0x%p pkt:0x%p size:%zu ref:%u scenario:%d flag:0x%llx start sync\n",
			task_count, handle, handle->pkt,
			handle->pkt->cmd_buf_size,
			(u32)atomic_read(task_ref_count),
			handle->scenario,
			random_context->handle->engineFlag);
	}

	queue_work(release_queues[get_random_int() % MAX_RELEASE_QUEUE],
		&random_context->release_work);

	msleep_interruptible(get_random_int() %
		(u32)thread_data->policy.loop_policy + 1);

	return 0;
}

static int _testcase_gen_task_thread(void *data)
{
	const u32 max_muti_task = CMDQ_MAX_TASK_IN_THREAD + 1;
	const unsigned long dummy_reg_va = CMDQ_TEST_GCE_DUMMY_VA;
	struct thread_param *thread_data = (struct thread_param *)data;
	struct workqueue_struct *release_queues[MAX_RELEASE_QUEUE] = {0};
	const bool ignore_timeout = _stress_is_ignore_timeout(
		&thread_data->policy);
	u32 task_count = 0;
	atomic_t task_ref_count;
	u32 wait_count = 0;
	u32 engine_sel = 0;
	u32 i = 0;
	u32 thd_timeout[CMDQ_MAX_THREAD_COUNT] = {0};
	u32 timeout_ms;

	CMDQ_LOG("%s\n", __func__);

	for (i = 0; i < MAX_RELEASE_QUEUE; i++)
		release_queues[i] = create_singlethread_workqueue(
			"cmdq_random_release");

	CMDQ_REG_SET32(dummy_reg_va, 0xdeaddead);
	if (CMDQ_REG_GET32(dummy_reg_va) != 0xdeaddead)
		CMDQ_ERR("%s verify pattern register fail:0x%08x\n",
			__func__, (u32)CMDQ_REG_GET32(dummy_reg_va));

	if (thread_data->policy.engines_policy == CMDQ_TESTCASE_ENGINE_SAME)
		engine_sel = get_random_int() %
		((1 << ARRAY_SIZE(stress_engines)) - 1);
	else
		engine_sel = 0;

	if (thread_data->policy.wait_policy ==
		CMDQ_TESTCASE_WAITOP_BEFORE_END) {
		timeout_ms = 2 * CMDQ_PREDUMP_TIMEOUT_MS;
	} else if (thread_data->policy.wait_policy ==
		CMDQ_TESTCASE_WAITOP_RANDOM_NOTIMEOUT)
		timeout_ms = 50 * CMDQ_PREDUMP_TIMEOUT_MS;
	else
		timeout_ms = 2 * CMDQ_PREDUMP_TIMEOUT_MS;

	/* setup thread timeout */
	for (i = 0; i < ARRAY_SIZE(thd_timeout); i++) {
		struct cmdq_client *clt = cmdq_helper_mbox_client(i);

		if (!clt)
			continue;

		thd_timeout[i] = cmdq_mbox_set_thread_timeout(
			clt->chan, timeout_ms);
	}

	atomic_set(&task_ref_count, 0);
	for (task_count = 0; !atomic_read(&thread_data->stop) &&
		task_count < thread_data->run_count; task_count++) {

		if (!thread_data->multi_task) {
			if (atomic_read(&task_ref_count) > 0) {
				msleep_interruptible(8);
				task_count--;
				continue;
			}
		} else {
			if (atomic_read(&task_ref_count) >= max_muti_task) {
				msleep_interruptible(8);
				task_count--;
				continue;
			}
		}

		if (_testcase_gen_task_thread_each(data, task_count,
			&task_ref_count, engine_sel, ignore_timeout,
			release_queues) < 0)
			break;
	}

	while (atomic_read(&task_ref_count) > 0) {
		/* set events to speed up task finish */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_1);

		msleep_interruptible(500);
		CMDQ_ERR("%s wait for all task done:%u\n",
			__func__, wait_count++);
	}

	CMDQ_LOG("%s END\n", __func__);

	for (i = 0; i < MAX_RELEASE_QUEUE; i++)
		destroy_workqueue(release_queues[i]);
	cmdq_core_reset_first_dump();

	/* rollback thread timeout */
	for (i = 0; i < ARRAY_SIZE(thd_timeout); i++) {
		struct cmdq_client *clt = cmdq_helper_mbox_client(i);

		if (!clt)
			continue;

		cmdq_mbox_set_thread_timeout(clt->chan, thd_timeout[i]);
	}

	complete(&thread_data->cmplt);

	return 0;
}

static int _testcase_trigger_event_thread(void *data)
{
	struct thread_param *thread_data = (struct thread_param *)data;
	u8 event_idx = 0;
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};
	const unsigned long dummy_poll_va = CMDQ_GPR_R32(
		CMDQ_GET_GPR_PX2RX_LOW(CMDQ_DATA_REG_2D_SHARPNESS_0_DST));
	u32 poll_bit_counter = 0xf;
	u32 dummy_value = 0;
	u32 trigger_interval = (u32)thread_data->policy.trigger_policy;

	CMDQ_LOG("%s\n", __func__);

	if (!trigger_interval)
		trigger_interval = get_random_int() %
		(u32)CMDQ_TESTCASE_TRIGGER_SLOW;
	if (trigger_interval <= 1)
		trigger_interval = 2;

	/* randomly clear/set event */
	while (!atomic_read(&thread_data->stop)) {
		event_idx = get_random_int() % ARRAY_SIZE(tokens);
		if (get_random_int() % 2)
			cmdqCoreSetEvent(tokens[event_idx]);
		else
			cmdqCoreClearEvent(tokens[event_idx]);

		dummy_value = CMDQ_REG_GET32(dummy_poll_va);
		CMDQ_REG_SET32(dummy_poll_va, dummy_value | poll_bit_counter);
		poll_bit_counter = poll_bit_counter << 4;
		if (poll_bit_counter > CMDQ_TEST_POLL_BIT)
			poll_bit_counter = 0xf;

		msleep_interruptible(get_random_int() % trigger_interval + 1);
	}

	CMDQ_LOG("%s END\n", __func__);
	complete(&thread_data->cmplt);
	return 0;
}

static int dummy_dump_smi(const int showSmiDump)
{
	return 0;
}

static void testcase_gen_random_case(bool multi_task,
	struct stress_policy policy)
{
	struct task_struct *random_thread_handle;
	struct task_struct *trigger_thread_handle;
	struct thread_param random_thread = { {0} };
	struct thread_param trigger_thread = { {0} };
	const u32 finish_timeout_ms = 1000;
	u32 timeout_counter = 0;
	s32 wait_status = 0;
	struct cmdqCoreFuncStruct *virtual_func = cmdq_get_func();
	CmdqDumpSMI dump_smi_func = virtual_func->dumpSMI;

	CMDQ_LOG(
		"%s start with multi-task: %s engine:%d wait:%d condition:%d loop:%d timeout: %s\n",
		__func__, multi_task ? "True" : "False", policy.engines_policy,
		policy.wait_policy, policy.condition_policy,
		policy.loop_policy,
		_stress_is_ignore_timeout(&policy) ? "ignore" : "aee");

	/* remove smi dump */
	virtual_func->dumpSMI = dummy_dump_smi;
	cmdq_core_set_aee(!_stress_is_ignore_timeout(&policy));

	random_thread.multi_task = multi_task;
	random_thread.policy = policy;

	do {
		init_completion(&random_thread.cmplt);
		atomic_set(&random_thread.stop, 0);

		init_completion(&trigger_thread.cmplt);
		atomic_set(&trigger_thread.stop, 0);

		random_thread.run_count = 3000;

		random_thread_handle = kthread_run(_testcase_gen_task_thread,
			(void *)&random_thread, "cmdq_gen");
		if (IS_ERR(random_thread_handle)) {
			CMDQ_TEST_FAIL("Fail to start gen task thread\n");
			break;
		}

		trigger_thread_handle = kthread_run(
			_testcase_trigger_event_thread,
			(void *)&trigger_thread, "mdq_trigger");
		if (IS_ERR(trigger_thread_handle)) {
			CMDQ_TEST_FAIL("Fail to start trigger event thread\n");
			atomic_set(&random_thread.stop, 1);
			wait_for_completion(&random_thread.cmplt);
			break;
		}

		wait_for_completion(&random_thread.cmplt);
		atomic_set(&trigger_thread.stop, 1);
		do {
			wait_status =
				wait_for_completion_interruptible_timeout(
				&trigger_thread.cmplt,
				msecs_to_jiffies(finish_timeout_ms));
			CMDQ_LOG(
				"wait trigger thread complete count:%u status:%d\n",
				timeout_counter, wait_status);
			msleep_interruptible(finish_timeout_ms);
			timeout_counter++;
		} while (wait_status <= 0);
	} while (0);

	/* restore smi dump */
	virtual_func->dumpSMI = dump_smi_func;
	cmdq_core_set_aee(true);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_stress_basic(void)
{
	struct stress_policy policy = {0};

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (gCmdqTestSecure)
		policy.secure_policy = CMDQ_TESTCASE_SECURE_RANDOM;
#endif

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_NONE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_FAST;
	policy.trigger_policy = CMDQ_TESTCASE_TRIGGER_SLOW;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_BEFORE_END;
	policy.trigger_policy = CMDQ_TESTCASE_TRIGGER_MEDIUM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_RANDOM;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_condition(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_RANDOM;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_FAST;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM_MORE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_poll(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	policy.poll_policy = CMDQ_TESTCASE_POLL_PASS;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.poll_policy = CMDQ_TESTCASE_POLL_ALL;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_timeout(void)
{
	struct stress_policy policy = {0};

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (gCmdqTestSecure)
		policy.secure_policy = CMDQ_TESTCASE_SECURE_RANDOM;
#endif

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_NONE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_reorder(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM_NOTIMEOUT;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_NONE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_FAST;
	policy.trigger_policy = CMDQ_TESTCASE_TRIGGER_SLOW;
	testcase_gen_random_case(true, policy);
}

#define TESTMBOX_CLT_IDX 14
#define TESTMBOX_CLT_IDX_LOOP 7

void testmbox_write(unsigned long dummy_va, unsigned long dummy_pa,
	u32 mask)
{
	const u32 pattern = (1 << 0) | (1 << 2) | (1 << 16);
	const u32 expect_result = pattern & mask;
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt;
	u32 value = 0;

	CMDQ_LOG("%s va:0x%lx pa:0x%lx mask:0x%08x\n",
		__func__, dummy_va, dummy_pa, mask);

	if (!mask)
		/* set to 0xFFFFFFFF */
		CMDQ_REG_SET32((void *)dummy_va, ~0);
	else
		CMDQ_REG_SET32((void *)dummy_va, 0);

	/* use CMDQ to set to PATTERN */
	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_write(pkt, clt_base, dummy_pa, pattern, mask);
	cmdq_pkt_flush(pkt);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != expect_result) {
		/* test fail */
		CMDQ_TEST_FAIL("wrote value is 0x%08x not 0x%08x\n",
			value, expect_result);
		cmdq_pkt_dump_buf(pkt, 0);
	}

	cmdq_pkt_destroy(pkt);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_write_gce(void)
{
	unsigned long dummy_va, dummy_pa;

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	testmbox_write(dummy_va, dummy_pa, ~0);
}

void testmbox_write_with_mask(void)
{
	unsigned long dummy_va, dummy_pa;
	const u32 mask = (1 << 16);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	testmbox_write(dummy_va, dummy_pa, mask);
}

void testmbox_loop(void)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(
		TESTMBOX_CLT_IDX_LOOP);
	struct cmdq_pkt *pkt;
	struct cmdq_thread *thread = (struct cmdq_thread *)clt->chan->con_priv;
	s32 err;

	CMDQ_LOG("%s\n", __func__);

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_pkt_finalize_loop(pkt);

	cmdq_dump_pkt(pkt, 0, true);

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	/* should success */
	CMDQ_LOG("%s start loop thread:%d pkt:0x%p\n",
		__func__, thread->idx, pkt);
	err = cmdq_pkt_flush_async(pkt, NULL, 0);

	/* WAIT */
	while (g_loopIter < 20)
		msleep_interruptible(2000);

	CMDQ_LOG("%s stop timer\n", __func__);
	cmdq_mbox_stop(clt);
	del_timer(&g_loopTimer);
	cmdq_pkt_destroy(pkt);

	CMDQ_LOG("%s end\n", __func__);
}

void testmbox_dma_access(void)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt, *pkt2;
	const u32 pattern = 0xabcdabcd;
	const u32 pattern2 = 0xaabbccdd;
	const u32 pat_default = 0xdeaddead;
	const u32 pattern3 = 0xdeadbeef;
	unsigned long dummy_va, dummy_pa;
	dma_addr_t slot = 0, slot2;
	u32 *va, value;
	u32 mem_off = 0xabc;

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(dummy_va, pattern);

	va = (u32 *)cmdq_mbox_buf_alloc(clt->client.dev, &slot);
	if (!va || !slot) {
		CMDQ_TEST_FAIL("%s alloc buffer fail\n", __func__);
		return;
	}

	/* write buffer with default value */
	va[0] = pat_default;
	va[1] = pat_default;
	va[2] = pattern2;

	/* use CMDQ to set to PATTERN */
	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_mem_move(pkt, clt_base, dummy_pa, slot, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, clt_base, slot, slot + 4, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, clt_base, slot + 8, dummy_pa, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_dump_buf(pkt, 0);
	cmdq_pkt_destroy(pkt);

	if (va[0] != pattern)
		CMDQ_TEST_FAIL(
			"%s move pa to dram value:0x%08x pattern:0x%08x default:0x%08x\n",
			__func__, va[0], pattern, pat_default);
	if (va[1] != pattern)
		CMDQ_TEST_FAIL(
			"%s move dram to dram value:0x%08x pattern:0x%08x default:0x%08x\n",
			__func__, va[1], pattern, pat_default);
	value = CMDQ_REG_GET32(dummy_va);
	if (va[2] != value)
		CMDQ_TEST_FAIL(
			"%s move pa to dram dummy:0x%08x pattern:0x%08x default:0x%08x\n",
			__func__, value, pattern2, pat_default);

	/* write pattern and read */
	pkt2 = cmdq_pkt_create(clt);
	cmdq_pkt_jump(pkt2, 8);

	va[mem_off / 4] = 0;
	va[mem_off / 4 + 1] = 0;
	slot2 = slot + mem_off;

	cmdq_pkt_write_value_addr(pkt2, slot2, 0xdeadbeef, ~0);
	cmdq_pkt_read_addr(pkt2, slot2, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_write_reg_addr(pkt2, slot2 + 4, CMDQ_THR_SPR_IDX1, ~0);
	cmdq_pkt_flush(pkt2);
	cmdq_pkt_dump_buf(pkt2, 0);
	cmdq_pkt_destroy(pkt2);

	if (va[mem_off / 4] != va[mem_off / 4 + 1] ||
		va[mem_off / 4 + 1] != pattern3) {
		CMDQ_TEST_FAIL(
			"pa:%pa va:0x%p pattern:0x%08x data:0x%08x 0x%08x\n",
			&slot2, &va[mem_off / 4], pattern3,
			va[mem_off / 4], va[mem_off / 4 + 1]);
	}

	cmdq_mbox_buf_free(clt->client.dev, va, slot);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_cmplt_cb_destroy(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	if (data.err < 0)
		CMDQ_TEST_FAIL("pkt:0x%p err:%d\n",
			cmplt->pkt, data.err);

	cmplt->err = !data.err ? false : true;
	cmdq_pkt_destroy(cmplt->pkt);
	complete(&cmplt->cmplt);
}

void testmbox_cmplt_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	if (data.err < 0)
		CMDQ_TEST_FAIL("pkt:0x%p err:%d\n",
			cmplt->pkt, data.err);

	cmplt->err = !data.err ? false : true;
	complete(&cmplt->cmplt);
}

void testmbox_async_flush(bool threaded)
{
#define TEST_REQ_COUNT 24
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_pkt *pkt[TEST_REQ_COUNT] = { 0 };
	struct cmdq_flush_completion *cmplt;
	u32 i, ret;

	CMDQ_LOG("%s threaded:%s\n", __func__, threaded ? "true" : "false");

	cmplt = kcalloc(TEST_REQ_COUNT, sizeof(*cmplt), GFP_KERNEL);

	test_timer_stop = false;
	setup_timer(&test_timer, &_testcase_sync_token_timer_loop_func,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&test_timer, jiffies + msecs_to_jiffies(10));

	/* Queue multiple async request */
	/* to test dynamic task allocation */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < TEST_REQ_COUNT; i++) {
		pkt[i] = cmdq_pkt_create(clt);
		cmdq_pkt_wfe(pkt[i], CMDQ_SYNC_TOKEN_USER_0);

		/* higher priority for later tasks */
		pkt[i]->priority = i;

		init_completion(&cmplt[i].cmplt);
		cmplt[i].pkt = pkt[i];

		if (threaded)
			cmdq_pkt_flush_threaded(pkt[i],
				testmbox_cmplt_cb_destroy, &cmplt[i]);
		else
			cmdq_pkt_flush_async(pkt[i],
				testmbox_cmplt_cb, &cmplt[i]);
	}

	/* release token and wait them */
	for (i = 0; i < TEST_REQ_COUNT; i++) {
		if (!pkt[i]) {
			CMDQ_ERR("%s pkt[%d] is NULL\n", __func__, i);
			continue;
		}

		msleep_interruptible(100);

		CMDQ_MSG("wait pkt[%2d] 0x%p\n", i, pkt[i]);
		ret = wait_for_completion_timeout(&cmplt[i].cmplt,
			msecs_to_jiffies(CMDQ_TIMEOUT_DEFAULT));
		if (!ret) {
			CMDQ_TEST_FAIL("wait pkt[%2d] 0x%p timeout\n",
				i, pkt[i]);
			continue;
		}
		if (!threaded)
			cmdq_pkt_destroy(pkt[i]);
	}

	/* clear token */
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	test_timer_stop = true;
	del_timer(&test_timer);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_large_command(void)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt;
	u32 data, i;

	CMDQ_LOG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	pkt = cmdq_pkt_create(clt);
	/* build a 64K instruction buffer */
	for (i = 0; i < 64 * 1024 / 8; i++)
		cmdq_pkt_write(pkt, clt_base, CMDQ_TEST_GCE_DUMMY_PA, i, ~0);
	CMDQ_LOG("pkt:0x%p buf size:%zu size:%zu avail:%zu\n",
		pkt, pkt->cmd_buf_size, pkt->buf_size,
		pkt->avail_buf_size);
	cmdq_pkt_flush(pkt);

	/* verify data */
	data = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (i - 1 != data)
		CMDQ_TEST_FAIL(
			"reg value is 0x%08x not idx 0x%08x\n", data, i);

	cmdq_pkt_destroy(pkt);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_poll_run(u32 poll_value, u32 poll_mask,
	bool use_mmsys_dummy)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt;
	struct cmdq_flush_completion cmplt = {0};
	u32 value = 0, dst_reg_pa;
	unsigned long dummy_va;

	if (gCmdqTestSecure || use_mmsys_dummy) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dst_reg_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dst_reg_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	CMDQ_REG_SET32(dummy_va, 0);

	CMDQ_LOG("%s poll value:0x%08x poll mask:0x%08x use mmsys:%s\n",
		__func__, poll_value, poll_mask,
		use_mmsys_dummy ? "true" : "false");

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);
	cmdq_pkt_poll(pkt, clt_base, poll_value, dst_reg_pa, poll_mask,
		CMDQ_DATA_REG_DEBUG);
	cmdq_pkt_set_event(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);
	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	cmdq_pkt_flush_async(pkt, testmbox_cmplt_cb, &cmplt);
	cmdq_pkt_dump_buf(pkt, 0);

	/* Set MMSYS dummy register value after clock is on */
	CMDQ_REG_SET32(dummy_va, poll_value);
	value = CMDQ_REG_GET32(dummy_va);
	CMDQ_LOG("target value is 0x%08x\n", value);

	wait_for_completion(&cmplt.cmplt);
	cmdq_pkt_destroy(pkt);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_poll(void)
{
	testmbox_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, false);
	testmbox_poll_run(0xdada1818, 0xFFFFFFFF, false);
	testmbox_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, false);
	testmbox_poll_run(0x00001818, 0xFFFFFFFF, false);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* fpga may not ready mmsys */
	testmbox_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, true);
	testmbox_poll_run(0xdada1818, 0xFFFFFFFF, true);
	testmbox_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, true);
	testmbox_poll_run(0x00001818, 0xFFFFFFFF, true);
#endif
}

void testmbox_dump_err(void)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_pkt *pkt;
	struct cmdq_flush_completion cmplt = {0};
	u32 ret;
	u64 *inst;
	dma_addr_t pc = 0;

	CMDQ_LOG("%s\n", __func__);

	cmdq_clear_event(clt->chan, CMDQ_SYNC_TOKEN_USER_0);

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_USER_0);
	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	cmdq_pkt_flush_async(pkt, testmbox_cmplt_cb, &cmplt);

	/* try dump */
	cmdq_thread_dump(clt->chan, pkt, &inst, &pc);

	/* set event and complete pkt */
	cmdq_set_event(clt->chan, CMDQ_SYNC_TOKEN_USER_0);
	ret = wait_for_completion_timeout(&cmplt.cmplt,
		msecs_to_jiffies(CMDQ_TIMEOUT_DEFAULT));
	if (!ret)
		CMDQ_TEST_FAIL("wait pkt 0x%p timeout inst:%llx pc:%pa\n",
			pkt, inst ? *inst : 0, &pc);
	else
		cmdq_pkt_destroy(pkt);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_poll_timeout_run(u32 poll_value, u32 poll_mask,
	bool use_mmsys_dummy, bool timeout)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;
	struct cmdq_flush_completion cmplt = {0};
	u32 value = 0, dst_reg_pa, cost;
	unsigned long dummy_va;
	dma_addr_t out_pa;
	u32 *out_va;
	u64 cpu_cost;

	if (gCmdqTestSecure || use_mmsys_dummy) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dst_reg_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dst_reg_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0x80000000);
	CMDQ_REG_SET32(dummy_va, 0);

	CMDQ_LOG("%s poll value:0x%08x poll mask:0x%08x use mmsys:%s\n",
		__func__, poll_value, poll_mask,
		use_mmsys_dummy ? "true" : "false");

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	/* use last 1024 as output buffer */
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	*out_va = 0;
	*(out_va + 1) = 0;

	cmdq_pkt_write_indriect(pkt, clt_base, out_pa, CMDQ_CPR_STRAT_ID, ~0);
	cmdq_pkt_poll_timeout(pkt, poll_value, SUBSYS_NO_SUPPORT, dst_reg_pa,
		poll_mask, 100, CMDQ_DATA_REG_DEBUG);
	cmdq_pkt_write_indriect(pkt, clt_base, out_pa + 4,
		CMDQ_CPR_STRAT_ID, ~0);
	cmdq_pkt_set_event(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);
	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;

	cpu_cost = sched_clock();
	cmdq_pkt_flush_async(pkt, testmbox_cmplt_cb, &cmplt);

	if (!timeout) {
		/* Set dummy register value after clock is on */
		CMDQ_REG_SET32(dummy_va, poll_value);
		value = CMDQ_REG_GET32(dummy_va);
	}

	wait_for_completion(&cmplt.cmplt);
	cpu_cost = div_u64(sched_clock() - cpu_cost, 1000000);

	cmdq_pkt_dump_buf(pkt, 0);

	/* calculate cost */
	if (*out_va <= *(out_va + 1))
		cost = *(out_va + 1) - *out_va;
	else
		cost = (0xffffffff - *out_va) + *(out_va + 1);

	/* wait 100 count and each time 5 tick, add 100 for buffer */
	if (cost > 600 || cpu_cost > 10)
		CMDQ_TEST_FAIL(
			"target value is 0x%08x cost timed out:%d to 600 cpu:%llu to 10ms\n",
			value, cost, cpu_cost);
	else
		CMDQ_LOG("target value is 0x%08x cost time:%d cpu:%llu\n",
			value, cost, cpu_cost);
	cmdq_pkt_destroy(pkt);

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_poll_timeout(void)
{
	testmbox_poll_timeout_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00,
		false, false);
	testmbox_poll_timeout_run(0xdada1818, 0xFFFFFFFF, false, false);
	testmbox_poll_timeout_run(0xdada1818 & 0x0000FF00, 0x0000FF00,
		false, false);
	testmbox_poll_timeout_run(0x00001818, 0xFFFFFFFF, false, false);
	testmbox_poll_timeout_run(0x1, 0xFFFFFFFF, false, true);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* fpga may not ready mmsys */
	testmbox_poll_timeout_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, true,
		false);
	testmbox_poll_timeout_run(0xdada1818, 0xFFFFFFFF, true, false);
	testmbox_poll_timeout_run(0xdada1818 & 0x0000FF00, 0x0000FF00, true,
		false);
	testmbox_poll_timeout_run(0x00001818, 0xFFFFFFFF, true, false);
#endif
}

void testmbox_gpr_timer(void)
{
#define CMDQ_TPR_TIMEOUT_EN		0xDC
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_thread *thread = (struct cmdq_thread *)clt->chan->con_priv;
	const u32 timeout_en = thread->gce_pa + CMDQ_TPR_TIMEOUT_EN;
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;
	const u16 reg_gpr = CMDQ_DATA_REG_DEBUG;
	const u32 tpr_en = 1 << reg_gpr;
	const u16 event = (u16)CMDQ_EVENT_GPR_TIMER + reg_gpr;
	struct cmdq_operand lop = {.reg = true, .idx = CMDQ_TPR_ID};
	struct cmdq_operand rop = {.reg = false, .value = 100};
	u32 cost;
	dma_addr_t out_pa;
	u32 *out_va;
	u64 cpu_cost;

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0x80000000);
	CMDQ_REG_SET32(CMDQ_TPR_GPR_TIMER, tpr_en);

	CMDQ_LOG("%s GCE PA:%pa\n", __func__, &thread->gce_pa);

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_write(pkt, clt_base, timeout_en, tpr_en, tpr_en);
	cmdq_pkt_clear_event(pkt, event);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	CMDQ_LOG("pkt:0x%p buf:0x%p\n", pkt, buf);
	if (!buf) {
		CMDQ_ERR("%s no buf in pkt\n", __func__);
		return;
	}

	/* use last 1024 as output buffer */
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	*out_va = 0;
	*(out_va + 1) = 0;

	CMDQ_LOG("use gce write tpr to:0x%p(%pa) and 0x%p\n",
		out_va, &out_pa, out_va + 1);

	cmdq_pkt_write_indriect(pkt, clt_base, out_pa, CMDQ_TPR_ID, ~0);
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, CMDQ_GPR_CNT_ID + reg_gpr,
		&lop, &rop);
	cmdq_pkt_wfe(pkt, event);
	cmdq_pkt_write_indriect(pkt, clt_base, out_pa + 4, CMDQ_TPR_ID, ~0);

	cpu_cost = sched_clock();
	cmdq_pkt_flush(pkt);
	cpu_cost = div_u64(sched_clock() - cpu_cost, 1000000);

	cmdq_pkt_dump_buf(pkt, 0);

	/* calculate cost */
	if (*out_va <= *(out_va + 1))
		cost = *(out_va + 1) - *out_va;
	else
		cost = (0xffffffff - *out_va) + *(out_va + 1);

	/* wait 100 count and each time 5 tick, add 100 for buffer */
	if (cost > 600 || cpu_cost > 10000)
		CMDQ_TEST_FAIL(
			"%s sleep cost timedout:%u (%u %u) to 600 cpu:%lluus to 10ms\n",
			__func__, cost, *out_va, *(out_va + 1), cpu_cost);
	else
		CMDQ_LOG("%s sleep cost time:%u (%u %u) cpu:%llu\n",
			__func__, cost, *out_va, *(out_va + 1), cpu_cost);
	cmdq_pkt_destroy(pkt);

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0);

	CMDQ_LOG("%s END\n", __func__);
}

void testmbox_sleep(void)
{
	struct cmdq_client *clt = cmdq_helper_mbox_client(TESTMBOX_CLT_IDX);
	struct cmdq_base *clt_base = cmdq_helper_mbox_base();
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;
	u32 cost;
	dma_addr_t out_pa;
	u32 *out_va;
	u64 cpu_cost;

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0x80000000);
	CMDQ_REG_SET32(CMDQ_TPR_GPR_TIMER,
		1 << CMDQ_DATA_REG_DEBUG);

	CMDQ_LOG("%s\n", __func__);

	pkt = cmdq_pkt_create(clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);

	CMDQ_LOG("pkt:0x%p buf:0x%p\n", pkt, buf);
	if (!buf) {
		CMDQ_ERR("%s no buf in pkt\n", __func__);
		return;
	}

	/* use last 1024 as output buffer */
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	CMDQ_LOG("use gce write tpr to:0x%p(%pa) and 0x%p\n",
		out_va, &out_pa, out_va + 1);
	*out_va = 0;
	*(out_va + 1) = 0;

	cmdq_pkt_write_indriect(pkt, clt_base, out_pa, CMDQ_TPR_ID, ~0);
	cmdq_pkt_sleep(pkt, 100, CMDQ_DATA_REG_DEBUG);
	cmdq_pkt_write_indriect(pkt, clt_base, out_pa + 4, CMDQ_TPR_ID, ~0);
	cmdq_pkt_set_event(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);

	cmdq_pkt_write_indriect(pkt, clt_base, out_pa + 8,
		CMDQ_GPR_CNT_ID + CMDQ_DATA_REG_DEBUG, ~0);

	cpu_cost = sched_clock();
	cmdq_pkt_flush(pkt);
	cpu_cost = div_u64(sched_clock() - cpu_cost, 1000000);

	cmdq_pkt_dump_buf(pkt, 0);

	/* calculate cost */
	if (*out_va <= *(out_va + 1))
		cost = *(out_va + 1) - *out_va;
	else
		cost = (0xffffffff - *out_va) + *(out_va + 1);

	/* wait 100 count and each time 5 tick, add 100 for buffer */
	if (cost > 600 || cpu_cost > 10000)
		CMDQ_TEST_FAIL(
			"sleep cost timedout:%u (%u %u %u) to 600 cpu:%lluus to 10ms\n",
			cost, *out_va, *(out_va + 1), *(out_va + 2), cpu_cost);
	else
		CMDQ_LOG("sleep cost time:%u (%u %u %u) cpu:%llu\n",
			cost, *out_va, *(out_va + 1), *(out_va + 2), cpu_cost);
	cmdq_pkt_destroy(pkt);

	CMDQ_REG_SET32(CMDQ_TPR_MASK, 0);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_sec_dapc_protect(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	void *wrot_va = (void *)cmdq_mdp_get_module_base_VA_MDP_WROT0() + 0xF00;
	const u32 pattern = 0xdeadbeef;
	u32 val;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	handle->engineFlag = 0x1LL << CMDQ_ENG_MDP_WROT0;

	cmdq_task_secure_enable_dapc(handle, 1LL << CMDQ_ENG_MDP_WROT0);
	cmdq_task_secure_enable_port_security(handle,
		1LL << CMDQ_ENG_MDP_WROT0);

	cmdq_task_set_secure(handle, true);
	cmdq_op_clear_event(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush_async(handle);

	CMDQ_REG_SET32(wrot_va, pattern);
	val = CMDQ_REG_GET32(wrot_va);
	if (val == pattern)
		CMDQ_TEST_FAIL("dapc protect fail addr:0x%p\n", wrot_va);

	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
#endif
}

enum CMDQ_TESTCASE_ENUM {
	CMDQ_TESTCASE_DEFAULT = 0,
	CMDQ_TESTCASE_BASIC = 1,
	CMDQ_TESTCASE_ERROR = 2,
	CMDQ_TESTCASE_FPGA = 3,
	/* user request get some registers' value when task execution */
	CMDQ_TESTCASE_READ_REG_REQUEST,
	CMDQ_TESTCASE_GPR,
	CMDQ_TESTCASE_SW_TIMEOUT_HANDLE,

	CMDQ_TESTCASE_END,	/* always at the end */
};

static void testcase_general_handling(s32 testID)
{
	u32 i;

	switch (testID) {
	case 510:
		testmbox_gpr_timer();
		testmbox_sleep();
		break;
	case 509:
		testmbox_poll_timeout();
		break;
	case 508:
		testmbox_dump_err();
		break;
	case 506:
		testmbox_poll();
		break;
	case 505:
		testmbox_large_command();
		break;
	case 504:
		testmbox_async_flush(false);
		testmbox_async_flush(true);
		break;
	case 503:
		testmbox_dma_access();
		break;
	case 502:
		testmbox_loop();
		break;
	case 501:
		testmbox_write_gce();
		testmbox_write_with_mask();
		break;
	case 500:
		testmbox_write_gce();
		testmbox_write_with_mask();
		testmbox_loop();
		testmbox_dma_access();
		testmbox_async_flush(false);
		testmbox_async_flush(true);
		testmbox_large_command();
		testmbox_poll();
		break;
	case 304:
		testcase_stress_reorder();
		break;
	case 303:
		testcase_stress_timeout();
		break;
	case 302:
		testcase_stress_poll();
		break;
	case 301:
		testcase_stress_condition();
		break;
	case 300:
		testcase_stress_basic();
		break;
	case 165:
		testcase_timeout_error();
		break;
	case 163:
		testcase_cmdq_trigger_devapc();
		break;
	case 161:
		testcase_sec_dapc_protect();
		break;
	case 160:
		testcase_error_irq_for_secure();
		break;
	case 159:
		testcase_cross_4k_buffer();
		break;
	case 157:
		testcase_remove_by_file_node();
		break;
	case 156:
		testcase_verify_timer();
		break;
	case 154:
		testcase_end_behavior(true, 0);
		testcase_end_behavior(false, 0);
		for (i = 0; i < 500; i++)
			testcase_end_behavior(false,
				get_random_int() % 50 + 1);
		break;
	case 152:
		testcase_end_addr_conflict();
		break;
	case 151:
		testcase_engineflag_conflict_dump();
		break;
	case 148:
		testcase_read_with_mask();
		break;
	case 147:
		testcase_efficient_polling();
		break;
	case 141:
		testcase_track_task_cb();
		break;
	case 139:
		testcase_invalid_handle();
		break;
	case 129:
		testcase_boundary_mem();
		break;
	case 128:
		testcase_boundary_mem_param();
		break;
	case 120:
		testcase_notify_and_delay_submit(16);
		break;
	case 119:
		testcase_check_dts_correctness();
		break;
	case 118:
		testcase_error_irq();
		break;
	case 117:
		testcase_timeout_reorder_test();
		break;
	case 116:
		testcase_timeout_wait_early_test();
		break;
	case 115:
		testcase_manual_suspend_resume_test();
		break;
	case 114:
		testcase_append_task_verify();
		break;
	case 113:
		testcase_trigger_engine_dispatch_check();
		break;
	case 112:
		testcase_complicated_engine_thread();
		break;
	case 111:
		testcase_module_full_mdp_engine();
		break;
	case 110:
		testcase_nonsuspend_irq();
		break;
	case 106:
		testcase_concurrency_for_normal_path_and_secure_path();
		break;
	case 104:
		testcase_submit_after_error_happened();
		break;
	case 101:
		testcase_write_stress_test();
		break;
	case 100:
		testcase_secure_basic();
		break;
	case 99:
		testcase_write();
		testcase_write_with_mask();
		break;
	case 98:
		testcase_errors();
		break;
	case 97:
		testcase_scenario();
		break;
	case 96:
		testcase_sync_token();
		break;
	case 95:
		testcase_write_address();
		break;
	case 94:
		testcase_async_request();
		break;
	case 92:
		testcase_async_request_partial_engine();
		break;
	case 91:
		testcase_prefetch_scenarios();
		break;
	case 90:
		testcase_loop();
		break;
	case 88:
		testcase_multiple_async_request();
		break;
	case 87:
		testcase_get_result();
		break;
	case 86:
		testcase_read_to_data_reg();
		break;
	case 85:
		testcase_dram_access();
		break;
	case 84:
		testcase_backup_register();
		break;
	case 83:
		testcase_fire_and_forget();
		break;
	case 82:
		testcase_sync_token_threaded();
		break;
	case 81:
		testcase_long_command();
		break;
	case 79:
		testcase_perisys_apb();
		break;
	case 78:
		testcase_backup_reg_to_slot();
		break;
	case 77:
		testcase_thread_dispatch();
		break;
	case 75:
		testcase_full_thread_array();
		break;
	case 74:
		testcase_module_full_dump();
		break;
	case 73:
		testcase_write_from_data_reg();
		break;
	case 72:
		testcase_update_value_to_slot();
		break;
	case 71:
		testcase_poll();
		break;
	case 70:
		testcase_write_reg_from_slot();
		break;
	case CMDQ_TESTCASE_FPGA:
		CMDQ_LOG("FPGA Verify Start!\n");
		testcase_write();
		testcase_write_with_mask();
		testcase_poll();
		testcase_scenario();
		testcase_write_stress_test();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_multiple_async_request();
		testcase_get_result();
		testcase_dram_access();
		testcase_backup_register();
		testcase_fire_and_forget();
		testcase_long_command();
		testcase_backup_reg_to_slot();
		testcase_thread_dispatch();
		testcase_write_from_data_reg();
		testcase_update_value_to_slot();
		CMDQ_LOG("FPGA Verify Done!\n");
		break;
	case CMDQ_TESTCASE_ERROR:
		testcase_errors();
		testcase_async_request();
		testcase_module_full_dump();
		break;
	case CMDQ_TESTCASE_BASIC:
		testcase_write();
		testcase_write_with_mask();
		testcase_poll();
		testcase_scenario();
		break;
	case CMDQ_TESTCASE_READ_REG_REQUEST:
		testcase_get_result();
		break;
	case CMDQ_TESTCASE_GPR:
		testcase_read_to_data_reg();	/* must verify! */
		testcase_dram_access();
		break;
	case CMDQ_TESTCASE_DEFAULT:
		testcase_multiple_async_request();
		testcase_read_to_data_reg();
		testcase_get_result();
		testcase_scenario();
		testcase_write();
		testcase_poll();
		testcase_write_address();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_prefetch();
		testcase_long_command();
		testcase_dram_access();
		testcase_backup_register();
		testcase_fire_and_forget();
		testcase_backup_reg_to_slot();
		testcase_thread_dispatch();
		testcase_full_thread_array();
		break;
	default:
		CMDQ_LOG(
			"[TESTCASE]CONFIG Not Found: gCmdqTestSecure:%d testType:%lld\n",
			 gCmdqTestSecure, gCmdqTestConfig[0]);
		break;
	}
}

ssize_t cmdq_test_proc(struct file *fp, char __user *u, size_t s, loff_t *l)
{
	s64 testParameter[CMDQ_TESTCASE_PARAMETER_MAX];

	mutex_lock(&gCmdqTestProcLock);
	/* make sure the following section is protected */
	smp_mb();

	CMDQ_LOG("[TESTCASE]CONFIG: gCmdqTestSecure:%d testType:%lld\n",
		 gCmdqTestSecure, gCmdqTestConfig[0]);
	CMDQ_LOG("[TESTCASE]CONFIG PARAMETER: [1]:%lld [2]:%lld [3]:%lld\n",
		 gCmdqTestConfig[1], gCmdqTestConfig[2], gCmdqTestConfig[3]);
	memcpy(testParameter, gCmdqTestConfig, sizeof(testParameter));
	gCmdqTestConfig[0] = 0LL;
	gCmdqTestConfig[1] = -1LL;
	mutex_unlock(&gCmdqTestProcLock);

	/* trigger test case here */
	CMDQ_MSG("//\n//\n//\ncmdq_test_proc\n");

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* Turn on GCE clock to make sure GPR is always alive */
	cmdq_dev_enable_gce_clock(true);
	cmdq_mdp_get_func()->mdpEnableCommonClock(true);
#else
	cmdq_core_reset_gce();
#endif

	cmdq_get_func()->testSetup();

	switch (testParameter[0]) {
	case CMDQ_TEST_TYPE_NORMAL:
	case CMDQ_TEST_TYPE_SECURE:
		testcase_general_handling((s32)testParameter[1]);
		break;
#if 0
	case CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP:
		/* (scenario, buffersize) */
		testcase_open_buffer_dump((s32)testParameter[1],
			(s32)testParameter[2]);
		break;
#endif
	case CMDQ_TEST_TYPE_MMSYS_PERFORMANCE:
		testcase_mmsys_performance((s32)testParameter[1]);
		break;
	default:
		break;
	}

	cmdq_get_func()->testCleanup();

#ifndef CONFIG_FPGA_EARLY_PORTING
	cmdq_mdp_get_func()->mdpEnableCommonClock(false);
	/* Turn off GCE clock */
	cmdq_dev_enable_gce_clock(false);
#endif

	CMDQ_MSG("%s ended\n", __func__);
	return 0;
}

static ssize_t cmdq_write_test_proc_config(struct file *file,
	const char __user *userBuf, size_t count, loff_t *data)
{
	char desc[50];
	long long int testConfig[CMDQ_TESTCASE_PARAMETER_MAX];
	u64 len = 0ULL;

	do {
		/* copy user input */
		len = (count < (sizeof(desc) - 1)) ?
			count : (sizeof(desc) - 1);
		if (copy_from_user(desc, userBuf, len)) {
			CMDQ_MSG("TEST_CONFIG: data fail, length:%d\n", len);
			break;
		}
		desc[len] = '\0';

		/* Set initial test config value */
		memset(testConfig, -1, sizeof(testConfig));

		/* process and update config */
		if (sscanf(desc, "%lld %lld %lld %lld", &testConfig[0],
			&testConfig[1],
			&testConfig[2], &testConfig[3]) <= 0) {
			/* sscanf returns the number of items in argument
			 * list successfully filled.
			 */
			CMDQ_MSG("TEST_CONFIG: sscanf failed, len:%d\n", len);
			break;
		}
		CMDQ_MSG("TEST_CONFIG:%lld, %lld, %lld, %lld\n",
			testConfig[0], testConfig[1], testConfig[2],
			testConfig[3]);

		if ((testConfig[0] < 0) || (testConfig[0] >=
			CMDQ_TEST_TYPE_MAX)) {
			CMDQ_MSG(
				"TEST_CONFIG: testType:%lld, newTestSuit:%lld\n",
				testConfig[0], testConfig[1]);
			break;
		}
		mutex_lock(&gCmdqTestProcLock);
		/* set memory barrier for lock */
		smp_mb();

		memcpy(&gCmdqTestConfig, &testConfig, sizeof(testConfig));
		if (testConfig[0] == CMDQ_TEST_TYPE_NORMAL)
			gCmdqTestSecure = false;
		else
			gCmdqTestSecure = true;

		mutex_unlock(&gCmdqTestProcLock);
	} while (0);

	return count;
}

void cmdq_test_init_setting(void)
{
}

static int cmdq_test_open(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static const struct file_operations cmdq_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_test_open,
	.read = cmdq_test_proc,
	.write = cmdq_write_test_proc_config,
};

static int __init cmdq_test_init(void)
{
#ifdef CMDQ_TEST_PROC
	CMDQ_MSG("%s\n", __func__);
	/* Initial value */
	gCmdqTestSecure = false;
	gCmdqTestConfig[0] = 0LL;
	gCmdqTestConfig[1] = -1LL;
	/* Mout proc entry for debug */
	gCmdqTestProcEntry = proc_mkdir("cmdq_test", NULL);
	if (gCmdqTestProcEntry != NULL) {
		if (proc_create("test", 0660, gCmdqTestProcEntry,
			&cmdq_fops) == NULL) {
			/* cmdq_test_init failed */
			CMDQ_ERR("%s failed\n", __func__);
		}
	}
#endif
	return 0;
}

static void __exit cmdq_test_exit(void)
{
#ifdef CMDQ_TEST_PROC
	CMDQ_MSG("%s\n", __func__);
	if (gCmdqTestProcEntry != NULL) {
		proc_remove(gCmdqTestProcEntry);
		gCmdqTestProcEntry = NULL;
	}
#endif
}
module_init(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_LICENSE("GPL");
#endif				/* CMDQ_TEST */
