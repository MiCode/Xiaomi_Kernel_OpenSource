// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>


#include "cmdq-util.h"
#include "cmdq-sec.h"
#include "../../mdp/mdp_cmdq_helper_ext.h"

#ifdef CMDQ_SECURE_MTEE_SUPPORT
#include "tz_m4u.h"
#endif

#define CMDQ_THR_SPR3(base, id)		((base) + (0x80 * (id)) + 0x16c)
#define CMDQ_GPR_R32(base, id)		((base) + (0x4 * (id)) + 0x80)
#define CMDQ_SYNC_TOKEN_UPD(base)	((base) + 0x68)
#define CMDQ_TPR_MASK(base)		((base) + 0xd0)
#define CMDQ_TPR_TIMEOUT_EN(base)	((base) + 0xdc)

#define	CMDQ_TEST_CNT			8

#define CMDQ_GPR_DEBUG_TIMER		CMDQ_GPR_R14
#define CMDQ_GPR_DEBUG_DUMMY		CMDQ_GPR_R15

enum {
	CMDQ_TEST_SUBSYS_GCE,
	CMDQ_TEST_SUBSYS_MMSYS,
	CMDQ_TEST_SUBSYS_NR,
	CMDQ_TEST_SUBSYS_ERR = 99
};

struct test_node {
	struct device	*dev;
	void __iomem	*va;
	phys_addr_t	pa;
	struct clk	*clk;
	struct clk	*clk_timer;
};

struct cmdq_test {
	struct device		*dev;
	struct mutex		lock;
	struct test_node	gce;
	struct test_node	mmsys;
	struct cmdq_client	*clt;
	struct cmdq_client	*loop;
	struct cmdq_client	*sec;
	u32			iter;
	u32			subsys[CMDQ_TEST_SUBSYS_NR];
	struct dentry		*fs;

	bool			tick;
	struct timer_list	timer;

	u16			token_user0;
	u16			token_gpr_set4;
};

static struct cmdq_test		*gtest;

static void cmdq_test_mbox_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion	*cmplt = data.data;

	if (data.err < 0)
		cmdq_err("pkt:%p err:%d", cmplt->pkt, data.err);
	cmplt->err = !data.err ? false : true;
	complete(&cmplt->cmplt);
}

static void cmdq_test_mbox_cb_destroy(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;

	if (data.err < 0)
		cmdq_err("pkt:%p err:%d", pkt, data.err);
	cmdq_pkt_destroy(pkt);
}

static void cmdq_test_mbox_cb_dump(struct cmdq_cb_data data)
{
	cmdq_msg("pkt:0x%p err:%d done", data.data, data.err);
}

static void cmdq_test_mbox_cb_dump_err(struct cmdq_cb_data data)
{
	cmdq_err("pkt:0x%p err:%d during err", data.data, data.err);
}

static void cmdq_test_mbox_err_dump(struct cmdq_test *test, const bool sec)
{
	struct cmdq_pkt *pkt;
	struct cmdq_flush_completion cmplt;
	s32 ret;
	u64 *inst;
	dma_addr_t pc;
	struct cmdq_client *clt = sec ? test->sec : test->clt;

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	cmdq_clear_event(clt->chan, test->token_user0);
	pkt = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (sec)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SEC_DEBUG,
			CMDQ_METAEX_NONE);
#endif

	cmdq_pkt_wfe(pkt, test->token_user0);

	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	cmdq_pkt_flush_async(pkt, cmdq_test_mbox_cb, &cmplt);

	cmdq_thread_dump(clt->chan, pkt, &inst, &pc);
	cmdq_set_event(clt->chan, test->token_user0);

	ret = cmdq_pkt_wait_complete(pkt);
	cmdq_msg("wait complete pkt:0x%p ret:%d", pkt, ret);

	ret = wait_for_completion_timeout(
		&cmplt.cmplt, msecs_to_jiffies(CMDQ_TIMEOUT_DEFAULT));
	if (!ret)
		cmdq_err(
			"wait_for_completion_timeout pkt:0x%p ret:%d inst:0x%016llx pc:%pa",
			pkt, ret, inst ? *inst : 0, &pc);
	else
		cmdq_msg("%s round 1 done", __func__);

	/* second round, use flush async ex with wait, pre-dump and timeout */
	pkt->err_cb.cb = cmdq_test_mbox_cb_dump_err;
	pkt->err_cb.data = pkt;
	cmdq_clear_event(clt->chan, test->token_user0);
	ret = cmdq_pkt_flush_async(pkt, cmdq_test_mbox_cb_dump, (void *)pkt);
	cmdq_msg("flush pkt:0x%p ret:%d", pkt, ret);
	ret = cmdq_pkt_wait_complete(pkt);
	cmdq_msg("wait complete pkt:0x%p ret:%d", pkt, ret);

	cmdq_pkt_destroy(pkt);

	clk_disable_unprepare(test->gce.clk);

	cmdq_msg("%s done", __func__);
}

