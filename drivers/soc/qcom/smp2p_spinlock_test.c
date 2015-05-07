/* drivers/soc/qcom/smp2p_spinlock_test.c
 *
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/remote_spinlock.h>
#include <soc/qcom/smem.h>
#include "smem_private.h"
#include "smp2p_private.h"
#include "smp2p_test_common.h"

#define RS_END_THIEF_PID_BIT 20
#define RS_END_THIEF_MASK 0x00f00000

/* Spinlock commands used for testing Apps<->RPM spinlocks. */
enum RPM_SPINLOCK_CMDS {
	RPM_CMD_INVALID,
	RPM_CMD_START,
	RPM_CMD_LOCKED,
	RPM_CMD_UNLOCKED,
	RPM_CMD_END,
};

/* Shared structure for testing Apps<->RPM spinlocks. */
struct rpm_spinlock_test {
	uint32_t apps_cmd;
	uint32_t apps_lock_count;
	uint32_t rpm_cmd;
	uint32_t rpm_lock_count;
};

static uint32_t ut_remote_spinlock_run_time = 1;

/**
 * smp2p_ut_remote_spinlock_core - Verify remote spinlock.
 *
 * @s:           Pointer to output file
 * @remote_pid:  Remote processor to test
 * @use_trylock: Use trylock to prevent an Apps deadlock if the
 *               remote spinlock fails.
 */
