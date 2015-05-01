/* drivers/soc/qcom/smp2p_test.c
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
#include <soc/qcom/subsystem_restart.h>
#include "smp2p_private.h"
#include "smp2p_test_common.h"

/**
 * smp2p_ut_local_basic - Basic sanity test using local loopback.
 *
 * @s: pointer to output file
 *
 * This test simulates a simple write and read
 * when remote processor does not exist.
 */
static void smp2p_ut_local_basic(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_out *smp2p_obj;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;
	uint32_t test_request;
	uint32_t test_response = 0;
	static struct mock_cb_data cb_data;

	seq_printf(s, "Running %s\n", __func__);
	mock_cb_data_init(&cb_data);
	do {
		/* initialize mock edge and start opening */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));

		msm_smp2p_set_remote_mock_exists(false);

		ret = msm_smp2p_out_open(SMP2P_REMOTE_MOCK_PROC, "smp2p",
			&cb_data.nb, &smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);

		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);
		UT_ASSERT_INT(cb_data.cb_count, ==, 0);
		rmp->rx_interrupt_count = 0;

		/* simulate response from remote side */
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
					SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
					SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
		rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
		rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
		rmp->remote_item.header.valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 0);
		rmp->remote_item.header.flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);
		rmp->tx_interrupt();

		/* verify port was opened */
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ / 2), >, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_open, ==, 1);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 2);

		/* do write (test outbound entries) */
		rmp->rx_interrupt_count = 0;
		test_request = 0xC0DE;
		ret = msm_smp2p_out_write(smp2p_obj, test_request);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		/* do read (test inbound entries) */
		ret = msm_smp2p_out_read(smp2p_obj, &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(test_request, ==, test_response);

		ret = msm_smp2p_out_close(&smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(smp2p_obj, ==, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
		(void)msm_smp2p_out_close(&smp2p_obj);
	}
}

/**
 * smp2p_ut_local_late_open - Verify post-negotiation opening.
 *
 * @s: pointer to output file
 *
 * Verify entry creation for opening entries after negotiation is complete.
 */
static void smp2p_ut_local_late_open(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_out *smp2p_obj;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;
	uint32_t test_request;
	uint32_t test_response = 0;
	static struct mock_cb_data cb_data;

	seq_printf(s, "Running %s\n", __func__);
	mock_cb_data_init(&cb_data);
	do {
		/* initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
			rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
			rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
			rmp->remote_item.header.valid_total_ent,
			SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 0);
		rmp->remote_item.header.flags = 0x0;

		msm_smp2p_set_remote_mock_exists(true);

		ret = msm_smp2p_out_open(SMP2P_REMOTE_MOCK_PROC, "smp2p",
			&cb_data.nb, &smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);

		/* verify port was opened */
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_open, ==, 1);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 2);

		/* do write (test outbound entries) */
		rmp->rx_interrupt_count = 0;
		test_request = 0xC0DE;
		ret = msm_smp2p_out_write(smp2p_obj, test_request);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		/* do read (test inbound entries) */
		ret = msm_smp2p_out_read(smp2p_obj, &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(test_request, ==, test_response);

		ret = msm_smp2p_out_close(&smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(smp2p_obj, ==, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
		(void)msm_smp2p_out_close(&smp2p_obj);
	}
}

/**
 * smp2p_ut_local_early_open - Verify pre-negotiation opening.
 *
 * @s: pointer to output file
 *
 * Verify entry creation for opening entries before negotiation is complete.
 */
static void smp2p_ut_local_early_open(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_out *smp2p_obj;
	struct msm_smp2p_remote_mock *rmp = NULL;
	struct smp2p_smem *outbound_item;
	int negotiation_state;
	int ret;
	uint32_t test_request;
	uint32_t test_response = 0;
	static struct mock_cb_data cb_data;

	seq_printf(s, "Running %s\n", __func__);
	mock_cb_data_init(&cb_data);
	do {
		/* initialize mock edge, but don't enable, yet */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
		rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
		rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
		rmp->remote_item.header.valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 0);
		rmp->remote_item.header.flags = 0x0;

		msm_smp2p_set_remote_mock_exists(false);
		UT_ASSERT_PTR(NULL, ==,
				smp2p_get_in_item(SMP2P_REMOTE_MOCK_PROC));

		/* initiate open, but verify it doesn't complete */
		ret = msm_smp2p_out_open(SMP2P_REMOTE_MOCK_PROC, "smp2p",
			&cb_data.nb, &smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ / 8),
			==, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 0);
		UT_ASSERT_INT(cb_data.event_open, ==, 0);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		outbound_item = smp2p_get_out_item(SMP2P_REMOTE_MOCK_PROC,
				&negotiation_state);
		UT_ASSERT_PTR(outbound_item, !=, NULL);
		UT_ASSERT_INT(negotiation_state, ==, SMP2P_EDGE_STATE_OPENING);
		UT_ASSERT_INT(0, ==,
			SMP2P_GET_ENT_VALID(outbound_item->valid_total_ent));

		/* verify that read/write don't work yet */
		rmp->rx_interrupt_count = 0;
		test_request = 0x0;
		ret = msm_smp2p_out_write(smp2p_obj, test_request);
		UT_ASSERT_INT(ret, ==, -ENODEV);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 0);

		ret = msm_smp2p_out_read(smp2p_obj, &test_response);
		UT_ASSERT_INT(ret, ==, -ENODEV);

		/* allocate remote entry and verify open */
		msm_smp2p_set_remote_mock_exists(true);
		rmp->tx_interrupt();

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_data.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_data.cb_count, ==, 1);
		UT_ASSERT_INT(cb_data.event_open, ==, 1);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 2);

		/* do write (test outbound entries) */
		rmp->rx_interrupt_count = 0;
		test_request = 0xC0DE;
		ret = msm_smp2p_out_write(smp2p_obj, test_request);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		/* do read (test inbound entries) */
		ret = msm_smp2p_out_read(smp2p_obj, &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(test_request, ==, test_response);

		ret = msm_smp2p_out_close(&smp2p_obj);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(smp2p_obj, ==, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
		(void)msm_smp2p_out_close(&smp2p_obj);
	}
}

