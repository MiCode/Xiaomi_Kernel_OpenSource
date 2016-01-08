/* drivers/soc/qcom/smsm_debug.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>

#include <soc/qcom/smem.h>
#include <soc/qcom/smsm.h>

#if defined(CONFIG_DEBUG_FS)


static void debug_read_smsm_state(struct seq_file *s)
{
	uint32_t *smsm;
	int n;

	smsm = smem_find(SMEM_SMSM_SHARED_STATE,
			 SMSM_NUM_ENTRIES * sizeof(uint32_t),
			 0,
			 SMEM_ANY_HOST_FLAG);

	if (smsm)
		for (n = 0; n < SMSM_NUM_ENTRIES; n++)
			seq_printf(s, "entry %d: 0x%08x\n", n, smsm[n]);
}

struct SMSM_CB_DATA {
	int cb_count;
	void *data;
	uint32_t old_state;
	uint32_t new_state;
};
static struct SMSM_CB_DATA smsm_cb_data;
static struct completion smsm_cb_completion;

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	smsm_cb_data.cb_count++;
	smsm_cb_data.old_state = old_state;
	smsm_cb_data.new_state = new_state;
	smsm_cb_data.data = data;
	complete_all(&smsm_cb_completion);
}

#define UT_EQ_INT(a, b) \
	{ \
		if ((a) != (b)) { \
			seq_printf(s, "%s:%d " #a "(%d) != " #b "(%d)\n", \
					__func__, __LINE__, \
					a, b); \
			break; \
		} \
	}

#define UT_GT_INT(a, b) \
	{ \
		if ((a) <= (b)) { \
			seq_printf(s, "%s:%d " #a "(%d) > " #b "(%d)\n", \
					__func__, __LINE__, \
					a, b); \
			break; \
		} \
	}

#define SMSM_CB_TEST_INIT() \
	do { \
		smsm_cb_data.cb_count = 0; \
		smsm_cb_data.old_state = 0; \
		smsm_cb_data.new_state = 0; \
		smsm_cb_data.data = 0; \
	} while (0)


static void debug_test_smsm(struct seq_file *s)
{
	int test_num = 0;
	int ret;

	/* Test case 1 - Register new callback for notification */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);

		/* de-assert SMSM_SMD_INIT to trigger state update */
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);

		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		UT_EQ_INT(smsm_cb_data.old_state & SMSM_SMDINIT, SMSM_SMDINIT);
		UT_EQ_INT(smsm_cb_data.new_state & SMSM_SMDINIT, 0x0);
		UT_EQ_INT((int)(uintptr_t)smsm_cb_data.data, 0x1234);

		/* re-assert SMSM_SMD_INIT to trigger state update */
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);
		UT_EQ_INT(smsm_cb_data.old_state & SMSM_SMDINIT, 0x0);
		UT_EQ_INT(smsm_cb_data.new_state & SMSM_SMDINIT, SMSM_SMDINIT);

		/* deregister callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		/* make sure state change doesn't cause any more callbacks */
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);

	/* Test case 2 - Update already registered callback */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 1);

		/* verify both callback bits work */
		reinit_completion(&smsm_cb_completion);
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);

		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 3);
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 4);

		/* deregister 1st callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 1);
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 4);

		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 5);
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 6);

		/* deregister 2nd callback */
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		/* make sure state change doesn't cause any more callbacks */
		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		UT_EQ_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 6);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);

	/* Test case 3 - Two callback registrations with different data */
	do {
		test_num++;
		SMSM_CB_TEST_INIT();
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 0);
		ret = smsm_state_cb_register(SMSM_APPS_STATE, SMSM_INIT,
				smsm_state_cb, (void *)0x3456);
		UT_EQ_INT(ret, 0);

		/* verify both callbacks work */
		reinit_completion(&smsm_cb_completion);
		UT_EQ_INT(smsm_cb_data.cb_count, 0);
		smsm_change_state(SMSM_APPS_STATE, SMSM_SMDINIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 1);
		UT_EQ_INT((int)(uintptr_t)smsm_cb_data.data, 0x1234);

		reinit_completion(&smsm_cb_completion);
		smsm_change_state(SMSM_APPS_STATE, SMSM_INIT, 0x0);
		UT_GT_INT((int)wait_for_completion_timeout(&smsm_cb_completion,
					msecs_to_jiffies(20)), 0);
		UT_EQ_INT(smsm_cb_data.cb_count, 2);
		UT_EQ_INT((int)(uintptr_t)smsm_cb_data.data, 0x3456);

		/* cleanup and unregister
		 * degregister in reverse to verify data field is
		 * being used
		 */
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_SMDINIT);
		smsm_change_state(SMSM_APPS_STATE, 0x0, SMSM_INIT);
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE,
				SMSM_INIT,
				smsm_state_cb, (void *)0x3456);
		UT_EQ_INT(ret, 2);
		ret = smsm_state_cb_deregister(SMSM_APPS_STATE,
				SMSM_SMDINIT,
				smsm_state_cb, (void *)0x1234);
		UT_EQ_INT(ret, 2);

		seq_printf(s, "Test %d - PASS\n", test_num);
	} while (0);
}

static void debug_read_intr_mask(struct seq_file *s)
{
	uint32_t *smsm;
	int m, n;

	smsm = smem_find(SMEM_SMSM_CPU_INTR_MASK,
			  SMSM_NUM_ENTRIES * SMSM_NUM_HOSTS * sizeof(uint32_t),
			  0,
			  SMEM_ANY_HOST_FLAG);

	if (smsm)
		for (m = 0; m < SMSM_NUM_ENTRIES; m++) {
			seq_printf(s, "entry %d:", m);
			for (n = 0; n < SMSM_NUM_HOSTS; n++)
				seq_printf(s, "   host %d: 0x%08x",
					       n, smsm[m * SMSM_NUM_HOSTS + n]);
			seq_puts(s, "\n");
		}
}

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

static void debug_create(const char *name, umode_t mode,
			 struct dentry *dent,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, mode, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smsm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smsm", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("state", 0444, dent, debug_read_smsm_state);
	debug_create("intr_mask", 0444, dent, debug_read_intr_mask);
	debug_create("smsm_test", 0444, dent, debug_test_smsm);

	init_completion(&smsm_cb_completion);

	return 0;
}

late_initcall(smsm_debugfs_init);
#endif