static void cmdq_test_mbox_gpr_sleep(struct cmdq_test *test, const bool sleep)
{
	struct cmdq_pkt		*pkt;
	struct cmdq_pkt_buffer	*buf;
	struct cmdq_operand	l_op = {.reg = true, .idx = CMDQ_TPR_ID};
	struct cmdq_operand	r_op = {.reg = false, .idx = 100};
	dma_addr_t		out_pa;
	u32			*out_va, gce_time;
	u64			cpu_time;
	const u16		event =
		(u16)CMDQ_EVENT_GPR_TIMER + CMDQ_GPR_DEBUG_TIMER;

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	if (clk_prepare_enable(test->gce.clk_timer)) {
		cmdq_err("clk fail");
		return;
	}

	writel(0x80000000, (void *)CMDQ_TPR_MASK(test->gce.va));
	writel(1 << CMDQ_GPR_DEBUG_TIMER,
		(void *)CMDQ_TPR_TIMEOUT_EN(test->gce.va));

	pkt = cmdq_pkt_create(test->clt);
	if (!sleep) {
		cmdq_pkt_write(pkt, NULL, CMDQ_TPR_TIMEOUT_EN(test->gce.pa),
			1 << CMDQ_GPR_DEBUG_TIMER, 1 << CMDQ_GPR_DEBUG_TIMER);
		cmdq_pkt_clear_event(pkt, event);
	} else
		cmdq_pkt_wfe(pkt, test->token_gpr_set4);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	*out_va = 0;
	*(out_va + 1) = 0;
	*(out_va + 2) = 0;

	if (!sleep) {
		cmdq_pkt_write_indriect(pkt, NULL, out_pa, CMDQ_TPR_ID, ~0);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD,
			CMDQ_GPR_CNT_ID + CMDQ_GPR_DEBUG_TIMER, &l_op, &r_op);
		cmdq_pkt_wfe(pkt, event);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_TPR_ID, ~0);
	} else {
		cmdq_pkt_write_indriect(pkt, NULL, out_pa, CMDQ_TPR_ID, ~0);
		cmdq_pkt_sleep(pkt, 100, CMDQ_GPR_DEBUG_TIMER);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_TPR_ID, ~0);
		cmdq_pkt_set_event(pkt, test->token_gpr_set4);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 8,
			CMDQ_GPR_CNT_ID + CMDQ_GPR_DEBUG_TIMER, ~0);
	}
	cmdq_pkt_dump_buf(pkt, 0);

	cpu_time = sched_clock();
	cmdq_pkt_flush(pkt);
	cpu_time = div_u64(sched_clock() - cpu_time, 1000000);

	if (*out_va <= *(out_va + 1))
		gce_time = *(out_va + 1) - *out_va;
	else
		gce_time = 0xffffffff - *out_va + *(out_va + 1);

	if (cpu_time > 100 || gce_time > 300)
		cmdq_err("sleep:%d cpu:%llu gce:%u out:%u %u %u",
			sleep, cpu_time, gce_time, *out_va, *(out_va + 1),
			*(out_va + 2));
	else
		cmdq_msg("sleep:%d cpu:%llu gce:%u out:%u %u %u",
			sleep, cpu_time, gce_time, *out_va, *(out_va + 1),
			*(out_va + 2));
	cmdq_pkt_destroy(pkt);
	writel(0, (void *)CMDQ_TPR_MASK(test->gce.va));

	clk_disable_unprepare(test->gce.clk_timer);
	clk_disable_unprepare(test->gce.clk);
}

static void cmdq_test_mbox_cpr(struct cmdq_test *test)
{
	unsigned long va = (unsigned long)CMDQ_GPR_R32(test->gce.va,
		CMDQ_GPR_DEBUG_DUMMY);
	unsigned long pa = CMDQ_GPR_R32(test->gce.pa,
		CMDQ_GPR_DEBUG_DUMMY);
	u32 pttn = 0xdeaddead, *buf_va, mark_assign, mark_write;
	s32 i, ret;
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	pkt = cmdq_pkt_create(test->clt);
	mark_assign = pkt->cmd_buf_size >> 2;
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3, pttn);
	mark_write = (pkt->cmd_buf_size + CMDQ_INST_SIZE) >> 2;
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_THR_SPR_IDX3, ~0);
	cmdq_pkt_finalize(pkt);
	cmdq_pkt_dump_buf(pkt, 0);

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	buf_va = (u32 *)buf->va_base;

	/* (1024 - 256 - 4 * 24 - 16) * 2 = 1312 */
	for (i = 0; i < 1312; i++) { // CPR_CNT
		writel(0, (void *)va);
		buf_va[mark_assign + 1] =
			(buf_va[1] & 0xffff0000) | (CMDQ_CPR_STRAT_ID + i);
		buf_va[mark_write] =
			((CMDQ_CPR_STRAT_ID + i) << 16) | (buf_va[5] & 0xffff);

		ret = cmdq_pkt_flush(pkt);
		if (ret)
			cmdq_err("cmdq_pkt_flush failed:%d i:%d", ret, i);

		ret = readl((void *)va);
		if (ret != pttn)
			cmdq_err("ret:%#x not equal to pttn:%#x for idx:%d",
				ret, pttn, i + CMDQ_CPR_STRAT_ID);
		else
			cmdq_msg("ret:%#x equals to pttn:%#x for idx:%d",
				ret, pttn, i + CMDQ_CPR_STRAT_ID);
	}
	cmdq_pkt_destroy(pkt);
	clk_disable_unprepare(test->gce.clk);

	cmdq_msg("%s end", __func__);
}