/**
 * smp2p_ut_mock_loopback - Exercise the remote loopback using remote mock.
 *
 * @s: pointer to output file
 *
 * This test exercises the remote loopback code using
 * remote mock object. The remote mock object simulates the remote
 * processor sending remote loopback commands to the local processor.
 */
static void smp2p_ut_mock_loopback(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;
	uint32_t test_request = 0;
	uint32_t test_response = 0;
	struct msm_smp2p_out  *local;

	seq_printf(s, "Running %s\n", __func__);
	do {
		/* Initialize the mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
		rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
		rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
		rmp->remote_item.header.valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 1);
		rmp->remote_item.header.flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);

		/* Create test entry and attach loopback server */
		rmp->rx_interrupt_count = 0;
		INIT_COMPLETION(rmp->cb_completion);
		strlcpy(rmp->remote_item.entries[0].name, "smp2p",
							SMP2P_MAX_ENTRY_NAME);
		rmp->remote_item.entries[0].entry = 0;
		rmp->tx_interrupt();

		local = msm_smp2p_init_rmt_lpb_proc(SMP2P_REMOTE_MOCK_PROC);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&rmp->cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 2);

		/* Send Echo Command */
		rmp->rx_interrupt_count = 0;
		INIT_COMPLETION(rmp->cb_completion);
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_ECHO);
		SMP2P_SET_RMT_DATA(test_request, 10);
		rmp->remote_item.entries[0].entry = test_request;
		rmp->tx_interrupt();
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&rmp->cb_completion, HZ / 2),
			>, 0);

		/* Verify Echo Response */
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);
		ret = msm_smp2p_out_read(local,
							&test_response);
		UT_ASSERT_INT(ret, ==, 0);
		test_response = SMP2P_GET_RMT_DATA(test_response);
		UT_ASSERT_INT(test_response, ==, 10);

		/* Send PINGPONG command */
		test_request = 0;
		test_response = 0;
		rmp->rx_interrupt_count = 0;
		INIT_COMPLETION(rmp->cb_completion);
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_PINGPONG);
		SMP2P_SET_RMT_DATA(test_request, 10);
		rmp->remote_item.entries[0].entry = test_request;
		rmp->tx_interrupt();
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&rmp->cb_completion, HZ / 2),
			>, 0);

		/* Verify PINGPONG Response */
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);
		ret = msm_smp2p_out_read(local, &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		test_response = SMP2P_GET_RMT_DATA(test_response);
		UT_ASSERT_INT(test_response, ==, 9);

		/* Send CLEARALL command */
		test_request = 0;
		test_response = 0;
		rmp->rx_interrupt_count = 0;
		INIT_COMPLETION(rmp->cb_completion);
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_CLEARALL);
		SMP2P_SET_RMT_DATA(test_request, 10);
		rmp->remote_item.entries[0].entry = test_request;
		rmp->tx_interrupt();
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&rmp->cb_completion, HZ / 2),
			>, 0);

		/* Verify CLEARALL response */
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);
		ret = msm_smp2p_out_read(local, &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		test_response = SMP2P_GET_RMT_DATA(test_response);
		UT_ASSERT_INT(test_response, ==, 0);

		ret = msm_smp2p_deinit_rmt_lpb_proc(SMP2P_REMOTE_MOCK_PROC);
		UT_ASSERT_INT(ret, ==, 0);
		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
		msm_smp2p_deinit_rmt_lpb_proc(SMP2P_REMOTE_MOCK_PROC);
	}
}

