/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt"\n"

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


static DEFINE_MUTEX(blk_dev_test_list_lock);
static LIST_HEAD(blk_dev_test_list);


/**
 * test_iosched_mark_test_completion() - Wakeup the debugfs
 * thread, waiting on the test completion
 */
void test_iosched_mark_test_completion(struct test_iosched *tios)
{
	if (!tios)
		return;

	pr_info("%s: mark test is completed, test_count=%d, ", __func__,
		tios->test_count);
	pr_info("%s: urgent_count=%d, reinsert_count=%d,", __func__,
		tios->urgent_count, tios->reinsert_count);

	tios->test_state = TEST_COMPLETED;
	wake_up(&tios->wait_q);
}
EXPORT_SYMBOL(test_iosched_mark_test_completion);

/**
 *  check_test_completion() - Check if all the queued test
 *  requests were completed
 */
void check_test_completion(struct test_iosched *tios)
{
	struct test_request *test_rq;

	if (!tios)
		goto exit;

	if (tios->test_info.check_test_completion_fn &&
		!tios->test_info.check_test_completion_fn(tios))
		goto exit;

	list_for_each_entry(test_rq, &tios->dispatched_queue, queuelist)
		if (!test_rq->req_completed)
			goto exit;

	if (!list_empty(&tios->test_queue)
			|| !list_empty(&tios->reinsert_queue)
			|| !list_empty(&tios->urgent_queue)) {
		pr_info("%s: Test still not completed,", __func__);
		pr_info("%s: test_count=%d, reinsert_count=%d", __func__,
			tios->test_count, tios->reinsert_count);
		pr_info("%s: dispatched_count=%d, urgent_count=%d", __func__,
			tios->dispatched_count,
			tios->urgent_count);
		goto exit;
	}

	tios->test_info.test_duration = ktime_sub(ktime_get(),
		tios->test_info.test_duration);

	test_iosched_mark_test_completion(tios);

exit:
	return;
}
EXPORT_SYMBOL(check_test_completion);

/*
 * A callback to be called per bio completion.
 * Frees the bio memory.
 */
static void end_test_bio(struct bio *bio)
{
	bio_put(bio);
}

void test_iosched_free_test_req_data_buffer(struct test_request *test_rq)
{
	int i;

	if (!test_rq)
		return;

	for (i = 0; i < BLK_MAX_SEGMENTS; i++)
		if (test_rq->bios_buffer[i]) {
			free_page((unsigned long)test_rq->bios_buffer[i]);
			test_rq->bios_buffer[i] = NULL;
		}
}
EXPORT_SYMBOL(test_iosched_free_test_req_data_buffer);

/*
 * A callback to be called per request completion.
 * the request memory is not freed here, will be freed later after the test
 * results checking.
 */