u32 *cmdq_test_mbox_polling_timeout_unit(struct cmdq_pkt *pkt,
	const unsigned long pa, const u32 pttn, const u32 mask, const bool aee)
{
	struct cmdq_pkt_buffer	*buf;
	u32			*out_va;
	dma_addr_t		out_pa;

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	// last 1k as output buffer
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	*out_va = 0;
	*(out_va + 1) = 0;

	cmdq_pkt_write_indriect(pkt, NULL, out_pa, CMDQ_CPR_STRAT_ID, ~0);
	cmdq_pkt_poll_timeout(pkt, pttn & mask,
		SUBSYS_NO_SUPPORT, pa, mask, aee ? U16_MAX : 100,
		CMDQ_GPR_DEBUG_TIMER);
	cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_CPR_STRAT_ID, ~0);
	return out_va;
}

void cmdq_test_mbox_polling(
	struct cmdq_test *test, const bool secure, const bool timeout,
	const bool aee)
{
	unsigned long	va = (unsigned long)(secure ?
		CMDQ_THR_SPR3(test->gce.va, 3) :
		CMDQ_GPR_R32(test->gce.va, CMDQ_GPR_DEBUG_DUMMY));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_GPR_DEBUG_DUMMY);
	const u32	pttn[CMDQ_TEST_CNT] = {
		0xdada1818, 0xdada1818, 0xdada1818, 0x00001818};
	const u32	mask[CMDQ_TEST_CNT] = {
		0xff00ff00, 0xffffffff, 0x0000ff00, 0xffffffff};

	struct cmdq_client		*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt			*pkt[CMDQ_TEST_CNT];
	u64				cpu_time;
	u32				i, val = 0, *out_va, gce_time;

	cmdq_msg("%s: secure:%d timeout:%d va:%#lx pa:%#lx",
		__func__, secure, timeout, va, pa);

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	if (clk_prepare_enable(test->gce.clk_timer)) {
		cmdq_err("clk fail");
		return;
	}

	writel(0, (void *)va);

	for (i = 0; i < CMDQ_TEST_CNT && pttn[i]; i++) {
		if (timeout)
			writel(0x80000000, (void *)CMDQ_TPR_MASK(test->gce.va));

		pkt[i] = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
		if (secure)
			cmdq_sec_pkt_set_data(pkt[i], 0, 0, CMDQ_SEC_DEBUG,
				CMDQ_METAEX_NONE);
#endif

		cmdq_pkt_wfe(pkt[i], test->token_gpr_set4);
		if (timeout)
			out_va = cmdq_test_mbox_polling_timeout_unit(
				pkt[i], pa, pttn[i], mask[i], aee);
		else
			cmdq_pkt_poll(pkt[i], NULL, pttn[i] & mask[i], pa,
				mask[i], CMDQ_GPR_DEBUG_TIMER);

		cmdq_pkt_set_event(pkt[i], test->token_gpr_set4);

		cpu_time = sched_clock();
		cmdq_pkt_flush_async(pkt[i], NULL, NULL);

		if (!timeout) {
			writel(pttn[i] & mask[i], (void *)va);
			val = readl((void *)va);
		}
		cmdq_pkt_wait_complete(pkt[i]);
		cpu_time = div_u64(sched_clock() - cpu_time, 1000000);

		if (!timeout)
			gce_time = 0;
		else if (*out_va <= *(out_va + 1))
			gce_time = *(out_va + 1) - *out_va;
		else
			gce_time = 0xffffffff - *out_va + *(out_va + 1);

		cmdq_msg("%d: pkt:%p pttn:%#x mask:%#x val:%#x cpu:%llu gce:%u",
			i, pkt[i], pttn[i], mask[i], val, cpu_time, gce_time);
		cmdq_pkt_dump_buf(pkt[i], 0);
		cmdq_pkt_destroy(pkt[i]);

		if (timeout)
			writel(0, (void *)CMDQ_TPR_MASK(test->gce.va));
	}

	clk_disable_unprepare(test->gce.clk_timer);
	clk_disable_unprepare(test->gce.clk);

	cmdq_msg("%s end", __func__);
}

static void cmdq_test_mbox_large_cmd(struct cmdq_test *test)
{
	unsigned long	va = (unsigned long)(CMDQ_GPR_R32(
		test->gce.va, CMDQ_GPR_DEBUG_DUMMY));
	unsigned long	pa = CMDQ_GPR_R32(
		test->gce.pa, CMDQ_GPR_DEBUG_DUMMY);

	struct cmdq_pkt		*pkt;
	s32			i, val;
	bool			perf_en = cmdq_util_is_feature_en((u8)
		CMDQ_LOG_FEAT_PERF);

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	writel(0xdeaddead, (void *)va);

	pkt = cmdq_pkt_create(test->clt);
	if (!perf_en)
		cmdq_pkt_perf_begin(pkt);
	for (i = 0; i < 64 * 1024 / 8; i++) // 64k instructions
		cmdq_pkt_write(pkt, NULL, pa, i, ~0);
	if (!perf_en)
		cmdq_pkt_perf_end(pkt);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);

	val = readl((void *)va);
	clk_disable_unprepare(test->gce.clk);

	if (val != --i)
		cmdq_err("val:%#x not equal to i:%#x", val, i);
	else
		cmdq_msg("val:%#x equals to i:%#x", val, i);
}