/**
 * smp2p_ut_remote_inout_core - Verify inbound/outbound functionality.
 *
 * @s: pointer to output file
 * @remote_pid:  Remote processor to test
 *
 * This test verifies inbound/outbound functionality for the remote processor.
 */
static void smp2p_ut_remote_inout_core(struct seq_file *s, int remote_pid)
{
	int failed = 0;
	struct msm_smp2p_out *handle;
	int ret;
	uint32_t test_request;
	uint32_t test_response = 0;
	static struct mock_cb_data cb_out;
	static struct mock_cb_data cb_in;

	seq_printf(s, "Running %s for '%s' remote pid %d\n",
		   __func__, smp2p_pid_to_name(remote_pid), remote_pid);
	mock_cb_data_init(&cb_out);
	mock_cb_data_init(&cb_in);
	do {
		/* Open output entry */
		ret = msm_smp2p_out_open(remote_pid, "smp2p",
			&cb_out.nb, &handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_out.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_out.cb_count, ==, 1);
		UT_ASSERT_INT(cb_out.event_open, ==, 1);

		/* Open inbound entry */
		ret = msm_smp2p_in_register(remote_pid, "smp2p",
				&cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_open, ==, 1);

		/* Write an echo request */
		mock_cb_data_reset(&cb_out);
		mock_cb_data_reset(&cb_in);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_ECHO);
		SMP2P_SET_RMT_DATA(test_request, 0xAA55);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		/* Verify inbound reply */
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 1);
		UT_ASSERT_INT(SMP2P_GET_RMT_DATA(
			    cb_in.entry_data.current_value), ==, 0xAA55);

		ret = msm_smp2p_in_read(remote_pid, "smp2p", &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(0, ==, SMP2P_GET_RMT_CMD_TYPE(test_response));
		UT_ASSERT_INT(SMP2P_LB_CMD_ECHO, ==,
				SMP2P_GET_RMT_CMD(test_response));
		UT_ASSERT_INT(0xAA55, ==, SMP2P_GET_RMT_DATA(test_response));

		/* Write a clear all request */
		mock_cb_data_reset(&cb_in);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_CLEARALL);
		SMP2P_SET_RMT_DATA(test_request, 0xAA55);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		/* Verify inbound reply */
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 1);
		UT_ASSERT_INT(SMP2P_GET_RMT_DATA(
			    cb_in.entry_data.current_value), ==, 0x0000);

		ret = msm_smp2p_in_read(remote_pid, "smp2p", &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(0, ==, SMP2P_GET_RMT_CMD_TYPE(test_response));
		UT_ASSERT_INT(0x0000, ==, SMP2P_GET_RMT_DATA(test_response));

		/* Write a decrement request */
		mock_cb_data_reset(&cb_in);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_LB_CMD_PINGPONG);
		SMP2P_SET_RMT_DATA(test_request, 0xAA55);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		/* Verify inbound reply */
		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ / 2),
			>, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 1);
		UT_ASSERT_INT(SMP2P_GET_RMT_DATA(
			    cb_in.entry_data.current_value), ==, 0xAA54);

		ret = msm_smp2p_in_read(remote_pid, "smp2p", &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(0, ==, SMP2P_GET_RMT_CMD_TYPE(test_response));
		UT_ASSERT_INT(SMP2P_LB_CMD_PINGPONG, ==,
				SMP2P_GET_RMT_CMD(test_response));
		UT_ASSERT_INT(0xAA54, ==, SMP2P_GET_RMT_DATA(test_response));

		/* Test the ignore flag */
		mock_cb_data_reset(&cb_in);
		test_request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(test_request, 1);
		SMP2P_SET_RMT_CMD(test_request, SMP2P_RLPB_IGNORE);
		SMP2P_SET_RMT_DATA(test_request, 0xAA55);
		ret = msm_smp2p_out_write(handle, test_request);
		UT_ASSERT_INT(ret, ==, 0);

		UT_ASSERT_INT(
			(int)wait_for_completion_timeout(
					&cb_in.cb_completion, HZ / 2),
			==, 0);
		UT_ASSERT_INT(cb_in.cb_count, ==, 0);
		UT_ASSERT_INT(cb_in.event_entry_update, ==, 0);
		ret = msm_smp2p_in_read(remote_pid, "smp2p", &test_response);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(0xAA54, ==, SMP2P_GET_RMT_DATA(test_response));

		/* Cleanup */
		ret = msm_smp2p_out_close(&handle);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_PTR(handle, ==, 0);
		ret = msm_smp2p_in_unregister(remote_pid, "smp2p", &cb_in.nb);
		UT_ASSERT_INT(ret, ==, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		if (handle)
			(void)msm_smp2p_out_close(&handle);
		(void)msm_smp2p_in_unregister(remote_pid, "smp2p", &cb_in.nb);

		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * smp2p_ut_remote_inout - Verify inbound/outbound functionality for all.
 *
 * @s: pointer to output file
 *
 * This test verifies inbound and outbound functionality for all
 * configured remote processor.
 */
static void smp2p_ut_remote_inout(struct seq_file *s)
{
	struct smp2p_interrupt_config *int_cfg;
	int pid;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s, "Remote processor config unavailable\n");
		return;
	}

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid) {
		if (!int_cfg[pid].is_configured)
			continue;

		msm_smp2p_deinit_rmt_lpb_proc(pid);
		smp2p_ut_remote_inout_core(s, pid);
		msm_smp2p_init_rmt_lpb_proc(pid);
	}
}

