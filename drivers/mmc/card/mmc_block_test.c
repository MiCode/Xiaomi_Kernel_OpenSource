/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* MMC block test */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt"\n"

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/debugfs.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>
#include <linux/test-iosched.h>
#include "queue.h"
#include <linux/mmc/mmc.h>

#define MODULE_NAME "mmc_block_test"
#define TEST_MAX_SECTOR_RANGE		(600*1024*1024) /* 600 MB */
#define TEST_MAX_BIOS_PER_REQ		128
#define CMD23_PACKED_BIT	(1 << 30)
#define LARGE_PRIME_1	1103515367
#define LARGE_PRIME_2	35757
#define PACKED_HDR_VER_MASK 0x000000FF
#define PACKED_HDR_RW_MASK 0x0000FF00
#define PACKED_HDR_NUM_REQS_MASK 0x00FF0000
#define PACKED_HDR_BITS_16_TO_29_SET 0x3FFF0000
/* the desired long test size to be read */
#define LONG_READ_TEST_MAX_NUM_BYTES (50*1024*1024) /* 50MB */
/* the minimum amount of requests that will be created */
#define LONG_WRITE_TEST_MIN_NUM_REQS 200 /* 100MB */
/* request queue limitation is 128 requests, and we leave 10 spare requests */
#define TEST_MAX_REQUESTS 118
#define LONG_READ_TEST_MAX_NUM_REQS	(LONG_READ_TEST_MAX_NUM_BYTES / \
		(TEST_MAX_BIOS_PER_REQ * sizeof(int) * BIO_U32_SIZE))
/* this doesn't allow the test requests num to be greater than the maximum */
#define LONG_READ_TEST_ACTUAL_NUM_REQS  \
			((TEST_MAX_REQUESTS < LONG_READ_TEST_MAX_NUM_REQS) ? \
				TEST_MAX_REQUESTS : LONG_READ_TEST_MAX_NUM_REQS)
#define MB_MSEC_RATIO_APPROXIMATION ((1024 * 1024) / 1000)
/* actual number of bytes in test */
#define LONG_READ_NUM_BYTES  (LONG_READ_TEST_ACTUAL_NUM_REQS *  \
			(TEST_MAX_BIOS_PER_REQ * sizeof(int) * BIO_U32_SIZE))
/* actual number of MiB in test multiplied by 10, for single digit precision*/
#define BYTE_TO_MB_x_10(x) ((x * 10) / (1024 * 1024))
/* extract integer value */
#define LONG_TEST_SIZE_INTEGER(x) (BYTE_TO_MB_x_10(x) / 10)
/* and calculate the MiB value fraction */
#define LONG_TEST_SIZE_FRACTION(x) (BYTE_TO_MB_x_10(x) - \
		(LONG_TEST_SIZE_INTEGER(x) * 10))
#define LONG_WRITE_TEST_SLEEP_TIME_MS 5

#define URGENT_DELAY_RANGE_MS 500

#define NEW_REQ_TEST_SLEEP_TIME 1
#define NEW_REQ_TEST_NUM_BIOS 64
#define TEST_REQUEST_NUM_OF_BIOS	3

#define CHECK_BKOPS_STATS(stats, exp_bkops, exp_hpi, exp_suspend)	\
				   ((stats.bkops != exp_bkops) ||	\
				    (stats.hpi != exp_hpi) ||		\
				    (stats.suspend != exp_suspend))
#define BKOPS_TEST_TIMEOUT 60000

enum is_random {
	NON_RANDOM_TEST,
	RANDOM_TEST,
};

enum mmc_block_test_testcases {
	/* Start of send write packing test group */
	SEND_WRITE_PACKING_MIN_TESTCASE,
	TEST_STOP_DUE_TO_READ = SEND_WRITE_PACKING_MIN_TESTCASE,
	TEST_STOP_DUE_TO_READ_AFTER_MAX_REQS,
	TEST_STOP_DUE_TO_FLUSH,
	TEST_STOP_DUE_TO_FLUSH_AFTER_MAX_REQS,
	TEST_STOP_DUE_TO_EMPTY_QUEUE,
	TEST_STOP_DUE_TO_MAX_REQ_NUM,
	TEST_STOP_DUE_TO_THRESHOLD,
	SEND_WRITE_PACKING_MAX_TESTCASE = TEST_STOP_DUE_TO_THRESHOLD,

	/* Start of err check test group */
	ERR_CHECK_MIN_TESTCASE,
	TEST_RET_ABORT = ERR_CHECK_MIN_TESTCASE,
	TEST_RET_PARTIAL_FOLLOWED_BY_SUCCESS,
	TEST_RET_PARTIAL_FOLLOWED_BY_ABORT,
	TEST_RET_PARTIAL_MULTIPLE_UNTIL_SUCCESS,
	TEST_RET_PARTIAL_MAX_FAIL_IDX,
	TEST_RET_RETRY,
	TEST_RET_CMD_ERR,
	TEST_RET_DATA_ERR,
	ERR_CHECK_MAX_TESTCASE = TEST_RET_DATA_ERR,

	/* Start of send invalid test group */
	INVALID_CMD_MIN_TESTCASE,
	TEST_HDR_INVALID_VERSION = INVALID_CMD_MIN_TESTCASE,
	TEST_HDR_WRONG_WRITE_CODE,
	TEST_HDR_INVALID_RW_CODE,
	TEST_HDR_DIFFERENT_ADDRESSES,
	TEST_HDR_REQ_NUM_SMALLER_THAN_ACTUAL,
	TEST_HDR_REQ_NUM_LARGER_THAN_ACTUAL,
	TEST_HDR_CMD23_PACKED_BIT_SET,
	TEST_CMD23_MAX_PACKED_WRITES,
	TEST_CMD23_ZERO_PACKED_WRITES,
	TEST_CMD23_PACKED_BIT_UNSET,
	TEST_CMD23_REL_WR_BIT_SET,
	TEST_CMD23_BITS_16TO29_SET,
	TEST_CMD23_HDR_BLK_NOT_IN_COUNT,
	INVALID_CMD_MAX_TESTCASE = TEST_CMD23_HDR_BLK_NOT_IN_COUNT,

	/*
	 * Start of packing control test group.
	 * in these next testcases the abbreviation FB = followed by
	 */
	PACKING_CONTROL_MIN_TESTCASE,
	TEST_PACKING_EXP_ONE_OVER_TRIGGER_FB_READ =
				PACKING_CONTROL_MIN_TESTCASE,
	TEST_PACKING_EXP_N_OVER_TRIGGER,
	TEST_PACKING_EXP_N_OVER_TRIGGER_FB_READ,
	TEST_PACKING_EXP_N_OVER_TRIGGER_FLUSH_N,
	TEST_PACKING_EXP_THRESHOLD_OVER_TRIGGER,
	TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS,
	TEST_PACKING_NOT_EXP_TRIGGER_REQUESTS,
	TEST_PACKING_NOT_EXP_TRIGGER_READ_TRIGGER,
	TEST_PACKING_NOT_EXP_TRIGGER_FLUSH_TRIGGER,
	TEST_PACK_MIX_PACKED_NO_PACKED_PACKED,
	TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED,
	PACKING_CONTROL_MAX_TESTCASE = TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED,

	/* Start of bkops test group */
	BKOPS_MIN_TESTCASE,
	BKOPS_DELAYED_WORK_LEVEL_1 = BKOPS_MIN_TESTCASE,
	BKOPS_DELAYED_WORK_LEVEL_1_HPI,
	BKOPS_CANCEL_DELAYED_WORK,
	BKOPS_URGENT_LEVEL_2,
	BKOPS_URGENT_LEVEL_2_TWO_REQS,
	BKOPS_URGENT_LEVEL_3,
	BKOPS_MAX_TESTCASE = BKOPS_URGENT_LEVEL_3,

	TEST_LONG_SEQUENTIAL_READ,
	TEST_LONG_SEQUENTIAL_WRITE,

	TEST_NEW_REQ_NOTIFICATION,
};

enum mmc_block_test_group {
	TEST_NO_GROUP,
	TEST_GENERAL_GROUP,
	TEST_SEND_WRITE_PACKING_GROUP,
	TEST_ERR_CHECK_GROUP,
	TEST_SEND_INVALID_GROUP,
	TEST_PACKING_CONTROL_GROUP,
	TEST_BKOPS_GROUP,
	TEST_NEW_NOTIFICATION_GROUP,
};

enum bkops_test_stages {
	BKOPS_STAGE_1,
	BKOPS_STAGE_2,
	BKOPS_STAGE_3,
	BKOPS_STAGE_4,
};

struct mmc_block_test_debug {
	struct dentry *send_write_packing_test;
	struct dentry *err_check_test;
	struct dentry *send_invalid_packed_test;
	struct dentry *random_test_seed;
	struct dentry *packing_control_test;
	struct dentry *bkops_test;
	struct dentry *long_sequential_read_test;
	struct dentry *long_sequential_write_test;
	struct dentry *new_req_notification_test;
};

struct mmc_block_test_data {
	/* The number of write requests that the test will issue */
	int num_requests;
	/* The expected write packing statistics for the current test */
	struct mmc_wr_pack_stats exp_packed_stats;
	/*
	 * A user-defined seed for random choices of number of bios written in
	 * a request, and of number of requests issued in a test
	 * This field is randomly updated after each use
	 */
	unsigned int random_test_seed;
	/* A retry counter used in err_check tests */
	int err_check_counter;
	/* Can be one of the values of enum test_group */
	enum mmc_block_test_group test_group;
	/*
	 * Indicates if the current testcase is running with random values of
	 * num_requests and num_bios (in each request)
	 */
	int is_random;
	/* Data structure for debugfs dentrys */
	struct mmc_block_test_debug debug;
	/*
	 * Data structure containing individual test information, including
	 * self-defined specific data
	 */
	struct test_info test_info;
	/* mmc block device test */
	struct blk_dev_test_type bdt;
	/* Current BKOPs test stage */
	enum bkops_test_stages	bkops_stage;
	/* A wait queue for BKOPs tests */
	wait_queue_head_t bkops_wait_q;
	/* A counter for the number of test requests completed */
	unsigned int completed_req_count;
};

static struct mmc_block_test_data *mbtd;

void print_mmc_packing_stats(struct mmc_card *card)
{
	int i;
	int max_num_of_packed_reqs = 0;

	if ((!card) || (!card->wr_pack_stats.packing_events))
		return;

	max_num_of_packed_reqs = card->ext_csd.max_packed_writes;

	spin_lock(&card->wr_pack_stats.lock);

	pr_info("%s: write packing statistics:",
		mmc_hostname(card->host));

	for (i = 1 ; i <= max_num_of_packed_reqs ; ++i) {
		if (card->wr_pack_stats.packing_events[i] != 0)
			pr_info("%s: Packed %d reqs - %d times",
				mmc_hostname(card->host), i,
				card->wr_pack_stats.packing_events[i]);
	}

	pr_info("%s: stopped packing due to the following reasons:",
		mmc_hostname(card->host));

	if (card->wr_pack_stats.pack_stop_reason[EXCEEDS_SEGMENTS])
		pr_info("%s: %d times: exceedmax num of segments",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[EXCEEDS_SEGMENTS]);
	if (card->wr_pack_stats.pack_stop_reason[EXCEEDS_SECTORS])
		pr_info("%s: %d times: exceeding the max num of sectors",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[EXCEEDS_SECTORS]);
	if (card->wr_pack_stats.pack_stop_reason[WRONG_DATA_DIR])
		pr_info("%s: %d times: wrong data direction",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[WRONG_DATA_DIR]);
	if (card->wr_pack_stats.pack_stop_reason[FLUSH_OR_DISCARD])
		pr_info("%s: %d times: flush or discard",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[FLUSH_OR_DISCARD]);
	if (card->wr_pack_stats.pack_stop_reason[EMPTY_QUEUE])
		pr_info("%s: %d times: empty queue",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[EMPTY_QUEUE]);
	if (card->wr_pack_stats.pack_stop_reason[REL_WRITE])
		pr_info("%s: %d times: rel write",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[REL_WRITE]);
	if (card->wr_pack_stats.pack_stop_reason[THRESHOLD])
		pr_info("%s: %d times: Threshold",
			mmc_hostname(card->host),
			card->wr_pack_stats.pack_stop_reason[THRESHOLD]);

	spin_unlock(&card->wr_pack_stats.lock);
}