static void cmdq_test_mbox_sync_token_loop_iter(struct timer_list *t)
{
	/*struct cmdq_test *test = from_timer(test, t, timer);*/

	if (!gtest->tick)
		del_timer(&gtest->timer);
	else {
		mod_timer(&gtest->timer, jiffies + msecs_to_jiffies(300));
		gtest->iter += 1;
	}
}

static void cmdq_test_mbox_loop(struct cmdq_test *test)
{
	struct cmdq_pkt		*pkt;
	struct cmdq_thread	*thread =
		(struct cmdq_thread *)test->loop->chan->con_priv;
	s32		ret;

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	pkt = cmdq_pkt_create(test->loop);
	cmdq_pkt_wfe(pkt, test->token_user0);
	cmdq_pkt_finalize_loop(pkt);

	cmdq_dump_pkt(pkt, 0, true);

	test->iter = 0;
	test->tick = true;
	timer_setup(&test->timer, cmdq_test_mbox_sync_token_loop_iter,
		0);
	mod_timer(&test->timer, jiffies + msecs_to_jiffies(300));

	writel(test->token_user0,
		(void *)CMDQ_SYNC_TOKEN_UPD(test->gce.va));

	ret = cmdq_pkt_flush_async(pkt, NULL, 0);
	while (test->iter < CMDQ_TEST_CNT) {
		cmdq_msg("loop thrd-idx:%u pkt:%p iter:%u",
			thread->idx, pkt, test->iter);
		msleep_interruptible(1000);
	}

	cmdq_mbox_stop(test->loop);
	clk_disable_unprepare(test->gce.clk);
	test->tick = false;
	del_timer(&test->timer);
}

static void cmdq_test_mbox_dma_access(struct cmdq_test *test, const bool secure)
{
	unsigned long	va = (unsigned long)(secure ?
		CMDQ_THR_SPR3(test->gce.va, 3) :
		CMDQ_GPR_R32(test->gce.va, CMDQ_GPR_DEBUG_DUMMY));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_GPR_DEBUG_DUMMY);
	const u32	ofst = 0xabc, pttn[CMDQ_TEST_CNT] = {
		0xabcdabcd, 0xaabbccdd, 0xdeaddead};

	struct cmdq_client	*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt		*pkt;

	u32		*dma_va;
	dma_addr_t	dma_pa;
	u32		val;

	dma_va = cmdq_mbox_buf_alloc(clt->client.dev, &dma_pa);
	if (!dma_va || !dma_pa) {
		cmdq_err("cmdq_mbox_buf_alloc failed");
		return;
	}
	dma_va[0] = pttn[0];
	dma_va[1] = pttn[0];
	dma_va[2] = pttn[1];

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	writel(pttn[2], (void *)va);

	pkt = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SEC_DEBUG,
			CMDQ_METAEX_NONE);
#endif

	cmdq_pkt_mem_move(pkt, NULL, pa, dma_pa, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, NULL, dma_pa, dma_pa + 4, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, NULL, dma_pa + 8, pa, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_dump_buf(pkt, 0);
	cmdq_pkt_destroy(pkt);

	if (dma_va[0] != pttn[2])
		cmdq_err("move pa:%#x to dma-0:%#x dft:%#x",
			pttn[2], dma_va[0], pttn[0]);
	if (dma_va[1] != pttn[2])
		cmdq_err("move dma-0:%#x to dma-1:%#x dft:%#x",
			pttn[2], dma_va[1], pttn[0]);
	val = readl((void *)va);
	if (dma_va[2] != val)
		cmdq_err("move dma-2:%#x to pa:%#x dft:%#x",
			dma_va[2], val, pttn[2]);

	pkt = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SEC_DEBUG,
			CMDQ_METAEX_NONE);
#endif

	cmdq_pkt_jump(pkt, 8);
	cmdq_pkt_write_value_addr(pkt, dma_pa + ofst, pttn[2], ~0);
	cmdq_pkt_read_addr(pkt, dma_pa + ofst, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_write_reg_addr(pkt, dma_pa + ofst + 4, CMDQ_THR_SPR_IDX1, ~0);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_dump_buf(pkt, 2);
	cmdq_pkt_destroy(pkt);

	if (dma_va[ofst / 4] != dma_va[ofst / 4 + 1] ||
		dma_va[ofst / 4 + 1] != pttn[2])
		cmdq_err("pa:%pa offset:%#x va:%p val:%#x %#x pttn:%#x",
			&dma_pa, ofst, &dma_va[ofst / 4], dma_va[ofst / 4],
			dma_va[ofst / 4 + 1], pttn[2]);

	clk_disable_unprepare(test->gce.clk);
	cmdq_mbox_buf_free(clt->client.dev, dma_va, dma_pa);
	cmdq_msg("%s done", __func__);
}