/**
 * smp2p_ut_remote_out_max_entries_core - Verify open functionality.
 *
 * @s: pointer to output file
 * @remote_pid:  Remote processor for which the test is executed.
 *
 * This test verifies open functionality by creating maximum outbound entries.
 */
static void smp2p_ut_remote_out_max_entries_core(struct seq_file *s,
	int remote_pid)
{
	int j = 0;
	int failed = 0;
	struct msm_smp2p_out *handle[SMP2P_MAX_ENTRY];
	int ret;
	static struct mock_cb_data cb_out[SMP2P_MAX_ENTRY];
	char entry_name[SMP2P_MAX_ENTRY_NAME];
	int num_created;

	seq_printf(s, "Running %s for '%s' remote pid %d\n",
		   __func__, smp2p_pid_to_name(remote_pid), remote_pid);

	for (j = 0; j < SMP2P_MAX_ENTRY; j++) {
		handle[j] = NULL;
		mock_cb_data_init(&cb_out[j]);
	}

	do {
		num_created = 0;
		for (j = 0; j < SMP2P_MAX_ENTRY; j++) {
			/* Open as many output entries as possible */
			scnprintf((char *)entry_name, SMP2P_MAX_ENTRY_NAME,
				"smp2p%d", j);
			ret = msm_smp2p_out_open(remote_pid, entry_name,
				&cb_out[j].nb, &handle[j]);
			if (ret == -ENOMEM)
				/* hit max number */
				break;
			UT_ASSERT_INT(ret, ==, 0);
			++num_created;
		}
		if (failed)
			break;

		/* verify we created more than 1 entry */
		UT_ASSERT_INT(num_created, <=, SMP2P_MAX_ENTRY);
		UT_ASSERT_INT(num_created, >, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}

	/* cleanup */
	for (j = 0; j < SMP2P_MAX_ENTRY; j++)
		ret = msm_smp2p_out_close(&handle[j]);
}

/**
 * smp2p_ut_remote_out_max_entries - Verify open for all configured processors.
 *
 * @s: pointer to output file
 *
 * This test verifies creating max number of entries for
 * all configured remote processor.
 */
static void smp2p_ut_remote_out_max_entries(struct seq_file *s)
{
	struct smp2p_interrupt_config *int_cfg;
	int pid;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s, "Remote processor config unavailable\n");
		return;
	}

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid) {
		if (!int_cfg[pid].is_configured)
			continue;

		smp2p_ut_remote_out_max_entries_core(s, pid);
	}
}