/*
 * A callback assigned to the packed_test_fn field.
 * Called from block layer in mmc_blk_packed_hdr_wrq_prep.
 * Here we alter the packed header or CMD23 in order to send an invalid
 * packed command to the card.
 */
static void test_invalid_packed_cmd(struct request_queue *q,
				    struct mmc_queue_req *mqrq)
{
	struct mmc_queue *mq = q->queuedata;
	u32 *packed_cmd_hdr = mqrq->packed->cmd_hdr;
	struct request *req = mqrq->req;
	struct request *second_rq;
	struct test_request *test_rq;
	struct mmc_blk_request *brq = &mqrq->brq;
	int num_requests;
	int max_packed_reqs;

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return;
	}

	test_rq = (struct test_request *)req->elv.priv[0];
	if (!test_rq) {
		pr_err("%s: NULL test_rq", __func__);
		return;
	}
	max_packed_reqs = mq->card->ext_csd.max_packed_writes;

	switch (mbtd->test_info.testcase) {
	case TEST_HDR_INVALID_VERSION:
		pr_info("%s: set invalid header version", __func__);
		/* Put 0 in header version field (1 byte, offset 0 in header) */
		packed_cmd_hdr[0] = packed_cmd_hdr[0] & ~PACKED_HDR_VER_MASK;
		break;
	case TEST_HDR_WRONG_WRITE_CODE:
		pr_info("%s: wrong write code", __func__);
		/* Set R/W field with R value (1 byte, offset 1 in header) */
		packed_cmd_hdr[0] = packed_cmd_hdr[0] & ~PACKED_HDR_RW_MASK;
		packed_cmd_hdr[0] = packed_cmd_hdr[0] | 0x00000100;
		break;
	case TEST_HDR_INVALID_RW_CODE:
		pr_info("%s: invalid r/w code", __func__);
		/* Set R/W field with invalid value */
		packed_cmd_hdr[0] = packed_cmd_hdr[0] & ~PACKED_HDR_RW_MASK;
		packed_cmd_hdr[0] = packed_cmd_hdr[0] | 0x00000400;
		break;
	case TEST_HDR_DIFFERENT_ADDRESSES:
		pr_info("%s: different addresses", __func__);
		second_rq = list_entry(req->queuelist.next, struct request,
				queuelist);
		pr_info("%s: test_rq->sector=%ld, second_rq->sector=%ld",
			      __func__, (long)req->__sector,
			     (long)second_rq->__sector);
		/*
		 * Put start sector of second write request in the first write
		 * request's cmd25 argument in the packed header
		 */
		packed_cmd_hdr[3] = second_rq->__sector;
		break;
	case TEST_HDR_REQ_NUM_SMALLER_THAN_ACTUAL:
		pr_info("%s: request num smaller than actual" , __func__);
		num_requests = (packed_cmd_hdr[0] & PACKED_HDR_NUM_REQS_MASK)
									>> 16;
		/* num of entries is decremented by 1 */
		num_requests = (num_requests - 1) << 16;
		/*
		 * Set number of requests field in packed write header to be
		 * smaller than the actual number (1 byte, offset 2 in header)
		 */
		packed_cmd_hdr[0] = (packed_cmd_hdr[0] &
				     ~PACKED_HDR_NUM_REQS_MASK) + num_requests;
		break;
	case TEST_HDR_REQ_NUM_LARGER_THAN_ACTUAL:
		pr_info("%s: request num larger than actual" , __func__);
		num_requests = (packed_cmd_hdr[0] & PACKED_HDR_NUM_REQS_MASK)
									>> 16;
		/* num of entries is incremented by 1 */
		num_requests = (num_requests + 1) << 16;
		/*
		 * Set number of requests field in packed write header to be
		 * larger than the actual number (1 byte, offset 2 in header).
		 */
		packed_cmd_hdr[0] = (packed_cmd_hdr[0] &
				     ~PACKED_HDR_NUM_REQS_MASK) + num_requests;
		break;
	case TEST_HDR_CMD23_PACKED_BIT_SET:
		pr_info("%s: header CMD23 packed bit set" , __func__);
		/*
		 * Set packed bit (bit 30) in cmd23 argument of first and second
		 * write requests in packed write header.
		 * These are located at bytes 2 and 4 in packed write header
		 */
		packed_cmd_hdr[2] = packed_cmd_hdr[2] | CMD23_PACKED_BIT;
		packed_cmd_hdr[4] = packed_cmd_hdr[4] | CMD23_PACKED_BIT;
		break;
	case TEST_CMD23_MAX_PACKED_WRITES:
		pr_info("%s: CMD23 request num > max_packed_reqs",
			      __func__);
		/*
		 * Set the individual packed cmd23 request num to
		 * max_packed_reqs + 1
		 */
		brq->sbc.arg = MMC_CMD23_ARG_PACKED | (max_packed_reqs + 1);
		break;
	case TEST_CMD23_ZERO_PACKED_WRITES:
		pr_info("%s: CMD23 request num = 0", __func__);
		/* Set the individual packed cmd23 request num to zero */
		brq->sbc.arg = MMC_CMD23_ARG_PACKED;
		break;
	case TEST_CMD23_PACKED_BIT_UNSET:
		pr_info("%s: CMD23 packed bit unset", __func__);
		/*
		 * Set the individual packed cmd23 packed bit to 0,
		 *  although there is a packed write request
		 */
		brq->sbc.arg &= ~CMD23_PACKED_BIT;
		break;
	case TEST_CMD23_REL_WR_BIT_SET:
		pr_info("%s: CMD23 REL WR bit set", __func__);
		/* Set the individual packed cmd23 reliable write bit */
		brq->sbc.arg = MMC_CMD23_ARG_PACKED | MMC_CMD23_ARG_REL_WR;
		break;
	case TEST_CMD23_BITS_16TO29_SET:
		pr_info("%s: CMD23 bits [16-29] set", __func__);
		brq->sbc.arg = MMC_CMD23_ARG_PACKED |
			PACKED_HDR_BITS_16_TO_29_SET;
		break;
	case TEST_CMD23_HDR_BLK_NOT_IN_COUNT:
		pr_info("%s: CMD23 hdr not in block count", __func__);
		brq->sbc.arg = MMC_CMD23_ARG_PACKED |
		((rq_data_dir(req) == READ) ? 0 : mqrq->packed->blocks);
		break;
	default:
		pr_err("%s: unexpected testcase %d",
			__func__, mbtd->test_info.testcase);
		break;
	}
}

/*
 * A callback assigned to the err_check_fn field of the mmc_request by the
 * MMC/card/block layer.
 * Called upon request completion by the MMC/core layer.
 * Here we emulate an error return value from the card.
 */
static int test_err_check(struct mmc_card *card, struct mmc_async_req *areq)
{
	struct mmc_queue_req *mq_rq = container_of(areq, struct mmc_queue_req,
			mmc_active);
	struct request_queue *req_q = test_iosched_get_req_queue();
	struct mmc_queue *mq;
	int max_packed_reqs;
	int ret = 0;
	struct mmc_blk_request *brq;

	if (req_q)
		mq = req_q->queuedata;
	else {
		pr_err("%s: NULL request_queue", __func__);
		return 0;
	}

	if (!mq) {
		pr_err("%s: %s: NULL mq", __func__,
			mmc_hostname(card->host));
		return 0;
	}

	max_packed_reqs = mq->card->ext_csd.max_packed_writes;

	if (!mq_rq) {
		pr_err("%s: %s: NULL mq_rq", __func__,
			mmc_hostname(card->host));
		return 0;
	}
	brq = &mq_rq->brq;

	switch (mbtd->test_info.testcase) {
	case TEST_RET_ABORT:
		pr_info("%s: return abort", __func__);
		ret = MMC_BLK_ABORT;
		break;
	case TEST_RET_PARTIAL_FOLLOWED_BY_SUCCESS:
		pr_info("%s: return partial followed by success",
			      __func__);
		/*
		 * Since in this testcase num_requests is always >= 2,
		 * we can be sure that packed_fail_idx is always >= 1
		 */
		mq_rq->packed->idx_failure = (mbtd->num_requests / 2);
		pr_info("%s: packed_fail_idx = %d"
			, __func__, mq_rq->packed->idx_failure);
		mq->err_check_fn = NULL;
		ret = MMC_BLK_PARTIAL;
		break;
	case TEST_RET_PARTIAL_FOLLOWED_BY_ABORT:
		if (!mbtd->err_check_counter) {
			pr_info("%s: return partial followed by abort",
				      __func__);
			mbtd->err_check_counter++;
			/*
			 * Since in this testcase num_requests is always >= 3,
			 * we have that packed_fail_idx is always >= 1
			 */
			mq_rq->packed->idx_failure = (mbtd->num_requests / 2);
			pr_info("%s: packed_fail_idx = %d"
				, __func__, mq_rq->packed->idx_failure);
			ret = MMC_BLK_PARTIAL;
			break;
		}
		mbtd->err_check_counter = 0;
		mq->err_check_fn = NULL;
		ret = MMC_BLK_ABORT;
		break;
	case TEST_RET_PARTIAL_MULTIPLE_UNTIL_SUCCESS:
		pr_info("%s: return partial multiple until success",
			     __func__);
		if (++mbtd->err_check_counter >= (mbtd->num_requests)) {
			mq->err_check_fn = NULL;
			mbtd->err_check_counter = 0;
			ret = MMC_BLK_PARTIAL;
			break;
		}
		mq_rq->packed->idx_failure = 1;
		ret = MMC_BLK_PARTIAL;
		break;
	case TEST_RET_PARTIAL_MAX_FAIL_IDX:
		pr_info("%s: return partial max fail_idx", __func__);
		mq_rq->packed->idx_failure = max_packed_reqs - 1;
		mq->err_check_fn = NULL;
		ret = MMC_BLK_PARTIAL;
		break;
	case TEST_RET_RETRY:
		pr_info("%s: return retry", __func__);
		ret = MMC_BLK_RETRY;
		break;
	case TEST_RET_CMD_ERR:
		pr_info("%s: return cmd err", __func__);
		ret = MMC_BLK_CMD_ERR;
		break;
	case TEST_RET_DATA_ERR:
		pr_info("%s: return data err", __func__);
		ret = MMC_BLK_DATA_ERR;
		break;
	case BKOPS_URGENT_LEVEL_2:
	case BKOPS_URGENT_LEVEL_3:
	case BKOPS_URGENT_LEVEL_2_TWO_REQS:
		if (mbtd->err_check_counter++ == 0) {
			pr_info("%s: simulate an exception from the card",
				     __func__);
			brq->cmd.resp[0] |= R1_EXCEPTION_EVENT;
		}
		mq->err_check_fn = NULL;
		break;
	default:
		pr_err("%s: unexpected testcase %d",
			__func__, mbtd->test_info.testcase);
	}

	return ret;
}

/*
 * This is a specific implementation for the get_test_case_str_fn function
 * pointer in the test_info data structure. Given a valid test_data instance,
 * the function returns a string resembling the test name, based on the testcase
 */