static void cmdq_test_mbox_sync_token_flush(struct timer_list *t)
{
	u32	val;
	struct cmdq_test *test = from_timer(test, t, timer);

	if (clk_prepare_enable(gtest->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	writel((1L << 16) | test->token_user0,
			(void *)CMDQ_SYNC_TOKEN_UPD(gtest->gce.va));
	val = readl((void *)CMDQ_SYNC_TOKEN_UPD(gtest->gce.va));
	cmdq_log("data:%#hx event:%#x val:%#x",
			test->token_user0, (1 << 16), val);

	if (!gtest->tick)
		del_timer(&gtest->timer);
	else
		mod_timer(&gtest->timer, jiffies + msecs_to_jiffies(10));

	clk_disable_unprepare(gtest->gce.clk);
}

void cmdq_test_mbox_flush(
	struct cmdq_test *test, const s32 secure, const bool threaded)
{
	struct cmdq_client		*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt			*pkt[CMDQ_TEST_CNT] = {0};
	s32				i, err;

	cmdq_msg("%s sec:%d threaded:%d", __func__, secure, threaded);

	test->tick = true;
	timer_setup(&test->timer, cmdq_test_mbox_sync_token_flush,
			0);
	mod_timer(&test->timer, jiffies + msecs_to_jiffies(10));

	for (i = 0; i < CMDQ_TEST_CNT; i++) {
		pkt[i] = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
		if (secure) {
			cmdq_sec_pkt_set_data(pkt[i], 0, 0, CMDQ_SEC_DEBUG,
				CMDQ_METAEX_NONE);
#ifdef CMDQ_SECURE_MTEE_SUPPORT
			if (!~secure)
				cmdq_sec_pkt_set_mtee(pkt[i], true, SEC_ID_SVP);
#endif
		}
#endif

		cmdq_pkt_wfe(pkt[i], test->token_user0);
		pkt[i]->priority = i;

		if (!threaded)
			cmdq_pkt_flush_async(pkt[i], NULL, NULL);
		else
			cmdq_pkt_flush_threaded(pkt[i],
				cmdq_test_mbox_cb_destroy, (void *)pkt[i]);
	}

	for (i = 0; i < CMDQ_TEST_CNT; i++) {
		if (!pkt[i]) {
			cmdq_err("NULL pkt:%d", i);
			continue;
		}
		msleep_interruptible(100);
		if (!threaded)
			err = cmdq_pkt_wait_complete(pkt[i]);
		else
			err = 0;
		if (err < 0) {
			cmdq_err("wait complete pkt[%d]:%p err:%d",
				i, pkt[i], err);
			continue;
		}
		if (!threaded)
			cmdq_pkt_destroy(pkt[i]);
	}

	test->tick = false;
	del_timer(&test->timer);

	cmdq_msg("%s end", __func__);
}

static void cmdq_test_mbox_write(
	struct cmdq_test *test, const s32 secure, const bool need_mask)
{
	const u32	mask = need_mask ? (1 << 16) : ~0;
	const u32	pttn = (1 << 0) | (1 << 2) | (1 << 16);
	unsigned long	va = (unsigned long)(secure ?
		CMDQ_THR_SPR3(test->gce.va, 3) :
		CMDQ_GPR_R32(test->gce.va, CMDQ_GPR_DEBUG_DUMMY));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_GPR_DEBUG_DUMMY);

	struct cmdq_client	*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt		*pkt;
	s32			val;

	cmdq_msg("sec:%d va:%#lx pa:%#lx pttn:%#x mask:%#x clt:%p",
		secure, va, pa, pttn, mask, clt);

	if (clk_prepare_enable(test->gce.clk)) {
		cmdq_err("clk fail");
		return;
	}

	writel(0, (void *)va);

	pkt = cmdq_pkt_create(clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure) {
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SEC_DEBUG,
			CMDQ_METAEX_NONE);
#ifdef CMDQ_SECURE_MTEE_SUPPORT
		if (!~secure)
			cmdq_sec_pkt_set_mtee(pkt, true, SEC_ID_SVP);
#endif
	}
#endif

	cmdq_pkt_write(pkt, NULL, pa, pttn, mask);
	cmdq_pkt_flush(pkt);

	val = readl((void *)va);
	if (val != (pttn & mask)) {
		cmdq_err("wrong val:%#x ans:%#x", val, pttn & mask);
		cmdq_pkt_dump_buf(pkt, 0);
	} else
		cmdq_msg("right val:%#x ans:%#x", val, pttn & mask);

	cmdq_pkt_destroy(pkt);

	clk_disable_unprepare(test->gce.clk);

	cmdq_msg("%s end", __func__);
}

static void cmdq_test_mbox_handshake_event(struct cmdq_test *test)
{
	struct cmdq_client *clt1 = test->clt, *clt2 = test->loop;
	struct cmdq_pkt *pkt_wait, *pkt_shake;
	int ret;

	if (cmdq_mbox_get_base_pa(clt1->chan) ==
		cmdq_mbox_get_base_pa(clt2->chan)) {
		cmdq_msg("no handshake for 2 same client");
		return;
	}

	pkt_wait = cmdq_pkt_create(clt1);
	cmdq_pkt_wfe(pkt_wait, CMDQ_EVENT_HANDSHAKE);
	cmdq_pkt_handshake_event(pkt_wait, CMDQ_EVENT_HANDSHAKE + 1);

	pkt_shake = cmdq_pkt_create(clt2);
	cmdq_pkt_handshake_event(pkt_shake, CMDQ_EVENT_HANDSHAKE);
	cmdq_pkt_wfe(pkt_shake, CMDQ_EVENT_HANDSHAKE + 1);

	cmdq_pkt_flush_async(pkt_wait, NULL, NULL);
	ret = cmdq_pkt_flush(pkt_shake);

	cmdq_pkt_wait_complete(pkt_wait);

	if (ret < 0)
		cmdq_err("shake event fail:%d", ret);

	cmdq_msg("%s end", __func__);
}

