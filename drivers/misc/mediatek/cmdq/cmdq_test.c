#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <mach/mt_clkmgr.h>
#include <mach/memory.h>

#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"
#include "cmdq_mdp.h"


#define CMDQ_TEST

#ifdef CMDQ_TEST

/* test configuration */
static DEFINE_MUTEX(gCmdqTestProcLock);
static int32_t gCmdqTestConfig[2] = { 0, 0 };	/* {test case, type(normal, secure)} */
static bool gCmdqTestSecure = false;

extern unsigned long msleep_interruptible(unsigned int msecs);

static struct proc_dir_entry *gCmdqTestProcEntry;

extern int32_t cmdq_core_suspend_HW_thread(int32_t thread);

extern int32_t cmdq_append_command(cmdqRecHandle handle, CMDQ_CODE_ENUM code, uint32_t argA,
				   uint32_t argB);
extern int32_t cmdq_rec_finalize_command(cmdqRecHandle handle, bool loop);

extern int32_t cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(
			cmdqCommandStruct *pDesc, cmdqRecHandle handle);

extern int32_t cmdq_rec_setup_profile_marker_data(cmdqCommandStruct *pDesc, cmdqRecHandle handle);

static int32_t _test_submit_async(cmdqRecHandle handle, TaskStruct **ppTask)
{
	cmdqCommandStruct desc = {
		.scenario = handle->scenario,
		.priority = handle->priority,
		.engineFlag = handle->engineFlag,
		.pVABase = (cmdqU32Ptr_t)(unsigned long)handle->pBuffer,
		.blockSize = handle->blockSize,
	};

	/* secure path */
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTaskAsync(&desc, NULL, 0, ppTask);
}

static void testcase_scenario(void)
{
	cmdqRecHandle hRec;
	int32_t ret;
	int i = 0;

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	for (i = 0; i < CMDQ_MAX_SCENARIO_COUNT; ++i) {
		if (cmdq_core_is_request_from_user_space(i)) {
			continue;
		}

		CMDQ_MSG("testcase_scenario id:%d\n", i);
		cmdqRecCreate((CMDQ_SCENARIO_ENUM) i, &hRec);
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, false);
		ret = cmdqRecFlush(hRec);
		cmdqRecDestroy(hRec);
	}

	CMDQ_MSG("%s END\n", __func__);

	return;
}

static struct timer_list timer;

static void _testcase_sync_token_timer_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
}

static void _testcase_sync_token_timer_loop_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);

	/* repeate timeout until user delete it */
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));
}

static void testcase_sync_token(void)
{
	cmdqRecHandle hRec;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	do {
		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hRec);
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, gCmdqTestSecure);

		/* setup timer to trigger sync token */
		setup_timer(&timer, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
		mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

		/* wait for sync token */
		cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdqRecFlush(hRec);
		cmdqRecDestroy(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
		del_timer(&timer);
	} while (0);

	CMDQ_MSG("%s, timeout case\n", __func__);
	/*  */
	/* test for timeout */
	/*  */
	do {
		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hRec);
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, gCmdqTestSecure);

		/* wait for sync token */
		cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdqRecFlush(hRec);
		cmdqRecDestroy(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

		BUG_ON(ret >= 0);
	} while (0);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_suspend_resume(void)
{
	cmdqRecHandle hReqA;
	TaskStruct *pTaskA;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	/* setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0); */
	/* mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(300)); */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	do {
		/* let this thread wait for user token, then finish */
		cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_ALL, &hReqA);
		cmdqRecReset(hReqA);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8);

		ret = _test_submit_async(hReqA, &pTaskA);

		CMDQ_MSG("%s pTask %p, engine:0x%llx, scenario:%d\n",
			__func__, pTaskA, pTaskA->engineFlag, pTaskA->scenario);
		CMDQ_MSG("%s start suspend+resume thread 0========\n", __func__);
		cmdq_core_suspend_HW_thread(0);
		CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(0), 0x00); /* resume */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

		msleep_interruptible(500);
		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);
	} while (0);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecDestroy(hReqA);
	/* del_timer(&timer_reqA); */

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_errors(void)
{
	cmdqRecHandle hReq;
	cmdqRecHandle hLoop;
	TaskStruct *pTask;
	int32_t ret;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t UNKNOWN_OP = 0x50;
	uint32_t *pCommand;

	ret = 0;
	do {
		/* SW timeout */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoop);
		cmdqRecReset(hLoop);
		cmdqRecSetSecure(hLoop, false);
		cmdqRecPoll(hLoop, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdqRecStartLoop(hLoop);

		CMDQ_MSG("=============== INIFINITE Wait ===================\n");

		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &hReq);

		/* turn on ALL engine flag to test dump */
		for (ret = 0; ret < CMDQ_MAX_ENGINE_COUNT; ++ret) {
			hReq->engineFlag |= 1LL << ret;
		}
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecWait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecFlush(hReq);

		CMDQ_MSG("=============== INIFINITE JUMP ===================\n");

		/* HW timeout */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecWait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, 0, 8);	/* JUMP to connect tasks */
		ret = _test_submit_async(hReq, &pTask);
		msleep_interruptible(500);
		ret = cmdqCoreWaitAndReleaseTask(pTask, 8000);

		CMDQ_MSG("================ POLL INIFINITE ====================\n");

		CMDQ_MSG("testReg: %lx\n", MMSYS_DUMMY_REG);

		CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0x0);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecPoll(hReq, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdqRecFlush(hReq);

		CMDQ_MSG("================= INVALID INSTR =================\n");

		/* invalid instruction */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, -1, 0);
		cmdqRecFlush(hReq);

		CMDQ_MSG("================= INVALID INSTR: UNKNOWN OP(0x%x) =================\n",
			 UNKNOWN_OP);
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		/* invalid instruction is asserted when unkown OP */
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		{
			pCommand = (uint32_t *) ((uint8_t *) hReq->pBuffer + hReq->blockSize);
			*pCommand++ = 0x0;
			*pCommand++ = (UNKNOWN_OP << 24);
			hReq->blockSize += 8;
		}
		cmdqRecFlush(hReq);

	} while (0);

	cmdqRecDestroy(hReq);
	cmdqRecDestroy(hLoop);

	CMDQ_MSG("%s END\n", __func__);
	return;
}

static int32_t finishCallback(unsigned long data)
{
	CMDQ_LOG("callback() with data=0x%08lx\n", data);
	return 0;
}