static char *get_test_case_str(struct test_data *td)
{
	if (!td) {
		pr_err("%s: NULL td", __func__);
		return NULL;
	}

switch (td->test_info.testcase) {
	case TEST_STOP_DUE_TO_FLUSH:
		return "\"stop due to flush\"";
	case TEST_STOP_DUE_TO_FLUSH_AFTER_MAX_REQS:
		return "\"stop due to flush after max-1 reqs\"";
	case TEST_STOP_DUE_TO_READ:
		return "\"stop due to read\"";
	case TEST_STOP_DUE_TO_READ_AFTER_MAX_REQS:
		return "\"stop due to read after max-1 reqs\"";
	case TEST_STOP_DUE_TO_EMPTY_QUEUE:
		return "\"stop due to empty queue\"";
	case TEST_STOP_DUE_TO_MAX_REQ_NUM:
		return "\"stop due to max req num\"";
	case TEST_STOP_DUE_TO_THRESHOLD:
		return "\"stop due to exceeding threshold\"";
	case TEST_RET_ABORT:
		return "\"err_check return abort\"";
	case TEST_RET_PARTIAL_FOLLOWED_BY_SUCCESS:
		return "\"err_check return partial followed by success\"";
	case TEST_RET_PARTIAL_FOLLOWED_BY_ABORT:
		return "\"err_check return partial followed by abort\"";
	case TEST_RET_PARTIAL_MULTIPLE_UNTIL_SUCCESS:
		return "\"err_check return partial multiple until success\"";
	case TEST_RET_PARTIAL_MAX_FAIL_IDX:
		return "\"err_check return partial max fail index\"";
	case TEST_RET_RETRY:
		return "\"err_check return retry\"";
	case TEST_RET_CMD_ERR:
		return "\"err_check return cmd error\"";
	case TEST_RET_DATA_ERR:
		return "\"err_check return data error\"";
	case TEST_HDR_INVALID_VERSION:
		return "\"invalid - wrong header version\"";
	case TEST_HDR_WRONG_WRITE_CODE:
		return "\"invalid - wrong write code\"";
	case TEST_HDR_INVALID_RW_CODE:
		return "\"invalid - wrong R/W code\"";
	case TEST_HDR_DIFFERENT_ADDRESSES:
		return "\"invalid - header different addresses\"";
	case TEST_HDR_REQ_NUM_SMALLER_THAN_ACTUAL:
		return "\"invalid - header req num smaller than actual\"";
	case TEST_HDR_REQ_NUM_LARGER_THAN_ACTUAL:
		return "\"invalid - header req num larger than actual\"";
	case TEST_HDR_CMD23_PACKED_BIT_SET:
		return "\"invalid - header cmd23 packed bit set\"";
	case TEST_CMD23_MAX_PACKED_WRITES:
		return "\"invalid - cmd23 max packed writes\"";
	case TEST_CMD23_ZERO_PACKED_WRITES:
		return "\"invalid - cmd23 zero packed writes\"";
	case TEST_CMD23_PACKED_BIT_UNSET:
		return "\"invalid - cmd23 packed bit unset\"";
	case TEST_CMD23_REL_WR_BIT_SET:
		return "\"invalid - cmd23 rel wr bit set\"";
	case TEST_CMD23_BITS_16TO29_SET:
		return "\"invalid - cmd23 bits [16-29] set\"";
	case TEST_CMD23_HDR_BLK_NOT_IN_COUNT:
		return "\"invalid - cmd23 header block not in count\"";
	case TEST_PACKING_EXP_N_OVER_TRIGGER:
		return "\"packing control - pack n\"";
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FB_READ:
		return "\"packing control - pack n followed by read\"";
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FLUSH_N:
		return "\"packing control - pack n followed by flush\"";
	case TEST_PACKING_EXP_ONE_OVER_TRIGGER_FB_READ:
		return "\"packing control - pack one followed by read\"";
	case TEST_PACKING_EXP_THRESHOLD_OVER_TRIGGER:
		return "\"packing control - pack threshold\"";
	case TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS:
		return "\"packing control - no packing\"";
	case TEST_PACKING_NOT_EXP_TRIGGER_REQUESTS:
		return "\"packing control - no packing, trigger requests\"";
	case TEST_PACKING_NOT_EXP_TRIGGER_READ_TRIGGER:
		return "\"packing control - no pack, trigger-read-trigger\"";
	case TEST_PACKING_NOT_EXP_TRIGGER_FLUSH_TRIGGER:
		return "\"packing control- no pack, trigger-flush-trigger\"";
	case TEST_PACK_MIX_PACKED_NO_PACKED_PACKED:
		return "\"packing control - mix: pack -> no pack -> pack\"";
	case TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED:
		return "\"packing control - mix: no pack->pack->no pack\"";
	case BKOPS_DELAYED_WORK_LEVEL_1:
		return "\"delayed work BKOPS level 1\"";
	case BKOPS_DELAYED_WORK_LEVEL_1_HPI:
		return "\"delayed work BKOPS level 1 with HPI\"";
	case BKOPS_CANCEL_DELAYED_WORK:
		return "\"cancel delayed BKOPS work\"";
	case BKOPS_URGENT_LEVEL_2:
		return "\"urgent BKOPS level 2\"";
	case BKOPS_URGENT_LEVEL_2_TWO_REQS:
		return "\"urgent BKOPS level 2, followed by a request\"";
	case BKOPS_URGENT_LEVEL_3:
		return "\"urgent BKOPS level 3\"";
	case TEST_LONG_SEQUENTIAL_READ:
		return "\"long sequential read\"";
	case TEST_LONG_SEQUENTIAL_WRITE:
		return "\"long sequential write\"";
	case TEST_NEW_REQ_NOTIFICATION:
		return "\"new request notification test\"";
	default:
		return " Unknown testcase";
	}

	return NULL;
}

/*
 * Compare individual testcase's statistics to the expected statistics:
 * Compare stop reason and number of packing events
 */