u32 cmdq_test_get_subsys_list(u32 **regs_out);

static void cmdq_access_sub_impl(struct cmdq_test *test,
	struct cmdq_client *clt, const char *tag)
{
	struct cmdq_pkt *pkt;
	u32 *regs, count, *va, i;
	dma_addr_t pa;
	u8 swap_reg = CMDQ_THR_SPR_IDX1;
	u32 pat_init = 0xdeaddead, pat_src = 0xbeefbeef;

	va = cmdq_mbox_buf_alloc(clt->client.dev, &pa);
	if (!va) {
		cmdq_err("cmdq_mbox_buf_alloc failed");
		return;
	}
	count = cmdq_test_get_subsys_list(&regs);

	for (i = 0; i < count; i++) {
		va[0] = pat_init;

		pkt = cmdq_pkt_create(test->clt);
		cmdq_pkt_write_value_addr(pkt, regs[i], pat_src, ~0);
		cmdq_pkt_mem_move(pkt, NULL, regs[i], pa, swap_reg);
		cmdq_pkt_flush(pkt);

		if (va[0] != pat_src)
			cmdq_err(
				"%s access reg fail addr:%#x val:%#x should be:%#x",
				tag, regs[i], va[0], pat_src);

		cmdq_pkt_destroy(pkt);
	}

	cmdq_mbox_buf_free(test->clt->client.dev, va, pa);
	cmdq_msg("%s end", __func__);
}

static void cmdq_test_mbox_subsys_access(struct cmdq_test *test)
{
	cmdq_access_sub_impl(test, test->clt, "clt");
	cmdq_access_sub_impl(test, test->loop, "loop");
}

static void cmdq_test_err_irq(struct cmdq_test *test)
{
	struct cmdq_pkt *pkt;
	struct cmdq_pkt_buffer *buf;
	u64 *inst;

	pkt = cmdq_pkt_create(test->clt);
	cmdq_pkt_jump(pkt, 0);

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	inst = (u64 *)buf->va_base;
	*inst = 0xffffbeefdeadbeef;

	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);

	cmdq_msg("%s end", __func__);
}

static void cmdq_test_devapc_vio(struct cmdq_test *test)
{
	struct cmdq_pkt *pkt;
	int ret;

	cmdq_msg("%s", __func__);

	pkt = cmdq_pkt_create(test->clt);
	cmdq_pkt_read(pkt, NULL, 0x14000000, CMDQ_THR_SPR_IDX3);
	ret = cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);

	cmdq_msg("%s ret:%d end", __func__, ret);
}

static void cmdq_test_mbox_stop(struct cmdq_test *test)
{
	struct cmdq_pkt *pkt[3];
	int i;

	cmdq_msg("%s", __func__);

	for (i = 0; i < ARRAY_SIZE(pkt); i++) {
		pkt[i] = cmdq_pkt_create(test->clt);
		cmdq_pkt_wfe(pkt[i], test->token_user0);
		cmdq_pkt_flush_async(pkt[i], NULL, NULL);
	}

	cmdq_msg("%s stop channel", __func__);
	cmdq_mbox_channel_stop(test->clt->chan);

	cmdq_msg("%s still wait it", __func__);
	for (i = 0; i < ARRAY_SIZE(pkt); i++) {
		cmdq_pkt_wait_complete(pkt[i]);
		cmdq_pkt_destroy(pkt[i]);
	}

	cmdq_msg("%s end", __func__);
}

static void cmdq_test_show_events(struct cmdq_test *test)
{
	u32 i;

	cmdq_msg("%s scan all active event ...", __func__);
	cmdq_mbox_enable(test->clt->chan);

	for (i = 0; i < CMDQ_EVENT_MAX; i++)
		if (cmdq_get_event(test->clt->chan, i))
			cmdq_msg("event set:%u", i);
	cmdq_mbox_disable(test->clt->chan);
	cmdq_msg("%s end", __func__);
}

