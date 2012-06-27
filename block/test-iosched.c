/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * The test scheduler allows to test the block device by dispatching
 * specific requests according to the test case and declare PASS/FAIL
 * according to the requests completion error code.
 * Each test is exposed via debugfs and can be triggered by writing to
 * the debugfs file.
 *
 */

/* elevator test iosched */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/test-iosched.h>
#include <linux/delay.h>
#include "blk.h"

#define MODULE_NAME "test-iosched"
#define WR_RD_START_REQ_ID 1234
#define UNIQUE_START_REQ_ID 5678
#define TIMEOUT_TIMER_MS 40000
#define TEST_MAX_TESTCASE_ROUNDS 15

#define test_pr_debug(fmt, args...) pr_debug("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_info(fmt, args...) pr_info("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_err(fmt, args...) pr_err("%s: "fmt"\n", MODULE_NAME, args)

static DEFINE_SPINLOCK(blk_dev_test_list_lock);
static LIST_HEAD(blk_dev_test_list);
static struct test_data *ptd;

/* Get the request after `test_rq' in the test requests list */
static struct test_request *
latter_test_request(struct request_queue *q,
				 struct test_request *test_rq)
{
	struct test_data *td = q->elevator->elevator_data;

	if (test_rq->queuelist.next == &td->test_queue)
		return NULL;
	return list_entry(test_rq->queuelist.next, struct test_request,
			  queuelist);
}

/**
 * test_iosched_get_req_queue() - returns the request queue
 * served by the scheduler
 */
struct request_queue *test_iosched_get_req_queue(void)
{
	if (!ptd)
		return NULL;

	return ptd->req_q;
}
EXPORT_SYMBOL(test_iosched_get_req_queue);

/**
 * test_iosched_mark_test_completion() - Wakeup the debugfs
 * thread, waiting on the test completion
 */
void test_iosched_mark_test_completion(void)
{
	if (!ptd)
		return;

	ptd->test_state = TEST_COMPLETED;
	wake_up(&ptd->wait_q);
}
EXPORT_SYMBOL(test_iosched_mark_test_completion);

/* Check if all the queued test requests were completed */
static void check_test_completion(void)
{
	struct test_request *test_rq;
	struct request *rq;

	list_for_each_entry(test_rq, &ptd->test_queue, queuelist) {
		rq = test_rq->rq;
		if (!test_rq->req_completed)
			return;
	}

	test_pr_info("%s: Test is completed", __func__);

	test_iosched_mark_test_completion();
}

/*
 * A callback to be called per bio completion.
 * Frees the bio memory.
 */
static void end_test_bio(struct bio *bio, int err)
{
	if (err)
		clear_bit(BIO_UPTODATE, &bio->bi_flags);

	bio_put(bio);
}

/*
 * A callback to be called per request completion.
 * the request memory is not freed here, will be freed later after the test
 * results checking.
 */
static void end_test_req(struct request *rq, int err)
{
	struct test_request *test_rq;

	test_rq = (struct test_request *)rq->elv.priv[0];
	BUG_ON(!test_rq);

	test_pr_info("%s: request %d completed, err=%d",
	       __func__, test_rq->req_id, err);

	test_rq->req_completed = 1;
	test_rq->req_result = err;

	check_test_completion();
}

/**
 * test_iosched_add_unique_test_req() - Create and queue a non
 * read/write request (such as FLUSH/DISCRAD/SANITIZE).
 * @is_err_expcted:	A flag to indicate if this request
 *			should succeed or not
 * @req_unique:		The type of request to add
 * @start_sec:		start address of the first bio
 * @nr_sects:		number of sectors in the request
 * @end_req_io:		specific completion callback. When not
 *			set, the defaulcallback will be used
 */
