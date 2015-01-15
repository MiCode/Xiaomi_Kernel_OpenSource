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

#ifndef _LINUX_TEST_IOSCHED_H
#define _LINUX_TEST_IOSCHED_H

/*
 * Patterns definitions for read/write requests data
 */
#define TEST_PATTERN_SEQUENTIAL	0x12345678
#define TEST_PATTERN_5A		0x5A5A5A5A
#define TEST_PATTERN_FF		0xFFFFFFFF
#define TEST_NO_PATTERN		0xDEADBEEF
#define BIO_U32_SIZE 1024
#define TEST_BIO_SIZE		PAGE_SIZE	/* use one page bios */

struct test_iosched;

typedef int (prepare_test_fn) (struct test_iosched *);
typedef int (run_test_fn) (struct test_iosched *);
typedef int (check_test_result_fn) (struct test_iosched *);
typedef int (post_test_fn) (struct test_iosched *);
typedef char* (get_test_case_str_fn) (int);
typedef int (blk_dev_test_init_fn) (struct test_iosched *);
typedef void (blk_dev_test_exit_fn) (struct test_iosched *);
typedef struct gendisk* (get_rq_disk_fn) (struct test_iosched *);
typedef bool (check_test_completion_fn) (struct test_iosched *);

/**
 * enum test_state - defines the state of the test
 */
enum test_state {
	TEST_IDLE,
	TEST_RUNNING,
	TEST_COMPLETED,
};

/**
 * enum test_results - defines the success orfailure of the test
 */
enum test_results {
	TEST_NO_RESULT,
	TEST_FAILED,
	TEST_PASSED,
	TEST_NOT_SUPPORTED,
};

/**
 * enum req_unique_type - defines a unique request type
 */
enum req_unique_type {
	REQ_UNIQUE_NONE,
	REQ_UNIQUE_DISCARD,
	REQ_UNIQUE_FLUSH,
};

/**
 * struct test_debug - debugfs directories
 * @debug_root:		The test-iosched debugfs root directory
 * @debug_utils_root:	test-iosched debugfs utils root
 *			directory
 * @debug_tests_root:	test-iosched debugfs tests root
 *			directory
 * @debug_test_result:	Exposes the test result to the user
 *			space
 * @start_sector:	The start sector for read/write requests
 * @sector_range:	Range of the test, starting from start_sector
 *			(in sectors)
 */
struct test_debug {
	struct dentry *debug_root;
	struct dentry *debug_utils_root;
	struct dentry *debug_tests_root;
	struct dentry *debug_test_result;
	struct dentry *start_sector;
	struct dentry *sector_range;
};

/**
 * struct test_request - defines a test request
 * @queuelist:		The test requests list
 * @bios_buffer:	Write/read requests data buffer, one page per bio
 * @buf_size:		Write/read requests data buffer size (in
 *			bytes)
 * @rq:			A block request, to be dispatched
 * @req_completed:	A flag to indicate if the request was
 *			completed
 * @req_result:		Keeps the error code received in the
 *			request completion callback
 * @is_err_expected:	A flag to indicate if the request should
 *			fail
 * @wr_rd_data_pattern:	A pattern written to the write data
 *			buffer. Can be used in read requests to
 *			verify the data
 * @req_id:		A unique ID to identify a test request
 *			to ease the debugging of the test cases
 */
struct test_request {
	struct list_head queuelist;
	void *bios_buffer[BLK_MAX_SEGMENTS];
	int buf_size;
	struct request *rq;
	bool req_completed;
	int req_result;
	int is_err_expected;
	int wr_rd_data_pattern;
	int req_id;
};

/**
 * struct test_info - specific test information
 * @testcase:		The current running test case
 * @timeout_msec:	Test specific test timeout
 * @buf_size:		Write/read requests data buffer size (in
 *			bytes)
 * @prepare_test_fn:	Test specific test preparation callback
 * @run_test_fn:	Test specific test running callback
 * @check_test_result_fn: Test specific test result checking
 *			callback
 * @get_test_case_str_fn: Test specific function to get the test name
 * @test_duration:	A ktime value saved for timing
 *			calculations
 * @data:		Test specific private data
 * @test_byte_count:	Total number of bytes dispatched in
 *			the test
 */
struct test_info {
	int testcase;
	unsigned timeout_msec;
	prepare_test_fn *prepare_test_fn;
	run_test_fn *run_test_fn;
	check_test_result_fn *check_test_result_fn;
	post_test_fn *post_test_fn;
	get_test_case_str_fn *get_test_case_str_fn;
	ktime_t test_duration;
	get_rq_disk_fn *get_rq_disk_fn;
	check_test_completion_fn *check_test_completion_fn;
	void *data;
	unsigned long test_byte_count;
};