static void testcase_fire_and_forget(void)
{
	cmdqRecHandle hReqA, hReqB;

	CMDQ_MSG("%s\n", __func__);
	do {
		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqA);
		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqB);
		cmdqRecReset(hReqA);
		cmdqRecReset(hReqB);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecSetSecure(hReqB, gCmdqTestSecure);

		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdqRecFlushAsync(hReqA);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdqRecFlushAsyncCallback(hReqB, finishCallback, 443);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
	} while (0);

	cmdqRecDestroy(hReqA);
	cmdqRecDestroy(hReqB);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_request(void)
{
	cmdqRecHandle hReqA, hReqB;
	TaskStruct *pTaskA, *pTaskB;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(1000));

	setup_timer(&timer_reqB, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_1);
	/* mod_timer(&timer_reqB, jiffies + msecs_to_jiffies(1300)); */

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	do {
		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqA);
		cmdqRecReset(hReqA);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8);

		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqB);
		cmdqRecReset(hReqB);
		cmdqRecSetSecure(hReqB, gCmdqTestSecure);
		cmdqRecWait(hReqB, CMDQ_SYNC_TOKEN_USER_1);
		cmdq_append_command(hReqB, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqB, CMDQ_CODE_JUMP, 0, 8);

		ret = _test_submit_async(hReqA, &pTaskA);
		ret = _test_submit_async(hReqB, &pTaskB);

		CMDQ_MSG("%s start wait sleep========\n", __func__);
		msleep_interruptible(500);

		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);

		CMDQ_MSG("%s start wait B, this should timeout========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskB, 600);
		CMDQ_MSG("%s wait B get %d ========\n", __func__, ret);

	} while (0);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	cmdqRecDestroy(hReqA);
	cmdqRecDestroy(hReqB);

	del_timer(&timer_reqA);
	del_timer(&timer_reqB);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_multiple_async_request(void)
{
#define TEST_REQ_COUNT 30
	cmdqRecHandle hReq[TEST_REQ_COUNT]= { 0 };
	TaskStruct *pTask[TEST_REQ_COUNT] = { 0 };
	int32_t ret = 0;
	int i;

	CMDQ_MSG("%s\n", __func__);

	setup_timer(&timer, &_testcase_sync_token_timer_loop_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));

	/* Queue multiple async request */
	/* to test dynamic task allocation */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < TEST_REQ_COUNT; ++i) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReq[i]);
		if (0 > ret) {
			CMDQ_ERR("%s cmdqRecCreate failed:%d, i:%d\n ", __func__, ret, i);
			continue;
		}

		cmdqRecReset(hReq[i]);

		/* specify engine flag in order to dispatch all tasks to the same HW thread*/
		hReq[i]->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

		cmdqRecSetSecure(hReq[i], gCmdqTestSecure);
		cmdqRecWait(hReq[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_rec_finalize_command(hReq[i], false);

		/* higher priority for later tasks */
		hReq[i]->priority = i;

		ret = _test_submit_async(hReq[i], &pTask[i]);

		CMDQ_MSG("======== create task[%2d]=0x%p done ========\n", i, pTask[i]);
	}

	/* release token and wait them */
	for (i = 0; i < TEST_REQ_COUNT; ++i) {

		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		msleep_interruptible(100);

		CMDQ_LOG("======== wait task[%2d]=0x%p ========\n", i, pTask[i]);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 1000);
		cmdqRecDestroy(hReq[i]);
	}

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	del_timer(&timer);

	CMDQ_MSG("%s END\n", __func__);
}


static void testcase_async_request_partial_engine(void)
{
	int32_t ret = 0;
	int i;
	CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,
		CMDQ_SCENARIO_JPEG_DEC,
		CMDQ_SCENARIO_PRIMARY_MEMOUT,
		CMDQ_SCENARIO_SUB_DISP,
		CMDQ_SCENARIO_DEBUG,
	};

	struct timer_list timers[sizeof(scn) / sizeof(scn[0])];

	cmdqRecHandle hReq[(sizeof(scn) / sizeof(scn[0]))] = { 0 };
	TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		setup_timer(&timers[i], &_testcase_sync_token_timer_func,
			    CMDQ_SYNC_TOKEN_USER_0 + i);
		mod_timer(&timers[i], jiffies + msecs_to_jiffies(400 * (1 + i)));
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);

		cmdqRecCreate(scn[i], &hReq[i]);
		cmdqRecReset(hReq[i]);
		cmdqRecSetSecure(hReq[i], false);
		cmdqRecWait(hReq[i], CMDQ_SYNC_TOKEN_USER_0 + i);
		cmdq_rec_finalize_command(hReq[i], false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq[i], &pTasks[i]);
	}


	/* wait for task completion */
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(3000));
	}

	/* clear token */
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);
		cmdqRecDestroy(hReq[i]);
		del_timer(&timers[i]);
	}

	CMDQ_MSG("%s END\n", __func__);

}

static void _testcase_unlock_all_event_timer_func(unsigned long data)
{
	uint32_t token = 0;

	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger events\n");
	for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token) {
		/* 3 threads waiting, so update 3 times */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
	}
}

static void testcase_sync_token_threaded(void)
{
	CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,	/* high prio */
		CMDQ_SCENARIO_JPEG_DEC,	/* normal prio */
		CMDQ_SCENARIO_TRIGGER_LOOP	/* normal prio */
	};
	int32_t ret = 0;
	int i = 0;
	uint32_t token = 0;
	struct timer_list eventTimer;
	cmdqRecHandle hReq[(sizeof(scn) / sizeof(scn[0]))] = { 0 };
	TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		setup_timer(&eventTimer, &_testcase_unlock_all_event_timer_func, 0);
		mod_timer(&eventTimer, jiffies + msecs_to_jiffies(500));

		/*  */
		/* 3 threads, all wait & clear 511 events */
		/*  */
		cmdqRecCreate(scn[i], &hReq[i]);
		cmdqRecReset(hReq[i]);
		cmdqRecSetSecure(hReq[i], false);
		for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token) {
			cmdqRecWait(hReq[i], (CMDQ_EVENT_ENUM) token);
		}
		cmdq_rec_finalize_command(hReq[i], false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq[i], &pTasks[i]);
	}


	/* wait for task completion */
	msleep_interruptible(1000);
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(5000));
	}

	/* clear token */
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		cmdqRecDestroy(hReq[i]);
	}

	del_timer(&eventTimer);
	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list g_loopTimer;
static int g_loopIter;
static cmdqRecHandle hLoopReq;

static void _testcase_loop_timer_func(unsigned long data)
{
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	g_loopIter++;
}

static void testcase_loop(void)
{
	int status = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoopReq);
	cmdqRecReset(hLoopReq);
	cmdqRecSetSecure(hLoopReq, false);
	cmdqRecWait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	/* should success */
	status = cmdqRecStartLoop(hLoopReq);
	BUG_ON(status != 0);

	/* should fail because already started */
	CMDQ_MSG("============testcase_loop start loop\n");
	status = cmdqRecStartLoop(hLoopReq);
	BUG_ON(status >= 0);

	cmdqRecDumpCommand(hLoopReq);

	/* WAIT */
	while (g_loopIter < 20) {
		msleep_interruptible(2000);
	}
	msleep_interruptible(2000);

	CMDQ_MSG("============testcase_loop stop timer\n");
	cmdqRecDestroy(hLoopReq);
	del_timer(&g_loopTimer);

	CMDQ_MSG("%s\n", __func__);
}

static unsigned long gLoopCount = 0L;
static void _testcase_trigger_func(unsigned long data)
{
	/* trigger sync event */
	CMDQ_MSG("_testcase_trigger_func");
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

	/* start again */
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));
	gLoopCount++;
}

/*
static void leave_loop_func(struct work_struct *w)
{
    CMDQ_MSG("leave_loop_func: cancel loop");
    cmdqRecStopLoop(hLoopConfig);
    hLoopConfig = NULL;
    return;
}

DECLARE_WORK(leave_loop, leave_loop_func);

int32_t my_irq_callback(unsigned long data)
{
    CMDQ_MSG("%s data=%d\n", __FUNCTION__, data);

    ++gLoopCount;

    switch(data)
    {
    case 1:
	if(gLoopCount < 20)
	{
	    return 0;
	}
	else
	{
	    return -1;
	}
	break;
    case 2:
	if(gLoopCount > 40)
	{
	    // insert stopping cal
	    schedule_work(&leave_loop);
	}
	break;
    }
    return 0;
}
*/