int test_iosched_add_unique_test_req(int is_err_expcted,
			enum req_unique_type req_unique,
			int start_sec, int nr_sects, rq_end_io_fn *end_req_io)
{
	struct bio *bio;
	struct request *rq;
	int rw_flags;
	struct test_request *test_rq;

	if (!ptd)
		return -ENODEV;

	bio = bio_alloc(GFP_KERNEL, 0);
	if (!bio) {
		test_pr_err("%s: Failed to allocate a bio", __func__);
		return -ENODEV;
	}
	bio_get(bio);
	bio->bi_end_io = end_test_bio;

	switch (req_unique) {
	case REQ_UNIQUE_FLUSH:
		bio->bi_rw = WRITE_FLUSH;
		break;
	case REQ_UNIQUE_DISCARD:
		bio->bi_rw = REQ_WRITE | REQ_DISCARD;
		bio->bi_size = nr_sects << 9;
		bio->bi_sector = start_sec;
		break;
	default:
		test_pr_err("%s: Invalid request type %d", __func__,
			    req_unique);
		bio_put(bio);
		return -ENODEV;
	}

	rw_flags = bio_data_dir(bio);
	if (bio->bi_rw & REQ_SYNC)
		rw_flags |= REQ_SYNC;

	rq = blk_get_request(ptd->req_q, rw_flags, GFP_KERNEL);
	if (!rq) {
		test_pr_err("%s: Failed to allocate a request", __func__);
		bio_put(bio);
		return -ENODEV;
	}

	init_request_from_bio(rq, bio);
	if (end_req_io)
		rq->end_io = end_req_io;
	else
		rq->end_io = end_test_req;

	test_rq = kzalloc(sizeof(struct test_request), GFP_KERNEL);
	if (!test_rq) {
		test_pr_err("%s: Failed to allocate a test request", __func__);
		bio_put(bio);
		blk_put_request(rq);
		return -ENODEV;
	}
	test_rq->req_completed = 0;
	test_rq->req_result = -1;
	test_rq->rq = rq;
	test_rq->is_err_expected = is_err_expcted;
	rq->elv.priv[0] = (void *)test_rq;
	test_rq->req_id = ptd->unique_next_req_id++;

	test_pr_debug(
		"%s: added request %d to the test requests list, type = %d",
		__func__, test_rq->req_id, req_unique);

	list_add_tail(&test_rq->queuelist, &ptd->test_queue);

	return 0;
}
EXPORT_SYMBOL(test_iosched_add_unique_test_req);

/*
 * Get a pattern to be filled in the request data buffer.
 * If the pattern used is (-1) the buffer will be filled with sequential
 * numbers
 */
static void fill_buf_with_pattern(int *buf, int num_bytes, int pattern)
{
	int i = 0;
	int num_of_dwords = num_bytes/sizeof(int);

	if (pattern == TEST_NO_PATTERN)
		return;

	/* num_bytes should be aligned to sizeof(int) */
	BUG_ON((num_bytes % sizeof(int)) != 0);

	if (pattern == TEST_PATTERN_SEQUENTIAL) {
		for (i = 0; i < num_of_dwords; i++)
			buf[i] = i;
	} else {
		for (i = 0; i < num_of_dwords; i++)
			buf[i] = pattern;
	}
}

/**
 * test_iosched_add_wr_rd_test_req() - Create and queue a
 * read/write request.
 * @is_err_expcted:	A flag to indicate if this request
 *			should succeed or not
 * @direction:		READ/WRITE
 * @start_sec:		start address of the first bio
 * @num_bios:		number of BIOs to be allocated for the
 *			request
 * @pattern:		A pattern, to be written into the write
 *			requests data buffer. In case of READ
 *			request, the given pattern is kept as
 *			the expected pattern. The expected
 *			pattern will be compared in the test
 *			check result function. If no comparisson
 *			is required, set pattern to
 *			TEST_NO_PATTERN.
 * @end_req_io:		specific completion callback. When not
 *			set,the default callback will be used
 *
 * This function allocates the test request and the block
 * request and calls blk_rq_map_kern which allocates the
 * required BIO. The allocated test request and the block
 * request memory is freed at the end of the test and the
 * allocated BIO memory is freed by end_test_bio.
 */
