/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

/*
 * SDIO-Abstraction-Layer Test Module.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <mach/sdio_smem.h>
#include <linux/wakelock.h>

#include <sdio_al_private.h>
#include <linux/debugfs.h>

#include <linux/kthread.h>
enum lpm_test_msg_type {
	LPM_NO_MSG,
	LPM_MSG_SEND,
	LPM_MSG_REC,
	LPM_SLEEP,
	LPM_WAKEUP
};

#define LPM_NO_MSG_NAME "LPM No Event"
#define LPM_MSG_SEND_NAME "LPM Send Msg Event"
#define LPM_MSG_REC_NAME "LPM Receive Msg Event"
#define LPM_SLEEP_NAME "LPM Sleep Event"
#define LPM_WAKEUP_NAME "LPM Wakeup Event"

/** Module name string */
#define TEST_MODULE_NAME "sdio_al_test"

#define TEST_SIGNATURE 0x12345678
#define TEST_CONFIG_SIGNATURE 0xBEEFCAFE

#define MAX_XFER_SIZE (16*1024)
#define SMEM_MAX_XFER_SIZE 0xBC000
#define A2_MIN_PACKET_SIZE 5

#define TEST_DBG(x...) if (test_ctx->runtime_debug) pr_info(x)

#define LPM_TEST_NUM_OF_PACKETS 100
#define LPM_ARRAY_SIZE	(7*LPM_TEST_NUM_OF_PACKETS)
#define SDIO_LPM_TEST "sdio_lpm_test_reading_task"
#define LPM_TEST_CONFIG_SIGNATURE 0xDEADBABE

enum sdio_test_case_type {
	SDIO_TEST_LOOPBACK_HOST,
	SDIO_TEST_LOOPBACK_CLIENT,
	SDIO_TEST_LPM_HOST_WAKER,
	SDIO_TEST_LPM_CLIENT_WAKER,
	SDIO_TEST_LPM_RANDOM,
	SDIO_TEST_HOST_SENDER_NO_LP,
	SDIO_TEST_PERF, /* must be last since is not part of the 9k tests */
};

struct lpm_task {
	struct task_struct *lpm_task;
	const char *task_name;
};

struct lpm_entry_type {
	enum lpm_test_msg_type msg_type;
	u32 counter;
	u32 current_ms;
};

static DEFINE_SPINLOCK(lpm_lock);
unsigned long flags;

struct lpm_msg {
	u32 signature;
	u32 counter;
	u32 reserve1;
	u32 reserve2;
};

struct lpm_test_struct {
	u64 start_jiffies;
	u64 end_jiffies;
	unsigned int total_ms;
	u32 next_avail_entry_in_array;
	u32 next_index_in_sent_msg;
	u32 last_index_in_rec_msg;
	struct lpm_task lpm_test_task;
	struct test_channel *test_ch;
	struct lpm_entry_type lpm_arr[LPM_ARRAY_SIZE];
};

struct test_config_msg {
	u32 signature;
	u32 test_case;
	u32 test_param;
	u32 num_packets;
	u32 num_iterations;
};

struct test_result_msg {
	u32 signature;
	u32 is_successful;
};

struct test_work {
	struct work_struct work;
	struct test_channel *test_ch;
};

enum sdio_channels_ids {
	SDIO_RPC,
	SDIO_QMI,
	SDIO_RMNT,
	SDIO_DIAG,
	SDIO_DUN,
	SDIO_SMEM,
	SDIO_CIQ,
	SDIO_MAX_CHANNELS
};

enum sdio_test_results {
	TEST_NO_RESULT,
	TEST_FAILED,
	TEST_PASSED
};

enum sdio_lpm_vote_state {
	SDIO_NO_VOTE,
	SDIO_VOTE_FOR_SLEEP,
	SDIO_VOTE_AGAINST_SLEEP
};

struct test_channel {
	struct sdio_channel *ch;

	char name[CHANNEL_NAME_SIZE];
	int ch_id;

	u32 *buf;
	u32 buf_size;

	struct workqueue_struct *workqueue;
	struct test_work test_work;

	u32 rx_bytes;
	u32 tx_bytes;

	wait_queue_head_t   wait_q;
	atomic_t rx_notify_count;
	atomic_t tx_notify_count;
	atomic_t any_notify_count;
	atomic_t wakeup_client;

	int wait_counter;

	int is_used;
	int test_type;
	int ch_ready;

	struct test_config_msg config_msg;

	int test_completed;
	int test_result;
	struct timer_list timer;
	int timer_interval_ms;

	struct timer_list timeout_timer;
	int timeout_ms;
	void *sdio_al_device;
	int is_ok_to_sleep;
	struct lpm_test_struct lpm_test_db;
	unsigned int packet_length;

	int random_packet_size;
};

struct sdio_al_test_debug {
	u32 dun_throughput;
	u32 rmnt_throughput;
	struct dentry *debug_root;
	struct dentry *debug_test_result;
	struct dentry *debug_dun_throughput;
	struct dentry *debug_rmnt_throughput;
};

struct test_context {
	dev_t dev_num;
	struct device *dev;
	struct cdev *cdev;

	struct test_channel *test_ch;

	struct test_channel *test_ch_arr[SDIO_MAX_CHANNELS];

	long testcase;

	const char *name;

	int exit_flag;

	u32 signature;

	int runtime_debug;

	struct platform_device smem_pdev;
	struct sdio_smem_client *sdio_smem;
	int smem_was_init;
	u8 *smem_buf;

	wait_queue_head_t   wait_q;
	int test_completed;
	int test_result;
	struct sdio_al_test_debug debug;

	struct wake_lock wake_lock;
};

/*
 * Seed for pseudo random time sleeping in Random LPM test.
 * If not set, current time in jiffies is used.
 */
static unsigned int seed;
module_param(seed, int, 0);

static struct test_context *test_ctx;

#ifdef CONFIG_DEBUG_FS
/*
*
* Trigger on/off for debug messages
* for trigger off the data messages debug level use:
* echo 0 > /sys/kernel/debugfs/sdio_al/debug_data_on
* for trigger on the data messages debug level use:
* echo 1 > /sys/kernel/debugfs/sdio_al/debug_data_on
* for trigger off the lpm messages debug level use:
* echo 0 > /sys/kernel/debugfs/sdio_al/debug_lpm_on
* for trigger on the lpm messages debug level use:
* echo 1 > /sys/kernel/debugfs/sdio_al/debug_lpm_on
*/
static int sdio_al_test_debugfs_init(void)
{
	test_ctx->debug.debug_root = debugfs_create_dir("sdio_al_test",
							       NULL);
	if (!test_ctx->debug.debug_root)
		return -ENOENT;

	test_ctx->debug.debug_test_result = debugfs_create_u32(
					"test_result",
					S_IRUGO | S_IWUGO,
					test_ctx->debug.debug_root,
					&test_ctx->test_result);

	test_ctx->debug.debug_dun_throughput = debugfs_create_u32(
					"dun_throughput",
					S_IRUGO | S_IWUGO,
					test_ctx->debug.debug_root,
					&test_ctx->debug.dun_throughput);

	test_ctx->debug.debug_rmnt_throughput = debugfs_create_u32(
					"rmnt_throughput",
					S_IRUGO | S_IWUGO,
					test_ctx->debug.debug_root,
					&test_ctx->debug.rmnt_throughput);

	if ((!test_ctx->debug.debug_dun_throughput) &&
	    (!test_ctx->debug.debug_rmnt_throughput)) {
		debugfs_remove_recursive(test_ctx->debug.debug_root);
		test_ctx->debug.debug_root = NULL;
		return -ENOENT;
	}
	return 0;
}

static void sdio_al_test_debugfs_cleanup(void)
{
       debugfs_remove(test_ctx->debug.debug_dun_throughput);
       debugfs_remove(test_ctx->debug.debug_rmnt_throughput);
       debugfs_remove(test_ctx->debug.debug_root);
}
#endif

static int channel_name_to_id(char *name)
{
	pr_info(TEST_MODULE_NAME "%s: channel name %s\n",
		__func__, name);

	if (!strncmp(name, "SDIO_RPC", strnlen("SDIO_RPC", CHANNEL_NAME_SIZE)))
		return SDIO_RPC;
	else if (!strncmp(name, "SDIO_QMI",
			  strnlen("SDIO_QMI", CHANNEL_NAME_SIZE)))
		return SDIO_QMI;
	else if (!strncmp(name, "SDIO_RMNT",
			  strnlen("SDIO_RMNT", CHANNEL_NAME_SIZE)))
		return SDIO_RMNT;
	else if (!strncmp(name, "SDIO_DIAG",
			  strnlen("SDIO_DIAG", CHANNEL_NAME_SIZE)))
		return SDIO_DIAG;
	else if (!strncmp(name, "SDIO_DUN",
			  strnlen("SDIO_DUN", CHANNEL_NAME_SIZE)))
		return SDIO_DUN;
	else if (!strncmp(name, "SDIO_SMEM",
			  strnlen("SDIO_SMEM", CHANNEL_NAME_SIZE)))
		return SDIO_SMEM;
	else if (!strncmp(name, "SDIO_CIQ",
			  strnlen("SDIO_CIQ", CHANNEL_NAME_SIZE)))
		return SDIO_CIQ;
	else
		return SDIO_MAX_CHANNELS;

	return SDIO_MAX_CHANNELS;
}