static void smp2p_ut_remote_spinlock_core(struct seq_file *s, int remote_pid,
		bool use_trylock)
{
	int failed = 0;
	unsigned lock_count = 0;
	struct msm_smp2p_out *handle = NULL;
	int ret;
	uint32_t test_request;
	uint32_t test_response;
	struct mock_cb_data cb_out;
	struct mock_cb_data cb_in;
	unsigned long flags;
	unsigned n;
	bool have_lock;
	bool timeout;
	int failed_tmp;
	int spinlock_owner;
	remote_spinlock_t *smem_spinlock;
	unsigned long end;

	seq_printf(s, "Running %s for '%s' remote pid %d\n",
		   __func__, smp2p_pid_to_name(remote_pid), remote_pid);

	cb_out.initialized = false;
	cb_in.initialized = false;
	mock_cb_data_init(&cb_out);
	mock_cb_data_init(&cb_in);
	do {
		smem_spinlock = smem_get_remote_spinlock();
		UT_ASSERT_PTR(smem_spinlock, !=, NULL);

		/* Open output entry */
		ret = msm_smp2p_out_open(remote_pid, SMP2P_RLPB_ENTRY_NAME,
			&cb_out.nb, &handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_out.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_out.cb_count, ==, 1);
		UT_ASSERT_INT(cb_out.event_open, ==, 1);

		/* Open inbound entry */
		ret = msm_smp2p_in_register(remote_pid, SMP2P_RLPB_ENTRY_NAME,
				&cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_open, ==, 1);

		/* Send start */
		mock_cb_data_reset(&cb_in);
		mock_cb_data_reset(&cb_out);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_START);
		SMP2P_SET_RMT_DATA(test_request, 0x0);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 1);
		ret = msm_smp2p_in_read(remote_pid, SMP2P_RLPB_ENTRY_NAME,
				&test_response);
		UT_ASSERT_INT(ret, ==, 0);

		test_response = SMP2P_GET_RMT_CMD(test_response);
		if (test_response != SMP2P_LB_CMD_RSPIN_LOCKED &&
				test_response != SMP2P_LB_CMD_RSPIN_UNLOCKED) {
			/* invalid response from remote - abort test */
			test_request = 0x0;
			SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
			SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, 0x0);
			ret = msm_smp2p_out_write(handle, test_request);
			UT_ASSERT_HEX(SMP2P_LB_CMD_RSPIN_LOCKED, ==,
					test_response);
		}

		/* Run spinlock test */
		if (use_trylock)
			seq_puts(s, "\tUsing remote_spin_trylock\n");
		else
			seq_puts(s, "\tUsing remote_spin_lock\n");

		flags = 0;
		have_lock = false;
		timeout = false;
		spinlock_owner = 0;
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);
		end = jiffies + (ut_remote_spinlock_run_time * HZ);
		if (ut_remote_spinlock_run_time < 300) {
				seq_printf(s, "\tRunning test for %u seconds; ",
					ut_remote_spinlock_run_time);
				seq_puts(s,
					"on physical hardware please run >= 300 seconds by doing 'echo 300 >  ut_remote_spinlock_time'\n");
		}
		while (time_is_after_jiffies(end)) {
			/* try to acquire spinlock */
			if (use_trylock) {
				unsigned long j_start = jiffies;
				while (!remote_spin_trylock_irqsave(
						smem_spinlock, flags)) {
					if (jiffies_to_msecs(jiffies - j_start)
							> 1000) {
						seq_puts(s,
							"\tFail: Timeout trying to get the lock\n");
						timeout = true;
						break;
					}
				}
				if (timeout)
					break;
			} else {
				remote_spin_lock_irqsave(smem_spinlock, flags);
			}
			have_lock = true;
			++lock_count;

			/* tell the remote side that we have the lock */
			SMP2P_SET_RMT_DATA(test_request, lock_count);
			SMP2P_SET_RMT_CMD(test_request,
					SMP2P_LB_CMD_RSPIN_LOCKED);
			ret = msm_smp2p_out_write(handle, test_request);
			UT_ASSERT_INT(ret, ==, 0);

			/* verify the other side doesn't say it has the lock */
			for (n = 0; n < 1000; ++n) {
				spinlock_owner =
					remote_spin_owner(smem_spinlock);
				if (spinlock_owner != SMEM_APPS) {
					/* lock stolen by remote side */
					seq_puts(s, "\tFail: Remote side: ");
					seq_printf(s, "%d stole lock pid: %d\n",
						remote_pid, spinlock_owner);
					failed = true;
					break;
				}
				spinlock_owner = 0;

				ret = msm_smp2p_in_read(remote_pid,
					SMP2P_RLPB_ENTRY_NAME, &test_response);
				UT_ASSERT_INT(ret, ==, 0);
				test_response =
					SMP2P_GET_RMT_CMD(test_response);
				UT_ASSERT_HEX(SMP2P_LB_CMD_RSPIN_UNLOCKED, ==,
					test_response);
			}
			if (failed)
				break;

			/* tell remote side we are unlocked and release lock */
			SMP2P_SET_RMT_CMD(test_request,
					SMP2P_LB_CMD_RSPIN_UNLOCKED);
			(void)msm_smp2p_out_write(handle, test_request);
			have_lock = false;
			remote_spin_unlock_irqrestore(smem_spinlock, flags);
		}
		if (have_lock)
			remote_spin_unlock_irqrestore(smem_spinlock, flags);

		/* End test */
		mock_cb_data_reset(&cb_in);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
		SMP2P_SET_RMT_DATA(test_request, lock_count |
				(spinlock_owner << RS_END_THIEF_PID_BIT));
		(void)msm_smp2p_out_write(handle, test_request);

		failed_tmp = failed;
		failed = false;
		do {
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ * 2),
				>, 0);
			INIT_COMPLETION(cb_in.cb_completion);
			ret = msm_smp2p_in_read(remote_pid,
					SMP2P_RLPB_ENTRY_NAME, &test_response);
			UT_ASSERT_INT(ret, ==, 0);
		} while (!failed &&
			SMP2P_GET_RMT_CMD(test_response) !=
			SMP2P_LB_CMD_RSPIN_END);
		if (failed)
			break;
		failed = failed_tmp;

		test_response = SMP2P_GET_RMT_DATA(test_response);
		seq_puts(s, "\tLocked spinlock ");
		seq_printf(s, "local %u times; remote %u times",
			lock_count,
			test_response & ((1 << RS_END_THIEF_PID_BIT) - 1)
			);
		if (test_response & RS_END_THIEF_MASK) {
			seq_puts(s, "Remote side reporting lock stolen by ");
			seq_printf(s, "pid %d.\n",
				SMP2P_GET_BITS(test_response,
					RS_END_THIEF_MASK,
					RS_END_THIEF_PID_BIT));
			failed = 1;
		}
		seq_puts(s, "\n");

		/* Cleanup */
		ret = msm_smp2p_out_close(&handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(handle, ==, NULL);
		ret = msm_smp2p_in_unregister(remote_pid,
				SMP2P_RLPB_ENTRY_NAME, &cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);

		if (!failed && !timeout)
			seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		if (handle) {
			/* send end command */
			test_request = 0;
			SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, lock_count);
			(void)msm_smp2p_out_write(handle, test_request);
			(void)msm_smp2p_out_close(&handle);
		}
		(void)msm_smp2p_in_unregister(remote_pid,
				SMP2P_RLPB_ENTRY_NAME, &cb_in.nb);

		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * smp2p_ut_remote_spinlock_pid - Verify remote spinlock for a processor.
 *
 * @s:           Pointer to output file
 * @pid:         Processor to test
 * @use_trylock: Use trylock to prevent an Apps deadlock if the
 *               remote spinlock fails.
 */