static void end_test_req(struct request *rq, int err)
{
	struct test_request *test_rq;
	struct test_iosched *tios = rq->q->elevator->elevator_data;
	test_rq = (struct test_request *)rq->elv.priv[0];
	BUG_ON(!test_rq);

	pr_debug("%s: request %d completed, err=%d",
	       __func__, test_rq->req_id, err);

	test_rq->req_completed = true;
	test_rq->req_result = err;

	check_test_completion(tios);
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
int test_iosched_add_unique_test_req(struct test_iosched *tios,
	int is_err_expcted, enum req_unique_type req_unique,
	int start_sec, int nr_sects, rq_end_io_fn *end_req_io)
{
	struct bio *bio;
	struct request *rq;
	int rw_flags;
	struct test_request *test_rq;
	unsigned long flags;

	if (!tios)
		return -ENODEV;

	bio = bio_alloc(GFP_KERNEL, 0);
	if (!bio) {
		pr_err("%s: Failed to allocate a bio", __func__);
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
		bio->bi_iter.bi_size = nr_sects << 9;
		bio->bi_iter.bi_sector = start_sec;
		break;
	default:
		pr_err("%s: Invalid request type %d", __func__,
			    req_unique);
		bio_put(bio);
		return -ENODEV;
	}

	rw_flags = bio_data_dir(bio);
	if (bio->bi_rw & REQ_SYNC)
		rw_flags |= REQ_SYNC;

	rq = blk_get_request(tios->req_q, rw_flags, GFP_KERNEL);
	if (!rq) {
		pr_err("%s: Failed to allocate a request", __func__);
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
		pr_err("%s: Failed to allocate a test request", __func__);
		bio_put(bio);
		blk_put_request(rq);
		return -ENODEV;
	}
	test_rq->req_completed = false;
	test_rq->req_result = -EINVAL;
	test_rq->rq = rq;
	test_rq->is_err_expected = is_err_expcted;
	rq->elv.priv[0] = (void *)test_rq;
	test_rq->req_id = tios->unique_next_req_id++;

	pr_debug(
		"%s: added request %d to the test requests list, type = %d",
		__func__, test_rq->req_id, req_unique);

	spin_lock_irqsave(tios->req_q->queue_lock, flags);
	list_add_tail(&test_rq->queuelist, &tios->test_queue);
	tios->test_count++;
	spin_unlock_irqrestore(tios->req_q->queue_lock, flags);

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
 * test_iosched_create_test_req() - Create a read/write request.
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
struct test_request *test_iosched_create_test_req(
	struct test_iosched *tios, int is_err_expcted,
	int direction, int start_sec, int num_bios, int pattern,
	rq_end_io_fn *end_req_io)
{
	struct request *rq;
	struct test_request *test_rq;
	struct bio *bio = NULL;
	int i;
	int ret;

	if (!tios)
		return NULL;

	rq = blk_get_request(tios->req_q, direction, GFP_KERNEL);
	if (!rq) {
		pr_err("%s: Failed to allocate a request", __func__);
		return NULL;
	}

	test_rq = kzalloc(sizeof(struct test_request), GFP_KERNEL);
	if (!test_rq) {
		pr_err("%s: Failed to allocate test request", __func__);
		goto err;
	}

	test_rq->buf_size = TEST_BIO_SIZE * num_bios;
	test_rq->wr_rd_data_pattern = pattern;

	for (i = 0; i < num_bios; i++) {
		test_rq->bios_buffer[i] = (void *)get_zeroed_page(GFP_KERNEL);
		if (!test_rq->bios_buffer[i]) {
			pr_err("%s: failed to kmap page for bio #%d/%d\n",
				__func__, i, num_bios);
			goto free_bios;
		}
		ret = blk_rq_map_kern(tios->req_q, rq, test_rq->bios_buffer[i],
			TEST_BIO_SIZE, GFP_KERNEL);
		if (ret) {
			pr_err("%s: blk_rq_map_kern returned error %d",
				__func__, ret);
			goto free_bios;
		}
		if (direction == WRITE)
			fill_buf_with_pattern(test_rq->bios_buffer[i],
				TEST_BIO_SIZE, pattern);
	}

	if (end_req_io)
		rq->end_io = end_req_io;
	else
		rq->end_io = end_test_req;
	rq->__sector = start_sec;
	rq->cmd_type |= REQ_TYPE_FS;
	rq->cmd_flags |= REQ_SORTED;
	rq->cmd_flags &= ~REQ_IO_STAT;

	if (rq->bio) {
		rq->bio->bi_iter.bi_sector = start_sec;
		rq->bio->bi_end_io = end_test_bio;
		bio = rq->bio;
		while ((bio = bio->bi_next) != NULL)
			bio->bi_end_io = end_test_bio;
	}

	tios->num_of_write_bios += num_bios;
	test_rq->req_id = tios->wr_rd_next_req_id++;

	test_rq->req_completed = false;
	test_rq->req_result = -EINVAL;
	test_rq->rq = rq;
	if (tios->test_info.get_rq_disk_fn)
		test_rq->rq->rq_disk = tios->test_info.get_rq_disk_fn(tios);
	test_rq->is_err_expected = is_err_expcted;
	rq->elv.priv[0] = (void *)test_rq;
	return test_rq;

free_bios:
	test_iosched_free_test_req_data_buffer(test_rq);
	kfree(test_rq);
err:
	blk_put_request(rq);
	return NULL;
}
EXPORT_SYMBOL(test_iosched_create_test_req);


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
 * required BIO. Upon success the new request is added to the
 * test_queue. The allocated test request and the block request
 * memory is freed at the end of the test and the allocated BIO
 * memory is freed by end_test_bio.
 */
int test_iosched_add_wr_rd_test_req(struct test_iosched *tios,
	int is_err_expcted, int direction, int start_sec, int num_bios,
	int pattern, rq_end_io_fn *end_req_io)
{
	struct test_request *test_rq = NULL;
	unsigned long flags;

	test_rq = test_iosched_create_test_req(tios, is_err_expcted, direction,
		start_sec, num_bios, pattern, end_req_io);
	if (test_rq) {
		spin_lock_irqsave(tios->req_q->queue_lock, flags);
		list_add_tail(&test_rq->queuelist, &tios->test_queue);
		tios->test_count++;
		spin_unlock_irqrestore(tios->req_q->queue_lock, flags);
		return 0;
	}
	return -ENODEV;
}
EXPORT_SYMBOL(test_iosched_add_wr_rd_test_req);

/* Converts the testcase number into a string */
static char *get_test_case_str(struct test_iosched *tios)
{
	if (tios->test_info.get_test_case_str_fn)
		return tios->test_info.get_test_case_str_fn(
			tios->test_info.testcase);

	return "Unknown testcase";
}

/*
 * Verify that the test request data buffer includes the expected
 * pattern
 */
int compare_buffer_to_pattern(struct test_request *test_rq)
{
	int i;
	int j;
	unsigned int *buf;

	/* num_bytes should be aligned to sizeof(int) */
	BUG_ON((test_rq->buf_size % sizeof(int)) != 0);
	BUG_ON(test_rq->bios_buffer == NULL);

	if (test_rq->wr_rd_data_pattern == TEST_NO_PATTERN)
		return 0;

	for (i = 0; i < test_rq->buf_size / TEST_BIO_SIZE; i++) {
		buf = test_rq->bios_buffer[i];
		for (j = 0; j < TEST_BIO_SIZE / sizeof(int); j++)
			if ((test_rq->wr_rd_data_pattern ==
				TEST_PATTERN_SEQUENTIAL && buf[j] != j) ||
				(test_rq->wr_rd_data_pattern !=
				TEST_PATTERN_SEQUENTIAL &&
				buf[j] != test_rq->wr_rd_data_pattern)) {
				pr_err("%s: wrong pattern 0x%x in index %d",
					__func__, buf[j], j);
				return -EINVAL;
			}
	}

	return 0;
}
EXPORT_SYMBOL(compare_buffer_to_pattern);

/*
 * Determine if the test passed or failed.
 * The function checks the test request completion value and calls
 * check_testcase_result for result checking that are specific
 * to a test case.
 */
static int check_test_result(struct test_iosched *tios)
{
	struct test_request *trq;
	int res = 0;
	static int run;

	list_for_each_entry(trq, &tios->dispatched_queue, queuelist) {
		if (!trq->rq) {
			pr_info("%s: req_id %d is contains empty req",
					__func__, trq->req_id);
			continue;
		}
		if (!trq->req_completed) {
			pr_err("%s: rq %d not completed", __func__,
				    trq->req_id);
			res = -EINVAL;
			goto err;
		}

		if ((trq->req_result < 0) && !trq->is_err_expected) {
			pr_err(
				"%s: rq %d completed with err, not as expected",
				__func__, trq->req_id);
			res = -EINVAL;
			goto err;
		}
		if ((trq->req_result == 0) && trq->is_err_expected) {
			pr_err("%s: rq %d succeeded, not as expected",
				    __func__, trq->req_id);
			res = -EINVAL;
			goto err;
		}
		if (rq_data_dir(trq->rq) == READ) {
			res = compare_buffer_to_pattern(trq);
			if (res) {
				pr_err("%s: read pattern not as expected",
					    __func__);
				res = -EINVAL;
				goto err;
			}
		}
	}

	if (tios->test_info.check_test_result_fn) {
		res = tios->test_info.check_test_result_fn(
			tios);
		if (res)
			goto err;
	}

	pr_info("%s: %s, run# %03d, PASSED",
		__func__, get_test_case_str(tios), ++run);
	tios->test_result = TEST_PASSED;

	return 0;
err:
	pr_err("%s: %s, run# %03d, FAILED",
		    __func__, get_test_case_str(tios), ++run);
	tios->test_result = TEST_FAILED;
	return res;
}

/* Create and queue the required requests according to the test case */
static int prepare_test(struct test_iosched *tios)
{
	int ret = 0;

	if (tios->test_info.prepare_test_fn) {
		ret = tios->test_info.prepare_test_fn(tios);
		return ret;
	}

	return 0;
}

/* Run the test */
static int run_test(struct test_iosched *tios)
{
	int ret = 0;

	if (tios->test_info.run_test_fn) {
		ret = tios->test_info.run_test_fn(tios);
		return ret;
	}

	blk_run_queue(tios->req_q);

	return 0;
}

/*
 * free_test_queue() - Free all allocated test requests in the given test_queue:
 * free their requests and BIOs buffer
 * @test_queue		the test queue to be freed
 */
static void free_test_queue(struct list_head *test_queue)
{
	struct test_request *test_rq;
	struct bio *bio;

	while (!list_empty(test_queue)) {
		test_rq = list_entry(test_queue->next, struct test_request,
				queuelist);

		list_del_init(&test_rq->queuelist);
		/*
		 * If the request was not completed we need to free its BIOs
		 * and remove it from the packed list
		 */
		if (!test_rq->req_completed) {
			pr_info(
				"%s: Freeing memory of an uncompleted request",
					__func__);
			list_del_init(&test_rq->rq->queuelist);
			while ((bio = test_rq->rq->bio) != NULL) {
				test_rq->rq->bio = bio->bi_next;
				bio_put(bio);
			}
		}
		blk_put_request(test_rq->rq);
		test_iosched_free_test_req_data_buffer(test_rq);
		kfree(test_rq);
	}
}

/*
 * free_test_requests() - Free all allocated test requests in
 * all test queues in given test_data.
 * @td		The test_data struct whos test requests will be
 *		freed.
 */
static void free_test_requests(struct test_iosched *tios)
{
	if (!tios)
		return;

	if (tios->urgent_count) {
		free_test_queue(&tios->urgent_queue);
		tios->urgent_count = 0;
	}
	if (tios->test_count) {
		free_test_queue(&tios->test_queue);
		tios->test_count = 0;
	}
	if (tios->dispatched_count) {
		free_test_queue(&tios->dispatched_queue);
		tios->dispatched_count = 0;
	}
	if (tios->reinsert_count) {
		free_test_queue(&tios->reinsert_queue);
		tios->reinsert_count = 0;
	}
}

/*
 * post_test() - Do post test operations. Free the allocated
 * test requests, their requests and BIOs buffer.
 * @td		The test_data struct for the test that has
 *		ended.
 */
static int post_test(struct test_iosched *tios)
{
	int ret = 0;

	if (tios->test_info.post_test_fn)
		ret = tios->test_info.post_test_fn(tios);

	tios->test_info.testcase = 0;
	tios->test_state = TEST_IDLE;

	free_test_requests(tios);

	return ret;
}

static unsigned int get_timeout_msec(struct test_iosched *tios)
{
	if (tios->test_info.timeout_msec)
		return tios->test_info.timeout_msec;
	return TIMEOUT_TIMER_MS;
}

/**
 * test_iosched_start_test() - Prepares and runs the test.
 * The members test_duration and test_byte_count of the input
 * parameter t_info are modified by this function.
 * @t_info:	the current test testcase and callbacks
 *		functions
 *
 * The function also checks the test result upon test completion
 */
int test_iosched_start_test(struct test_iosched *tios,
	struct test_info *t_info)
{
	int ret = 0;
	unsigned long timeout;
	int counter = 0;
	char *test_name = NULL;

	if (!tios)
		return -ENODEV;

	if (!t_info) {
		tios->test_result = TEST_FAILED;
		return -EINVAL;
	}

	timeout = msecs_to_jiffies(get_timeout_msec(tios));

	do {
		if (tios->ignore_round)
			/*
			 * We ignored the last run due to FS write requests.
			 * Sleep to allow those requests to be issued
			 */
			msleep(2000);

		spin_lock(&tios->lock);

		if (tios->test_state != TEST_IDLE) {
			pr_info(
				"%s: Another test is running, try again later",
				__func__);
			spin_unlock(&tios->lock);
			return -EBUSY;
		}

		if (tios->start_sector == 0) {
			pr_err("%s: Invalid start sector", __func__);
			tios->test_result = TEST_FAILED;
			spin_unlock(&tios->lock);
			return -EINVAL;
		}

		memcpy(&tios->test_info, t_info, sizeof(*t_info));

		tios->test_result = TEST_NO_RESULT;
		tios->num_of_write_bios = 0;

		tios->unique_next_req_id = UNIQUE_START_REQ_ID;
		tios->wr_rd_next_req_id = WR_RD_START_REQ_ID;

		tios->ignore_round = false;
		tios->fs_wr_reqs_during_test = false;

		tios->test_state = TEST_RUNNING;

		spin_unlock(&tios->lock);
		/*
		 * Give an already dispatch request from
		 * FS a chanse to complete
		 */
		msleep(2000);

		if (tios->test_info.get_test_case_str_fn)
			test_name =
				tios->test_info.get_test_case_str_fn(
					tios->test_info.testcase);
		else
			test_name = "Unknown testcase";
		pr_info("%s: Starting test %s", __func__, test_name);

		ret = prepare_test(tios);
		if (ret) {
			pr_err("%s: failed to prepare the test",
				    __func__);
			goto error;
		}

		tios->test_info.test_duration = ktime_get();
		ret = run_test(tios);
		if (ret) {
			pr_err("%s: failed to run the test", __func__);
			goto error;
		}

		pr_info("%s: Waiting for the test completion", __func__);

		ret = wait_event_interruptible_timeout(tios->wait_q,
			(tios->test_state == TEST_COMPLETED), timeout);
		if (ret <= 0) {
			tios->test_state = TEST_COMPLETED;
			if (!ret)
				pr_info("%s: Test timeout\n", __func__);
			else
				pr_err("%s: Test error=%d\n", __func__, ret);
			goto error;
		}

		memcpy(t_info, &tios->test_info, sizeof(*t_info));

		ret = check_test_result(tios);
		if (ret) {
			pr_err("%s: check_test_result failed", __func__);
			goto error;
		}

		ret = post_test(tios);
		if (ret) {
			pr_err("%s: post_test failed", __func__);
			goto error;
		}

		/*
		 * Wakeup the queue thread to fetch FS requests that might got
		 * postponded due to the test
		 */
		blk_run_queue(tios->req_q);

		if (tios->ignore_round)
			pr_info(
			"%s: Round canceled (Got wr reqs in the middle)",
			__func__);

		if (++counter == TEST_MAX_TESTCASE_ROUNDS) {
			pr_info("%s: Too many rounds, did not succeed...",
			     __func__);
			tios->test_result = TEST_FAILED;
		}

	} while ((tios->ignore_round) &&
		(counter < TEST_MAX_TESTCASE_ROUNDS));

	if (tios->test_result == TEST_PASSED)
		return 0;
	else
		return -EINVAL;

error:
	post_test(tios);
	tios->test_result = TEST_FAILED;
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
	if (!bdt)
		return;

	mutex_lock(&blk_dev_test_list_lock);
	list_add_tail(&bdt->list, &blk_dev_test_list);
	mutex_unlock(&blk_dev_test_list_lock);

}
EXPORT_SYMBOL(test_iosched_register);

/**
 * test_iosched_unregister() - unregister a block device test
 * utility.
 * @bdt:	the block device test type to unregister
 */
void test_iosched_unregister(struct blk_dev_test_type *bdt)
{
	if (!bdt)
		return;

	mutex_lock(&blk_dev_test_list_lock);
	list_del_init(&bdt->list);
	mutex_unlock(&blk_dev_test_list_lock);
}
EXPORT_SYMBOL(test_iosched_unregister);

/**
 * test_iosched_set_test_result() - Set the test
 * result(PASS/FAIL)
 * @test_result:	the test result
 */
void test_iosched_set_test_result(struct test_iosched *tios,
	int test_result)
{
	if (!tios)
		return;

	tios->test_result = test_result;
}
EXPORT_SYMBOL(test_iosched_set_test_result);


/**
 * test_iosched_set_ignore_round() - Set the ignore_round flag
 * @ignore_round:	A flag to indicate if this test round
 * should be ignored and re-run
 */
void test_iosched_set_ignore_round(struct test_iosched *tios,
	bool ignore_round)
{
	if (!tios)
		return;

	tios->ignore_round = ignore_round;
}
EXPORT_SYMBOL(test_iosched_set_ignore_round);

static int test_debugfs_init(struct test_iosched *tios)
{
	char name[2*BDEVNAME_SIZE];


	snprintf(name, 2*BDEVNAME_SIZE - 1, "%s-%s", "test-iosched",
		tios->req_q->kobj.parent->name);
	pr_debug("%s: creating test-iosched instance %s\n", __func__, name);

	tios->debug.debug_root = debugfs_create_dir(name, NULL);
	if (!tios->debug.debug_root)
		return -ENOENT;

	tios->debug.debug_tests_root = debugfs_create_dir("tests",
		tios->debug.debug_root);
	if (!tios->debug.debug_tests_root)
		goto err;

	tios->debug.debug_utils_root = debugfs_create_dir("utils",
		tios->debug.debug_root);
	if (!tios->debug.debug_utils_root)
		goto err;

	tios->debug.debug_test_result = debugfs_create_u32(
					"test_result",
					S_IRUGO | S_IWUGO,
					tios->debug.debug_utils_root,
					&tios->test_result);
	if (!tios->debug.debug_test_result)
		goto err;

	tios->debug.start_sector = debugfs_create_u32(
					"start_sector",
					S_IRUGO | S_IWUGO,
					tios->debug.debug_utils_root,
					&tios->start_sector);
	if (!tios->debug.start_sector)
		goto err;

	tios->debug.sector_range = debugfs_create_u32(
						"sector_range",
						S_IRUGO | S_IWUGO,
						tios->debug.debug_utils_root,
						&tios->sector_range);
	if (!tios->debug.sector_range)
		goto err;

	return 0;

err:
	debugfs_remove_recursive(tios->debug.debug_root);
	return -ENOENT;
}

static void test_debugfs_cleanup(struct test_iosched *tios)
{
	debugfs_remove_recursive(tios->debug.debug_root);
}

static void print_req(struct request *req)
{
	struct bio *bio;
	struct test_request *test_rq;

	if (!req)
		return;

	test_rq = (struct test_request *)req->elv.priv[0];

	if (test_rq) {
		pr_debug("%s: Dispatch request %d: __sector=0x%lx",
		       __func__, test_rq->req_id, (unsigned long)req->__sector);
		pr_debug("%s: nr_phys_segments=%d, num_of_sectors=%d",
		       __func__, req->nr_phys_segments, blk_rq_sectors(req));
		bio = req->bio;
		pr_debug("%s: bio: bi_size=%d, bi_sector=0x%lx",
			      __func__, bio->bi_iter.bi_size,
			      (unsigned long)bio->bi_iter.bi_sector);
		while ((bio = bio->bi_next) != NULL) {
			pr_debug("%s: bio: bi_size=%d, bi_sector=0x%lx",
				      __func__, bio->bi_iter.bi_size,
				      (unsigned long)bio->bi_iter.bi_sector);
		}
	}
}

static void test_merged_requests(struct request_queue *q,
			 struct request *rq, struct request *next)
{
	list_del_init(&next->queuelist);
}
/*
 * test_dispatch_from(): Dispatch request from @queue to the @dispatched_queue.
 * Also update the dispatched_count counter.
 */
static int test_dispatch_from(struct request_queue *q,
		struct list_head *queue, unsigned int *count)
{
	struct test_request *test_rq;
	struct request *rq;
	int ret = 0;
	struct test_iosched *tios = q->elevator->elevator_data;
	unsigned long flags;

	if (!tios)
		goto err;

	spin_lock_irqsave(&tios->lock, flags);
	if (!list_empty(queue)) {
		test_rq = list_entry(queue->next, struct test_request,
				queuelist);
		rq = test_rq->rq;
		if (!rq) {
			pr_err("%s: null request,return", __func__);
			spin_unlock_irqrestore(&tios->lock, flags);
			goto err;
		}
		list_move_tail(&test_rq->queuelist,
			&tios->dispatched_queue);
		tios->dispatched_count++;
		(*count)--;
		spin_unlock_irqrestore(&tios->lock, flags);

		print_req(rq);
		elv_dispatch_sort(q, rq);
		tios->test_info.test_byte_count += test_rq->buf_size;
		ret = 1;
		goto err;
	}
	spin_unlock_irqrestore(&tios->lock, flags);

err:
	return ret;
}

/*
 * Dispatch a test request in case there is a running test Otherwise, dispatch
 * a request that was queued by the FS to keep the card functional.
 */
static int test_dispatch_requests(struct request_queue *q, int force)
{
	struct test_iosched *tios = q->elevator->elevator_data;
	struct request *rq = NULL;
	int ret = 0;

	switch (tios->test_state) {
	case TEST_IDLE:
		if (!list_empty(&tios->queue)) {
			rq = list_entry(tios->queue.next,
				struct request, queuelist);
			list_del_init(&rq->queuelist);
			elv_dispatch_sort(q, rq);
			ret = 1;
			goto exit;
		}
		break;
	case TEST_RUNNING:
		if (test_dispatch_from(q, &tios->urgent_queue,
				       &tios->urgent_count)) {
			pr_debug("%s: Dispatched from urgent_count=%d",
					__func__, tios->urgent_count);
			ret = 1;
			goto exit;
		}
		if (test_dispatch_from(q, &tios->reinsert_queue,
				       &tios->reinsert_count)) {
			pr_debug("%s: Dispatched from reinsert_count=%d",
					__func__, tios->reinsert_count);
			ret = 1;
			goto exit;
		}
		if (test_dispatch_from(q, &tios->test_queue,
			&tios->test_count)) {
			pr_debug("%s: Dispatched from test_count=%d",
					__func__, tios->test_count);
			ret = 1;
			goto exit;
		}
		break;
	case TEST_COMPLETED:
	default:
		break;
	}

exit:
	return ret;
}

static void test_add_request(struct request_queue *q, struct request *rq)
{
	struct test_iosched *tios = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &tios->queue);

	/*
	 * The write requests can be followed by a FLUSH request that might
	 * cause unexpected results of the test.
	 */
	if (rq_data_dir(rq) == WRITE &&
		tios->test_state == TEST_RUNNING) {
		pr_debug("%s: got WRITE req in the middle of the test",
			__func__);
		tios->fs_wr_reqs_during_test = true;
	}
}

static struct request *
test_former_request(struct request_queue *q, struct request *rq)
{
	struct test_iosched *tios = q->elevator->elevator_data;

	if (rq->queuelist.prev == &tios->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
test_latter_request(struct request_queue *q, struct request *rq)
{
	struct test_iosched *tios = q->elevator->elevator_data;

	if (rq->queuelist.next == &tios->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int test_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct blk_dev_test_type *__bdt;
	struct elevator_queue *eq;
	struct test_iosched *tios;
	const char *blk_dev_name;
	int ret;
	bool found = false;
	unsigned long flags;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	tios = kzalloc_node(sizeof(*tios), GFP_KERNEL, q->node);
	if (!tios) {
		pr_err("%s: failed to allocate test iosched\n", __func__);
		ret = -ENOMEM;
		goto free_kobj;
	}
	eq->elevator_data = tios;

	INIT_LIST_HEAD(&tios->queue);
	INIT_LIST_HEAD(&tios->test_queue);
	INIT_LIST_HEAD(&tios->dispatched_queue);
	INIT_LIST_HEAD(&tios->reinsert_queue);
	INIT_LIST_HEAD(&tios->urgent_queue);
	init_waitqueue_head(&tios->wait_q);
	tios->req_q = q;

	spin_lock_init(&tios->lock);

	ret = test_debugfs_init(tios);
	if (ret) {
		pr_err("%s: Failed to create debugfs files, ret=%d",
			__func__, ret);
		goto free_mem;
	}
	blk_dev_name = q->kobj.parent->name;

	/* Traverse the block device test list and init matches */
	mutex_lock(&blk_dev_test_list_lock);

	list_for_each_entry(__bdt, &blk_dev_test_list, list) {
		pr_debug("%s: checking if %s is a match to device %s\n",
			__func__, __bdt->type_prefix, blk_dev_name);
		if (!strnstr(blk_dev_name, __bdt->type_prefix,
			strlen(__bdt->type_prefix)))
			continue;

		pr_debug("%s: found the match!\n", __func__);
		found = true;
		break;
	}
	mutex_unlock(&blk_dev_test_list_lock);

	/* No match found */
	if (!found) {
		pr_err("%s: No matching block device test utility found\n",
			__func__);
		ret = -ENODEV;
		goto free_debugfs;
	} else {
		ret = __bdt->init_fn(tios);
		if (ret) {
			pr_err("%s: failed to init block device test, ret=%d\n",
				__func__, ret);
			goto free_debugfs;
		}
	}

	spin_lock_irqsave(q->queue_lock, flags);
	q->elevator = eq;
	spin_unlock_irqrestore(q->queue_lock, flags);

	return 0;

free_debugfs:
	test_debugfs_cleanup(tios);
free_mem:
	kfree(tios);
free_kobj:
	kobject_put(&eq->kobj);
	return ret;
}

static void test_exit_queue(struct elevator_queue *e)
{
	struct test_iosched *tios = e->elevator_data;
	struct blk_dev_test_type *__bdt;

	BUG_ON(!list_empty(&tios->queue));

	list_for_each_entry(__bdt, &blk_dev_test_list, list)
		__bdt->exit_fn(tios);

	test_debugfs_cleanup(tios);

	kfree(tios);
}

/**
 * test_iosched_add_urgent_req() - Add an urgent test_request.
 * First mark the request as urgent, then add it to the
 * urgent_queue test queue.
 * @test_rq:		pointer to the urgent test_request to be
 *			added.
 *
 */
void test_iosched_add_urgent_req(struct test_iosched *tios,
	struct test_request *test_rq)
{
	unsigned long flags;

	if (!tios)
		return;

	spin_lock_irqsave(&tios->lock, flags);
	test_rq->rq->cmd_flags |= REQ_URGENT;
	list_add_tail(&test_rq->queuelist, &tios->urgent_queue);
	tios->urgent_count++;
	spin_unlock_irqrestore(&tios->lock, flags);
}
EXPORT_SYMBOL(test_iosched_add_urgent_req);

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