/**
 * Config message
 */

static void send_config_msg(struct test_channel *test_ch)
{
	int ret = 0 ;
	u32 write_avail = 0;
	int size = sizeof(test_ch->config_msg);

	pr_debug(TEST_MODULE_NAME "%s\n", __func__);

	memcpy(test_ch->buf, (void *)&test_ch->config_msg, size);

	if (test_ctx->exit_flag) {
		pr_info(TEST_MODULE_NAME ":Exit Test.\n");
		return;
	}

	pr_info(TEST_MODULE_NAME ":Sending the config message.\n");

	/* wait for data ready event */
	write_avail = sdio_write_avail(test_ch->ch);
	pr_debug(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
	if (write_avail < size) {
		wait_event(test_ch->wait_q,
			   atomic_read(&test_ch->tx_notify_count));
		atomic_dec(&test_ch->tx_notify_count);
	}

	write_avail = sdio_write_avail(test_ch->ch);
	pr_debug(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
	if (write_avail < size) {
		pr_info(TEST_MODULE_NAME ":not enough write avail.\n");
		return;
	}

	ret = sdio_write(test_ch->ch, test_ch->buf, size);
	if (ret)
		pr_err(TEST_MODULE_NAME ":%s sdio_write err=%d.\n",
			__func__, -ret);
	else
		pr_info(TEST_MODULE_NAME ":%s sent config_msg successfully.\n",
		       __func__);
}

/**
 * Loopback Test
 */
static void loopback_test(struct test_channel *test_ch)
{
	int ret = 0 ;
	u32 read_avail = 0;
	u32 write_avail = 0;

	while (1) {

		if (test_ctx->exit_flag) {
			pr_info(TEST_MODULE_NAME ":Exit Test.\n");
			return;
		}

		TEST_DBG(TEST_MODULE_NAME "--LOOPBACK WAIT FOR EVENT--.\n");
		/* wait for data ready event */
		wait_event(test_ch->wait_q,
			   atomic_read(&test_ch->rx_notify_count));
		atomic_dec(&test_ch->rx_notify_count);

		read_avail = sdio_read_avail(test_ch->ch);
		if (read_avail == 0)
			continue;


		write_avail = sdio_write_avail(test_ch->ch);
		if (write_avail < read_avail) {
			pr_info(TEST_MODULE_NAME
				":not enough write avail.\n");
			continue;
		}

		ret = sdio_read(test_ch->ch, test_ch->buf, read_avail);
		if (ret) {
			pr_info(TEST_MODULE_NAME
			       ":worker, sdio_read err=%d.\n", -ret);
			continue;
		}
		test_ch->rx_bytes += read_avail;

		TEST_DBG(TEST_MODULE_NAME ":worker total rx bytes = 0x%x.\n",
			 test_ch->rx_bytes);


		ret = sdio_write(test_ch->ch,
				 test_ch->buf, read_avail);
		if (ret) {
			pr_info(TEST_MODULE_NAME
				":loopback sdio_write err=%d.\n",
				-ret);
			continue;
		}
		test_ch->tx_bytes += read_avail;

		TEST_DBG(TEST_MODULE_NAME
			 ":loopback total tx bytes = 0x%x.\n",
			 test_ch->tx_bytes);
	} /* end of while */
}

/**
 * Check if all tests completed
 */
static void check_test_completion(void)
{
	int i;

	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];

		if ((!tch) || (!tch->is_used) || (!tch->ch_ready))
			continue;
		if (!tch->test_completed) {
			pr_info(TEST_MODULE_NAME ":Channel %s test is not "
						 "completed",
				tch->name);
			return;
		}
	}
	pr_info(TEST_MODULE_NAME ":Test is completed");
	test_ctx->test_completed = 1;
	wake_up(&test_ctx->wait_q);
}

static int pseudo_random_seed(unsigned int *seed_number)
{
	if (!seed_number)
		return 0;

	*seed_number = (unsigned int)(((unsigned long)*seed_number *
				(unsigned long)1103515367) + 35757);
	return (int)(*seed_number / (64*1024) % 1000);
}

static void lpm_test_update_entry(struct test_channel *tch,
				  enum lpm_test_msg_type msg_type,
				  int counter)
{
	u32 index = 0;
	static int print_full = 1;

	if (!tch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return;
	}

	if (tch->lpm_test_db.next_avail_entry_in_array >= LPM_ARRAY_SIZE) {
		pr_err(TEST_MODULE_NAME ": %s - lpm array is full",
			__func__);
		if (print_full) {
			print_hex_dump(KERN_INFO, TEST_MODULE_NAME ": lpm_arr:",
				0, 32, 2,
				(void *)tch->lpm_test_db.lpm_arr,
				sizeof(tch->lpm_test_db.lpm_arr), false);
			print_full = 0;
		}
		return;
	}

	index = tch->lpm_test_db.next_avail_entry_in_array;
	if ((msg_type == LPM_MSG_SEND) || (msg_type == LPM_MSG_REC))
		tch->lpm_test_db.lpm_arr[index].counter = counter;
	else
		tch->lpm_test_db.lpm_arr[index].counter = 0;
	tch->lpm_test_db.lpm_arr[index].msg_type = msg_type;
	tch->lpm_test_db.lpm_arr[index].current_ms =
		jiffies_to_msecs(get_jiffies_64());

	tch->lpm_test_db.next_avail_entry_in_array++;
}

static int wait_for_result_msg(struct test_channel *test_ch)
{
	u32 read_avail = 0;
	int ret = 0;

	pr_info(TEST_MODULE_NAME ": %s - START\n", __func__);

	while (1) {
		read_avail = sdio_read_avail(test_ch->ch);

		if (read_avail == 0) {
			pr_info(TEST_MODULE_NAME
				": read_avail is 0 for chan %s\n",
				test_ch->name);
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->rx_notify_count));
			atomic_dec(&test_ch->rx_notify_count);
			continue;
		}

		memset(test_ch->buf, 0x00, test_ch->buf_size);

		ret = sdio_read(test_ch->ch, test_ch->buf, read_avail);
		if (ret) {
			pr_info(TEST_MODULE_NAME ":  sdio_read for chan"
				"%s failed, err=%d.\n",
				test_ch->name, -ret);
			goto exit_err;
		}

		if (test_ch->buf[0] != TEST_CONFIG_SIGNATURE) {
			pr_info(TEST_MODULE_NAME ": Not a test_result "
				"signature. expected 0x%x. received 0x%x "
				"for chan %s\n",
				TEST_CONFIG_SIGNATURE,
				test_ch->buf[0],
				test_ch->name);
			continue;
		} else {
			pr_info(TEST_MODULE_NAME ": Signature is "
				"TEST_CONFIG_SIGNATURE as expected\n");
			break;
		}
	}

	return test_ch->buf[1];

exit_err:
	return 0;
}

static int check_random_lpm_test_array(struct test_channel *test_ch)
{
	int i = 0, j = 0;
	struct lpm_test_struct *lpm_db = &test_ch->lpm_test_db;
	unsigned int delta_ms = 0;
	int arr_ind = 0;

	if (!test_ch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return -ENODEV;
	}

	print_hex_dump(KERN_INFO, TEST_MODULE_NAME ": lpm_arr:", 0, 32, 2,
		       (void *)test_ch->lpm_test_db.lpm_arr,
		       sizeof(test_ch->lpm_test_db.lpm_arr), false);

	for (i = 0; i < lpm_db->next_avail_entry_in_array; i++) {
		if ((lpm_db->lpm_arr[i].msg_type == LPM_MSG_SEND) ||
		    (lpm_db->lpm_arr[i].msg_type == LPM_MSG_REC)) {
			/* find the next message in the array */
			arr_ind = lpm_db->next_avail_entry_in_array;
			for (j = i+1; j < arr_ind; j++) {
				if ((lpm_db->lpm_arr[j].msg_type ==
				     LPM_MSG_SEND) ||
				    (lpm_db->lpm_arr[j].msg_type ==
				     LPM_MSG_REC))
					break;
			}
			if (j == arr_ind) {
				if (lpm_db->lpm_arr[i].counter ==
				    test_ch->config_msg.num_packets - 1) {
					/* i is last msg in the array */
					return 1;
				} else {
					pr_err(TEST_MODULE_NAME "%s: invalid "
						"last msg, i=%d, counter=%d",
					       __func__, i,
					       lpm_db->lpm_arr[i].counter);
					return 0;
				}
			}
			delta_ms = lpm_db->lpm_arr[j].current_ms -
				lpm_db->lpm_arr[i].current_ms;
			if (delta_ms < 30) {
				if (j != i+1) {
					pr_err(TEST_MODULE_NAME "%s: lpm "
						"activity while delta is less "
						"than 30, i=%d, j=%d",
					       __func__, i, j);
					return 0;
				}
			} else {
				if (delta_ms > 90) {
					if (j != i+3) {
						pr_err(TEST_MODULE_NAME "%s: "
							"lpm activity while "
							"delta is less than "
							"30, i=%d, j=%d",
						       __func__, i, j);
						return 0;
					}
					if (lpm_db->lpm_arr[i+1].msg_type !=
					    LPM_SLEEP) {
						pr_err(TEST_MODULE_NAME "%s: "
							"no sleep when delta "
							"is bigger than 90"
							", i=%d, j=%d",
						       __func__, i, j);
						return 0;
					}
					if (lpm_db->lpm_arr[i+2].msg_type !=
					    LPM_WAKEUP) {
						pr_err(TEST_MODULE_NAME "%s: "
							"no sleep when delta "
							"is bigger than 90"
							", i=%d, j=%d",
						       __func__, i, j);
						return 0;
					}
				}
			}
		}

	}
	return 1;
}