static void smp2p_ut_remote_spinlock_pid(struct seq_file *s, int pid,
		bool use_trylock)
{
	struct smp2p_interrupt_config *int_cfg;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s, "Remote processor config unavailable\n");
		return;
	}

	if (pid >= SMP2P_NUM_PROCS || !int_cfg[pid].is_configured)
		return;

	msm_smp2p_deinit_rmt_lpb_proc(pid);
	smp2p_ut_remote_spinlock_core(s, pid, use_trylock);
	msm_smp2p_init_rmt_lpb_proc(pid);
}

/**
 * smp2p_ut_remote_spinlock - Verify remote spinlock for all processors.
 *
 * @s:   pointer to output file
 */
static void smp2p_ut_remote_spinlock(struct seq_file *s)
{
	int pid;

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid)
		smp2p_ut_remote_spinlock_pid(s, pid, false);
}

/**
 * smp2p_ut_remote_spin_trylock - Verify remote trylock for all processors.
 *
 * @s:   Pointer to output file
 */
static void smp2p_ut_remote_spin_trylock(struct seq_file *s)
{
	int pid;

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid)
		smp2p_ut_remote_spinlock_pid(s, pid, true);
}

/**
 * smp2p_ut_remote_spinlock - Verify remote spinlock for all processors.
 *
 * @s:   pointer to output file
 *
 * This test verifies inbound and outbound functionality for all
 * configured remote processor.
 */
static void smp2p_ut_remote_spinlock_modem(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_pid(s, SMP2P_MODEM_PROC, false);
}

static void smp2p_ut_remote_spinlock_adsp(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_pid(s, SMP2P_AUDIO_PROC, false);
}

static void smp2p_ut_remote_spinlock_dsps(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_pid(s, SMP2P_SENSOR_PROC, false);
}

static void smp2p_ut_remote_spinlock_wcnss(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_pid(s, SMP2P_WIRELESS_PROC, false);
}

static void smp2p_ut_remote_spinlock_tz(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_pid(s, SMP2P_TZ_PROC, false);
}

/**
 * smp2p_ut_remote_spinlock_rpm - Verify remote spinlock.
 *
 * @s:   pointer to output file
 * @remote_pid:  Remote processor to test
 */