int test_iosched_add_wr_rd_test_req(int is_err_expcted,
		      int direction, int start_sec,
		      int num_bios, int pattern, rq_end_io_fn *end_req_io)
{
	struct request *rq = NULL;
	struct test_request *test_rq = NULL;
	int rw_flags = 0;
	int buf_size = 0;
	int ret = 0, i = 0;
	unsigned int *bio_ptr = NULL;
	struct bio *bio = NULL;

	if (!ptd)
		return -ENODEV;

	rw_flags = direction;

	rq = blk_get_request(ptd->req_q, rw_flags, GFP_KERNEL);
	if (!rq) {
		test_pr_err("%s: Failed to allocate a request", __func__);
		return -ENODEV;
	}

	test_rq = kzalloc(sizeof(struct test_request), GFP_KERNEL);
	if (!test_rq) {
		test_pr_err("%s: Failed to allocate test request", __func__);
		blk_put_request(rq);
		return -ENODEV;
	}

	buf_size = sizeof(unsigned int) * BIO_U32_SIZE * num_bios;
	test_rq->bios_buffer = kzalloc(buf_size, GFP_KERNEL);
	if (!test_rq->bios_buffer) {
		test_pr_err("%s: Failed to allocate the data buf", __func__);
		goto err;
	}
	test_rq->buf_size = buf_size;

	if (direction == WRITE)
		fill_buf_with_pattern(test_rq->bios_buffer,
						   buf_size, pattern);
	test_rq->wr_rd_data_pattern = pattern;

	bio_ptr = test_rq->bios_buffer;
	for (i = 0; i < num_bios; ++i) {
		ret = blk_rq_map_kern(ptd->req_q, rq,
				      (void *)bio_ptr,
				      sizeof(unsigned int)*BIO_U32_SIZE,
				      GFP_KERNEL);
		if (ret) {
			test_pr_err("%s: blk_rq_map_kern returned error %d",
				    __func__, ret);
			goto err;
		}
		bio_ptr += BIO_U32_SIZE;
	}

	if (end_req_io)
		rq->end_io = end_req_io;
	else
		rq->end_io = end_test_req;
	rq->__sector = start_sec;
	rq->cmd_type |= REQ_TYPE_FS;

	if (rq->bio) {
		rq->bio->bi_sector = start_sec;
		rq->bio->bi_end_io = end_test_bio;
		bio = rq->bio;
		while ((bio = bio->bi_next) != NULL)
			bio->bi_end_io = end_test_bio;
	}

	ptd->num_of_write_bios += num_bios;
	test_rq->req_id = ptd->wr_rd_next_req_id++;

	test_rq->req_completed = 0;
	test_rq->req_result = -1;
	test_rq->rq = rq;
	test_rq->is_err_expected = is_err_expcted;
	rq->elv.priv[0] = (void *)test_rq;

	test_pr_debug(
		"%s: added request %d to the test requests list, buf_size=%d",
		__func__, test_rq->req_id, buf_size);

	list_add_tail(&test_rq->queuelist, &ptd->test_queue);

	return 0;
err:
	blk_put_request(rq);
	kfree(test_rq->bios_buffer);
	return -ENODEV;
}
EXPORT_SYMBOL(test_iosched_add_wr_rd_test_req);

/* Converts the testcase number into a string */
static char *get_test_case_str(struct test_data *td)
{
	if (td->test_info.get_test_case_str_fn)
		return td->test_info.get_test_case_str_fn(td);

	return "Unknown testcase";
}

/*
 * Verify that the test request data buffer includes the expected
 * pattern
 */
static int compare_buffer_to_pattern(struct test_request *test_rq)
{
	int i = 0;
	int num_of_dwords = test_rq->buf_size/sizeof(int);

	/* num_bytes should be aligned to sizeof(int) */
	BUG_ON((test_rq->buf_size % sizeof(int)) != 0);
	BUG_ON(test_rq->bios_buffer == NULL);

	if (test_rq->wr_rd_data_pattern == TEST_NO_PATTERN)
		return 0;

	if (test_rq->wr_rd_data_pattern == TEST_PATTERN_SEQUENTIAL) {
		for (i = 0; i < num_of_dwords; i++) {
			if (test_rq->bios_buffer[i] != i) {
				test_pr_err(
					"%s: wrong pattern 0x%x in index %d",
					__func__, test_rq->bios_buffer[i], i);
				return -EINVAL;
			}
		}
	} else {
		for (i = 0; i < num_of_dwords; i++) {
			if (test_rq->bios_buffer[i] !=
			    test_rq->wr_rd_data_pattern) {
				test_pr_err(
					"%s: wrong pattern 0x%x in index %d",
					__func__, test_rq->bios_buffer[i], i);
				return -EINVAL;
			}
		}
	}

	return 0;
}

