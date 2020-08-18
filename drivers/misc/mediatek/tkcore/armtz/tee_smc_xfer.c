/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include <linux/version.h>

#include <linux/tee_clkmgr.h>

#include <tee_kernel_lowlevel_api.h>
#include <arm_common/teesmc.h>

struct tee_smc_xfer_ctl {
	/* number of cmds that are not
	 * processed by `tee_smc_daemon` kthreads
	 */
	atomic_t nr_unbound_cmds;
	/* guarantee the mutual-exlusiveness of smc */
	struct mutex xfer_lock;

	/* cmds that wait for
	 * available TEE thread slots
	 */
	atomic_t nr_waiting_cmds;
	struct completion smc_comp;

	/* statistics information */
	s64 max_smc_time;
	s64 max_cmd_time;

};

static struct tee_smc_xfer_ctl tee_smc_xfer_ctl;

static inline void trace_tee_smc(struct tee_smc_xfer_ctl *ctl, int rv,
				 s64 time_start, s64 time_end)
{
	s64 duration = time_end - time_start;

	if (duration > 1000000LL) {
		pr_warn("WARNING SMC[0x%x] %sDURATION %lld us\n", rv,
			rv == TEESMC_RPC_FUNC_IRQ ? "IRQ " : "", duration);
	}

	/* we needn't handle concurrency here. */
	if (duration > ctl->max_smc_time)
		ctl->max_smc_time = duration;
}

static inline void trace_tee_smc_done(struct tee_smc_xfer_ctl *ctl,
				      s64 time_start,
				      s64 time_end)
{
	s64 duration = time_end - time_start;

	if (duration > ctl->max_cmd_time)
		ctl->max_cmd_time = duration;
}

/* return 0 for nonpreempt rpc, 1 for others */
static int handle_nonpreempt_rpc(struct smc_param *p)
{
	uint32_t func_id = TEESMC_RETURN_GET_RPC_FUNC(p->a0);

	/* for compatibility with legacy tee-os which
	 * does not support clkmgr
	 */
	if (func_id == T6SMC_RPC_CLKMGR_LEGACY_CMD) {
		p->a1 = tee_clkmgr_handle(p->a1, p->a2);
		return 0;
	}

	if (func_id != T6SMC_RPC_NONPREEMPT_CMD)
		return 1;

	switch (T6SMC_RPC_NONPREEMPT_GET_FUNC(p->a0)) {
	case T6SMC_RPC_CLKMGR_CMD:
		/* compatible with old interface */
		p->a1 = tee_clkmgr_handle(p->a1,
			(p->a1 & TEE_CLKMGR_TOKEN_NOT_LEGACY) ?
				p->a2 : (p->a2 | TEE_CLKMGR_OP_ENABLE));
		break;

	default:
		pr_err("Unknown non-preempt rpc cmd: 0x%llx\n",
			(unsigned long long) p->a0);
	}

	return 0;
}

static void tee_smc_work(struct tee_smc_xfer_ctl *ctl, struct smc_param *p)
{
	s64 start, end;


	u64 rv = p->a0 == TEESMC32_FASTCALL_WITH_ARG ?
		 TEESMC32_FASTCALL_RETURN_FROM_RPC :
		 TEESMC32_CALL_RETURN_FROM_RPC;

	/* we need to place atomic_inc ahead of xfer_lock
	 * in order that an smc-execution thread can
	 * see other pending commands without releasing
	 * xfer_lock
	 */
	atomic_inc(&ctl->nr_unbound_cmds);

	mutex_lock(&ctl->xfer_lock);

	start = ktime_to_us(ktime_get());

	while (1) {

		s64 a = ktime_to_us(ktime_get()), b;

		tee_smc_call(p);

		b = ktime_to_us(ktime_get());
		trace_tee_smc(ctl, TEESMC_RETURN_GET_RPC_FUNC(p->a0), a, b);

		if (!TEESMC_RETURN_IS_RPC(p->a0))
			goto smc_return;

		if (handle_nonpreempt_rpc(p)) {
			if (TEESMC_RETURN_GET_RPC_FUNC(p->a0)
					!= TEESMC_RPC_FUNC_IRQ)
				goto smc_return;
		}
		p->a0 = rv;
	}

smc_return:

	atomic_dec(&ctl->nr_unbound_cmds);

	mutex_unlock(&ctl->xfer_lock);

	end = ktime_to_us(ktime_get());

	trace_tee_smc_done(ctl, start, end);
}


static inline void __smc_xfer(struct tee_smc_xfer_ctl *ctl, struct smc_param *p)
{
	tee_smc_work(ctl, p);
}

static int platform_bl_init(struct tee_smc_xfer_ctl *ctl)
{
	return 0;
}

static void platform_bl_deinit(struct tee_smc_xfer_ctl *ctl) { }

static void xfer_enqueue_waiters(struct tee_smc_xfer_ctl *ctl)
{
	/*TODO handle too long time of waiting */
	atomic_inc(&ctl->nr_waiting_cmds);
	wait_for_completion(&ctl->smc_comp);
}

static void xfer_dequeue_waiters(struct tee_smc_xfer_ctl *ctl)
{
	if (atomic_dec_if_positive(&ctl->nr_waiting_cmds) >= 0)
		complete(&ctl->smc_comp);
}

void __call_tee(struct smc_param *p)
{
	/* NOTE!!! we remove the e_lock_teez(ptee) here !!!! */
#ifdef ARM64
	uint64_t orig_a0 = p->a0;
#else
	uint32_t orig_a0 = p->a0;
#endif
	for (;;) {
		__smc_xfer(&tee_smc_xfer_ctl, p);
		if (p->a0 == TEESMC_RETURN_ETHREAD_LIMIT) {
			xfer_enqueue_waiters(&tee_smc_xfer_ctl);
			p->a0 = orig_a0;
		} else {
			if (!TEESMC_RETURN_IS_RPC(p->a0))
				xfer_dequeue_waiters(&tee_smc_xfer_ctl);
			break;
		}
	}
}

inline void smc_xfer(struct smc_param *p)
{
	__smc_xfer(&tee_smc_xfer_ctl, p);
}

int tee_init_smc_xfer(void)
{
	int r;
	struct tee_smc_xfer_ctl *ctl = &tee_smc_xfer_ctl;

	atomic_set(&ctl->nr_unbound_cmds, 0);
	mutex_init(&ctl->xfer_lock);

	atomic_set(&ctl->nr_waiting_cmds, 0);
	init_completion(&ctl->smc_comp);

	ctl->max_smc_time = 0LL;
	ctl->max_cmd_time = 0LL;

	r = platform_bl_init(ctl);
	if (r < 0)
		goto err;

	return 0;

err:
	return r;
}

void tee_exit_smc_xfer(void)
{
	struct tee_smc_xfer_ctl *ctl = &tee_smc_xfer_ctl;

	platform_bl_deinit(ctl);
}
