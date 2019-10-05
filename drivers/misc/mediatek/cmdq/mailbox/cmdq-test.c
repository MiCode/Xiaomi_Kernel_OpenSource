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

#include "cmdq-sec.h"

#define CMDQ_THR_SPR3(base, id)		((base) + (0x80 * (id)) + 0x16c)
#define CMDQ_GPR_R32(base, id)		((base) + (0x4 * (id)) + 0x80)
#define CMDQ_SYNC_TOKEN_UPD(base)	((base) + 0x68)
#define CMDQ_TPR_MASK(base)		((base) + 0xd0)
#define CMDQ_TPR_TIMEOUT_EN(base)	((base) + 0xdc)

#define	CMDQ_TEST_CNT			(16)

#define CMDQ_SYNC_TOKEN_USER_0		(649)
#define CMDQ_SYNC_TOKEN_GPR_SET_4	(704)

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
	struct cmdq_flush_completion	*cmplt = data.data;

	if (data.err < 0)
		cmdq_err("pkt:%p err:%d", cmplt->pkt, data.err);
	cmplt->err = !data.err ? false : true;
	cmdq_pkt_destroy(cmplt->pkt);
	complete(&cmplt->cmplt);
}

static void cmdq_test_mbox_err_dump(struct cmdq_test *test)
{
	struct cmdq_pkt			*pkt;
	struct cmdq_flush_completion	cmplt;
	s32				ret;

	clk_prepare_enable(test->gce.clk);

	cmdq_clear_event(test->clt->chan, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_pkt_cl_create(&pkt, test->clt);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_USER_0);

	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	cmdq_pkt_flush_async(test->clt, pkt, cmdq_test_mbox_cb, &cmplt);

	cmdq_thread_dump_err(test->clt->chan);

	cmdq_set_event(test->clt->chan, CMDQ_SYNC_TOKEN_USER_0);
	ret = wait_for_completion_timeout(
		&cmplt.cmplt, msecs_to_jiffies(CMDQ_TIMEOUT_DEFAULT));
	if (!ret)
		cmdq_err("wait_for_completion_timeout pkt:%p", pkt);
	else {
		cmdq_msg("%s done", __func__);
		cmdq_pkt_destroy(pkt);
	}
	clk_disable_unprepare(test->gce.clk);
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
		(u16)GCE_TOKEN_GPR_TIMER + CMDQ_DATA_REG_DEBUG;

	clk_prepare_enable(test->gce.clk);
	clk_prepare_enable(test->gce.clk_timer);
	writel(0x80000000, (void *)CMDQ_TPR_MASK(test->gce.va));
	writel(1 << CMDQ_DATA_REG_DEBUG,
		(void *)CMDQ_TPR_TIMEOUT_EN(test->gce.va));

	cmdq_pkt_cl_create(&pkt, test->clt);
	if (!sleep) {
		cmdq_pkt_write(pkt, NULL, CMDQ_TPR_TIMEOUT_EN(test->gce.pa),
			1 << CMDQ_DATA_REG_DEBUG, 1 << CMDQ_DATA_REG_DEBUG);
		cmdq_pkt_clear_event(pkt, event);
	} else
		cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	out_pa = buf->pa_base + 3096;
	out_va = (u32 *)(buf->va_base + 3096);
	*out_va = 0;
	*(out_va + 1) = 0;
	*(out_va + 2) = 0;

	if (!sleep) {
		cmdq_pkt_write_indriect(pkt, NULL, out_pa, CMDQ_TPR_ID, ~0);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD,
			CMDQ_GPR_CNT_ID + CMDQ_DATA_REG_DEBUG, &l_op, &r_op);
		cmdq_pkt_wfe(pkt, event);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_TPR_ID, ~0);
	} else {
		cmdq_pkt_write_indriect(pkt, NULL, out_pa, CMDQ_TPR_ID, ~0);
		cmdq_pkt_sleep(pkt, 100, CMDQ_DATA_REG_DEBUG);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_TPR_ID, ~0);
		cmdq_pkt_set_event(pkt, CMDQ_SYNC_TOKEN_GPR_SET_4);
		cmdq_pkt_write_indriect(pkt, NULL, out_pa + 8,
			CMDQ_GPR_CNT_ID + CMDQ_DATA_REG_DEBUG, ~0);
	}
	cmdq_pkt_dump_buf(pkt, 0);

	cpu_time = sched_clock();
	cmdq_pkt_flush(test->clt, pkt);
	cpu_time = div_u64(sched_clock() - cpu_time, 1000000);

	if (*out_va <= *(out_va + 1))
		gce_time = *(out_va + 1) - *out_va;
	else
		gce_time = 0xffffffff - *out_va + *(out_va + 1);

	cmdq_msg("sleep:%d cpu:%llu gce:%u out:%u %u %u", sleep,
		cpu_time, gce_time, *out_va, *(out_va + 1), *(out_va + 2));
	cmdq_pkt_destroy(pkt);
	writel(0, (void *)CMDQ_TPR_MASK(test->gce.va));

	clk_disable_unprepare(test->gce.clk_timer);
	clk_disable_unprepare(test->gce.clk);
}