/*
 * Determine if the test passed or failed.
 * The function checks the test request completion value and calls
 * check_testcase_result for result checking that are specific
 * to a test case.
 */
static int check_test_result(struct test_data *td)
{
	struct test_request *test_rq;
	struct request *rq;
	int res = 0;
	static int run;

	list_for_each_entry(test_rq, &ptd->test_queue, queuelist) {
		rq = test_rq->rq;
		if (!test_rq->req_completed) {
			test_pr_err("%s: rq %d not completed", __func__,
				    test_rq->req_id);
			res = -EINVAL;
			goto err;
		}

		if ((test_rq->req_result < 0) && !test_rq->is_err_expected) {
			test_pr_err(
				"%s: rq %d completed with err, not as expected",
				__func__, test_rq->req_id);
			res = -EINVAL;
			goto err;
		}
		if ((test_rq->req_result == 0) && test_rq->is_err_expected) {
			test_pr_err("%s: rq %d succeeded, not as expected",
				    __func__, test_rq->req_id);
			res = -EINVAL;
			goto err;
		}
		if (rq_data_dir(test_rq->rq) == READ) {
			res = compare_buffer_to_pattern(test_rq);
			if (res) {
				test_pr_err("%s: read pattern not as expected",
					    __func__);
				res = -EINVAL;
				goto err;
			}
		}
	}

	if (td->test_info.check_test_result_fn) {
		res = td->test_info.check_test_result_fn(td);
		if (res)
			goto err;
	}

	test_pr_info("%s: %s, run# %03d, PASSED",
			    __func__, get_test_case_str(td), ++run);
	td->test_result = TEST_PASSED;

	return 0;
err:
	test_pr_err("%s: %s, run# %03d, FAILED",
		    __func__, get_test_case_str(td), ++run);
	td->test_result = TEST_FAILED;
	return res;
}

/* Create and queue the required requests according to the test case */
static int prepare_test(struct test_data *td)
{
	int ret = 0;

	if (td->test_info.prepare_test_fn) {
		ret = td->test_info.prepare_test_fn(td);
		return ret;
	}

	return 0;
}

/* Run the test */
static int run_test(struct test_data *td)
{
	int ret = 0;

	if (td->test_info.run_test_fn) {
		ret = td->test_info.run_test_fn(td);
		return ret;
	}

	/*
	 * Set the next_req pointer to the first request in the test requests
	 * list
	 */
	if (!list_empty(&td->test_queue))
		td->next_req = list_entry(td->test_queue.next,
					  struct test_request, queuelist);
	__blk_run_queue(td->req_q);

	return 0;
}

/* Free the allocated test requests, their requests and BIOs buffer */
static void free_test_requests(struct test_data *td)
{
	struct test_request *test_rq;
	while (!list_empty(&td->test_queue)) {
		test_rq = list_entry(td->test_queue.next, struct test_request,
				     queuelist);
		list_del_init(&test_rq->queuelist);
		blk_put_request(test_rq->rq);
		kfree(test_rq->bios_buffer);
		kfree(test_rq);
	}
}

/*
 * Do post test operations.
 * Free the allocated test requests, their requests and BIOs buffer.
 */
static int post_test(struct test_data *td)
{
	int ret = 0;

	if (td->test_info.post_test_fn)
		ret = td->test_info.post_test_fn(td);

	ptd->test_info.testcase = 0;
	ptd->test_state = TEST_IDLE;

	free_test_requests(td);

	return ret;
}

/*
 * The timer verifies that the test will be completed even if we don't get
 * the completion callback for all the requests.
 */
static void test_timeout_handler(unsigned long data)
{
	struct test_data *td = (struct test_data *)data;

	test_pr_info("%s: TIMEOUT timer expired", __func__);
	td->test_state = TEST_COMPLETED;
	wake_up(&td->wait_q);
	return;
}

static unsigned int get_timeout_msec(struct test_data *td)
{
	if (td->test_info.timeout_msec)
		return td->test_info.timeout_msec;
	else
		return TIMEOUT_TIMER_MS;
}