/**
 * smp2p_ut_local_in_max_entries - Verify registering and unregistering.
 *
 * @s: pointer to output file
 *
 * This test verifies registering and unregistering for inbound entries using
 * the remote mock processor.
 */
static void smp2p_ut_local_in_max_entries(struct seq_file *s)
{
	int j = 0;
	int failed = 0;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;
	static struct mock_cb_data cb_in[SMP2P_MAX_ENTRY];
	static struct mock_cb_data cb_out;

	seq_printf(s, "Running %s\n", __func__);

	for (j = 0; j < SMP2P_MAX_ENTRY; j++)
		mock_cb_data_init(&cb_in[j]);

	mock_cb_data_init(&cb_out);

	do {
		/* Initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
		rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
		rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
		rmp->remote_item.header.valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 0);
		rmp->remote_item.header.flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);

		/* Create Max Entries in the remote mock object */
		for (j = 0; j < SMP2P_MAX_ENTRY; j++) {
			scnprintf(rmp->remote_item.entries[j].name,
				SMP2P_MAX_ENTRY_NAME, "smp2p%d", j);
			rmp->remote_item.entries[j].entry = 0;
			rmp->tx_interrupt();
		}

		/* Register for in entries */
		for (j = 0; j < SMP2P_MAX_ENTRY; j++) {
			ret = msm_smp2p_in_register(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[j].name,
				&(cb_in[j].nb));
			UT_ASSERT_INT(ret, ==, 0);
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&(cb_in[j].cb_completion), HZ / 2),
				>, 0);
			UT_ASSERT_INT(cb_in[j].cb_count, ==, 1);
			UT_ASSERT_INT(cb_in[j].event_entry_update, ==, 0);
		}
		UT_ASSERT_INT(j, ==, SMP2P_MAX_ENTRY);

		/* Unregister */
		for (j = 0; j < SMP2P_MAX_ENTRY; j++) {
			ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[j].name,
				&(cb_in[j].nb));
		    UT_ASSERT_INT(ret, ==, 0);
		}
		UT_ASSERT_INT(j, ==, SMP2P_MAX_ENTRY);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");

		for (j = 0; j < SMP2P_MAX_ENTRY; j++)
			ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[j].name,
				&(cb_in[j].nb));
	}
}

/**
 * smp2p_ut_local_in_multiple - Verify Multiple Inbound Registration.
 *
 * @s: pointer to output file
 *
 * This test verifies multiple clients registering for same inbound entries
 * using the remote mock processor.
 */