static void testcase_trigger_thread(void)
{
	cmdqRecHandle hTrigger, hConfig;
	int32_t ret = 0;
	int index = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token for every 1 sec */
	setup_timer(&timer, &_testcase_trigger_func, 0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

	do {
		/* THREAD 1, trigger loop */
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
		cmdqRecReset(hTrigger);
		/* * WAIT and CLEAR config dirty */
		/* cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY); */

		/* * WAIT and CLEAR TE */
		/* cmdqRecWait(hTrigger, CMDQ_EVENT_MDP_DSI0_TE_SOF); */

		/* * WAIT and CLEAR stream done */
		/* cmdqRecWait(hTrigger, CMDQ_EVENT_MUTEX0_STREAM_EOF); */

		/* * WRITE mutex enable */
		/* cmdqRecWait(hTrigger, MM_MUTEX_BASE + 0x20); */

		cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);

		/* * RUN forever but each IRQ trigger is bypass to my_irq_callback */
		ret = cmdqRecStartLoop(hTrigger);

		/* THREAD 2, config thread */
		cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &hConfig);


		hConfig->priority = CMDQ_THR_PRIO_NORMAL;
		cmdqRecReset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 10; ++index) {
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
		}
		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 0\n");

		hConfig->priority = CMDQ_THR_PRIO_DISPLAY_CONFIG;
		cmdqRecReset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 10; ++index) {
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
		}
		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 1\n");

		cmdqRecReset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 500; ++index) {
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
		}
		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 2\n");

		/* WAIT */
		while (gLoopCount < 20) {
			msleep_interruptible(2000);
		}
	} while (0);

	del_timer(&timer);
	cmdqRecDestroy(hTrigger);
	cmdqRecDestroy(hConfig);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch_scenarios(void)
{
	/* make sure both prefetch and non-prefetch cases */
	/* handle 248+ instructions properly */
	cmdqRecHandle hConfig;
	int32_t ret = 0;
	int index = 0, scn = 0;
	const int INSTRUCTION_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with 248+ commands */
	for (scn = 0; scn < CMDQ_MAX_SCENARIO_COUNT; ++scn) {
		if (cmdq_core_is_request_from_user_space(scn)) {
			continue;
		}

		CMDQ_MSG("testcase_prefetch_scenarios scenario:%d\n", scn);
		cmdqRecCreate((CMDQ_SCENARIO_ENUM) scn, &hConfig);
		cmdqRecReset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < INSTRUCTION_COUNT; ++index) {
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
		}

		ret = cmdqRecFlush(hConfig);
		BUG_ON(ret < 0);
		cmdqRecDestroy(hConfig);
	}
	CMDQ_MSG("%s END\n", __func__);
}

extern void cmdq_core_reset_hw_events(void);

