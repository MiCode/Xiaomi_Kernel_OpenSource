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

#define MODULE_NAME "ufs_test"

#define TEST_MAX_BIOS_PER_REQ		120
#define LARGE_PRIME_1	1103515367
#define LARGE_PRIME_2	35757
#define DEFAULT_NUM_OF_BIOS	2

#define test_pr_debug(fmt, args...) pr_debug("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_info(fmt, args...) pr_info("%s: "fmt"\n", MODULE_NAME, args)
#define test_pr_err(fmt, args...) pr_err("%s: "fmt"\n", MODULE_NAME, args)

enum ufs_test_testcases {
	UFS_TEST_WRITE_READ_TEST,
};

struct ufs_test_debug {
	struct dentry *write_read_test; /* basic test */
	struct dentry *random_test_seed; /* parameters in utils */
};

struct ufs_test_data {
	/* Data structure for debugfs dentrys */
	struct ufs_test_debug debug;
	/*
	 * Data structure containing individual test information, including
	 * self-defined specific data
	 */
	struct test_info test_info;
	/* device test */
	struct blk_dev_test_type bdt;
	/* A wait queue for OPs to complete */
	wait_queue_head_t wait_q;
	/* a flag for read compleation */
	bool read_completed;
	/* a flag for write compleation */
	bool write_completed;
	/*
	 * To determine the number of r/w bios. When seed = 0, random is
	 * disabled and 2 BIOs are written.
	 */
	unsigned int random_test_seed;
};

static struct ufs_test_data *utd;

static bool message_repeat;

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

static void ufs_test_write_read_test_end_io_fn(struct request *rq, int err)
{
	struct test_request *test_rq = (struct test_request *)rq->elv.priv[0];
	BUG_ON(!test_rq);

	test_rq->req_completed = 1;
	test_rq->req_result = err;

	test_pr_info("%s: request %d completed, err=%d",
			__func__, test_rq->req_id, err);

	utd->write_completed = true;
	wake_up(&utd->wait_q);
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
	test_pr_info("%s: Adding a write requests to Q, first req_id=%d",
			__func__, td->wr_rd_next_req_id);

	utd->write_completed = false;
	ret = test_iosched_add_wr_rd_test_req(0, WRITE, start_sec,
					num_bios, TEST_PATTERN_5A,
					ufs_test_write_read_test_end_io_fn);

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

static
int ufs_test_write_read_test_open_cb(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	message_repeat = 1;
	test_pr_info("%s:UFS test initialized", __func__);
	return 0;
}

static ssize_t ufs_test_write_read_test_write_cb(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	int ret = 0;
	int i;
	int number;

	sscanf(buf, "%d", &number);

	if (number <= 0)
		number = 1;

	test_pr_info("%s:the test will run for %d iterations.",
			__func__, number);
	memset(&utd->test_info, 0, sizeof(struct test_info));

	/* Initializing test */
	utd->test_info.data = utd;
	utd->test_info.get_test_case_str_fn = ufs_test_get_test_case_str;
	utd->test_info.testcase = UFS_TEST_WRITE_READ_TEST;
	utd->test_info.get_rq_disk_fn = ufs_test_get_rq_disk;
	utd->test_info.run_test_fn = ufs_test_run_write_read_test;

	/* Running the test multiple times */
	for (i = 0; i < number; ++i) {
		ret = test_iosched_start_test(&utd->test_info);
		if (ret) {
			test_pr_err("%s: Test failed.", __func__);
			return ret;
		}
	}

	test_pr_info("%s: Completed all the ufs test iterations.", __func__);

	return count;
}

static ssize_t ufs_test_write_read_test_read_cb(struct file *file,
		char __user *buffer, size_t count, loff_t *offset)
{
	memset((void *) buffer, 0, count);

	snprintf(buffer, count, "\nThis is a UFS write-read test for debug.\n");

	if (message_repeat == 1) {
		message_repeat = 0;
		return strnlen(buffer, count);
	} else
		return 0;
}

const struct file_operations write_read_test_ops = {
		.open = ufs_test_write_read_test_open_cb,
		.write = ufs_test_write_read_test_write_cb,
		.read = ufs_test_write_read_test_read_cb,
};

static void ufs_test_debugfs_cleanup(void)
{
	debugfs_remove(utd->debug.write_read_test);
}

static int ufs_test_debugfs_init(void)
{
	struct dentry *utils_root, *tests_root;

	utils_root = test_iosched_get_debugfs_utils_root();
	tests_root = test_iosched_get_debugfs_tests_root();

	if (!utils_root || !tests_root) {
		test_pr_err("%s: Failed to create debugfs root.", __func__);
		return -EINVAL;
	}

	utd->debug.random_test_seed = debugfs_create_u32("random_test_seed",
			S_IRUGO | S_IWUGO, utils_root, &utd->random_test_seed);

	if (!utd->debug.random_test_seed) {
		test_pr_err("%s: Could not create debugfs random_test_seed.",
				__func__);
		return -ENOMEM;
	}

	utd->debug.write_read_test = debugfs_create_file("write_read_test",
					S_IRUGO | S_IWUGO, tests_root,
					NULL, &write_read_test_ops);

	if (!utd->debug.write_read_test) {
		debugfs_remove(utd->debug.random_test_seed);
		test_pr_err("%s: Could not create debugfs write_read_test.",
				__func__);
		return -ENOMEM;
	}

	return 0;
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
module_init(ufs_test_init)
;
module_exit(ufs_test_exit)
;

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("UFC test");