static void
cmdq_test_trigger(struct cmdq_test *test, const s32 sec, const s32 id)
{
	struct cmdq_thread	*thread_clt =
		(struct cmdq_thread *)test->clt->chan->con_priv;
	s32 backup_clt = cmdq_thread_timeout_backup(thread_clt, CMDQ_TIMEOUT_DEFAULT);
	struct cmdq_thread	*thread_loop =
		(struct cmdq_thread *)test->loop->chan->con_priv;
	s32 backup_loop = cmdq_thread_timeout_backup(thread_loop, CMDQ_NO_TIMEOUT);
#ifdef CMDQ_SECURE_SUPPORT
	struct cmdq_thread	*thread_sec =
		(struct cmdq_thread *)test->sec->chan->con_priv;
	s32 backup_sec = cmdq_thread_timeout_backup(thread_sec, CMDQ_TIMEOUT_DEFAULT);
#endif

#ifndef CMDQ_SECURE_SUPPORT
	if (sec) {
		cmdq_err("CMDQ_SECURE not support");
		return;
	}
#endif
	switch (id) {
	case 0:
		cmdq_test_mbox_write(test, sec, false);
		cmdq_test_mbox_write(test, sec, true);
		cmdq_test_mbox_flush(test, sec, false);
		cmdq_test_mbox_flush(test, sec, true);
		cmdq_test_mbox_polling(test, sec, false, false);
		cmdq_test_mbox_dma_access(test, sec);
		cmdq_test_mbox_gpr_sleep(test, false);
		cmdq_test_mbox_gpr_sleep(test, true);
		cmdq_test_mbox_loop(test);
		cmdq_test_mbox_large_cmd(test);
		cmdq_test_mbox_cpr(test);
		break;
	case 1:
		cmdq_test_mbox_write(test, sec, false);
		cmdq_test_mbox_write(test, sec, true);
		break;
	case 2:
		cmdq_test_mbox_flush(test, sec, false);
		cmdq_test_mbox_flush(test, sec, true);
		break;
	case 3:
		cmdq_test_mbox_polling(test, sec, false, false);
		cmdq_test_mbox_polling(test, sec, true, false);
		break;
	case 4:
		cmdq_test_mbox_dma_access(test, sec);
		break;
	case 5:
		cmdq_test_mbox_gpr_sleep(test, false);
		cmdq_test_mbox_gpr_sleep(test, true);
		break;
	case 6:
		cmdq_test_mbox_loop(test);
		break;
	case 7:
		cmdq_test_mbox_large_cmd(test);
		break;
	case 8:
		cmdq_test_mbox_cpr(test);
		break;
	case 9:
		cmdq_test_mbox_err_dump(test, sec);
		break;
	case 10:
		cmdq_test_mbox_handshake_event(test);
		break;
	case 11:
		cmdq_test_mbox_subsys_access(test);
		break;
	case 12:
		cmdq_test_err_irq(test);
		break;
	case 13:
		cmdq_test_devapc_vio(test);
		break;
	case 14:
		cmdq_test_mbox_polling(test, sec, true, true);
		break;
	case 15:
		cmdq_test_mbox_stop(test);
		break;
	case 16:
		cmdq_test_show_events(test);
		break;
	default:
		break;
	}
	cmdq_thread_timeout_restore(thread_clt, backup_clt);
	cmdq_thread_timeout_restore(thread_loop, backup_loop);
#ifdef CMDQ_SECURE_SUPPORT
	cmdq_thread_timeout_restore(thread_sec, backup_sec);
#endif

}

#define MAX_SCAN 30

static ssize_t
cmdq_test_write(struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	struct cmdq_test *test = (struct cmdq_test *)filp->f_inode->i_private;
	char		str[MAX_SCAN] = {0};
	s32		sec, id = 0;
	u32		len;

	len = (count < MAX_SCAN - 1) ? count : (MAX_SCAN - 1);
	if (copy_from_user(str, buf, len)) {
		cmdq_err("copy_from_user failed len:%d", len);
		return count;
	}
	str[len] = '\0';

	if (sscanf(str, "%d %d", &sec, &id) != 2) {
		cmdq_err("sscanf failed str:%s sec:%d id:%d", str, sec, id);
		return count;
	}
	cmdq_msg("test:%p len:%d sec:%d id:%d str:%s", test, len, sec, id, str);

	mutex_lock(&test->lock);
	cmdq_test_trigger(test, sec, id);
	mutex_unlock(&test->lock);
	return count;
}

static const struct file_operations cmdq_test_fops = {
	.write = cmdq_test_write,
};

static s32 cmdq_test_client_get(struct cmdq_client **clt, const u32 end_idx)
{
	struct cmdq_client *mdp = NULL;
	s32 i;

	for (i = end_idx; i >= 0 && !mdp; i--)
		mdp = cmdq_helper_mbox_client(i);
	if (mdp)
		*clt = mdp;
	return i;
}

static int cmdq_test_probe(struct platform_device *pdev)
{
	struct cmdq_test	*test;
	struct device_node	*np;
	struct platform_device	*np_pdev;
	struct resource		res;
	struct dentry		*dir;
	s32			i, ret;

	test = devm_kzalloc(&pdev->dev, sizeof(*test), GFP_KERNEL);
	if (!test)
		return -ENOMEM;
	test->dev = &pdev->dev;
	mutex_init(&test->lock);
	test->tick = false;
	gtest = test;

	// gce
	np = of_parse_phandle(pdev->dev.of_node, "mediatek,gce", 0);
	if (!np) {
		cmdq_err("of_parse_phandle mediatek,gce failed");
		return -EINVAL;
	}

	np_pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!np_pdev)
		return -EINVAL;
	test->gce.dev = &np_pdev->dev;

	test->gce.va = of_iomap(np_pdev->dev.of_node, 0);
	if (!test->gce.va)
		return -EINVAL;

	ret = of_address_to_resource(np_pdev->dev.of_node, 0, &res);
	if (ret)
		return ret;
	test->gce.pa = res.start;

	test->gce.clk = devm_clk_get(&np_pdev->dev, "gce");
	if (IS_ERR(test->gce.clk)) {
		cmdq_err("devm_clk_get gce clk failed:%ld",
			PTR_ERR(test->gce.clk));
		test->gce.clk = NULL;
	}

	test->gce.clk_timer = devm_clk_get(&np_pdev->dev, "gce-timer");
	if (IS_ERR(test->gce.clk_timer)) {
		cmdq_err("devm_clk_get gce clk_timer failed:%ld",
			PTR_ERR(test->gce.clk_timer));
		test->gce.clk_timer = NULL;
	}
	cmdq_msg("gce dev:%p va:%p pa:%pa",
		test->gce.dev, test->gce.va, &test->gce.pa);

	// mmsys
	np = of_parse_phandle(pdev->dev.of_node, "mmsys_config", 0);
	if (!np) {
		cmdq_err("of_parse_phandle mmsys_config failed");
		return -EINVAL;
	}

	np_pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!np_pdev) {
		cmdq_err("of_find_device_by_node to mmsys_config failed");
		return -EINVAL;
	}
	test->mmsys.dev = &np_pdev->dev;

	test->mmsys.va = of_iomap(np_pdev->dev.of_node, 0);
	if (!test->mmsys.va)
		return -EINVAL;

	ret = of_address_to_resource(np_pdev->dev.of_node, 0, &res);
	if (ret) {
		cmdq_err(
			"of_address_to_resource to mmsys_config failed ret:%d",
			ret);
		return ret;
	}
	test->mmsys.pa = res.start;
	cmdq_msg("mmsys dev:%p va:%p pa:%pa",
		test->mmsys.dev, test->mmsys.va, &test->mmsys.pa);

	// clt
	ret = CMDQ_MAX_THREAD_COUNT;
	test->clt = cmdq_mbox_create(&pdev->dev, 0);
	if (IS_ERR(test->clt) || !test->clt) {
		ret = cmdq_test_client_get(&test->clt, ret - 1);
		if (!test->clt)
			return -ENXIO;
	}

	test->loop = cmdq_mbox_create(&pdev->dev, 1);
	if (IS_ERR(test->loop) || !test->loop) {
		ret = cmdq_test_client_get(&test->loop, ret - 1);
		if (!test->loop)
			return -ENXIO;
	}