static int check_wr_packing_statistics(struct test_data *td)
{
	struct mmc_wr_pack_stats *mmc_packed_stats;
	struct mmc_queue *mq = td->req_q->queuedata;
	int max_packed_reqs = mq->card->ext_csd.max_packed_writes;
	int i;
	struct mmc_card *card = mq->card;
	struct mmc_wr_pack_stats expected_stats;
	int *stop_reason;
	int ret = 0;

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	expected_stats = mbtd->exp_packed_stats;

	mmc_packed_stats = mmc_blk_get_packed_statistics(card);
	if (!mmc_packed_stats) {
		pr_err("%s: NULL mmc_packed_stats", __func__);
		return -EINVAL;
	}

	if (!mmc_packed_stats->packing_events) {
		pr_err("%s: NULL packing_events", __func__);
		return -EINVAL;
	}

	spin_lock(&mmc_packed_stats->lock);

	if (!mmc_packed_stats->enabled) {
		pr_err("%s write packing statistics are not enabled",
			     __func__);
		ret = -EINVAL;
		goto exit_err;
	}

	stop_reason = mmc_packed_stats->pack_stop_reason;

	for (i = 1; i <= max_packed_reqs; ++i) {
		if (mmc_packed_stats->packing_events[i] !=
		    expected_stats.packing_events[i]) {
			pr_err(
			"%s: Wrong pack stats in index %d, got %d, expected %d",
			__func__, i, mmc_packed_stats->packing_events[i],
			       expected_stats.packing_events[i]);
			if (td->fs_wr_reqs_during_test)
				goto cancel_round;
			ret = -EINVAL;
			goto exit_err;
		}
	}

	if (mmc_packed_stats->pack_stop_reason[EXCEEDS_SEGMENTS] !=
	    expected_stats.pack_stop_reason[EXCEEDS_SEGMENTS]) {
		pr_err(
		"%s: Wrong pack stop reason EXCEEDS_SEGMENTS %d, expected %d",
			__func__, stop_reason[EXCEEDS_SEGMENTS],
		       expected_stats.pack_stop_reason[EXCEEDS_SEGMENTS]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

	if (mmc_packed_stats->pack_stop_reason[EXCEEDS_SECTORS] !=
	    expected_stats.pack_stop_reason[EXCEEDS_SECTORS]) {
		pr_err(
		"%s: Wrong pack stop reason EXCEEDS_SECTORS %d, expected %d",
			__func__, stop_reason[EXCEEDS_SECTORS],
		       expected_stats.pack_stop_reason[EXCEEDS_SECTORS]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

	if (mmc_packed_stats->pack_stop_reason[WRONG_DATA_DIR] !=
	    expected_stats.pack_stop_reason[WRONG_DATA_DIR]) {
		pr_err(
		"%s: Wrong pack stop reason WRONG_DATA_DIR %d, expected %d",
		       __func__, stop_reason[WRONG_DATA_DIR],
		       expected_stats.pack_stop_reason[WRONG_DATA_DIR]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

	if (mmc_packed_stats->pack_stop_reason[FLUSH_OR_DISCARD] !=
	    expected_stats.pack_stop_reason[FLUSH_OR_DISCARD]) {
		pr_err(
		"%s: Wrong pack stop reason FLUSH_OR_DISCARD %d, expected %d",
		       __func__, stop_reason[FLUSH_OR_DISCARD],
		       expected_stats.pack_stop_reason[FLUSH_OR_DISCARD]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

	if (mmc_packed_stats->pack_stop_reason[EMPTY_QUEUE] !=
	    expected_stats.pack_stop_reason[EMPTY_QUEUE]) {
		pr_err(
		"%s: Wrong pack stop reason EMPTY_QUEUE %d, expected %d",
		       __func__, stop_reason[EMPTY_QUEUE],
		       expected_stats.pack_stop_reason[EMPTY_QUEUE]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

	if (mmc_packed_stats->pack_stop_reason[REL_WRITE] !=
	    expected_stats.pack_stop_reason[REL_WRITE]) {
		pr_err(
			"%s: Wrong pack stop reason REL_WRITE %d, expected %d",
		       __func__, stop_reason[REL_WRITE],
		       expected_stats.pack_stop_reason[REL_WRITE]);
		if (td->fs_wr_reqs_during_test)
			goto cancel_round;
		ret = -EINVAL;
		goto exit_err;
	}

exit_err:
	spin_unlock(&mmc_packed_stats->lock);
	if (ret && mmc_packed_stats->enabled)
		print_mmc_packing_stats(card);
	return ret;
cancel_round:
	spin_unlock(&mmc_packed_stats->lock);
	test_iosched_set_ignore_round(true);
	return 0;
}

/*
 * Pseudo-randomly choose a seed based on the last seed, and update it in
 * seed_number. then return seed_number (mod max_val), or min_val.
 */
static unsigned int pseudo_random_seed(unsigned int *seed_number,
				       unsigned int min_val,
				       unsigned int max_val)
{
	int ret = 0;

	if (!seed_number)
		return 0;

	*seed_number = ((unsigned int)(((unsigned long)*seed_number *
				(unsigned long)LARGE_PRIME_1) + LARGE_PRIME_2));
	ret = (unsigned int)((*seed_number) % max_val);

	return (ret > min_val ? ret : min_val);
}

/*
 * Given a pseudo-random seed, find a pseudo-random num_of_bios.
 * Make sure that num_of_bios is not larger than TEST_MAX_SECTOR_RANGE
 */
static void pseudo_rnd_num_of_bios(unsigned int *num_bios_seed,
				   unsigned int *num_of_bios)
{
	do {
		*num_of_bios = pseudo_random_seed(num_bios_seed, 1,
						  TEST_MAX_BIOS_PER_REQ);
		if (!(*num_of_bios))
			*num_of_bios = 1;
	} while ((*num_of_bios) * BIO_U32_SIZE * 4 > TEST_MAX_SECTOR_RANGE);
}

/* Add a single read request to the given td's request queue */
static int prepare_request_add_read(struct test_data *td)
{
	int ret;
	int start_sec;

	if (td)
		start_sec = td->start_sector;
	else {
		pr_err("%s: NULL td", __func__);
		return 0;
	}

	pr_info("%s: Adding a read request, first req_id=%d", __func__,
		     td->wr_rd_next_req_id);

	ret = test_iosched_add_wr_rd_test_req(0, READ, start_sec, 2,
					      TEST_PATTERN_5A, NULL);
	if (ret) {
		pr_err("%s: failed to add a read request", __func__);
		return ret;
	}

	return 0;
}

/* Add a single flush request to the given td's request queue */
static int prepare_request_add_flush(struct test_data *td)
{
	int ret;

	if (!td) {
		pr_err("%s: NULL td", __func__);
		return 0;
	}

	pr_info("%s: Adding a flush request, first req_id=%d", __func__,
		     td->unique_next_req_id);
	ret = test_iosched_add_unique_test_req(0, REQ_UNIQUE_FLUSH,
				  0, 0, NULL);
	if (ret) {
		pr_err("%s: failed to add a flush request", __func__);
		return ret;
	}

	return ret;
}

/*
 * Add num_requets amount of write requests to the given td's request queue.
 * If random test mode is chosen we pseudo-randomly choose the number of bios
 * for each write request, otherwise add between 1 to 5 bio per request.
 */
static int prepare_request_add_write_reqs(struct test_data *td,
					  int num_requests, int is_err_expected,
					  int is_random)
{
	int i;
	unsigned int start_sec;
	int num_bios;
	int ret = 0;
	unsigned int *bio_seed = &mbtd->random_test_seed;

	if (td)
		start_sec = td->start_sector;
	else {
		pr_err("%s: NULL td", __func__);
		return ret;
	}

	pr_info("%s: Adding %d write requests, first req_id=%d", __func__,
		     num_requests, td->wr_rd_next_req_id);

	for (i = 1 ; i <= num_requests ; i++) {
		start_sec =
			td->start_sector + sizeof(int) *
			BIO_U32_SIZE * td->num_of_write_bios;
		if (is_random)
			pseudo_rnd_num_of_bios(bio_seed, &num_bios);
		else
			/*
			 * For the non-random case, give num_bios a value
			 * between 1 and 5, to keep a small number of BIOs
			 */
			num_bios = (i%5)+1;

		ret = test_iosched_add_wr_rd_test_req(is_err_expected, WRITE,
				start_sec, num_bios, TEST_PATTERN_5A, NULL);

		if (ret) {
			pr_err("%s: failed to add a write request",
				    __func__);
			return ret;
		}
	}
	return 0;
}

/*
 * Prepare the write, read and flush requests for a generic packed commands
 * testcase
 */
static int prepare_packed_requests(struct test_data *td, int is_err_expected,
				   int num_requests, int is_random)
{
	int ret = 0;
	struct mmc_queue *mq;
	int max_packed_reqs;
	struct request_queue *req_q;

	if (!td) {
		pr_err("%s: NULL td", __func__);
		return -EINVAL;
	}

	req_q = td->req_q;

	if (!req_q) {
		pr_err("%s: NULL request queue", __func__);
		return -EINVAL;
	}

	mq = req_q->queuedata;
	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_packed_reqs = mq->card->ext_csd.max_packed_writes;

	if (mbtd->random_test_seed <= 0) {
		mbtd->random_test_seed =
			(unsigned int)(get_jiffies_64() & 0xFFFF);
		pr_info("%s: got seed from jiffies %d",
			     __func__, mbtd->random_test_seed);
	}

	ret = prepare_request_add_write_reqs(td, num_requests, is_err_expected,
					     is_random);
	if (ret)
		return ret;

	/* Avoid memory corruption in upcoming stats set */
	if (td->test_info.testcase == TEST_STOP_DUE_TO_THRESHOLD)
		num_requests--;

	memset((void *)mbtd->exp_packed_stats.pack_stop_reason, 0,
		sizeof(mbtd->exp_packed_stats.pack_stop_reason));
	memset(mbtd->exp_packed_stats.packing_events, 0,
		(max_packed_reqs + 1) * sizeof(u32));
	if (num_requests <= max_packed_reqs)
		mbtd->exp_packed_stats.packing_events[num_requests] = 1;

	switch (td->test_info.testcase) {
	case TEST_STOP_DUE_TO_FLUSH:
	case TEST_STOP_DUE_TO_FLUSH_AFTER_MAX_REQS:
		ret = prepare_request_add_flush(td);
		if (ret)
			return ret;

		mbtd->exp_packed_stats.pack_stop_reason[FLUSH_OR_DISCARD] = 1;
		break;
	case TEST_STOP_DUE_TO_READ:
	case TEST_STOP_DUE_TO_READ_AFTER_MAX_REQS:
		ret = prepare_request_add_read(td);
		if (ret)
			return ret;

		mbtd->exp_packed_stats.pack_stop_reason[WRONG_DATA_DIR] = 1;
		break;
	case TEST_STOP_DUE_TO_THRESHOLD:
		mbtd->exp_packed_stats.packing_events[num_requests] = 1;
		mbtd->exp_packed_stats.packing_events[1] = 1;
		mbtd->exp_packed_stats.pack_stop_reason[THRESHOLD] = 1;
		mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;
		break;
	case TEST_STOP_DUE_TO_MAX_REQ_NUM:
	case TEST_RET_PARTIAL_MAX_FAIL_IDX:
		mbtd->exp_packed_stats.pack_stop_reason[THRESHOLD] = 1;
		break;
	default:
		mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;
	}
	mbtd->num_requests = num_requests;

	return 0;
}

/*
 * Prepare the write, read and flush requests for the packing control
 * testcases
 */
static int prepare_packed_control_tests_requests(struct test_data *td,
			int is_err_expected, int num_requests, int is_random)
{
	int ret = 0;
	struct mmc_queue *mq;
	int max_packed_reqs;
	int temp_num_req = num_requests;
	struct request_queue *req_q;
	int test_packed_trigger;
	int num_packed_reqs;

	if (!td) {
		pr_err("%s: NULL td", __func__);
		return -EINVAL;
	}

	req_q = td->req_q;

	if (!req_q) {
		pr_err("%s: NULL request queue", __func__);
		return -EINVAL;
	}

	mq = req_q->queuedata;
	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_packed_reqs = mq->card->ext_csd.max_packed_writes;
	test_packed_trigger = mq->num_wr_reqs_to_start_packing;
	num_packed_reqs = num_requests - test_packed_trigger;

	if (mbtd->random_test_seed == 0) {
		mbtd->random_test_seed =
			(unsigned int)(get_jiffies_64() & 0xFFFF);
		pr_info("%s: got seed from jiffies %d",
			     __func__, mbtd->random_test_seed);
	}

	if (td->test_info.testcase ==
			TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED) {
		temp_num_req = num_requests;
		num_requests = test_packed_trigger - 1;
	}

	/* Verify that the packing is disabled before starting the test */
	mq->wr_packing_enabled = false;
	mq->num_of_potential_packed_wr_reqs = 0;

	if (td->test_info.testcase == TEST_PACK_MIX_PACKED_NO_PACKED_PACKED) {
		mq->num_of_potential_packed_wr_reqs = test_packed_trigger + 1;
		mq->wr_packing_enabled = true;
		num_requests = test_packed_trigger + 2;
	}

	ret = prepare_request_add_write_reqs(td, num_requests, is_err_expected,
					     is_random);
	if (ret)
		goto exit;

	if (td->test_info.testcase == TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED)
		num_requests = temp_num_req;

	memset((void *)mbtd->exp_packed_stats.pack_stop_reason, 0,
		sizeof(mbtd->exp_packed_stats.pack_stop_reason));
	memset(mbtd->exp_packed_stats.packing_events, 0,
		(max_packed_reqs + 1) * sizeof(u32));

	switch (td->test_info.testcase) {
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FB_READ:
	case TEST_PACKING_EXP_ONE_OVER_TRIGGER_FB_READ:
		ret = prepare_request_add_read(td);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.pack_stop_reason[WRONG_DATA_DIR] = 1;
		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 1;
		break;
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FLUSH_N:
		ret = prepare_request_add_flush(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, num_packed_reqs,
					     is_err_expected, is_random);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;
		mbtd->exp_packed_stats.pack_stop_reason[FLUSH_OR_DISCARD] = 1;
		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 2;
		break;
	case TEST_PACKING_NOT_EXP_TRIGGER_READ_TRIGGER:
		ret = prepare_request_add_read(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, test_packed_trigger,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 1;
		break;
	case TEST_PACKING_NOT_EXP_TRIGGER_FLUSH_TRIGGER:
		ret = prepare_request_add_flush(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, test_packed_trigger,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 1;
		break;
	case TEST_PACK_MIX_PACKED_NO_PACKED_PACKED:
		ret = prepare_request_add_read(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, test_packed_trigger-1,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, num_requests,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.packing_events[num_requests] = 1;
		mbtd->exp_packed_stats.packing_events[num_requests-1] = 1;
		mbtd->exp_packed_stats.pack_stop_reason[WRONG_DATA_DIR] = 1;
		mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;
		break;
	case TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED:
		ret = prepare_request_add_read(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, num_requests,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		ret = prepare_request_add_read(td);
		if (ret)
			goto exit;

		ret = prepare_request_add_write_reqs(td, test_packed_trigger-1,
						    is_err_expected, is_random);
		if (ret)
			goto exit;

		mbtd->exp_packed_stats.pack_stop_reason[WRONG_DATA_DIR] = 1;
		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 1;
		break;
	case TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS:
	case TEST_PACKING_NOT_EXP_TRIGGER_REQUESTS:
		break;
	default:
		BUG_ON(num_packed_reqs < 0);
		mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;
		mbtd->exp_packed_stats.packing_events[num_packed_reqs] = 1;
	}
	mbtd->num_requests = num_requests;

exit:
	return ret;
}

/*
 * Prepare requests for the TEST_RET_PARTIAL_FOLLOWED_BY_ABORT testcase.
 * In this testcase we have mixed error expectations from different
 * write requests, hence the special prepare function.
 */
static int prepare_partial_followed_by_abort(struct test_data *td,
					      int num_requests)
{
	int i, start_address;
	int is_err_expected = 0;
	int ret = 0;
	struct mmc_queue *mq = test_iosched_get_req_queue()->queuedata;
	int max_packed_reqs;

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_packed_reqs = mq->card->ext_csd.max_packed_writes;

	for (i = 1; i <= num_requests; i++) {
		if (i > (num_requests / 2))
			is_err_expected = 1;

		start_address = td->start_sector +
			sizeof(int) * BIO_U32_SIZE * td->num_of_write_bios;
		ret = test_iosched_add_wr_rd_test_req(is_err_expected, WRITE,
				start_address, (i % 5) + 1, TEST_PATTERN_5A,
				NULL);
		if (ret) {
			pr_err("%s: failed to add a write request",
				    __func__);
			return ret;
		}
	}

	memset((void *)&mbtd->exp_packed_stats.pack_stop_reason, 0,
		sizeof(mbtd->exp_packed_stats.pack_stop_reason));
	memset(mbtd->exp_packed_stats.packing_events, 0,
		(max_packed_reqs + 1) * sizeof(u32));
	mbtd->exp_packed_stats.packing_events[num_requests] = 1;
	mbtd->exp_packed_stats.pack_stop_reason[EMPTY_QUEUE] = 1;

	mbtd->num_requests = num_requests;

	return ret;
}

/*
 * Get number of write requests for current testcase. If random test mode was
 * chosen, pseudo-randomly choose the number of requests, otherwise set to
 * two less than the packing threshold.
 */
static int get_num_requests(struct test_data *td)
{
	int *seed = &mbtd->random_test_seed;
	struct request_queue *req_q;
	struct mmc_queue *mq;
	int max_num_requests;
	int num_requests;
	int min_num_requests = 2;
	int is_random = mbtd->is_random;
	int max_for_double;
	int test_packed_trigger;

	req_q = test_iosched_get_req_queue();
	if (req_q)
		mq = req_q->queuedata;
	else {
		pr_err("%s: NULL request queue", __func__);
		return 0;
	}

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_num_requests = mq->card->ext_csd.max_packed_writes;
	num_requests = max_num_requests - 2;
	test_packed_trigger = mq->num_wr_reqs_to_start_packing;

	/*
	 * Here max_for_double is intended for packed control testcases
	 * in which we issue many write requests. It's purpose is to prevent
	 * exceeding max number of req_queue requests.
	 */
	max_for_double = max_num_requests - 10;

	if (td->test_info.testcase ==
				TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS)
		/* Don't expect packing, so issue up to trigger-1 reqs */
		num_requests = test_packed_trigger - 1;

	if (is_random) {
		if (td->test_info.testcase ==
		    TEST_RET_PARTIAL_FOLLOWED_BY_ABORT)
			/*
			 * Here we don't want num_requests to be less than 1
			 * as a consequence of division by 2.
			 */
			min_num_requests = 3;

		if (td->test_info.testcase ==
				TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS)
			/* Don't expect packing, so issue up to trigger reqs */
			max_num_requests = test_packed_trigger;

		num_requests = pseudo_random_seed(seed, min_num_requests,
						  max_num_requests - 1);
	}

	if (td->test_info.testcase ==
				TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS)
		num_requests -= test_packed_trigger;

	if (td->test_info.testcase == TEST_PACKING_EXP_N_OVER_TRIGGER_FLUSH_N)
		num_requests =
		num_requests > max_for_double ? max_for_double : num_requests;

	if (mbtd->test_group == TEST_PACKING_CONTROL_GROUP)
		num_requests += test_packed_trigger;

	if (td->test_info.testcase == TEST_PACKING_NOT_EXP_TRIGGER_REQUESTS)
		num_requests = test_packed_trigger;

	return num_requests;
}

static int prepare_long_read_test_requests(struct test_data *td)
{

	int ret;
	int start_sec;
	int j;
	unsigned long read_test_no_req = LONG_READ_TEST_ACTUAL_NUM_REQS;

	if (td)
		start_sec = td->start_sector;
	else {
		pr_err("%s: NULL td", __func__);
		return -EINVAL;
	}

	pr_info("%s: Adding %lu read requests, first req_id=%d", __func__,
		     read_test_no_req, td->wr_rd_next_req_id);

	for (j = 0; j < LONG_READ_TEST_ACTUAL_NUM_REQS; j++) {

		ret = test_iosched_add_wr_rd_test_req(0, READ,
						start_sec,
						TEST_MAX_BIOS_PER_REQ,
						TEST_NO_PATTERN, NULL);
		if (ret) {
			pr_err("%s: failed to add a read request, err = %d"
				    , __func__, ret);
			return ret;
		}

		start_sec +=
			(TEST_MAX_BIOS_PER_REQ * sizeof(int) * BIO_U32_SIZE);
	}

	return 0;
}

/*
 * An implementation for the prepare_test_fn pointer in the test_info
 * data structure. According to the testcase we add the right number of requests
 * and decide if an error is expected or not.
 */
static int prepare_test(struct test_data *td)
{
	struct mmc_queue *mq = test_iosched_get_req_queue()->queuedata;
	int max_num_requests;
	int num_requests = 0;
	int ret = 0;
	int is_random = mbtd->is_random;
	int test_packed_trigger = mq->num_wr_reqs_to_start_packing;

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_num_requests = mq->card->ext_csd.max_packed_writes;

	if (is_random && mbtd->random_test_seed == 0) {
		mbtd->random_test_seed =
			(unsigned int)(get_jiffies_64() & 0xFFFF);
		pr_info("%s: got seed from jiffies %d",
			__func__, mbtd->random_test_seed);
	}

	num_requests = get_num_requests(td);

	if (mbtd->test_group == TEST_SEND_INVALID_GROUP)
		mq->packed_test_fn =
				test_invalid_packed_cmd;

	if (mbtd->test_group == TEST_ERR_CHECK_GROUP)
		mq->err_check_fn = test_err_check;

	switch (td->test_info.testcase) {
	case TEST_STOP_DUE_TO_FLUSH:
	case TEST_STOP_DUE_TO_READ:
	case TEST_RET_PARTIAL_FOLLOWED_BY_SUCCESS:
	case TEST_RET_PARTIAL_MULTIPLE_UNTIL_SUCCESS:
	case TEST_STOP_DUE_TO_EMPTY_QUEUE:
	case TEST_CMD23_PACKED_BIT_UNSET:
		ret = prepare_packed_requests(td, 0, num_requests, is_random);
		break;
	case TEST_STOP_DUE_TO_FLUSH_AFTER_MAX_REQS:
	case TEST_STOP_DUE_TO_READ_AFTER_MAX_REQS:
		ret = prepare_packed_requests(td, 0, max_num_requests - 1,
					      is_random);
		break;
	case TEST_RET_PARTIAL_FOLLOWED_BY_ABORT:
		ret = prepare_partial_followed_by_abort(td, num_requests);
		break;
	case TEST_STOP_DUE_TO_MAX_REQ_NUM:
	case TEST_RET_PARTIAL_MAX_FAIL_IDX:
		ret = prepare_packed_requests(td, 0, max_num_requests,
					      is_random);
		break;
	case TEST_STOP_DUE_TO_THRESHOLD:
		ret = prepare_packed_requests(td, 0, max_num_requests + 1,
					      is_random);
		break;
	case TEST_RET_ABORT:
	case TEST_RET_RETRY:
	case TEST_RET_CMD_ERR:
	case TEST_RET_DATA_ERR:
	case TEST_HDR_INVALID_VERSION:
	case TEST_HDR_WRONG_WRITE_CODE:
	case TEST_HDR_INVALID_RW_CODE:
	case TEST_HDR_DIFFERENT_ADDRESSES:
	case TEST_HDR_REQ_NUM_SMALLER_THAN_ACTUAL:
	case TEST_HDR_REQ_NUM_LARGER_THAN_ACTUAL:
	case TEST_CMD23_MAX_PACKED_WRITES:
	case TEST_CMD23_ZERO_PACKED_WRITES:
	case TEST_CMD23_REL_WR_BIT_SET:
	case TEST_CMD23_BITS_16TO29_SET:
	case TEST_CMD23_HDR_BLK_NOT_IN_COUNT:
	case TEST_HDR_CMD23_PACKED_BIT_SET:
		ret = prepare_packed_requests(td, 1, num_requests, is_random);
		break;
	case TEST_PACKING_EXP_N_OVER_TRIGGER:
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FB_READ:
	case TEST_PACKING_NOT_EXP_TRIGGER_REQUESTS:
	case TEST_PACKING_NOT_EXP_LESS_THAN_TRIGGER_REQUESTS:
	case TEST_PACK_MIX_PACKED_NO_PACKED_PACKED:
	case TEST_PACK_MIX_NO_PACKED_PACKED_NO_PACKED:
		ret = prepare_packed_control_tests_requests(td, 0, num_requests,
			is_random);
		break;
	case TEST_PACKING_EXP_THRESHOLD_OVER_TRIGGER:
		ret = prepare_packed_control_tests_requests(td, 0,
			max_num_requests, is_random);
		break;
	case TEST_PACKING_EXP_ONE_OVER_TRIGGER_FB_READ:
		ret = prepare_packed_control_tests_requests(td, 0,
			test_packed_trigger + 1,
					is_random);
		break;
	case TEST_PACKING_EXP_N_OVER_TRIGGER_FLUSH_N:
		ret = prepare_packed_control_tests_requests(td, 0, num_requests,
			is_random);
		break;
	case TEST_PACKING_NOT_EXP_TRIGGER_READ_TRIGGER:
	case TEST_PACKING_NOT_EXP_TRIGGER_FLUSH_TRIGGER:
		ret = prepare_packed_control_tests_requests(td, 0,
			test_packed_trigger, is_random);
		break;
	case TEST_LONG_SEQUENTIAL_WRITE:
	case TEST_LONG_SEQUENTIAL_READ:
		ret = prepare_long_read_test_requests(td);
		break;
	default:
		pr_info("%s: Invalid test case...", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int run_packed_test(struct test_data *td)
{
	struct mmc_queue *mq;
	struct request_queue *req_q;

	if (!td) {
		pr_err("%s: NULL td", __func__);
		return -EINVAL;
	}

	req_q = td->req_q;

	if (!req_q) {
		pr_err("%s: NULL request queue", __func__);
		return -EINVAL;
	}

	mq = req_q->queuedata;
	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}
	mmc_blk_init_packed_statistics(mq->card);

	if (td->test_info.testcase != TEST_PACK_MIX_PACKED_NO_PACKED_PACKED) {
		/*
		 * Verify that the packing is disabled before starting the
		 * test
		 */
		mq->wr_packing_enabled = false;
		mq->num_of_potential_packed_wr_reqs = 0;
	}

	__blk_run_queue(td->req_q);

	return 0;
}

/*
 * An implementation for the post_test_fn in the test_info data structure.
 * In our case we just reset the function pointers in the mmc_queue in order for
 * the FS to be able to dispatch it's requests correctly after the test is
 * finished.
 */
static int post_test(struct test_data *td)
{
	struct mmc_queue *mq;

	if (!td)
		return -EINVAL;

	mq = td->req_q->queuedata;

	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	mq->packed_test_fn = NULL;
	mq->err_check_fn = NULL;

	return 0;
}

/*
 * This function checks, based on the current test's test_group, that the
 * packed commands capability and control are set right. In addition, we check
 * if the card supports the packed command feature.
 */
static int validate_packed_commands_settings(void)
{
	struct request_queue *req_q;
	struct mmc_queue *mq;
	int max_num_requests;
	struct mmc_host *host;

	req_q = test_iosched_get_req_queue();
	if (!req_q) {
		pr_err("%s: test_iosched_get_req_queue failed", __func__);
		test_iosched_set_test_result(TEST_FAILED);
		return -EINVAL;
	}

	mq = req_q->queuedata;
	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return -EINVAL;
	}

	max_num_requests = mq->card->ext_csd.max_packed_writes;
	host = mq->card->host;

	if (!(host->caps2 && MMC_CAP2_PACKED_WR)) {
		pr_err("%s: Packed Write capability disabled, exit test",
			    __func__);
		test_iosched_set_test_result(TEST_NOT_SUPPORTED);
		return -EINVAL;
	}

	if (max_num_requests == 0) {
		pr_err(
		"%s: no write packing support, ext_csd.max_packed_writes=%d",
		__func__, mq->card->ext_csd.max_packed_writes);
		test_iosched_set_test_result(TEST_NOT_SUPPORTED);
		return -EINVAL;
	}

	pr_info("%s: max number of packed requests supported is %d ",
		     __func__, max_num_requests);

	switch (mbtd->test_group) {
	case TEST_SEND_WRITE_PACKING_GROUP:
	case TEST_ERR_CHECK_GROUP:
	case TEST_SEND_INVALID_GROUP:
		/* disable the packing control */
		host->caps2 &= ~MMC_CAP2_PACKED_WR_CONTROL;
		break;
	case TEST_PACKING_CONTROL_GROUP:
		host->caps2 |=  MMC_CAP2_PACKED_WR_CONTROL;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * Post test operations for BKOPs test
 * Disable the BKOPs statistics and clear the feature flags
 */
static int bkops_post_test(struct test_data *td)
{
	struct request_queue *q = td->req_q;
	struct mmc_queue *mq = (struct mmc_queue *)q->queuedata;
	struct mmc_card *card = mq->card;

	mmc_card_clr_doing_bkops(mq->card);
	card->ext_csd.raw_bkops_status = 0;

	spin_lock(&card->bkops_info.bkops_stats.lock);
	card->bkops_info.bkops_stats.enabled = false;
	spin_unlock(&card->bkops_info.bkops_stats.lock);

	return 0;
}

/*
 * Verify the BKOPs statsistics
 */
static int check_bkops_result(struct test_data *td)
{
	struct request_queue *q = td->req_q;
	struct mmc_queue *mq = (struct mmc_queue *)q->queuedata;
	struct mmc_card *card = mq->card;
	struct mmc_bkops_stats *bkops_stat;

	if (!card)
		goto fail;

	bkops_stat = &card->bkops_info.bkops_stats;

	pr_info("%s: Test results: bkops:(%d,%d,%d) hpi:%d, suspend:%d",
			__func__,
			bkops_stat->bkops_level[BKOPS_SEVERITY_1_INDEX],
			bkops_stat->bkops_level[BKOPS_SEVERITY_2_INDEX],
			bkops_stat->bkops_level[BKOPS_SEVERITY_3_INDEX],
			bkops_stat->hpi,
			bkops_stat->suspend);

	switch (mbtd->test_info.testcase) {
	case BKOPS_DELAYED_WORK_LEVEL_1:
		if ((bkops_stat->bkops_level[BKOPS_SEVERITY_1_INDEX] == 1) &&
		    (bkops_stat->suspend == 1) &&
		    (bkops_stat->hpi == 0))
			goto exit;
		else
			goto fail;
		break;
	case BKOPS_DELAYED_WORK_LEVEL_1_HPI:
		if ((bkops_stat->bkops_level[BKOPS_SEVERITY_1_INDEX] == 1) &&
		    (bkops_stat->suspend == 0) &&
		    (bkops_stat->hpi == 1))
			goto exit;
		/* this might happen due to timing issues */
		else if
		   ((bkops_stat->bkops_level[BKOPS_SEVERITY_1_INDEX] == 0) &&
		    (bkops_stat->suspend == 0) &&
		    (bkops_stat->hpi == 0))
			goto ignore;
		else
			goto fail;
		break;
	case BKOPS_CANCEL_DELAYED_WORK:
		if ((bkops_stat->bkops_level[BKOPS_SEVERITY_1_INDEX] == 0) &&
		    (bkops_stat->bkops_level[BKOPS_SEVERITY_2_INDEX] == 0) &&
		    (bkops_stat->bkops_level[BKOPS_SEVERITY_3_INDEX] == 0) &&
			(bkops_stat->suspend == 0) &&
			  (bkops_stat->hpi == 0))
			goto exit;
		else
			goto fail;
	case BKOPS_URGENT_LEVEL_2:
	case BKOPS_URGENT_LEVEL_2_TWO_REQS:
		if ((bkops_stat->bkops_level[BKOPS_SEVERITY_2_INDEX] == 1) &&
		    (bkops_stat->suspend == 0) &&
		    (bkops_stat->hpi == 0))
			goto exit;
		else
			goto fail;
	case BKOPS_URGENT_LEVEL_3:
		if ((bkops_stat->bkops_level[BKOPS_SEVERITY_3_INDEX] == 1) &&
		    (bkops_stat->suspend == 0) &&
		    (bkops_stat->hpi == 0))
			goto exit;
		else
			goto fail;
	default:
		return -EINVAL;
	}

exit:
	return 0;
ignore:
	test_iosched_set_ignore_round(true);
	return 0;
fail:
	if (td->fs_wr_reqs_during_test) {
		pr_info("%s: wr reqs during test, cancel the round",
		     __func__);
		test_iosched_set_ignore_round(true);
		return 0;
	}

	pr_info("%s: BKOPs statistics are not as expected, test failed",
		     __func__);
	return -EINVAL;
}

static void bkops_end_io_final_fn(struct request *rq, int err)
{
	struct test_request *test_rq =
		(struct test_request *)rq->elv.priv[0];
	BUG_ON(!test_rq);

	test_rq->req_completed = 1;
	test_rq->req_result = err;

	pr_info("%s: request %d completed, err=%d",
		     __func__, test_rq->req_id, err);

	mbtd->bkops_stage = BKOPS_STAGE_4;
	wake_up(&mbtd->bkops_wait_q);
}

static void bkops_end_io_fn(struct request *rq, int err)
{
	struct test_request *test_rq =
		(struct test_request *)rq->elv.priv[0];
	BUG_ON(!test_rq);

	test_rq->req_completed = 1;
	test_rq->req_result = err;

	pr_info("%s: request %d completed, err=%d",
		     __func__, test_rq->req_id, err);
	mbtd->bkops_stage = BKOPS_STAGE_2;
	wake_up(&mbtd->bkops_wait_q);

}

static int prepare_bkops(struct test_data *td)
{
	int ret = 0;
	struct request_queue *q = td->req_q;
	struct mmc_queue *mq = (struct mmc_queue *)q->queuedata;
	struct mmc_card  *card = mq->card;
	struct mmc_bkops_stats *bkops_stat;

	if (!card)
		return -EINVAL;

	bkops_stat = &card->bkops_info.bkops_stats;

	if (!card->ext_csd.bkops_en) {
		pr_err("%s: BKOPS is not enabled by card or host)",
				__func__);
		return -ENOTSUPP;
	}
	if (mmc_card_doing_bkops(card)) {
		pr_err("%s: BKOPS in progress, try later", __func__);
		return -EAGAIN;
	}

	mmc_blk_init_bkops_statistics(card);

	if ((mbtd->test_info.testcase == BKOPS_URGENT_LEVEL_2) ||
	    (mbtd->test_info.testcase ==  BKOPS_URGENT_LEVEL_2_TWO_REQS) ||
	    (mbtd->test_info.testcase == BKOPS_URGENT_LEVEL_3))
		mq->err_check_fn = test_err_check;
	mbtd->err_check_counter = 0;

	return ret;
}

static int run_bkops(struct test_data *td)
{
	int ret = 0;
	struct request_queue *q = td->req_q;
	struct mmc_queue *mq = (struct mmc_queue *)q->queuedata;
	struct mmc_card  *card = mq->card;
	struct mmc_bkops_stats *bkops_stat;

	if (!card)
		return -EINVAL;

	bkops_stat = &card->bkops_info.bkops_stats;

	switch (mbtd->test_info.testcase) {
	case BKOPS_DELAYED_WORK_LEVEL_1:
		bkops_stat->ignore_card_bkops_status = true;
		card->ext_csd.raw_bkops_status = 1;
		card->bkops_info.sectors_changed =
			card->bkops_info.min_sectors_to_queue_delayed_work + 1;
		mbtd->bkops_stage = BKOPS_STAGE_1;

		__blk_run_queue(q);
		/* this long sleep makes sure the host starts bkops and
		   also, gets into suspend */
		msleep(10000);

		bkops_stat->ignore_card_bkops_status = false;
		card->ext_csd.raw_bkops_status = 0;

		test_iosched_mark_test_completion();
		break;

	case BKOPS_DELAYED_WORK_LEVEL_1_HPI:
		bkops_stat->ignore_card_bkops_status = true;
		card->ext_csd.raw_bkops_status = 1;
		card->bkops_info.sectors_changed =
			card->bkops_info.min_sectors_to_queue_delayed_work + 1;
		mbtd->bkops_stage = BKOPS_STAGE_1;

		__blk_run_queue(q);
		msleep(card->bkops_info.delay_ms);

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				      td->start_sector,
				      TEST_REQUEST_NUM_OF_BIOS,
				      TEST_PATTERN_5A,
				      bkops_end_io_final_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);
		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_4);
		bkops_stat->ignore_card_bkops_status = false;

		test_iosched_mark_test_completion();
		break;

	case BKOPS_CANCEL_DELAYED_WORK:
		bkops_stat->ignore_card_bkops_status = true;
		card->ext_csd.raw_bkops_status = 1;
		card->bkops_info.sectors_changed =
			card->bkops_info.min_sectors_to_queue_delayed_work + 1;
		mbtd->bkops_stage = BKOPS_STAGE_1;

		__blk_run_queue(q);

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				bkops_end_io_final_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);
		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_4);
		bkops_stat->ignore_card_bkops_status = false;

		test_iosched_mark_test_completion();
		break;

	case BKOPS_URGENT_LEVEL_2:
	case BKOPS_URGENT_LEVEL_3:
		bkops_stat->ignore_card_bkops_status = true;
		if (mbtd->test_info.testcase == BKOPS_URGENT_LEVEL_2)
			card->ext_csd.raw_bkops_status = 2;
		else
			card->ext_csd.raw_bkops_status = 3;
		mbtd->bkops_stage = BKOPS_STAGE_1;

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				bkops_end_io_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);
		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_2);
		card->ext_csd.raw_bkops_status = 0;

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				bkops_end_io_final_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);

		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_4);

		bkops_stat->ignore_card_bkops_status = false;
		test_iosched_mark_test_completion();
		break;

	case BKOPS_URGENT_LEVEL_2_TWO_REQS:
		mq->wr_packing_enabled = false;
		bkops_stat->ignore_card_bkops_status = true;
		card->ext_csd.raw_bkops_status = 2;
		mbtd->bkops_stage = BKOPS_STAGE_1;

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				NULL);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				bkops_end_io_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);
		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_2);
		card->ext_csd.raw_bkops_status = 0;

		ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				td->start_sector,
				TEST_REQUEST_NUM_OF_BIOS,
				TEST_PATTERN_5A,
				bkops_end_io_final_fn);
		if (ret) {
			pr_err("%s: failed to add a write request",
					__func__);
			ret = -EINVAL;
			break;
		}

		__blk_run_queue(q);

		wait_event(mbtd->bkops_wait_q,
			   mbtd->bkops_stage == BKOPS_STAGE_4);

		bkops_stat->ignore_card_bkops_status = false;
		test_iosched_mark_test_completion();

		break;
	default:
		pr_err("%s: wrong testcase: %d", __func__,
			    mbtd->test_info.testcase);
		ret = -EINVAL;
	}
	return ret;
}

/*
 * new_req_post_test() - Do post test operations for
 * new_req_notification test: disable the statistics and clear
 * the feature flags.
 * @td		The test_data for the new_req test that has
 *		ended.
 */
static int new_req_post_test(struct test_data *td)
{
	struct mmc_queue *mq;

	if (!td || !td->req_q)
		goto exit;

	mq = (struct mmc_queue *)td->req_q->queuedata;

	if (!mq || !mq->card)
		goto exit;

	pr_info("Completed %d requests",
			mbtd->completed_req_count);

exit:
	return 0;
}

/*
 * check_new_req_result() - Print out the number of completed
 * requests. Assigned to the check_test_result_fn pointer,
 * therefore the name.
 * @td		The test_data for the new_req test that has
 *		ended.
 */
static int check_new_req_result(struct test_data *td)
{
	pr_info("%s: Test results: Completed %d requests",
			__func__, mbtd->completed_req_count);
	return 0;
}

/*
 * new_req_free_end_io_fn() - Remove request from queuelist and
 * free request's allocated memory. Used as a call-back
 * assigned to end_io member in request struct.
 * @rq		The request to be freed
 * @err		Unused
 */
static void new_req_free_end_io_fn(struct request *rq, int err)
{
	struct test_request *test_rq =
		(struct test_request *)rq->elv.priv[0];
	struct test_data *ptd = test_get_test_data();

	BUG_ON(!test_rq);

	spin_lock_irq(&ptd->lock);
	list_del_init(&test_rq->queuelist);
	ptd->dispatched_count--;
	spin_unlock_irq(&ptd->lock);

	__blk_put_request(ptd->req_q, test_rq->rq);
	kfree(test_rq->bios_buffer);
	kfree(test_rq);
	mbtd->completed_req_count++;
}

static int prepare_new_req(struct test_data *td)
{
	struct request_queue *q = td->req_q;
	struct mmc_queue *mq = (struct mmc_queue *)q->queuedata;

	mmc_blk_init_packed_statistics(mq->card);
	mbtd->completed_req_count = 0;

	return 0;
}

static int run_new_req(struct test_data *ptd)
{
	int ret = 0;
	int i;
	unsigned int requests_count = 2;
	unsigned int bio_num;
	struct test_request *test_rq = NULL;

	while (1) {
		for (i = 0; i < requests_count; i++) {
			bio_num =  TEST_MAX_BIOS_PER_REQ;
			test_rq = test_iosched_create_test_req(0, READ,
					ptd->start_sector,
					bio_num, TEST_PATTERN_5A,
					new_req_free_end_io_fn);
			if (test_rq) {
				spin_lock_irq(ptd->req_q->queue_lock);
				list_add_tail(&test_rq->queuelist,
					      &ptd->test_queue);
				ptd->test_count++;
				spin_unlock_irq(ptd->req_q->queue_lock);
			} else {
				pr_err("%s: failed to create read request",
					     __func__);
				ret = -ENODEV;
				break;
			}
		}

		__blk_run_queue(ptd->req_q);
		/* wait while a mmc layer will send all requests in test_queue*/
		while (!list_empty(&ptd->test_queue))
			msleep(NEW_REQ_TEST_SLEEP_TIME);

		/* test finish criteria */
		if (mbtd->completed_req_count > 1000) {
			if (ptd->dispatched_count)
				continue;
			else
				break;
		}

		for (i = 0; i < requests_count; i++) {
			bio_num =  NEW_REQ_TEST_NUM_BIOS;
			test_rq = test_iosched_create_test_req(0, READ,
					ptd->start_sector,
					bio_num, TEST_PATTERN_5A,
					new_req_free_end_io_fn);
			if (test_rq) {
				spin_lock_irq(ptd->req_q->queue_lock);
				list_add_tail(&test_rq->queuelist,
					      &ptd->test_queue);
				ptd->test_count++;
				spin_unlock_irq(ptd->req_q->queue_lock);
			} else {
				pr_err("%s: failed to create read request",
					     __func__);
				ret = -ENODEV;
				break;
			}
		}
		__blk_run_queue(ptd->req_q);
	}

	test_iosched_mark_test_completion();
	pr_info("%s: EXIT: %d code", __func__, ret);

	return ret;
}

static bool message_repeat;
static int test_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	message_repeat = 1;
	return 0;
}

/* send_packing TEST */
static ssize_t send_write_packing_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	int j = 0;

	pr_info("%s: -- send_write_packing TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;


	mbtd->test_group = TEST_SEND_WRITE_PACKING_GROUP;

	if (validate_packed_commands_settings())
		return count;

	if (mbtd->random_test_seed > 0)
		pr_info("%s: Test seed: %d", __func__,
			      mbtd->random_test_seed);

	memset(&mbtd->test_info, 0, sizeof(struct test_info));

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_test;
	mbtd->test_info.run_test_fn = run_packed_test;
	mbtd->test_info.check_test_result_fn = check_wr_packing_statistics;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.post_test_fn = post_test;

	for (i = 0; i < number; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		for (j = SEND_WRITE_PACKING_MIN_TESTCASE;
		      j <= SEND_WRITE_PACKING_MAX_TESTCASE; j++) {

			mbtd->is_random = RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				break;
			/* Allow FS requests to be dispatched */
			msleep(1000);
			mbtd->test_info.testcase = j;
			mbtd->is_random = NON_RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				break;
			/* Allow FS requests to be dispatched */
			msleep(1000);
		}
	}

	pr_info("%s: Completed all the test cases.", __func__);

	return count;
}

static ssize_t send_write_packing_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nsend_write_packing_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test checks the following scenarios\n"
		 "- Pack due to FLUSH message\n"
		 "- Pack due to FLUSH after threshold writes\n"
		 "- Pack due to READ message\n"
		 "- Pack due to READ after threshold writes\n"
		 "- Pack due to empty queue\n"
		 "- Pack due to threshold writes\n"
		 "- Pack due to one over threshold writes\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else {
		return 0;
	}
}

const struct file_operations send_write_packing_test_ops = {
	.open = test_open,
	.write = send_write_packing_test_write,
	.read = send_write_packing_test_read,
};

/* err_check TEST */
static ssize_t err_check_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	int j = 0;

	pr_info("%s: -- err_check TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	mbtd->test_group = TEST_ERR_CHECK_GROUP;

	if (validate_packed_commands_settings())
		return count;

	if (mbtd->random_test_seed > 0)
		pr_info("%s: Test seed: %d", __func__,
			      mbtd->random_test_seed);

	memset(&mbtd->test_info, 0, sizeof(struct test_info));

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_test;
	mbtd->test_info.run_test_fn = run_packed_test;
	mbtd->test_info.check_test_result_fn = check_wr_packing_statistics;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.post_test_fn = post_test;

	for (i = 0; i < number; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		for (j = ERR_CHECK_MIN_TESTCASE;
					j <= ERR_CHECK_MAX_TESTCASE ; j++) {
			mbtd->test_info.testcase = j;
			mbtd->is_random = RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				break;
			/* Allow FS requests to be dispatched */
			msleep(1000);
			mbtd->test_info.testcase = j;
			mbtd->is_random = NON_RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				break;
			/* Allow FS requests to be dispatched */
			msleep(1000);
		}
	}

	pr_info("%s: Completed all the test cases.", __func__);

	return count;
}

static ssize_t err_check_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nerr_check_TEST\n"
		 "=========\n"
		 "Description:\n"
		 "This test checks the following scenarios\n"
		 "- Return ABORT\n"
		 "- Return PARTIAL followed by success\n"
		 "- Return PARTIAL followed by abort\n"
		 "- Return PARTIAL multiple times until success\n"
		 "- Return PARTIAL with fail index = threshold\n"
		 "- Return RETRY\n"
		 "- Return CMD_ERR\n"
		 "- Return DATA_ERR\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else {
		return 0;
	}
}