static void cmdq_test_mbox_cpr(struct cmdq_test *test)
{
	unsigned long	va = (unsigned long)CMDQ_GPR_R32(test->gce.va,
		CMDQ_DATA_REG_2D_SHARPNESS_1);
	unsigned long	pa = CMDQ_GPR_R32(test->gce.pa,
		CMDQ_DATA_REG_2D_SHARPNESS_1);
	u32	pttn = 0xdeaddead, *buf_va;
	s32	i, ret;
	struct cmdq_pkt		*pkt;
	struct cmdq_pkt_buffer	*buf;

	clk_prepare_enable(test->gce.clk);

	cmdq_pkt_cl_create(&pkt, test->clt);
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3, pttn);
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_THR_SPR_IDX3, ~0);
	cmdq_pkt_finalize(pkt);
	cmdq_pkt_dump_buf(pkt, 0);

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	buf_va = (u32 *)buf->va_base;

	for (i = 0; i < 0; i++) { // CPR_CNT
		writel(0, (void *)va);
		buf_va[1] = (buf_va[1] & 0xffff0000) | (CMDQ_CPR_STRAT_ID + i);
		buf_va[4] =
			((CMDQ_CPR_STRAT_ID + i) << 16) | (buf_va[5] & 0xffff);

		ret = cmdq_pkt_flush(test->clt, pkt);
		if (ret)
			cmdq_err("cmdq_pkt_flush failed:%d i:%d", ret, i);

		ret = readl((void *)va);
		if (ret != pttn)
			cmdq_err("ret:%#x not equal to pttn:%#x", ret, pttn);
		else
			cmdq_msg("ret:%#x equals to pttn:%#x", ret, pttn);
	}
	cmdq_pkt_destroy(pkt);
	clk_disable_unprepare(test->gce.clk);
}

static u32 *cmdq_test_mbox_polling_timeout_unit(struct cmdq_pkt *pkt,
	const unsigned long pa, const u32 pttn, const u32 mask)
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
		SUBSYS_NO_SUPPORT, pa, mask, 100, CMDQ_DATA_REG_DEBUG);
	cmdq_pkt_write_indriect(pkt, NULL, out_pa + 4, CMDQ_CPR_STRAT_ID, ~0);
	return out_va;
}