static int lpm_test_main_task(void *ptr)
{
	u32 read_avail = 0;
	int last_msg_index = 0;
	struct test_channel *test_ch = (struct test_channel *)ptr;
	struct lpm_msg lpm_msg;
	int ret = 0;
	int modem_result = 0;
	int host_result = 0;

	pr_info(TEST_MODULE_NAME ": %s - STARTED\n", __func__);

	if (!test_ch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return -ENODEV;
	}

	while (last_msg_index < test_ch->config_msg.num_packets - 1) {
		TEST_DBG(TEST_MODULE_NAME ": %s - "
			"IN LOOP last_msg_index=%d\n",
		       __func__, last_msg_index);

		read_avail = sdio_read_avail(test_ch->ch);
		if (read_avail == 0) {
			TEST_DBG(TEST_MODULE_NAME
					":read_avail 0 for chan %s, "
					"wait for event\n",
					test_ch->name);
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->rx_notify_count));
			atomic_dec(&test_ch->rx_notify_count);

			read_avail = sdio_read_avail(test_ch->ch);
			if (read_avail == 0) {
				pr_err(TEST_MODULE_NAME
					":read_avail size %d for chan %s not as"
					" expected\n",
					read_avail, test_ch->name);
				continue;
			}
		}

		memset(test_ch->buf, 0x00, sizeof(test_ch->buf));

		ret = sdio_read(test_ch->ch, test_ch->buf, read_avail);
		if (ret) {
			pr_info(TEST_MODULE_NAME ":sdio_read for chan %s"
						 " err=%d.\n",
				test_ch->name, -ret);
			goto exit_err;
		}

		memcpy((void *)&lpm_msg, test_ch->buf, sizeof(lpm_msg));

		if (lpm_msg.signature != LPM_TEST_CONFIG_SIGNATURE) {
			pr_err(TEST_MODULE_NAME ": Not lpm test_result "
				"signature. expected 0x%x. received 0x%x "
				"for chan %s\n",
				LPM_TEST_CONFIG_SIGNATURE,
				lpm_msg.signature,
				test_ch->name);
			continue;
		} else {
			pr_debug(TEST_MODULE_NAME ": Signature is "
				"LPM_TEST_CONFIG_SIGNATURE as expected\n");

			spin_lock_irqsave(&lpm_lock, flags);
			last_msg_index = lpm_msg.counter;
			test_ch->lpm_test_db.last_index_in_rec_msg =
				last_msg_index;
			lpm_test_update_entry(test_ch, LPM_MSG_REC,
					      last_msg_index);
			spin_unlock_irqrestore(&lpm_lock, flags);
			continue;
		}
	}

	pr_info(TEST_MODULE_NAME ":%s: Finished to recieve all apckets from the"
				 " modem, waiting for result_msg", __func__);

	/* Wait for the resault message from the modem */
	modem_result = wait_for_result_msg(test_ch);

	pr_info(TEST_MODULE_NAME ": modem result was %d", modem_result);

	/* Wait for all the packets to be sent to the modem */
	while (1) {
		spin_lock_irqsave(&lpm_lock, flags);
		if (test_ch->lpm_test_db.next_index_in_sent_msg >=
		    test_ch->config_msg.num_packets - 1) {
			spin_unlock_irqrestore(&lpm_lock, flags);
			break;
		} else {
			pr_info(TEST_MODULE_NAME ":%s: Didn't finished to send "
				"all apckets, next_index_in_sent_msg = %d ",
				__func__,
				test_ch->lpm_test_db.next_index_in_sent_msg);
		}
		spin_unlock_irqrestore(&lpm_lock, flags);
		msleep(60);
	}

	sdio_al_unregister_lpm_cb(test_ch->sdio_al_device);

	host_result = check_random_lpm_test_array(test_ch);

	test_ch->test_completed = 1;
	if (modem_result && host_result) {
		pr_info(TEST_MODULE_NAME ": Random LPM TEST_PASSED for ch %s",
			test_ch->name);
		test_ch->test_result = TEST_PASSED;
	} else {
		pr_info(TEST_MODULE_NAME ": Random LPM TEST_FAILED for ch %s",
			test_ch->name);
		test_ch->test_result = TEST_FAILED;
	}

	check_test_completion();

	return 0;

exit_err:
	pr_info(TEST_MODULE_NAME ": TEST FAIL for chan %s.\n",
		test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_FAILED;
	check_test_completion();
	return -ENODEV;
}

static int lpm_test_create_read_thread(struct test_channel *test_ch)
{
	pr_info(TEST_MODULE_NAME ": %s - STARTED\n", __func__);

	if (!test_ch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return -ENODEV;
	}

	test_ch->lpm_test_db.lpm_test_task.task_name = SDIO_LPM_TEST;

	test_ch->lpm_test_db.lpm_test_task.lpm_task =
		kthread_create(lpm_test_main_task,
			       (void *)(test_ch),
			       test_ch->lpm_test_db.lpm_test_task.task_name);

	if (IS_ERR(test_ch->lpm_test_db.lpm_test_task.lpm_task)) {
		pr_err(TEST_MODULE_NAME ": %s - kthread_create() failed\n",
			__func__);
		return -ENOMEM;
	}

	wake_up_process(test_ch->lpm_test_db.lpm_test_task.lpm_task);

	return 0;
}

static void lpm_continuous_rand_test(struct test_channel *test_ch)
{
	unsigned int local_ms = 0;
	int ret = 0;
	unsigned int write_avail = 0;

	pr_info(MODULE_NAME ": %s - STARTED\n", __func__);

	if (!test_ch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return;
	}

	/* initialize lpm_test_db */
	test_ch->lpm_test_db.next_avail_entry_in_array = 0;
	test_ch->lpm_test_db.next_index_in_sent_msg = 0;
	 /* read the current time */
	test_ch->lpm_test_db.start_jiffies = get_jiffies_64();

	pr_err(TEST_MODULE_NAME ": %s - initializing the lpm_array", __func__);

	memset(test_ch->lpm_test_db.lpm_arr, 0,
	       sizeof(test_ch->lpm_test_db.lpm_arr));

	ret = lpm_test_create_read_thread(test_ch);
	if (ret != 0) {
		pr_err(TEST_MODULE_NAME ": %s - failed to create lpm reading "
		       "thread", __func__);
	}

	while (test_ch->lpm_test_db.next_index_in_sent_msg <=
	      test_ch->config_msg.num_packets - 1) {
		struct lpm_msg msg;
		u32 ret = 0;

		local_ms = pseudo_random_seed(&test_ch->config_msg.test_param);
		TEST_DBG(TEST_MODULE_NAME ":%s: SLEEPING for %d ms",
		       __func__, local_ms);
		msleep(local_ms);

		msg.counter = test_ch->lpm_test_db.next_index_in_sent_msg;
		msg.signature = LPM_TEST_CONFIG_SIGNATURE;
		msg.reserve1 = 0;
		msg.reserve2 = 0;

		/* wait for data ready event */
		write_avail = sdio_write_avail(test_ch->ch);
		pr_debug(TEST_MODULE_NAME ": %s: write_avail=%d\n",
		       __func__, write_avail);
		if (write_avail < sizeof(msg)) {
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->tx_notify_count));
			atomic_dec(&test_ch->tx_notify_count);
		}

		write_avail = sdio_write_avail(test_ch->ch);
		if (write_avail < sizeof(msg)) {
			pr_info(TEST_MODULE_NAME ": %s: not enough write "
				"avail.\n", __func__);
			break;
		}

		ret = sdio_write(test_ch->ch, (u32 *)&msg, sizeof(msg));
		if (ret)
			pr_err(TEST_MODULE_NAME ":%s: sdio_write err=%d.\n",
				__func__, -ret);

		TEST_DBG(TEST_MODULE_NAME ": %s: write, index=%d\n",
		       __func__, test_ch->lpm_test_db.next_index_in_sent_msg);

		spin_lock_irqsave(&lpm_lock, flags);
		lpm_test_update_entry(test_ch, LPM_MSG_SEND,
			test_ch->lpm_test_db.next_index_in_sent_msg);
		test_ch->lpm_test_db.next_index_in_sent_msg++;
		spin_unlock_irqrestore(&lpm_lock, flags);
	}

	pr_info(TEST_MODULE_NAME ": %s: Finished to send all packets to "
			"the modem", __func__);

	return;
}