const struct file_operations err_check_test_ops = {
	.open = test_open,
	.write = err_check_test_write,
	.read = err_check_test_read,
};

/* send_invalid_packed TEST */
static ssize_t send_invalid_packed_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	int j = 0;
	int num_of_failures = 0;

	pr_info("%s: -- send_invalid_packed TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	mbtd->test_group = TEST_SEND_INVALID_GROUP;

	if (validate_packed_commands_settings())
		return count;

	if (mbtd->random_test_seed > 0)
		pr_info("%s: Test seed: %d", __func__,
			      mbtd->random_test_seed);

	memset(&mbtd->test_info, 0, sizeof(struct test_info));

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_test;
	mbtd->test_info.run_test_fn = run_packed_test;
	mbtd->test_info.check_test_result_fn = check_wr_packing_statistics;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.post_test_fn = post_test;

	for (i = 0; i < number; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		for (j = INVALID_CMD_MIN_TESTCASE;
				j <= INVALID_CMD_MAX_TESTCASE ; j++) {

			mbtd->test_info.testcase = j;
			mbtd->is_random = RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				num_of_failures++;
			/* Allow FS requests to be dispatched */
			msleep(1000);

			mbtd->test_info.testcase = j;
			mbtd->is_random = NON_RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				num_of_failures++;
			/* Allow FS requests to be dispatched */
			msleep(1000);
		}
	}

	pr_info("%s: Completed all the test cases.", __func__);

	if (num_of_failures > 0) {
		test_iosched_set_test_result(TEST_FAILED);
		pr_err(
			"There were %d failures during the test, TEST FAILED",
			num_of_failures);
	}
	return count;
}