void testcase_clkmgr_impl(enum cg_clk_id gateId,
			char *name,
			const unsigned long testWriteReg,
			const uint32_t testWriteValue,
			const unsigned long testReadReg,
			const bool verifyWriteResult)
{
/* clkmgr is not available on FPGA */
#ifndef CONFIG_MTK_FPGA
	uint32_t value = 0;

	CMDQ_MSG("====== %s:%s ====== \n", __func__, name);
	CMDQ_VERBOSE("clk:%d, name:%s\n", gateId, name);
	CMDQ_VERBOSE("write reg(0x%lx) to 0x%08x, read reg(0x%lx), verify write result:%d\n",
				testWriteReg, testWriteValue, testReadReg, verifyWriteResult);

	/* turn on CLK, function should work */
	CMDQ_MSG("enable_clock\n");
	enable_clock(gateId, name);

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if ((true == verifyWriteResult) &&
		(testWriteValue != value)) {
		CMDQ_ERR("when enable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		/* BUG(); */
	}

	/* turn off CLK, function should not work and access register should not cause hang */
	CMDQ_MSG("disable_clock\n");
	disable_clock(gateId, name);

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (0 != value) {
		CMDQ_ERR("when disable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		/* BUG(); */
	}
#endif
}

static void testcase_clkmgr(void)
{
	CMDQ_MSG("%s\n", __func__);
	testcase_clkmgr_impl(MT_CG_INFRA_GCE,
			"CMDQ_TEST",
			CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG),
			0xFFFFDEAD,
			CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG),
			true);

	testcase_clkmgr_mdp();

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_dram_access(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t *regResults;
	dma_addr_t regResultsMVA;
	dma_addr_t dstMVA;
	uint32_t argA;
	uint32_t subsysCode;
	uint32_t *pCmdEnd = NULL;
	unsigned long long data64;

	CMDQ_MSG("%s\n", __func__);

	regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
					sizeof(uint32_t) * 2, &regResultsMVA, GFP_KERNEL);

	/* set up intput */
	regResults[0] = 0xdeaddead;	/* this is read-from */
	regResults[1] = 0xffffffff;	/* this is write-to */

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/*  */
	/* READ from DRAME: register to read from */
	/*  */
	/* note that we force convert to physical reg address. */
	/* if it is already physical address, it won't be affected (at least on this platform) */
	argA = CMDQ_TEST_MMSYS_DUMMY_PA;
	subsysCode = cmdq_subsys_from_phys_addr(argA);

	pCmdEnd = (uint32_t *) (((char *)handle->pBuffer) + handle->blockSize);

	CMDQ_MSG("pCmdEnd initial=0x%p, reg MVA=%pa, size=%d\n",
		 pCmdEnd, &regResultsMVA, handle->blockSize);

	/* Move &(regResults[0]) to CMDQ_DATA_REG_DEBUG_DST */
	*pCmdEnd = (uint32_t) CMDQ_PHYS_TO_AREG(regResultsMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((regResultsMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	/*  */
	/* WRITE to DRAME: */
	/* from src_addr(CMDQ_DATA_REG_DEBUG_DST) to external RAM (regResults[1]) */
	/*  */

	/* Read data from *CMDQ_DATA_REG_DEBUG_DST to CMDQ_DATA_REG_DEBUG */
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd =
	    (CMDQ_CODE_READ << 24) | (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 <<
												21);
	pCmdEnd += 1;

	/* Load dst_addr to GPR: Move &(regResults[1]) to CMDQ_DATA_REG_DEBUG_DST */
	dstMVA = regResultsMVA + 4;	/* note regResults is a uint32_t array */
	*pCmdEnd = ((uint32_t) dstMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((dstMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	/* Write from CMDQ_DATA_REG_DEBUG to *CMDQ_DATA_REG_DEBUG_DST */
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_WRITE << 24) |
	    (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 << 21);

	pCmdEnd += 1;

	handle->blockSize += 4 * 8;	/* 4 * 64-bit instructions */

	cmdqRecDumpCommand(handle);

	cmdqRecFlush(handle);

	cmdqRecDumpCommand(handle);

	cmdqRecDestroy(handle);

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_DEBUG_DST);

	CMDQ_MSG("regResults=[0x%08x, 0x%08x]\n", regResults[0], regResults[1]);
	CMDQ_MSG("CMDQ_DATA_REG_DEBUG=0x%08x, CMDQ_DATA_REG_DEBUG_DST=0x%llx\n",
		 CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)), data64);

	if (regResults[1] != regResults[0]) {
		CMDQ_ERR("ERROR!!!!!!\n");
	} else {
		CMDQ_MSG("OK!!!!!!\n");
	}

	cmdq_core_free_hw_buffer(cmdq_dev_get(), 2 * sizeof(uint32_t), regResults, regResultsMVA);

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
#endif
}

static void testcase_long_command(void)
{
	int i;
	cmdqRecHandle handle;
	uint32_t data;
	uint32_t pattern = 0x0;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	/* build a 64KB instruction buffer */
	for (i = 0; i < 64 * 1024 / 8; ++i) {
		pattern = i;
		cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pattern, ~0);
	}
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	/* verify data */
	do {
		if(true == gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n", __func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
		if (pattern != data) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x, not pattern 0x%08x\n", data, pattern);
		}
	} while (0);
	CMDQ_MSG("%s END\n", __func__);

	return;
}

static void testcase_perisys_apb(void)
{
#ifdef CMDQ_GPR_SUPPORT
	/* write value to PERISYS register */
	/* we use MSDC debug to test: */
	/* write SEL, read OUT. */

	const uint32_t MSDC_SW_DBG_SEL_PA = 0x11230000 + 0xA0;
	const uint32_t MSDC_SW_DBG_OUT_PA = 0x11230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0_PA = 0x11220000;

#ifdef CMDQ_OF_SUPPORT
	const unsigned long MSDC_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MSDC0");
	const unsigned long AUDIO_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,AUDIO");
	const unsigned long MSDC_SW_DBG_OUT = MSDC_VA_BASE + 0xA4;
	const unsigned long AUDIO_TOP_CONF0 = AUDIO_VA_BASE;

	/* CMDQ_LOG("MSDC_VA_BASE:  VA:%lx, PA: 0x%08x\n", MSDC_VA_BASE, 0x11230000); */
	/* CMDQ_LOG("AUDIO_VA_BASE: VA:%lx, PA: 0x%08x\n", AUDIO_TOP_CONF0_PA, 0x11220000); */
#else
	const uint32_t MSDC_SW_DBG_OUT = 0xF1230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0 = 0xF1220000;
#endif

	const uint32_t AUDIO_TOP_MASK = ~0 & ~(1 << 28 |
					       1 << 21 |
					       1 << 17 |
					       1 << 16 |
					       1 << 15 |
					       1 << 11 |
					       1 << 10 |
					       1 << 7 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 1 | 1 << 0);
	cmdqRecHandle handle = NULL;
	uint32_t data = 0;
	uint32_t dataRead = 0;

	CMDQ_MSG("%s\n", __func__);
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWrite(handle, MSDC_SW_DBG_SEL_PA, 1, ~0);
	cmdqRecFlush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(MSDC_SW_DBG_OUT);
	CMDQ_MSG("MSDC_SW_DBG_OUT = 0x%08x=====\n", data);

	/* test read from AP_DMA_GLOBAL_SLOW_DOWN to CMDQ GPR */
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecReadToDataRegister(handle, MSDC_SW_DBG_OUT_PA, CMDQ_DATA_REG_PQ_COLOR);
	cmdqRecFlush(handle);

	/* verify data */
	dataRead = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != dataRead) {
		CMDQ_ERR("TEST FAIL: CMDQ_DATA_REG_PQ_COLOR is 0x%08x, different=====\n", dataRead);
	}

	CMDQ_REG_SET32(AUDIO_TOP_CONF0, ~0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("write 0xFFFFFFFF to AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	CMDQ_REG_SET32(AUDIO_TOP_CONF0, 0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("Before AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, AUDIO_TOP_CONF0_PA, ~0, AUDIO_TOP_MASK);
	cmdqRecFlush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("after AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	if (data != AUDIO_TOP_MASK) {
		CMDQ_ERR("TEST FAIL: AUDIO_TOP_CONF0 is 0x%08x=====\n", data);
	}

	cmdqRecDestroy(handle);

#ifdef CMDQ_OF_SUPPORT
	/* release registers map */
	cmdq_dev_free_module_base_VA(MSDC_VA_BASE);
	cmdq_dev_free_module_base_VA(AUDIO_VA_BASE);
#endif

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
#endif /* CMDQ_GPR_SUPPORT */
}

static void testcase_write_address(void)
{
	dma_addr_t pa = 0;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqCoreAllocWriteAddress(3, &pa);
	CMDQ_LOG("ALLOC: 0x%pa\n", &pa);
	value = cmdqCoreReadWriteAddress(pa);
	CMDQ_LOG("value 0: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 1);
	CMDQ_LOG("value 1: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 2);
	CMDQ_LOG("value 2: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 3);
	CMDQ_LOG("value 3: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 4);
	CMDQ_LOG("value 4: 0x%08x\n", value);

	value = cmdqCoreReadWriteAddress(pa + (4 * 20));
	CMDQ_LOG("value 80: 0x%08x\n", value);

	/* free invalid start address fist to verify error handle */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:0, it's a error case\n");
	cmdqCoreFreeWriteAddress(0);

	/* ok case */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:%pa, it's a ok case\n", &pa);
	cmdqCoreFreeWriteAddress(pa);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write_from_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t value;
	const uint32_t PATTERN = 0xFFFFDEAD;
	const uint32_t srcGprId = CMDQ_DATA_REG_DEBUG;
	const uint32_t dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	const unsigned long dstRegVA = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	/* clean dst register value*/
	CMDQ_REG_SET32(dstRegVA, 0x0);

	/* init GPR as value 0xFFFFDEAD */
	CMDQ_REG_SET32(CMDQ_GPR_R32(srcGprId), PATTERN);
	value = CMDQ_REG_GET32(CMDQ_GPR_R32(srcGprId));
	if (PATTERN != value) {
		CMDQ_ERR("init CMDQ_DATA_REG_DEBUG to 0x%08x failed, value: 0x%08x\n", PATTERN, value);
	}

	/* write GPR data reg to hw register*/
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWriteFromDataRegister(handle, srcGprId, dstRegPA);
	cmdqRecFlush(handle);

	cmdqRecDumpCommand(handle);

	cmdqRecDestroy(handle);

	/* verify */
	value = CMDQ_REG_GET32(dstRegVA);
	if (PATTERN != value) {
		CMDQ_ERR("%s failed, dstReg value is not 0x%08x, value: 0x%08x\n", __func__, PATTERN, value);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
#endif
}

static void testcase_read_to_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t data;
	unsigned long long data64;
	unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	/* init GPR 64*/
	CMDQ_REG_SET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST, 0x1234567890ABCDEFULL);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xbeefbeef); /* R4 */
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0x0); /* R5 */

	cmdq_core_gpr_dump();

	/* [read 64 bit test] move data from GPR to GPR_Px: COLOR to COLOR_DST (64 bit) */
	#if 1
	cmdqRecReadToDataRegister(handle, CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
				  CMDQ_DATA_REG_PQ_COLOR_DST);
	#else
	/* 64 bit behavior of Read OP depends APB bus implementation */
	/* (CMDQ uses APB to access HW register, use AXI to access DRAM) */
	/* from DE's suggestion, */
	/* 1. for read HW register case, it's better to separate 1 x 64 bit length read to 2 x 32 bit length read*/
	/* 2. for GPRx each assignment case, it's better performance to use MOVE op to read GPR_x1 to GPR_x2*/

	/* when Read 64 length failed, try to use move to clear up if APB issue */
	const uint32_t srcDataReg = CMDQ_DATA_REG_PQ_COLOR;
	const uint32_t dstDataReg = CMDQ_DATA_REG_PQ_COLOR_DST;
	/* argA, 22 bit 1: argB is GPR*/
	/* argA, 23 bit 1: argA is GPR*/
	cmdq_append_command(
				handle,
				CMDQ_CODE_RAW,
				(CMDQ_CODE_MOVE << 24) | (dstDataReg << 16) | (4 << 21) | (2 << 21),
				srcDataReg);
	#endif

	/* [read 32 bit test] move data from register value to GPR_Rx: MM_DUMMY_REG to COLOR(32 bit) */
	cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);

	cmdqRecFlush(handle);
	cmdqRecDumpCommand(handle);
	cmdqRecDestroy(handle);

	cmdq_core_gpr_dump();

	/* verify data */
	data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != 0xdeaddead) {
		CMDQ_ERR("[Read 32 bit from GPR_Rx]TEST FAIL: PQ reg value is 0x%08x\n", data);
	}

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST);
	if (0xbeefbeef != data64) {
		CMDQ_ERR("[Read 64 bit from GPR_Px]TEST FAIL: PQ_DST reg value is 0x%llx\n", data64);
	}

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
	return;