static void lpm_test(struct test_channel *test_ch)
{
	int modem_result = 0;

	pr_info(TEST_MODULE_NAME ": %s - START\n", __func__);

	if (!test_ch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return;
	}

	modem_result = wait_for_result_msg(test_ch);
	pr_debug(TEST_MODULE_NAME ": %s - delete the timeout timer\n",
	       __func__);
	del_timer_sync(&test_ch->timeout_timer);

	if (modem_result == 0) {
		pr_err(TEST_MODULE_NAME ": LPM TEST - Client didn't sleep. "
		       "Result Msg - is_successful=%d\n", test_ch->buf[1]);
		goto exit_err;
	} else {
		pr_info(TEST_MODULE_NAME ": %s -"
			"LPM 9K WAS SLEEPING - PASS\n", __func__);
		if (test_ch->test_result == TEST_PASSED) {
			pr_info(TEST_MODULE_NAME ": LPM TEST_PASSED\n");
			test_ch->test_completed = 1;
			check_test_completion();
		} else {
			pr_err(TEST_MODULE_NAME ": LPM TEST - Host didn't "
			       "sleep. Client slept\n");
			goto exit_err;
		}
	}

	return;

exit_err:
	pr_info(TEST_MODULE_NAME ": TEST FAIL for chan %s.\n",
		test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_FAILED;
	check_test_completion();
	return;
}


/**
 * LPM Test while the host wakes up the modem
 */
static void lpm_test_host_waker(struct test_channel *test_ch)
{
	pr_info(TEST_MODULE_NAME ": %s - START\n", __func__);
	wait_event(test_ch->wait_q, atomic_read(&test_ch->wakeup_client));
	atomic_set(&test_ch->wakeup_client, 0);

	pr_info(TEST_MODULE_NAME ": %s - Sending the config_msg to wakeup "
		" the client\n", __func__);
	send_config_msg(test_ch);

	lpm_test(test_ch);
}

/**
 * sender Test
 */
static void sender_test(struct test_channel *test_ch)
{
	int ret = 0 ;
	u32 read_avail = 0;
	u32 write_avail = 0;
	int packet_count = 0;
	int size = 512;
	u16 *buf16 = (u16 *) test_ch->buf;
	int i;
	int max_packet_count = 10000;
	int random_num = 0;

	max_packet_count = test_ch->config_msg.num_packets;

	for (i = 0 ; i < size / 2 ; i++)
		buf16[i] = (u16) (i & 0xFFFF);


	pr_info(TEST_MODULE_NAME
		 ":SENDER TEST START for chan %s\n", test_ch->name);

	while (packet_count < max_packet_count) {

		if (test_ctx->exit_flag) {
			pr_info(TEST_MODULE_NAME ":Exit Test.\n");
			return;
		}

		random_num = get_random_int();
		size = (random_num % test_ch->packet_length) + 1;

		TEST_DBG(TEST_MODULE_NAME "SENDER WAIT FOR EVENT for chan %s\n",
			test_ch->name);

		/* wait for data ready event */
		write_avail = sdio_write_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
		if (write_avail < size) {
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->tx_notify_count));
			atomic_dec(&test_ch->tx_notify_count);
		}

		write_avail = sdio_write_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
		if (write_avail < size) {
			pr_info(TEST_MODULE_NAME ":not enough write avail.\n");
			continue;
		}

		test_ch->buf[0] = packet_count;

		ret = sdio_write(test_ch->ch, test_ch->buf, size);
		if (ret) {
			pr_info(TEST_MODULE_NAME ":sender sdio_write err=%d.\n",
				-ret);
			goto exit_err;
		}

		/* wait for read data ready event */
		TEST_DBG(TEST_MODULE_NAME ":sender wait for rx data for "
					  "chan %s\n",
			 test_ch->name);
		read_avail = sdio_read_avail(test_ch->ch);
		wait_event(test_ch->wait_q,
			   atomic_read(&test_ch->rx_notify_count));
		atomic_dec(&test_ch->rx_notify_count);

		read_avail = sdio_read_avail(test_ch->ch);

		if (read_avail != size) {
			pr_info(TEST_MODULE_NAME
				":read_avail size %d for chan %s not as "
				"expected size %d.\n",
				read_avail, test_ch->name, size);
			goto exit_err;
		}

		memset(test_ch->buf, 0x00, size);

		ret = sdio_read(test_ch->ch, test_ch->buf, size);
		if (ret) {
			pr_info(TEST_MODULE_NAME ":sender sdio_read for chan %s"
						 " err=%d.\n",
				test_ch->name, -ret);
			goto exit_err;
		}


		if ((test_ch->buf[0] != packet_count) && (size != 1)) {
			pr_info(TEST_MODULE_NAME ":sender sdio_read WRONG DATA"
						 " for chan %s, size=%d\n",
				test_ch->name, size);
			goto exit_err;
		}

		test_ch->tx_bytes += size;
		test_ch->rx_bytes += size;
		packet_count++;

		TEST_DBG(TEST_MODULE_NAME
			 ":sender total rx bytes = 0x%x , packet#=%d, size=%d"
			 " for chan %s\n",
			 test_ch->rx_bytes, packet_count, size, test_ch->name);
		TEST_DBG(TEST_MODULE_NAME
			 ":sender total tx bytes = 0x%x , packet#=%d, size=%d"
			 " for chan %s\n",
			 test_ch->tx_bytes, packet_count, size, test_ch->name);

	} /* end of while */

	pr_info(TEST_MODULE_NAME
		 ":SENDER TEST END: total rx bytes = 0x%x, "
		 " total tx bytes = 0x%x for chan %s\n",
		 test_ch->rx_bytes, test_ch->tx_bytes, test_ch->name);

	pr_info(TEST_MODULE_NAME ": TEST PASS for chan %s.\n",
		test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_PASSED;
	check_test_completion();
	return;

exit_err:
	pr_info(TEST_MODULE_NAME ": TEST FAIL for chan %s.\n",
		test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_FAILED;
	check_test_completion();
	return;
}

/**
 * A2 Perf Test
 */
static void a2_performance_test(struct test_channel *test_ch)
{
	int ret = 0 ;
	u32 read_avail = 0;
	u32 write_avail = 0;
	int tx_packet_count = 0;
	int rx_packet_count = 0;
	int size = 0;
	u16 *buf16 = (u16 *) test_ch->buf;
	int i;
	int total_bytes = 0;
	int max_packets = 10000;
	u32 packet_size = test_ch->buf_size;
	int rand_size = 0;

	u64 start_jiffy, end_jiffy, delta_jiffies;
	unsigned int time_msec = 0;
	u32 throughput = 0;

	max_packets = test_ch->config_msg.num_packets;
	packet_size = test_ch->packet_length;

	for (i = 0; i < packet_size / 2; i++)
		buf16[i] = (u16) (i & 0xFFFF);

	pr_info(TEST_MODULE_NAME ": A2 PERFORMANCE TEST START for chan %s\n",
		test_ch->name);

	start_jiffy = get_jiffies_64(); /* read the current time */

	while (tx_packet_count < max_packets) {

		if (test_ctx->exit_flag) {
			pr_info(TEST_MODULE_NAME ":Exit Test.\n");
			return;
		}

		if (test_ch->random_packet_size) {
			rand_size = get_random_int();
			packet_size = (rand_size % test_ch->packet_length) + 1;
			if (packet_size < A2_MIN_PACKET_SIZE)
				packet_size = A2_MIN_PACKET_SIZE;
		}

		/* wait for data ready event */
		/* use a func to avoid compiler optimizations */
		write_avail = sdio_write_avail(test_ch->ch);
		read_avail = sdio_read_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":channel %s, write_avail=%d, "
					 "read_avail=%d for chan %s\n",
			test_ch->name, write_avail, read_avail,
			test_ch->name);
		if ((write_avail == 0) && (read_avail == 0)) {
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->any_notify_count));
			atomic_set(&test_ch->any_notify_count, 0);
		}

		write_avail = sdio_write_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":channel %s, write_avail=%d\n",
			 test_ch->name, write_avail);
		if (write_avail > 0) {
			size = min(packet_size, write_avail) ;
			TEST_DBG(TEST_MODULE_NAME ":tx size = %d for chan %s\n",
				 size, test_ch->name);
			test_ch->buf[0] = tx_packet_count;
			test_ch->buf[(size/4)-1] = tx_packet_count;

			ret = sdio_write(test_ch->ch, test_ch->buf, size);
			if (ret) {
				pr_info(TEST_MODULE_NAME ":sdio_write err=%d"
							 " for chan %s\n",
					-ret, test_ch->name);
				goto exit_err;
			}
			tx_packet_count++;
			test_ch->tx_bytes += size;
		}

		read_avail = sdio_read_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":channel %s, read_avail=%d\n",
			 test_ch->name, read_avail);
		if (read_avail > 0) {
			size = min(packet_size, read_avail);
			pr_debug(TEST_MODULE_NAME ":rx size = %d.\n", size);
			ret = sdio_read(test_ch->ch, test_ch->buf, size);
			if (ret) {
				pr_info(TEST_MODULE_NAME ": sdio_read size %d "
							 " err=%d"
							 " for chan %s\n",
					size, -ret, test_ch->name);
				goto exit_err;
			}
			rx_packet_count++;
			test_ch->rx_bytes += size;
		}

		TEST_DBG(TEST_MODULE_NAME
			 ":total rx bytes = %d , rx_packet#=%d"
			 " for chan %s\n",
			 test_ch->rx_bytes, rx_packet_count, test_ch->name);
		TEST_DBG(TEST_MODULE_NAME
			 ":total tx bytes = %d , tx_packet#=%d"
			 " for chan %s\n",
			 test_ch->tx_bytes, tx_packet_count, test_ch->name);

	} /* while (tx_packet_count < max_packets ) */

	end_jiffy = get_jiffies_64(); /* read the current time */

	delta_jiffies = end_jiffy - start_jiffy;
	time_msec = jiffies_to_msecs(delta_jiffies);

	pr_info(TEST_MODULE_NAME ":total rx bytes = 0x%x , rx_packet#=%d for"
				 " chan %s.\n",
		test_ch->rx_bytes, rx_packet_count, test_ch->name);
	pr_info(TEST_MODULE_NAME ":total tx bytes = 0x%x , tx_packet#=%d"
				 " for chan %s.\n",
		test_ch->tx_bytes, tx_packet_count, test_ch->name);

	total_bytes = (test_ch->tx_bytes + test_ch->rx_bytes);
	pr_err(TEST_MODULE_NAME ":total bytes = %d, time msec = %d"
				" for chan %s\n",
		   total_bytes , (int) time_msec, test_ch->name);

	if (!test_ch->random_packet_size) {
		throughput = (total_bytes / time_msec) * 8 / 1000;
		pr_err(TEST_MODULE_NAME ":Performance = %d Mbit/sec for "
					"chan %s\n",
		       throughput, test_ch->name);
	}