/**
 * test_iosched_start_test() - Prepares and runs the test.
 * @t_info:	the current test testcase and callbacks
 *		functions
 *
 * The function also checks the test result upon test completion
 */
int test_iosched_start_test(struct test_info *t_info)
{
	int ret = 0;
	unsigned timeout_msec;
	int counter = 0;
	char *test_name = NULL;

	if (!ptd)
		return -ENODEV;

	if (!t_info) {
		ptd->test_result = TEST_FAILED;
		return -EINVAL;
	}

	do {
		if (ptd->ignore_round)
			/*
			 * We ignored the last run due to FS write requests.
			 * Sleep to allow those requests to be issued
			 */
			msleep(2000);

		spin_lock(&ptd->lock);

		if (ptd->test_state != TEST_IDLE) {
			test_pr_info(
				"%s: Another test is running, try again later",
				__func__);
			return -EINVAL;
		}

		if (ptd->start_sector == 0) {
			test_pr_err("%s: Invalid start sector", __func__);
			ptd->test_result = TEST_FAILED;
			spin_unlock(&ptd->lock);
			return -EINVAL;
		}

		memcpy(&ptd->test_info, t_info, sizeof(struct test_info));

		ptd->next_req = NULL;
		ptd->test_result = TEST_NO_RESULT;
		ptd->num_of_write_bios = 0;

		ptd->unique_next_req_id = UNIQUE_START_REQ_ID;
		ptd->wr_rd_next_req_id = WR_RD_START_REQ_ID;

		ptd->ignore_round = false;
		ptd->fs_wr_reqs_during_test = false;

		ptd->test_state = TEST_RUNNING;

		spin_unlock(&ptd->lock);

		timeout_msec = get_timeout_msec(ptd);
		mod_timer(&ptd->timeout_timer, jiffies +
			  msecs_to_jiffies(timeout_msec));

		if (ptd->test_info.get_test_case_str_fn)
			test_name = ptd->test_info.get_test_case_str_fn(ptd);
		else
			test_name = "Unknown testcase";
		test_pr_info("%s: Starting test %s\n", __func__, test_name);

		ret = prepare_test(ptd);
		if (ret) {
			test_pr_err("%s: failed to prepare the test\n",
				    __func__);
			goto error;
		}

		ret = run_test(ptd);
		if (ret) {
			test_pr_err("%s: failed to run the test\n", __func__);
			goto error;
		}

		test_pr_info("%s: Waiting for the test completion", __func__);

		wait_event(ptd->wait_q, ptd->test_state == TEST_COMPLETED);
		del_timer_sync(&ptd->timeout_timer);

		ret = check_test_result(ptd);
		if (ret) {
			test_pr_err("%s: check_test_result failed\n",
				    __func__);
			goto error;
		}

		ret = post_test(ptd);
		if (ret) {
			test_pr_err("%s: post_test failed\n", __func__);
			goto error;
		}

		/*
		 * Wakeup the queue thread to fetch FS requests that might got
		 * postponded due to the test
		 */
		__blk_run_queue(ptd->req_q);

		if (ptd->ignore_round)
			test_pr_info(
			"%s: Round canceled (Got wr reqs in the middle)",
			__func__);

		if (++counter == TEST_MAX_TESTCASE_ROUNDS) {
			test_pr_info("%s: Too many rounds, did not succeed...",
			     __func__);
			ptd->test_result = TEST_FAILED;
		}

	} while ((ptd->ignore_round) && (counter < TEST_MAX_TESTCASE_ROUNDS));

	if (ptd->test_result == TEST_PASSED)
		return 0;
	else
		return -EINVAL;

error:
	ptd->test_result = TEST_FAILED;
	ptd->test_info.testcase = 0;
	post_test(ptd);
	return ret;
}
EXPORT_SYMBOL(test_iosched_start_test);

/**
 * test_iosched_register() - register a block device test
 * utility.
 * @bdt:	the block device test type to register
 */
void test_iosched_register(struct blk_dev_test_type *bdt)
{
	spin_lock(&blk_dev_test_list_lock);
	list_add_tail(&bdt->list, &blk_dev_test_list);
	spin_unlock(&blk_dev_test_list_lock);
}
EXPORT_SYMBOL_GPL(test_iosched_register);