static void smp2p_ut_remote_spinlock_rpm(struct seq_file *s)
{
	int failed = 0;
	unsigned long flags;
	unsigned n;
	unsigned test_num;
	struct rpm_spinlock_test *data_ptr;
	remote_spinlock_t *smem_spinlock;
	bool have_lock;

	seq_printf(s, "Running %s for Apps<->RPM Test\n",
		   __func__);
	do {
		smem_spinlock = smem_get_remote_spinlock();
		UT_ASSERT_PTR(smem_spinlock, !=, NULL);

		data_ptr = smem_alloc(SMEM_ID_VENDOR0,
				sizeof(struct rpm_spinlock_test), 0,
				SMEM_ANY_HOST_FLAG);
		UT_ASSERT_PTR(0, !=, data_ptr);

		/* Send start */
		writel_relaxed(0, &data_ptr->apps_lock_count);
		writel_relaxed(RPM_CMD_START, &data_ptr->apps_cmd);

		seq_puts(s, "\tWaiting for RPM to start test\n");
		for (n = 0; n < 1000; ++n) {
			if (readl_relaxed(&data_ptr->rpm_cmd) !=
					RPM_CMD_INVALID)
				break;
			usleep(1000);
		}
		if (readl_relaxed(&data_ptr->rpm_cmd) == RPM_CMD_INVALID) {
			/* timeout waiting for RPM */
			writel_relaxed(RPM_CMD_INVALID, &data_ptr->apps_cmd);
			UT_ASSERT_INT(RPM_CMD_LOCKED, !=, RPM_CMD_INVALID);
		}

		/* Run spinlock test */
		flags = 0;
		have_lock = false;
		for (test_num = 0; !failed && test_num < 10000; ++test_num) {
			/* acquire spinlock */
			remote_spin_lock_irqsave(smem_spinlock, flags);
			have_lock = true;
			data_ptr->apps_lock_count++;
			writel_relaxed(data_ptr->apps_lock_count,
				&data_ptr->apps_lock_count);
			writel_relaxed(RPM_CMD_LOCKED, &data_ptr->apps_cmd);
			/*
			 * Ensure that the remote side sees our lock has
			 * been acquired before we start polling their status.
			 */
			wmb();

			/* verify the other side doesn't say it has the lock */
			for (n = 0; n < 1000; ++n) {
				UT_ASSERT_HEX(RPM_CMD_UNLOCKED, ==,
					readl_relaxed(&data_ptr->rpm_cmd));
			}
			if (failed)
				break;

			/* release spinlock */
			have_lock = false;
			writel_relaxed(RPM_CMD_UNLOCKED, &data_ptr->apps_cmd);
			/*
			 * Ensure that our status-update write was committed
			 * before we unlock the spinlock.
			 */
			wmb();
			remote_spin_unlock_irqrestore(smem_spinlock, flags);
		}
		if (have_lock)
			remote_spin_unlock_irqrestore(smem_spinlock, flags);

		/* End test */
		writel_relaxed(RPM_CMD_INVALID, &data_ptr->apps_cmd);
		seq_printf(s, "\tLocked spinlock local %u remote %u\n",
				readl_relaxed(&data_ptr->apps_lock_count),
				readl_relaxed(&data_ptr->rpm_lock_count));

		if (!failed)
			seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

struct rmt_spinlock_work_item {
	struct work_struct work;
	struct completion try_lock;
	struct completion locked;
	bool has_locked;
};

static void ut_remote_spinlock_ssr_worker(struct work_struct *work)
{
	remote_spinlock_t *smem_spinlock;
	unsigned long flags;
	struct rmt_spinlock_work_item *work_item =
		container_of(work, struct rmt_spinlock_work_item, work);

	work_item->has_locked = false;
	complete(&work_item->try_lock);
	smem_spinlock = smem_get_remote_spinlock();
	if (!smem_spinlock) {
		pr_err("%s Failed\n", __func__);
		return;
	}

	remote_spin_lock_irqsave(smem_spinlock, flags);
	remote_spin_unlock_irqrestore(smem_spinlock, flags);
	work_item->has_locked = true;
	complete(&work_item->locked);
}

/**
 * smp2p_ut_remote_spinlock_ssr - Verify remote spinlock.
 *
 * @s:   pointer to output file
 */
static void smp2p_ut_remote_spinlock_ssr(struct seq_file *s)
{
	int failed = 0;
	unsigned long flags;
	remote_spinlock_t *smem_spinlock;
	int spinlock_owner = 0;

	struct workqueue_struct *ws = NULL;
	struct rmt_spinlock_work_item work_item;

	seq_printf(s, " Running %s Test\n",
		   __func__);
	do {
		smem_spinlock = smem_get_remote_spinlock();
		UT_ASSERT_PTR(smem_spinlock, !=, NULL);

		ws = create_singlethread_workqueue("ut_remote_spinlock_ssr");
		UT_ASSERT_PTR(ws, !=, NULL);
		INIT_WORK(&work_item.work, ut_remote_spinlock_ssr_worker);
		init_completion(&work_item.try_lock);
		init_completion(&work_item.locked);

		remote_spin_lock_irqsave(smem_spinlock, flags);
		/* Unlock local spin lock and hold HW spinlock */
		spin_unlock_irqrestore(&((smem_spinlock)->local), flags);

		queue_work(ws, &work_item.work);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&work_item.try_lock, HZ * 2), >, 0);
		UT_ASSERT_INT((int)work_item.has_locked, ==, 0);
		spinlock_owner = remote_spin_owner(smem_spinlock);
		UT_ASSERT_INT(spinlock_owner, ==, SMEM_APPS);
		remote_spin_release_all(SMEM_APPS);

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&work_item.locked, HZ * 2), >, 0);

		if (!failed)
			seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * smp2p_ut_remote_spinlock_track_core - Verify remote spinlock.
 *
 * @s:           Pointer to output file
 * @remote_pid:  Remote processor to test
 *
 * This test has the remote subsystem grab the lock, and then has the local
 * subsystem attempt to grab the lock using the trylock() API. It then verifies
 * that the ID in the hw_spinlocks array matches the owner of the lock.
 */