static void smp2p_ut_local_in_multiple(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;
	static struct mock_cb_data cb_in_1;
	static struct mock_cb_data cb_in_2;
	static struct mock_cb_data cb_out;

	seq_printf(s, "Running %s\n", __func__);

	mock_cb_data_init(&cb_in_1);
	mock_cb_data_init(&cb_in_2);
	mock_cb_data_init(&cb_out);

	do {
		/* Initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		rmp->remote_item.header.magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(
		rmp->remote_item.header.rem_loc_proc_id,
						SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(
		rmp->remote_item.header.feature_version, 1);
		SMP2P_SET_FEATURES(
		rmp->remote_item.header.feature_version, 0);
		SMP2P_SET_ENT_TOTAL(
		rmp->remote_item.header.valid_total_ent, 1);
		SMP2P_SET_ENT_VALID(
		rmp->remote_item.header.valid_total_ent, 0);
		rmp->remote_item.header.flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);

		/* Create an Entry in the remote mock object */
		scnprintf(rmp->remote_item.entries[0].name,
				SMP2P_MAX_ENTRY_NAME, "smp2p%d", 1);
		rmp->remote_item.entries[0].entry = 0;
		rmp->tx_interrupt();

		/* Register multiple clients for the inbound entry */
		ret = msm_smp2p_in_register(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&cb_in_1.nb);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
				&(cb_in_1.cb_completion), HZ / 2),
				>, 0);
		UT_ASSERT_INT(cb_in_1.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in_1.event_entry_update, ==, 0);

		ret = msm_smp2p_in_register(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&cb_in_2.nb);
		UT_ASSERT_INT(ret, ==, 0);
		UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
				&(cb_in_2.cb_completion), HZ / 2),
				>, 0);
		UT_ASSERT_INT(cb_in_2.cb_count, ==, 1);
		UT_ASSERT_INT(cb_in_2.event_entry_update, ==, 0);


		/* Unregister the clients */
		ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&(cb_in_1.nb));
		UT_ASSERT_INT(ret, ==, 0);

		ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&(cb_in_2.nb));
		UT_ASSERT_INT(ret, ==, 0);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");

		ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&(cb_in_1.nb));

		ret = msm_smp2p_in_unregister(SMP2P_REMOTE_MOCK_PROC,
				rmp->remote_item.entries[0].name,
				&(cb_in_2.nb));
	}
}

/**
 * smp2p_ut_local_ssr_ack - Verify SSR Done/ACK Feature
 *
 * @s: pointer to output file
 */
static void smp2p_ut_local_ssr_ack(struct seq_file *s)
{
	int failed = 0;
	struct msm_smp2p_remote_mock *rmp = NULL;
	int ret;

	seq_printf(s, "Running %s\n", __func__);
	do {
		struct smp2p_smem *rhdr;
		struct smp2p_smem *lhdr;
		int negotiation_state;

		/* initialize v1 without SMP2P_FEATURE_SSR_ACK enabled */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);
		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);
		rhdr = &rmp->remote_item.header;

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0, sizeof(struct smp2p_smem_item));
		rhdr->magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(rhdr->rem_loc_proc_id,
				SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(rhdr->rem_loc_proc_id, SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(rhdr->feature_version, 1);
		SMP2P_SET_FEATURES(rhdr->feature_version, 0);
		SMP2P_SET_ENT_TOTAL(rhdr->valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(rhdr->valid_total_ent, 0);
		rhdr->flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);
		rmp->tx_interrupt();

		/* verify edge is open */
		lhdr = smp2p_get_out_item(SMP2P_REMOTE_MOCK_PROC,
					&negotiation_state);
		UT_ASSERT_PTR(NULL, !=, lhdr);
		UT_ASSERT_INT(negotiation_state, ==, SMP2P_EDGE_STATE_OPENED);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		/* verify no response to ack feature */
		rmp->rx_interrupt_count = 0;
		SMP2P_SET_RESTART_DONE(rhdr->flags, 1);
		rmp->tx_interrupt();
		UT_ASSERT_INT(0, ==, SMP2P_GET_RESTART_DONE(lhdr->flags));
		UT_ASSERT_INT(0, ==, SMP2P_GET_RESTART_ACK(lhdr->flags));
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 0);

		/* initialize v1 with SMP2P_FEATURE_SSR_ACK enabled */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);
		rmp = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(rmp, !=, NULL);
		rhdr = &rmp->remote_item.header;

		rmp->rx_interrupt_count = 0;
		memset(&rmp->remote_item, 0, sizeof(struct smp2p_smem_item));
		rhdr->magic = SMP2P_MAGIC;
		SMP2P_SET_LOCAL_PID(rhdr->rem_loc_proc_id,
				SMP2P_REMOTE_MOCK_PROC);
		SMP2P_SET_REMOTE_PID(rhdr->rem_loc_proc_id, SMP2P_APPS_PROC);
		SMP2P_SET_VERSION(rhdr->feature_version, 1);
		SMP2P_SET_FEATURES(rhdr->feature_version,
				SMP2P_FEATURE_SSR_ACK);
		SMP2P_SET_ENT_TOTAL(rhdr->valid_total_ent, SMP2P_MAX_ENTRY);
		SMP2P_SET_ENT_VALID(rhdr->valid_total_ent, 0);
		rmp->rx_interrupt_count = 0;
		rhdr->flags = 0x0;
		msm_smp2p_set_remote_mock_exists(true);
		rmp->tx_interrupt();

		/* verify edge is open */
		lhdr = smp2p_get_out_item(SMP2P_REMOTE_MOCK_PROC,
					&negotiation_state);
		UT_ASSERT_PTR(NULL, !=, lhdr);
		UT_ASSERT_INT(negotiation_state, ==, SMP2P_EDGE_STATE_OPENED);
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		/* verify response to ack feature */
		rmp->rx_interrupt_count = 0;
		SMP2P_SET_RESTART_DONE(rhdr->flags, 1);
		rmp->tx_interrupt();
		UT_ASSERT_INT(0, ==, SMP2P_GET_RESTART_DONE(lhdr->flags));
		UT_ASSERT_INT(1, ==, SMP2P_GET_RESTART_ACK(lhdr->flags));
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		rmp->rx_interrupt_count = 0;
		SMP2P_SET_RESTART_DONE(rhdr->flags, 0);
		rmp->tx_interrupt();
		UT_ASSERT_INT(0, ==, SMP2P_GET_RESTART_DONE(lhdr->flags));
		UT_ASSERT_INT(0, ==, SMP2P_GET_RESTART_ACK(lhdr->flags));
		UT_ASSERT_INT(rmp->rx_interrupt_count, ==, 1);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * get_ssr_name_for_proc - Retrieve an SSR name from the provided list
 *
 * @names:	List of possible processor names
 * @name_len:	The length of @names
 * @index:	Index into @names
 *
 * Return: Pointer to the next processor name, NULL in error conditions
 */