#ifdef CONFIG_DEBUG_FS
	switch (test_ch->ch_id) {
	case SDIO_DUN:
		test_ctx->debug.dun_throughput = throughput;
		break;
	case SDIO_RMNT:
		test_ctx->debug.rmnt_throughput = throughput;
		break;
	default:
		pr_err(TEST_MODULE_NAME "No debugfs for this channel "
					"throughput");
	}
#endif

	pr_err(TEST_MODULE_NAME ": A2 PERFORMANCE TEST END for chan %s.\n",
	       test_ch->name);

	pr_err(TEST_MODULE_NAME ": TEST PASS for chan %s\n", test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_PASSED;
	check_test_completion();
	return;

exit_err:
	pr_err(TEST_MODULE_NAME ": TEST FAIL for chan %s\n", test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_FAILED;
	check_test_completion();
	return;
}

/**
 * sender No loopback Test
 */
static void sender_no_loopback_test(struct test_channel *test_ch)
{
	int ret = 0 ;
	u32 write_avail = 0;
	int packet_count = 0;
	int size = 512;
	u16 *buf16 = (u16 *) test_ch->buf;
	int i;
	int max_packet_count = 10000;
	int random_num = 0;
	int modem_result;

	max_packet_count = test_ch->config_msg.num_packets;

	for (i = 0 ; i < size / 2 ; i++)
		buf16[i] = (u16) (i & 0xFFFF);

	pr_info(TEST_MODULE_NAME
		 ":SENDER NO LP TEST START for chan %s\n", test_ch->name);

	while (packet_count < max_packet_count) {

		if (test_ctx->exit_flag) {
			pr_info(TEST_MODULE_NAME ":Exit Test.\n");
			return;
		}

		random_num = get_random_int();
		size = (random_num % test_ch->packet_length) + 1;

		TEST_DBG(TEST_MODULE_NAME ":SENDER WAIT FOR EVENT "
					  "for chan %s\n",
			test_ch->name);

		/* wait for data ready event */
		write_avail = sdio_write_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
		if (write_avail < size) {
			wait_event(test_ch->wait_q,
				   atomic_read(&test_ch->tx_notify_count));
			atomic_dec(&test_ch->tx_notify_count);
		}

		write_avail = sdio_write_avail(test_ch->ch);
		TEST_DBG(TEST_MODULE_NAME ":write_avail=%d\n", write_avail);
		if (write_avail < size) {
			pr_info(TEST_MODULE_NAME ":not enough write avail.\n");
			continue;
		}

		test_ch->buf[0] = packet_count;

		ret = sdio_write(test_ch->ch, test_ch->buf, size);
		if (ret) {
			pr_info(TEST_MODULE_NAME ":sender sdio_write err=%d.\n",
				-ret);
			goto exit_err;
		}

		test_ch->tx_bytes += size;
		packet_count++;

		TEST_DBG(TEST_MODULE_NAME
			 ":sender total tx bytes = 0x%x , packet#=%d, size=%d"
			 " for chan %s\n",
			 test_ch->tx_bytes, packet_count, size, test_ch->name);

	} /* end of while */

	pr_info(TEST_MODULE_NAME
		 ":SENDER TEST END: total tx bytes = 0x%x, "
		 " for chan %s\n",
		 test_ch->tx_bytes, test_ch->name);

	modem_result = wait_for_result_msg(test_ch);

	if (modem_result) {
		pr_info(TEST_MODULE_NAME ": TEST PASS for chan %s.\n",
			test_ch->name);
		test_ch->test_result = TEST_PASSED;
	} else {
		pr_info(TEST_MODULE_NAME ": TEST FAILURE for chan %s.\n",
			test_ch->name);
		test_ch->test_result = TEST_FAILED;
	}
	test_ch->test_completed = 1;
	check_test_completion();
	return;

exit_err:
	pr_info(TEST_MODULE_NAME ": TEST FAIL for chan %s.\n",
		test_ch->name);
	test_ch->test_completed = 1;
	test_ch->test_result = TEST_FAILED;
	check_test_completion();
	return;
}

/**
 * Worker thread to handle the tests types
 */
static void worker(struct work_struct *work)
{
	struct test_channel *test_ch = NULL;
	struct test_work *test_work = container_of(work,
						 struct	test_work,
						 work);
	int test_type = 0;

	test_ch = test_work->test_ch;

	if (test_ch == NULL) {
		pr_err(TEST_MODULE_NAME ":NULL test_ch\n");
		return;
	}

	test_type = test_ch->test_type;

	switch (test_type) {
	case SDIO_TEST_LOOPBACK_HOST:
		loopback_test(test_ch);
		break;
	case SDIO_TEST_LOOPBACK_CLIENT:
		sender_test(test_ch);
		break;
	case SDIO_TEST_PERF:
		a2_performance_test(test_ch);
		break;
	case SDIO_TEST_LPM_CLIENT_WAKER:
		lpm_test(test_ch);
		break;
	case SDIO_TEST_LPM_HOST_WAKER:
		lpm_test_host_waker(test_ch);
		break;
	case SDIO_TEST_HOST_SENDER_NO_LP:
		sender_no_loopback_test(test_ch);
		break;
	case SDIO_TEST_LPM_RANDOM:
		lpm_continuous_rand_test(test_ch);
		break;
	default:
		pr_err(TEST_MODULE_NAME ":Bad Test type = %d.\n",
			(int) test_type);
	}
}


/**
 * Notification Callback
 *
 * Notify the worker
 *
 */
static void notify(void *priv, unsigned channel_event)
{
	struct test_channel *test_ch = (struct test_channel *) priv;

	pr_debug(TEST_MODULE_NAME ":notify event=%d.\n", channel_event);

	if (test_ch->ch == NULL) {
		pr_info(TEST_MODULE_NAME ":notify before ch ready.\n");
		return;
	}

	switch (channel_event) {
	case SDIO_EVENT_DATA_READ_AVAIL:
		atomic_inc(&test_ch->rx_notify_count);
		atomic_set(&test_ch->any_notify_count, 1);
		TEST_DBG(TEST_MODULE_NAME ":SDIO_EVENT_DATA_READ_AVAIL, "
					  "any_notify_count=%d, "
					  "rx_notify_count=%d\n",
			 atomic_read(&test_ch->any_notify_count),
			 atomic_read(&test_ch->rx_notify_count));
		break;

	case SDIO_EVENT_DATA_WRITE_AVAIL:
		atomic_inc(&test_ch->tx_notify_count);
		atomic_set(&test_ch->any_notify_count, 1);
		TEST_DBG(TEST_MODULE_NAME ":SDIO_EVENT_DATA_WRITE_AVAIL, "
					  "any_notify_count=%d, "
					  "tx_notify_count=%d\n",
			 atomic_read(&test_ch->any_notify_count),
			 atomic_read(&test_ch->tx_notify_count));
		break;

	default:
		BUG();
	}
	wake_up(&test_ch->wait_q);

}

#ifdef CONFIG_MSM_SDIO_SMEM
static int sdio_smem_test_cb(int event)
{
	struct test_channel *tch = test_ctx->test_ch_arr[SDIO_SMEM];
	pr_debug(TEST_MODULE_NAME ":%s: Received event %d\n", __func__, event);

	switch (event) {
	case SDIO_SMEM_EVENT_READ_DONE:
		tch->rx_bytes += SMEM_MAX_XFER_SIZE;
		if (tch->rx_bytes >= 40000000) {
			if (!tch->test_completed) {
				pr_info(TEST_MODULE_NAME ":SMEM test PASSED\n");
				tch->test_completed = 1;
				tch->test_result = TEST_PASSED;
				check_test_completion();
			}

		}
		break;
	case SDIO_SMEM_EVENT_READ_ERR:
		pr_err(TEST_MODULE_NAME ":Read overflow, SMEM test FAILED\n");
		tch->test_completed = 1;
		tch->test_result = TEST_FAILED;
		check_test_completion();
		return -EIO;
	default:
		pr_err(TEST_MODULE_NAME ":Unhandled event\n");
		return -EINVAL;
	}
	return 0;
}

static int sdio_smem_test_probe(struct platform_device *pdev)
{
	int ret = 0;

	test_ctx->sdio_smem = container_of(pdev, struct sdio_smem_client,
					   plat_dev);

	test_ctx->sdio_smem->buf = test_ctx->smem_buf;
	test_ctx->sdio_smem->size = SMEM_MAX_XFER_SIZE;
	test_ctx->sdio_smem->cb_func = sdio_smem_test_cb;
	ret = sdio_smem_register_client();
	if (ret)
		pr_info(TEST_MODULE_NAME "%s: Error (%d) registering sdio_smem "
					 "test client\n",
			__func__, ret);
	return ret;
}

static struct platform_driver sdio_smem_drv = {
	.probe		= sdio_smem_test_probe,
	.driver		= {
		.name	= "SDIO_SMEM_CLIENT",
		.owner	= THIS_MODULE,
	},
};
#endif


static void default_sdio_al_test_release(struct device *dev)
{
	pr_info(MODULE_NAME ":platform device released.\n");
}

static void sdio_test_lpm_timeout_handler(unsigned long data)
{
	struct test_channel *tch = (struct test_channel *)data;

	pr_info(TEST_MODULE_NAME ": %s - LPM TEST TIMEOUT Expired after "
			    "%d ms\n", __func__, tch->timeout_ms);
	tch->test_completed = 1;
	pr_info(TEST_MODULE_NAME ": %s - tch->test_result = TEST_FAILED\n",
		__func__);
	tch->test_completed = 1;
	tch->test_result = TEST_FAILED;
	check_test_completion();
	return;
}

static void sdio_test_lpm_timer_handler(unsigned long data)
{
	struct test_channel *tch = (struct test_channel *)data;

	pr_info(TEST_MODULE_NAME ": %s - LPM TEST Timer Expired after "
			    "%d ms\n", __func__, tch->timer_interval_ms);

	if (!tch) {
		pr_err(TEST_MODULE_NAME ": %s - LPM TEST FAILED. "
		       "tch is NULL\n", __func__);
		return;
	}

	if (!tch->ch) {
		pr_err(TEST_MODULE_NAME ": %s - LPM TEST FAILED. tch->ch "
		       "is NULL\n", __func__);
		tch->test_result = TEST_FAILED;
		return;
	}

	/* Verfiy that we voted for sleep */
	if (tch->is_ok_to_sleep) {
		tch->test_result = TEST_PASSED;
		pr_info(TEST_MODULE_NAME ": %s - 8K voted for sleep\n",
			__func__);
	} else {
		tch->test_result = TEST_FAILED;
		pr_info(TEST_MODULE_NAME ": %s - 8K voted against sleep\n",
			__func__);

	}
	sdio_al_unregister_lpm_cb(tch->sdio_al_device);

	if (tch->test_type == SDIO_TEST_LPM_HOST_WAKER) {
		atomic_set(&tch->wakeup_client, 1);
		wake_up(&tch->wait_q);
	}
}

int sdio_test_wakeup_callback(void *device_handle, int is_vote_for_sleep)
{
	int i = 0;

	TEST_DBG(TEST_MODULE_NAME ": %s is_vote_for_sleep=%d!!!",
		__func__, is_vote_for_sleep);

	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];

		if ((!tch) || (!tch->is_used) || (!tch->ch_ready))
			continue;
		if (tch->sdio_al_device == device_handle) {
			tch->is_ok_to_sleep = is_vote_for_sleep;

			spin_lock_irqsave(&lpm_lock, flags);
			if (is_vote_for_sleep == 1)
				lpm_test_update_entry(tch, LPM_SLEEP, 0);
			else
				lpm_test_update_entry(tch, LPM_WAKEUP, 0);
			spin_unlock_irqrestore(&lpm_lock, flags);
		}
	}


	return 0;
}