static ssize_t send_invalid_packed_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nsend_invalid_packed_TEST\n"
		 "=========\n"
		 "Description:\n"
		 "This test checks the following scenarios\n"
		 "- Send an invalid header version\n"
		 "- Send the wrong write code\n"
		 "- Send an invalid R/W code\n"
		 "- Send wrong start address in header\n"
		 "- Send header with block_count smaller than actual\n"
		 "- Send header with block_count larger than actual\n"
		 "- Send header CMD23 packed bit set\n"
		 "- Send CMD23 with block count over threshold\n"
		 "- Send CMD23 with block_count equals zero\n"
		 "- Send CMD23 packed bit unset\n"
		 "- Send CMD23 reliable write bit set\n"
		 "- Send CMD23 bits [16-29] set\n"
		 "- Send CMD23 header block not in block_count\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else {
		return 0;
	}
}

const struct file_operations send_invalid_packed_test_ops = {
	.open = test_open,
	.write = send_invalid_packed_test_write,
	.read = send_invalid_packed_test_read,
};

/* packing_control TEST */
static ssize_t write_packing_control_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	int j = 0;
	struct mmc_queue *mq = test_iosched_get_req_queue()->queuedata;
	int max_num_requests = mq->card->ext_csd.max_packed_writes;
	int test_successful = 1;

	pr_info("%s: -- write_packing_control TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	pr_info("%s: max_num_requests = %d ", __func__,
			max_num_requests);

	memset(&mbtd->test_info, 0, sizeof(struct test_info));
	mbtd->test_group = TEST_PACKING_CONTROL_GROUP;

	if (validate_packed_commands_settings())
		return count;

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_test;
	mbtd->test_info.run_test_fn = run_packed_test;
	mbtd->test_info.check_test_result_fn = check_wr_packing_statistics;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;

	for (i = 0; i < number; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		for (j = PACKING_CONTROL_MIN_TESTCASE;
				j <= PACKING_CONTROL_MAX_TESTCASE; j++) {

			test_successful = 1;
			mbtd->test_info.testcase = j;
			mbtd->is_random = RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret) {
				test_successful = 0;
				break;
			}
			/* Allow FS requests to be dispatched */
			msleep(1000);

			mbtd->test_info.testcase = j;
			mbtd->is_random = NON_RANDOM_TEST;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret) {
				test_successful = 0;
				break;
			}
			/* Allow FS requests to be dispatched */
			msleep(1000);
		}

		if (!test_successful)
			break;
	}

	pr_info("%s: Completed all the test cases.", __func__);

	return count;
}