#endif
}

static void testcase_write_reg_from_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const uint32_t PATTEN = 0xBCBCBCBC;
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;
	uint32_t value = 0;
	long long value64 = 0LL;
	const CMDQ_DATA_REGISTER_ENUM dstRegId = CMDQ_DATA_REG_DEBUG;
	const CMDQ_DATA_REGISTER_ENUM srcRegId = CMDQ_DATA_REG_DEBUG_DST;

	CMDQ_MSG("%s\n", __func__);

	/* init */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(dstRegId), 0xdeaddead);
	CMDQ_REG_SET64_GPR_PX(srcRegId, 0xdeaddeaddeaddead);

	cmdqBackupAllocateSlot(&hSlot, 1);
	cmdqBackupWriteSlot(hSlot, 0, PATTEN);
	cmdqBackupReadSlot(hSlot, 0, &value);
	if (PATTEN != value) {
		CMDQ_ERR("%s, slot init failed\n", __func__);
	}

	/* Create cmdqRec */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	/* Reset command buffer */
	cmdqRecReset(handle);

	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/* Insert commands to write register with slot's value */
	cmdqRecBackupWriteRegisterFromSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);

	/* Execute commands */
	cmdqRecFlush(handle);

	/* debug dump command instructions */
	cmdqRecDumpCommand(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdqRecDestroy(handle);

	/* verify */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (PATTEN != value) {
		CMDQ_ERR("%s failed, value:0x%x\n", __func__, value);
	}

	value = CMDQ_REG_GET32(CMDQ_GPR_R32(dstRegId));
	value64 = CMDQ_REG_GET64_GPR_PX(srcRegId);
	CMDQ_LOG("srcGPR(%x):0x%llx\n", srcRegId, value64);
	CMDQ_LOG("dstGPR(%x):0x%08x\n", dstRegId, value);

	/* release result free slot */
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
	return;
#endif
}

static void testcase_backup_reg_to_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	cmdqBackupSlotHandle hSlot = 0;
	int i;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	/* Create cmdqRec */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	/* Create Slot */
	cmdqBackupAllocateSlot(&hSlot, 5);

	for (i = 0; i < 5; ++i) {
		cmdqBackupWriteSlot(hSlot, i, i);
	}

	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);
		if (value != i) {
			CMDQ_ERR("testcase_cmdqBackupWriteSlot FAILED!!!!!\n");
		}
		CMDQ_LOG("testcase_cmdqBackupWriteSlot OK!!!!!\n");
	}

	/* Reset command buffer */
	cmdqRecReset(handle);

	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/* Insert commands to backup registers */
	for (i = 0; i < 5; ++i) {
		cmdqRecBackupRegisterToSlot(handle, hSlot, i, CMDQ_TEST_MMSYS_DUMMY_PA);
	}

	/* Execute commands */
	cmdqRecFlush(handle);

	/* debug dump command instructions */
	cmdqRecDumpCommand(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdqRecDestroy(handle);

	/* verify data by reading it back from slot */
	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);
		CMDQ_LOG("backup slot %d = 0x%08x\n", i, value);

		if (value != 0xdeaddead) {
			CMDQ_ERR("content error!!!!!!!!!!!!!!!!!!!!\n");
		}
	}

	/* release result free slot */
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
	return;
#endif
}

static void testcase_update_value_to_slot(void)
{
	int32_t i;
	uint32_t value;
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;
	const uint32_t PATTERNS[] = {
		0xDEAD0000, 0xDEAD0001, 0xDEAD0002, 0xDEAD0003, 0xDEAD0004 };

	CMDQ_MSG("%s\n", __func__);

	/* Create Slot*/
	cmdqBackupAllocateSlot(&hSlot, 5);

	/*use CMDQ to update slot value*/
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	for (i = 0; i < 5; ++i) {
		cmdqRecBackupUpdateSlot(handle, hSlot, i, PATTERNS[i]);
	}
	cmdqRecFlush(handle);
	cmdqRecDumpCommand(handle);
	cmdqRecDestroy(handle);

	/* CPU verify value by reading it back from slot  */
	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);

		if (PATTERNS[i] != value) {
			CMDQ_ERR("slot[%d] = 0x%08x...content error! It should be 0x%08x\n",
				i, value, PATTERNS[i]);
		} else {
			CMDQ_LOG("slot[%d] = 0x%08x\n", i, value);
		}
	}

	/* release result free slot */
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_poll(void)
{
	cmdqRecHandle handle;

	uint32_t value = 0;
	uint32_t pollingVal = 0x00003001;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	/* it's too slow that set value after enable CMDQ */
	/* sw timeout will be hanppened before CPU schedule to set value..., so we set value here */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, pollingVal);
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	CMDQ_MSG("target value is 0x%08x\n", value);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pollingVal, ~0);

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (pollingVal != value) {
		CMDQ_ERR("polling target value is 0x%08x\n", value);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write_with_mask(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t MASK = (1 << 16);
	const uint32_t EXPECT_RESULT = PATTERN & MASK;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0x0 */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, 0x0);

	/* use CMDQ to set to PATTERN */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, MASK);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (EXPECT_RESULT != value ) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, EXPECT_RESULT);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch(void)
{
	cmdqRecHandle handle;
	int i;
	uint32_t value = 0;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);	/* 0xDEADDEAD; */
	const uint32_t testRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	const unsigned long testRegVA = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t REP_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(testRegVA, ~0);

	/* No prefetch. */
	/* use CMDQ to set to PATTERN */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for (i = 0; i < REP_COUNT; ++i) {
		cmdqRecWrite(handle, testRegPA, PATTERN, ~0);
	}
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	msleep_interruptible(1000);
	cmdqRecDestroy(handle);

	/* use prefetch */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for (i = 0; i < REP_COUNT; ++i) {
		cmdqRecWrite(handle, testRegPA, PATTERN, ~0);
	}
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	msleep_interruptible(1000);
	cmdqRecDestroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(testRegVA);
	if (value != PATTERN) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_backup_register(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	cmdqRecHandle handle;
	int ret = 0;
	uint32_t regAddr[3] = { CMDQ_TEST_MMSYS_DUMMY_PA,
		CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
		CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_0)
	};
	uint32_t regValue[3] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xAAAAAAAA);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xBBBBBBBB);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0xCCCCCCCC);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	ret = cmdqRecFlushAndReadRegister(handle, 3, regAddr, regValue);
	cmdqRecDestroy(handle);

	if (regValue[0] != 0xAAAAAAAA) {
		CMDQ_ERR("regValue[0] is 0x%08x, wrong!\n", regValue[0]);
	}
	if (regValue[1] != 0xBBBBBBBB) {
		CMDQ_ERR("regValue[1] is 0x%08x, wrong!\n", regValue[1]);
	}
	if (regValue[2] != 0xCCCCCCCC) {
		CMDQ_ERR("regValue[2] is 0x%08x, wrong!\n", regValue[2]);
	}

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
#endif
}

