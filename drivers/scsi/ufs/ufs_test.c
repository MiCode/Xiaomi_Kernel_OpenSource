/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/debugfs.h>
#include <linux/test-iosched.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <../sd.h>
#include <linux/delay.h>

#define MODULE_NAME "ufs_test"

#define TEST_MAX_BIOS_PER_REQ		16
#define LARGE_PRIME_1	1103515367
#define LARGE_PRIME_2	35757
#define DEFAULT_NUM_OF_BIOS		2
#define LONG_SEQUENTIAL_MIXED_TIMOUT_MS 100000

/* the amount of requests that will be inserted */
#define LONG_SEQ_TEST_NUM_REQS  256
/* request queue limitation is 128 requests, and we leave 10 spare requests */
#define QUEUE_MAX_REQUESTS 118
#define MB_MSEC_RATIO_APPROXIMATION ((1024 * 1024) / 1000)
/* actual number of MiB in test multiplied by 10, for single digit precision*/
#define BYTE_TO_MB_x_10(x) ((x * 10) / (1024 * 1024))
/* extract integer value */
#define LONG_TEST_SIZE_INTEGER(x) (BYTE_TO_MB_x_10(x) / 10)
/* and calculate the MiB value fraction */
#define LONG_TEST_SIZE_FRACTION(x) (BYTE_TO_MB_x_10(x) - \
		(LONG_TEST_SIZE_INTEGER(x) * 10))

#define test_pr_debug(fmt, args...) pr_debug("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_info(fmt, args...) pr_info("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_err(fmt, args...) pr_err("%s: "fmt"\n", MODULE_NAME, args)