static void cmdq_test_mbox_polling(
	struct cmdq_test *test, const bool secure, const bool timeout)
{
	unsigned long	va = (unsigned long)(secure ?
		CMDQ_THR_SPR3(test->gce.va, 3) :
		CMDQ_GPR_R32(test->gce.va, CMDQ_DATA_REG_2D_SHARPNESS_1));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_DATA_REG_2D_SHARPNESS_1);
	const u32	pttn[CMDQ_TEST_CNT] = {
		0xdada1818, 0xdada1818, 0xdada1818, 0x00001818};
	const u32	mask[CMDQ_TEST_CNT] = {
		0xff00ff00, 0xffffffff, 0x0000ff00, 0xffffffff};

	struct cmdq_client		*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt			*pkt[CMDQ_TEST_CNT];
	struct cmdq_flush_completion	cmplt[CMDQ_TEST_CNT];
	u64				cpu_time;
	u32				i, val = 0, *out_va, gce_time;

	cmdq_msg("%s: secure:%d timeout:%d va:%#lx pa:%#lx",
		__func__, secure, timeout, va, pa);

	clk_prepare_enable(test->gce.clk);
	writel(0, (void *)va);

	for (i = 0; i < CMDQ_TEST_CNT && pttn[i]; i++) {
		if (timeout) {
			clk_prepare_enable(test->gce.clk_timer);
			writel(0x80000000, (void *)CMDQ_TPR_MASK(test->gce.va));
		}

		cmdq_pkt_cl_create(&pkt[i], clt);
#ifdef CMDQ_SECURE_SUPPORT
		if (secure)
			cmdq_sec_pkt_set_data(
				pkt[i], 0, 0, CMDQ_SCENARIO_DEBUG);
#endif

		cmdq_pkt_wfe(pkt[i], CMDQ_SYNC_TOKEN_GPR_SET_4);
		if (timeout)
			out_va = cmdq_test_mbox_polling_timeout_unit(
				pkt[i], pa, pttn[i], mask[i]);
		else
			cmdq_pkt_poll(pkt[i], NULL, pttn[i] & mask[i], pa,
				mask[i], CMDQ_DATA_REG_DEBUG);

		cmdq_pkt_set_event(pkt[i], CMDQ_SYNC_TOKEN_GPR_SET_4);
		init_completion(&cmplt[i].cmplt);
		cmplt[i].pkt = pkt[i];

		cpu_time = sched_clock();
		cmdq_pkt_flush_async(clt, pkt[i], cmdq_test_mbox_cb, &cmplt[i]);

		if (!timeout) {
			writel(pttn[i] & mask[i], (void *)va);
			val = readl((void *)va);
		}
		wait_for_completion(&cmplt[i].cmplt);
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

		if (timeout) {
			writel(0, (void *)CMDQ_TPR_MASK(test->gce.va));
			clk_disable_unprepare(test->gce.clk_timer);
		}
	}
	clk_disable_unprepare(test->gce.clk);
}

static void cmdq_test_mbox_large_cmd(struct cmdq_test *test)
{
	unsigned long	va = (unsigned long)(CMDQ_GPR_R32(
		test->gce.va, CMDQ_DATA_REG_2D_SHARPNESS_1));
	unsigned long	pa = CMDQ_GPR_R32(
		test->gce.pa, CMDQ_DATA_REG_2D_SHARPNESS_1);

	struct cmdq_pkt		*pkt;
	s32			i, val;

	clk_prepare_enable(test->gce.clk);
	writel(0xdeaddead, (void *)va);

	cmdq_pkt_cl_create(&pkt, test->clt);
	for (i = 0; i < 64 * 1024 / 8; i++) // 64k instructions
		cmdq_pkt_write(pkt, NULL, pa, i, ~0);
	cmdq_pkt_flush(test->clt, pkt);
	cmdq_pkt_destroy(pkt);

	val = readl((void *)va);
	clk_disable_unprepare(test->gce.clk);

	if (val != --i)
		cmdq_err("val:%#x not equal to i:%#x", val, i);
	else
		cmdq_msg("val:%#x equals to i:%#x", val, i);
}