static void testcase_get_result(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	int i;
	cmdqRecHandle handle;
	int ret = 0;
	cmdqCommandStruct desc = {0};

	int registers[1] = { CMDQ_TEST_MMSYS_DUMMY_PA };
	int result[1] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	/* use CMDQ_SCENARIO_PRIMARY_ALL to test */
	/* because it has COLOR0 HW flag */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_ALL, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/* insert dummy commands */
	cmdq_rec_finalize_command(handle, false);

	/* init desc attributes after finalize command to ensure correct size and buffer addr */
	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t)(unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;

	desc.regRequest.count = 1;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t)(unsigned long)registers;
	desc.regValue.count = 1;
	desc.regValue.regValues = (cmdqU32Ptr_t)(unsigned long)result;

	desc.secData.isSecure = handle->secData.isSecure;
	desc.secData.addrMetadataCount = 0;
	desc.secData.addrMetadataMaxCount = 0;
	desc.secData.waitCookie = 0;
	desc.secData.resetExecCnt = false;

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	/* manually raise the dirty flag */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX0_STREAM_EOF);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX1_STREAM_EOF);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX2_STREAM_EOF);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX3_STREAM_EOF);

	for (i = 0; i < 1; ++i) {
		ret = cmdqCoreSubmitTask(&desc);
		if (CMDQ_U32_PTR(desc.regValue.regValues)[0] != 0xdeaddead) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x\n", CMDQ_U32_PTR(desc.regValue.regValues)[0]);
		}
	}

	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
	return;
#else
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
#endif
}

static void testcase_emergency_buffer(void)
{
	/* ensure to define CMDQ_TEST_EMERGENCY_BUFFER in cmdq_core.c*/

	const uint32_t longCommandSize = 160 * 1024;
	const uint32_t submitTaskCount = 4;
	cmdqRecHandle handle;
	int32_t i;

	CMDQ_MSG("%s\n", __func__);

	/* force to use emergency buffer */
	if (0 > cmdq_core_enable_emergency_buffer_test(true) ) {
		return;
	}

	/* prepare long command */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for(i = 0; i < (longCommandSize / CMDQ_INST_SIZE); i++)
	{
		cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);
	}

	/* submit */
	for (i = 0; i < submitTaskCount; i++)
 	{
 		CMDQ_LOG("async submit large command(size: %d), count:%d\n", longCommandSize, i);
		cmdqRecFlushAsync(handle);
	}

	msleep_interruptible(1000);

	/* reset to apply normal memory allocation flow */
	cmdq_core_enable_emergency_buffer_test(false);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static int _testcase_simplest_command_loop_submit(const uint32_t loop,
					CMDQ_SCENARIO_ENUM scenario,
					const long long engineFlag,	/* force specify engineFlag */
					const bool isSecureTask)
{
	cmdqRecHandle handle;
	int32_t i;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(scenario, &handle);
	for(i = 0; i < loop; i++)
	{
		CMDQ_MSG("pid: %d, flush:%4d, engineFlag:0x%llx, isSecureTask:%d\n",
			current->pid, i, engineFlag, isSecureTask);
		cmdqRecReset(handle);
		cmdqRecSetSecure(handle, isSecureTask);
		handle->engineFlag = engineFlag;
		cmdqRecFlush(handle);
	}
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

/* threadfn: int (*threadfn)(void *data) */
static int _testcase_thread_dispatch(void *data)
{
	long long engineFlag;
	engineFlag = *((long long*)data);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG, engineFlag, false);

	return 0;
}

static void testcase_thread_dispatch(void)
{
	char threadName[20];
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const long long engineFlag1 = (0x1 << CMDQ_ENG_ISP_IMGI) | (0x1 << CMDQ_ENG_ISP_IMGO);
	const long long engineFlag2 = (0x1 << CMDQ_ENG_MDP_RDMA0) | (0x1 << CMDQ_ENG_MDP_WDMA);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=============== 2 THREAD with different engines ===============\n");

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag1);
	pKThread1 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag1), threadName);
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag2);
	pKThread2 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag2), threadName);
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag1, false);
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag2, false);

	CMDQ_MSG("%s END\n", __func__);

	return;
}

static int _testcase_full_thread_array(void *data)
{
	/* this testcase will be passed only when cmdqSecDr support async config mode because */
	/* never execute event setting till IWC back to NWd */

	cmdqRecHandle handle;
	int32_t i;

	/* clearn event first */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	/* specify engine flag in order to dispatch all tasks to the same HW thread*/
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < 50; i++) {
		CMDQ_LOG("pid: %d, flush:%6d\n", current->pid, i);

		if (40 == i) {
			CMDQ_LOG("set token: %d to 1\n", CMDQ_SYNC_TOKEN_USER_0);
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		}

		cmdqRecFlushAsync(handle);
	}
	cmdqRecDestroy(handle);

	return 0;
}

static void testcase_full_thread_array(void)
{
	char threadName[20];
	struct task_struct *pKThread;

	CMDQ_MSG("%s\n", __func__);

	sprintf(threadName, "cmdqKTHR");
	pKThread = kthread_run(_testcase_full_thread_array, NULL, threadName);
	if (IS_ERR(pKThread)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
	}

	msleep_interruptible(5 * 1000);

	CMDQ_MSG("%s END\n", __func__);
	return;
}

static void testcase_module_full_dump(void)
{
	cmdqRecHandle handle;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();

	CMDQ_MSG("%s\n", __func__);

	/* enable full dump*/
	if (false == alreadyEnableLog) {
		cmdq_core_set_log_level(1);
	}

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	/* clean SW token to invoke SW timeout latter*/
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/* turn on ALL except DISP engine flag to test dump */
	handle->engineFlag = ~(CMDQ_ENG_DISP_GROUP_BITS);

	CMDQ_LOG("%s, engine: 0x%llx, it's a timeout case\n",
		__func__, handle->engineFlag);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecFlush(handle);

	/* disable full dump*/
	if (false == alreadyEnableLog) {
		cmdq_core_set_log_level(0);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_profile_marker(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_MSG("%s: write op without profile marker\n", __func__);
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBCBCBCBC, ~0);
	cmdqRecFlush(handle);

	CMDQ_MSG("%s: write op with profile marker\n", __func__);	
	cmdqRecReset(handle);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0x11111111, ~0);
	cmdqRecProfileMarker(handle, "WRI_BEGIN");
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0x22222222, ~0);
	cmdqRecProfileMarker(handle, "WRI_END");

	cmdqRecDumpCommand(handle);
	cmdqRecFlush(handle);

	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_estimate_command_exec_time(void)
{
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;

	cmdqBackupAllocateSlot(&hSlot, 1);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);

	CMDQ_MSG("%s\n", __func__);
	
	CMDQ_LOG("=====write(1), write_w_mask(2), poll(2), wait(2), sync(1), eof(1), jump(1)\n");
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBA, ~0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBB, 0x1);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBC, 0x3);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xCCCCCCCCA, ~0);
	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xCCCCCCCCB, 0x1);

	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecClearEventToken(handle, CMDQ_SYNC_TOKEN_USER_1);

	cmdqRecDumpCommand(handle);
	cmdqRecEstimateEommandExecTime(handle);

	CMDQ_LOG("=====slots...\n");

	cmdqRecReset(handle);
	cmdqRecBackupRegisterToSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);
	cmdqRecBackupWriteRegisterFromSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);
	cmdqRecBackupUpdateSlot(handle, hSlot, 0, 0xDEADDEAD);

	cmdqRecDumpCommand(handle);
	cmdqRecEstimateEommandExecTime(handle);

	CMDQ_MSG("%s END\n", __func__);

	cmdqBackupFreeSlot(hSlot);
	cmdqRecDestroy(handle);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