/**
 * Test Main
 */
static int test_start(void)
{
	int ret = -ENOMEM;
	int i;

	pr_debug(TEST_MODULE_NAME ":Starting Test ....\n");

	test_ctx->test_completed = 0;
	test_ctx->test_result = TEST_NO_RESULT;
	test_ctx->debug.dun_throughput = 0;
	test_ctx->debug.rmnt_throughput = 0;

	/* Open The Channels */
	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];

		if ((!tch) || (!tch->is_used))
			continue;

		tch->rx_bytes = 0;
		tch->tx_bytes = 0;

		atomic_set(&tch->tx_notify_count, 0);
		atomic_set(&tch->rx_notify_count, 0);
		atomic_set(&tch->any_notify_count, 0);
		atomic_set(&tch->wakeup_client, 0);

		memset(tch->buf, 0x00, tch->buf_size);
		tch->test_result = TEST_NO_RESULT;

		tch->test_completed = 0;

		memset(&tch->lpm_test_db, 0, sizeof(tch->lpm_test_db));

		if (!tch->ch_ready) {
			pr_info(TEST_MODULE_NAME ":openning channel %s\n",
				tch->name);
			tch->ch_ready = true;
			if (tch->ch_id == SDIO_SMEM) {
				test_ctx->smem_pdev.name = "SDIO_SMEM";
				test_ctx->smem_pdev.dev.release =
					default_sdio_al_test_release;
				platform_device_register(&test_ctx->smem_pdev);
			} else {
				ret = sdio_open(tch->name , &tch->ch, tch,
						notify);
				if (ret) {
					pr_info(TEST_MODULE_NAME
						":openning channel %s failed\n",
					tch->name);
					tch->ch_ready = false;
				}

				tch->sdio_al_device = tch->ch->sdio_al_dev;
			}
		}

		if ((tch->ch_ready) && (tch->ch_id != SDIO_SMEM))
			send_config_msg(tch);

		if ((tch->test_type == SDIO_TEST_LPM_HOST_WAKER) ||
		    (tch->test_type == SDIO_TEST_LPM_CLIENT_WAKER) ||
		    (tch->test_type == SDIO_TEST_LPM_RANDOM)) {
			sdio_al_register_lpm_cb(tch->sdio_al_device,
					 sdio_test_wakeup_callback);

			if (tch->timer_interval_ms > 0) {
				pr_info(TEST_MODULE_NAME ": %s - init timer, "
							 "ms=%d\n",
					__func__, tch->timer_interval_ms);
				init_timer(&tch->timer);
				tch->timer.data = (unsigned long)tch;
				tch->timer.function =
					sdio_test_lpm_timer_handler;
				tch->timer.expires = jiffies +
				    msecs_to_jiffies(tch->timer_interval_ms);
				add_timer(&tch->timer);
			}
		}
	}

	pr_debug(TEST_MODULE_NAME ":queue_work..\n");
	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];

		if ((!tch) || (!tch->is_used) || (!tch->ch_ready) ||
		    (tch->ch_id == SDIO_SMEM))
			continue;
		queue_work(tch->workqueue, &tch->test_work.work);
	}

	pr_info(TEST_MODULE_NAME ":Waiting for the test completion\n");

	if (!test_ctx->test_completed) {
		wait_event(test_ctx->wait_q, test_ctx->test_completed);
		pr_info(TEST_MODULE_NAME ":Test Completed\n");
		for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
			struct test_channel *tch = test_ctx->test_ch_arr[i];

			if ((!tch) || (!tch->is_used) || (!tch->ch_ready))
				continue;
			if (tch->test_result == TEST_FAILED) {
				pr_info(TEST_MODULE_NAME ":Test FAILED\n");
				test_ctx->test_result = TEST_FAILED;
				return 0;
			}
		}
		pr_info(TEST_MODULE_NAME ":Test PASSED\n");
		test_ctx->test_result = TEST_PASSED;
	}


	return 0;
}