#define TEST_OPS(test_name, upper_case_name)				\
static int ufs_test_ ## test_name ## _show(struct seq_file *file,	\
		void *data)						\
{ return ufs_test_show(file, UFS_TEST_ ## upper_case_name); }		\
static int ufs_test_ ## test_name ## _open(struct inode *inode,		\
		struct file *file)					\
{ return single_open(file, ufs_test_ ## test_name ## _show,		\
		inode->i_private); }					\
static ssize_t ufs_test_ ## test_name ## _write(struct file *file,	\
		const char __user *buf, size_t count, loff_t *ppos)	\
{ return ufs_test_write(file, buf, count, ppos,				\
			UFS_TEST_ ## upper_case_name); }		\
const struct file_operations ufs_test_ ## test_name ## _ops = {		\
	.open = ufs_test_ ## test_name ## _open,			\
	.read = seq_read,						\
	.write = ufs_test_ ## test_name ## _write,			\
};

#define add_test(utd, test_name, upper_case_name)			\
ufs_test_add_test(utd, UFS_TEST_ ## upper_case_name, "ufs_test_"#test_name,\
				&(ufs_test_ ## test_name ## _ops));	\





enum ufs_test_testcases {
	UFS_TEST_WRITE_READ_TEST,

	UFS_TEST_LONG_SEQUENTIAL_READ,
	UFS_TEST_LONG_SEQUENTIAL_WRITE,
	UFS_TEST_LONG_SEQUENTIAL_MIXED,

	NUM_TESTS,
};

enum ufs_test_stage {
	DEFAULT,
	UFS_TEST_ERROR,

	UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE1,
	UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE2,
};

struct ufs_test_data {
	/* Data structure for debugfs dentrys */
	struct dentry **test_list;
	/*
	 * Data structure containing individual test information, including
	 * self-defined specific data
	 */
	struct test_info test_info;
	/* device test */
	struct blk_dev_test_type bdt;
	/* A wait queue for OPs to complete */
	wait_queue_head_t wait_q;
	/* a flag for write compleation */
	bool write_completed;
	/*
	 * To determine the number of r/w bios. When seed = 0, random is
	 * disabled and 2 BIOs are written.
	 */
	unsigned int random_test_seed;
	struct dentry *random_test_seed_dentry;

	/* A counter for the number of test requests completed */
	unsigned int completed_req_count;
	/* Test stage */
	enum ufs_test_stage test_stage;
};

static struct ufs_test_data *utd;

static int ufs_test_add_test(struct ufs_test_data *utd,
		enum ufs_test_testcases test_id, char *test_str,
		const struct file_operations *test_fops)
{
	int ret = 0;
	struct dentry *tests_root;

	if (test_id >= NUM_TESTS)
		return -EINVAL;

	tests_root = test_iosched_get_debugfs_tests_root();
	if (!tests_root) {
		test_pr_err("%s: Failed to create debugfs root.", __func__);
		return -EINVAL;
	}

	utd->test_list[test_id] = debugfs_create_file(test_str,
						S_IRUGO | S_IWUGO, tests_root,
						NULL, test_fops);
	if (!utd->test_list[test_id]) {
		test_pr_err("%s: Could not create the test %s", test_str,
				__func__);
		ret = -ENOMEM;
	}
	return ret;
}

static char *ufs_test_get_test_case_str(struct test_data *td)
{
	if (!td) {
		test_pr_err("%s: NULL td", __func__);
		return NULL;
	}

	switch (td->test_info.testcase) {
	case UFS_TEST_WRITE_READ_TEST:
		return "UFS write read test";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_READ:
		return "UFS long sequential read test";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_WRITE:
		return "UFS long sequential write test";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_MIXED:
		return "UFS long sequential mixed test";
		break;
	default:
		return "Unknown test";
	}
}

static unsigned int ufs_test_pseudo_random_seed(unsigned int *seed_number,
		unsigned int min_val, unsigned int max_val)
{
	int ret = 0;

	if (!seed_number)
		return 0;

	*seed_number = ((unsigned int) (((unsigned long) *seed_number
			* (unsigned long) LARGE_PRIME_1) + LARGE_PRIME_2));
	ret = (unsigned int) ((*seed_number) % max_val);

	return (ret > min_val ? ret : min_val);
}

static void ufs_test_pseudo_rnd_size(unsigned int *seed,
				unsigned int *num_of_bios)
{
	*num_of_bios = ufs_test_pseudo_random_seed(seed, 1,
						TEST_MAX_BIOS_PER_REQ);
	if (!(*num_of_bios))
		*num_of_bios = DEFAULT_NUM_OF_BIOS;
}

static int ufs_test_show(struct seq_file *file, int test_case)
{
	char *test_description;

	switch (test_case) {
	case UFS_TEST_WRITE_READ_TEST:
		test_description = "\nufs_write_read_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test write once a random block and than reads it to "
		 "verify its content. Used to debug first time transactions.\n";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_READ:
		test_description = "\nufs_long_sequential_read_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test runs the following scenarios\n"
		 "- Long Sequential Read Test: this test measures read "
		 "throughput at the driver level by sequentially reading many "
		 "large requests.\n";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_WRITE:
		test_description =  "\nufs_long_sequential_write_test\n"
		 "=========\n"
		 "Description:\n"
		 "This test runs the following scenarios\n"
		 "- Long Sequential Write Test: this test measures write "
		 "throughput at the driver level by sequentially writing many "
		 "large requests\n";
		break;
	case UFS_TEST_LONG_SEQUENTIAL_MIXED:
		test_description = "\nufs_long_sequential_mixed_test_read\n"
		 "=========\n"
		 "Description:\n"
		 "The test will verify correctness of sequential data pattern "
		 "written to the device while new data (with same pattern) is "
		 "written simultaneously.\n"
		 "First this test will run a long sequential write scenario."
		 "This first stage will write the pattern that will be read "
		 "later. Second, sequential read requests will read and "
		 "compare the same data. The second stage reads, will issue in "
		 "Parallel to write requests with the same LBA and size.\n"
		 "NOTE: The test requires a long timeout.\n";
		break;
	default:
		test_description = "Unknown test";
	}

	seq_puts(file, test_description);
	return 0;
}

static struct gendisk *ufs_test_get_rq_disk(void)
{
	struct request_queue *req_q = test_iosched_get_req_queue();
	struct scsi_device *sd;
	struct device *dev;
	struct scsi_disk *sdkp;
	struct gendisk *gd;

	if (!req_q) {
		test_pr_info("%s: Could not fetch request_queue", __func__);
		gd = NULL;
		goto exit;
	}

	sd = (struct scsi_device *)req_q->queuedata;

	dev = &sd->sdev_gendev;
	sdkp = scsi_disk_get_from_dev(dev);
	if (!sdkp) {
		test_pr_info("%s: Could not fatch scsi disk", __func__);
		gd = NULL;
		goto exit;
	}

	gd = sdkp->disk;
exit:
	return gd;
}

static int ufs_test_check_result(struct test_data *td)
{
	if (utd->test_stage == UFS_TEST_ERROR) {
		test_pr_err("%s: An error occurred during the test.", __func__);
		return TEST_FAILED;
	}
	return 0;
}

static bool ufs_write_read_completion(void)
{
	if (!utd->write_completed) {
		utd->write_completed = true;
		wake_up(&utd->wait_q);
		return false;
	}
	return true;
}

static int ufs_test_run_write_read_test(struct test_data *td)
{
	int ret = 0;
	unsigned int start_sec;
	unsigned int num_bios;
	struct request_queue *q = td->req_q;


	start_sec = td->start_sector + sizeof(int) * BIO_U32_SIZE
			* td->num_of_write_bios;
	if (utd->random_test_seed != 0)
		ufs_test_pseudo_rnd_size(&utd->random_test_seed, &num_bios);
	else
		num_bios = DEFAULT_NUM_OF_BIOS;

	/* Adding a write request */
	test_pr_info(
		"%s: Adding a write request with %d bios to Q, req_id=%d"
			, __func__, num_bios, td->wr_rd_next_req_id);

	utd->write_completed = false;
	ret = test_iosched_add_wr_rd_test_req(0, WRITE, start_sec, num_bios,
						TEST_PATTERN_5A, NULL);

	if (ret) {
		test_pr_err("%s: failed to add a write request", __func__);
		return ret;
	}

	/* waiting for the write request to finish */
	blk_run_queue(q);
	wait_event(utd->wait_q, utd->write_completed);

	/* Adding a read request*/
	test_pr_info("%s: Adding a read request to Q", __func__);

	ret = test_iosched_add_wr_rd_test_req(0, READ, start_sec,
			num_bios, TEST_PATTERN_5A, NULL);

	if (ret) {
		test_pr_err("%s: failed to add a read request", __func__);
		return ret;
	}

	blk_run_queue(q);
	return ret;
}

static void long_seq_test_free_end_io_fn(struct request *rq, int err)
{
	struct test_request *test_rq;
	struct test_data *ptd = test_get_test_data();

	if (rq) {
		test_rq = (struct test_request *)rq->elv.priv[0];
	} else {
		test_pr_err("%s: error: NULL request", __func__);
		return;
	}

	BUG_ON(!test_rq);

	spin_lock_irq(&ptd->lock);
	ptd->dispatched_count--;
	list_del_init(&test_rq->queuelist);
	__blk_put_request(ptd->req_q, test_rq->rq);
	spin_unlock_irq(&ptd->lock);

	if (utd->test_stage == UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE2 &&
			rq_data_dir(rq) == READ &&
			compare_buffer_to_pattern(test_rq)) {
		/* if the pattern does not match */
		test_pr_err("%s: read pattern not as expected", __func__);
		utd->test_stage = UFS_TEST_ERROR;
		check_test_completion();
		return;
	}

	kfree(test_rq->bios_buffer);
	kfree(test_rq);
	utd->completed_req_count++;

	if (err)
		test_pr_err("%s: request %d completed, err=%d", __func__,
			test_rq->req_id, err);

	check_test_completion();
}

/**
 * run_long_seq_test - main function for long sequential test
 * @td - test specific data
 *
 * This function is used to fill up (and keep full) the test queue with
 * requests. There are two scenarios this function works with:
 * 1. Only read/write (STAGE_1 or no stage)
 * 2. Simultaneous read and write to the same LBAs (STAGE_2)
 */
static int run_long_seq_test(struct test_data *td)
{
	int ret = 0;
	int direction;
	static unsigned int inserted_requests;
	u32 sector;

	BUG_ON(!td);
	sector = td->start_sector;
	if (utd->test_stage != UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE2) {
		td->test_count = 0;
		utd->completed_req_count = 0;
		inserted_requests = 0;
	}

	/* Set the direction */
	switch (td->test_info.testcase) {
	case UFS_TEST_LONG_SEQUENTIAL_READ:
		direction = READ;
		break;
	case UFS_TEST_LONG_SEQUENTIAL_WRITE:
	case UFS_TEST_LONG_SEQUENTIAL_MIXED:
	default:
		direction = WRITE;
	}
	test_pr_info("%s: Adding %d requests, first req_id=%d",
		     __func__, LONG_SEQ_TEST_NUM_REQS,
		     td->wr_rd_next_req_id);

	do {
		/*
		* since our requests come from a pool containing 128
		* requests, we don't want to exhaust this quantity,
		* therefore we add up to QUEUE_MAX_REQUESTS (which
		* includes a safety margin) and then call the block layer
		* to fetch them
		*/
		if (td->test_count >= QUEUE_MAX_REQUESTS) {
			blk_run_queue(td->req_q);
			continue;
		}

		ret = test_iosched_add_wr_rd_test_req(0, direction, sector,
				TEST_MAX_BIOS_PER_REQ, TEST_PATTERN_5A,
				long_seq_test_free_end_io_fn);
		if (ret) {
			test_pr_err("%s: failed to create request" , __func__);
			break;
		}
		inserted_requests++;
		if (utd->test_stage == UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE2) {
			ret = test_iosched_add_wr_rd_test_req(0, READ, sector,
					TEST_MAX_BIOS_PER_REQ, TEST_PATTERN_5A,
					long_seq_test_free_end_io_fn);
			if (ret) {
				test_pr_err("%s: failed to create request" ,
						__func__);
				break;
			}
			inserted_requests++;
		}
		/* NUM_OF_BLOCK * (BLOCK_SIZE / SECTOR_SIZE) */
		sector += TEST_MAX_BIOS_PER_REQ * (PAGE_SIZE /
				td->req_q->limits.logical_block_size);
		td->test_info.test_byte_count +=
			(TEST_MAX_BIOS_PER_REQ * sizeof(unsigned int) *
			BIO_U32_SIZE);

	} while (inserted_requests < LONG_SEQ_TEST_NUM_REQS);

	/* in this case the queue will not run in the above loop */
	if (LONG_SEQ_TEST_NUM_REQS < QUEUE_MAX_REQUESTS)
		blk_run_queue(td->req_q);

	return ret;
}

static int run_mixed_long_seq_test(struct test_data *td)
{
	int ret;

	utd->test_stage = UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE1;
	ret = run_long_seq_test(td);
	if (ret)
		goto out;

	test_pr_info("%s: First write iteration completed.", __func__);
	test_pr_info("%s: Starting mixed write and reads sequence.", __func__);
	utd->test_stage = UFS_TEST_LONG_SEQUENTIAL_MIXED_STAGE2;
	ret = run_long_seq_test(td);
out:
	return ret;
}

static int long_seq_test_calc_throughput(struct test_data *td)
{
	unsigned long fraction, integer;
	unsigned long mtime, byte_count;

	mtime = ktime_to_ms(utd->test_info.test_duration);
	byte_count = utd->test_info.test_byte_count;

	test_pr_info("%s: time is %lu msec, size is %lu.%lu MiB",
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

	test_pr_info("%s: Throughput: %lu.%lu MiB/sec\n",
		__func__, integer, fraction);

	return 0;
}

static ssize_t ufs_test_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos, int test_case)
{
	int ret = 0;
	int i;
	int number;

	ret = kstrtoint_from_user(buf, count, 0, &number);
	if (ret < 0) {
		test_pr_err("%s: Error while reading test parameter value %d",
				__func__, ret);
		return ret;
	}

	if (number <= 0)
		number = 1;

	test_pr_info("%s:the test will run for %d iterations.",
			__func__, number);
	memset(&utd->test_info, 0, sizeof(struct test_info));

	/* Initializing test */
	utd->test_info.data = utd;
	utd->test_info.get_test_case_str_fn = ufs_test_get_test_case_str;
	utd->test_info.testcase = test_case;
	utd->test_info.get_rq_disk_fn = ufs_test_get_rq_disk;
	utd->test_stage = DEFAULT;

	switch (test_case) {
	case UFS_TEST_WRITE_READ_TEST:
		utd->test_info.run_test_fn = ufs_test_run_write_read_test;
		utd->test_info.check_test_completion_fn =
				ufs_write_read_completion;
		break;
	case UFS_TEST_LONG_SEQUENTIAL_READ:
	case UFS_TEST_LONG_SEQUENTIAL_WRITE:
		utd->test_info.run_test_fn = run_long_seq_test;
		utd->test_info.post_test_fn = long_seq_test_calc_throughput;
		utd->test_info.check_test_result_fn = ufs_test_check_result;
		break;
	case UFS_TEST_LONG_SEQUENTIAL_MIXED:
		utd->test_info.timeout_msec = LONG_SEQUENTIAL_MIXED_TIMOUT_MS;
		utd->test_info.run_test_fn = run_mixed_long_seq_test;
		utd->test_info.post_test_fn = long_seq_test_calc_throughput;
		utd->test_info.check_test_result_fn = ufs_test_check_result;
		break;
	default:
		test_pr_err("%s: Unknown test-case: %d", __func__, test_case);
		WARN_ON(true);
	}

	/* Running the test multiple times */
	for (i = 0; i < number; ++i) {
		test_pr_info("%s: Cycle # %d / %d", __func__, i+1, number);
		test_pr_info("%s: ====================", __func__);

		utd->test_info.test_byte_count = 0;
		ret = test_iosched_start_test(&utd->test_info);
		if (ret) {
			test_pr_err("%s: Test failed, err=%d.", __func__, ret);
			return ret;
		}

		/* Allow FS requests to be dispatched */
		msleep(1000);
	}

	test_pr_info("%s: Completed all the ufs test iterations.", __func__);

	return count;
}

TEST_OPS(write_read_test, WRITE_READ_TEST);
TEST_OPS(long_sequential_read, LONG_SEQUENTIAL_READ);
TEST_OPS(long_sequential_write, LONG_SEQUENTIAL_WRITE);
TEST_OPS(long_sequential_mixed, LONG_SEQUENTIAL_MIXED);

static void ufs_test_debugfs_cleanup(void)
{
	debugfs_remove_recursive(test_iosched_get_debugfs_tests_root());
	kfree(utd->test_list);
}

static int ufs_test_debugfs_init(void)
{
	struct dentry *utils_root, *tests_root;
	int ret = 0;

	utils_root = test_iosched_get_debugfs_utils_root();
	tests_root = test_iosched_get_debugfs_tests_root();

	utd->test_list = kmalloc(sizeof(struct dentry *) * NUM_TESTS,
			GFP_KERNEL);
	if (!utd->test_list) {
		test_pr_err("%s: failed to allocate tests dentrys", __func__);
		return -ENODEV;
	}

	if (!utils_root || !tests_root) {
		test_pr_err("%s: Failed to create debugfs root.", __func__);
		ret = -EINVAL;
		goto exit_err;
	}

	utd->random_test_seed_dentry = debugfs_create_u32("random_test_seed",
			S_IRUGO | S_IWUGO, utils_root, &utd->random_test_seed);

	if (!utd->random_test_seed_dentry) {
		test_pr_err("%s: Could not create debugfs random_test_seed.",
				__func__);
		ret = -ENOMEM;
		goto exit_err;
	}

	ret = add_test(utd, write_read_test, WRITE_READ_TEST);
	if (ret)
		goto exit_err;
	ret = add_test(utd, long_sequential_read, LONG_SEQUENTIAL_READ);
	if (ret)
		goto exit_err;
	ret = add_test(utd, long_sequential_write, LONG_SEQUENTIAL_WRITE);
	if (ret)
		goto exit_err;
	ret = add_test(utd, long_sequential_mixed, LONG_SEQUENTIAL_MIXED);
	if (ret)
		goto exit_err;

	goto exit;

exit_err:
	ufs_test_debugfs_cleanup();
exit:
	return ret;
}

static void ufs_test_probe(void)
{
	ufs_test_debugfs_init();
}

static void ufs_test_remove(void)
{
	ufs_test_debugfs_cleanup();
}

int __init ufs_test_init(void)
{
	utd = kzalloc(sizeof(struct ufs_test_data), GFP_KERNEL);
	if (!utd) {
		test_pr_err("%s: failed to allocate ufs_test_data", __func__);
		return -ENODEV;
	}

	init_waitqueue_head(&utd->wait_q);
	utd->bdt.init_fn = ufs_test_probe;
	utd->bdt.exit_fn = ufs_test_remove;
	INIT_LIST_HEAD(&utd->bdt.list);
	test_iosched_register(&utd->bdt);

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_test_init);

static void __exit ufs_test_exit(void)
{
	test_iosched_unregister(&utd->bdt);
	kfree(utd);
}
module_init(ufs_test_init);
module_exit(ufs_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("UFC test");