#include "cmdq_sec_iwc_common.h"
#include "cmdqSecTl_Api.h"
int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
				TaskStruct *pTask, int32_t thread, CmdqSecFillIwcCB iwcFillCB, void *data);
#endif

void testcase_secure_basic(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s\n", __func__);

	do {

		CMDQ_MSG("=========== Hello cmdqSecTl ===========\n ");
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
			CMD_CMDQ_TL_TEST_HELLO_TL, NULL, CMDQ_INVALID_THREAD , NULL, NULL);
		if(0 > status) {
			CMDQ_ERR("entry cmdqSecTL failed, status:%d\n", status);
		}

		CMDQ_MSG("=========== Hello cmdqSecDr ===========\n ");
		status = cmdq_sec_submit_to_secure_world_async_unlocked(
			CMD_CMDQ_TL_TEST_DUMMY, NULL, CMDQ_INVALID_THREAD , NULL, NULL);
		if(0 > status) {
			CMDQ_ERR("entry cmdqSecDr failed, status:%d\n", status);
		}
	} while(0);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_disp_scenario(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	/* note: this case used to verify command compose in secure world. */
	/* It must test when DISP driver has switched primary DISP to secure path, */
	/* otherwise we should disable "enable GCE" in SWd in order to prevent phone hang */
	cmdqRecHandle hDISP;
	cmdqRecHandle hDisableDISP;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_LOG("=========== secure primary path ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &hDISP);
	cmdqRecReset(hDISP);
	cmdqRecSetSecure(hDISP, true);

	cmdqRecWrite(hDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);

	cmdqRecFlush(hDISP);
	cmdqRecDestroy(hDISP);
	CMDQ_LOG("=========== disp secure primary path ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH, &hDisableDISP);
	cmdqRecReset(hDisableDISP);
	cmdqRecSetSecure(hDisableDISP, true);
	cmdqRecWrite(hDisableDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlush(hDisableDISP);
	cmdqRecDestroy(hDisableDISP);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_meta_data(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqRecHandle hReqMDP;
	cmdqRecHandle hReqDISP;
	const uint32_t PATTERN_MDP = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t PATTERN_DISP = 0xBCBCBCBC;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	CMDQ_MSG("=========== MDP case ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqMDP);
	cmdqRecReset(hReqMDP);
	cmdqRecSetSecure(hReqMDP, true);

	/* specify use MDP engine */
	hReqMDP->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) | (1LL << CMDQ_ENG_MDP_WROT0);

	/* enable secure test */
	cmdqRecSecureEnableDAPC(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) | (1LL << CMDQ_ENG_MDP_WROT0));
	cmdqRecSecureEnablePortSecurity(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) | (1LL << CMDQ_ENG_MDP_WROT0));

	/* record command */
	cmdqRecWrite(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_MDP, ~0);

	cmdqRecFlush(hReqMDP);
	cmdqRecDestroy(hReqMDP);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_MDP) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_MDP);
	}

	CMDQ_MSG("=========== DISP case ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqDISP);
	cmdqRecReset(hReqDISP);
	cmdqRecSetSecure(hReqDISP, true);

	/* enable secure test */
	cmdqRecSecureEnableDAPC(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));
	cmdqRecSecureEnablePortSecurity(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));

	/* record command */
	cmdqRecWrite(hReqDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_DISP, ~0);

	cmdqRecFlush(hReqDISP);
	cmdqRecDestroy(hReqDISP);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_DISP) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_DISP);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
#endif

}