static void smp2p_ut_remote_spinlock_track_core(struct seq_file *s,
		int remote_pid)
{
	int failed = 0;
	struct msm_smp2p_out *handle = NULL;
	int ret;
	uint32_t test_request;
	uint32_t test_response;
	struct mock_cb_data cb_out;
	struct mock_cb_data cb_in;
	unsigned long flags;
	int stored_value;
	remote_spinlock_t *smem_spinlock;

	seq_printf(s, "Running %s for '%s' remote pid %d\n",
		   __func__, smp2p_pid_to_name(remote_pid), remote_pid);

	cb_out.initialized = false;
	cb_in.initialized = false;
	mock_cb_data_init(&cb_out);
	mock_cb_data_init(&cb_in);
	do {
		smem_spinlock = smem_get_remote_spinlock();
		UT_ASSERT_PTR(smem_spinlock, !=, NULL);

		/* Open output entry */
		ret = msm_smp2p_out_open(remote_pid, SMP2P_RLPB_ENTRY_NAME,
			&cb_out.nb, &handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_out.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_out.cb_count, ==, 1);
		UT_ASSERT_INT(cb_out.event_open, ==, 1);

		/* Open inbound entry */
		ret = msm_smp2p_in_register(remote_pid, SMP2P_RLPB_ENTRY_NAME,
				&cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_open, ==, 1);

		/* Send start */
		mock_cb_data_reset(&cb_in);
		mock_cb_data_reset(&cb_out);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_START);
		SMP2P_SET_RMT_DATA(test_request, 0x0);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ * 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 1);
		ret = msm_smp2p_in_read(remote_pid, SMP2P_RLPB_ENTRY_NAME,
				&test_response);
		UT_ASSERT_INT(ret, ==, 0);

		test_response = SMP2P_GET_RMT_CMD(test_response);
		if (test_response != SMP2P_LB_CMD_RSPIN_LOCKED &&
				test_response != SMP2P_LB_CMD_RSPIN_UNLOCKED) {
			/* invalid response from remote - abort test */
			test_request = 0x0;
			SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
			SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, 0x0);
			ret = msm_smp2p_out_write(handle, test_request);
			UT_ASSERT_HEX(SMP2P_LB_CMD_RSPIN_LOCKED, ==,
					test_response);
		}

		/* Run spinlock test */
		flags = 0;
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE_REQ(test_request);

		/* try to acquire spinlock */
		remote_spin_trylock_irqsave(smem_spinlock, flags);
		/*
		 * Need to check against the locking token (PID + 1)
		 * because the remote_spin_owner() API only returns the
		 * PID.
		 */
		stored_value = remote_spin_get_hw_spinlocks_element(
				smem_spinlock);
		UT_ASSERT_INT(stored_value, ==,
			remote_spin_owner(smem_spinlock) + 1);
		UT_ASSERT_INT(stored_value, ==, remote_pid + 1);

		/* End test */
		test_request = 0x0;
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
		SMP2P_SET_RMT_DATA(test_request, 0x0);
		(void)msm_smp2p_out_write(handle, test_request);

		/* Cleanup */
		ret = msm_smp2p_out_close(&handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(handle, ==, NULL);
		ret = msm_smp2p_in_unregister(remote_pid,
				SMP2P_RLPB_ENTRY_NAME, &cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);

		if (!failed)
			seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		if (handle) {
			/* send end command */
			test_request = 0x0;
			SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_RSPIN_END);
			SMP2P_SET_RMT_DATA(test_request, 0x0);
			(void)msm_smp2p_out_write(handle, test_request);
			(void)msm_smp2p_out_close(&handle);
		}
		(void)msm_smp2p_in_unregister(remote_pid,
				SMP2P_RLPB_ENTRY_NAME, &cb_in.nb);

		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * smp2p_ut_remote_spinlock_track - Verify PID tracking for modem.
 *
 * @s:	Pointer to output file
 * @pid:		The processor to test
 */