static ssize_t write_packing_control_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nwrite_packing_control_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test checks the following scenarios\n"
		 "- Packing expected - one over trigger\n"
		 "- Packing expected - N over trigger\n"
		 "- Packing expected - N over trigger followed by read\n"
		 "- Packing expected - N over trigger followed by flush\n"
		 "- Packing expected - threshold over trigger FB by flush\n"
		 "- Packing not expected - less than trigger\n"
		 "- Packing not expected - trigger requests\n"
		 "- Packing not expected - trigger, read, trigger\n"
		 "- Mixed state - packing -> no packing -> packing\n"
		 "- Mixed state - no packing -> packing -> no packing\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else {
		return 0;
	}
}

const struct file_operations write_packing_control_test_ops = {
	.open = test_open,
	.write = write_packing_control_test_write,
	.read = write_packing_control_test_read,
};

static ssize_t bkops_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0, j;
	int number = -1;

	pr_info("%s: -- bkops_test TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	mbtd->test_group = TEST_BKOPS_GROUP;

	memset(&mbtd->test_info, 0, sizeof(struct test_info));

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_bkops;
	mbtd->test_info.check_test_result_fn = check_bkops_result;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.run_test_fn = run_bkops;
	mbtd->test_info.timeout_msec = BKOPS_TEST_TIMEOUT;
	mbtd->test_info.post_test_fn = bkops_post_test;

	for (i = 0 ; i < number ; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ===================", __func__);
		for (j = BKOPS_MIN_TESTCASE ;
				j <= BKOPS_MAX_TESTCASE ; j++) {
			mbtd->test_info.testcase = j;
			ret = test_iosched_start_test(&mbtd->test_info);
			if (ret)
				break;
		}
	}

	pr_info("%s: Completed all the test cases.", __func__);

	return count;
}

static ssize_t bkops_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nbkops_test\n========================\n"
		 "Description:\n"
		 "This test simulates BKOPS status from card\n"
		 "and verifies that:\n"
		 " - Starting BKOPS delayed work, level 1\n"
		 " - Starting BKOPS delayed work, level 1, with HPI\n"
		 " - Cancel starting BKOPS delayed work, "
		 " when a request is received\n"
		 " - Starting BKOPS urgent, level 2,3\n"
		 " - Starting BKOPS urgent with 2 requests\n");
	return strnlen(buffer, count);
}

const struct file_operations bkops_test_ops = {
	.open = test_open,
	.write = bkops_test_write,
	.read = bkops_test_read,
};

static ssize_t long_sequential_read_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	unsigned long mtime, integer, fraction;
	unsigned long test_size_integer, test_size_fraction;

	test_size_integer = LONG_TEST_SIZE_INTEGER(LONG_READ_NUM_BYTES);
	test_size_fraction = LONG_TEST_SIZE_FRACTION(LONG_READ_NUM_BYTES);

	pr_info("%s: -- Long Sequential Read TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	memset(&mbtd->test_info, 0, sizeof(struct test_info));
	mbtd->test_group = TEST_GENERAL_GROUP;

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_test;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;

	for (i = 0 ; i < number ; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		mbtd->test_info.testcase = TEST_LONG_SEQUENTIAL_READ;
		mbtd->is_random = NON_RANDOM_TEST;
		ret = test_iosched_start_test(&mbtd->test_info);
		if (ret)
			break;

		mtime = ktime_to_ms(mbtd->test_info.test_duration);

		pr_info("%s: time is %lu msec, size is %lu.%lu MiB",
			__func__, mtime, test_size_integer, test_size_fraction);

		/* we first multiply in order not to lose precision */
		mtime *= MB_MSEC_RATIO_APPROXIMATION;
		/* divide values to get a MiB/sec integer value with one
		   digit of precision. Multiply by 10 for one digit precision
		 */
		fraction = integer = (LONG_READ_NUM_BYTES * 10) / mtime;
		integer /= 10;
		/* and calculate the MiB value fraction */
		fraction -= integer * 10;

		pr_info("%s: Throughput: %lu.%lu MiB/sec"
			, __func__, integer, fraction);

		/* Allow FS requests to be dispatched */
		msleep(1000);
	}

	return count;
}