static int set_params_loopback_9k(struct test_channel *tch)
{
	if (!tch) {
		pr_err(TEST_MODULE_NAME ":NULL channel\n");
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->test_type = SDIO_TEST_LOOPBACK_CLIENT;
	tch->config_msg.signature = TEST_CONFIG_SIGNATURE;
	tch->config_msg.test_case = SDIO_TEST_LOOPBACK_CLIENT;
	tch->config_msg.num_packets = 10000;
	tch->config_msg.num_iterations = 1;

	tch->packet_length = 512;
	if (tch->ch_id == SDIO_RPC)
		tch->packet_length = 128;
	tch->timer_interval_ms = 0;

	return 0;
}

static int set_params_a2_perf(struct test_channel *tch)
{
	if (!tch) {
		pr_err(TEST_MODULE_NAME ":NULL channel\n");
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->test_type = SDIO_TEST_PERF;
	tch->config_msg.signature = TEST_CONFIG_SIGNATURE;
	tch->config_msg.test_case = SDIO_TEST_LOOPBACK_CLIENT;
	if (tch->ch_id == SDIO_DIAG)
		tch->packet_length = 512;
	else
		tch->packet_length = MAX_XFER_SIZE;

	tch->config_msg.num_packets = 10000;
	tch->config_msg.num_iterations = 1;
	tch->random_packet_size = 0;

	tch->timer_interval_ms = 0;

	return 0;
}

static int set_params_a2_small_pkts(struct test_channel *tch)
{
	if (!tch) {
		pr_err(TEST_MODULE_NAME ":NULL channel\n");
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->test_type = SDIO_TEST_PERF;
	tch->config_msg.signature = TEST_CONFIG_SIGNATURE;
	tch->config_msg.test_case = SDIO_TEST_LOOPBACK_CLIENT;
	tch->packet_length = 128;

	tch->config_msg.num_packets = 1000000;
	tch->config_msg.num_iterations = 1;
	tch->random_packet_size = 1;

	tch->timer_interval_ms = 0;

	return 0;
}

static int set_params_smem_test(struct test_channel *tch)
{
	if (!tch) {
		pr_err(TEST_MODULE_NAME ":NULL channel\n");
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->timer_interval_ms = 0;

	return 0;
}

static int set_params_lpm_test(struct test_channel *tch,
				enum sdio_test_case_type test,
				int timer_interval_ms)
{
	static int first_time = 1;
	if (!tch) {
		pr_err(TEST_MODULE_NAME ": %s - NULL channel\n", __func__);
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->test_type = test;
	tch->config_msg.signature = TEST_CONFIG_SIGNATURE;
	tch->config_msg.test_case = test;
	tch->config_msg.num_packets = 100;
	tch->config_msg.num_iterations = 1;
	if (seed != 0)
		tch->config_msg.test_param = seed;
	else
		tch->config_msg.test_param =
			(unsigned int)(get_jiffies_64() & 0xFFFF);
	pr_info(TEST_MODULE_NAME ":%s: seed is %d",
	       __func__, tch->config_msg.test_param);

	tch->timer_interval_ms = timer_interval_ms;
	tch->timeout_ms = 10000;

	tch->packet_length = 0;
	if (test != SDIO_TEST_LPM_RANDOM) {
		init_timer(&tch->timeout_timer);
		tch->timeout_timer.data = (unsigned long)tch;
		tch->timeout_timer.function = sdio_test_lpm_timeout_handler;
		tch->timeout_timer.expires = jiffies +
			msecs_to_jiffies(tch->timeout_ms);
		add_timer(&tch->timeout_timer);
		pr_info(TEST_MODULE_NAME ": %s - Initiated LPM TIMEOUT TIMER."
			"set to %d ms\n",
			__func__, tch->timeout_ms);
	}

	if (first_time) {
		pr_info(TEST_MODULE_NAME ": %s - wake_lock_init() called\n",
		__func__);
		wake_lock_init(&test_ctx->wake_lock,
			       WAKE_LOCK_SUSPEND, TEST_MODULE_NAME);
		first_time = 0;
	}

	pr_info(TEST_MODULE_NAME ": %s - wake_lock() for the TEST is "
		"called. to prevent real sleeping\n", __func__);
	wake_lock(&test_ctx->wake_lock);

	return 0;
}

static int set_params_8k_sender_no_lp(struct test_channel *tch)
{
	if (!tch) {
		pr_err(TEST_MODULE_NAME ":NULL channel\n");
		return -EINVAL;
	}
	tch->is_used = 1;
	tch->test_type = SDIO_TEST_HOST_SENDER_NO_LP;
	tch->config_msg.signature = TEST_CONFIG_SIGNATURE;
	tch->config_msg.test_case = SDIO_TEST_HOST_SENDER_NO_LP;
	tch->config_msg.num_packets = 1000;
	tch->config_msg.num_iterations = 1;

	tch->packet_length = 512;
	if (tch->ch_id == SDIO_RPC)
		tch->packet_length = 128;
	tch->timer_interval_ms = 0;

	return 0;
}

/**
 * Write File.
 *
 * @note Trigger the test from user space by:
 * echo 1 > /dev/sdio_al_test
 *
 */
ssize_t test_write(struct file *filp, const char __user *buf, size_t size,
		   loff_t *f_pos)
{
	int ret = 0;
	int i;

	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];
		if (!tch)
			continue;
		tch->is_used = 0;
	}

	ret = strict_strtol(buf, 10, &test_ctx->testcase);

	switch (test_ctx->testcase) {
	case 1:
		/* RPC */
		pr_debug(TEST_MODULE_NAME " --RPC sender--.\n");
		if (set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_RPC]))
			return size;
		break;
	case 2:
		/* RPC, QMI and DIAG */
		pr_debug(TEST_MODULE_NAME " --RPC, QMI and DIAG sender--.\n");
		if (set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_RPC]) ||
		    set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_QMI]) ||
		    set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_DIAG]))
			return size;
		break;
	case 4:
		pr_debug(TEST_MODULE_NAME " --SMEM--.\n");
		if (set_params_smem_test(test_ctx->test_ch_arr[SDIO_SMEM]))
			return size;
		break;

	case 5:
		pr_debug(TEST_MODULE_NAME " --SMEM and RPC--.\n");
		if (set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_RPC]) ||
		    set_params_smem_test(test_ctx->test_ch_arr[SDIO_SMEM]))
			return size;
		break;
	case 6:
		pr_debug(TEST_MODULE_NAME " --RmNet A2 Performance--.\n");
		if (set_params_a2_perf(test_ctx->test_ch_arr[SDIO_RMNT]))
			return size;
		break;

	case 7:
		pr_debug(TEST_MODULE_NAME " --DUN A2 Performance--.\n");
		if (set_params_a2_perf(test_ctx->test_ch_arr[SDIO_DUN]))
			return size;
		break;
	case 8:
		pr_debug(TEST_MODULE_NAME " --RmNet and DUN A2 Performance--."
					  "\n");
		if (set_params_a2_perf(test_ctx->test_ch_arr[SDIO_RMNT]) ||
		    set_params_a2_perf(test_ctx->test_ch_arr[SDIO_DUN]))
			return size;
		break;
	case 9:
		pr_debug(TEST_MODULE_NAME " --RPC sender and RmNet A2 "
					  "Performance--.\n");
		if (set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_RPC]) ||
		    set_params_a2_perf(test_ctx->test_ch_arr[SDIO_RMNT]))
			return size;
		break;
	case 10:
		pr_debug(TEST_MODULE_NAME " --All the channels--.\n");
		if (set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_RPC]) ||
		    set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_QMI]) ||
		    set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_DIAG]) ||
		    set_params_a2_perf(test_ctx->test_ch_arr[SDIO_RMNT]) ||
		    set_params_a2_perf(test_ctx->test_ch_arr[SDIO_DUN]) ||
		    set_params_smem_test(test_ctx->test_ch_arr[SDIO_SMEM]) ||
		    set_params_loopback_9k(test_ctx->test_ch_arr[SDIO_CIQ]))
			return size;
		break;
	case 11:
		pr_info(TEST_MODULE_NAME " --LPM Test For Device 0. Client "
			"wakes the Host --.\n");
		set_params_lpm_test(test_ctx->test_ch_arr[SDIO_RMNT],
				    SDIO_TEST_LPM_CLIENT_WAKER, 90);
		break;
	case 12:
		pr_info(TEST_MODULE_NAME " --LPM Test For Device 1. Client "
			"wakes the Host --.\n");
		set_params_lpm_test(test_ctx->test_ch_arr[SDIO_RPC],
				    SDIO_TEST_LPM_CLIENT_WAKER, 90);
		break;
	case 13:
		pr_info(TEST_MODULE_NAME " --LPM Test For Device 0. Host "
			"wakes the Client --.\n");
		set_params_lpm_test(test_ctx->test_ch_arr[SDIO_RMNT],
				    SDIO_TEST_LPM_HOST_WAKER, 120);
		break;
	case 14:
		pr_info(TEST_MODULE_NAME " --LPM Test For Device 1. Host "
			"wakes the Client --.\n");
		set_params_lpm_test(test_ctx->test_ch_arr[SDIO_RPC],
				    SDIO_TEST_LPM_HOST_WAKER, 120);
		break;
	case 15:
		pr_info(TEST_MODULE_NAME " --LPM Test RANDOM --.\n");
		set_params_lpm_test(test_ctx->test_ch_arr[SDIO_RPC],
				    SDIO_TEST_LPM_RANDOM, 0);
		break;
	case 16:
		pr_info(TEST_MODULE_NAME " -- host sender no LP for Diag --");
		set_params_8k_sender_no_lp(test_ctx->test_ch_arr[SDIO_DIAG]);
		break;
	case 17:
		pr_info(TEST_MODULE_NAME " -- host sender no LP for Diag, RPC, "
					 "CIQ  --");
		set_params_8k_sender_no_lp(test_ctx->test_ch_arr[SDIO_DIAG]);
		set_params_8k_sender_no_lp(test_ctx->test_ch_arr[SDIO_CIQ]);
		set_params_8k_sender_no_lp(test_ctx->test_ch_arr[SDIO_RPC]);
		break;
	case 18:
		pr_info(TEST_MODULE_NAME " -- rmnet small packets (5-128)  --");
		if (set_params_a2_small_pkts(test_ctx->test_ch_arr[SDIO_RMNT]))
			return size;
		break;
	case 98:
		pr_info(TEST_MODULE_NAME " set runtime debug on");
		test_ctx->runtime_debug = 1;
		return size;
	case 99:
		pr_info(TEST_MODULE_NAME " set runtime debug off");
		test_ctx->runtime_debug = 0;
		return size;
	default:
		pr_info(TEST_MODULE_NAME ":Bad Test number = %d.\n",
			(int)test_ctx->testcase);
		return 0;
	}
	ret = test_start();
	if (ret) {
		pr_err(TEST_MODULE_NAME ":test_start failed, ret = %d.\n",
			ret);

	}
	return size;
}