static void smp2p_ut_remote_spinlock_track(struct seq_file *s, int pid)
{
	struct smp2p_interrupt_config *int_cfg;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s, "Remote processor config unavailable\n");
		return;
	}

	if (pid >= SMP2P_NUM_PROCS || !int_cfg[pid].is_configured)
		return;

	msm_smp2p_deinit_rmt_lpb_proc(pid);
	smp2p_ut_remote_spinlock_track_core(s, pid);
	msm_smp2p_init_rmt_lpb_proc(pid);
}

/**
 * smp2p_ut_remote_spinlock_track - Verify PID tracking for all processors.
 *
 * @s:	Pointer to output file
 *
 * This test verifies PID tracking for all configured remote processors.
 */
static void smp2p_ut_remote_spinlock_track_modem(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_track(s, SMP2P_MODEM_PROC);
}

static void smp2p_ut_remote_spinlock_track_adsp(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_track(s, SMP2P_AUDIO_PROC);
}

static void smp2p_ut_remote_spinlock_track_dsps(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_track(s, SMP2P_SENSOR_PROC);
}

static void smp2p_ut_remote_spinlock_track_wcnss(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_track(s, SMP2P_WIRELESS_PROC);
}

static void smp2p_ut_remote_spinlock_track_tz(struct seq_file *s)
{
	smp2p_ut_remote_spinlock_track(s, SMP2P_TZ_PROC);
}

static int __init smp2p_debugfs_init(void)
{
	/*
	 * Add Unit Test entries.
	 *
	 * The idea with unit tests is that you can run all of them
	 * from ADB shell by doing:
	 *  adb shell
	 *  cat ut*
	 *
	 * And if particular tests fail, you can then repeatedly run the
	 * failing tests as you debug and resolve the failing test.
	 */
	smp2p_debug_create("ut_remote_spinlock",
		smp2p_ut_remote_spinlock);
	smp2p_debug_create("ut_remote_spin_trylock",
		smp2p_ut_remote_spin_trylock);
	smp2p_debug_create("ut_remote_spinlock_modem",
		smp2p_ut_remote_spinlock_modem);
	smp2p_debug_create("ut_remote_spinlock_adsp",
		smp2p_ut_remote_spinlock_adsp);
	smp2p_debug_create("ut_remote_spinlock_dsps",
		smp2p_ut_remote_spinlock_dsps);
	smp2p_debug_create("ut_remote_spinlock_wcnss",
		smp2p_ut_remote_spinlock_wcnss);
	smp2p_debug_create("ut_remote_spinlock_tz",
		smp2p_ut_remote_spinlock_tz);
	smp2p_debug_create("ut_remote_spinlock_rpm",
		smp2p_ut_remote_spinlock_rpm);
	smp2p_debug_create_u32("ut_remote_spinlock_time",
		&ut_remote_spinlock_run_time);
	smp2p_debug_create("ut_remote_spinlock_ssr",
		&smp2p_ut_remote_spinlock_ssr);
	smp2p_debug_create("ut_remote_spinlock_track_modem",
		&smp2p_ut_remote_spinlock_track_modem);
	smp2p_debug_create("ut_remote_spinlock_track_adsp",
		&smp2p_ut_remote_spinlock_track_adsp);
	smp2p_debug_create("ut_remote_spinlock_track_dsps",
		&smp2p_ut_remote_spinlock_track_dsps);
	smp2p_debug_create("ut_remote_spinlock_track_wcnss",
		&smp2p_ut_remote_spinlock_track_wcnss);
	smp2p_debug_create("ut_remote_spinlock_track_tz",
		&smp2p_ut_remote_spinlock_track_tz);
	return 0;
}
module_init(smp2p_debugfs_init);