static char *get_ssr_name_for_proc(char *names[], size_t name_len, int index)
{
	if (index >= name_len) {
		pr_err("%s: SSR failed; check subsys name table\n",
				__func__);
		return NULL;
	}

	return names[index];
}

/**
 * smp2p_ut_local_ssr_ack - Verify SSR Done/ACK Feature
 *
 * @s: pointer to output file
 * @rpid: Remote processor ID
 * @int_cfg: Interrupt config
 */
static void smp2p_ut_remotesubsys_ssr_ack(struct seq_file *s, uint32_t rpid,
		struct smp2p_interrupt_config *int_cfg)
{
	int failed = 0;

	seq_printf(s, "Running %s\n", __func__);
	do {
		struct smp2p_smem *rhdr;
		struct smp2p_smem *lhdr;
		int negotiation_state;
		int name_index;
		int ret;
		uint32_t ssr_done_start;
		bool ssr_ack_enabled = false;
		bool ssr_success = false;
		char *name = NULL;

		static char *mpss_names[] = {"modem", "mpss"};
		static char *lpass_names[] = {"adsp", "lpass"};
		static char *sensor_names[] = {"slpi", "dsps"};
		static char *wcnss_names[] = {"wcnss"};

		lhdr = smp2p_get_out_item(rpid, &negotiation_state);
		UT_ASSERT_PTR(NULL, !=, lhdr);
		UT_ASSERT_INT(SMP2P_EDGE_STATE_OPENED, ==, negotiation_state);

		rhdr = smp2p_get_in_item(rpid);
		UT_ASSERT_PTR(NULL, !=, rhdr);

		/* get initial state of SSR flags */
		if (SMP2P_GET_FEATURES(rhdr->feature_version)
				& SMP2P_FEATURE_SSR_ACK)
			ssr_ack_enabled = true;
		else
			ssr_ack_enabled = false;

		ssr_done_start = SMP2P_GET_RESTART_DONE(rhdr->flags);
		UT_ASSERT_INT(ssr_done_start, ==,
				SMP2P_GET_RESTART_ACK(lhdr->flags));

		/* trigger restart */
		name_index = 0;
		while (!ssr_success) {

			switch (rpid) {
			case SMP2P_MODEM_PROC:
				name = get_ssr_name_for_proc(mpss_names,
						ARRAY_SIZE(mpss_names),
						name_index);
				break;
			case SMP2P_AUDIO_PROC:
				name = get_ssr_name_for_proc(lpass_names,
						ARRAY_SIZE(lpass_names),
						name_index);
				break;
			case SMP2P_SENSOR_PROC:
				name = get_ssr_name_for_proc(sensor_names,
						ARRAY_SIZE(sensor_names),
						name_index);
				break;
			case SMP2P_WIRELESS_PROC:
				name = get_ssr_name_for_proc(wcnss_names,
						ARRAY_SIZE(wcnss_names),
						name_index);
				break;
			default:
				pr_err("%s: Invalid proc ID %d given for ssr\n",
						__func__, rpid);
			}

			if (!name) {
				seq_puts(s, "\tSSR failed; check subsys name table\n");
				failed = true;
				break;
			}

			seq_printf(s, "Restarting '%s'\n", name);
			ret = subsystem_restart(name);
			if (ret == -ENODEV) {
				seq_puts(s, "\tSSR call failed\n");
				++name_index;
				continue;
			}
			ssr_success = true;
		}
		if (failed)
			break;

		msleep(10*1000);

		/* verify ack signaling */
		if (ssr_ack_enabled) {
			ssr_done_start ^= 1;
			UT_ASSERT_INT(ssr_done_start, ==,
					SMP2P_GET_RESTART_ACK(lhdr->flags));
			UT_ASSERT_INT(ssr_done_start, ==,
					SMP2P_GET_RESTART_DONE(rhdr->flags));
			UT_ASSERT_INT(0, ==,
					SMP2P_GET_RESTART_DONE(lhdr->flags));
			seq_puts(s, "\tSSR ACK Enabled and Toggled\n");
		} else {
			UT_ASSERT_INT(0, ==,
					SMP2P_GET_RESTART_DONE(lhdr->flags));
			UT_ASSERT_INT(0, ==,
					SMP2P_GET_RESTART_ACK(lhdr->flags));

			UT_ASSERT_INT(0, ==,
					SMP2P_GET_RESTART_DONE(rhdr->flags));
			UT_ASSERT_INT(0, ==,
					SMP2P_GET_RESTART_ACK(rhdr->flags));
			seq_puts(s, "\tSSR ACK Disabled\n");
		}

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}
}