/**
 * Test Channel Init.
 */
int test_channel_init(char *name)
{
	struct test_channel *test_ch;
	int ch_id = 0;
#ifdef CONFIG_MSM_SDIO_SMEM
	int ret;
#endif

	pr_debug(TEST_MODULE_NAME ":%s.\n", __func__);
	pr_info(TEST_MODULE_NAME ": init test cahnnel %s.\n", name);

	ch_id = channel_name_to_id(name);
	pr_debug(TEST_MODULE_NAME ":id = %d.\n", ch_id);
	if (test_ctx->test_ch_arr[ch_id] == NULL) {
		test_ch = kzalloc(sizeof(*test_ch), GFP_KERNEL);
		if (test_ch == NULL) {
			pr_err(TEST_MODULE_NAME ":kzalloc err for allocating "
						"test_ch %s.\n",
			       name);
			return -ENOMEM;
		}
		test_ctx->test_ch_arr[ch_id] = test_ch;

		test_ch->ch_id = ch_id;

		memcpy(test_ch->name, name, CHANNEL_NAME_SIZE);

		test_ch->buf_size = MAX_XFER_SIZE;

		test_ch->buf = kzalloc(test_ch->buf_size, GFP_KERNEL);
		if (test_ch->buf == NULL) {
			kfree(test_ch);
			test_ctx->test_ch = NULL;
			return -ENOMEM;
		}

		if (test_ch->ch_id == SDIO_SMEM) {
			test_ctx->smem_buf = kzalloc(SMEM_MAX_XFER_SIZE,
						     GFP_KERNEL);
			if (test_ctx->smem_buf == NULL) {
				pr_err(TEST_MODULE_NAME ":%s: Unable to "
							"allocate smem buf\n",
				       __func__);
				kfree(test_ch);
				test_ctx->test_ch = NULL;
				return -ENOMEM;
			}

#ifdef CONFIG_MSM_SDIO_SMEM
			ret = platform_driver_register(&sdio_smem_drv);
			if (ret) {
				pr_err(TEST_MODULE_NAME ":%s: Unable to "
							"register sdio smem "
							"test client\n",
				       __func__);
				return ret;
			}
#endif
		} else {
			test_ch->workqueue =
				create_singlethread_workqueue(test_ch->name);
			test_ch->test_work.test_ch = test_ch;
			INIT_WORK(&test_ch->test_work.work, worker);

			init_waitqueue_head(&test_ch->wait_q);
		}
	} else {
		pr_err(TEST_MODULE_NAME ":trying to call test_channel_init "
					"twice for chan %d\n",
		       ch_id);
	}

	return 0;
}

static struct class *test_class;

const struct file_operations test_fops = {
	.owner = THIS_MODULE,
	.write = test_write,
};

/**
 * Module Init.
 */
static int __init test_init(void)
{
	int ret;

	pr_debug(TEST_MODULE_NAME ":test_init.\n");

	test_ctx = kzalloc(sizeof(*test_ctx), GFP_KERNEL);
	if (test_ctx == NULL) {
		pr_err(TEST_MODULE_NAME ":kzalloc err.\n");
		return -ENOMEM;
	}
	test_ctx->test_ch = NULL;
	test_ctx->signature = TEST_SIGNATURE;

	test_ctx->name = "UNKNOWN";

	init_waitqueue_head(&test_ctx->wait_q);

#ifdef CONFIG_DEBUG_FS
	sdio_al_test_debugfs_init();
#endif

	test_class = class_create(THIS_MODULE, TEST_MODULE_NAME);

	ret = alloc_chrdev_region(&test_ctx->dev_num, 0, 1, TEST_MODULE_NAME);
	if (ret) {
		pr_err(TEST_MODULE_NAME "alloc_chrdev_region err.\n");
		return -ENODEV;
	}

	test_ctx->dev = device_create(test_class, NULL, test_ctx->dev_num,
				      test_ctx, TEST_MODULE_NAME);
	if (IS_ERR(test_ctx->dev)) {
		pr_err(TEST_MODULE_NAME ":device_create err.\n");
		return -ENODEV;
	}

	test_ctx->cdev = cdev_alloc();
	if (test_ctx->cdev == NULL) {
		pr_err(TEST_MODULE_NAME ":cdev_alloc err.\n");
		return -ENODEV;
	}
	cdev_init(test_ctx->cdev, &test_fops);
	test_ctx->cdev->owner = THIS_MODULE;

	ret = cdev_add(test_ctx->cdev, test_ctx->dev_num, 1);
	if (ret)
		pr_err(TEST_MODULE_NAME ":cdev_add err=%d\n", -ret);
	else
		pr_debug(TEST_MODULE_NAME ":SDIO-AL-Test init OK..\n");

	return ret;
}

/**
 * Module Exit.
 */
static void __exit test_exit(void)
{
	int i;

	pr_debug(TEST_MODULE_NAME ":test_exit.\n");

	test_ctx->exit_flag = true;

	msleep(100); /* allow gracefully exit of the worker thread */

	cdev_del(test_ctx->cdev);
	device_destroy(test_class, test_ctx->dev_num);
	unregister_chrdev_region(test_ctx->dev_num, 1);

	for (i = 0; i < SDIO_MAX_CHANNELS; i++) {
		struct test_channel *tch = test_ctx->test_ch_arr[i];
		if (!tch)
			continue;
		kfree(tch->buf);
		kfree(tch);
	}

#ifdef CONFIG_DEBUG_FS
	sdio_al_test_debugfs_cleanup();
#endif

	kfree(test_ctx);

	pr_debug(TEST_MODULE_NAME ":test_exit complete.\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SDIO_AL Test");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");