/**
 * struct blk_dev_test_type - identifies block device test
 * @list:		list head pointer
 * @type_prefix:	prefix of device class name, i.e. "mmc"/ "sd"
 * @init_fn:		block device test init callback
 * @exit_fn:		block device test exit callback
 */
struct blk_dev_test_type {
	struct list_head list;
	const char *type_prefix;
	blk_dev_test_init_fn *init_fn;
	blk_dev_test_exit_fn *exit_fn;
};

/**
 * struct test_data - global test iosched data
 * @queue:		The test IO scheduler requests list
 * @test_queue:		The test requests list
 * @dispatched_queue:   The queue contains requests dispatched
 *			from @test_queue
 * @reinsert_queue:     The queue contains reinserted from underlying
 *			driver requests
 * @urgent_queue:       The queue contains requests for urgent delivery
 *			These requests will be delivered before @test_queue
 *			and @reinsert_queue requests
 * @test_count:         Number of requests in the @test_queue
 * @dispatched_count:   Number of requests in the @dispatched_queue
 * @reinsert_count:     Number of requests in the @reinsert_queue
 * @urgent_count:       Number of requests in the @urgent_queue
 * @wait_q:		A wait queue for waiting for the test
 *			requests completion
 * @test_state:		Indicates if there is a running test.
 *			Used for dispatch function
 * @test_result:	Indicates if the test passed or failed
 * @debug:		The test debugfs entries
 * @req_q:		The block layer request queue
 * @num_of_write_bios:	The number of write BIOs added to the test requests.
 *			Used to calcualte the sector number of
 *			new BIOs.
 * @start_sector:	The address of the first sector that can
 *			be accessed by the test
 * @sector_range:	Range of the test, starting from start_sector
 *			(in sectors)
 * @wr_rd_next_req_id:	A unique ID to identify WRITE/READ
 *			request to ease the debugging of the
 *			test cases
 * @unique_next_req_id:	A unique ID to identify
 *			FLUSH/DISCARD/SANITIZE request to ease
 *			the debugging of the test cases
 * @lock:		A lock to verify running a single test
 *			at a time
 * @test_info:		A specific test data to be set by the
 *			test invokation function
 * @ignore_round:	A boolean variable indicating that a
 *			test round was disturbed by an external
 *			flush request, therefore disqualifying
 *			the results
 * @blk_dev_test_data:	associated specific block device test utility
 */
struct test_iosched {
	struct list_head queue;
	struct list_head test_queue;
	struct list_head dispatched_queue;
	struct list_head reinsert_queue;
	struct list_head urgent_queue;
	unsigned int  test_count;
	unsigned int  dispatched_count;
	unsigned int  reinsert_count;
	unsigned int  urgent_count;
	wait_queue_head_t wait_q;
	enum test_state test_state;
	enum test_results test_result;
	struct test_debug debug;
	struct request_queue *req_q;
	int num_of_write_bios;
	u32 start_sector;
	u32 sector_range;
	int wr_rd_next_req_id;
	int unique_next_req_id;
	spinlock_t lock;
	struct test_info test_info;
	bool fs_wr_reqs_during_test;
	bool ignore_round;

	void *blk_dev_test_data;
};

extern int test_iosched_start_test(struct test_iosched *,
	struct test_info *t_info);
extern void test_iosched_mark_test_completion(struct test_iosched *);
extern void check_test_completion(struct test_iosched *);
extern int test_iosched_add_unique_test_req(struct test_iosched *,
	int is_err_expcted, enum req_unique_type req_unique,
	int start_sec, int nr_sects, rq_end_io_fn *end_req_io);
extern int test_iosched_add_wr_rd_test_req(struct test_iosched *,
	int is_err_expcted, int direction, int start_sec, int num_bios,
	int pattern, rq_end_io_fn *end_req_io);
extern struct test_request *test_iosched_create_test_req(struct test_iosched *,
	int is_err_expcted, int direction, int start_sec, int num_bios,
	int pattern, rq_end_io_fn *end_req_io);

extern void test_iosched_free_test_req_data_buffer(struct test_request *);

extern void test_iosched_set_test_result(struct test_iosched*, int test_result);

extern void test_iosched_set_ignore_round(struct test_iosched *,
	bool ignore_round);

extern void test_iosched_register(struct blk_dev_test_type *bdt);

extern void test_iosched_unregister(struct blk_dev_test_type *bdt);

extern void test_iosched_add_urgent_req(struct test_iosched *,
	struct test_request *);

extern void check_test_completion(struct test_iosched *);

extern int compare_buffer_to_pattern(struct test_request *test_rq);

#endif /* _LINUX_TEST_IOSCHED_H */