static void cmdq_test_mbox_sync_token_loop_iter(unsigned long data)
{
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
	struct cmdq_thread	*thrd =
		(struct cmdq_thread *)test->loop->chan->con_priv;
	s32		ret;

	cmdq_pkt_cl_create(&pkt, test->loop);
	cmdq_pkt_wfe(pkt, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_pkt_finalize_loop(pkt);

	cmdq_dump_pkt(pkt);

	test->iter = 0;
	test->tick = true;
	setup_timer(&test->timer, &cmdq_test_mbox_sync_token_loop_iter,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&test->timer, jiffies + msecs_to_jiffies(300));
	clk_prepare_enable(test->gce.clk);
	writel(CMDQ_SYNC_TOKEN_USER_0,
		(void *)CMDQ_SYNC_TOKEN_UPD(test->gce.va));

	ret = cmdq_pkt_flush_async(test->loop, pkt, NULL, 0);
	while (test->iter < CMDQ_TEST_CNT) {
		cmdq_msg("loop thrd_idx:%u pkt:%p iter:%u",
			thrd->idx, pkt, test->iter);
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
		CMDQ_GPR_R32(test->gce.va, CMDQ_DATA_REG_2D_SHARPNESS_1));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_DATA_REG_2D_SHARPNESS_1);
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

	clk_prepare_enable(test->gce.clk);
	writel(pttn[2], (void *)va);

	cmdq_pkt_cl_create(&pkt, clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SCENARIO_DEBUG);
#endif

	cmdq_pkt_mem_move(pkt, NULL, pa, dma_pa, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, NULL, dma_pa, dma_pa + 4, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_mem_move(pkt, NULL, dma_pa + 8, pa, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_flush(clt, pkt);
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

	cmdq_pkt_cl_create(&pkt, clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SCENARIO_DEBUG);
#endif

	cmdq_pkt_jump(pkt, 8);
	cmdq_pkt_write_value_addr(pkt, dma_pa + ofst, pttn[2], ~0);
	cmdq_pkt_read_addr(pkt, dma_pa + ofst, CMDQ_THR_SPR_IDX1);
	cmdq_pkt_write_reg_addr(pkt, dma_pa + ofst + 4, CMDQ_THR_SPR_IDX1, ~0);
	cmdq_pkt_flush(clt, pkt);
	cmdq_pkt_dump_buf(pkt, 2);
	cmdq_pkt_destroy(pkt);

	if (dma_va[ofst / 4] != dma_va[ofst / 4 + 1] ||
		dma_va[ofst / 4 + 1] != pttn[2])
		cmdq_err("pa:%pa va:%p val:%#x %#x pttn:%#x",
			&dma_pa + ofst, &dma_va[ofst / 4], dma_va[ofst / 4],
			dma_va[ofst / 4 + 1], pttn[2]);

	clk_disable_unprepare(test->gce.clk);
	cmdq_mbox_buf_free(clt->client.dev, dma_va, dma_pa);
	cmdq_msg("%s done", __func__);
}

static void cmdq_test_mbox_sync_token_flush(unsigned long data)
{
	u32	val;

	writel((1L << 16) | data, (void *)CMDQ_SYNC_TOKEN_UPD(gtest->gce.va));
	val = readl((void *)CMDQ_SYNC_TOKEN_UPD(gtest->gce.va));
	cmdq_err("data:%#lx event:%#x val:%#x", data, (1 << 16), val);

	if (!gtest->tick)
		del_timer(&gtest->timer);
	else
		mod_timer(&gtest->timer, jiffies + msecs_to_jiffies(10));
}

static void cmdq_test_mbox_flush(
	struct cmdq_test *test, const bool secure, const bool threaded)
{
	struct cmdq_client		*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt			*pkt[CMDQ_TEST_CNT] = {0};
	struct cmdq_flush_completion	cmplt[CMDQ_TEST_CNT];
	s32				i, ret;

	cmdq_msg("sec:%d threaded:%d", secure, threaded);

	test->tick = true;
	setup_timer(&test->timer, &cmdq_test_mbox_sync_token_flush,
		CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&test->timer, jiffies + msecs_to_jiffies(10));
	clk_prepare_enable(test->gce.clk);
	writel(CMDQ_SYNC_TOKEN_USER_0,
		(void *)CMDQ_SYNC_TOKEN_UPD(test->gce.va));

	for (i = 0; i < CMDQ_TEST_CNT; i++) {
		cmdq_pkt_cl_create(&pkt[i], clt);
#ifdef CMDQ_SECURE_SUPPORT
		if (secure)
			cmdq_sec_pkt_set_data(
				pkt[i], 0, 0, CMDQ_SCENARIO_DEBUG);
#endif

		cmdq_pkt_wfe(pkt[i], CMDQ_SYNC_TOKEN_USER_0);
		pkt[i]->priority = i;
		init_completion(&cmplt[i].cmplt);
		cmplt[i].pkt = pkt[i];

		if (!threaded)
			cmdq_pkt_flush_async(clt, pkt[i],
				cmdq_test_mbox_cb, &cmplt[i]);
		else
			cmdq_pkt_flush_threaded(clt, pkt[i],
				cmdq_test_mbox_cb_destroy, &cmplt[i]);
	}

	for (i = 0; i < CMDQ_TEST_CNT; i++) {
		if (!pkt[i]) {
			cmdq_err("NULL pkt:%d", i);
			continue;
		}
		msleep_interruptible(100);
		ret = wait_for_completion_timeout(&cmplt[i].cmplt,
			msecs_to_jiffies(CMDQ_TIMEOUT_DEFAULT));
		if (!ret) {
			cmdq_err("wait_for_completion_timeout pkt[%d]:%p",
				i, pkt[i]);
			continue;
		}
		if (!threaded)
			cmdq_pkt_destroy(pkt[i]);
	}

	writel(CMDQ_SYNC_TOKEN_USER_0,
		(void *)CMDQ_SYNC_TOKEN_UPD(test->gce.va));
	clk_disable_unprepare(test->gce.clk);
	test->tick = false;
	del_timer(&test->timer);
}

static void cmdq_test_mbox_write(
	struct cmdq_test *test, const bool secure, const bool need_mask)
{
	const u32	mask = need_mask ? (1 << 16) : ~0;
	const u32	pttn = (1 << 0) | (1 << 2) | (1 << 16);
	unsigned long	va = (unsigned long)(secure ?
		CMDQ_THR_SPR3(test->gce.va, 3) :
		CMDQ_GPR_R32(test->gce.va, CMDQ_DATA_REG_2D_SHARPNESS_1));
	unsigned long	pa = secure ? CMDQ_THR_SPR3(test->gce.pa, 3) :
		CMDQ_GPR_R32(test->gce.pa, CMDQ_DATA_REG_2D_SHARPNESS_1);

	struct cmdq_client	*clt = secure ? test->sec : test->clt;
	struct cmdq_pkt		*pkt;
	s32			val;

	cmdq_msg("sec:%d va:%#lx pa:%#lx pttn:%#x mask:%#x clt:%p",
		secure, va, pa, pttn, mask, clt);

	clk_prepare_enable(test->gce.clk);
	writel(0, (void *)va);

	cmdq_pkt_cl_create(&pkt, clt);
#ifdef CMDQ_SECURE_SUPPORT
	if (secure)
		cmdq_sec_pkt_set_data(pkt, 0, 0, CMDQ_SCENARIO_DEBUG);
#endif

	cmdq_pkt_write(pkt, NULL, pa, pttn, mask);
	cmdq_pkt_flush(clt, pkt);

	val = readl((void *)va);
	if (val != (pttn & mask)) {
		cmdq_err("wrong val:%#x ans:%#x", val, pttn & mask);
		cmdq_pkt_dump_buf(pkt, 0);
	} else
		cmdq_msg("right val:%#x ans:%#x", val, pttn & mask);

	cmdq_pkt_destroy(pkt);
	clk_disable_unprepare(test->gce.clk);
}

static void cmdq_test_trigger(struct cmdq_test *test, const s32 id)
{
	switch (id < 0 ? -id : id) {
	case 0:
		cmdq_test_mbox_write(test, false, false);
		cmdq_test_mbox_write(test, false, true);
		cmdq_test_mbox_write(test, true, false);
		cmdq_test_mbox_write(test, true, true);

		cmdq_test_mbox_flush(test, false, false);
		cmdq_test_mbox_flush(test, false, true);
		cmdq_test_mbox_flush(test, true, false);
		cmdq_test_mbox_flush(test, true, true);

		cmdq_test_mbox_polling(test, false, false);
		cmdq_test_mbox_polling(test, false, true);
		cmdq_test_mbox_polling(test, true, false);
		cmdq_test_mbox_polling(test, true, true);

		cmdq_test_mbox_dma_access(test, false);
		cmdq_test_mbox_dma_access(test, true);

		cmdq_test_mbox_gpr_sleep(test, false);
		cmdq_test_mbox_gpr_sleep(test, true);

		cmdq_test_mbox_loop(test);
		cmdq_test_mbox_large_cmd(test);

		cmdq_test_mbox_cpr(test);
		cmdq_test_mbox_err_dump(test);
		break;
	case 1:
		cmdq_test_mbox_write(test, id < 0 ? true : false, false);
		cmdq_test_mbox_write(test, id < 0 ? true : false, true);
		break;
	case 2:
		cmdq_test_mbox_flush(test, id < 0 ? true : false, false);
		cmdq_test_mbox_flush(test, id < 0 ? true : false, true);
		break;
	case 3:
		cmdq_test_mbox_polling(test, id < 0 ? true : false, false);
		cmdq_test_mbox_polling(test, id < 0 ? true : false, true);
		break;
	case 4:
		cmdq_test_mbox_dma_access(test, id < 0 ? true : false);
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
		cmdq_test_mbox_err_dump(test);
		break;
	default:
		break;
	}
}

static ssize_t
cmdq_test_write(struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	struct cmdq_test *test = (struct cmdq_test *)filp->f_inode->i_private;
	char		str[MAX_INPUT];
	s32		len, id;

	len = (count < MAX_INPUT - 1) ? count : (MAX_INPUT - 1);
	if (copy_from_user(str, buf, len)) {
		cmdq_err("copy_from_user failed buf:%s len:%d", buf, len);
		return count;
	}
	str[len] = '\0';

	if (kstrtoint(str, 0, &id)) {
		cmdq_err("sscanf failed str:%s id:%d", str, id);
		return count;
	}
	cmdq_msg("test:%p len:%d id:%d str:%s", test, len, id, str);

	mutex_lock(&test->lock);
	cmdq_test_trigger(test, id);
	mutex_unlock(&test->lock);
	return count;
}

static const struct file_operations cmdq_test_fops = {
	.write = cmdq_test_write,
};

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

	test->gce.clk = devm_clk_get(&np_pdev->dev, "GCE");
	if (IS_ERR(test->gce.clk)) {
		cmdq_err("devm_clk_get gce clk failed:%d",
			PTR_ERR(test->gce.clk));
		return PTR_ERR(test->gce.clk);
	}

	test->gce.clk_timer = devm_clk_get(&np_pdev->dev, "GCE_TIMER");
	if (IS_ERR(test->gce.clk_timer)) {
		cmdq_err("devm_clk_get gce clk_timer failed:%d",
			PTR_ERR(test->gce.clk_timer));
		return PTR_ERR(test->gce.clk_timer);
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
	if (!np_pdev)
		return -EINVAL;
	test->mmsys.dev = &np_pdev->dev;

	test->mmsys.va = of_iomap(np_pdev->dev.of_node, 0);
	if (!test->mmsys.va)
		return -EINVAL;

	ret = of_address_to_resource(np_pdev->dev.of_node, 0, &res);
	if (ret)
		return ret;
	test->mmsys.pa = res.start;
	cmdq_msg("mmsys dev:%p va:%p pa:%pa",
		test->mmsys.dev, test->mmsys.va, &test->mmsys.pa);

	// clt
	test->clt = cmdq_mbox_create(&pdev->dev, 0);
	if (IS_ERR(test->clt)) {
		cmdq_err("cmdq_mbox_create failed:%d", PTR_ERR(test->clt));
		return PTR_ERR(test->clt);
	}

	test->loop = cmdq_mbox_create(&pdev->dev, 1);
	if (IS_ERR(test->loop)) {
		cmdq_err("cmdq_mbox_create failed:%d", PTR_ERR(test->loop));
		return PTR_ERR(test->loop);
	}

	test->sec = cmdq_mbox_create(&pdev->dev, 2);
	if (IS_ERR(test->sec)) {
		cmdq_err("cmdq_mbox_create failed:%d", PTR_ERR(test->sec));
		return PTR_ERR(test->sec);
	}
	cmdq_msg("test:%p dev:%p clt:%p loop:%p sec:%p",
		test, test->dev, test->clt, test->loop, test->sec);

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
	for (; i >= 0; i--)
		cmdq_msg("subsys[%d]:%u", i, test->subsys[i]);

	// fs
	dir = debugfs_create_dir("cmdq", NULL);
	if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
		cmdq_err("debugfs_create_dir cmdq failed:%d", PTR_ERR(dir));
		return PTR_ERR(dir);
	}

	test->fs = debugfs_create_file(
		"cmdq-test", 0444, dir, test, &cmdq_test_fops);
	if (IS_ERR(test->fs)) {
		cmdq_err("debugfs_create_file cmdq-test failed:%d",
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
module_platform_driver(cmdq_test_drv);