static ssize_t long_sequential_read_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nlong_sequential_read_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test runs the following scenarios\n"
		 "- Long Sequential Read Test: this test measures read "
		 "throughput at the driver level by sequentially reading many "
		 "large requests.\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else
		return 0;
}

const struct file_operations long_sequential_read_test_ops = {
	.open = test_open,
	.write = long_sequential_read_test_write,
	.read = long_sequential_read_test_read,
};

static void long_seq_write_free_end_io_fn(struct request *rq, int err)
{
	struct test_request *test_rq =
		(struct test_request *)rq->elv.priv[0];
	struct test_data *ptd = test_get_test_data();

	BUG_ON(!test_rq);

	spin_lock_irq(&ptd->lock);
	list_del_init(&test_rq->queuelist);
	ptd->dispatched_count--;
	__blk_put_request(ptd->req_q, test_rq->rq);
	spin_unlock_irq(&ptd->lock);

	kfree(test_rq->bios_buffer);
	kfree(test_rq);
	mbtd->completed_req_count++;

	check_test_completion();
}

static int run_long_seq_write(struct test_data *td)
{
	int ret = 0;
	int i;
	int num_requests = TEST_MAX_REQUESTS / 2;

	td->test_count = 0;
	mbtd->completed_req_count = 0;

	pr_info("%s: Adding at least %d write requests, first req_id=%d",
		     __func__, LONG_WRITE_TEST_MIN_NUM_REQS,
		     td->wr_rd_next_req_id);

	do {
		for (i = 0; i < num_requests; i++) {
			/*
			 * since our requests come from a pool containing 128
			 * requests, we don't want to exhaust this quantity,
			 * therefore we add up to num_requests (which
			 * includes a safety margin) and then call the mmc layer
			 * to fetch them
			 */
			if (td->test_count > num_requests)
				break;

			ret = test_iosched_add_wr_rd_test_req(0, WRITE,
				  td->start_sector, TEST_MAX_BIOS_PER_REQ,
				  TEST_PATTERN_5A,
				  long_seq_write_free_end_io_fn);
			 if (ret) {
				pr_err("%s: failed to create write request"
					    , __func__);
				break;
			}
		}

		__blk_run_queue(td->req_q);

	} while (mbtd->completed_req_count < LONG_WRITE_TEST_MIN_NUM_REQS);

	pr_info("%s: completed %d requests", __func__,
		     mbtd->completed_req_count);

	return ret;
}

static ssize_t long_sequential_write_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;
	unsigned long mtime, integer, fraction, byte_count;

	pr_info("%s: -- Long Sequential Write TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	memset(&mbtd->test_info, 0, sizeof(struct test_info));
	mbtd->test_group = TEST_GENERAL_GROUP;

	mbtd->test_info.data = mbtd;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.run_test_fn = run_long_seq_write;

	for (i = 0 ; i < number ; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ====================", __func__);

		integer = 0;
		fraction = 0;
		mbtd->test_info.test_byte_count = 0;
		mbtd->test_info.testcase = TEST_LONG_SEQUENTIAL_WRITE;
		mbtd->is_random = NON_RANDOM_TEST;
		ret = test_iosched_start_test(&mbtd->test_info);
		if (ret)
			break;

		mtime = ktime_to_ms(mbtd->test_info.test_duration);
		byte_count = mbtd->test_info.test_byte_count;

		pr_info("%s: time is %lu msec, size is %lu.%lu MiB",
			__func__, mtime, LONG_TEST_SIZE_INTEGER(byte_count),
			      LONG_TEST_SIZE_FRACTION(byte_count));

		/* we first multiply in order not to lose precision */
		mtime *= MB_MSEC_RATIO_APPROXIMATION;
		/* divide values to get a MiB/sec integer value with one
		   digit of precision
		 */
		fraction = integer = (byte_count * 10) / mtime;
		integer /= 10;
		/* and calculate the MiB value fraction */
		fraction -= integer * 10;

		pr_info("%s: Throughput: %lu.%lu MiB/sec",
			__func__, integer, fraction);

		/* Allow FS requests to be dispatched */
		msleep(1000);
	}

	return count;
}

static ssize_t long_sequential_write_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nlong_sequential_write_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test runs the following scenarios\n"
		 "- Long Sequential Write Test: this test measures write "
		 "throughput at the driver level by sequentially writing many "
		 "large requests\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else
		return 0;
}

const struct file_operations long_sequential_write_test_ops = {
	.open = test_open,
	.write = long_sequential_write_test_write,
	.read = long_sequential_write_test_read,
};

static ssize_t new_req_notification_test_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	int number = -1;

	pr_info("%s: -- new_req_notification TEST --", __func__);

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	mbtd->test_group = TEST_NEW_NOTIFICATION_GROUP;

	memset(&mbtd->test_info, 0, sizeof(struct test_info));

	mbtd->test_info.data = mbtd;
	mbtd->test_info.prepare_test_fn = prepare_new_req;
	mbtd->test_info.check_test_result_fn = check_new_req_result;
	mbtd->test_info.get_test_case_str_fn = get_test_case_str;
	mbtd->test_info.run_test_fn = run_new_req;
	mbtd->test_info.timeout_msec = 10 * 60 * 1000; /* 1 min */
	mbtd->test_info.post_test_fn = new_req_post_test;

	for (i = 0 ; i < number ; ++i) {
		pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		pr_info("%s: ===================", __func__);
		pr_info("%s: start test case TEST_NEW_REQ_NOTIFICATION",
			      __func__);
		mbtd->test_info.testcase = TEST_NEW_REQ_NOTIFICATION;
		ret = test_iosched_start_test(&mbtd->test_info);
		if (ret) {
			pr_info("%s: break from new_req tests loop",
				      __func__);
			break;
		}
	}
	return count;
}

static ssize_t new_req_notification_test_read(struct file *file,
			       char __user *buffer,
			       size_t count,
			       loff_t *offset)
{
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	memset((void *)buffer, 0, count);

	snprintf(buffer, count,
		 "\nnew_req_notification_test\n========================\n"
		 "Description:\n"
		 "This test checks following scenarious\n"
		 "- new request arrives after a NULL request was sent to the "
		 "mmc_queue,\n"
		 "which is waiting for completion of a former request\n");

	return strnlen(buffer, count);
}

const struct file_operations new_req_notification_test_ops = {
	.open = test_open,
	.write = new_req_notification_test_write,
	.read = new_req_notification_test_read,
};

static void mmc_block_test_debugfs_cleanup(void)
{
	debugfs_remove(mbtd->debug.random_test_seed);
	debugfs_remove(mbtd->debug.send_write_packing_test);
	debugfs_remove(mbtd->debug.err_check_test);
	debugfs_remove(mbtd->debug.send_invalid_packed_test);
	debugfs_remove(mbtd->debug.packing_control_test);
	debugfs_remove(mbtd->debug.bkops_test);
	debugfs_remove(mbtd->debug.long_sequential_read_test);
	debugfs_remove(mbtd->debug.long_sequential_write_test);
	debugfs_remove(mbtd->debug.new_req_notification_test);
}

static int mmc_block_test_debugfs_init(void)
{
	struct dentry *utils_root, *tests_root;

	utils_root = test_iosched_get_debugfs_utils_root();
	tests_root = test_iosched_get_debugfs_tests_root();

	if (!utils_root || !tests_root)
		return -EINVAL;

	mbtd->debug.random_test_seed = debugfs_create_u32(
					"random_test_seed",
					S_IRUGO | S_IWUGO,
					utils_root,
					&mbtd->random_test_seed);

	if (!mbtd->debug.random_test_seed)
		goto err_nomem;

	mbtd->debug.send_write_packing_test =
		debugfs_create_file("send_write_packing_test",
				    S_IRUGO | S_IWUGO,
				    tests_root,
				    NULL,
				    &send_write_packing_test_ops);

	if (!mbtd->debug.send_write_packing_test)
		goto err_nomem;

	mbtd->debug.err_check_test =
		debugfs_create_file("err_check_test",
				    S_IRUGO | S_IWUGO,
				    tests_root,
				    NULL,
				    &err_check_test_ops);

	if (!mbtd->debug.err_check_test)
		goto err_nomem;

	mbtd->debug.send_invalid_packed_test =
		debugfs_create_file("send_invalid_packed_test",
				    S_IRUGO | S_IWUGO,
				    tests_root,
				    NULL,
				    &send_invalid_packed_test_ops);

	if (!mbtd->debug.send_invalid_packed_test)
		goto err_nomem;

	mbtd->debug.packing_control_test = debugfs_create_file(
					"packing_control_test",
					S_IRUGO | S_IWUGO,
					tests_root,
					NULL,
					&write_packing_control_test_ops);

	if (!mbtd->debug.packing_control_test)
		goto err_nomem;

	mbtd->debug.bkops_test =
		debugfs_create_file("bkops_test",
				    S_IRUGO | S_IWUGO,
				    tests_root,
				    NULL,
				    &bkops_test_ops);

	mbtd->debug.new_req_notification_test =
		debugfs_create_file("new_req_notification_test",
				    S_IRUGO | S_IWUGO,
				    tests_root,
				    NULL,
				    &new_req_notification_test_ops);

	if (!mbtd->debug.new_req_notification_test)
		goto err_nomem;

	if (!mbtd->debug.bkops_test)
		goto err_nomem;

	mbtd->debug.long_sequential_read_test = debugfs_create_file(
					"long_sequential_read_test",
					S_IRUGO | S_IWUGO,
					tests_root,
					NULL,
					&long_sequential_read_test_ops);

	if (!mbtd->debug.long_sequential_read_test)
		goto err_nomem;

	mbtd->debug.long_sequential_write_test = debugfs_create_file(
					"long_sequential_write_test",
					S_IRUGO | S_IWUGO,
					tests_root,
					NULL,
					&long_sequential_write_test_ops);

	if (!mbtd->debug.long_sequential_write_test)
		goto err_nomem;

	return 0;

err_nomem:
	mmc_block_test_debugfs_cleanup();
	return -ENOMEM;
}

static void mmc_block_test_probe(void)
{
	struct request_queue *q = test_iosched_get_req_queue();
	struct mmc_queue *mq;
	int max_packed_reqs;

	if (!q) {
		pr_err("%s: NULL request queue", __func__);
		return;
	}

	mq = q->queuedata;
	if (!mq) {
		pr_err("%s: NULL mq", __func__);
		return;
	}

	max_packed_reqs = mq->card->ext_csd.max_packed_writes;
	mbtd->exp_packed_stats.packing_events =
			kzalloc((max_packed_reqs + 1) *
				sizeof(*mbtd->exp_packed_stats.packing_events),
				GFP_KERNEL);

	mmc_block_test_debugfs_init();
}

static void mmc_block_test_remove(void)
{
	mmc_block_test_debugfs_cleanup();
}

static int __init mmc_block_test_init(void)
{
	mbtd = kzalloc(sizeof(struct mmc_block_test_data), GFP_KERNEL);
	if (!mbtd) {
		pr_err("%s: failed to allocate mmc_block_test_data",
			    __func__);
		return -ENODEV;
	}

	init_waitqueue_head(&mbtd->bkops_wait_q);
	mbtd->bdt.init_fn = mmc_block_test_probe;
	mbtd->bdt.exit_fn = mmc_block_test_remove;
	INIT_LIST_HEAD(&mbtd->bdt.list);
	test_iosched_register(&mbtd->bdt);

	return 0;
}

static void __exit mmc_block_test_exit(void)
{
	test_iosched_unregister(&mbtd->bdt);
	kfree(mbtd);
}

module_init(mmc_block_test_init);
module_exit(mmc_block_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MMC block test");