/**
 * test_iosched_unregister() - unregister a block device test
 * utility.
 * @bdt:	the block device test type to unregister
 */
void test_iosched_unregister(struct blk_dev_test_type *bdt)
{
	spin_lock(&blk_dev_test_list_lock);
	list_del_init(&bdt->list);
	spin_unlock(&blk_dev_test_list_lock);
}
EXPORT_SYMBOL_GPL(test_iosched_unregister);

/**
 * test_iosched_set_test_result() - Set the test
 * result(PASS/FAIL)
 * @test_result:	the test result
 */
void test_iosched_set_test_result(int test_result)
{
	if (!ptd)
		return;

	ptd->test_result = test_result;
}
EXPORT_SYMBOL(test_iosched_set_test_result);


/**
 * test_iosched_set_ignore_round() - Set the ignore_round flag
 * @ignore_round:	A flag to indicate if this test round
 * should be ignored and re-run
 */
void test_iosched_set_ignore_round(bool ignore_round)
{
	if (!ptd)
		return;

	ptd->ignore_round = ignore_round;
}
EXPORT_SYMBOL(test_iosched_set_ignore_round);

/**
 * test_iosched_get_debugfs_tests_root() - returns the root
 * debugfs directory for the test_iosched tests
 */
struct dentry *test_iosched_get_debugfs_tests_root(void)
{
	if (!ptd)
		return NULL;

	return ptd->debug.debug_tests_root;
}
EXPORT_SYMBOL(test_iosched_get_debugfs_tests_root);

/**
 * test_iosched_get_debugfs_utils_root() - returns the root
 * debugfs directory for the test_iosched utils
 */
struct dentry *test_iosched_get_debugfs_utils_root(void)
{
	if (!ptd)
		return NULL;

	return ptd->debug.debug_utils_root;
}
EXPORT_SYMBOL(test_iosched_get_debugfs_utils_root);

static int test_debugfs_init(struct test_data *td)
{
	td->debug.debug_root = debugfs_create_dir("test-iosched", NULL);
	if (!td->debug.debug_root)
		return -ENOENT;

	td->debug.debug_tests_root = debugfs_create_dir("tests",
							td->debug.debug_root);
	if (!td->debug.debug_tests_root)
		goto err;

	td->debug.debug_utils_root = debugfs_create_dir("utils",
							td->debug.debug_root);
	if (!td->debug.debug_utils_root)
		goto err;

	td->debug.debug_test_result = debugfs_create_u32(
					"test_result",
					S_IRUGO | S_IWUGO,
					td->debug.debug_utils_root,
					&td->test_result);
	if (!td->debug.debug_test_result)
		goto err;

	td->debug.start_sector = debugfs_create_u32(
					"start_sector",
					S_IRUGO | S_IWUGO,
					td->debug.debug_utils_root,
					&td->start_sector);
	if (!td->debug.start_sector)
		goto err;

	return 0;

err:
	debugfs_remove_recursive(td->debug.debug_root);
	return -ENOENT;
}

static void test_debugfs_cleanup(struct test_data *td)
{
	debugfs_remove_recursive(td->debug.debug_root);
}

static void print_req(struct request *req)
{
	struct bio *bio;
	struct test_request *test_rq;

	if (!req)
		return;

	test_rq = (struct test_request *)req->elv.priv[0];

	if (test_rq) {
		test_pr_debug("%s: Dispatch request %d: __sector=0x%lx",
		       __func__, test_rq->req_id, (unsigned long)req->__sector);
		test_pr_debug("%s: nr_phys_segments=%d, num_of_sectors=%d",
		       __func__, req->nr_phys_segments, blk_rq_sectors(req));
		bio = req->bio;
		test_pr_debug("%s: bio: bi_size=%d, bi_sector=0x%lx",
			      __func__, bio->bi_size,
			      (unsigned long)bio->bi_sector);
		while ((bio = bio->bi_next) != NULL) {
			test_pr_debug("%s: bio: bi_size=%d, bi_sector=0x%lx",
				      __func__, bio->bi_size,
				      (unsigned long)bio->bi_sector);
		}
	}
}