void testcase_submit_after_error_happened(void)
{
	cmdqRecHandle handle;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t pollingVal = 0x00003001;

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=========== timeout case ===========\n");

	/* let poll INIFINITE */
	/* CMDQ_REG_SET32(MMSYS_DUMMY_REG, pollingVal); */
	CMDQ_REG_SET32(MMSYS_DUMMY_REG, ~0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pollingVal, ~0);
	cmdqRecFlush(handle);

	CMDQ_MSG("=========== okay case ===========\n");
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	/* clear up */
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_write_stress_test(void)
{
	int32_t loop;

	CMDQ_MSG("%s\n", __func__);

	loop = 1;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	loop = 100;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_multiple_command(void)
{
/* define #define CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND in cmdq_core.c for debug */
#define LOOP 2

	int32_t i;
	int32_t ret;
	cmdqRecHandle handle[LOOP] = {0};
	TaskStruct *pTask[LOOP] = { 0 };

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for(i = 0; i < LOOP; i++)
	{
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, LOOP);

		cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &(handle[i]));
		cmdqRecReset(handle[i]);
		cmdqRecSetSecure(handle[i], false);

		/* record instructions which needs prefetch */
		cmdqRecEnablePrefetch(handle[i]);
		cmdqRecWait(handle[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdqRecDisablePrefetch(handle[i]);

		/* record instructions which does not need prefetch */
		cmdqRecWrite(handle[i], CMDQ_TEST_MMSYS_DUMMY_PA, 0x3000, ~0);

		cmdq_rec_finalize_command(handle[i], false);
		cmdqRecDumpCommand(handle[i]);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	for (i = 0; i < LOOP; ++i) {
		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdqRecDestroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static int _testcase_concurrency(void *data)
{
	uint32_t securePath;
	securePath = *((uint32_t*)data);

	CMDQ_MSG("start secure(%d) path\n", securePath);
	_testcase_simplest_command_loop_submit(
		1000, CMDQ_SCENARIO_DEBUG, (0x1 << CMDQ_ENG_MDP_RSZ0), securePath);

	return 0;
}

static void testcase_concurrency_for_normal_path_and_secure_path(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const uint32_t securePath[2] = {0, 1};

	CMDQ_MSG("%s\n", __func__);

	pKThread1 = kthread_run(_testcase_concurrency, (void *)(&securePath[0]), "cmdqNormal");
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create cmdqNormal failed\n");
		return;
	}

	pKThread2 = kthread_run(_testcase_concurrency, (void *)(&securePath[1]), "cmdqSecure");
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create cmdqSecure failed\n");
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0x0, false);

	CMDQ_MSG("%s END\n", __func__);

	return;
#endif
}

void testcase_async_write_stress_test(void)
{
#if 0
#define LOOP 100

	int32_t i;
	int32_t ret;
	cmdqRecHandle handle[LOOP] = {0};
	TaskStruct *pTask[LOOP] = { 0 };

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for(i = 0; i < LOOP; i++)
	{
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, LOOP);

		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &(handle[i]));
		cmdqRecReset(handle[i]);
		cmdqRecSetSecure(handle[i], gCmdqTestSecure);

		cmdqRecWait(handle[i], CMDQ_SYNC_TOKEN_USER_0);

		cmdq_rec_finalize_command(handle[i], false);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	/* release token and wait them */
	for (i = 0; i < LOOP; ++i) {

		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdqRecDestroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
#endif
}

static void testcase_nonsuspend_irq(void)
{
	cmdqRecHandle handle, handle2;
	TaskStruct *pTask, *pTask2;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	
	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);
	/* use CMDQ to set to PATTERN */
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle, false);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdqRecReset(handle2);
	cmdqRecSetSecure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	/* force GCE to wait in second command before EOC */
	cmdqRecWait(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle2, false);
	
	_test_submit_async(handle, &pTask);
	_test_submit_async(handle2, &pTask2);

	msleep_interruptible(500);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* test code: use to trigger GCE continue test command, put in cmdq_core::handleIRQ to test*/
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_MSG("IRQ: After set user sw token\n");

	cmdqCoreWaitAndReleaseTask(pTask, 500);	
	cmdqCoreWaitAndReleaseTask(pTask2, 500);	
	cmdqRecDestroy(handle);
	cmdqRecDestroy(handle2);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}


typedef enum CMDQ_TESTCASE_ENUM {
	CMDQ_TESTCASE_ALL = 0,
	CMDQ_TESTCASE_BASIC = 1,
	CMDQ_TESTCASE_ERROR = 2,
	CMDQ_TESTCASE_READ_REG_REQUEST,	/* user request get some registers' value when task execution */
	CMDQ_TESTCASE_GPR,
	CMDQ_TESTCASE_SW_TIMEOUT_HANDLE,

	CMDQ_TESTCASE_END,	/* always at the end */
} CMDQ_TESTCASE_ENUM;

ssize_t cmdq_test_proc(struct file *fp, char __user *u, size_t s, loff_t *l)
{
	uint32_t testId;
	bool isSecureTest;

	mutex_lock(&gCmdqTestProcLock);
	smp_mb();

	CMDQ_LOG("[TESTCASE]CONFIG: test case: %d, secure test: %d, gCmdqTestSecure:%d\n",
		gCmdqTestConfig[0], gCmdqTestConfig[1], gCmdqTestSecure);
	testId = gCmdqTestConfig[0];
	isSecureTest = (0 < gCmdqTestConfig[1]) ? (true) : (false);
	mutex_unlock(&gCmdqTestProcLock);

	/* trigger test case here */
	CMDQ_MSG("//\n//\n//\ncmdq_test_proc\n");

	cmdq_test_setup();
	switch (gCmdqTestConfig[0]) {
	case 110:
		testcase_nonsuspend_irq();
		break;
	case 109: 
		testcase_estimate_command_exec_time();
		break;
	case 108:
		testcase_profile_marker();
		break;
	case 107:
		testcase_prefetch_multiple_command();
		break;
	case 106:
		testcase_concurrency_for_normal_path_and_secure_path();
		break;
	case 105:
		testcase_async_write_stress_test();
		break;
	case 104:
		testcase_submit_after_error_happened();
		break;
	case 103:
		testcase_secure_meta_data();
		break;
	case 102:
		testcase_secure_disp_scenario();
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
	case 93:
		testcase_async_suspend_resume();
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
	case 89:
		testcase_trigger_thread();
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
	case 80:
		testcase_clkmgr();
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
	case 76:
		testcase_emergency_buffer();
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
	case CMDQ_TESTCASE_ERROR:
		testcase_errors();
		break;
	case CMDQ_TESTCASE_BASIC:
		testcase_write();
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
	case CMDQ_TESTCASE_ALL:
	default:
		testcase_multiple_async_request();
		testcase_read_to_data_reg();
		testcase_get_result();
		testcase_errors();

		testcase_scenario();
		testcase_sync_token();

		testcase_write();
		testcase_poll();
		testcase_write_address();
		testcase_async_request();
		testcase_async_suspend_resume();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_trigger_thread();
		testcase_prefetch();

		/* testcase_sync_token_threaded(); */

		testcase_long_command();

		/* testcase_clkmgr(); */
		testcase_dram_access();
		testcase_perisys_apb();
		testcase_backup_register();
		testcase_fire_and_forget();

		testcase_backup_reg_to_slot();

		testcase_emergency_buffer();

		testcase_thread_dispatch();
		testcase_full_thread_array();
		testcase_module_full_dump();
		break;
	}

	cmdq_test_cleanup();

	CMDQ_MSG("cmdq_test_proc ended\n");
	return 0;
}

static ssize_t cmdq_write_test_proc_config(struct file *file,
					   const char __user *userBuf, size_t count, loff_t *data)
{
	char desc[10];
	int testType = -1;
	int newTestSuit = -1;
	int32_t len = 0;

	/* copy user input */
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, userBuf, count)) {
		CMDQ_ERR("TEST_CONFIG: data fail\n");
		return 0;
	}
	desc[len] = '\0';

	/* process and update config */
	if (0 >= sscanf(desc, "%d %d", &testType, &newTestSuit)) {
		/* sscanf returns the number of items in argument list successfully filled. */
		CMDQ_ERR("TEST_CONFIG: sscanf failed\n");
		return 0;
	}

	if ((0 > testType) || (2 <= testType) || (-1 == newTestSuit)) {
		CMDQ_ERR("TEST_CONFIG: testType:%d, newTestSuit:%d\n", testType, newTestSuit);
		return 0;
	}

	mutex_lock(&gCmdqTestProcLock);
	smp_mb();

	gCmdqTestConfig[0] = newTestSuit;
	gCmdqTestConfig[1] = testType;
	gCmdqTestSecure = (testType == 1) ? (true) : (false);

	mutex_unlock(&gCmdqTestProcLock);

	return count;
}

static int cmdq_test_open(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static struct file_operations cmdq_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_test_open,
	.read = cmdq_test_proc,
	.write = cmdq_write_test_proc_config,
};

static int __init cmdq_test_init(void)
{
	CMDQ_MSG("cmdq_test_init\n");

	/* Mout proc entry for debug */
	gCmdqTestProcEntry = proc_mkdir("cmdq_test", NULL);
	if (NULL != gCmdqTestProcEntry) {
		if (NULL == proc_create("test", 0660, gCmdqTestProcEntry, &cmdq_fops)) {
			CMDQ_MSG("cmdq_test_init failed\n");
		}
	}

	return 0;
}

static void __exit cmdq_test_exit(void)
{
	CMDQ_MSG("cmdq_test_exit\n");
	if (NULL != gCmdqTestProcEntry) {
		proc_remove(gCmdqTestProcEntry);
		gCmdqTestProcEntry = NULL;
	}
}
module_init(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_LICENSE("GPL");

#else
void testcase_clkmgr_impl(enum cg_clk_id gateId,
			char *name,
			const unsigned long testWriteReg,
			const uint32_t testWriteValue,
			const unsigned long testReadReg,
			const bool verifyWriteResult)
{
}
#endif				/* CMDQ_TEST */