/**
 * smp2p_ut_remote_ssr_ack - Verify SSR Done/ACK Feature
 *
 * @s: pointer to output file
 *
 * Triggers SSR for each subsystem.
 */
static void smp2p_ut_remote_ssr_ack(struct seq_file *s)
{
	struct smp2p_interrupt_config *int_cfg;
	int pid;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s,
			"Remote processor config unavailable\n");
		return;
	}

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid) {
		if (!int_cfg[pid].is_configured)
			continue;

		msm_smp2p_deinit_rmt_lpb_proc(pid);
		smp2p_ut_remotesubsys_ssr_ack(s, pid, &int_cfg[pid]);
		msm_smp2p_init_rmt_lpb_proc(pid);
	}
}

static struct dentry *dent;

static int debugfs_show(struct seq_file *s, void *data)
{
	void (*show)(struct seq_file *) = s->private;

	show(s);

	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_show, inode->i_private);
}

static const struct file_operations debug_ops = {
	.open = debug_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

void smp2p_debug_create(const char *name,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, 0444, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

void smp2p_debug_create_u32(const char *name, uint32_t *value)
{
	struct dentry *file;

	file = debugfs_create_u32(name, S_IRUGO | S_IWUSR, dent, value);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smp2p_debugfs_init(void)
{
	dent = debugfs_create_dir("smp2p_test", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

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
	smp2p_debug_create("ut_local_basic",
			smp2p_ut_local_basic);
	smp2p_debug_create("ut_local_late_open",
			smp2p_ut_local_late_open);
	smp2p_debug_create("ut_local_early_open",
			smp2p_ut_local_early_open);
	smp2p_debug_create("ut_mock_loopback",
			smp2p_ut_mock_loopback);
	smp2p_debug_create("ut_remote_inout",
			smp2p_ut_remote_inout);
	smp2p_debug_create("ut_local_in_max_entries",
		smp2p_ut_local_in_max_entries);
	smp2p_debug_create("ut_remote_out_max_entries",
			smp2p_ut_remote_out_max_entries);
	smp2p_debug_create("ut_local_in_multiple",
			smp2p_ut_local_in_multiple);
	smp2p_debug_create("ut_local_ssr_ack",
			smp2p_ut_local_ssr_ack);
	smp2p_debug_create("ut_remote_ssr_ack",
			smp2p_ut_remote_ssr_ack);

	return 0;
}
module_init(smp2p_debugfs_init);