static void test_merged_requests(struct request_queue *q,
			 struct request *rq, struct request *next)
{
	list_del_init(&next->queuelist);
}

/*
 * Dispatch a test request in case there is a running test Otherwise, dispatch
 * a request that was queued by the FS to keep the card functional.
 */
static int test_dispatch_requests(struct request_queue *q, int force)
{
	struct test_data *td = q->elevator->elevator_data;
	struct request *rq = NULL;

	switch (td->test_state) {
	case TEST_IDLE:
		if (!list_empty(&td->queue)) {
			rq = list_entry(td->queue.next, struct request,
					queuelist);
			list_del_init(&rq->queuelist);
			elv_dispatch_sort(q, rq);
			return 1;
		}
		break;
	case TEST_RUNNING:
		if (td->next_req) {
			rq = td->next_req->rq;
			td->next_req =
				latter_test_request(td->req_q, td->next_req);
			if (!rq)
				return 0;
			print_req(rq);
			elv_dispatch_sort(q, rq);
			return 1;
		}
		break;
	case TEST_COMPLETED:
	default:
		return 0;
	}

	return 0;
}

static void test_add_request(struct request_queue *q, struct request *rq)
{
	struct test_data *td = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &td->queue);

	/*
	 * The write requests can be followed by a FLUSH request that might
	 * cause unexpected results of the test.
	 */
	if ((rq_data_dir(rq) == WRITE) && (td->test_state == TEST_RUNNING)) {
		test_pr_debug("%s: got WRITE req in the middle of the test",
			__func__);
		td->fs_wr_reqs_during_test = true;
	}
}

static struct request *
test_former_request(struct request_queue *q, struct request *rq)
{
	struct test_data *td = q->elevator->elevator_data;

	if (rq->queuelist.prev == &td->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
test_latter_request(struct request_queue *q, struct request *rq)
{
	struct test_data *td = q->elevator->elevator_data;

	if (rq->queuelist.next == &td->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static void *test_init_queue(struct request_queue *q)
{
	struct blk_dev_test_type *__bdt;

	ptd = kmalloc_node(sizeof(struct test_data), GFP_KERNEL,
			     q->node);
	if (!ptd) {
		test_pr_err("%s: failed to allocate test data", __func__);
		return NULL;
	}
	memset((void *)ptd, 0, sizeof(struct test_data));
	INIT_LIST_HEAD(&ptd->queue);
	INIT_LIST_HEAD(&ptd->test_queue);
	init_waitqueue_head(&ptd->wait_q);
	ptd->req_q = q;

	setup_timer(&ptd->timeout_timer, test_timeout_handler,
		    (unsigned long)ptd);

	spin_lock_init(&ptd->lock);

	if (test_debugfs_init(ptd)) {
		test_pr_err("%s: Failed to create debugfs files", __func__);
		return NULL;
	}

	list_for_each_entry(__bdt, &blk_dev_test_list, list)
		__bdt->init_fn();

	return ptd;
}

static void test_exit_queue(struct elevator_queue *e)
{
	struct test_data *td = e->elevator_data;
	struct blk_dev_test_type *__bdt;

	BUG_ON(!list_empty(&td->queue));

	list_for_each_entry(__bdt, &blk_dev_test_list, list)
		__bdt->exit_fn();

	test_debugfs_cleanup(td);

	kfree(td);
}

static struct elevator_type elevator_test_iosched = {
	.ops = {
		.elevator_merge_req_fn = test_merged_requests,
		.elevator_dispatch_fn = test_dispatch_requests,
		.elevator_add_req_fn = test_add_request,
		.elevator_former_req_fn = test_former_request,
		.elevator_latter_req_fn = test_latter_request,
		.elevator_init_fn = test_init_queue,
		.elevator_exit_fn = test_exit_queue,
	},
	.elevator_name = "test-iosched",
	.elevator_owner = THIS_MODULE,
};

static int __init test_init(void)
{
	elv_register(&elevator_test_iosched);

	return 0;
}

static void __exit test_exit(void)
{
	elv_unregister(&elevator_test_iosched);
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test IO scheduler");