#ifdef CMDQ_SECURE_SUPPORT
	test->sec = cmdq_mbox_create(&pdev->dev, 2);
	if (IS_ERR(test->sec) || !test->sec) {
		ret = cmdq_test_client_get(&test->sec, 10);
		if (!test->sec)
			return -ENXIO;
	}
#endif
	cmdq_msg("%s test:%p dev:%p clt:%p loop:%p sec:%p",
		__func__, test, test->dev, test->clt, test->loop, test->sec);

	// subsys
	i = of_property_count_u32_elems(
		pdev->dev.of_node, "mediatek,gce-subsys");
	if (i < 0) {
		cmdq_err("of_property_count_u32_elems gce-subsys failed:%d", i);
		for (i = 0; i < CMDQ_TEST_SUBSYS_NR; i++)
			test->subsys[i] = CMDQ_TEST_SUBSYS_ERR;
	} else {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"mediatek,gce-subsys", test->subsys, i);
		if (ret) {
			cmdq_err("of_property_read_u32_array failed:%d", ret);
			for (i = 0; i < CMDQ_TEST_SUBSYS_NR; i++)
				test->subsys[i] = CMDQ_TEST_SUBSYS_ERR;
		}
	}
	for (i = 0; i < CMDQ_TEST_SUBSYS_NR; i++)
		cmdq_msg("subsys[%d]:%u", i, test->subsys[i]);

	ret = of_property_read_u16(pdev->dev.of_node, "token_user0",
		&test->token_user0);
	if (ret < 0) {
		cmdq_err("no token_user0 err:%d", ret);
		test->token_user0 = CMDQ_EVENT_MAX;
	}

	ret = of_property_read_u16(pdev->dev.of_node, "token_gpr_set4",
		&test->token_gpr_set4);
	if (ret < 0) {
		cmdq_err("no token_gpr_set4 err:%d", ret);
		test->token_gpr_set4 = CMDQ_EVENT_MAX;
	}

	// fs
	dir = debugfs_create_dir("cmdq", NULL);
	if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
		cmdq_err("debugfs_create_dir cmdq failed:%ld", PTR_ERR(dir));
		return PTR_ERR(dir);
	}

	test->fs = debugfs_create_file(
		"cmdq-test", 0444, dir, test, &cmdq_test_fops);
	if (IS_ERR(test->fs)) {
		cmdq_err("debugfs_create_file cmdq-test failed:%ld",
			PTR_ERR(test->fs));
		return PTR_ERR(test->fs);
	}

	platform_set_drvdata(pdev, test);

	return 0;
}

static int cmdq_test_remove(struct platform_device *pdev)
{
	struct cmdq_test *test = (struct cmdq_test *)platform_get_drvdata(pdev);

	cmdq_mbox_destroy(test->clt);
	cmdq_mbox_destroy(test->loop);
	cmdq_mbox_destroy(test->sec);
	return 0;
}

static const struct of_device_id cmdq_test_of_ids[] = {
	{
		.compatible = "mediatek,cmdq-test",
	},
	{}
};
MODULE_DEVICE_TABLE(of, cmdq_test_of_ids);

static struct platform_driver cmdq_test_drv = {
	.probe = cmdq_test_probe,
	.remove = cmdq_test_remove,
	.driver = {
		.name = "cmdq-test",
		.of_match_table = cmdq_test_of_ids,
	},
};

static int __init cmdq_test_init(void)
{
	return platform_driver_register(&cmdq_test_drv);
}

static void __exit cmdq_test_exit(void)
{
	return platform_driver_unregister(&cmdq_test_drv);
}

device_initcall_sync(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_DESCRIPTION("MEDIATEK Module Cmdq-test driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
